#include <windows.h>
#include <bit>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <wil/result.h>
#include "ConvertImage.h"
#include <crtdbg.h>

[[noreturn]]
void usage()
{
	fputs(
		"Make VHD/VHDX that shares data blocks with source.\n"
		"\n"
		"MakeVHDX [-fixed|-dynamic] [-b<N>] [-sparse|-unsparse] <Source> [<Destination>]\n"
		"\n"
		"Source       Specifies conversion source.\n"
		"Destination  Specifies conversion destination.\n"
		"             If not specified, will use file extension\n"
		"             exchanged with \".vhd\" when the source is \".vhdx\", exchanged with \".vhdx\" otherwise.\n"
		"-fixed       Make output image is fixed file size type.\n"
		"-dynamic     Make output image is variable file size type.\n"
		"             If neither is specified, will be same type as source.\n"
		"-b           Specifies output image block size by 1MB. It must be power of 2.\n"
		"             Silently ignore, if output image type doesn't use blocks. (Such as fixed VHD)\n"
		"-sparse      Make output image is sparse file.\n"
		"-unsparse    Make output image is non-sparse file.\n"
		"             By default, output file is also sparse only when source file is sparse.\n"
		"\n"
		"Supported Image Types and File Extensions\n"
		"VHDX : .vhdx\n"
		"VHD  : .vhd\n"
		"RAW  : .* (Other than above)\n",
		stderr);
	ExitProcess(EXIT_FAILURE);
}
int wmain(int argc, PWSTR argv[])
{
	FAIL_FAST_IF_WIN32_BOOL_FALSE(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32));
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
	setlocale(LC_CTYPE, "");

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
			if (options.fixed)
			{
				usage();
			}
			options.fixed = true;
		}
		else if (_wcsicmp(argv[i], L"-dynamic") == 0)
		{
			if (options.fixed)
			{
				usage();
			}
			options.fixed = false;
		}
		else if (_wcsicmp(argv[i], L"-sparse") == 0)
		{
			if (options.sparse)
			{
				usage();
			}
			options.sparse = true;
		}
		else if (_wcsicmp(argv[i], L"-unsparse") == 0)
		{
			if (options.sparse)
			{
				usage();
			}
			options.sparse = false;
		}
		else if (_wcsnicmp(argv[i], L"-b", 2) == 0)
		{
			if (options.block_size || wcslen(argv[i]) < 3)
			{
				usage();
			}
			options.block_size = wcstoul(argv[i] + 2, nullptr, 0) * 1024 * 1024;
			if (!std::has_single_bit(options.block_size))
			{
				usage();
			}
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
	std::filesystem::path destination_buffer;
	if (destination == nullptr)
	{
		destination_buffer = source;
		if (_wcsicmp(destination_buffer.extension().c_str(), L".vhdx") == 0)
		{
			destination_buffer.replace_extension(L".vhd");
		}
		else
		{
			destination_buffer.replace_extension(L".vhdx");
		}
		destination = destination_buffer.c_str();
	}

	try
	{
		ConvertImage(source, destination, options);
		puts("\nDone.");
	}
	catch (wil::ResultException e)
	{
#ifdef _DEBUG
		fputs(e.what(), stderr);
#else
		wil::unique_hlocal_string error_msg;
		FAIL_FAST_IF(!FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e.GetErrorCode(), 0, reinterpret_cast<PWSTR>(&error_msg), 0, nullptr));
		fputws(error_msg.get(), stderr);
#endif
		return EXIT_FAILURE;
	}
	catch (std::exception e)
	{
		fputs(e.what(), stderr);
		fputs("\n", stderr);
		return EXIT_FAILURE;
	}
}