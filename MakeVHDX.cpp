#define WIN32_LEAN_AND_MEAN
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlchecked.h>
#include <windows.h>
#include <initguid.h>
#include <pathcch.h>
#include <winioctl.h>
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <type_traits>
#include <fcntl.h>
#include <io.h>
#include "crc32c.h"
#pragma comment(lib, "pathcch")

#pragma region misc
constexpr UINT32 byteswap32(UINT32 v) noexcept
{
	return v >> 24 & 0xFF | v >> 8 & 0xFF << 8 | v << 8 & 0xFF << 16 | v << 24 & 0xFF << 24;
}
static_assert(0x11335577 == byteswap32(0x77553311));
template <typename Ty1, typename Ty2>
constexpr Ty1 ROUNDUP(Ty1 number, Ty2 num_digits) noexcept
{
	return (number + num_digits - 1) / num_digits * num_digits;
}
static_assert(ROUNDUP(42, 15) == 45);
template <typename Ty1, typename Ty2>
constexpr Ty1 CEILING(Ty1 number, Ty2 significance) noexcept
{
	return (number + significance - 1) / significance;
}
static_assert(CEILING(42, 15) == 3);
constexpr bool is_power_of_2(UINT64 value) noexcept
{
	return value && (value & (value - 1)) == 0;
}
static_assert(is_power_of_2(64));
static_assert(!is_power_of_2(65));
static_assert(!is_power_of_2(0));
[[noreturn]]
void die(PCWSTR err_msg = nullptr)
{
	if (!err_msg)
	{
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), 0, reinterpret_cast<PWSTR>(&err_msg), 0, nullptr);
	}
	_CrtDbgBreak();
	fputws(err_msg, stderr);
	fputws(L"\n", stderr);
	ExitProcess(EXIT_FAILURE);
}
template <typename Ty>
Ty* ReadFileWithOffset(
	_In_ HANDLE hFile,
	_Out_writes_bytes_(nNumberOfBytesToRead) __out_data_source(FILE) Ty* lpBuffer,
	_In_ ULONG nNumberOfBytesToRead,
	_In_ ULONGLONG Offset
)
{
	static_assert(!std::is_pointer_v<Ty>);
	ULONG read;
	OVERLAPPED o = {};
	o.Offset = static_cast<ULONG>(Offset);
	o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
	if (!ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &read, &o) || read != nNumberOfBytesToRead)
	{
		_CrtDbgBreak();
		die();
	}
	return lpBuffer;
}
template <typename Ty>
Ty* ReadFileWithOffset(
	_In_ HANDLE hFile,
	_Out_writes_bytes_(sizeof(Ty)) __out_data_source(FILE) Ty* lpBuffer,
	_In_ ULONGLONG Offset
)
{
	return ReadFileWithOffset(hFile, lpBuffer, sizeof(Ty), Offset);
}
VOID WriteFileWithOffset(
	_In_ HANDLE hFile,
	_In_reads_bytes_(nNumberOfBytesToWrite) LPCVOID lpBuffer,
	_In_ ULONG nNumberOfBytesToWrite,
	_In_ ULONGLONG Offset
)
{
	_ASSERT(Offset % 512 == 0);
	ULONG write;
	OVERLAPPED o = {};
	o.Offset = static_cast<ULONG>(Offset);
	o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
	if (!WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &write, &o) || write != nNumberOfBytesToWrite)
	{
		_CrtDbgBreak();
		die();
	}
}
template <typename Ty>
VOID WriteFileWithOffset(
	_In_ HANDLE hFile,
	_In_ const Ty& lpBuffer,
	_In_ ULONGLONG Offset
)
{
	static_assert(!std::is_pointer_v<Ty>);
	WriteFileWithOffset(hFile, &lpBuffer, sizeof(Ty), Offset);
}
#pragma endregion
#pragma region VHD
const UINT64 VHD_COOKIE = 0x78697463656e6f63;
const UINT32 VHD_VALID_FEATURE_MASK = byteswap32(3);
const UINT32 VHD_VERSION = byteswap32(MAKELONG(0, 1));
const UINT64 VHD_INVALID_OFFSET = UINT64_MAX;
const UINT64 VHD_DYNAMIC_COOKIE = 0x6573726170737863;
const UINT32 VHD_DYNAMIC_VERSION = byteswap32(MAKELONG(0, 1));
const UINT32 VHD_SECTOR_SIZE = 512;
const UINT64 VHD_MAX_DISK_SIZE = 2040ULL * 1024 * 1024 * 1024;
const UINT32 VHD_DEFAULT_BLOCK_SIZE = 2 * 1024 * 1024;
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
	UINT32 operator=(UINT32 rvalue)
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
	UINT8  ParentUniqueId[16];
	UINT32 ParentTimeStamp;
	UINT32 Reserved1;
	UINT16 ParentUnicodeName[256];
	UINT8  ParentLocatorEntry[24][8];
	UINT8  Reserved2[256];
};
static_assert(sizeof(VHD_DYNAMIC_HEADER) == 1024);
template <typename Ty>
UINT32 VHDChecksumUpdate(Ty* header)
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
bool VHDChecksumValidate(Ty header)
{
	const UINT32 checksum = header.Checksum;
	return VHDChecksumUpdate(&header) == checksum;
}
UINT32 CHSCalculate(UINT64 disk_size)
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
#pragma endregion
#pragma region VHDX
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
const UINT32 VHDX_BLOCK_1MB = 1024 * 1024;
DEFINE_GUID(BAT, 0x2dc27766, 0xf623, 0x4200, 0x9d, 0x64, 0x11, 0x5e, 0x9b, 0xfd, 0x4a, 0x08);
DEFINE_GUID(Metadata, 0x8B7CA206, 0x4790, 0x4B9A, 0xB8, 0xFE, 0x57, 0x5F, 0x05, 0x0F, 0x88, 0x6E);
DEFINE_GUID(FileParameters, 0xCAA16737, 0xFA36, 0x4D43, 0xB3, 0xB6, 0x33, 0xF0, 0xAA, 0x44, 0xE7, 0x6B);
DEFINE_GUID(VirtualDiskSize, 0x2FA54224, 0xCD1B, 0x4876, 0xB2, 0x11, 0x5D, 0xBE, 0xD8, 0x3B, 0xF4, 0xB8);
DEFINE_GUID(Page83Data, 0xBECA12AB, 0xB2E6, 0x4523, 0x93, 0xEF, 0xC3, 0x09, 0xE0, 0x00, 0xC7, 0x46);
DEFINE_GUID(LogicalSectorSize, 0x8141BF1D, 0xA96F, 0x4709, 0xBA, 0x47, 0xF2, 0x33, 0xA8, 0xFA, 0xAB, 0x5F);
DEFINE_GUID(PhysicalSectorSize, 0xCDA348C7, 0x445D, 0x4471, 0x9C, 0xC9, 0xE9, 0x88, 0x52, 0x51, 0xC5, 0x56);
DEFINE_GUID(ParentLocator, 0xA8D35F2D, 0xB30B, 0x454D, 0xAB, 0xF7, 0xD3, 0xD8, 0x48, 0x34, 0xAB, 0x0C);
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
	UINT32 Required : 1;
	UINT32 Reserved : 31;
};
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
enum : UINT32
{
	PAYLOAD_BLOCK_NOT_PRESENT = 0,
	PAYLOAD_BLOCK_UNDEFINED = 1,
	PAYLOAD_BLOCK_ZERO = 2,
	PAYLOAD_BLOCK_UNMAPPED = 3,
	PAYLOAD_BLOCK_FULLY_PRESENT = 6,
	PAYLOAD_BLOCK_PARTIALLY_PRESENT = 7,
};
struct VHDX_BAT_ENTRY
{
	UINT64 State : 3;
	UINT64 Reserved : 17;
	UINT64 FileOffsetMB : 44;
};
struct VHDX_METADATA_PACKED
{
	VHDX_FILE_PARAMETERS VhdxFileParameters;
	UINT64 VirtualDiskSize;
	UINT32 LogicalSectorSize;
	UINT32 PhysicalSectorSize;
	GUID   Page83Data;
	UINT8  Padding[4096 - 40];
};
static_assert(sizeof(VHDX_METADATA_PACKED) == 4096);
template <typename Ty>
bool VHDXChecksumValidate(Ty header)
{
	const UINT32 Checksum = header.Checksum;
	header.Checksum = 0;
	return crc32c_append(0, static_cast<uint8_t*>(static_cast<void*>(&header)), sizeof(Ty)) == Checksum;
}
template <typename Ty>
void VHDXChecksumUpdate(Ty* header)
{
	header->Checksum = 0;
	header->Checksum = crc32c_append(0, static_cast<uint8_t*>(static_cast<void*>(header)), sizeof(Ty));
	_ASSERT(VHDXChecksumValidate(*header));
}
#pragma endregion
struct VHD
{
private:
	const HANDLE image;
	const UINT32 require_alignment;
	VHD_FOOTER vhd_footer;
	VHD_DYNAMIC_HEADER vhd_dyn_header;
	std::unique_ptr<VHD_BAT_ENTRY[]> vhd_block_allocation_table;
	UINT64 vhd_next_free_address;
	UINT64 vhd_disk_size;
	UINT32 vhd_block_size;
	UINT32 vhd_bitmap_size;
	UINT32 vhd_bitmap_aligned_size;
	UINT32 vhd_bitmap_padding_size;
	UINT32 vhd_table_entries_count;
	UINT32 vhd_table_write_size;
public:
	VHD(HANDLE image, UINT32 require_alignment) : image(image), require_alignment(require_alignment)
	{
		if (!is_power_of_2(require_alignment))
		{
			die(L"Require alignment isn't power of 2.");
		}
	}
	void ReadHeader()
	{
		LARGE_INTEGER fsize;
		ATLENSURE(GetFileSizeEx(image, &fsize));
		ReadFileWithOffset(image, &vhd_footer, fsize.QuadPart - sizeof vhd_footer);
		if (vhd_footer.Cookie != VHD_COOKIE)
		{
			die(L"Missing VHD signature.");
		}
		if (!VHDChecksumValidate(vhd_footer))
		{
			die(L"VHD footer checksum mismatch.");
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
		if (vhd_disk_size == 0)
		{
			die(L"Zero length VHD.");
		}
		if (vhd_disk_size % VHD_SECTOR_SIZE != 0)
		{
			die(L"VHD disk size is not multiple of sector.");
		}
		if (vhd_footer.DiskType == VHDType::Difference)
		{
			die(L"Differencing VHD is not supported.");
		}
		else if (vhd_footer.DiskType == VHDType::Fixed)
		{
			ULONG bit_shift;
			BitScanForward64(&bit_shift, vhd_disk_size);
			vhd_block_size = 1U << (std::min)(bit_shift, 31UL);
			if (vhd_block_size < require_alignment)
			{
				die(L"VHD isn't aligned.");
			}
			vhd_table_entries_count = static_cast<UINT32>(vhd_disk_size / vhd_block_size);
			return;
		}
		else if (vhd_footer.DiskType != VHDType::Dynamic)
		{
			die(L"Unknown VHD type.");
		}
		ReadFileWithOffset(image, &vhd_dyn_header, _byteswap_uint64(vhd_footer.DataOffset));
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
			die(L"Unknown extra data.");
		}
		if (vhd_dyn_header.HeaderVersion != VHD_DYNAMIC_VERSION)
		{
			die(L"Unknown Dynamic VHD version.");
		}
		vhd_block_size = _byteswap_ulong(vhd_dyn_header.BlockSize);
		if (vhd_block_size < require_alignment)
		{
			die(L"VHD isn't aligned.");
		}
		if (!is_power_of_2(vhd_block_size))
		{
			die(L"VHD is corrupted.");
		}
		vhd_bitmap_size = (std::max)(vhd_block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_SECTOR_SIZE);
		vhd_table_entries_count = _byteswap_ulong(vhd_dyn_header.MaxTableEntries);
		vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(vhd_table_entries_count);
		ReadFileWithOffset(image, vhd_block_allocation_table.get(), vhd_table_entries_count * sizeof(VHD_BAT_ENTRY), _byteswap_uint64(vhd_dyn_header.TableOffset));
	}
	void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed)
	{
		if (block_size == 0)
		{
			block_size = VHD_DEFAULT_BLOCK_SIZE;
		}
		else if (!is_power_of_2(block_size))
		{
			die(L"Unsuported VHD block size.");
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
			vhd_block_size = 1U << (std::min)(bit_shift, 31UL);
			if (vhd_block_size < require_alignment)
			{
				die(L"VHD isn't aligned.");
			}
			vhd_table_entries_count = static_cast<UINT32>(disk_size / vhd_block_size);
			vhd_footer =
			{
				VHD_COOKIE,
				byteswap32(2),
				VHD_VERSION,
				VHD_INVALID_OFFSET,
				0,
				0,
				0,
				0,
				0,
				_byteswap_uint64(disk_size),
				CHSCalculate(disk_size),
				VHDType::Fixed,
			};
			ATLENSURE_SUCCEEDED(CoCreateGuid(&vhd_footer.UniqueId));
			VHDChecksumUpdate(&vhd_footer);
			FILE_END_OF_FILE_INFO eof_info;
			eof_info.EndOfFile.QuadPart = disk_size + sizeof vhd_footer;
			if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
			{
				die();
			}
			return;
		}
		else
		{
			if (disk_size > VHD_MAX_DISK_SIZE)
			{
				die(L"Exceeded max VHD disk size.");
			}
			vhd_disk_size = disk_size;
			vhd_block_size = block_size;
			vhd_bitmap_size = std::max<UINT32>(block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_SECTOR_SIZE);
			vhd_bitmap_aligned_size = ROUNDUP(vhd_bitmap_size, require_alignment);
			vhd_bitmap_padding_size = vhd_bitmap_aligned_size - vhd_bitmap_size;
			vhd_table_entries_count = static_cast<UINT32>(CEILING(disk_size, block_size));
			const UINT32 vhd_table_write_address = sizeof vhd_footer + sizeof vhd_dyn_header;
			vhd_table_write_size = ROUNDUP(vhd_table_entries_count * static_cast<UINT32>(sizeof(VHD_BAT_ENTRY)), require_alignment);
			vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(vhd_table_write_size / sizeof(VHD_BAT_ENTRY));
			vhd_next_free_address = ROUNDUP(sizeof vhd_footer + sizeof vhd_dyn_header + vhd_table_write_size, require_alignment);
			vhd_footer =
			{
				VHD_COOKIE,
				byteswap32(2),
				VHD_VERSION,
				_byteswap_uint64(sizeof vhd_footer),
				0,
				0,
				0,
				0,
				0,
				_byteswap_uint64(disk_size),
				CHSCalculate(disk_size),
				VHDType::Dynamic,
			};
			ATLENSURE_SUCCEEDED(CoCreateGuid(&vhd_footer.UniqueId));
			VHDChecksumUpdate(&vhd_footer);
			vhd_dyn_header =
			{
				VHD_DYNAMIC_COOKIE,
				VHD_INVALID_OFFSET,
				_byteswap_uint64(vhd_table_write_address),
				VHD_DYNAMIC_VERSION,
				_byteswap_ulong(vhd_table_entries_count),
				_byteswap_ulong(block_size),
			};
			VHDChecksumUpdate(&vhd_dyn_header);
		}
	}
	void WriteHeader() const
	{
		if (vhd_footer.DiskType == VHDType::Fixed)
		{
			WriteFileWithOffset(image, vhd_footer, vhd_disk_size);
			return;
		}
		else if (vhd_footer.DiskType == VHDType::Dynamic)
		{
			WriteFileWithOffset(image, vhd_footer, 0);
			WriteFileWithOffset(image, vhd_dyn_header, sizeof vhd_footer);
			WriteFileWithOffset(image, vhd_block_allocation_table.get(), vhd_table_write_size, sizeof vhd_footer + sizeof vhd_dyn_header);
			WriteFileWithOffset(image, vhd_footer, vhd_next_free_address);

			_ASSERT(IsAligned());
			auto vhd_bitmap_buffer = std::make_unique<BYTE[]>(vhd_bitmap_size);
			memset(vhd_bitmap_buffer.get(), 0xFF, vhd_bitmap_size);
			for (UINT32 i = 0; i < vhd_table_entries_count; i++)
			{
				if (vhd_block_allocation_table[i] != VHD_UNUSED_BAT_ENTRY)
				{
					WriteFileWithOffset(image, vhd_bitmap_buffer.get(), vhd_bitmap_size, 1ULL * vhd_block_allocation_table[i] * VHD_SECTOR_SIZE);
				}
			}
			return;
		}
		else
		{
			die(L"BUG");
		}
	}
	bool IsAligned() const
	{
		if (vhd_footer.DiskType == VHDType::Fixed)
		{
			return true;
		}
		for (UINT32 i = 0; i < vhd_table_entries_count; i++)
		{
			if (vhd_block_allocation_table[i] != VHD_UNUSED_BAT_ENTRY)
			{
				if ((vhd_block_allocation_table[i] * VHD_SECTOR_SIZE + vhd_bitmap_size) % require_alignment != 0)
				{
					_CrtDbgBreak();
					return false;
				}
			}
		}
		return true;
	}
	bool IsFixed() const
	{
		return vhd_footer.DiskType == VHDType::Fixed;
	}
	PCSTR GetImageTypeName() const
	{
		return "VHD";
	}
	UINT64 GetDiskSize() const
	{
		return vhd_disk_size;
	}
	UINT32 GetSectorSize() const
	{
		return VHD_SECTOR_SIZE;
	}
	UINT32 GetBlockSize() const
	{
		return vhd_block_size;
	}
	UINT32 GetTableEntriesCount() const
	{
		return vhd_table_entries_count;
	}
	std::optional<UINT64> operator[](UINT32 index) const
	{
		if (index > vhd_table_entries_count)
		{
			_CrtDbgBreak();
			die(L"BUG");
		}
		if (vhd_footer.DiskType == VHDType::Dynamic)
		{
			if (UINT64 block_address = vhd_block_allocation_table[index]; block_address != VHD_UNUSED_BAT_ENTRY)
			{
				return block_address * VHD_SECTOR_SIZE + vhd_bitmap_size;
			}
			else
			{
				return std::nullopt;
			}
		}
		else if (vhd_footer.DiskType == VHDType::Fixed)
		{
			return 1ULL * index * vhd_block_size;
		}
		else
		{
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			die();
		}
	}
	UINT64 AllocateBlockForWrite(UINT32 index)
	{
		if (const auto offset = (*this)[index])
		{
			return *offset;
		}
		else
		{
			if (IsFixed())
			{
				_CrtDbgBreak();
				die(L"BUG");
			}
			FILE_END_OF_FILE_INFO eof_info;
			eof_info.EndOfFile.QuadPart = vhd_next_free_address + vhd_bitmap_aligned_size + vhd_block_size;
			_ASSERT(eof_info.EndOfFile.QuadPart >= sizeof vhd_footer + sizeof vhd_dyn_header);
			if (eof_info.EndOfFile.QuadPart > 1ULL * UINT32_MAX * VHD_SECTOR_SIZE)
			{
				SetLastError(ERROR_ARITHMETIC_OVERFLOW);
				die();
			}
			if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
			{
				die();
			}
			vhd_block_allocation_table[index] = static_cast<UINT32>((vhd_next_free_address + vhd_bitmap_padding_size) / VHD_SECTOR_SIZE);
			vhd_next_free_address += vhd_bitmap_aligned_size + vhd_block_size;
			return vhd_next_free_address - vhd_block_size;
		}
	}
};
struct VHDX
{

private:
	const HANDLE image;
	const UINT32 require_alignment;
	VHDX_FILE_IDENTIFIER vhdx_file_indentifier;
	VHDX_HEADER vhdx_header;
	VHDX_REGION_TABLE_HEADER vhdx_region_table_header;
	VHDX_METADATA_TABLE_HEADER vhdx_metadata_table_header;
	VHDX_METADATA_PACKED vhdx_metadata_packed;
	std::unique_ptr<VHDX_BAT_ENTRY[]> vhdx_block_allocation_table;
	UINT64 vhdx_next_free_address;
	UINT32 vhdx_chuck_ratio;
	UINT32 vhdx_data_blocks_count;
	UINT32 vhdx_table_write_size;
public:
	VHDX(HANDLE image, UINT32 require_alignment) : image(image), require_alignment(ROUNDUP(require_alignment, VHDX_MINIMUM_ALIGNMENT))
	{
		if (!is_power_of_2(require_alignment))
		{
			die(L"Require alignment isn't power of 2.");
		}
	}
	void ReadHeader()
	{
		ReadFileWithOffset(image, &vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_OFFSET);
		if (vhdx_file_indentifier.Signature != VHDX_SIGNATURE)
		{
			die(L"Missing VHDX signature.");
		}
		VHDX_HEADER vhdx_headers[2];
		ReadFileWithOffset(image, &vhdx_headers[0], VHDX_HEADER1_OFFSET);
		ReadFileWithOffset(image, &vhdx_headers[1], VHDX_HEADER2_OFFSET);
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
		if (vhdx_header.LogGuid != GUID_NULL)
		{
			die(L"VHDX journal log needs recovery.");
		}
		VHDX_REGION_TABLE_HEADER vhdx_region_table_headers[2];
		ReadFileWithOffset(image, &vhdx_region_table_headers[0], VHDX_REGION_TABLE_HEADER1_OFFSET);
		ReadFileWithOffset(image, &vhdx_region_table_headers[1], VHDX_REGION_TABLE_HEADER2_OFFSET);
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
				ReadFileWithOffset(image, vhdx_block_allocation_table.get(), Length, FileOffset);
			}
			else if (vhdx_region_table_header.RegionTableEntries[i].Guid == Metadata)
			{
				ReadFileWithOffset(image, &vhdx_metadata_table_header, FileOffset);
				if (vhdx_metadata_table_header.Signature != VHDX_METADATA_HEADER_SIGNATURE || vhdx_metadata_table_header.EntryCount > VHDX_MAX_ENTRIES)
				{
					die(L"VHDX region header corrupted.");
				}
				for (UINT32 j = 0; j < vhdx_metadata_table_header.EntryCount; j++)
				{
					const UINT32 Offset = vhdx_metadata_table_header.MetadataTableEntries[j].Offset;
					if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == FileParameters)
					{
						ReadFileWithOffset(image, &vhdx_metadata_packed.VhdxFileParameters, FileOffset + Offset);
					}
					else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == VirtualDiskSize)
					{
						ReadFileWithOffset(image, &vhdx_metadata_packed.VirtualDiskSize, FileOffset + Offset);
					}
					else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == LogicalSectorSize)
					{
						ReadFileWithOffset(image, &vhdx_metadata_packed.LogicalSectorSize, FileOffset + Offset);
					}
					else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == ParentLocator)
					{
						die(L"Differencing VHDX is not supported.");
					}
					else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == PhysicalSectorSize)
					{
						__noop;
					}
					else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == Page83Data)
					{
						__noop;
					}
					else if (vhdx_metadata_table_header.MetadataTableEntries[j].IsRequired)
					{
						die(L"Unknown require VHDX metadata found.");
					}
				}
				vhdx_data_blocks_count = static_cast<UINT32>(CEILING(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize));
				vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
			}
			else if (vhdx_region_table_header.RegionTableEntries[i].Required)
			{
				die(L"Unknown require VHDX region found.");
			}
		}
	}
	void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed)
	{
		ATLENSURE(require_alignment >= VHDX_MINIMUM_ALIGNMENT);
		if (disk_size > VHDX_MAX_DISK_SIZE)
		{
			die(L"Exceeded max VHDX disk size.");
		}
		if (block_size == 0)
		{
			block_size = VHDX_DEFAULT_BLOCK_SIZE;
		}
		else if (block_size < VHDX_MIN_BLOCK_SIZE || block_size > VHDX_MAX_BLOCK_SIZE || !is_power_of_2(block_size))
		{
			die(L"Unsuported VHDX block size.");
		}
		if (sector_size != 512 && sector_size != 4096)
		{
			die(L"Unsuported VHDX sector size.");
		}
		vhdx_file_indentifier = { VHDX_SIGNATURE };
		vhdx_header = { VHDX_HEADER_SIGNATURE, 0, 0, {}, {}, {}, 0, VHDX_CURRENT_VERSION, VHDX_LOG_LENGTH, VHDX_LOG_LOCATION };
		VHDXChecksumUpdate(&vhdx_header);
		vhdx_metadata_table_header =
		{
			VHDX_METADATA_HEADER_SIGNATURE,
			0,
			5,
			{},
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
					Page83Data,
					offsetof(VHDX_METADATA_PACKED, Page83Data) + VHDX_METADATA_START_OFFSET,
					sizeof(VHDX_METADATA_PACKED::Page83Data),
					0, 1, 1
				}
			}
		};
		vhdx_metadata_packed =
		{
			{ block_size, is_fixed },
			disk_size,
			sector_size,
			VHDX_PHYSICAL_SECTOR_SIZE,
		};
		ATLENSURE_SUCCEEDED(CoCreateGuid(&vhdx_metadata_packed.Page83Data));
		vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
		vhdx_data_blocks_count = static_cast<UINT32>(CEILING(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize));
		const UINT32 vhdx_table_entries_count = vhdx_data_blocks_count + (vhdx_data_blocks_count - 1) / vhdx_chuck_ratio;
		vhdx_table_write_size = ROUNDUP(vhdx_table_entries_count * static_cast<UINT32>(sizeof(VHDX_BAT_ENTRY)), require_alignment);
		vhdx_block_allocation_table = std::make_unique<VHDX_BAT_ENTRY[]>(vhdx_table_write_size / sizeof(VHDX_BAT_ENTRY));
		vhdx_region_table_header = { VHDX_REGION_HEADER_SIGNATURE };
		VHDX_REGION_TABLE_ENTRY vhdx_region_table_entry[] =
		{
			{ Metadata, VHDX_METADATA_LOCATION, ROUNDUP(VHDX_METADATA_LENGTH, require_alignment), 1 },
			{ BAT, VHDX_BAT_LOCATION, ROUNDUP(vhdx_table_write_size, require_alignment), 1 }
		};
		vhdx_region_table_header.EntryCount = ARRAYSIZE(vhdx_region_table_entry);
		for (int i = 0; i < ARRAYSIZE(vhdx_region_table_entry); i++)
		{
			vhdx_region_table_header.RegionTableEntries[i] = vhdx_region_table_entry[i];
		}
		VHDXChecksumUpdate(&vhdx_region_table_header);
		vhdx_next_free_address = VHDX_BAT_LOCATION + ROUNDUP(vhdx_table_write_size, require_alignment);
		if (is_fixed)
		{
			for (UINT32 i = 0; i < vhdx_data_blocks_count; i++)
			{
				vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].State = PAYLOAD_BLOCK_FULLY_PRESENT;
				vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].FileOffsetMB = vhdx_next_free_address / VHDX_BLOCK_1MB;
				vhdx_next_free_address += block_size;
			}
			FILE_END_OF_FILE_INFO eof_info;
			eof_info.EndOfFile.QuadPart = vhdx_next_free_address;
			if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
			{
				die();
			}
		}
	}
	void WriteHeader() const
	{
		WriteFileWithOffset(image, vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_OFFSET);
		WriteFileWithOffset(image, vhdx_header, VHDX_HEADER1_OFFSET);
		WriteFileWithOffset(image, vhdx_header, VHDX_HEADER2_OFFSET);
		WriteFileWithOffset(image, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER1_OFFSET);
		WriteFileWithOffset(image, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER2_OFFSET);
		WriteFileWithOffset(image, vhdx_metadata_table_header, VHDX_METADATA_LOCATION);
		WriteFileWithOffset(image, vhdx_metadata_packed, VHDX_METADATA_LOCATION + VHDX_METADATA_START_OFFSET);
		WriteFileWithOffset(image, vhdx_block_allocation_table.get(), vhdx_table_write_size, VHDX_BAT_LOCATION);
	}
	bool IsAligned() const
	{
		if (require_alignment <= VHDX_MINIMUM_ALIGNMENT)
		{
			return true;
		}
		else
		{
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			die();
		}
	}
	bool IsFixed() const
	{
		return !!vhdx_metadata_packed.VhdxFileParameters.LeaveBlocksAllocated;
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
	std::optional<UINT64> operator[](UINT32 index) const
	{
		if (index > vhdx_data_blocks_count)
		{
			_CrtDbgBreak();
			die(L"BUG");
		}
		index += index / vhdx_chuck_ratio;
		if (vhdx_block_allocation_table[index].State == PAYLOAD_BLOCK_FULLY_PRESENT)
		{
			return vhdx_block_allocation_table[index].FileOffsetMB * VHDX_BLOCK_1MB;
		}
		else
		{
			return std::nullopt;
		}
	}
	UINT64 AllocateBlockForWrite(UINT32 index)
	{
		if (const auto offset = (*this)[index])
		{
			return *offset;
		}
		else
		{
			if (IsFixed())
			{
				_CrtDbgBreak();
				die(L"BUG");
			}
			index += index / vhdx_chuck_ratio;
			FILE_END_OF_FILE_INFO eof_info;
			eof_info.EndOfFile.QuadPart = vhdx_next_free_address + vhdx_metadata_packed.VhdxFileParameters.BlockSize;
			_ASSERT(eof_info.EndOfFile.QuadPart % VHDX_MINIMUM_ALIGNMENT == 0);
			_ASSERT(eof_info.EndOfFile.QuadPart >= 4 * 1024 * 1024);
			if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
			{
				die();
			}
			vhdx_block_allocation_table[index].FileOffsetMB = vhdx_next_free_address / VHDX_BLOCK_1MB;
			vhdx_block_allocation_table[index].State = PAYLOAD_BLOCK_FULLY_PRESENT;
			vhdx_next_free_address += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
			return vhdx_next_free_address - vhdx_metadata_packed.VhdxFileParameters.BlockSize;
		}
	}
};
struct Option
{
	std::optional<UINT32> block_size;
	std::optional<bool> is_fixed;
};
template <typename COPY_FROM, typename COPY_TO>
void ConvertImage(PCWSTR src_file_name, PCWSTR dst_file_name, const Option& options)
{
	wprintf(
		L"Source\n"
		L"Path:              %ls\n",
		src_file_name
	);
	ATL::CHandle src_file(CreateFileW(src_file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
	if (src_file == INVALID_HANDLE_VALUE)
	{
		_CrtDbgBreak();
		src_file.Detach();
		die();
	}
	ULONG fs_flags;
	ATLENSURE(GetVolumeInformationByHandleW(src_file, nullptr, 0, nullptr, nullptr, &fs_flags, nullptr, 0));
	if (!(fs_flags & FILE_SUPPORTS_BLOCK_REFCOUNTING))
	{
		die(L"Filesystem doesn't support Block Cloning feature.");
	}
	BY_HANDLE_FILE_INFORMATION file_info;
	ATLENSURE(GetFileInformationByHandle(src_file, &file_info));
	ULONG dummy;
	FSCTL_GET_INTEGRITY_INFORMATION_BUFFER get_integrity;
	if (!DeviceIoControl(src_file, FSCTL_GET_INTEGRITY_INFORMATION, nullptr, 0, &get_integrity, sizeof get_integrity, &dummy, nullptr))
	{
		die();
	}
	COPY_FROM src_img(src_file, get_integrity.ClusterSizeInBytes);
	src_img.ReadHeader();
	if (!src_img.IsAligned())
	{
		die(L"Source image isn't aligned.");
	}
	wprintf(
		L"Image format:      %hs\n"
		L"Allocation policy: %hs\n"
		L"Disk size:         %llu(%.3fGB)\n"
		L"Block size:        %.1fMB\n",
		src_img.GetImageTypeName(),
		src_img.IsFixed() ? "Preallocate" : "Dynamic",
		src_img.GetDiskSize(),
		src_img.GetDiskSize() / (1024.f * 1024.f * 1024.f),
		src_img.GetBlockSize() / (1024.f * 1024.f)
	);

	wprintf(
		L"\n"
		L"Destination\n"
		L"Path:              %ls\n",
		dst_file_name
	);
#ifdef _DEBUG
	ATL::CHandle dst_file(CreateFileW(dst_file_name, GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
#else
	ATL::CHandle dst_file(CreateFileW(dst_file_name, GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
#endif
	if (dst_file == INVALID_HANDLE_VALUE)
	{
		_CrtDbgBreak();
		dst_file.Detach();
		die();
	}
	FILE_DISPOSITION_INFO dispos = { TRUE };
	ATLENSURE(SetFileInformationByHandle(dst_file, FileDispositionInfo, &dispos, sizeof dispos));
	FSCTL_SET_INTEGRITY_INFORMATION_BUFFER set_integrity = { get_integrity.ChecksumAlgorithm, 0, get_integrity.Flags };
	if (!DeviceIoControl(dst_file, FSCTL_SET_INTEGRITY_INFORMATION, &set_integrity, sizeof set_integrity, nullptr, 0, nullptr, nullptr))
	{
		die();
	}
	if (file_info.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE)
	{
		if (!DeviceIoControl(dst_file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &dummy, nullptr))
		{
			die();
		}
	}
	COPY_TO dst_img(dst_file, get_integrity.ClusterSizeInBytes);
	dst_img.ConstructHeader(src_img.GetDiskSize(), options.block_size.value_or(0), src_img.GetSectorSize(), options.is_fixed.value_or(src_img.IsFixed()));
	wprintf(
		L"Image format:      %hs\n"
		L"Allocation policy: %hs\n"
		L"Disk size:         %llu(%.3fGB)\n"
		L"Block size:        %.1fMB\n",
		dst_img.GetImageTypeName(),
		dst_img.IsFixed() ? "Preallocate" : "Dynamic",
		dst_img.GetDiskSize(),
		dst_img.GetDiskSize() / (1024.f * 1024.f * 1024.f),
		dst_img.GetBlockSize() / (1024.f * 1024.f)
	);

	const UINT32 source_block_size = src_img.GetBlockSize();
	const UINT32 destination_block_size = dst_img.GetBlockSize();
	DUPLICATE_EXTENTS_DATA dup_extent = { src_file };
	if (source_block_size <= destination_block_size)
	{
		for (UINT32 read_block_number = 0; read_block_number < src_img.GetTableEntriesCount(); read_block_number++)
		{
			if (const std::optional<UINT64> read_physical_address = src_img[read_block_number])
			{
				const UINT64 read_virtual_address = 1ULL * source_block_size * read_block_number;
				const UINT32 write_virtual_block_number = static_cast<UINT32>(read_virtual_address / destination_block_size);
				const UINT32 write_virtual_block_offset = static_cast<UINT32>(read_virtual_address % destination_block_size);
				dup_extent.SourceFileOffset.QuadPart = *read_physical_address;
				dup_extent.TargetFileOffset.QuadPart = dst_img.AllocateBlockForWrite(write_virtual_block_number) + write_virtual_block_offset;
				dup_extent.ByteCount.QuadPart = source_block_size;
				_ASSERTE(dup_extent.SourceFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
				_ASSERTE(dup_extent.TargetFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
				_ASSERTE(dup_extent.ByteCount.QuadPart % get_integrity.ClusterSizeInBytes == 0);
				if (!DeviceIoControl(dst_file, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &dummy, nullptr))
				{
					_CrtDbgBreak();
					die();
				}
			}
		}
	}
	else
	{
		for (UINT32 read_block_number = 0; read_block_number < src_img.GetTableEntriesCount(); read_block_number++)
		{
			if (const std::optional<UINT64> read_physical_address = src_img[read_block_number])
			{
				for (UINT32 i = 0; i < source_block_size / destination_block_size; i++)
				{
					const UINT64 read_virtual_address = 1ULL * source_block_size * read_block_number;
					const UINT32 read_block_offset = destination_block_size * i;
					const UINT32 write_virtual_block_number = static_cast<UINT32>((read_virtual_address + read_block_offset) / destination_block_size);
					dup_extent.SourceFileOffset.QuadPart = *read_physical_address + read_block_offset;
					dup_extent.TargetFileOffset.QuadPart = dst_img.AllocateBlockForWrite(write_virtual_block_number);
					dup_extent.ByteCount.QuadPart = destination_block_size;
					_ASSERTE(dup_extent.SourceFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
					_ASSERTE(dup_extent.TargetFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
					_ASSERTE(dup_extent.ByteCount.QuadPart % get_integrity.ClusterSizeInBytes == 0);
					if (!DeviceIoControl(dst_file, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &dummy, nullptr))
					{
						_CrtDbgBreak();
						die();
					}
				}
			}
		}
	}

	_ASSERT(dst_img.IsAligned());
	dst_img.WriteHeader();
	dispos = { FALSE };
	ATLENSURE(SetFileInformationByHandle(dst_file, FileDispositionInfo, &dispos, sizeof dispos));
}
[[noreturn]]
void usage()
{
	fputws(
		L"Make VHD/VHDX that shares data blocks with source.\n"
		L"\n"
		L"MakeVHDX [-fixed | -dynamic] [-sN] Source [Destination]\n"
		L"\n"
		L"Source       Specifies conversion source.\n"
		L"Destination  Specifies conversion destination.\n"
		L"             If not specified, use file extension exchanged with \".vhd\" and \".vhdx\".\n"
		L"-fixed       Make output image fixed file size type.\n"
		L"-dynamic     Make output image variable file size type.\n"
		L"             If neither is specified, will be same type as source.\n"
		L"-s           Specifies output image block size by 1MB. It must be power of 2.\n"
		L"             Ignore this indication when output is fixed VHD.\n",
		stderr);
	ExitProcess(EXIT_FAILURE);
}
int __cdecl wmain(int argc, PWSTR argv[])
{
	ATLENSURE(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32));
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
	setlocale(LC_ALL, "");
	_setmode(_fileno(stdout), _O_WTEXT);

	if (argc < 2)
	{
		usage();
	}
	PCWSTR source = nullptr;
	PCWSTR destination = nullptr;
	Option options;
	for (int i = 1; i < argc; i++)
	{
		if (_wcsicmp(argv[i], L"-fixed") == 0)
		{
			if (options.is_fixed)
			{
				usage();
			}
			options.is_fixed = true;
		}
		else if (_wcsicmp(argv[i], L"-dynamic") == 0)
		{
			if (options.is_fixed)
			{
				usage();
			}
			options.is_fixed = false;
		}
		else if (_wcsnicmp(argv[i], L"-s", 2) == 0)
		{
			if (options.block_size || wcslen(argv[i]) < 3)
			{
				usage();
			}
			options.block_size = wcstoul(argv[i] + 2, nullptr, 0);
		}
		else if (source == nullptr)
		{
			source = argv[i];
		}
		else if (destination == nullptr)
		{
			destination = argv[i];
		}
		else
		{
			usage();
		}
	}
	if (source == nullptr)
	{
		usage();
	}
	WCHAR destination_buffer[PATHCCH_MAX_CCH];
	if (destination == nullptr)
	{
		ATL::AtlCrtErrorCheck(wcscpy_s(destination_buffer, source));
		destination = destination_buffer;
		if (_wcsicmp(PathFindExtensionW(source), L".vhd") == 0)
		{
			ATLENSURE_SUCCEEDED(PathCchRenameExtension(destination_buffer, PATHCCH_MAX_CCH, L".vhdx"));
		}
		else if (_wcsicmp(PathFindExtensionW(source), L".vhdx") == 0)
		{
			ATLENSURE_SUCCEEDED(PathCchRenameExtension(destination_buffer, PATHCCH_MAX_CCH, L".vhd"));
		}
		else
		{
			usage();
		}
	}

	if (_wcsicmp(PathFindExtensionW(source), L".vhd") == 0)
	{
		if (_wcsicmp(PathFindExtensionW(destination), L".vhd") == 0)
		{
			ConvertImage<VHD, VHD>(source, destination, options);
		}
		else if (_wcsicmp(PathFindExtensionW(destination), L".vhdx") == 0)
		{
			ConvertImage<VHD, VHDX>(source, destination, options);
		}
		else
		{
			usage();
		}
	}
	else if (_wcsicmp(PathFindExtensionW(source), L".vhdx") == 0)
	{
		if (_wcsicmp(PathFindExtensionW(destination), L".vhd") == 0)
		{
			ConvertImage<VHDX, VHD>(source, destination, options);
		}
		else if (_wcsicmp(PathFindExtensionW(destination), L".vhdx") == 0)
		{
			ConvertImage<VHDX, VHDX>(source, destination, options);
		}
		else
		{
			usage();
		}
	}

	_putws(L"\nDone.");
}