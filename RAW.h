#pragma once
#include "Image.h"

constexpr UINT32 RAW_SECTOR_SIZE = 512;
struct RAW : Image
{
private:
	LARGE_INTEGER raw_disk_size;
	UINT32 raw_block_size;
public:
	void ReadHeader()
	{
		THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(image_file, &raw_disk_size));
		if (std::cmp_less(raw_disk_size.QuadPart, MINIMUM_DISK_SIZE))
		{
			throw std::runtime_error("RAW disk size is less than 3MB.");
		}
		if (raw_disk_size.QuadPart % RAW_SECTOR_SIZE != 0)
		{
			throw std::runtime_error("RAW disk size is not multiple of sector.");
		}
		raw_block_size = std::max(1U << std::min(std::countr_zero<ULONGLONG>(raw_disk_size.QuadPart), 31), require_alignment);
	}
	void ConstructHeader(UINT64 disk_size, UINT32, UINT32 sector_size, bool)
	{
		if (std::cmp_less(disk_size, MINIMUM_DISK_SIZE))
		{
			throw std::invalid_argument("RAW disk size is less than 3MB.");
		}
		if (std::cmp_greater(disk_size, std::numeric_limits<decltype(FILE_END_OF_FILE_INFO::EndOfFile.QuadPart)>::max()))
		{
			throw std::invalid_argument("Exceed maximum file size.");
		}
		if (disk_size % RAW_SECTOR_SIZE != 0)
		{
			throw std::invalid_argument("RAW disk size is not multiple of sector.");
		}
		if (sector_size != RAW_SECTOR_SIZE)
		{
			throw std::invalid_argument("Unsuported RAW sector size.");
		}
		raw_disk_size.QuadPart = disk_size;
		raw_block_size = std::max(1U << std::min(std::countr_zero<ULONGLONG>(disk_size), 31), require_alignment);
		FILE_END_OF_FILE_INFO eof_info = { {.QuadPart = static_cast<LONGLONG>(disk_size) } };
		THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info));
	}
	void WriteHeader() const
	{
		THROW_IF_WIN32_BOOL_FALSE(FlushFileBuffers(image_file));
	}
	void CheckConvertible() const
	{
		THROW_WIN32_IF(ERROR_ARITHMETIC_OVERFLOW, GetDiskSize() / GetBlockSize() > UINT32_MAX);
	}
	bool IsFixed() const
	{
		return true;
	}
	PCSTR GetImageTypeName() const
	{
		return "RAW";
	}
	UINT64 GetDiskSize() const
	{
		return raw_disk_size.QuadPart;
	}
	UINT32 GetSectorSize() const
	{
		return RAW_SECTOR_SIZE;
	}
	UINT32 GetBlockSize() const
	{
		return raw_block_size;
	}
	UINT32 GetTableEntriesCount() const
	{
		return ceil_div(GetDiskSize(), GetBlockSize());
	}
	std::optional<UINT64> ProbeBlock(UINT32 index) const
	{
		_ASSERT(index < GetTableEntriesCount());
		return static_cast<UINT64>(GetBlockSize()) * index;
	}
	UINT64 AllocateBlock(UINT32 index)
	{
		return *ProbeBlock(index);
	}
	static std::unique_ptr<Image> DetectImageFormatByData(HANDLE file)
	{
		LARGE_INTEGER fsize;
		THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file, &fsize));
		if (fsize.QuadPart > 0 && fsize.QuadPart % RAW_SECTOR_SIZE == 0)
		{
			return std::unique_ptr<Image>(new RAW);
		}
		return nullptr;
	}
};