#pragma once
#define WIN32_LEAN_AND_MEAN
#define STRICT_GS_ENABLED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <windows.h>
#include <memory>
#include <optional>
#include "Image.hpp"

struct Option
{
	UINT32 block_size = 0;
	std::optional<bool> is_fixed;
	bool force_sparse = false;
};
std::unique_ptr<Image> DetectImageFormatByData(_In_ HANDLE file);
std::unique_ptr<Image> DetectImageFormatByExtension(_In_z_ PCWSTR file_name);
void ConvertImage(_In_z_ PCWSTR src_file_name, _In_z_ PCWSTR dst_file_name, _In_ const Option& options);