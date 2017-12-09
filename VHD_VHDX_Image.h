#pragma once

#include "VHD_VHDX_Base.h"

struct VHD_Image : public VHD
{
private:

  const HANDLE image;
public:
  VHD_Image(HANDLE image, UINT32 require_alignment) : VHD(require_alignment),
                                                image(image)
  {
    if (!is_power_of_2(require_alignment))
    {
      die(L"Require alignment isn't power of 2.");
    }
  }
  void ReadHeader()
  {
    LARGE_INTEGER fsize;
    ATLENSURE(GetFileSizeEx(image, &fsize));
    ReadFileWithOffset(image, &vhd_footer, fsize.QuadPart - sizeof vhd_footer);
    if (vhd_footer.Cookie != VHD_COOKIE)
    {
      die(L"Missing VHD signature.");
    }
    if (!VHDChecksumValidate(vhd_footer))
    {
      die(L"VHD footer checksum mismatch.");
    }
    if ((vhd_footer.Features & ~VHD_VALID_FEATURE_MASK) != 0)
    {
      die(L"Unknown VHD feature flag.");
    }
    if (vhd_footer.FileFormatVersion != VHD_VERSION)
    {
      die(L"Unknown VHD version.");
    }
    vhd_disk_size = _byteswap_uint64(vhd_footer.CurrentSize);
    if (vhd_disk_size == 0)
    {
      die(L"Zero length VHD.");
    }
    if (vhd_disk_size % VHD_SECTOR_SIZE != 0)
    {
      die(L"VHD disk size is not multiple of sector.");
    }
    if (vhd_footer.DiskType == VHDType::Difference)
    {
      die(L"Differencing VHD is not supported.");
    }
    else if (vhd_footer.DiskType == VHDType::Fixed)
    {
      ULONG bit_shift;
      BitScanForward64(&bit_shift, vhd_disk_size);
      vhd_block_size = 1U << (std::min)(bit_shift, 31UL);
      if (vhd_block_size < require_alignment)
      {
        die(L"VHD isn't aligned.");
      }
      vhd_table_entries_count = static_cast<UINT32>(vhd_disk_size / vhd_block_size);
      return;
    }
    else if (vhd_footer.DiskType != VHDType::Dynamic)
    {
      die(L"Unknown VHD type.");
    }
    ReadFileWithOffset(image, &vhd_dyn_header, _byteswap_uint64(vhd_footer.DataOffset));
    if (vhd_dyn_header.Cookie != VHD_DYNAMIC_COOKIE)
    {
      die(L"Missing Dynamic VHD signature.");
    }
    if (!VHDChecksumValidate(vhd_dyn_header))
    {
      die(L"VHD dynamic header checksum mismatch.");
    }
    if (vhd_dyn_header.DataOffset != VHD_INVALID_OFFSET)
    {
      die(L"Unknown extra data.");
    }
    if (vhd_dyn_header.HeaderVersion != VHD_DYNAMIC_VERSION)
    {
      die(L"Unknown Dynamic VHD version.");
    }
    vhd_block_size = _byteswap_ulong(vhd_dyn_header.BlockSize);
    if (vhd_block_size < require_alignment)
    {
      die(L"VHD isn't aligned.");
    }
    if (!is_power_of_2(vhd_block_size))
    {
      die(L"VHD is corrupted.");
    }
    vhd_bitmap_size = (std::max)(vhd_block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_SECTOR_SIZE);
    vhd_table_entries_count = _byteswap_ulong(vhd_dyn_header.MaxTableEntries);
    vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(vhd_table_entries_count);
    ReadFileWithOffset(image, vhd_block_allocation_table.get(), vhd_table_entries_count * sizeof(VHD_BAT_ENTRY), _byteswap_uint64(vhd_dyn_header.TableOffset));
  }

