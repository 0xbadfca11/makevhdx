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
#include "../VHD.hpp"

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
	if (vhd_footer.DiskType == VHDType::Fixed)
	{
		ExitProcess(EXIT_SUCCESS);
	}
	else if (vhd_footer.DiskType == VHDType::Difference)
	{
		ExitProcess(EXIT_FAILURE);
	}
	else if (vhd_footer.DiskType != VHDType::Dynamic || vhd_footer.DataOffset == UINT64_MAX)
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
	auto vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(_byteswap_ulong(vhd_dyn_header.MaxTableEntries));
	ReadFileWithOffset(h, vhd_block_allocation_table.get(), _byteswap_ulong(vhd_dyn_header.MaxTableEntries) * sizeof(UINT32), _byteswap_uint64(vhd_dyn_header.TableOffset));
	ULONG unaligned = fsize.QuadPart % 4096;
	for (UINT32 i = 0; i < _byteswap_ulong(vhd_dyn_header.MaxTableEntries); i++)
	{
		if (vhd_block_allocation_table[i] != UINT32_MAX)
		{
			unaligned |= (vhd_block_allocation_table[i] * 512 + (std::max)(_byteswap_ulong(vhd_dyn_header.BlockSize) / (512 * CHAR_BIT), 512UL)) % 4096;
		}
	}
	printf("Alignment\t%u\n", !unaligned);
	ExitProcess(EXIT_SUCCESS);
}