#define WIN32_LEAN_AND_MEAN
#define STRICT_GS_ENABLED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <windows.h>
#include <pathcch.h>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <memory>
#include <io.h>
#include "ConvertImage.hpp"
#pragma comment(lib, "pathcch")

[[noreturn]]
void usage()
{
	fputws(
		L"Make VHD/VHDX that shares data blocks with source.\n"
		L"\n"
		L"MakeVHDX [-fixed | -dynamic] [-bN] [-sparse] Source [Destination]\n"
		L"\n"
		L"Source       Specifies conversion source.\n"
		L"Destination  Specifies conversion destination.\n"
		L"             If not specified, will use file extension\n"
		L"             exchanged with \".vhd\" when the source is \".vhdx\", exchanged with \".vhdx\" otherwise.\n"
		L"-fixed       Make output image is fixed file size type.\n"
		L"-dynamic     Make output image is variable file size type.\n"
		L"             If neither is specified, will be same type as source.\n"
		L"-b           Specifies output image block size by 1MB. It must be power of 2.\n"
		L"             Silently ignore, if output is image type that doesn't use blocks. (Such as fixed VHD)\n"
		L"-sparse      Make output image is sparse file.\n"
		L"             By default, output file is also sparse only when source file is sparse.\n"
		L"\n"
		L"Supported Image Types and File Extensions\n"
		L" VHDX : .vhdx (.avhdx Disallowed)\n"
		L" VHD  : .vhd  (.avhd  Disallowed)\n"
		L" RAW  : .* (Other than above)\n",
		stderr);
	ExitProcess(EXIT_FAILURE);
}
int __cdecl wmain(_In_ int argc, _Inout_z_ PWSTR argv[])
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
		else if (_wcsicmp(argv[i], L"-sparse") == 0)
		{
			if (options.force_sparse)
			{
				usage();
			}
			options.force_sparse = true;
		}
		else if (_wcsnicmp(argv[i], L"-b", 2) == 0)
		{
			if (options.block_size || wcslen(argv[i]) < 3)
			{
				usage();
			}
			options.block_size = wcstoul(argv[i] + 2, nullptr, 0) * 1024 * 1024;
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
	std::unique_ptr<WCHAR[]> destination_buffer;
	if (destination == nullptr)
	{
		destination_buffer = std::make_unique<WCHAR[]>(PATHCCH_MAX_CCH);
		ATL::Checked::wcscpy_s(destination_buffer.get(), PATHCCH_MAX_CCH, source);
		destination = destination_buffer.get();
		if (_wcsicmp(PathFindExtensionW(source), L".vhdx") == 0)
		{
			ATLENSURE_SUCCEEDED(PathCchRenameExtension(destination_buffer.get(), PATHCCH_MAX_CCH, L".vhd"));
		}
		else
		{
			ATLENSURE_SUCCEEDED(PathCchRenameExtension(destination_buffer.get(), PATHCCH_MAX_CCH, L".vhdx"));
		}
	}

	ConvertImage(source, destination, options);
	_putws(L"\nDone.");
}