  void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed)
  {
    FILE_END_OF_FILE_INFO eof_info;
    VHD::ConstructHeader(disk_size, block_size, sector_size, is_fixed, eof_info);
    if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
    {
      die();
    }
  }

  void WriteHeader() const
  {
    if (vhd_footer.DiskType == VHDType::Fixed)
    {
      WriteFileWithOffset(image, vhd_footer, vhd_disk_size);
      return;
    }
    else if (vhd_footer.DiskType == VHDType::Dynamic)
    {
      WriteFileWithOffset(image, vhd_footer, 0);
      WriteFileWithOffset(image, vhd_dyn_header, sizeof vhd_footer);
      WriteFileWithOffset(image, vhd_block_allocation_table.get(), vhd_table_write_size, sizeof vhd_footer + sizeof vhd_dyn_header);
      WriteFileWithOffset(image, vhd_footer, vhd_next_free_address);

      _ASSERT(IsAligned());
      auto vhd_bitmap_buffer = std::make_unique<BYTE[]>(vhd_bitmap_aligned_size);
      memset(vhd_bitmap_buffer.get() + vhd_bitmap_padding_size, 0xFF, vhd_bitmap_size);
      for (UINT32 i = 0; i < vhd_table_entries_count; i++)
      {
        if (vhd_block_allocation_table[i] != VHD_UNUSED_BAT_ENTRY)
        {
          WriteFileWithOffset(image, vhd_bitmap_buffer.get(), vhd_bitmap_aligned_size, 1ULL * vhd_block_allocation_table[i] * VHD_SECTOR_SIZE - vhd_bitmap_padding_size);
        }
      }
      return;
    }
    else
    {
      die(L"BUG");
    }
  }
  UINT64 AllocateBlockForWrite(UINT32 index)
  {
    if (const auto offset = (*this)[index])
    {
      return *offset;
    }
    else
    {
      if (IsFixed())
      {
        _CrtDbgBreak();
        die(L"BUG");
      }
      FILE_END_OF_FILE_INFO eof_info;
      eof_info.EndOfFile.QuadPart = vhd_next_free_address + vhd_bitmap_aligned_size + vhd_block_size;
      _ASSERT(eof_info.EndOfFile.QuadPart >= sizeof vhd_footer + sizeof vhd_dyn_header);
      if (eof_info.EndOfFile.QuadPart > 1ULL * UINT32_MAX * VHD_SECTOR_SIZE)
      {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        die();
      }
      if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
      {
        die();
      }
      vhd_block_allocation_table[index] = static_cast<UINT32>((vhd_next_free_address + vhd_bitmap_padding_size) / VHD_SECTOR_SIZE);
      vhd_next_free_address += vhd_bitmap_aligned_size + vhd_block_size;
      return vhd_next_free_address - vhd_block_size;
    }
  }
};

