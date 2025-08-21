#include <bit>
#include <iterator>
#include <stdexcept>
#include <wil/result.h>
#include "VHDX.h"
#pragma comment(lib, "ntdll")

void VHDX::ReadHeader()
{
	ReadFileWithOffset(image_file, &vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_LOCATION);
	if (vhdx_file_indentifier.Signature != VHDX_SIGNATURE)
	{
		throw std::runtime_error("Missing VHDX signature.");
	}
	const auto vhdx_headers = std::make_unique_for_overwrite<VHDX_HEADER[]>(2);
	ReadFileWithOffset(image_file, &vhdx_headers[0], VHDX_HEADER1_LOCATION);
	ReadFileWithOffset(image_file, &vhdx_headers[1], VHDX_HEADER2_LOCATION);
	const bool header1_available = vhdx_headers[0].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_headers[0]);
	const bool header2_available = vhdx_headers[1].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_headers[1]);
	if (!header1_available && !header2_available)
	{
		throw std::runtime_error("VHDX header corrupted.");
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
	else if (memcmp(&vhdx_headers[0], &vhdx_headers[1], sizeof(VHDX_HEADER)) == 0)
	{
		vhdx_header = vhdx_headers[0];
	}
	else
	{
		throw std::runtime_error("VHDX header corrupted.");
	}
	if (vhdx_header.Version != VHDX_CURRENT_VERSION)
	{
		throw std::runtime_error("Unknown VHDX header version.");
	}
	const auto vhdx_region_table_headers = std::make_unique_for_overwrite<VHDX_REGION_TABLE_HEADER[]>(2);
	ReadFileWithOffset(image_file, &vhdx_region_table_headers[0], VHDX_REGION_TABLE_HEADER1_OFFSET);
	ReadFileWithOffset(image_file, &vhdx_region_table_headers[1], VHDX_REGION_TABLE_HEADER2_OFFSET);
	const bool region_header1_available = vhdx_region_table_headers[0].Signature == VHDX_REGION_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_region_table_headers[0]);
	const bool region_header2_available = vhdx_region_table_headers[1].Signature == VHDX_REGION_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_region_table_headers[1]);
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
		throw std::runtime_error("VHDX region header corrupted.");
	}
	if (vhdx_region_table_header.EntryCount > VHDX_MAX_ENTRIES)
	{
		throw std::runtime_error("VHDX region header corrupted.");
	}
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
			if (vhdx_metadata_table_header.Signature != VHDX_METADATA_HEADER_SIGNATURE || vhdx_metadata_table_header.EntryCount > VHDX_MAX_ENTRIES)
			{
				throw std::runtime_error("VHDX region header corrupted.");
			}
			for (UINT32 j = 0; j < vhdx_metadata_table_header.EntryCount; j++)
			{
				const UINT32 Offset = vhdx_metadata_table_header.MetadataTableEntries[j].Offset;
				if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == FileParameters)
				{
					ReadFileWithOffset(image_file, &vhdx_metadata_packed.VhdxFileParameters, FileOffset + Offset);
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
					throw std::runtime_error("Unknown required VHDX metadata found.");
				}
			}
			vhdx_data_blocks_count = ceil_div(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize);
			vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
		}
		else if (vhdx_region_table_header.RegionTableEntries[i].Required)
		{
			throw std::runtime_error("Unknown required VHDX region found.");
		}
	}
}
void VHDX::ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool fixed)
{
	if (disk_size > VHDX_MAX_DISK_SIZE)
	{
		throw std::invalid_argument("Exceeded max VHDX disk size.");
	}
	if (block_size == 0)
	{
		block_size = VHDX_DEFAULT_BLOCK_SIZE;
	}
	else if (block_size < VHDX_MIN_BLOCK_SIZE || block_size > VHDX_MAX_BLOCK_SIZE || !std::has_single_bit(block_size))
	{
		throw std::invalid_argument("Unsuported VHDX block size.");
	}
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
			.IsUser = 0,
			.IsVirtualDisk = 0,
			.IsRequired = 1
		},
		{
			.ItemId = VirtualDiskSize,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, VirtualDiskSize)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::VirtualDiskSize),
			.IsUser = 0,
			.IsVirtualDisk = 1,
			.IsRequired = 1
		},
		{
			.ItemId = LogicalSectorSize,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, LogicalSectorSize)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::LogicalSectorSize),
			.IsUser = 0,
			.IsVirtualDisk = 1,
			.IsRequired = 1
		},
		{
			.ItemId = PhysicalSectorSize,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, PhysicalSectorSize)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::PhysicalSectorSize),
			.IsUser = 0,
			.IsVirtualDisk = 1,
			.IsRequired = 1
		},
		{
			.ItemId = VirtualDiskID,
			.Offset = static_cast<UINT32>(offsetof(VHDX_METADATA_PACKED, VirtualDiskId)) + VHDX_METADATA_START_OFFSET,
			.Length = sizeof(VHDX_METADATA_PACKED::VirtualDiskId),
			.IsUser = 0,
			.IsVirtualDisk = 1,
			.IsRequired = 1
		}
	};
	memset(&vhdx_metadata_table_header, 0, sizeof vhdx_metadata_table_header);
	vhdx_metadata_table_header.Signature = VHDX_METADATA_HEADER_SIGNATURE;
	vhdx_metadata_table_header.EntryCount = ARRAYSIZE(vhdx_metadata_table_entry);
	for (UINT32 i = 0; i < ARRAYSIZE(vhdx_metadata_table_entry); i++)
	{
		vhdx_metadata_table_header.MetadataTableEntries[i] = vhdx_metadata_table_entry[i];
	}
	memset(&vhdx_metadata_packed, 0, sizeof vhdx_metadata_packed);
	vhdx_metadata_packed.VhdxFileParameters = { block_size, fixed };
	vhdx_metadata_packed.VirtualDiskSize = disk_size;
	vhdx_metadata_packed.LogicalSectorSize = sector_size;
	vhdx_metadata_packed.PhysicalSectorSize = VHDX_PHYSICAL_SECTOR_SIZE;
	THROW_IF_FAILED(CoCreateGuid(&vhdx_metadata_packed.VirtualDiskId));
	vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
	vhdx_data_blocks_count = ceil_div(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize);
	const UINT32 vhdx_table_entries_count = vhdx_data_blocks_count + (vhdx_data_blocks_count - 1) / vhdx_chuck_ratio;
	vhdx_table_write_size = round_up(vhdx_table_entries_count * static_cast<UINT32>(sizeof(VHDX_BAT_ENTRY)), require_alignment);
	vhdx_block_allocation_table = std::make_unique<VHDX_BAT_ENTRY[]>(vhdx_table_write_size / sizeof(VHDX_BAT_ENTRY));
	const VHDX_REGION_TABLE_ENTRY vhdx_region_table_entry[] =
	{
		{ Metadata, VHDX_METADATA_LOCATION, VHDX_METADATA_LENGTH, 1 },
		{ BAT, VHDX_BAT_LOCATION, round_up(vhdx_table_write_size, VHDX_MINIMUM_ALIGNMENT), 1 }
	};
	memset(&vhdx_region_table_header, 0, sizeof vhdx_region_table_header);
	vhdx_region_table_header.Signature = VHDX_REGION_HEADER_SIGNATURE;
	vhdx_region_table_header.EntryCount = ARRAYSIZE(vhdx_region_table_entry);
	for (UINT32 i = 0; i < ARRAYSIZE(vhdx_region_table_entry); i++)
	{
		vhdx_region_table_header.RegionTableEntries[i] = vhdx_region_table_entry[i];
	}
	VHDXChecksumUpdate(&vhdx_region_table_header);
	vhdx_next_free_address = VHDX_BAT_LOCATION + round_up(vhdx_table_write_size, VHDX_MINIMUM_ALIGNMENT);
	if (fixed)
	{
		for (UINT64 i = 0; i < vhdx_data_blocks_count; i++)
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
bool VHDX::CheckConvertible(PCSTR* reason) const
{
	if (vhdx_header.LogGuid != GUID_NULL)
	{
		*reason = "VHDX journal log needs recovery.";
		return false;
	}
	if (vhdx_metadata_packed.VhdxFileParameters.HasParent)
	{
		*reason = "Differencing VHDX is not supported.";
		return false;
	}
	if (require_alignment > VHDX_MINIMUM_ALIGNMENT)
	{
		THROW_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
	}
	return true;
}
std::optional<UINT64> VHDX::ProbeBlock(UINT32 index) const noexcept
{
	_ASSERT(index < GetTableEntriesCount());
	const UINT64 real_index = 1ULL * index + index / vhdx_chuck_ratio;
	if (vhdx_block_allocation_table[real_index].State == VHDXBlockState::PAYLOAD_BLOCK_FULLY_PRESENT)
	{
		return vhdx_block_allocation_table[real_index].FileOffsetMB * VHDX_BAT_UNIT;
	}
	else
	{
		return std::nullopt;
	}
}
UINT64 VHDX::AllocateBlock(UINT32 index)
{
	if (const auto offset = ProbeBlock(index))
	{
		return *offset;
	}
	else
	{
		const UINT64 real_index = 1ULL * index + index / vhdx_chuck_ratio;
		FILE_END_OF_FILE_INFO eof_info = { {.QuadPart = static_cast<LONGLONG>(vhdx_next_free_address + vhdx_metadata_packed.VhdxFileParameters.BlockSize) } };
		_ASSERT(eof_info.EndOfFile.QuadPart % VHDX_MINIMUM_ALIGNMENT == 0);
		_ASSERT(eof_info.EndOfFile.QuadPart * 1ULL >= VHDX_BAT_LOCATION + VHDX_MINIMUM_ALIGNMENT);
		THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info));
		vhdx_block_allocation_table[real_index].FileOffsetMB = vhdx_next_free_address / VHDX_BAT_UNIT;
		vhdx_block_allocation_table[real_index].State = PAYLOAD_BLOCK_FULLY_PRESENT;
		vhdx_next_free_address += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
		return vhdx_next_free_address - vhdx_metadata_packed.VhdxFileParameters.BlockSize;
	}
}
std::unique_ptr<Image> VHDX::DetectImageFormatByData(HANDLE file)
{
	decltype(VHDX_FILE_IDENTIFIER::Signature) vhdx_sig;
	ReadFileWithOffset(file, &vhdx_sig, VHDX_FILE_IDENTIFIER_LOCATION);
	if (vhdx_sig == VHDX_SIGNATURE)
	{
		return std::unique_ptr<Image>(new VHDX);
	}
	return nullptr;
}
template <typename Ty>
bool VHDX::VHDXChecksumValidate(const Ty& header) const
{
	const auto header_copy = std::make_unique<Ty>(header);
	header_copy->Checksum = 0;
	return RtlCrc32(header_copy.get(), sizeof(Ty), 0) == header.Checksum;
}
template <typename Ty>
void VHDX::VHDXChecksumUpdate(Ty* header) const
{
	header->Checksum = 0;
	header->Checksum = RtlCrc32(header, sizeof(Ty), 0);
}