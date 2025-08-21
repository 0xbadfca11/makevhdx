#pragma once
#include <windows.h>
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <wil/result.h>

constexpr UINT32 MINIMUM_DISK_SIZE = 3 * 1024 * 1024;
struct Image
{
protected:
	HANDLE image_file;
	UINT32 require_alignment;
	Image() = default;
public:
	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;
	virtual ~Image() = default;
	virtual void Attach(HANDLE file, UINT32 cluster_size)
	{
		if (!std::has_single_bit(cluster_size))
		{
			throw std::runtime_error("Require alignment is not power of 2.");
		}
		image_file = file;
		require_alignment = cluster_size;
	}
	virtual void ReadHeader() = 0;
	virtual void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool fixed) = 0;
	virtual void WriteHeader() const = 0;
	virtual bool CheckConvertible(PCSTR* reason) const = 0;
	virtual bool IsFixed() const noexcept = 0;
	virtual PCSTR GetImageTypeName() const noexcept = 0;
	virtual UINT64 GetDiskSize() const noexcept = 0;
	virtual UINT32 GetSectorSize() const noexcept = 0;
	virtual UINT32 GetBlockSize() const noexcept = 0;
	virtual UINT32 GetTableEntriesCount() const noexcept = 0;
	virtual std::optional<UINT64> ProbeBlock(UINT32 index) const noexcept = 0;
	virtual UINT64 AllocateBlock(UINT32 index) = 0;
	static std::unique_ptr<Image> DetectImageFormatByData(HANDLE file) = delete;
};

constexpr auto inline round_up(auto value, auto base_multiple) noexcept
{
	return (value + base_multiple - 1) / base_multiple * base_multiple;
}
static_assert(round_up(1024 * 8 + 1, 1024) == 1024 * 9);

constexpr auto inline ceil_div(auto value, auto divisor) noexcept
{
	return static_cast<UINT32>((value + divisor - 1) / divisor);
}
static_assert(ceil_div(1024 * 8 + 1, 1024) == 9);

void inline ReadFileWithOffset(HANDLE hFile, PVOID lpBuffer, ULONG nNumberOfBytesToRead, ULONGLONG Offset)
{
	OVERLAPPED o = {
		.Offset = static_cast<ULONG>(Offset & ~0U),
		.OffsetHigh = static_cast<ULONG>(Offset >> 32),
	};
	THROW_IF_WIN32_BOOL_FALSE(ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, nullptr, &o));
}
template <typename Ty>
void inline ReadFileWithOffset(HANDLE hFile, Ty* lpBuffer, ULONGLONG Offset)
{
	static_assert(!std::is_pointer_v<Ty>);
	return ReadFileWithOffset(hFile, lpBuffer, sizeof(Ty), Offset);
}

void inline WriteFileWithOffset(HANDLE hFile, LPCVOID lpBuffer, ULONG nNumberOfBytesToWrite, ULONGLONG Offset)
{
	OVERLAPPED o = {
		.Offset = static_cast<ULONG>(Offset & ~0U),
		.OffsetHigh = static_cast<ULONG>(Offset >> 32),
	};
	THROW_IF_WIN32_BOOL_FALSE(WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, nullptr, &o));
}
template <typename Ty>
void inline WriteFileWithOffset(HANDLE hFile, const Ty& lpBuffer, ULONGLONG Offset)
{
	static_assert(!std::is_pointer_v<Ty>);
	WriteFileWithOffset(hFile, &lpBuffer, sizeof(Ty), Offset);
}