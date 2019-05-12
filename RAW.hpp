#pragma once
#include "Image.hpp"

const UINT32 RAW_SECTOR_SIZE = 512;
struct RAW : Image
{
protected:
	LARGE_INTEGER raw_disk_size;
	UINT32 raw_block_size;
public:
	RAW() = default;
	RAW(_In_ HANDLE file, _In_ UINT32 cluster_size) : Image(file, cluster_size)
	{}
	void ReadHeader()
	{
		ATLENSURE(GetFileSizeEx(image_file, &raw_disk_size));
		if (raw_disk_size.LowPart % RAW_SECTOR_SIZE != 0)
		{
			die(L"RAW disk size is not multiple of sector.");
		}
		ULONG bit_shift;
		BitScanForward64(&bit_shift, raw_disk_size.QuadPart);
		raw_block_size = (std::max)(1U << (std::min)(bit_shift, 31UL), require_alignment);
	}
	void ConstructHeader(_In_ UINT64 disk_size, _In_ UINT32, _In_ UINT32 sector_size, _In_ bool)
	{
		if (disk_size == 0 || disk_size % RAW_SECTOR_SIZE != 0)
		{
			die(L"RAW disk size is not multiple of sector.");
		}
		if (sector_size != RAW_SECTOR_SIZE)
		{
			die(L"Unsuported RAW sector size.");
		}
		ULONG bit_shift;
		BitScanForward64(&bit_shift, disk_size);
		raw_block_size = (std::max)(1U << (std::min)(bit_shift, 31UL), require_alignment);
		FILE_END_OF_FILE_INFO eof_info;
		raw_disk_size.QuadPart = eof_info.EndOfFile.QuadPart = disk_size;
		if (!SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info))
		{
			die();
		}
	}
	void WriteHeader() const
	{
		if (!FlushFileBuffers(image_file))
		{
			die();
		}
	}
	virtual bool CheckConvertible(_When_(return == false, _Outptr_result_z_) PCWSTR*) const noexcept
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
	UINT32 GetBlockSize() const noexcept
	{
		return raw_block_size;
	}
	UINT32 GetTableEntriesCount() const noexcept
	{
		return static_cast<UINT32>(CEILING(raw_disk_size.QuadPart, raw_block_size));
	}
	std::optional<UINT64> ProbeBlock(_In_ UINT32 index) const noexcept
	{
		_ASSERT(index < GetTableEntriesCount());
		return raw_block_size * index;
	}
	UINT64 AllocateBlockForWrite(_In_ UINT32 index) noexcept
	{
		return *ProbeBlock(index);
	}
};