struct VHDX_Image : public VHDX
{
private:
  const HANDLE image;
public:
  VHDX_Image(HANDLE image, UINT32 require_alignment) : 
    VHDX(require_alignment),
    image(image)
  {
    if (!is_power_of_2(require_alignment))
    {
      die(L"Require alignment isn't power of 2.");
    }
  }
  void ReadHeader()
  {
    ReadFileWithOffset(image, &vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_OFFSET);
    if (vhdx_file_indentifier.Signature != VHDX_SIGNATURE)
    {
      die(L"Missing VHDX signature.");
    }
    VHDX_HEADER vhdx_headers[2];
    ReadFileWithOffset(image, &vhdx_headers[0], VHDX_HEADER1_OFFSET);
    ReadFileWithOffset(image, &vhdx_headers[1], VHDX_HEADER2_OFFSET);
    const bool header1_available = vhdx_headers[0].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_headers[0]);
    const bool header2_available = vhdx_headers[1].Signature == VHDX_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_headers[1]);
    if (!header1_available && !header2_available)
    {
      die(L"VHDX header corrupted.");
    }
    else if (!header1_available && header2_available)
    {
      vhdx_header = vhdx_headers[1];
    }
    else if (header1_available && !header2_available)
    {
      vhdx_header = vhdx_headers[0];
    }
    else if (vhdx_headers[0].SequenceNumber > vhdx_headers[1].SequenceNumber)
    {
      vhdx_header = vhdx_headers[0];
    }
    else if (vhdx_headers[0].SequenceNumber < vhdx_headers[1].SequenceNumber)
    {
      vhdx_header = vhdx_headers[1];
    }
    else if (memcmp(&vhdx_headers[0], &vhdx_headers[1], sizeof(VHDX_HEADER)) == 0)
    {
      vhdx_header = vhdx_headers[0];
    }
    else
    {
      die(L"VHDX header corrupted.");
    }
    if (vhdx_header.Version != VHDX_CURRENT_VERSION)
    {
      die(L"Unknown VHDX header version.");
    }
    if (vhdx_header.LogGuid != GUID_NULL)
    {
      die(L"VHDX journal log needs recovery.");
    }
    VHDX_REGION_TABLE_HEADER vhdx_region_table_headers[2];
    ReadFileWithOffset(image, &vhdx_region_table_headers[0], VHDX_REGION_TABLE_HEADER1_OFFSET);
    ReadFileWithOffset(image, &vhdx_region_table_headers[1], VHDX_REGION_TABLE_HEADER2_OFFSET);
    const bool region_header1_available = vhdx_region_table_headers[0].Signature == VHDX_REGION_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_region_table_headers[0]);
    const bool region_header2_available = vhdx_region_table_headers[1].Signature == VHDX_REGION_HEADER_SIGNATURE && VHDXChecksumValidate(vhdx_region_table_headers[1]);
    if (region_header1_available)
    {
      vhdx_region_table_header = vhdx_region_table_headers[0];
    }
    else if (region_header2_available)
    {
      vhdx_region_table_header = vhdx_region_table_headers[1];
    }
    else
    {
      die(L"VHDX region header corrupted.");
    }
    if (vhdx_region_table_header.EntryCount > VHDX_MAX_ENTRIES)
    {
      die(L"VHDX region header corrupted.");
    }
    for (UINT32 i = 0; i < vhdx_region_table_header.EntryCount; i++)
    {
      const UINT64 FileOffset = vhdx_region_table_header.RegionTableEntries[i].FileOffset;
      const UINT32 Length = vhdx_region_table_header.RegionTableEntries[i].Length;
      if (vhdx_region_table_header.RegionTableEntries[i].Guid == BAT)
      {
        vhdx_block_allocation_table = std::make_unique<VHDX_BAT_ENTRY[]>(Length / sizeof(VHDX_BAT_ENTRY));
        ReadFileWithOffset(image, vhdx_block_allocation_table.get(), Length, FileOffset);
      }
      else if (vhdx_region_table_header.RegionTableEntries[i].Guid == Metadata)
      {
        ReadFileWithOffset(image, &vhdx_metadata_table_header, FileOffset);
        if (vhdx_metadata_table_header.Signature != VHDX_METADATA_HEADER_SIGNATURE || vhdx_metadata_table_header.EntryCount > VHDX_MAX_ENTRIES)
        {
          die(L"VHDX region header corrupted.");
        }
        for (UINT32 j = 0; j < vhdx_metadata_table_header.EntryCount; j++)
        {
          const UINT32 Offset = vhdx_metadata_table_header.MetadataTableEntries[j].Offset;
          if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == FileParameters)
          {
            ReadFileWithOffset(image, &vhdx_metadata_packed.VhdxFileParameters, FileOffset + Offset);
          }
          else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == VirtualDiskSize)
          {
            ReadFileWithOffset(image, &vhdx_metadata_packed.VirtualDiskSize, FileOffset + Offset);
          }
          else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == LogicalSectorSize)
          {
            ReadFileWithOffset(image, &vhdx_metadata_packed.LogicalSectorSize, FileOffset + Offset);
          }
          else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == ParentLocator)
          {
            die(L"Differencing VHDX is not supported.");
          }
          else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == PhysicalSectorSize)
          {
            __noop;
          }
          else if (vhdx_metadata_table_header.MetadataTableEntries[j].ItemId == Page83Data)
          {
            __noop;
          }
          else if (vhdx_metadata_table_header.MetadataTableEntries[j].IsRequired)
          {
            die(L"Unknown require VHDX metadata found.");
          }
        }
        vhdx_data_blocks_count = static_cast<UINT32>(CEILING(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize));
        vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
      }
      else if (vhdx_region_table_header.RegionTableEntries[i].Required)
      {
        die(L"Unknown require VHDX region found.");
      }
    }
  }

    void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed)
    {
      FILE_END_OF_FILE_INFO eof_info;
      VHDX::ConstructHeader(disk_size, block_size, sector_size, is_fixed, eof_info);
      if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
      {
        die();
      }
    }
    void WriteHeader() const
    {
      VHDX::WriteHeader(image);
    }
    UINT64 AllocateBlockForWrite(UINT32 index)
    {
      if (const auto offset = (*this)[index])
      {
        return *offset;
      }
      else
      {
        if (IsFixed())
        {
          _CrtDbgBreak();
          die(L"BUG");
        }
        index += index / vhdx_chuck_ratio;
        FILE_END_OF_FILE_INFO eof_info;
        eof_info.EndOfFile.QuadPart = vhdx_next_free_address + vhdx_metadata_packed.VhdxFileParameters.BlockSize;
        _ASSERT(eof_info.EndOfFile.QuadPart % VHDX_MINIMUM_ALIGNMENT == 0);
        _ASSERT(eof_info.EndOfFile.QuadPart >= 4 * 1024 * 1024);
        if (!SetFileInformationByHandle(image, FileEndOfFileInfo, &eof_info, sizeof eof_info))
        {
          die();
        }
        vhdx_block_allocation_table[index].FileOffsetMB = vhdx_next_free_address / VHDX_BLOCK_1MB;
        vhdx_block_allocation_table[index].State = PAYLOAD_BLOCK_FULLY_PRESENT;
        vhdx_next_free_address += vhdx_metadata_packed.VhdxFileParameters.BlockSize;
        return vhdx_next_free_address - vhdx_metadata_packed.VhdxFileParameters.BlockSize;
      }
    }

  };
