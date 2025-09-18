#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#include <wil/common.h>
#include <wil/filesystem.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <iterator>
#include <stdexcept>
#include "ConvertImage.h"
#include "Image.h"
#include "RAW.h"
#include "VHD.h"
#include "VHDX.h"
#pragma comment(lib, "shlwapi")

std::unique_ptr<Image> DetectImageFormatByData(HANDLE file)
{
	static const auto img_detect_funcs = {
		VHDX::DetectImageFormatByData,
		VHD::DetectImageFormatByData,
		RAW::DetectImageFormatByData,
	};
	for (auto img_detect_func : img_detect_funcs)
	{
		if (auto image = img_detect_func(file))
		{
			return image;
		}
	}
	return nullptr;
}
std::unique_ptr<Image> DetectImageFormatByExtension(PCWSTR file_name)
{
	const PCWSTR extension = PathFindExtensionW(file_name);
	if (_wcsicmp(extension, L".vhdx") == 0)
	{
		return std::unique_ptr<Image>(new VHDX);
	}
	if (_wcsicmp(extension, L".vhd") == 0)
	{
		return std::unique_ptr<Image>(new VHD);
	}
	return std::unique_ptr<Image>(new RAW);
}
void ConvertImage(PCWSTR src_file_name, PCWSTR dst_file_name, const Option& options)
{
	printf(
		"Source\n"
		"Path:              %ls\n",
		src_file_name
	);
	const auto src_file = wil::open_file(src_file_name);
	ULONG fs_flags;
	THROW_IF_WIN32_BOOL_FALSE(GetVolumeInformationByHandleW(src_file.get(), nullptr, 0, nullptr, nullptr, &fs_flags, nullptr, 0));
	if (WI_IsFlagClear(fs_flags, FILE_SUPPORTS_BLOCK_REFCOUNTING))
	{
		throw std::runtime_error("Filesystem doesn't support Block Cloning feature.");
	}
	BY_HANDLE_FILE_INFORMATION file_info;
	THROW_IF_WIN32_BOOL_FALSE(GetFileInformationByHandle(src_file.get(), &file_info));
	ULONG _;
	FSCTL_GET_INTEGRITY_INFORMATION_BUFFER get_integrity;
	THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(src_file.get(), FSCTL_GET_INTEGRITY_INFORMATION, nullptr, 0, &get_integrity, sizeof get_integrity, &_, nullptr));
	const auto src_img = DetectImageFormatByData(src_file.get());
	if (!src_img)
	{
		throw std::runtime_error("No supported image types detected.");
	}
	src_img->Attach(src_file.get(), get_integrity.ClusterSizeInBytes);
	src_img->ReadHeader();
	char buf[0x20];
	printf(
		"Image format:      %hs\n"
		"Allocation policy: %hs\n"
		"Disk size:         %llu (%s)\n"
		"Block size:        %u MB\n",
		src_img->GetImageTypeName(),
		src_img->IsFixed() ? "Fixed" : "Dynamic",
		src_img->GetDiskSize(),
		StrFormatByteSize64A(src_img->GetDiskSize(), buf, std::size(buf)),
		src_img->GetBlockSize() / 1024 / 1024
	);
	src_img->CheckConvertible();

	printf(
		"\n"
		"Destination\n"
		"Path:              %ls\n",
		dst_file_name
	);
#if _DEBUG
	const auto dst_file = wil::open_or_truncate_existing_file(dst_file_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, FILE_FLAG_DELETE_ON_CLOSE);
#elif NTDDI_VERSION < NTDDI_WIN10_RS3
	const auto dst_file = wil::create_new_file(dst_file_name, GENERIC_READ | GENERIC_WRITE | DELETE);
	FILE_DISPOSITION_INFO dispos = { TRUE };
	THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(dst_file.get(), FileDispositionInfo, &dispos, sizeof dispos));
#else
	const auto dst_file = wil::create_new_file(dst_file_name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, FILE_FLAG_DELETE_ON_CLOSE);
#endif
	FSCTL_SET_INTEGRITY_INFORMATION_BUFFER set_integrity = { get_integrity.ChecksumAlgorithm, 0, get_integrity.Flags };
	THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(dst_file.get(), FSCTL_SET_INTEGRITY_INFORMATION, &set_integrity, sizeof set_integrity, nullptr, 0, nullptr, nullptr));
	THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(dst_file.get(), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &_, nullptr));
	const auto dst_img = DetectImageFormatByExtension(dst_file_name);
	dst_img->Attach(dst_file.get(), get_integrity.ClusterSizeInBytes);
	dst_img->ConstructHeader(src_img->GetDiskSize(), options.block_size, src_img->GetSectorSize(), options.fixed.value_or(src_img->IsFixed()));
	printf(
		"Image format:      %hs\n"
		"Allocation policy: %hs\n"
		"Disk size:         %llu (%s)\n"
		"Block size:        %u MB\n",
		dst_img->GetImageTypeName(),
		dst_img->IsFixed() ? "Fixed" : "Dynamic",
		dst_img->GetDiskSize(),
		StrFormatByteSize64A(dst_img->GetDiskSize(), buf, std::size(buf)),
		dst_img->GetBlockSize() / 1024 / 1024
	);

	const UINT64 source_block_size = src_img->GetBlockSize();
	const UINT64 destination_block_size = dst_img->GetBlockSize();
	const UINT64 gcd_block_size = std::min(source_block_size, destination_block_size);
	DUPLICATE_EXTENTS_DATA dup_extent = { .FileHandle = src_file.get() };
	for (UINT32 source_block_index = 0; source_block_index < src_img->GetTableEntriesCount(); source_block_index++)
	{
		const auto source_block_address = src_img->ProbeBlock(source_block_index);
		if (!source_block_address)
		{
			continue;
		}
		const UINT64 source_virtual_address = source_block_size * source_block_index;
		for (UINT32 i = 0; i < source_block_size / gcd_block_size; i++)
		{
			const UINT64 source_block_offset = destination_block_size * i;
			if (source_virtual_address + source_block_offset >= src_img->GetDiskSize())
			{
				break;
			}
			dup_extent.SourceFileOffset.QuadPart = *source_block_address + source_block_offset;
			const UINT32 destination_block_index = static_cast<UINT32>((source_virtual_address + source_block_offset) / destination_block_size);
			const UINT32 destination_block_offset = static_cast<UINT32>(source_virtual_address % destination_block_size);
			dup_extent.TargetFileOffset.QuadPart = dst_img->AllocateBlock(destination_block_index) + destination_block_offset;
			dup_extent.ByteCount.QuadPart = gcd_block_size;
			THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(dst_file.get(), FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &_, nullptr));
		}
	}

	dst_img->WriteHeader();
	FILE_SET_SPARSE_BUFFER set_sparse = { options.sparse.value_or(WI_IsFlagSet(file_info.dwFileAttributes, FILE_ATTRIBUTE_SPARSE_FILE)) };
	THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(dst_file.get(), FSCTL_SET_SPARSE, &set_sparse, sizeof set_sparse, nullptr, 0, &_, nullptr));
#if !_DEBUG && NTDDI_VERSION < NTDDI_WIN10_RS3
	dispos = { FALSE };
	THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(dst_file.get(), FileDispositionInfo, &dispos, sizeof dispos));
#else
	FILE_DISPOSITION_INFO_EX fdie = { FILE_DISPOSITION_FLAG_DO_NOT_DELETE | FILE_DISPOSITION_FLAG_ON_CLOSE };
	THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(dst_file.get(), FileDispositionInfoEx, &fdie, sizeof fdie));
#endif
}