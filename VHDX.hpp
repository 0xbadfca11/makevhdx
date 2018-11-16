#pragma once
#include "Image.hpp"
//#include <initguid.h>
#include <guiddef.h>
#pragma comment(lib, "ntdll")
#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        constexpr inline GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

const UINT64 VHDX_SIGNATURE = 0x656C696678646876;
const UINT32 VHDX_MAX_ENTRIES = 2047;
const UINT32 VHDX_HEADER_SIGNATURE = 0x64616568;
const UINT16 VHDX_CURRENT_VERSION = 1;
const UINT64 VHDX_LOG_LOCATION = 1024 * 1024;
const UINT32 VHDX_LOG_LENGTH = 1024 * 1024;
const UINT32 VHDX_REGION_HEADER_SIGNATURE = 0x69676572;
const UINT64 VHDX_METADATA_HEADER_SIGNATURE = 0x617461646174656D;
const UINT64 VHDX_METADATA_LOCATION = VHDX_LOG_LOCATION + VHDX_LOG_LENGTH;
const UINT32 VHDX_METADATA_LENGTH = 1024 * 1024;
const UINT32 VHDX_METADATA_START_OFFSET = 64 * 1024;
const UINT64 VHDX_BAT_LOCATION = VHDX_METADATA_LOCATION + VHDX_METADATA_LENGTH;
const UINT32 VHDX_FILE_IDENTIFIER_OFFSET = 0;
const UINT32 VHDX_HEADER1_OFFSET = 1 * 64 * 1024;
const UINT32 VHDX_HEADER2_OFFSET = 2 * 64 * 1024;
const UINT32 VHDX_REGION_TABLE_HEADER1_OFFSET = 3 * 64 * 1024;
const UINT32 VHDX_REGION_TABLE_HEADER2_OFFSET = 4 * 64 * 1024;
const UINT64 VHDX_PHYSICAL_SECTOR_SIZE = 4096;
const UINT64 VHDX_MAX_DISK_SIZE = 64ULL * 1024 * 1024 * 1024 * 1024;
const UINT32 VHDX_MIN_BLOCK_SIZE = 1 * 1024 * 1024;
const UINT32 VHDX_MAX_BLOCK_SIZE = 256 * 1024 * 1024;
const UINT32 VHDX_DEFAULT_BLOCK_SIZE = 32 * 1024 * 1024;
const UINT32 VHDX_MINIMUM_ALIGNMENT = 1024 * 1024;
const UINT32 VHDX_BAT_UNIT = 1024 * 1024;
struct VHDX_FILE_IDENTIFIER
{
	UINT64 Signature;
	UINT16 Creator[256];
	UINT8  Padding[4096 - 8 - 512];
};
static_assert(sizeof(VHDX_FILE_IDENTIFIER) == 4096);
struct VHDX_HEADER
{
	UINT32 Signature;
	UINT32 Checksum;
	UINT64 SequenceNumber;
	GUID   FileWriteGuid;
	GUID   DataWriteGuid;
	GUID   LogGuid;
	UINT16 LogVersion;
	UINT16 Version;
	UINT32 LogLength;
	UINT64 LogOffset;
	UINT8  Reserved[4016];
};
static_assert(sizeof(VHDX_HEADER) == 4096);
struct VHDX_REGION_TABLE_ENTRY
{
	GUID   Guid;
	UINT64 FileOffset;
	UINT32 Length;
	UINT32 Required;
};
static_assert(sizeof(VHDX_REGION_TABLE_ENTRY) == 32);
DEFINE_GUID(BAT, 0x2dc27766, 0xf623, 0x4200, 0x9d, 0x64, 0x11, 0x5e, 0x9b, 0xfd, 0x4a, 0x08);
DEFINE_GUID(Metadata, 0x8B7CA206, 0x4790, 0x4B9A, 0xB8, 0xFE, 0x57, 0x5F, 0x05, 0x0F, 0x88, 0x6E);
struct VHDX_REGION_TABLE_HEADER
{
	UINT32 Signature;
	UINT32 Checksum;
	UINT32 EntryCount;
	UINT32 Reserved;
	_Field_size_full_(EntryCount) VHDX_REGION_TABLE_ENTRY RegionTableEntries[VHDX_MAX_ENTRIES];
	BYTE   Padding[16];
};
static_assert(sizeof(VHDX_REGION_TABLE_HEADER) == 64 * 1024);
struct VHDX_BAT_ENTRY
{
	UINT64 State : 3;
	UINT64 Reserved : 17;
	UINT64 FileOffsetMB : 44;
};
static_assert(sizeof(VHDX_BAT_ENTRY) == 8);
enum VHDXBlockState : UINT32
{
	PAYLOAD_BLOCK_NOT_PRESENT = 0,
	PAYLOAD_BLOCK_UNDEFINED = 1,
	PAYLOAD_BLOCK_ZERO = 2,
	PAYLOAD_BLOCK_UNMAPPED = 3,
	PAYLOAD_BLOCK_FULLY_PRESENT = 6,
	PAYLOAD_BLOCK_PARTIALLY_PRESENT = 7,
};
struct VHDX_METADATA_TABLE_ENTRY
{
	GUID   ItemId;
	UINT32 Offset;
	UINT32 Length;
	UINT32 IsUser : 1;
	UINT32 IsVirtualDisk : 1;
	UINT32 IsRequired : 1;
	UINT32 Reserved : 29;
	UINT32 Reserved2;
};
static_assert(sizeof(VHDX_METADATA_TABLE_ENTRY) == 32);
DEFINE_GUID(FileParameters, 0xCAA16737, 0xFA36, 0x4D43, 0xB3, 0xB6, 0x33, 0xF0, 0xAA, 0x44, 0xE7, 0x6B);
DEFINE_GUID(VirtualDiskSize, 0x2FA54224, 0xCD1B, 0x4876, 0xB2, 0x11, 0x5D, 0xBE, 0xD8, 0x3B, 0xF4, 0xB8);
DEFINE_GUID(VirtualDiskID, 0xBECA12AB, 0xB2E6, 0x4523, 0x93, 0xEF, 0xC3, 0x09, 0xE0, 0x00, 0xC7, 0x46);
DEFINE_GUID(LogicalSectorSize, 0x8141BF1D, 0xA96F, 0x4709, 0xBA, 0x47, 0xF2, 0x33, 0xA8, 0xFA, 0xAB, 0x5F);
DEFINE_GUID(PhysicalSectorSize, 0xCDA348C7, 0x445D, 0x4471, 0x9C, 0xC9, 0xE9, 0x88, 0x52, 0x51, 0xC5, 0x56);
DEFINE_GUID(ParentLocator, 0xA8D35F2D, 0xB30B, 0x454D, 0xAB, 0xF7, 0xD3, 0xD8, 0x48, 0x34, 0xAB, 0x0C);
struct VHDX_METADATA_TABLE_HEADER
{
	UINT64 Signature;
	UINT16 Reserved;
	UINT16 EntryCount;
	UINT32 Reserved2[5];
	_Field_size_full_(EntryCount) VHDX_METADATA_TABLE_ENTRY MetadataTableEntries[VHDX_MAX_ENTRIES];
};
static_assert(sizeof(VHDX_METADATA_TABLE_HEADER) == 64 * 1024);
struct VHDX_FILE_PARAMETERS
{
	UINT32 BlockSize;
	UINT32 LeaveBlocksAllocated : 1;
	UINT32 HasParent : 1;
	UINT32 Reserved : 30;
};
static_assert(sizeof(VHDX_FILE_PARAMETERS) == 8);
struct VHDX : Image
{
protected:
	VHDX_FILE_IDENTIFIER vhdx_file_indentifier;
	VHDX_HEADER vhdx_header;
	VHDX_REGION_TABLE_HEADER vhdx_region_table_header;
	VHDX_METADATA_TABLE_HEADER vhdx_metadata_table_header;
	struct VHDX_METADATA_PACKED
	{
		VHDX_FILE_PARAMETERS VhdxFileParameters;
		UINT64 VirtualDiskSize;
		UINT32 LogicalSectorSize;
		UINT32 PhysicalSectorSize;
		GUID   VirtualDiskId;
		UINT8  Padding[4096 - 40];
	} vhdx_metadata_packed;
	static_assert(sizeof(VHDX_METADATA_PACKED) == 4096);
	std::unique_ptr<VHDX_BAT_ENTRY[]> vhdx_block_allocation_table;
	UINT64 vhdx_next_free_address;
	UINT32 vhdx_chuck_ratio;
	UINT32 vhdx_data_blocks_count;
	UINT32 vhdx_table_write_size;
public:
	VHDX() = default;
	VHDX(_In_ HANDLE file, _In_ UINT32 cluster_size) : Image(file, cluster_size)
	{
		if (cluster_size > VHDX_MINIMUM_ALIGNMENT)
		{
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			die();
		}
	}
	virtual void Attach(_In_ HANDLE file, _In_ UINT32 cluster_size)
	{
		if (!IsPow2(cluster_size))
		{
			die(L"Require alignment is not power of 2.");
		}
		if (cluster_size > VHDX_MINIMUM_ALIGNMENT)
		{
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			die();
		}
		image_file = file;
		require_alignment = cluster_size;
	}
	void ReadHeader()
	{
		ReadFileWithOffset(image_file, &vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_OFFSET);
		if (vhdx_file_indentifier.Signature != VHDX_SIGNATURE)
		{
			die(L"Missing VHDX signature.");
		}
		auto vhdx_headers = std::make_unique<VHDX_HEADER[]>(2);
		ReadFileWithOffset(image_file, &vhdx_headers[0], VHDX_HEADER1_OFFSET);
		ReadFileWithOffset(image_file, &vhdx_headers[1], VHDX_HEADER2_OFFSET);
		const bool header1_available = vhdx_headers[0].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_headers[0]);
		const bool header2_available = vhdx_headers[1].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_headers[1]);
		if (!header1_available && !header2_available)
		{
			die(L"VHDX header corrupted.");
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
			die(L"VHDX header corrupted.");
		}
		if (vhdx_header.Version != VHDX_CURRENT_VERSION)
		{
			die(L"Unknown VHDX header version.");
		}
		auto vhdx_region_table_headers = std::make_unique<VHDX_REGION_TABLE_HEADER[]>(2);
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
			die(L"VHDX region header corrupted.");
		}
		if (vhdx_region_table_header.EntryCount > VHDX_MAX_ENTRIES)
		{
			die(L"VHDX region header corrupted.");
		}
		for (UINT32 i = 0; i < vhdx_region_table_header.EntryCount; i++)
		{
			const UINT64 FileOffset = vhdx_region_table_header.RegionTableEntries[i].FileOffset;
			const UINT32 Length = vhdx_region_table_header.RegionTableEntries[i].Length;
			if (vhdx_region_table_header.RegionTableEntries[i].Guid == BAT)
			{
				vhdx_block_allocation_table = std::make_unique<VHDX_BAT_ENTRY[]>(Length / sizeof(VHDX_BAT_ENTRY));
				ReadFileWithOffset(image_file, vhdx_block_allocation_table.get(), Length, FileOffset);
			}
			else if (vhdx_region_table_header.RegionTableEntries[i].Guid == Metadata)
			{
				ReadFileWithOffset(image_file, &vhdx_metadata_table_header, FileOffset);
				if (vhdx_metadata_table_header.Signature != VHDX_METADATA_HEADER_SIGNATURE || vhdx_metadata_table_header.EntryCount > VHDX_MAX_ENTRIES)
				{
					die(L"VHDX region header corrupted.");
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
						die(L"Unknown required VHDX metadata found.");
					}
				}
				vhdx_data_blocks_count = static_cast<UINT32>(CEILING(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize));
				vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
			}
			else if (vhdx_region_table_header.RegionTableEntries[i].Required)
			{
				die(L"Unknown required VHDX region found.");
			}
		}
	}
	void ConstructHeader(_In_ UINT64 disk_size, _In_ UINT32 block_size, _In_ UINT32 sector_size, _In_ bool is_fixed)
	{
		if (disk_size > VHDX_MAX_DISK_SIZE)
		{
			die(L"Exceeded max VHDX disk size.");
		}
		if (block_size == 0)
		{
			block_size = VHDX_DEFAULT_BLOCK_SIZE;
		}
		else if (block_size < VHDX_MIN_BLOCK_SIZE || block_size > VHDX_MAX_BLOCK_SIZE || !IsPow2(block_size))
		{
			die(L"Unsuported VHDX block size.");
		}
		if (sector_size != 512 && sector_size != 4096)
		{
			die(L"Unsuported VHDX sector size.");
		}
		if (disk_size == 0 || disk_size % sector_size != 0)
		{
			die(L"VHDX disk size is not multiple of sector.");
		}
		memset(&vhdx_file_indentifier, 0, sizeof vhdx_file_indentifier);
		vhdx_file_indentifier.Signature = VHDX_SIGNATURE;
		memset(&vhdx_header, 0, sizeof vhdx_header);
		vhdx_header.Signature = VHDX_HEADER_SIGNATURE;
		vhdx_header.Version = VHDX_CURRENT_VERSION;
		vhdx_header.LogLength = VHDX_LOG_LENGTH;
		vhdx_header.LogOffset = VHDX_LOG_LOCATION;
		VHDXChecksumUpdate(&vhdx_header);
		constexpr VHDX_METADATA_TABLE_ENTRY vhdx_metadata_table_entry[] =
		{
			{
				FileParameters,
				offsetof(VHDX_METADATA_PACKED, VhdxFileParameters) + VHDX_METADATA_START_OFFSET,
				sizeof(VHDX_METADATA_PACKED::VhdxFileParameters),
				0, 0, 1
			},
			{
				VirtualDiskSize,
				offsetof(VHDX_METADATA_PACKED, VirtualDiskSize) + VHDX_METADATA_START_OFFSET,
				sizeof(VHDX_METADATA_PACKED::VirtualDiskSize),
				0, 1, 1
			},
			{
				LogicalSectorSize,
				offsetof(VHDX_METADATA_PACKED, LogicalSectorSize) + VHDX_METADATA_START_OFFSET,
				sizeof(VHDX_METADATA_PACKED::LogicalSectorSize),
				0, 1, 1
			},
			{
				PhysicalSectorSize,
				offsetof(VHDX_METADATA_PACKED, PhysicalSectorSize) + VHDX_METADATA_START_OFFSET,
				sizeof(VHDX_METADATA_PACKED::PhysicalSectorSize),
				0, 1, 1
			},
			{
				VirtualDiskID,
				offsetof(VHDX_METADATA_PACKED, VirtualDiskId) + VHDX_METADATA_START_OFFSET,
				sizeof(VHDX_METADATA_PACKED::VirtualDiskId),
				0, 1, 1
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
		vhdx_metadata_packed.VhdxFileParameters = { block_size, is_fixed };
		vhdx_metadata_packed.VirtualDiskSize = disk_size;
		vhdx_metadata_packed.LogicalSectorSize = sector_size;
		vhdx_metadata_packed.PhysicalSectorSize = VHDX_PHYSICAL_SECTOR_SIZE;
		ATLENSURE_SUCCEEDED(CoCreateGuid(&vhdx_metadata_packed.VirtualDiskId));
		vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
		vhdx_data_blocks_count = static_cast<UINT32>(CEILING(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize));
		const UINT32 vhdx_table_entries_count = vhdx_data_blocks_count + (vhdx_data_blocks_count - 1) / vhdx_chuck_ratio;
		vhdx_table_write_size = ROUNDUP(vhdx_table_entries_count * static_cast<UINT32>(sizeof(VHDX_BAT_ENTRY)), require_alignment);
		vhdx_block_allocation_table = std::make_unique<VHDX_BAT_ENTRY[]>(vhdx_table_write_size / sizeof(VHDX_BAT_ENTRY));
		const VHDX_REGION_TABLE_ENTRY vhdx_region_table_entry[] =
		{
			{ Metadata, VHDX_METADATA_LOCATION, VHDX_METADATA_LENGTH, 1 },
			{ BAT, VHDX_BAT_LOCATION, ROUNDUP(vhdx_table_write_size, VHDX_MINIMUM_ALIGNMENT), 1 }
		};
		memset(&vhdx_region_table_header, 0, sizeof vhdx_region_table_header);
		vhdx_region_table_header.Signature = VHDX_REGION_HEADER_SIGNATURE;
		vhdx_region_table_header.EntryCount = ARRAYSIZE(vhdx_region_table_entry);
		for (UINT32 i = 0; i < ARRAYSIZE(vhdx_region_table_entry); i++)
		{
			vhdx_region_table_header.RegionTableEntries[i] = vhdx_region_table_entry[i];
		}
		VHDXChecksumUpdate(&vhdx_region_table_header);
		vhdx_next_free_address = VHDX_BAT_LOCATION + ROUNDUP(vhdx_table_write_size, VHDX_MINIMUM_ALIGNMENT);
		if (is_fixed)
		{
			for (UINT32 i = 0; i < vhdx_data_blocks_count; i++)
			{
				vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].State = PAYLOAD_BLOCK_FULLY_PRESENT;
				vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].FileOffsetMB = vhdx_next_free_address / VHDX_BAT_UNIT;
				vhdx_next_free_address += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
			}
		}
		FILE_END_OF_FILE_INFO eof_info;
		eof_info.EndOfFile.QuadPart = vhdx_next_free_address;
		if (!SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info))
		{
			die();
		}
	}
	void WriteHeader() const
	{
#ifdef _DEBUG
		LARGE_INTEGER fsize;
		ATLENSURE(GetFileSizeEx(image_file, &fsize));
		_ASSERT(fsize.QuadPart % VHDX_MINIMUM_ALIGNMENT == 0);
#endif
		WriteFileWithOffset(image_file, vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_OFFSET);
		WriteFileWithOffset(image_file, vhdx_header, VHDX_HEADER1_OFFSET);
		WriteFileWithOffset(image_file, vhdx_header, VHDX_HEADER2_OFFSET);
		WriteFileWithOffset(image_file, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER1_OFFSET);
		WriteFileWithOffset(image_file, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER2_OFFSET);
		WriteFileWithOffset(image_file, vhdx_metadata_table_header, VHDX_METADATA_LOCATION);
		WriteFileWithOffset(image_file, vhdx_metadata_packed, VHDX_METADATA_LOCATION + VHDX_METADATA_START_OFFSET);
		WriteFileWithOffset(image_file, vhdx_block_allocation_table.get(), vhdx_table_write_size, VHDX_BAT_LOCATION);
		if (!FlushFileBuffers(image_file))
		{
			die();
		}
	}
	bool CheckConvertible(_Inout_ std::wstring* reason) const
	{
		if (vhdx_header.LogGuid != GUID_NULL)
		{
			*reason = L"VHDX journal log needs recovery.";
			return false;
		}
		if (vhdx_metadata_packed.VhdxFileParameters.HasParent)
		{
			*reason = L"Differencing VHDX is not supported.";
			return false;
		}
		if (require_alignment > VHDX_MINIMUM_ALIGNMENT)
		{
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			die();
		}
		return true;
	}
	bool IsFixed() const noexcept
	{
		return !!vhdx_metadata_packed.VhdxFileParameters.LeaveBlocksAllocated;
	}
	PCSTR GetImageTypeName() const noexcept
	{
		return "VHDX";
	}
	UINT64 GetDiskSize() const noexcept
	{
		return vhdx_metadata_packed.VirtualDiskSize;
	}
	UINT32 GetSectorSize() const noexcept
	{
		return vhdx_metadata_packed.LogicalSectorSize;
	}
	UINT32 GetBlockSize() const noexcept
	{
		return vhdx_metadata_packed.VhdxFileParameters.BlockSize;
	}
	UINT32 GetTableEntriesCount() const noexcept
	{
		return vhdx_data_blocks_count;
	}
	std::optional<UINT64> ProbeBlock(_In_ UINT32 index) const noexcept
	{
		_ASSERT(index < vhdx_data_blocks_count);
		index += index / vhdx_chuck_ratio;
		if (vhdx_block_allocation_table[index].State == VHDXBlockState::PAYLOAD_BLOCK_FULLY_PRESENT)
		{
			return vhdx_block_allocation_table[index].FileOffsetMB * VHDX_BAT_UNIT;
		}
		else
		{
			return std::nullopt;
		}
	}
	UINT64 AllocateBlockForWrite(_In_ UINT32 index)
	{
		if (const auto offset = ProbeBlock(index))
		{
			return *offset;
		}
		else
		{
			_ASSERT(!IsFixed());
			index += index / vhdx_chuck_ratio;
			FILE_END_OF_FILE_INFO eof_info;
			eof_info.EndOfFile.QuadPart = vhdx_next_free_address + vhdx_metadata_packed.VhdxFileParameters.BlockSize;
			_ASSERT(eof_info.EndOfFile.QuadPart % VHDX_MINIMUM_ALIGNMENT == 0);
			_ASSERT(eof_info.EndOfFile.QuadPart >= VHDX_BAT_LOCATION + VHDX_MINIMUM_ALIGNMENT);
			if (!SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info))
			{
				die();
			}
			vhdx_block_allocation_table[index].FileOffsetMB = vhdx_next_free_address / VHDX_BAT_UNIT;
			vhdx_block_allocation_table[index].State = PAYLOAD_BLOCK_FULLY_PRESENT;
			vhdx_next_free_address += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
			return vhdx_next_free_address - vhdx_metadata_packed.VhdxFileParameters.BlockSize;
		}
	}
protected:
	template <typename Ty>
	bool VHDXChecksumValidate(_In_ const Ty& header)
	{
		auto header_copy = std::make_unique<Ty>(header);
		header_copy->Checksum = 0;
		return RtlCrc32(header_copy.get(), sizeof(Ty), 0) == header.Checksum;
	}
	template <typename Ty>
	void VHDXChecksumUpdate(_Inout_ Ty* header)
	{
		header->Checksum = 0;
		header->Checksum = RtlCrc32(header, sizeof(Ty), 0);
	}
};