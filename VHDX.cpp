#define NOMINMAX
#include <windows.h>
#include <initguid.h>
#include "VHDX.h"
#pragma comment(lib, "ntdll")

consteval bool ValidateIndexNeverExceeds32bits()
{
	const auto maximum_block_count = VHDX_MAX_DISK_SIZE / VHDX_MIN_BLOCK_SIZE;
	const auto chuck_ratio = VHDX_NUMBER_OF_SECTORS_PER_SECTOR_BITMAP_BLOCK * 512 / VHDX_MIN_BLOCK_SIZE;
	return maximum_block_count + maximum_block_count / chuck_ratio <= INT32_MAX;
}
static_assert(ValidateIndexNeverExceeds32bits());

void VHDX::ReadHeader()
{
	ReadFileWithOffset(image_file, &vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_LOCATION);
	THROW_WIN32_IF(ERROR_VHD_DRIVE_FOOTER_MISSING, vhdx_file_indentifier.Signature != VHDX_SIGNATURE);
	CreateFileMappingFromApp(image_file, nullptr, PAGE_READONLY, 0, nullptr);
	const auto vhdx_headers = std::make_unique_for_overwrite<VHDX_HEADER[]>(2);
	ReadFileWithOffset(image_file, &vhdx_headers[0], VHDX_HEADER1_LOCATION);
	ReadFileWithOffset(image_file, &vhdx_headers[1], VHDX_HEADER2_LOCATION);
	const bool header1_available = vhdx_headers[0].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(&vhdx_headers[0]);
	const bool header2_available = vhdx_headers[1].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(&vhdx_headers[1]);
	if (!header1_available && !header2_available)
	{
		THROW_WIN32(ERROR_VHD_DRIVE_FOOTER_CHECKSUM_MISMATCH);
	}
	else if (!header1_available && header2_available)
	{
		vhdx_header = vhdx_headers[1];
	}
	else if (header1_available && !header2_available)
	{
		vhdx_header = vhdx_headers[0];
	}
	else if (vhdx_headers[0].SequenceNumber > vhdx_headers[1].SequenceNumber)
	{
		vhdx_header = vhdx_headers[0];
	}
	else if (vhdx_headers[0].SequenceNumber < vhdx_headers[1].SequenceNumber)
	{
		vhdx_header = vhdx_headers[1];
	}
	// https://gitlab.com/qemu-project/qemu/-/commit/6906046169ffa9d829beeeaafe1fadeba51669fb
	else if (vhdx_headers[0] == vhdx_headers[1])
	{
		vhdx_header = vhdx_headers[0];
	}
	else
	{
		THROW_WIN32(ERROR_VHD_DRIVE_FOOTER_CORRUPT);
	}
	THROW_WIN32_IF(ERROR_VHD_FORMAT_UNSUPPORTED_VERSION, vhdx_header.Version != VHDX_CURRENT_VERSION);
	const auto vhdx_region_table_headers = std::make_unique_for_overwrite<VHDX_REGION_TABLE_HEADER[]>(2);
	ReadFileWithOffset(image_file, &vhdx_region_table_headers[0], VHDX_REGION_TABLE_HEADER1_OFFSET);
	ReadFileWithOffset(image_file, &vhdx_region_table_headers[1], VHDX_REGION_TABLE_HEADER2_OFFSET);
	const bool region_header1_available = vhdx_region_table_headers[0].Signature == VHDX_REGION_HEADER_SIGNATURE && VHDXChecksumValidate(&vhdx_region_table_headers[0]);
	const bool region_header2_available = vhdx_region_table_headers[1].Signature == VHDX_REGION_HEADER_SIGNATURE && VHDXChecksumValidate(&vhdx_region_table_headers[1]);
	if (region_header1_available)
	{
		vhdx_region_table_header = vhdx_region_table_headers[0];
	}
	else if (region_header2_available)
	{
		vhdx_region_table_header = vhdx_region_table_headers[1];
	}
	else
	{
		THROW_WIN32(ERROR_VHD_SPARSE_HEADER_CHECKSUM_MISMATCH);
	}
	THROW_WIN32_IF(ERROR_VHD_SPARSE_HEADER_CORRUPT, vhdx_region_table_header.EntryCount > VHDX_MAX_ENTRIES);
	for (UINT32 i = 0; i < vhdx_region_table_header.EntryCount; i++)
	{
		const UINT64 FileOffset = vhdx_region_table_header.RegionTableEntries[i].FileOffset;
		const UINT32 Length = vhdx_region_table_header.RegionTableEntries[i].Length;
		if (vhdx_region_table_header.RegionTableEntries[i].Guid == BAT)
		{
			vhdx_block_allocation_table = std::make_unique_for_overwrite<VHDX_BAT_ENTRY[]>(Length / sizeof(VHDX_BAT_ENTRY));
			ReadFileWithOffset(image_file, vhdx_block_allocation_table.get(), Length, FileOffset);
		}
		else if (vhdx_region_table_header.RegionTableEntries[i].Guid == Metadata)
		{
			ReadFileWithOffset(image_file, &vhdx_metadata_table_header, FileOffset);
			THROW_WIN32_IF(ERROR_VHD_SPARSE_HEADER_CORRUPT, vhdx_metadata_table_header.Signature != VHDX_METADATA_HEADER_SIGNATURE || vhdx_metadata_table_header.EntryCount > VHDX_MAX_ENTRIES);
			for (UINT32 j = 0; j < vhdx_metadata_table_header.EntryCount; j++)
			{
				const UINT32 Offset = vhdx_metadata_table_header.MetadataTableEntries[j].Offset;
				if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == FileParameters)
				{
					ReadFileWithOffset(image_file, &vhdx_metadata_packed.VhdxFileParameters, FileOffset + Offset);
					THROW_WIN32_IF(ERROR_VHD_INVALID_BLOCK_SIZE, vhdx_metadata_packed.VhdxFileParameters.BlockSize < VHDX_MIN_BLOCK_SIZE || vhdx_metadata_packed.VhdxFileParameters.BlockSize > VHDX_MAX_BLOCK_SIZE || !std::has_single_bit(vhdx_metadata_packed.VhdxFileParameters.BlockSize));
				}
				else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == VirtualDiskSize)
				{
					ReadFileWithOffset(image_file, &vhdx_metadata_packed.VirtualDiskSize, FileOffset + Offset);
				}
				else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == LogicalSectorSize)
				{
					ReadFileWithOffset(image_file, &vhdx_metadata_packed.LogicalSectorSize, FileOffset + Offset);
				}
				else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == ParentLocator)
				{
					__noop;
				}
				else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == PhysicalSectorSize)
				{
					__noop;
				}
				else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == VirtualDiskID)
				{
					__noop;
				}
				else if (vhdx_metadata_table_header.MetadataTableEntries[j].IsRequired)
				{
					THROW_WIN32(ERROR_VHD_SPARSE_HEADER_UNSUPPORTED_VERSION);
				}
			}
			vhdx_data_blocks_count = ceil_div(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize);
			vhdx_chuck_ratio = CalculateChuckRatio(vhdx_metadata_packed.LogicalSectorSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize);
		}
		else if (vhdx_region_table_header.RegionTableEntries[i].Required)
		{
			THROW_WIN32(ERROR_VHD_SPARSE_HEADER_UNSUPPORTED_VERSION);
		}
	}
}
void VHDX::ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool fixed)
{
	THROW_WIN32_IF(ERROR_CALL_NOT_IMPLEMENTED, require_alignment > VHDX_MINIMUM_ALIGNMENT);
	if (disk_size > VHDX_MAX_DISK_SIZE)
	{
		throw std::invalid_argument("Exceeded max VHDX disk size.");
	}
	if (block_size == 0)
	{
		block_size = VHDX_DEFAULT_BLOCK_SIZE;
	}
	THROW_WIN32_IF(ERROR_VHD_INVALID_BLOCK_SIZE, block_size < VHDX_MIN_BLOCK_SIZE || block_size > VHDX_MAX_BLOCK_SIZE || !std::has_single_bit(block_size));
	if (sector_size != 512 && sector_size != 4096)
	{
		throw std::invalid_argument("Unsuported VHDX sector size.");
	}
	if (disk_size < MINIMUM_DISK_SIZE)
	{
		throw std::invalid_argument("VHDX disk size is less than 3MB.");
	}
	if (disk_size % sector_size != 0)
	{
		throw std::invalid_argument("VHDX disk size is not multiple of sector.");
	}
	memset(&vhdx_file_indentifier, 0, sizeof vhdx_file_indentifier);
	vhdx_file_indentifier.Signature = VHDX_SIGNATURE;
	memset(&vhdx_header, 0, sizeof vhdx_header);
	vhdx_header.Signature = VHDX_HEADER_SIGNATURE;
	vhdx_header.Version = VHDX_CURRENT_VERSION;
	vhdx_header.LogLength = VHDX_LOG_LENGTH;
	vhdx_header.LogOffset = VHDX_LOG_LOCATION;
	VHDXChecksumUpdate(&vhdx_header);
	const VHDX_METADATA_TABLE_ENTRY vhdx_metadata_table_entry[] =
	{
		{
			.ItemId = FileParameters,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, VhdxFileParameters)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::VhdxFileParameters),
			.IsVirtualDisk = 0,
			.IsRequired = 1
		},
		{
			.ItemId = VirtualDiskSize,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, VirtualDiskSize)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::VirtualDiskSize),
			.IsVirtualDisk = 1,
			.IsRequired = 1
		},
		{
			.ItemId = LogicalSectorSize,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, LogicalSectorSize)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::LogicalSectorSize),
			.IsVirtualDisk = 1,
			.IsRequired = 1
		},
		{
			.ItemId = PhysicalSectorSize,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, PhysicalSectorSize)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::PhysicalSectorSize),
			.IsVirtualDisk = 1,
			.IsRequired = 1
		},
		{
			.ItemId = VirtualDiskID,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, VirtualDiskId)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::VirtualDiskId),
			.IsVirtualDisk = 1,
			.IsRequired = 1
		}
	};
	memset(&vhdx_metadata_table_header, 0, sizeof vhdx_metadata_table_header);
	vhdx_metadata_table_header.Signature = VHDX_METADATA_HEADER_SIGNATURE;
	vhdx_metadata_table_header.EntryCount = std::size(vhdx_metadata_table_entry);
	for (UINT32 i = 0; i < std::size(vhdx_metadata_table_entry); i++)
	{
		vhdx_metadata_table_header.MetadataTableEntries[i] = vhdx_metadata_table_entry[i];
	}
	memset(&vhdx_metadata_packed, 0, sizeof vhdx_metadata_packed);
	vhdx_metadata_packed.VhdxFileParameters = { .BlockSize = block_size, .LeaveBlocksAllocated = fixed };
	vhdx_metadata_packed.VirtualDiskSize = disk_size;
	vhdx_metadata_packed.LogicalSectorSize = sector_size;
	vhdx_metadata_packed.PhysicalSectorSize = VHDX_PHYSICAL_SECTOR_SIZE;
	THROW_IF_FAILED(CoCreateGuid(&vhdx_metadata_packed.VirtualDiskId));
	vhdx_chuck_ratio = CalculateChuckRatio(vhdx_metadata_packed.LogicalSectorSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize);
	vhdx_data_blocks_count = ceil_div(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize);
	const UINT32 vhdx_table_entries_count = vhdx_data_blocks_count + (vhdx_data_blocks_count - 1) / vhdx_chuck_ratio;
	vhdx_table_write_size = round_up(vhdx_table_entries_count * static_cast<UINT32>(sizeof(VHDX_BAT_ENTRY)), require_alignment);
	vhdx_block_allocation_table = std::make_unique<VHDX_BAT_ENTRY[]>(vhdx_table_write_size / sizeof(VHDX_BAT_ENTRY));
	const VHDX_REGION_TABLE_ENTRY vhdx_region_table_entry[] =
	{
		{
			.Guid = Metadata,
			.FileOffset = VHDX_METADATA_LOCATION,
			.Length = VHDX_METADATA_LENGTH,
			.Required = 1
		},
		{
			.Guid = BAT,
			.FileOffset = VHDX_BAT_LOCATION,
			.Length = round_up(vhdx_table_write_size, VHDX_MINIMUM_ALIGNMENT),
			.Required = 1
		}
	};
	memset(&vhdx_region_table_header, 0, sizeof vhdx_region_table_header);
	vhdx_region_table_header.Signature = VHDX_REGION_HEADER_SIGNATURE;
	vhdx_region_table_header.EntryCount = std::size(vhdx_region_table_entry);
	for (UINT32 i = 0; i < std::size(vhdx_region_table_entry); i++)
	{
		vhdx_region_table_header.RegionTableEntries[i] = vhdx_region_table_entry[i];
	}
	VHDXChecksumUpdate(&vhdx_region_table_header);
	vhdx_next_free_address = VHDX_BAT_LOCATION + round_up(vhdx_table_write_size, VHDX_MINIMUM_ALIGNMENT);
	if (fixed)
	{
		for (UINT32 i = 0; i < vhdx_data_blocks_count; i++)
		{
			vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].State = PAYLOAD_BLOCK_FULLY_PRESENT;
			vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].FileOffsetMB = vhdx_next_free_address / VHDX_BAT_UNIT;
			vhdx_next_free_address += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
		}
	}
	FILE_END_OF_FILE_INFO eof_info = { {.QuadPart = static_cast<LONGLONG>(vhdx_next_free_address) } };
	THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info));
}
void VHDX::WriteHeader() const
{
	WriteFileWithOffset(image_file, vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_LOCATION);
	WriteFileWithOffset(image_file, vhdx_header, VHDX_HEADER1_LOCATION);
	WriteFileWithOffset(image_file, vhdx_header, VHDX_HEADER2_LOCATION);
	WriteFileWithOffset(image_file, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER1_OFFSET);
	WriteFileWithOffset(image_file, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER2_OFFSET);
	WriteFileWithOffset(image_file, vhdx_metadata_table_header, VHDX_METADATA_LOCATION);
	WriteFileWithOffset(image_file, vhdx_metadata_packed, VHDX_METADATA_LOCATION + VHDX_METADATA_START_OFFSET);
	WriteFileWithOffset(image_file, vhdx_block_allocation_table.get(), vhdx_table_write_size, VHDX_BAT_LOCATION);
#ifdef _DEBUG
	LARGE_INTEGER fsize;
	_ASSERT(GetFileSizeEx(image_file, &fsize));
	_ASSERT(fsize.QuadPart % VHDX_MINIMUM_ALIGNMENT == 0);
#endif
	THROW_IF_WIN32_BOOL_FALSE(FlushFileBuffers(image_file));
}
void VHDX::CheckConvertible() const
{
	if (vhdx_header.LogGuid != GUID_NULL)
	{
		throw std::runtime_error("VHDX journal log needs recovery.");
	}
	if (vhdx_metadata_packed.VhdxFileParameters.HasParent)
	{
		throw std::runtime_error("Differencing VHDX is not supported.");
	}
	THROW_WIN32_IF(ERROR_CALL_NOT_IMPLEMENTED, require_alignment > VHDX_MINIMUM_ALIGNMENT);
}
std::optional<UINT64> VHDX::ProbeBlock(UINT32 index) const
{
	_ASSERT(index < GetTableEntriesCount());
	index += index / vhdx_chuck_ratio;
	if (vhdx_block_allocation_table[index].State == VHDXBlockState::PAYLOAD_BLOCK_FULLY_PRESENT)
	{
		return vhdx_block_allocation_table[index].FileOffsetMB * VHDX_BAT_UNIT;
	}
	return std::nullopt;
}
UINT64 VHDX::AllocateBlock(UINT32 index)
{
	if (const auto offset = ProbeBlock(index))
	{
		return *offset;
	}
	index += index / vhdx_chuck_ratio;
	FILE_END_OF_FILE_INFO eof_info = { {.QuadPart = static_cast<LONGLONG>(vhdx_next_free_address + vhdx_metadata_packed.VhdxFileParameters.BlockSize) } };
	_ASSERT(eof_info.EndOfFile.QuadPart % VHDX_MINIMUM_ALIGNMENT == 0);
	_ASSERT(std::cmp_greater_equal(eof_info.EndOfFile.QuadPart, VHDX_BAT_LOCATION + VHDX_MINIMUM_ALIGNMENT));
	THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info));
	vhdx_block_allocation_table[index].FileOffsetMB = vhdx_next_free_address / VHDX_BAT_UNIT;
	vhdx_block_allocation_table[index].State = PAYLOAD_BLOCK_FULLY_PRESENT;
	vhdx_next_free_address += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
	return vhdx_next_free_address - vhdx_metadata_packed.VhdxFileParameters.BlockSize;
}
std::unique_ptr<Image> VHDX::DetectImageFormatByData(HANDLE file)
{
	LARGE_INTEGER fsize;
	THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file, &fsize));
	if (std::cmp_less(fsize.QuadPart, VHDX_BAT_LOCATION + VHDX_MINIMUM_ALIGNMENT))
	{
		return nullptr;
	}
	decltype(VHDX_FILE_IDENTIFIER::Signature) vhdx_sig;
	ReadFileWithOffset(file, &vhdx_sig, VHDX_FILE_IDENTIFIER_LOCATION);
	if (vhdx_sig == VHDX_SIGNATURE)
	{
		return std::unique_ptr<Image>(new VHDX);
	}
	return nullptr;
}
template <typename Ty>
bool VHDX::VHDXChecksumValidate(Ty* header)
{
	UINT32 checksum = header->Checksum;
	header->Checksum = 0;
	return RtlCrc32(header, sizeof(Ty), 0) == checksum;
}
template <typename Ty>
void VHDX::VHDXChecksumUpdate(Ty* header)
{
	header->Checksum = 0;
	header->Checksum = RtlCrc32(header, sizeof(Ty), 0);
}
UINT32 VHDX::CalculateChuckRatio(UINT32 sector_size, UINT32 block_size)
{
	return static_cast<UINT32>(VHDX_NUMBER_OF_SECTORS_PER_SECTOR_BITMAP_BLOCK * sector_size / block_size);
}