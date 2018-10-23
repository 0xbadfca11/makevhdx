#define WIN32_LEAN_AND_MEAN
#define STRICT_GS_ENABLED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <windows.h>
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <memory>

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
[[noreturn]]
void die()
{
	_CrtDbgBreak();
	PCWSTR err_msg;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), 0, reinterpret_cast<PWSTR>(&err_msg), 0, nullptr);
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
int __cdecl wmain(int, PWSTR argv[])
{
	setlocale(LC_ALL, "");
	HANDLE h = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (h == INVALID_HANDLE_VALUE)
	{
		die();
	}
	LARGE_INTEGER fsize;
	ATLENSURE(GetFileSizeEx(h, &fsize));
	printf("FileSize %% 4096\t%llu\n\n", fsize.QuadPart % 4096);
	bool is_legacy = fsize.QuadPart % 512 == 511;
	VHD_FOOTER vhd_footer;
	ReadFileWithOffset(h, &vhd_footer, is_legacy ? sizeof vhd_footer - 1 : sizeof vhd_footer, (fsize.QuadPart - sizeof vhd_footer + 1) / sizeof vhd_footer * sizeof vhd_footer);
	char buffer[0x80];
	printf(
		"DataOffset\t%llu/0x%llX\n"
		"CurrentSize\t%llu(%s)\n"
		"DiskGeometry\t%u/%u/%u\n"
		"DiskType\t%u\n\n",
		_byteswap_uint64(vhd_footer.DataOffset),
		_byteswap_uint64(vhd_footer.DataOffset),
		_byteswap_uint64(vhd_footer.CurrentSize),
		StrFormatByteSize64A(_byteswap_uint64(vhd_footer.CurrentSize), buffer, ARRAYSIZE(buffer)),
		_byteswap_ulong(vhd_footer.DiskGeometry) >> 16,
		_byteswap_ulong(vhd_footer.DiskGeometry) >> 8 & 0xFF,
		_byteswap_ulong(vhd_footer.DiskGeometry) & 0xFF,
		_byteswap_ulong(vhd_footer.DiskType)
	);
	enum
	{
		Fixed = 2,
		Dynamic = 3,
		Difference = 4,
	};
	if (_byteswap_ulong(vhd_footer.DiskType) == Fixed)
	{
		ExitProcess(EXIT_SUCCESS);
	}
	else if (_byteswap_ulong(vhd_footer.DiskType) == Difference)
	{
		ExitProcess(EXIT_FAILURE);
	}
	else if (_byteswap_ulong(vhd_footer.DiskType) != Dynamic || vhd_footer.DataOffset == UINT64_MAX)
	{
		ExitProcess(EXIT_FAILURE);
	}
	VHD_FOOTER vhd_header;
	ReadFileWithOffset(h, &vhd_header, sizeof vhd_header, 0);
	if (memcmp(&vhd_footer, &vhd_header, is_legacy ? sizeof vhd_footer - 1 : sizeof vhd_footer) == 0)
	{
		puts("Footer and header are identical.\n");
	}
	else
	{
		puts("Footer and header are not identical.\n");
		printf(
			"DataOffset\t%llu/0x%llX\n"
			"CurrentSize\t%llu(%s)\n"
			"DiskGeometry\t%u/%u/%u\n"
			"DiskType\t%u\n\n",
			_byteswap_uint64(vhd_header.DataOffset),
			_byteswap_uint64(vhd_header.DataOffset),
			_byteswap_uint64(vhd_header.CurrentSize),
			StrFormatByteSize64A(_byteswap_uint64(vhd_header.CurrentSize), buffer, ARRAYSIZE(buffer)),
			_byteswap_ulong(vhd_header.DiskGeometry) >> 16,
			_byteswap_ulong(vhd_header.DiskGeometry) >> 8 & 0xFF,
			_byteswap_ulong(vhd_header.DiskGeometry) & 0xFF,
			_byteswap_ulong(vhd_header.DiskType)
		);
	}
	VHD_DYNAMIC_HEADER vhd_dyn_header;
	ReadFileWithOffset(h, &vhd_dyn_header, sizeof vhd_dyn_header, _byteswap_uint64(vhd_footer.DataOffset));
	printf(
		"DataOffset\t0x%llX\n"
		"TableOffset\t%llu/0x%llX\n"
		"MaxTableEntries\t%u\n"
		"BlockSize\t%u(%s)\n\n",
		_byteswap_uint64(vhd_dyn_header.DataOffset),
		_byteswap_uint64(vhd_dyn_header.TableOffset),
		_byteswap_uint64(vhd_dyn_header.TableOffset),
		_byteswap_ulong(vhd_dyn_header.MaxTableEntries),
		_byteswap_ulong(vhd_dyn_header.BlockSize),
		StrFormatByteSize64A(_byteswap_ulong(vhd_dyn_header.BlockSize), buffer, ARRAYSIZE(buffer))
	);
	auto vhd_block_allocation_table = std::make_unique<UINT32[]>(_byteswap_ulong(vhd_dyn_header.MaxTableEntries));
	ReadFileWithOffset(h, vhd_block_allocation_table.get(), _byteswap_ulong(vhd_dyn_header.MaxTableEntries) * sizeof(UINT32), _byteswap_uint64(vhd_dyn_header.TableOffset));
	ULONG unaligned = fsize.QuadPart % 4096;
	for (UINT32 i = 0; i < _byteswap_ulong(vhd_dyn_header.MaxTableEntries); i++)
	{
		if (vhd_block_allocation_table[i] != UINT32_MAX)
		{
			unaligned |= (_byteswap_ulong(vhd_block_allocation_table[i]) * 512 + (std::max)(_byteswap_ulong(vhd_dyn_header.BlockSize) / (512 * CHAR_BIT), 512UL)) % 4096;
		}
	}
	printf("Alignment\t%u\n", !unaligned);
	ExitProcess(EXIT_SUCCESS);
}