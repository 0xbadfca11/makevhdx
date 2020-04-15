#pragma once
#include "Image.hpp"
#include <winioctl.h>

const UINT64 VHD_COOKIE = 0x78697463656e6f63;
const UINT32 VHD_VALID_FEATURE_MASK = _byteswap_ulong(3);
const UINT32 VHD_FEATURE_RESERVED_MUST_ALWAYS_ON = _byteswap_ulong(2);
const UINT32 VHD_VERSION = _byteswap_ulong(MAKELONG(0, 1));
const UINT64 VHD_INVALID_OFFSET = UINT64_MAX;
const UINT64 VHD_DYNAMIC_COOKIE = 0x6573726170737863;
const UINT32 VHD_DYNAMIC_VERSION = _byteswap_ulong(MAKELONG(0, 1));
const UINT32 VHD_SECTOR_SIZE = 512;
const UINT64 VHD_MAX_DISK_SIZE = 2040ULL * 1024 * 1024 * 1024;
const UINT32 VHD_DEFAULT_BLOCK_SIZE = 2 * 1024 * 1024;
const UINT32 VHD_MINIMUM_BITMAP_SIZE = 512;
const UINT32 VHD_BAT_UNIT = 512;
const UINT32 VHD_UNUSED_BAT_ENTRY = UINT32_MAX;
class VHD_BAT_ENTRY
{
private:
	UINT32 value = VHD_UNUSED_BAT_ENTRY;
public:
	operator UINT32() const
	{
		return _byteswap_ulong(value);
	}
	UINT32 operator=(_In_ UINT32 rvalue)
	{
		value = _byteswap_ulong(rvalue);
		return rvalue;
	}
};
static_assert(sizeof(VHD_BAT_ENTRY) == sizeof(UINT32));
enum VHDType : UINT32
{
	Fixed = byteswap32(2),
	Dynamic = byteswap32(3),
	Difference = byteswap32(4),
};
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
const UINT32 VHD_HEADER_OFFSET = 0;
const UINT32 VHD_DYNAMIC_HEADER_OFFSET = sizeof(VHD_FOOTER);
const UINT32 VHD_BLOCK_ALLOC_TABLE_OFFSET = VHD_DYNAMIC_HEADER_OFFSET + sizeof(VHD_DYNAMIC_HEADER);
struct VHD : Image
{
protected:
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
	VHD(_In_ HANDLE file, _In_ UINT32 cluster_size) : Image(file, cluster_size)
	{}
	void ReadHeader()
	{
		LARGE_INTEGER fsize;
		ATLENSURE(GetFileSizeEx(image_file, &fsize));
		const bool is_modern = fsize.QuadPart % 512 == 0;
		memset(&vhd_footer, 0, sizeof vhd_footer);
		ReadFileWithOffset(image_file, &vhd_footer, is_modern ? sizeof vhd_footer : sizeof vhd_footer - 1, ROUNDUP(fsize.QuadPart - VHD_DYNAMIC_HEADER_OFFSET, sizeof(VHD_FOOTER)));
		if (vhd_footer.Cookie != VHD_COOKIE)
		{
			die(L"Missing VHD signature.");
		}
		if (!VHDChecksumValidate(vhd_footer))
		{
			ReadFileWithOffset(image_file, &vhd_footer, VHD_HEADER_OFFSET);
			if (vhd_footer.Cookie != VHD_COOKIE && !VHDChecksumValidate(vhd_footer))
			{
				die(L"VHD footer checksum mismatch.");
			}
		}
		if ((vhd_footer.Features & ~VHD_VALID_FEATURE_MASK) != 0)
		{
			die(L"Unknown VHD feature flag.");
		}
		if (vhd_footer.FileFormatVersion != VHD_VERSION)
		{
			die(L"Unknown VHD version.");
		}
		vhd_disk_size = _byteswap_uint64(vhd_footer.CurrentSize);
		if (vhd_disk_size == 0 || vhd_disk_size % VHD_SECTOR_SIZE != 0)
		{
			die(L"VHD disk size is not multiple of sector.");
		}
		if (vhd_footer.DiskType == VHDType::Fixed)
		{
			ULONG bit_shift;
			BitScanForward64(&bit_shift, vhd_disk_size);
			vhd_block_size = (std::max)(1U << (std::min)(bit_shift, 31UL), require_alignment);
			vhd_table_entries_count = static_cast<UINT32>(CEILING(vhd_disk_size, vhd_block_size));
			return;
		}
		if (vhd_footer.DiskType != VHDType::Difference && vhd_footer.DiskType != VHDType::Dynamic)
		{
			die(L"Unknown VHD type.");
		}
		ReadFileWithOffset(image_file, &vhd_dyn_header, _byteswap_uint64(vhd_footer.DataOffset));
		if (vhd_dyn_header.Cookie != VHD_DYNAMIC_COOKIE)
		{
			die(L"Missing Dynamic VHD signature.");
		}
		if (!VHDChecksumValidate(vhd_dyn_header))
		{
			die(L"VHD dynamic header checksum mismatch.");
		}
		if (vhd_dyn_header.DataOffset != VHD_INVALID_OFFSET)
		{
			die(L"Unknown extra VHD header.");
		}
		if (vhd_dyn_header.HeaderVersion != VHD_DYNAMIC_VERSION)
		{
			die(L"Unknown Dynamic VHD version.");
		}
		vhd_block_size = _byteswap_ulong(vhd_dyn_header.BlockSize);
		if (!IsPow2(vhd_block_size))
		{
			die(L"VHD is corrupted.");
		}
		vhd_bitmap_real_size = (std::max)(vhd_block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_MINIMUM_BITMAP_SIZE);
		vhd_table_entries_count = _byteswap_ulong(vhd_dyn_header.MaxTableEntries);
		vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(vhd_table_entries_count);
		ReadFileWithOffset(image_file, vhd_block_allocation_table.get(), vhd_table_entries_count * sizeof(VHD_BAT_ENTRY), _byteswap_uint64(vhd_dyn_header.TableOffset));
	}
	void ConstructHeader(_In_ UINT64 disk_size, _In_ UINT32 block_size, _In_ UINT32 sector_size, _In_ bool is_fixed)
	{
		if (block_size == 0)
		{
			block_size = VHD_DEFAULT_BLOCK_SIZE;
		}
		else if (!IsPow2(block_size))
		{
			die(L"Unsuported VHD block size.");
		}
		if (disk_size == 0 || disk_size % VHD_SECTOR_SIZE != 0)
		{
			die(L"VHD disk size is not multiple of sector.");
		}
		if (sector_size != VHD_SECTOR_SIZE)
		{
			die(L"Unsuported VHD sector size.");
		}
		if (is_fixed)
		{
			vhd_disk_size = disk_size;
			ULONG bit_shift;
			BitScanForward64(&bit_shift, disk_size);
			vhd_block_size = (std::max)(1U << (std::min)(bit_shift, 31UL), require_alignment);
			vhd_table_entries_count = static_cast<UINT32>(CEILING(vhd_disk_size, vhd_block_size));
			memset(&vhd_footer, 0, sizeof vhd_footer);
			vhd_footer.Cookie = VHD_COOKIE;
			vhd_footer.Features = VHD_FEATURE_RESERVED_MUST_ALWAYS_ON;
			vhd_footer.FileFormatVersion = VHD_VERSION;
			vhd_footer.DataOffset = VHD_INVALID_OFFSET;
			vhd_footer.CurrentSize = _byteswap_uint64(disk_size);
			vhd_footer.DiskGeometry = CHSCalculate(disk_size);
			vhd_footer.DiskType = VHDType::Fixed;
			ATLENSURE_SUCCEEDED(CoCreateGuid(&vhd_footer.UniqueId));
			VHDChecksumUpdate(&vhd_footer);
			FILE_END_OF_FILE_INFO eof_info;
			eof_info.EndOfFile.QuadPart = disk_size + sizeof vhd_footer;
			if (!SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info))
			{
				die();
			}
			return;
		}
		else
		{
			if (disk_size > VHD_MAX_DISK_SIZE)
			{
				die(L"Exceeded maximum dynamic VHD disk size.");
			}
			vhd_disk_size = disk_size;
			vhd_block_size = block_size;
			vhd_bitmap_real_size = std::max<UINT32>(block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_MINIMUM_BITMAP_SIZE);
			vhd_bitmap_aligned_size = ROUNDUP(vhd_bitmap_real_size, require_alignment);
			vhd_bitmap_padding_size = vhd_bitmap_aligned_size - vhd_bitmap_real_size;
			vhd_table_entries_count = static_cast<UINT32>(CEILING(disk_size, block_size));
			vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(vhd_table_entries_count);
			vhd_next_free_address = ROUNDUP(VHD_BLOCK_ALLOC_TABLE_OFFSET + vhd_table_entries_count * sizeof(VHD_BAT_ENTRY), require_alignment);
			vhd_template_bitmap_address = 0;
			memset(&vhd_footer, 0, sizeof vhd_footer);
			vhd_footer.Cookie = VHD_COOKIE;
			vhd_footer.Features = VHD_FEATURE_RESERVED_MUST_ALWAYS_ON;
			vhd_footer.FileFormatVersion = VHD_VERSION;
			vhd_footer.DataOffset = _byteswap_uint64(VHD_DYNAMIC_HEADER_OFFSET);
			vhd_footer.CurrentSize = _byteswap_uint64(disk_size);
			vhd_footer.DiskGeometry = CHSCalculate(disk_size);
			vhd_footer.DiskType = VHDType::Dynamic;
			ATLENSURE_SUCCEEDED(CoCreateGuid(&vhd_footer.UniqueId));
			VHDChecksumUpdate(&vhd_footer);
			memset(&vhd_dyn_header, 0, sizeof vhd_dyn_header);
			vhd_dyn_header.Cookie = VHD_DYNAMIC_COOKIE;
			vhd_dyn_header.DataOffset = VHD_INVALID_OFFSET;
			vhd_dyn_header.TableOffset = _byteswap_uint64(VHD_BLOCK_ALLOC_TABLE_OFFSET);
			vhd_dyn_header.HeaderVersion = VHD_DYNAMIC_VERSION;
			vhd_dyn_header.MaxTableEntries = _byteswap_ulong(vhd_table_entries_count);
			vhd_dyn_header.BlockSize = _byteswap_ulong(block_size);
			VHDChecksumUpdate(&vhd_dyn_header);
		}
	}
	void WriteHeader() const
	{
		if (vhd_footer.DiskType == VHDType::Fixed)
		{
			WriteFileWithOffset(image_file, vhd_footer, vhd_disk_size);
			if (!FlushFileBuffers(image_file))
			{
				die();
			}
			return;
		}
		else if (vhd_footer.DiskType == VHDType::Dynamic)
		{
			_ASSERT(CheckConvertible(nullptr));
			WriteFileWithOffset(image_file, vhd_footer, VHD_HEADER_OFFSET);
			WriteFileWithOffset(image_file, vhd_dyn_header, VHD_DYNAMIC_HEADER_OFFSET);
			WriteFileWithOffset(image_file, vhd_block_allocation_table.get(), vhd_table_entries_count * sizeof(VHD_BAT_ENTRY), VHD_BLOCK_ALLOC_TABLE_OFFSET);
			WriteFileWithOffset(image_file, vhd_footer, vhd_next_free_address + require_alignment - sizeof vhd_footer);
#ifdef _DEBUG
			LARGE_INTEGER fsize;
			ATLENSURE(GetFileSizeEx(image_file, &fsize));
			_ASSERT(fsize.QuadPart % require_alignment == 0);
#endif
			if (!FlushFileBuffers(image_file))
			{
				die();
			}
			return;
		}
		_CrtDbgBreak();
	}
	virtual bool CheckConvertible(_When_(return == false, _Outptr_result_z_) PCWSTR* reason) const
	{
		if (vhd_footer.DiskType == VHDType::Fixed)
		{
			return true;
		}
		if (vhd_footer.DiskType == VHDType::Difference)
		{
			*reason = L"Differencing VHD is not supported.";
			return false;
		}
		_ASSERT(vhd_footer.DiskType == VHDType::Dynamic);
		if (vhd_block_size < require_alignment)
		{
			*reason = L"VHD block size is smaller than required alignment.";
			return false;
		}
		for (UINT32 i = 0; i < vhd_table_entries_count; i++)
		{
			if (vhd_block_allocation_table[i] != VHD_UNUSED_BAT_ENTRY)
			{
				if ((vhd_block_allocation_table[i] * VHD_BAT_UNIT + vhd_bitmap_real_size) % require_alignment != 0)
				{
					*reason = L"VHD data blocks is not aligned.";
					return false;
				}
			}
		}
		return true;
	}
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
	virtual UINT32 GetSectorSize() const noexcept
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
	std::optional<UINT64> ProbeBlock(_In_ UINT32 index) const noexcept
	{
		_ASSERT(index < vhd_table_entries_count);
		if (vhd_footer.DiskType == VHDType::Fixed)
		{
			return 1ULL * vhd_block_size * index;
		}
		if (vhd_footer.DiskType == VHDType::Dynamic)
		{
			if (UINT64 block_address = vhd_block_allocation_table[index]; block_address != VHD_UNUSED_BAT_ENTRY)
			{
				return block_address * VHD_BAT_UNIT + vhd_bitmap_real_size;
			}
			else
			{
				return std::nullopt;
			}
		}
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		die();
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
			FILE_END_OF_FILE_INFO eof_info;
			eof_info.EndOfFile.QuadPart = vhd_next_free_address + vhd_bitmap_aligned_size + vhd_block_size;
			_ASSERT(eof_info.EndOfFile.QuadPart > VHD_BLOCK_ALLOC_TABLE_OFFSET);
			if (eof_info.EndOfFile.QuadPart > 1LL * UINT32_MAX * VHD_BAT_UNIT)
			{
				SetLastError(ERROR_ARITHMETIC_OVERFLOW);
				die();
			}
			if (!SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info))
			{
				die();
			}
			ULONG junk;
			DUPLICATE_EXTENTS_DATA dup_extent = { image_file };
			dup_extent.SourceFileOffset.QuadPart = vhd_template_bitmap_address;
			dup_extent.TargetFileOffset.QuadPart = vhd_next_free_address;
			dup_extent.ByteCount.QuadPart = require_alignment;
			_ASSERT(dup_extent.SourceFileOffset.QuadPart % require_alignment == 0);
			_ASSERT(dup_extent.TargetFileOffset.QuadPart % require_alignment == 0);
			_ASSERT((SetLastError(NO_ERROR), true));
			if (!vhd_template_bitmap_address || !DeviceIoControl(image_file, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &junk, nullptr))
			{
				_ASSERT(GetLastError() == NO_ERROR || GetLastError() == ERROR_BLOCK_TOO_MANY_REFERENCES);
				vhd_template_bitmap_address = vhd_next_free_address;
				auto vhd_bitmap_buffer = std::make_unique<BYTE[]>(vhd_bitmap_aligned_size);
				memset(vhd_bitmap_buffer.get() + vhd_bitmap_padding_size, 0xFF, vhd_bitmap_real_size);
				WriteFileWithOffset(image_file, vhd_bitmap_buffer.get(), vhd_bitmap_aligned_size, vhd_next_free_address);
			}
			vhd_block_allocation_table[index] = static_cast<UINT32>((vhd_next_free_address + vhd_bitmap_padding_size) / VHD_BAT_UNIT);
			vhd_next_free_address += vhd_bitmap_aligned_size + vhd_block_size;
			return vhd_next_free_address - vhd_block_size;
		}
	}
