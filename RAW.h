#pragma once
#include <algorithm>
#include <wil/result.h>
#include "Image.h"

constexpr UINT32 RAW_SECTOR_SIZE = 512;
struct RAW : Image
{
protected:
	LARGE_INTEGER raw_disk_size;
	UINT32 raw_block_size;
public:
	RAW() = default;
	void ReadHeader()
	{
		THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(image_file, &raw_disk_size));
		if (raw_disk_size.QuadPart < MINIMUM_DISK_SIZE)
		{
			throw std::runtime_error("RAW disk size is less than 3MB.");
		}
		if (raw_disk_size.LowPart % RAW_SECTOR_SIZE != 0)
		{
			throw std::runtime_error("RAW disk size is not multiple of sector.");
		}
		raw_block_size = (std::max)(1U << (std::min)(std::countr_zero<ULONGLONG>(raw_disk_size.QuadPart), 31), require_alignment);
	}
	void ConstructHeader(UINT64 disk_size, UINT32, UINT32 sector_size, bool)
	{
		if (disk_size < MINIMUM_DISK_SIZE)
		{
			throw std::invalid_argument("RAW disk size is less than 3MB.");
		}
		if (disk_size % RAW_SECTOR_SIZE != 0)
		{
			throw std::invalid_argument("RAW disk size is not multiple of sector.");
		}
		if (sector_size != RAW_SECTOR_SIZE)
		{
			throw std::invalid_argument("Unsuported RAW sector size.");
		}
		raw_block_size = (std::max)(1U << (std::min)(std::countr_zero<ULONGLONG>(raw_disk_size.QuadPart), 31), require_alignment);
		FILE_END_OF_FILE_INFO eof_info = { {.QuadPart = static_cast<LONGLONG>(disk_size) } };
		THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info));
	}
	void WriteHeader() const
	{
		THROW_IF_WIN32_BOOL_FALSE(FlushFileBuffers(image_file));
	}
	bool CheckConvertible(PCSTR*) const noexcept
	{
		return true;
	}
	bool IsFixed() const noexcept
	{
		return true;
	}
	PCSTR GetImageTypeName() const noexcept
	{
		return "RAW";
	}
	UINT64 GetDiskSize() const noexcept
	{
		return raw_disk_size.QuadPart;
	}
	UINT32 GetSectorSize() const noexcept
	{
		return RAW_SECTOR_SIZE;
	}
	UINT32 GetBlockSize() const noexcept
	{
		return raw_block_size;
	}
	UINT32 GetTableEntriesCount() const noexcept
	{
		return ceil_div(raw_disk_size.QuadPart, raw_block_size);
	}
	std::optional<UINT64> ProbeBlock(UINT32 index) const noexcept
	{
		_ASSERT(index < GetTableEntriesCount());
		return 1ULL * raw_block_size * index;
	}
	UINT64 AllocateBlock(UINT32 index) noexcept
	{
		return *ProbeBlock(index);
	}
	static std::unique_ptr<Image> DetectImageFormatByData(HANDLE file)
	{
		LARGE_INTEGER fsize;
		THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file, &fsize));
		if (fsize.QuadPart % RAW_SECTOR_SIZE == 0)
		{
			return std::unique_ptr<Image>(new RAW);
		}
		return nullptr;
	}
};