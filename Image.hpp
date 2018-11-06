#pragma once
#define WIN32_LEAN_AND_MEAN
#define STRICT_GS_ENABLED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <windows.h>
#include <algorithm>
#include <memory>
#include <optional>
#include "miscutil.hpp"

class Image
{
protected:
	const HANDLE image_file = INVALID_HANDLE_VALUE;
	const UINT32 require_alignment = UINT32_MAX;
	Image() = default;
	Image(_In_ HANDLE file, _In_ UINT32 cluster_size) : image_file(file), require_alignment(cluster_size)
	{
		if (!IsPow2(require_alignment))
		{
			die(L"Require alignment is not power of 2.");
		}
	}
public:
	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;
	virtual ~Image() = default;
	virtual void Attach(_In_ HANDLE file, _In_ UINT32 cluster_size)
	{
		if (!IsPow2(cluster_size))
		{
			die(L"Require alignment is not power of 2.");
		}
		const_cast<HANDLE&>(image_file) = file;
		const_cast<UINT32&>(require_alignment) = cluster_size;
	}
	virtual void ReadHeader() = 0;
	virtual void ConstructHeader(_In_ UINT64 disk_size, _In_ UINT32 block_size, _In_ UINT32 sector_size, _In_ bool is_fixed) = 0;
	virtual void WriteHeader() const = 0;
	virtual bool IsAligned() const = 0;
	virtual bool IsFixed() const = 0;
	virtual PCSTR GetImageTypeName() const = 0;
	virtual UINT64 GetDiskSize() const = 0;
	virtual UINT32 GetSectorSize() const = 0;
	virtual UINT32 GetBlockSize() const = 0;
	virtual UINT32 GetTableEntriesCount() const = 0;
	virtual std::optional<UINT64> ProbeBlock(_In_ UINT32 index) const = 0;
	virtual UINT64 AllocateBlockForWrite(_In_ UINT32 index) = 0;
};