protected:
	template <typename Ty>
	UINT32 VHDChecksumUpdate(_Inout_ Ty* header)
	{
		header->Checksum = 0;
		PBYTE driveFooter = static_cast<PBYTE>(static_cast<void*>(header));
		UINT32 checksum = 0;
		for (UINT32 counter = 0; counter < sizeof(Ty); counter++)
		{
			checksum += driveFooter[counter];
		}
		return header->Checksum = _byteswap_ulong(~checksum);
	}
	template <typename Ty>
	bool VHDChecksumValidate(_In_ Ty header)
	{
		const UINT32 checksum = header.Checksum;
		return VHDChecksumUpdate(&header) == checksum;
	}
	UINT32 CHSCalculate(_In_ UINT64 disk_size)
	{
		UINT64 totalSectors = disk_size / VHD_SECTOR_SIZE;
		UINT32 cylinderTimesHeads;
		UINT16 cylinders;
		UINT8  heads;
		UINT8  sectorsPerTrack;
		if (totalSectors > 65535 * 16 * 255)
		{
			totalSectors = 65535 * 16 * 255;
		}
		if (totalSectors >= 65535 * 16 * 63)
		{
			sectorsPerTrack = 255;
			heads = 16;
			cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
		}
		else
		{
			sectorsPerTrack = 17;
			cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
			heads = static_cast<UINT8>((cylinderTimesHeads + 1023) / 1024);
			if (heads < 4)
			{
				heads = 4;
			}
			if (cylinderTimesHeads >= (heads * 1024U) || heads > 16)
			{
				sectorsPerTrack = 31;
				heads = 16;
				cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
			}
			if (cylinderTimesHeads >= (heads * 1024U))
			{
				sectorsPerTrack = 63;
				heads = 16;
				cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
			}
		}
		cylinders = static_cast<UINT16>(cylinderTimesHeads / heads);
		return _byteswap_ulong(cylinders << 16 | heads << 8 | sectorsPerTrack);
	}
};