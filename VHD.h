#pragma once
#include "Image.h"

constexpr UINT32 VHD_UNUSED_BAT_ENTRY = ~0U;
struct VHD_BAT_ENTRY
{
private:
	UINT32 value = VHD_UNUSED_BAT_ENTRY;
public:
	operator UINT32() const
	{
		return std::byteswap(value);
	}
	UINT32 operator=(UINT32 rvalue)
	{
		value = std::byteswap(rvalue);
		return rvalue;
	}
};
static_assert(sizeof(VHD_BAT_ENTRY) == sizeof(UINT32));
enum VHDType : UINT32
{
	Fixed = std::byteswap(2),
	Dynamic = std::byteswap(3),
	Difference = std::byteswap(4),
};
constexpr UINT64 VHD_COOKIE = 0x78697463656e6f63;
constexpr UINT32 VHD_VALID_FEATURE_MASK = std::byteswap(3);
constexpr UINT32 VHD_FEATURE_RESERVED_MUST_ALWAYS_ON = std::byteswap(2);
constexpr UINT32 VHD_VERSION = std::byteswap(0x10000);
constexpr UINT64 VHD_INVALID_OFFSET = ~0ULL;
struct VHD_FOOTER
{
	UINT64 Cookie;
	UINT32 Features;
	UINT32 FileFormatVersion;
	UINT64 DataOffset;
	UINT32 TimeStamp;
	UINT32 CreatorApplication;
	UINT32 CreatorVersion;
	UINT32 CreatorHostOS;
	UINT64 OriginalSize;
	UINT64 CurrentSize;
	UINT32 DiskGeometry;
	UINT32 DiskType;
	UINT32 Checksum;
	GUID   UniqueId;
	UINT8  SavedState;
	UINT8  Reserved[427];
};
static_assert(sizeof(VHD_FOOTER) == 512);
constexpr UINT64 VHD_DYNAMIC_COOKIE = 0x6573726170737863;
constexpr UINT32 VHD_DYNAMIC_VERSION = std::byteswap(0x10000);
struct VHD_DYNAMIC_HEADER
{
	UINT64 Cookie;
	UINT64 DataOffset;
	UINT64 TableOffset;
	UINT32 HeaderVersion;
	UINT32 MaxTableEntries;
	UINT32 BlockSize;
	UINT32 Checksum;
	GUID   ParentUniqueId;
	UINT32 ParentTimeStamp;
	UINT32 Reserved1;
	UINT16 ParentUnicodeName[256];
	UINT8  ParentLocatorEntry[24][8];
	UINT8  Reserved2[256];
};
static_assert(sizeof(VHD_DYNAMIC_HEADER) == 1024);
constexpr UINT32 VHD_SECTOR_SIZE = 512;
constexpr UINT64 VHD_MAX_DYNAMIC_DISK_SIZE = 2040ULL * 1024 * 1024 * 1024;
constexpr UINT32 VHD_HEADER_LOCATION = 0;
constexpr UINT32 VHD_FOOTER_OFFSET = 512;
constexpr UINT32 VHD_FOOTER_ALIGN = 512;
constexpr UINT64 VHD_DYNAMIC_HEADER_LOCATION = sizeof(VHD_FOOTER);
constexpr UINT64 VHD_BLOCK_ALLOC_TABLE_LOCATION = VHD_DYNAMIC_HEADER_LOCATION + sizeof(VHD_DYNAMIC_HEADER);
constexpr UINT32 VHD_DEFAULT_BLOCK_SIZE = 2 * 1024 * 1024;
constexpr UINT32 VHD_MINIMUM_BITMAP_SIZE = 512;
constexpr UINT32 VHD_BAT_UNIT = 512;
struct VHD : Image
{
private:
	VHD_FOOTER vhd_footer;
	VHD_DYNAMIC_HEADER vhd_dyn_header;
	std::unique_ptr<VHD_BAT_ENTRY[]> vhd_block_allocation_table;
	UINT64 vhd_next_free_address;
	UINT64 vhd_template_bitmap_address;
	UINT64 vhd_disk_size;
	UINT32 vhd_block_size;
	UINT32 vhd_bitmap_real_size;
	UINT32 vhd_bitmap_padding_size;
	UINT32 vhd_bitmap_aligned_size;
	UINT32 vhd_table_entries_count;
public:
	VHD() = default;
	void ReadHeader();
	void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed);
	void WriteHeader() const;
	bool CheckConvertible(PCSTR* reason) const;
	bool IsFixed() const noexcept
	{
		return vhd_footer.DiskType == VHDType::Fixed;
	}
	PCSTR GetImageTypeName() const noexcept
	{
		return "VHD";
	}
	UINT64 GetDiskSize() const noexcept
	{
		return vhd_disk_size;
	}
	UINT32 GetSectorSize() const noexcept
	{
		return VHD_SECTOR_SIZE;
	}
	UINT32 GetBlockSize() const noexcept
	{
		return vhd_block_size;
	}
	UINT32 GetTableEntriesCount() const noexcept
	{
		return vhd_table_entries_count;
	}
	std::optional<UINT64> ProbeBlock(UINT32 index) const noexcept;
	UINT64 AllocateBlock(UINT32 index);
	static std::unique_ptr<Image> DetectImageFormatByData(HANDLE file);
protected:
	template <typename Ty>
	UINT32 VHDChecksumUpdate(Ty* header);
	template <typename Ty>
	bool VHDChecksumValidate(Ty header);
	UINT32 CHSCalculate(UINT64 disk_size);
};