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
#include "MakeVHDX.h"
#include "ConvertImage.h"
#pragma comment(lib, "pathcch")

void usage()
{
  fputws(
    L"Make VHD/VHDX that shares data blocks with source.\n"
    L"\n"
    L"MakeVHDX [-fixed | -dynamic] [-bN] [-sparse] Source [Destination]\n"
    L"\n"
    L"Source       Specifies conversion source.\n"
    L"Destination  Specifies conversion destination.\n"
    L"             If not specified, use file extension exchanged with \".vhd\" and \".vhdx\".\n"
    L"-fixed       Make output image is fixed file size type.\n"
    L"-dynamic     Make output image is variable file size type.\n"
    L"             If neither is specified, will be same type as source.\n"
    L"-b           Specifies output image block size by 1MB. It must be power of 2.\n"
    L"             Ignore this indication when output is fixed VHD.\n"
    L"-sparse      Make output image is sparse file.\n",
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