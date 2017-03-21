#define WIN32_LEAN_AND_MEAN
#define STRICT
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <windows.h>
#include <initguid.h>
#include <pathcch.h>
#include <winioctl.h>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "crc32c.h"
#pragma comment(lib, "pathcch")

constexpr UINT32 byteswap32(UINT32 v) noexcept
{
	return v >> 24 & 0xFF | v >> 8 & 0xFF << 8 | v << 8 & 0xFF << 16 | v << 24 & 0xFF << 24;
}
static_assert(0x11335577 == byteswap32(0x77553311), "");
constexpr UINT64 byteswap64(UINT64 v) noexcept
{
	return v >> 56 & 0xFFULL
		| v >> 40 & 0xFFULL << 8
		| v >> 24 & 0xFFULL << 16
		| v >> 8 & 0xFFULL << 24
		| v << 8 & 0xFFULL << 32
		| v << 24 & 0xFFULL << 40
		| v << 40 & 0xFFULL << 48
		| v << 56 & 0xFFULL << 56;
}
static_assert(0x1133557722446688 == byteswap64(0x8866442277553311), "");
template <typename Ty1, typename Ty2>
constexpr Ty1 ROUNDUP(Ty1 number, Ty2 num_digits) noexcept
{
	return (number + num_digits - 1) / num_digits * num_digits;
}
static_assert(ROUNDUP(42, 15) == 45, "");
template <typename Ty1, typename Ty2>
constexpr Ty1 CEILING(Ty1 number, Ty2 significance) noexcept
{
	return (number + significance - 1) / significance;
}
static_assert(CEILING(42, 15) == 3, "");
template <typename Ty1, typename Ty2>
constexpr Ty1 FLOOR(Ty1 number, Ty2 significance) noexcept
{
	return number / significance;
}
static_assert(FLOOR(42, 15) == 2, "");
#pragma region VHD
const UINT32 VHD_FOOTER_OFFSET = 512;
const UINT64 VHD_SECTOR_SIZE = 512;
const UINT64 VHD_COOKIE = 0x78697463656e6f63;
const UINT32 VHD_VALID_FEATURE_MASK = byteswap32(3);
const UINT32 VHD_VERSION = byteswap32(MAKELONG(0, 1));
const UINT64 VHD_INVALID_OFFSET = UINT64_MAX;
const UINT64 VHD_DYNAMIC_COOKIE = 0x6573726170737863;
const UINT32 VHD_DYNAMIC_VERSION = byteswap32(MAKELONG(0, 1));
const UINT32 VHD_4K_ALIGNED_MASK = 0b111;
const UINT32 VHD_4K_ALIGNED_LEAF = 0b111;
const UINT32 VHD_UNUSED_BAT_ENTRY = UINT32_MAX;
using VHD_BAT_ENTRY = UINT32;
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
	UINT8  UniqueId[16];
	UINT8  SavedState;
};
struct VHD_DYNAMIC_HEADER
{
	UINT64 Cookie;
	UINT64 DataOffset;
	UINT64 TableOffset;
	UINT32 HeaderVersion;
	UINT32 MaxTableEntries;
	UINT32 BlockSize;
	UINT32 Checksum;
};
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
const UINT64 VHDX_LOGICAL_SECTOR_SIZE = VHD_SECTOR_SIZE;
const UINT64 VHDX_PHYSICAL_SECTOR_SIZE = 4096;
const UINT32 VHDX_REGION_TABLE_HEADER_TRUNCATE_SIZE = 4 * 1024;
const UINT64 VHDX_BAT_LOCATION = VHDX_METADATA_LOCATION + VHDX_METADATA_LENGTH;
enum : UINT32
{
	VHDX_FILE_IDENTIFIER_OFFSET = 0,
	VHDX_HEADER1_OFFSET = 1 * 64 * 1024,
	VHDX_HEADER2_OFFSET = 2 * 64 * 1024,
	VHDX_REGION_TABLE_HEADER1_OFFSET = 3 * 64 * 1024,
	VHDX_REGION_TABLE_HEADER2_OFFSET = 4 * 64 * 1024,
};
DEFINE_GUID(BAT, 0x2dc27766, 0xf623, 0x4200, 0x9d, 0x64, 0x11, 0x5e, 0x9b, 0xfd, 0x4a, 0x08);
DEFINE_GUID(Metadata, 0x8B7CA206, 0x4790, 0x4B9A, 0xB8, 0xFE, 0x57, 0x5F, 0x05, 0x0F, 0x88, 0x6E);
DEFINE_GUID(FileParameters, 0xCAA16737, 0xFA36, 0x4D43, 0xB3, 0xB6, 0x33, 0xF0, 0xAA, 0x44, 0xE7, 0x6B);
DEFINE_GUID(VirtualDiskSize, 0x2FA54224, 0xCD1B, 0x4876, 0xB2, 0x11, 0x5D, 0xBE, 0xD8, 0x3B, 0xF4, 0xB8);
DEFINE_GUID(Page83Data, 0xBECA12AB, 0xB2E6, 0x4523, 0x93, 0xEF, 0xC3, 0x09, 0xE0, 0x00, 0xC7, 0x46);
DEFINE_GUID(LogicalSectorSize, 0x8141BF1D, 0xA96F, 0x4709, 0xBA, 0x47, 0xF2, 0x33, 0xA8, 0xFA, 0xAB, 0x5F);
DEFINE_GUID(PhysicalSectorSize, 0xCDA348C7, 0x445D, 0x4471, 0x9C, 0xC9, 0xE9, 0x88, 0x52, 0x51, 0xC5, 0x56);
struct VHDX_FILE_IDENTIFIER
{
	UINT64 Signature;
	UINT16 Creator[256];
	UINT8  Padding[4 * 1024 - 8 - 512];
};
static_assert(sizeof(VHDX_FILE_IDENTIFIER) == 4 * 1024, "");
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
static_assert(sizeof(VHDX_HEADER) == 4 * 1024, "");
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
static_assert(sizeof(VHDX_REGION_TABLE_HEADER) == 64 * 1024, "");
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
	_Field_size_full_(EntryCount) VHDX_METADATA_TABLE_ENTRY MetadataTableEntries[VHDX_MAX_ENTRIES / 16];
};
static_assert(sizeof(VHDX_METADATA_TABLE_HEADER) == 4 * 1024, "");
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
	UINT8  Padding[4 * 1024 - 40];
};
static_assert(sizeof(VHDX_METADATA_PACKED) == 4 * 1024, "");
#pragma endregion
template <typename Ty>
bool VHDXChecksumValidate(Ty header) noexcept
{
	UINT32 Checksum = header.Checksum;
	header.Checksum = 0;
	return crc32c_append(0, static_cast<uint8_t*>(static_cast<void*>(&header)), sizeof(Ty)) == Checksum;
}
template <typename Ty>
void VHDXUpdateChecksum(Ty* header) noexcept
{
	header->Checksum = 0;
	header->Checksum = crc32c_append(0, static_cast<uint8_t*>(static_cast<void*>(header)), sizeof(Ty));
}
template <typename Ty>
bool IsTruncationSafe(const Ty& data, int limiter) noexcept
{
	const uint8_t* head = static_cast<const uint8_t*>(static_cast<const void*>(&data));
	for (int i = limiter; i < sizeof(Ty); i++)
	{
		if (head[i] != 0)
		{
			return false;
		}
	}
	return true;
}
[[noreturn]]
void die()
{
	PWSTR err_msg;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), 0, reinterpret_cast<PWSTR>(&err_msg), 0, nullptr);
	fputws(err_msg, stderr);
	ExitProcess(EXIT_FAILURE);
}
template <typename Ty>
Ty* ReadFileWithOffset(
	_In_ HANDLE hFile,
	_Out_writes_bytes_(nNumberOfBytesToRead) __out_data_source(FILE) Ty* lpBuffer,
	_In_ ULONG nNumberOfBytesToRead,
	_In_ ULONGLONG Offset
) noexcept
{
	ULONG read;
	OVERLAPPED o = {};
	o.Offset = static_cast<ULONG>(Offset);
	o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
	if (!ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &read, &o))
	{
		_CrtDbgBreak();
		die();
	}
	if (read != nNumberOfBytesToRead)
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
) noexcept
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
	ULONG write;
	OVERLAPPED o = {};
	o.Offset = static_cast<ULONG>(Offset);
	o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
	if (!WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &write, &o))
	{
		_CrtDbgBreak();
		die();
	}
	if (write != nNumberOfBytesToWrite)
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
	WriteFileWithOffset(hFile, &lpBuffer, sizeof(Ty), Offset);
}
int __cdecl wmain(int argc, PWSTR argv[])
{
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
	setlocale(LC_ALL, "");
	if (argc < 2)
	{
		fputs(
			"Make VHDX that shares data blocks with source VHD.\n"
			"\n"
			"MakeVHDX VHD [VHDX]\n"
			"\n"
			"VHD          Specifies a VHD file to convert.\n"
			"             VHD file must be on volume having Block Cloning feature.\n"
			"VHDX         Specifies new VHDX file name.\n"
			"             Converted VHDX must be placed on the same volume as source VHD.\n"
			"             If not specified, using file name that extension of VHD file name changed to \".vhdx\" is used.\n",
			stderr);
		return EXIT_FAILURE;
	}

	auto vhdx_path = std::make_unique<WCHAR[]>(PATHCCH_MAX_CCH);
	if (argc < 3)
	{
		argv[2] = vhdx_path.get();
		ATL::Checked::wcscpy_s(argv[2], PATHCCH_MAX_CCH, argv[1]);
		PathCchRemoveExtension(argv[2], PATHCCH_MAX_CCH);
		PathCchAddExtension(argv[2], PATHCCH_MAX_CCH, L"vhdx");
	}
	ATL::CHandle vhd(CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
	if (vhd == INVALID_HANDLE_VALUE)
	{
		_CrtDbgBreak();
		vhd.Detach();
		die();
	}
	ULONG fs_flags;
	ATLENSURE(GetVolumeInformationByHandleW(vhd, nullptr, 0, nullptr, nullptr, &fs_flags, nullptr, 0));
	if (!(fs_flags & FILE_SUPPORTS_BLOCK_REFCOUNTING))
	{
		fputs("Filesystem doesn't support Block Cloning feature.\n", stderr);
		return EXIT_FAILURE;
	}
	BY_HANDLE_FILE_INFORMATION file_info;
	ATLENSURE(GetFileInformationByHandle(vhd, &file_info));
	if (file_info.dwFileAttributes & FILE_ATTRIBUTE_INTEGRITY_STREAM)
	{
		fputs("Source VHD file has integrity stream.\n", stderr);
		return EXIT_FAILURE;
	}
	if (file_info.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE)
	{
		fputs("Source VHD file is sparse.\n", stderr);
		return EXIT_FAILURE;
	}
	ULONG dummy;
	FSCTL_GET_INTEGRITY_INFORMATION_BUFFER get_integrity;
	if (!DeviceIoControl(vhd, FSCTL_GET_INTEGRITY_INFORMATION, nullptr, 0, &get_integrity, sizeof get_integrity, &dummy, nullptr))
	{
		die();
	}
	if (get_integrity.ClusterSizeInBytes != 4 * 1024)
	{
		fputs("Cluster size isn't 4KB.\n", stderr);
		return EXIT_FAILURE;
	}

#ifdef _DEBUG
	ATL::CHandle vhdx(CreateFileW(argv[2], FILE_READ_DATA | GENERIC_WRITE | DELETE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_FLAG_RANDOM_ACCESS, nullptr));
#else
	ATL::CHandle vhdx(CreateFileW(argv[2], FILE_READ_DATA | GENERIC_WRITE | DELETE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_FLAG_RANDOM_ACCESS, nullptr));
#endif
	if (vhdx == INVALID_HANDLE_VALUE)
	{
		_CrtDbgBreak();
		vhdx.Detach();
		die();
	}
	FILE_DISPOSITION_INFO dispos = { TRUE };
	SetFileInformationByHandle(vhdx, FileDispositionInfo, &dispos, sizeof dispos);
	FSCTL_SET_INTEGRITY_INFORMATION_BUFFER set_integrity = { CHECKSUM_TYPE_NONE };
	if (!DeviceIoControl(vhdx, FSCTL_SET_INTEGRITY_INFORMATION, &set_integrity, sizeof set_integrity, nullptr, 0, nullptr, nullptr))
	{
		die();
	}

	VHD_FOOTER vhd_footer;
	ReadFileWithOffset(vhd, &vhd_footer, file_info.nFileSizeHigh * (1ULL << 32) + file_info.nFileSizeLow - VHD_FOOTER_OFFSET);
	if (vhd_footer.Cookie != VHD_COOKIE)
	{
		fputs("Missing VHD signature.\n", stderr);
		return EXIT_FAILURE;
	}
	if ((vhd_footer.Features & ~VHD_VALID_FEATURE_MASK) != 0)
	{
		fputs("Unknown feature flag.\n", stderr);
		return EXIT_FAILURE;
	}
	if (vhd_footer.FileFormatVersion != VHD_VERSION)
	{
		fputs("Unknown VHD version.\n", stderr);
		return EXIT_FAILURE;
	}
	if (vhd_footer.DiskType != VHDType::Dynamic)
	{
		fputs("Only dynamic VHD is supported.\n", stderr);
		return EXIT_FAILURE;
	}

	VHD_DYNAMIC_HEADER vhd_dyn_header;
	ReadFileWithOffset(vhd, &vhd_dyn_header, byteswap64(vhd_footer.DataOffset));
	if (vhd_dyn_header.Cookie != VHD_DYNAMIC_COOKIE)
	{
		fputs("Missing Dynamic VHD signature.\n", stderr);
		return EXIT_FAILURE;
	}
	if (vhd_dyn_header.DataOffset != VHD_INVALID_OFFSET)
	{
		fputs("Unknown extra data.\n", stderr);
		return EXIT_FAILURE;
	}
	if (vhd_dyn_header.HeaderVersion != VHD_DYNAMIC_VERSION)
	{
		fputs("Unknown Dynamic VHD version.\n", stderr);
		return EXIT_FAILURE;
	}
	if (vhd_dyn_header.BlockSize != byteswap32(2 * 1024 * 1024))
	{
		fputs("Only 2MB BlockSize is supported.\n", stderr);
		return EXIT_FAILURE;
	}

	const UINT32 vhd_block_size = byteswap32(vhd_dyn_header.BlockSize);
	const UINT32 vhd_bitmap_size = CEILING(vhd_block_size, 512 * 8);
	_RPTN(_CRT_WARN, "CurrentSize == %llu(%.3fGB)\n", byteswap64(vhd_footer.CurrentSize), byteswap64(vhd_footer.CurrentSize) / (1024.f * 1024.f * 1024.f));
	_RPTN(_CRT_WARN, "BlockSize == %luKB\n", vhd_block_size / 1024);
	UINT64 using_blocks_count = 0;
	const UINT32 max_table_entries = byteswap32(vhd_dyn_header.MaxTableEntries);
	_RPTN(_CRT_WARN, "MaxTableEntries == %lu\n", max_table_entries);
	auto block_alloc_table = std::make_unique<VHD_BAT_ENTRY[]>(max_table_entries);
	ReadFileWithOffset(vhd, block_alloc_table.get(), max_table_entries * sizeof(VHD_BAT_ENTRY), byteswap64(vhd_dyn_header.TableOffset));
	for (UINT32 i = 0; i < max_table_entries; i++)
	{
		if (block_alloc_table[i] != VHD_UNUSED_BAT_ENTRY)
		{
			block_alloc_table[i] = byteswap32(block_alloc_table[i]);
			if ((block_alloc_table[i] & VHD_4K_ALIGNED_MASK) != VHD_4K_ALIGNED_LEAF)
			{
				_CrtDbgBreak();
				fputs("VHD isn't aligned 4KB.\n", stderr);
				return EXIT_FAILURE;
			}
			using_blocks_count++;
		}
	}
	_ASSERT(using_blocks_count <= UINT32_MAX);

	VHDX_FILE_IDENTIFIER vhdx_file_indentifier = { VHDX_SIGNATURE };
	VHDX_HEADER vhdx_header = { VHDX_HEADER_SIGNATURE, 0, 0, {}, {}, {}, 0, VHDX_CURRENT_VERSION, VHDX_LOG_LENGTH, VHDX_LOG_LOCATION };
	VHDXUpdateChecksum(&vhdx_header);
	_ASSERT(VHDXChecksumValidate(vhdx_header));

	VHDX_METADATA_PACKED vhdx_metadata_packed =
	{
		{ vhd_block_size },
		byteswap64(vhd_footer.CurrentSize),
		VHDX_LOGICAL_SECTOR_SIZE,
		VHDX_PHYSICAL_SECTOR_SIZE,
	};
	ATLENSURE_SUCCEEDED(CoCreateGuid(&vhdx_metadata_packed.Page83Data));

	VHDX_METADATA_TABLE_HEADER vhdx_metadata_table_header =
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

	const UINT32 chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
	_RPTN(_CRT_WARN, "Chuck Ratio == %lu\n", chuck_ratio);
	const UINT32 data_blocks_count = static_cast<UINT32>(CEILING(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize));
	_RPTN(_CRT_WARN, "DataBlocksCount == %lu\n", data_blocks_count);
	_ASSERT(data_blocks_count == max_table_entries);
	const UINT32 bitmap_blocks_count = CEILING(data_blocks_count, chuck_ratio);
	_RPTN(_CRT_WARN, "BitmapBlocksCount == %lu\n", bitmap_blocks_count);
	const UINT32 total_bat_entries = data_blocks_count + FLOOR(data_blocks_count - 1, chuck_ratio);
	_RPTN(_CRT_WARN, "TotalBATEntries == %lu\n", total_bat_entries);

	const UINT32 vhdx_bat_length = ROUNDUP(total_bat_entries * static_cast<UINT32>(sizeof(VHDX_BAT_ENTRY)), 1024 * 1024);
	VHDX_REGION_TABLE_HEADER vhdx_region_table_header = { VHDX_REGION_HEADER_SIGNATURE };
	VHDX_REGION_TABLE_ENTRY vhdx_region_table_entry[] =
	{
		{ Metadata, VHDX_METADATA_LOCATION, VHDX_METADATA_LENGTH, 1 },
		{ BAT, VHDX_BAT_LOCATION, vhdx_bat_length, 1 }
	};
	vhdx_region_table_header.EntryCount = ARRAYSIZE(vhdx_region_table_entry);
	for (int i = 0; i < ARRAYSIZE(vhdx_region_table_entry); i++)
	{
		vhdx_region_table_header.RegionTableEntries[i] = vhdx_region_table_entry[i];
	}
	VHDXUpdateChecksum(&vhdx_region_table_header);
	_ASSERT(VHDXChecksumValidate(vhdx_region_table_header));
	_ASSERT(IsTruncationSafe(vhdx_region_table_header, VHDX_REGION_TABLE_HEADER_TRUNCATE_SIZE));

	FILE_END_OF_FILE_INFO eof_info;
	eof_info.EndOfFile.QuadPart = VHDX_BAT_LOCATION + vhdx_bat_length + using_blocks_count * vhdx_metadata_packed.VhdxFileParameters.BlockSize;
	_RPTN(_CRT_WARN, "Final File Size == %llu(%.3fGB)\n", eof_info.EndOfFile.QuadPart, eof_info.EndOfFile.QuadPart / (1024.f * 1024.f * 1024.f));
	SetFileInformationByHandle(vhdx, FileEndOfFileInfo, &eof_info, sizeof eof_info);

	const UINT32 vhdx_bat_size = ROUNDUP(total_bat_entries * static_cast<UINT32>(sizeof(VHDX_BAT_ENTRY)), 4 * 1024);
	auto vhdx_bat = std::make_unique<VHDX_BAT_ENTRY[]>(vhdx_bat_size / sizeof(VHDX_BAT_ENTRY));
	UINT64 vhdx_write_offset = VHDX_BAT_LOCATION + vhdx_bat_length;
	for (UINT32 vhd_bat_index = 0, vhdx_bat_index = 0; vhd_bat_index < max_table_entries; vhd_bat_index++, vhdx_bat_index++)
	{
		bool is_bitmap_entry = (vhd_bat_index + 1) % (chuck_ratio + 1) == 0;
		if (is_bitmap_entry)
		{
			vhdx_bat_index++;
		}
		if (block_alloc_table[vhd_bat_index] != VHD_UNUSED_BAT_ENTRY)
		{
			_ASSERT((block_alloc_table[vhd_bat_index] & VHD_4K_ALIGNED_MASK) == VHD_4K_ALIGNED_LEAF);
			const UINT64 vhd_read_offset = block_alloc_table[vhd_bat_index] * VHD_SECTOR_SIZE + vhd_bitmap_size;
			_ASSERT(vhd_read_offset % (4 * 1024) == 0);
			_ASSERT(vhdx_write_offset >= VHDX_BAT_LOCATION + vhdx_bat_length);

			DUPLICATE_EXTENTS_DATA dup_extent = { vhd };
			dup_extent.SourceFileOffset.QuadPart = vhd_read_offset;
			dup_extent.TargetFileOffset.QuadPart = vhdx_write_offset;
			dup_extent.ByteCount.QuadPart = vhd_block_size;
			if (!DeviceIoControl(vhdx, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &dummy, nullptr))
			{
				_CrtDbgBreak();
				die();
			}

			vhdx_bat[vhdx_bat_index].FileOffsetMB = vhdx_write_offset / 1024 / 1024;
			vhdx_bat[vhdx_bat_index].State = PAYLOAD_BLOCK_FULLY_PRESENT;
			vhdx_write_offset += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
		}
	}

	WriteFileWithOffset(vhdx, vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_OFFSET);
	WriteFileWithOffset(vhdx, vhdx_header, VHDX_HEADER1_OFFSET);
	WriteFileWithOffset(vhdx, vhdx_header, VHDX_HEADER2_OFFSET);
	WriteFileWithOffset(vhdx, &vhdx_region_table_header, VHDX_REGION_TABLE_HEADER_TRUNCATE_SIZE, VHDX_REGION_TABLE_HEADER1_OFFSET);
	WriteFileWithOffset(vhdx, &vhdx_region_table_header, VHDX_REGION_TABLE_HEADER_TRUNCATE_SIZE, VHDX_REGION_TABLE_HEADER2_OFFSET);
	WriteFileWithOffset(vhdx, vhdx_metadata_table_header, VHDX_METADATA_LOCATION);
	WriteFileWithOffset(vhdx, vhdx_metadata_packed, VHDX_METADATA_LOCATION + VHDX_METADATA_START_OFFSET);
	WriteFileWithOffset(vhdx, vhdx_bat.get(), vhdx_bat_size, VHDX_BAT_LOCATION);

	dispos = { FALSE };
	SetFileInformationByHandle(vhdx, FileDispositionInfo, &dispos, sizeof dispos);
}