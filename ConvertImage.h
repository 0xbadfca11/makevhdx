#pragma once
#include <windows.h>
#include <optional>

struct Option
{
	UINT32 block_size = 0;
	std::optional<bool> fixed;
	std::optional<bool> sparse;
};
void ConvertImage(PCWSTR src_file_name, PCWSTR dst_file_name, const Option& options);