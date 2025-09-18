#include <windows.h>
#include <wil/result.h>
#include <bit>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "ConvertImage.h"
#include <crtdbg.h>

[[noreturn]]
void usage()
{
	fputs(
		"Make VHD/VHDX that shares data blocks with source.\n"
		"\n"
		"MakeVHDX [-fixed|-dynamic] [-b<N>] [-sparse|-nosparse] <Source> [<Destination>]\n"
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
		"-nosparse    Make output image is non-sparse file.\n"
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
		else if (_wcsicmp(argv[i], L"-nosparse") == 0)
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
#ifdef _DEBUG
		// because QEMU's autodetection will mistakenly identify fixed VHD as RAW.
		const bool s_is_vhd = (_wcsicmp(std::filesystem::path(source).extension().c_str(), L".vhd") == 0);
		const bool d_is_vhd = (_wcsicmp(std::filesystem::path(destination).extension().c_str(), L".vhd") == 0);
		setlocale(LC_CTYPE, ".utf8");
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);
		wil::unique_pipe ps(_popen("Powershell.exe -Command -", "w"));
		if (d_is_vhd)
		{
			fprintf(ps.get(), "$VHD = Get-VHD '%ls';", destination);
			fprintf(ps.get(), R"(if($VHD.Alignment -EQ 1){Write-Output "$([char]27)[92m"}else{Write-Output "$([char]27)[91m";Write-Output '[BUG]'})");
			fprintf(ps.get(), R"(Write-Output "$([char]27)[3F";)" "\n");
			fprintf(ps.get(), "$VHD | Format-List Path,Alignment;");
			fprintf(ps.get(), R"(Write-Output "$([char]27)[0m$([char]27)[4F";)" "\n");
		}
		fprintf(ps.get(), R"(Write-Output "$([char]27)[93m";)");
		fprintf(ps.get(), R"(qemu-img.exe compare -p %s "%ls" %s "%ls";)", s_is_vhd ? "-f vpc" : "", source, d_is_vhd ? "-F vpc" : "", destination);
		fprintf(ps.get(), R"(Write-Output "$([char]27)[2F$([char]27)[0m";)" "\n");
#endif
	}
	catch (const wil::ResultException& e)
	{
		fputs("\x1B[91m", stderr);
#ifdef _DEBUG
		fputs(e.what(), stderr);
#else
		wil::unique_hlocal_string error_msg;
		FAIL_FAST_IF(!FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e.GetErrorCode(), 0, reinterpret_cast<PWSTR>(&error_msg), 0, nullptr));
		fputws(error_msg.get(), stderr);
#endif
		fputs("\x1B[0m", stderr);
		return EXIT_FAILURE;
	}
	catch (const std::exception& e)
	{
		fputs("\x1B[91m", stderr);
		fputs(e.what(), stderr);
		fputs("\x1B[0m\n", stderr);
		return EXIT_FAILURE;
	}
}