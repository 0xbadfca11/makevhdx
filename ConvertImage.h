#pragma once
struct Option
{
  UINT32 block_size = 0;
  std::optional<bool> is_fixed;
  bool force_sparse = false;
};

template <typename COPY_FROM, typename COPY_TO>
void ConvertImage(PCWSTR src_file_name, PCWSTR dst_file_name, const Option& options)
{
  wprintf(
    L"Source\n"
    L"Path:              %ls\n",
    src_file_name
  );
  ATL::CHandle src_file(CreateFileW(src_file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
  if (src_file == INVALID_HANDLE_VALUE)
  {
    _CrtDbgBreak();
    src_file.Detach();
    die();
  }
  ULONG fs_flags;
  ATLENSURE(GetVolumeInformationByHandleW(src_file, nullptr, 0, nullptr, nullptr, &fs_flags, nullptr, 0));
  if (!(fs_flags & FILE_SUPPORTS_BLOCK_REFCOUNTING))
  {
    die(L"Filesystem doesn't support Block Cloning feature.");
  }
  BY_HANDLE_FILE_INFORMATION file_info;
  ATLENSURE(GetFileInformationByHandle(src_file, &file_info));
  ULONG dummy;
  FSCTL_GET_INTEGRITY_INFORMATION_BUFFER get_integrity;
  if (!DeviceIoControl(src_file, FSCTL_GET_INTEGRITY_INFORMATION, nullptr, 0, &get_integrity, sizeof get_integrity, &dummy, nullptr))
  {
    die();
  }
  COPY_FROM src_img(src_file, get_integrity.ClusterSizeInBytes);
  src_img.ReadHeader();
  if (!src_img.IsAligned())
  {
    die(L"Source image isn't aligned.");
  }
  wprintf(
    L"Image format:      %hs\n"
    L"Allocation policy: %hs\n"
    L"Disk size:         %llu(%.3fGB)\n"
    L"Block size:        %.1fMB\n",
    src_img.GetImageTypeName(),
    src_img.IsFixed() ? "Preallocate" : "Dynamic",
    src_img.GetDiskSize(),
    src_img.GetDiskSize() / (1024.f * 1024.f * 1024.f),
    src_img.GetBlockSize() / (1024.f * 1024.f)
  );

  wprintf(
    L"\n"
    L"Destination\n"
    L"Path:              %ls\n",
    dst_file_name
  );
#ifdef _DEBUG
  ATL::CHandle dst_file(CreateFileW(dst_file_name, GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
#else
  ATL::CHandle dst_file(CreateFileW(dst_file_name, GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
#endif
  if (dst_file == INVALID_HANDLE_VALUE)
  {
    _CrtDbgBreak();
    dst_file.Detach();
    die();
  }
  FILE_DISPOSITION_INFO dispos = { TRUE };
  ATLENSURE(SetFileInformationByHandle(dst_file, FileDispositionInfo, &dispos, sizeof dispos));
  FSCTL_SET_INTEGRITY_INFORMATION_BUFFER set_integrity = { get_integrity.ChecksumAlgorithm, 0, get_integrity.Flags };
  if (!DeviceIoControl(dst_file, FSCTL_SET_INTEGRITY_INFORMATION, &set_integrity, sizeof set_integrity, nullptr, 0, nullptr, nullptr))
  {
    die();
  }
  if (options.force_sparse || file_info.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE)
  {
    if (!DeviceIoControl(dst_file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &dummy, nullptr))
    {
      die();
    }
  }
  COPY_TO dst_img(dst_file, get_integrity.ClusterSizeInBytes);
  dst_img.ConstructHeader(src_img.GetDiskSize(), options.block_size, src_img.GetSectorSize(), options.is_fixed.value_or(src_img.IsFixed()));
  wprintf(
    L"Image format:      %hs\n"
    L"Allocation policy: %hs\n"
    L"Disk size:         %llu(%.3fGB)\n"
    L"Block size:        %.1fMB\n",
    dst_img.GetImageTypeName(),
    dst_img.IsFixed() ? "Preallocate" : "Dynamic",
    dst_img.GetDiskSize(),
    dst_img.GetDiskSize() / (1024.f * 1024.f * 1024.f),
    dst_img.GetBlockSize() / (1024.f * 1024.f)
  );

  const UINT32 source_block_size = src_img.GetBlockSize();
  const UINT32 destination_block_size = dst_img.GetBlockSize();
  DUPLICATE_EXTENTS_DATA dup_extent = { src_file };
  if (source_block_size <= destination_block_size)
  {
    for (UINT32 read_block_number = 0; read_block_number < src_img.GetTableEntriesCount(); read_block_number++)
    {
      if (const std::optional<UINT64> read_physical_address = src_img[read_block_number])
      {
        const UINT64 read_virtual_address = 1ULL * source_block_size * read_block_number;
        const UINT32 write_virtual_block_number = static_cast<UINT32>(read_virtual_address / destination_block_size);
        const UINT32 write_virtual_block_offset = static_cast<UINT32>(read_virtual_address % destination_block_size);
        dup_extent.SourceFileOffset.QuadPart = *read_physical_address;
        dup_extent.TargetFileOffset.QuadPart = dst_img.AllocateBlockForWrite(write_virtual_block_number) + write_virtual_block_offset;
        dup_extent.ByteCount.QuadPart = source_block_size;
        _ASSERTE(dup_extent.SourceFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
        _ASSERTE(dup_extent.TargetFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
        _ASSERTE(dup_extent.ByteCount.QuadPart % get_integrity.ClusterSizeInBytes == 0);
        if (!DeviceIoControl(dst_file, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &dummy, nullptr))
        {
          _CrtDbgBreak();
          die();
        }
      }
    }
  }
  else
  {
    for (UINT32 read_block_number = 0; read_block_number < src_img.GetTableEntriesCount(); read_block_number++)
    {
      if (const std::optional<UINT64> read_physical_address = src_img[read_block_number])
      {
        for (UINT32 i = 0; i < source_block_size / destination_block_size; i++)
        {
          const UINT64 read_virtual_address = 1ULL * source_block_size * read_block_number;
          const UINT32 read_block_offset = destination_block_size * i;
          const UINT32 write_virtual_block_number = static_cast<UINT32>((read_virtual_address + read_block_offset) / destination_block_size);
          dup_extent.SourceFileOffset.QuadPart = *read_physical_address + read_block_offset;
          dup_extent.TargetFileOffset.QuadPart = dst_img.AllocateBlockForWrite(write_virtual_block_number);
          dup_extent.ByteCount.QuadPart = destination_block_size;
          _ASSERTE(dup_extent.SourceFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
          _ASSERTE(dup_extent.TargetFileOffset.QuadPart % get_integrity.ClusterSizeInBytes == 0);
          _ASSERTE(dup_extent.ByteCount.QuadPart % get_integrity.ClusterSizeInBytes == 0);
          if (!DeviceIoControl(dst_file, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &dummy, nullptr))
          {
            _CrtDbgBreak();
            die();
          }
        }
      }
    }
  }

  _ASSERT(dst_img.IsAligned());
  dst_img.WriteHeader();
  dispos = { FALSE };
  ATLENSURE(SetFileInformationByHandle(dst_file, FileDispositionInfo, &dispos, sizeof dispos));
}