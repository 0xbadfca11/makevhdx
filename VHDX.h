#pragma once
#include "Image.h"

constexpr UINT64 VHDX_SIGNATURE = 0x656C696678646876;
constexpr UINT32 VHDX_MAX_ENTRIES = 2047;
constexpr UINT32 VHDX_HEADER_SIGNATURE = 0x64616568;
constexpr UINT16 VHDX_CURRENT_VERSION = 1;
constexpr UINT64 VHDX_LOG_LOCATION = 1024 * 1024;
constexpr UINT32 VHDX_LOG_LENGTH = 1024 * 1024;
constexpr UINT32 VHDX_REGION_HEADER_SIGNATURE = 0x69676572;
constexpr UINT64 VHDX_METADATA_HEADER_SIGNATURE = 0x617461646174656D;
constexpr UINT64 VHDX_METADATA_LOCATION = VHDX_LOG_LOCATION + VHDX_LOG_LENGTH;
constexpr UINT32 VHDX_METADATA_LENGTH = 1024 * 1024;
constexpr UINT32 VHDX_METADATA_START_OFFSET = 64 * 1024;
constexpr UINT64 VHDX_BAT_LOCATION = VHDX_METADATA_LOCATION + VHDX_METADATA_LENGTH;
constexpr UINT32 VHDX_FILE_IDENTIFIER_LOCATION = 0;
constexpr UINT32 VHDX_HEADER1_LOCATION = 1 * 64 * 1024;
constexpr UINT32 VHDX_HEADER2_LOCATION = 2 * 64 * 1024;
constexpr UINT32 VHDX_REGION_TABLE_HEADER1_OFFSET = 3 * 64 * 1024;
constexpr UINT32 VHDX_REGION_TABLE_HEADER2_OFFSET = 4 * 64 * 1024;
constexpr UINT64 VHDX_PHYSICAL_SECTOR_SIZE = 4096;
constexpr UINT64 VHDX_MAX_DISK_SIZE = 64ULL * 1024 * 1024 * 1024 * 1024;
constexpr UINT32 VHDX_MIN_BLOCK_SIZE = 1 * 1024 * 1024;
constexpr UINT32 VHDX_MAX_BLOCK_SIZE = 256 * 1024 * 1024;
constexpr UINT32 VHDX_DEFAULT_BLOCK_SIZE = 32 * 1024 * 1024;
constexpr UINT32 VHDX_MINIMUM_ALIGNMENT = 1024 * 1024;
constexpr UINT32 VHDX_BAT_UNIT = 1024 * 1024;
constexpr UINT64 VHDX_NUMBER_OF_SECTORS_PER_SECTOR_BITMAP_BLOCK = 1U << 23;
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
bool inline operator==(const VHDX_HEADER& l, const VHDX_HEADER& r)
{
	return memcmp(&l, &r, sizeof(VHDX_HEADER)) == 0;
}
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
	VHDX_REGION_TABLE_ENTRY RegionTableEntries[VHDX_MAX_ENTRIES];
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
	VHDX_METADATA_TABLE_ENTRY MetadataTableEntries[VHDX_MAX_ENTRIES];
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
private:
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
	template <typename Ty>
	static bool VHDXChecksumValidate(Ty* header);
	template <typename Ty>
	static void VHDXChecksumUpdate(Ty* header);
	static UINT32 CalculateChuckRatio(UINT32 sector_size, UINT32 block_size);
public:
	void ReadHeader();
	void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool fixed);
	void WriteHeader() const;
	void CheckConvertible() const;
	bool IsFixed() const
	{
		return vhdx_metadata_packed.VhdxFileParameters.LeaveBlocksAllocated;
	}
	PCSTR GetImageTypeName() const
	{
		return "VHDX";
	}
	UINT64 GetDiskSize() const
	{
		return vhdx_metadata_packed.VirtualDiskSize;
	}
	UINT32 GetSectorSize() const
	{
		return vhdx_metadata_packed.LogicalSectorSize;
	}
	UINT32 GetBlockSize() const
	{
		return vhdx_metadata_packed.VhdxFileParameters.BlockSize;
	}
	UINT32 GetTableEntriesCount() const
	{
		return vhdx_data_blocks_count;
	}
	std::optional<UINT64> ProbeBlock(UINT32 index) const;
	UINT64 AllocateBlock(UINT32 index);
	static std::unique_ptr<Image> DetectImageFormatByData(HANDLE file);
};