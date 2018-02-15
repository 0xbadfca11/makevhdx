#pragma once
#define WIN32_LEAN_AND_MEAN
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlchecked.h>
#include <windows.h>
#include <initguid.h>
#include <pathcch.h>
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <type_traits>
#include <fcntl.h>
#include <io.h>
#include <vector>
#include "crc32c.h"


struct VHD_VHDX_IF
{
public:
  virtual ~VHD_VHDX_IF() {}
  virtual void WriteHeaderToBuffer(std::vector<BYTE> &buffer) const = 0;
  virtual void WriteFooterToBuffer(std::vector<BYTE> &buffer) const = 0;
  virtual void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed, FILE_END_OF_FILE_INFO &eof_info) = 0;
};

#pragma region misc
constexpr UINT32 byteswap32(UINT32 v) noexcept
{
  return v >> 24 & 0xFF | v >> 8 & 0xFF << 8 | v << 8 & 0xFF << 16 | v << 24 & 0xFF << 24;
}
static_assert(0x11335577 == byteswap32(0x77553311),"");
template <typename Ty1, typename Ty2>
constexpr Ty1 ROUNDUP(Ty1 number, Ty2 num_digits) noexcept
{
  return (number + num_digits - 1) / num_digits * num_digits;
}
static_assert(ROUNDUP(42, 15) == 45,"");
template <typename Ty1, typename Ty2>
constexpr Ty1 CEILING(Ty1 number, Ty2 significance) noexcept
{
  return (number + significance - 1) / significance;
}
static_assert(CEILING(42, 15) == 3);
constexpr bool is_power_of_2(UINT64 value) noexcept
{
  return value && (value & (value - 1)) == 0;
}
static_assert(is_power_of_2(64),"");
static_assert(!is_power_of_2(65), "");
static_assert(!is_power_of_2(0), "");
[[noreturn]]
void die(PCWSTR err_msg = nullptr)
{
  if (!err_msg)
  {
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), 0, reinterpret_cast<PWSTR>(&err_msg), 0, nullptr);
  }
  _CrtDbgBreak();
  fputws(err_msg, stderr);
  fputws(L"\n", stderr);
  ExitProcess(EXIT_FAILURE);
}
template <typename Ty>
Ty* ReadFileWithOffset(
  _In_ HANDLE hFile,
  _Out_writes_bytes_(nNumberOfBytesToRead) __out_data_source(FILE) Ty* lpBuffer,
  _In_ ULONG nNumberOfBytesToRead,
  _In_ ULONGLONG Offset
)
{
  static_assert(!std::is_pointer_v<Ty>);
  ULONG read;
  OVERLAPPED o = {};
  o.Offset = static_cast<ULONG>(Offset);
  o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
  if (!ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &read, &o) || read != nNumberOfBytesToRead)
  {
    _CrtDbgBreak();
    die();
  }
  return lpBuffer;
}
template <typename Ty>
Ty* ReadFileWithOffset(
  _In_ HANDLE hFile,
  _Out_writes_bytes_(sizeof(Ty)) __out_data_source(FILE) Ty* lpBuffer,
  _In_ ULONGLONG Offset
)
{
  return ReadFileWithOffset(hFile, lpBuffer, sizeof(Ty), Offset);
}
VOID WriteFileWithOffset(
  _In_ HANDLE hFile,
  _In_reads_bytes_(nNumberOfBytesToWrite) LPCVOID lpBuffer,
  _In_ ULONG nNumberOfBytesToWrite,
  _In_ ULONGLONG Offset
)
{
  _ASSERT(Offset % 512 == 0);
  ULONG write;
  OVERLAPPED o = {};
  o.Offset = static_cast<ULONG>(Offset);
  o.OffsetHigh = static_cast<ULONG>(Offset >> 32);
  if (!WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &write, &o) || write != nNumberOfBytesToWrite)
  {
    _CrtDbgBreak();
    die();
  }
}
ULONGLONG WriteBufferWithOffset(
  std::vector<BYTE> &outBuffer,
  _In_reads_bytes_(nNumberOfBytesToWrite) LPCVOID lpBuffer,
  _In_ ULONG nNumberOfBytesToWrite,
  _In_ ULONGLONG Offset
)
{
  _ASSERT(Offset % 512 == 0);
  outBuffer.reserve(Offset + nNumberOfBytesToWrite);
  outBuffer.resize(Offset + nNumberOfBytesToWrite);
  memcpy(&outBuffer.data()[Offset], lpBuffer, nNumberOfBytesToWrite);
  return Offset + nNumberOfBytesToWrite;
}

template <typename Ty>
VOID WriteFileWithOffset(
  _In_ HANDLE hFile,
  _In_ const Ty& lpBuffer,
  _In_ ULONGLONG Offset
)
{
  static_assert(!std::is_pointer_v<Ty>);
  WriteFileWithOffset(hFile, &lpBuffer, sizeof(Ty), Offset);
}

template <typename Ty>
ULONGLONG WriteBufferWithOffset(
  std::vector<BYTE> &outBuffer,
  _In_ const Ty& lpBuffer,
  _In_ ULONGLONG Offset
)
{
  static_assert(!std::is_pointer_v<Ty>);
  return WriteBufferWithOffset(outBuffer, &lpBuffer, sizeof(Ty), Offset);
}

#pragma endregion
#pragma region VHD
const UINT64 VHD_COOKIE = 0x78697463656e6f63;
const UINT32 VHD_VALID_FEATURE_MASK = byteswap32(3);
const UINT32 VHD_VERSION = byteswap32(MAKELONG(0, 1));
const UINT64 VHD_INVALID_OFFSET = UINT64_MAX;
const UINT64 VHD_DYNAMIC_COOKIE = 0x6573726170737863;
const UINT32 VHD_DYNAMIC_VERSION = byteswap32(MAKELONG(0, 1));
const UINT32 VHD_SECTOR_SIZE = 512;
const UINT64 VHD_MAX_DISK_SIZE = 2040ULL * 1024 * 1024 * 1024;
const UINT32 VHD_DEFAULT_BLOCK_SIZE = 2 * 1024 * 1024;
const UINT32 VHD_UNUSED_BAT_ENTRY = UINT32_MAX;
class VHD_BAT_ENTRY
{
private:
  UINT32 value = VHD_UNUSED_BAT_ENTRY;
public:
  operator UINT32() const
  {
    return _byteswap_ulong(value);
  }
  UINT32 operator=(UINT32 rvalue)
  {
    value = _byteswap_ulong(rvalue);
    return rvalue;
  }
};
static_assert(sizeof(VHD_BAT_ENTRY) == sizeof(UINT32));
enum VHDType : UINT32
{
  Fixed = byteswap32(2),
  Dynamic = byteswap32(3),
  Difference = byteswap32(4),
};
struct VHD_FOOTER
{
  UINT64 Cookie;
  UINT32 Features;
  UINT32 FileFormatVersion;
  UINT64 DataOffset;
  UINT32 TimeStamp;
  UINT32 CreatorApplication;
  UINT32 CreatorVersion;
  UINT32 CreatorHostOS;
  UINT64 OriginalSize;
  UINT64 CurrentSize;
  UINT32 DiskGeometry;
  UINT32 DiskType;
  UINT32 Checksum;
  GUID   UniqueId;
  UINT8  SavedState;
  UINT8  Reserved[427];
};
static_assert(sizeof(VHD_FOOTER) == 512);
struct VHD_DYNAMIC_HEADER
{
  UINT64 Cookie;
  UINT64 DataOffset;
  UINT64 TableOffset;
  UINT32 HeaderVersion;
  UINT32 MaxTableEntries;
  UINT32 BlockSize;
  UINT32 Checksum;
  UINT8  ParentUniqueId[16];
  UINT32 ParentTimeStamp;
  UINT32 Reserved1;
  UINT16 ParentUnicodeName[256];
  UINT8  ParentLocatorEntry[24][8];
  UINT8  Reserved2[256];
};
static_assert(sizeof(VHD_DYNAMIC_HEADER) == 1024);
template <typename Ty>
UINT32 VHDChecksumUpdate(Ty* header)
{
  header->Checksum = 0;
  PBYTE driveFooter = static_cast<PBYTE>(static_cast<void*>(header));
  UINT32 checksum = 0;
  for (UINT32 counter = 0; counter < sizeof(Ty); counter++)
  {
    checksum += driveFooter[counter];
  }
  return header->Checksum = _byteswap_ulong(~checksum);
}
template <typename Ty>
bool VHDChecksumValidate(Ty header)
{
  const UINT32 checksum = header.Checksum;
  return VHDChecksumUpdate(&header) == checksum;
}
UINT32 CHSCalculate(UINT64 disk_size)
{
  UINT64 totalSectors = disk_size / VHD_SECTOR_SIZE;
  UINT32 cylinderTimesHeads;
  UINT16 cylinders;
  UINT8  heads;
  UINT8  sectorsPerTrack;
  if (totalSectors > 65535 * 16 * 255)
  {
    totalSectors = 65535 * 16 * 255;
  }
  if (totalSectors >= 65535 * 16 * 63)
  {
    sectorsPerTrack = 255;
    heads = 16;
    cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
  }
  else
  {
    sectorsPerTrack = 17;
    cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
    heads = static_cast<UINT8>((cylinderTimesHeads + 1023) / 1024);
    if (heads < 4)
    {
      heads = 4;
    }
    if (cylinderTimesHeads >= (heads * 1024U) || heads > 16)
    {
      sectorsPerTrack = 31;
      heads = 16;
      cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
    }
    if (cylinderTimesHeads >= (heads * 1024U))
    {
      sectorsPerTrack = 63;
      heads = 16;
      cylinderTimesHeads = static_cast<UINT32>(totalSectors / sectorsPerTrack);
    }
  }
  cylinders = static_cast<UINT16>(cylinderTimesHeads / heads);
  return _byteswap_ulong(cylinders << 16 | heads << 8 | sectorsPerTrack);
}
#pragma endregion
#pragma region VHDX
const UINT64 VHDX_SIGNATURE = 0x656C696678646876;
const UINT32 VHDX_MAX_ENTRIES = 2047;
const UINT32 VHDX_HEADER_SIGNATURE = 0x64616568;
const UINT16 VHDX_CURRENT_VERSION = 1;
const UINT64 VHDX_LOG_LOCATION = 1024 * 1024;
const UINT32 VHDX_LOG_LENGTH = 1024 * 1024;
const UINT32 VHDX_REGION_HEADER_SIGNATURE = 0x69676572;
const UINT64 VHDX_METADATA_HEADER_SIGNATURE = 0x617461646174656D;
const UINT64 VHDX_METADATA_LOCATION = VHDX_LOG_LOCATION + VHDX_LOG_LENGTH;
const UINT32 VHDX_METADATA_LENGTH = 1024 * 1024;
const UINT32 VHDX_METADATA_START_OFFSET = 64 * 1024;
const UINT64 VHDX_BAT_LOCATION = VHDX_METADATA_LOCATION + VHDX_METADATA_LENGTH;
const UINT32 VHDX_FILE_IDENTIFIER_OFFSET = 0;
const UINT32 VHDX_HEADER1_OFFSET = 1 * 64 * 1024;
const UINT32 VHDX_HEADER2_OFFSET = 2 * 64 * 1024;
const UINT32 VHDX_REGION_TABLE_HEADER1_OFFSET = 3 * 64 * 1024;
const UINT32 VHDX_REGION_TABLE_HEADER2_OFFSET = 4 * 64 * 1024;
const UINT64 VHDX_PHYSICAL_SECTOR_SIZE = 4096;
const UINT64 VHDX_MAX_DISK_SIZE = 64ULL * 1024 * 1024 * 1024 * 1024;
const UINT32 VHDX_MIN_BLOCK_SIZE = 1 * 1024 * 1024;
const UINT32 VHDX_MAX_BLOCK_SIZE = 256 * 1024 * 1024;
const UINT32 VHDX_DEFAULT_BLOCK_SIZE = 32 * 1024 * 1024;
const UINT32 VHDX_MINIMUM_ALIGNMENT = 1024 * 1024;
const UINT32 VHDX_BLOCK_1MB = 1024 * 1024;
DEFINE_GUID(BAT, 0x2dc27766, 0xf623, 0x4200, 0x9d, 0x64, 0x11, 0x5e, 0x9b, 0xfd, 0x4a, 0x08);
DEFINE_GUID(Metadata, 0x8B7CA206, 0x4790, 0x4B9A, 0xB8, 0xFE, 0x57, 0x5F, 0x05, 0x0F, 0x88, 0x6E);
DEFINE_GUID(FileParameters, 0xCAA16737, 0xFA36, 0x4D43, 0xB3, 0xB6, 0x33, 0xF0, 0xAA, 0x44, 0xE7, 0x6B);
DEFINE_GUID(VirtualDiskSize, 0x2FA54224, 0xCD1B, 0x4876, 0xB2, 0x11, 0x5D, 0xBE, 0xD8, 0x3B, 0xF4, 0xB8);
DEFINE_GUID(Page83Data, 0xBECA12AB, 0xB2E6, 0x4523, 0x93, 0xEF, 0xC3, 0x09, 0xE0, 0x00, 0xC7, 0x46);
DEFINE_GUID(LogicalSectorSize, 0x8141BF1D, 0xA96F, 0x4709, 0xBA, 0x47, 0xF2, 0x33, 0xA8, 0xFA, 0xAB, 0x5F);
DEFINE_GUID(PhysicalSectorSize, 0xCDA348C7, 0x445D, 0x4471, 0x9C, 0xC9, 0xE9, 0x88, 0x52, 0x51, 0xC5, 0x56);
DEFINE_GUID(ParentLocator, 0xA8D35F2D, 0xB30B, 0x454D, 0xAB, 0xF7, 0xD3, 0xD8, 0x48, 0x34, 0xAB, 0x0C);
struct VHDX_FILE_IDENTIFIER
{
  UINT64 Signature;
  UINT16 Creator[256];
  UINT8  Padding[4096 - 8 - 512];
};
static_assert(sizeof(VHDX_FILE_IDENTIFIER) == 4096);
struct VHDX_HEADER
{
  UINT32 Signature;
  UINT32 Checksum;
  UINT64 SequenceNumber;
  GUID   FileWriteGuid;
  GUID   DataWriteGuid;
  GUID   LogGuid;
  UINT16 LogVersion;
  UINT16 Version;
  UINT32 LogLength;
  UINT64 LogOffset;
  UINT8  Reserved[4016];
};
static_assert(sizeof(VHDX_HEADER) == 4096);
struct VHDX_REGION_TABLE_ENTRY
{
  GUID   Guid;
  UINT64 FileOffset;
  UINT32 Length;
  UINT32 Required : 1;
  UINT32 Reserved : 31;
};
struct VHDX_REGION_TABLE_HEADER
{
  UINT32 Signature;
  UINT32 Checksum;
  UINT32 EntryCount;
  UINT32 Reserved;
  _Field_size_full_(EntryCount) VHDX_REGION_TABLE_ENTRY RegionTableEntries[VHDX_MAX_ENTRIES];
  BYTE   Padding[16];
};
static_assert(sizeof(VHDX_REGION_TABLE_HEADER) == 64 * 1024);
struct VHDX_METADATA_TABLE_ENTRY
{
  GUID   ItemId;
  UINT32 Offset;
  UINT32 Length;
  UINT32 IsUser : 1;
  UINT32 IsVirtualDisk : 1;
  UINT32 IsRequired : 1;
  UINT32 Reserved : 29;
  UINT32 Reserved2;
};
struct VHDX_METADATA_TABLE_HEADER
{
  UINT64 Signature;
  UINT16 Reserved;
  UINT16 EntryCount;
  UINT32 Reserved2[5];
  _Field_size_full_(EntryCount) VHDX_METADATA_TABLE_ENTRY MetadataTableEntries[VHDX_MAX_ENTRIES];
};
static_assert(sizeof(VHDX_METADATA_TABLE_HEADER) == 64 * 1024);
struct VHDX_FILE_PARAMETERS
{
  UINT32 BlockSize;
  UINT32 LeaveBlocksAllocated : 1;
  UINT32 HasParent : 1;
  UINT32 Reserved : 30;
};
enum : UINT32
{
  PAYLOAD_BLOCK_NOT_PRESENT = 0,
  PAYLOAD_BLOCK_UNDEFINED = 1,
  PAYLOAD_BLOCK_ZERO = 2,
  PAYLOAD_BLOCK_UNMAPPED = 3,
  PAYLOAD_BLOCK_FULLY_PRESENT = 6,
  PAYLOAD_BLOCK_PARTIALLY_PRESENT = 7,
};
struct VHDX_BAT_ENTRY
{
  UINT64 State : 3;
  UINT64 Reserved : 17;
  UINT64 FileOffsetMB : 44;
};
struct VHDX_METADATA_PACKED
{
  VHDX_FILE_PARAMETERS VhdxFileParameters;
  UINT64 VirtualDiskSize;
  UINT32 LogicalSectorSize;
  UINT32 PhysicalSectorSize;
  GUID   Page83Data;
  UINT8  Padding[4096 - 40];
};
static_assert(sizeof(VHDX_METADATA_PACKED) == 4096);
template <typename Ty>
bool VHDXChecksumValidate(Ty header)
{
  const UINT32 Checksum = header.Checksum;
  header.Checksum = 0;
  return crc32c_append(0, static_cast<uint8_t*>(static_cast<void*>(&header)), sizeof(Ty)) == Checksum;
}
template <typename Ty>
void VHDXChecksumUpdate(Ty* header)
{
  header->Checksum = 0;
  header->Checksum = crc32c_append(0, static_cast<uint8_t*>(static_cast<void*>(header)), sizeof(Ty));
  _ASSERT(VHDXChecksumValidate(*header));
}
#pragma endregion
struct VHD : public VHD_VHDX_IF
{
protected:
  const UINT32 require_alignment;
  VHD_FOOTER vhd_footer;
  VHD_DYNAMIC_HEADER vhd_dyn_header;
  std::unique_ptr<VHD_BAT_ENTRY[]> vhd_block_allocation_table;
  UINT64 vhd_next_free_address;
  UINT64 vhd_disk_size;
  UINT32 vhd_block_size;
  UINT32 vhd_bitmap_size;
  UINT32 vhd_bitmap_aligned_size;
  UINT32 vhd_bitmap_padding_size;
  UINT32 vhd_table_entries_count;
  UINT32 vhd_table_write_size;
public:
  VHD(UINT32 require_alignment) : require_alignment(require_alignment)
  {
    if (!is_power_of_2(require_alignment))
    {
      die(L"Require alignment isn't power of 2.");
    }
  }
  void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed, FILE_END_OF_FILE_INFO &eof_info)
  {
    if (block_size == 0)
    {
      block_size = VHD_DEFAULT_BLOCK_SIZE;
    }
    else if (!is_power_of_2(block_size))
    {
      die(L"Unsuported VHD block size.");
    }
    if (sector_size != VHD_SECTOR_SIZE)
    {
      die(L"Unsuported VHD sector size.");
    }
    if (is_fixed)
    {
      vhd_disk_size = disk_size;
      ULONG bit_shift;
      BitScanForward64(&bit_shift, disk_size);
      vhd_block_size = 1U << (std::min)(bit_shift, 31UL);
      if (vhd_block_size < require_alignment)
      {
        die(L"VHD isn't aligned.");
      }
      vhd_table_entries_count = static_cast<UINT32>(disk_size / vhd_block_size);
      vhd_footer =
      {
        VHD_COOKIE,
        byteswap32(2),
        VHD_VERSION,
        VHD_INVALID_OFFSET,
        0,
        0,
        0,
        0,
        0,
        _byteswap_uint64(disk_size),
        CHSCalculate(disk_size),
        VHDType::Fixed,
      };
      ATLENSURE_SUCCEEDED(CoCreateGuid(&vhd_footer.UniqueId));
      VHDChecksumUpdate(&vhd_footer);
      eof_info.EndOfFile.QuadPart = disk_size + sizeof vhd_footer;
      return;
    }
    else
    {
      if (disk_size > VHD_MAX_DISK_SIZE)
      {
        die(L"Exceeded max VHD disk size.");
      }
      vhd_disk_size = disk_size;
      vhd_block_size = block_size;
      vhd_bitmap_size = std::max<UINT32>(block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_SECTOR_SIZE);
      vhd_bitmap_aligned_size = ROUNDUP(vhd_bitmap_size, require_alignment);
      vhd_bitmap_padding_size = vhd_bitmap_aligned_size - vhd_bitmap_size;
      vhd_table_entries_count = static_cast<UINT32>(CEILING(disk_size, block_size));
      const UINT32 vhd_table_write_address = sizeof vhd_footer + sizeof vhd_dyn_header;
      vhd_table_write_size = ROUNDUP(vhd_table_entries_count * static_cast<UINT32>(sizeof(VHD_BAT_ENTRY)), require_alignment);
      vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(vhd_table_write_size / sizeof(VHD_BAT_ENTRY));
      vhd_next_free_address = ROUNDUP(sizeof vhd_footer + sizeof vhd_dyn_header + vhd_table_write_size, require_alignment);
      vhd_footer =
      {
        VHD_COOKIE,
        byteswap32(2),
        VHD_VERSION,
        _byteswap_uint64(sizeof vhd_footer),
        0,
        0,
        0,
        0,
        0,
        _byteswap_uint64(disk_size),
        CHSCalculate(disk_size),
        VHDType::Dynamic,
      };
      ATLENSURE_SUCCEEDED(CoCreateGuid(&vhd_footer.UniqueId));
      VHDChecksumUpdate(&vhd_footer);
      vhd_dyn_header =
      {
        VHD_DYNAMIC_COOKIE,
        VHD_INVALID_OFFSET,
        _byteswap_uint64(vhd_table_write_address),
        VHD_DYNAMIC_VERSION,
        _byteswap_ulong(vhd_table_entries_count),
        _byteswap_ulong(block_size),
      };
      VHDChecksumUpdate(&vhd_dyn_header);
    }
  }
  bool IsAligned() const
  {
    if (vhd_footer.DiskType == VHDType::Fixed)
    {
      return true;
    }
    for (UINT32 i = 0; i < vhd_table_entries_count; i++)
    {
      if (vhd_block_allocation_table[i] != VHD_UNUSED_BAT_ENTRY)
      {
        if ((vhd_block_allocation_table[i] * VHD_SECTOR_SIZE + vhd_bitmap_size) % require_alignment != 0)
        {
          _CrtDbgBreak();
          return false;
        }
      }
    }
    return true;
  }
  bool IsFixed() const
  {
    return vhd_footer.DiskType == VHDType::Fixed;
  }
  PCSTR GetImageTypeName() const
  {
    return "VHD";
  }
  UINT64 GetDiskSize() const
  {
    return vhd_disk_size;
  }
  UINT32 GetSectorSize() const
  {
    return VHD_SECTOR_SIZE;
  }
  UINT32 GetBlockSize() const
  {
    return vhd_block_size;
  }
  UINT32 GetTableEntriesCount() const
  {
    return vhd_table_entries_count;
  }
  std::optional<UINT64> operator[](UINT32 index) const
  {
    if (index > vhd_table_entries_count)
    {
      _CrtDbgBreak();
      die(L"BUG");
    }
    if (vhd_footer.DiskType == VHDType::Dynamic)
    {
      if (UINT64 block_address = vhd_block_allocation_table[index]; block_address != VHD_UNUSED_BAT_ENTRY)
      {
        return block_address * VHD_SECTOR_SIZE + vhd_bitmap_size;
      }
      else
      {
        return std::nullopt;
      }
    }
    else if (vhd_footer.DiskType == VHDType::Fixed)
    {
      return 1ULL * index * vhd_block_size;
    }
    else
    {
      SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
      die();
    }
  }
  
  void WriteHeaderToBuffer(std::vector<BYTE> &buffer) const
  {
    buffer.clear();
    if (vhd_footer.DiskType == VHDType::Fixed)
    {
      return;
    }
    else if (vhd_footer.DiskType == VHDType::Dynamic)
    {
      WriteBufferWithOffset(buffer, vhd_footer, 0);
      WriteBufferWithOffset(buffer, vhd_dyn_header, sizeof vhd_footer);
      WriteBufferWithOffset(buffer, vhd_block_allocation_table.get(), vhd_table_write_size, sizeof vhd_footer + sizeof vhd_dyn_header);
      WriteBufferWithOffset(buffer, vhd_footer, vhd_next_free_address);

      _ASSERT(IsAligned());
      auto vhd_bitmap_buffer = std::make_unique<BYTE[]>(vhd_bitmap_aligned_size);
      memset(vhd_bitmap_buffer.get() + vhd_bitmap_padding_size, 0xFF, vhd_bitmap_size);
      for (UINT32 i = 0; i < vhd_table_entries_count; i++)
      {
        if (vhd_block_allocation_table[i] != VHD_UNUSED_BAT_ENTRY)
        {
          WriteBufferWithOffset(buffer, vhd_bitmap_buffer.get(), vhd_bitmap_aligned_size, 1ULL * vhd_block_allocation_table[i] * VHD_SECTOR_SIZE - vhd_bitmap_padding_size);
        }
      }
    }
    else
    {
      die(L"BUG");
    }
  }

  void WriteHeaderToFile(HANDLE image) const
  {
    std::vector<BYTE> buffer;
    WriteHeaderToBuffer(buffer);
    WriteFileWithOffset(image, buffer.data(), (ULONG) buffer.size(), 0);
  }

  void WriteFooterToBuffer(std::vector<BYTE> &buffer) const
  {
    buffer.clear();
    buffer.reserve(sizeof(VHD_FOOTER));
    buffer.resize(sizeof(VHD_FOOTER));
    memcpy(buffer.data(), &vhd_footer, sizeof(VHD_FOOTER));
  }
};
struct VHDX : public VHD_VHDX_IF
{

protected:
  const UINT32 require_alignment;
  VHDX_FILE_IDENTIFIER vhdx_file_indentifier;
  VHDX_HEADER vhdx_header;
  VHDX_REGION_TABLE_HEADER vhdx_region_table_header;
  VHDX_METADATA_TABLE_HEADER vhdx_metadata_table_header;
  VHDX_METADATA_PACKED vhdx_metadata_packed;
  std::unique_ptr<VHDX_BAT_ENTRY[]> vhdx_block_allocation_table;
  UINT64 vhdx_next_free_address;
  UINT32 vhdx_chuck_ratio;
  UINT32 vhdx_data_blocks_count;
  UINT32 vhdx_table_write_size;
public:
  VHDX(UINT32 require_alignment= VHDX_MINIMUM_ALIGNMENT) : require_alignment(ROUNDUP(require_alignment, VHDX_MINIMUM_ALIGNMENT))
  {
    if (!is_power_of_2(require_alignment))
    {
      die(L"Require alignment isn't power of 2.");
    }
  }
  void ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool is_fixed, FILE_END_OF_FILE_INFO &eof_info)
  {
    ATLENSURE(require_alignment >= VHDX_MINIMUM_ALIGNMENT);
    if (disk_size > VHDX_MAX_DISK_SIZE)
    {
      die(L"Exceeded max VHDX disk size.");
    }
    if (block_size == 0)
    {
      block_size = VHDX_DEFAULT_BLOCK_SIZE;
    }
    else if (block_size < VHDX_MIN_BLOCK_SIZE || block_size > VHDX_MAX_BLOCK_SIZE || !is_power_of_2(block_size))
    {
      die(L"Unsuported VHDX block size.");
    }
    if (sector_size != 512 && sector_size != 4096)
    {
      die(L"Unsuported VHDX sector size.");
    }
    vhdx_file_indentifier = { VHDX_SIGNATURE };
    vhdx_header = { VHDX_HEADER_SIGNATURE, 0, 0,{},{},{}, 0, VHDX_CURRENT_VERSION, VHDX_LOG_LENGTH, VHDX_LOG_LOCATION };
    VHDXChecksumUpdate(&vhdx_header);
    vhdx_metadata_table_header =
    {
      VHDX_METADATA_HEADER_SIGNATURE,
      0,
      5,
    {},
      {
        {
          FileParameters,
          offsetof(VHDX_METADATA_PACKED, VhdxFileParameters) + VHDX_METADATA_START_OFFSET,
          sizeof(VHDX_METADATA_PACKED::VhdxFileParameters),
          0, 0, 1
        },
        {
          VirtualDiskSize,
          offsetof(VHDX_METADATA_PACKED, VirtualDiskSize) + VHDX_METADATA_START_OFFSET,
          sizeof(VHDX_METADATA_PACKED::VirtualDiskSize),
          0, 1, 1
        },
        {
          LogicalSectorSize,
          offsetof(VHDX_METADATA_PACKED, LogicalSectorSize) + VHDX_METADATA_START_OFFSET,
          sizeof(VHDX_METADATA_PACKED::LogicalSectorSize),
          0, 1, 1
        },
        {
          PhysicalSectorSize,
          offsetof(VHDX_METADATA_PACKED, PhysicalSectorSize) + VHDX_METADATA_START_OFFSET,
          sizeof(VHDX_METADATA_PACKED::PhysicalSectorSize),
          0, 1, 1
        },
        {
          Page83Data,
          offsetof(VHDX_METADATA_PACKED, Page83Data) + VHDX_METADATA_START_OFFSET,
          sizeof(VHDX_METADATA_PACKED::Page83Data),
          0, 1, 1
        }
      }
    };
    vhdx_metadata_packed =
    {
      { block_size, is_fixed },
      disk_size,
      sector_size,
      VHDX_PHYSICAL_SECTOR_SIZE,
    };
    ATLENSURE_SUCCEEDED(CoCreateGuid(&vhdx_metadata_packed.Page83Data));
    vhdx_chuck_ratio = static_cast<UINT32>((1ULL << 23) * vhdx_metadata_packed.LogicalSectorSize / vhdx_metadata_packed.VhdxFileParameters.BlockSize);
    vhdx_data_blocks_count = static_cast<UINT32>(CEILING(vhdx_metadata_packed.VirtualDiskSize, vhdx_metadata_packed.VhdxFileParameters.BlockSize));
    const UINT32 vhdx_table_entries_count = vhdx_data_blocks_count + (vhdx_data_blocks_count - 1) / vhdx_chuck_ratio;
    vhdx_table_write_size = ROUNDUP(vhdx_table_entries_count * static_cast<UINT32>(sizeof(VHDX_BAT_ENTRY)), require_alignment);
    vhdx_block_allocation_table = std::make_unique<VHDX_BAT_ENTRY[]>(vhdx_table_write_size / sizeof(VHDX_BAT_ENTRY));
    vhdx_region_table_header = { VHDX_REGION_HEADER_SIGNATURE };
    VHDX_REGION_TABLE_ENTRY vhdx_region_table_entry[] =
    {
      { Metadata, VHDX_METADATA_LOCATION, ROUNDUP(VHDX_METADATA_LENGTH, require_alignment), 1 },
    { BAT, VHDX_BAT_LOCATION, ROUNDUP(vhdx_table_write_size, require_alignment), 1 }
    };
    vhdx_region_table_header.EntryCount = ARRAYSIZE(vhdx_region_table_entry);
    for (int i = 0; i < ARRAYSIZE(vhdx_region_table_entry); i++)
    {
      vhdx_region_table_header.RegionTableEntries[i] = vhdx_region_table_entry[i];
    }
    VHDXChecksumUpdate(&vhdx_region_table_header);
    vhdx_next_free_address = VHDX_BAT_LOCATION + ROUNDUP(vhdx_table_write_size, require_alignment);
    if (is_fixed)
    {
      for (UINT32 i = 0; i < vhdx_data_blocks_count; i++)
      {
        vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].State = PAYLOAD_BLOCK_FULLY_PRESENT;
        vhdx_block_allocation_table[i + i / vhdx_chuck_ratio].FileOffsetMB = vhdx_next_free_address / VHDX_BLOCK_1MB;
        vhdx_next_free_address += block_size;
      }
      eof_info.EndOfFile.QuadPart = vhdx_next_free_address;
    }
  }
  void WriteHeaderToBuffer(std::vector<BYTE> &buffer) const
  {
    buffer.clear();
    WriteBufferWithOffset(buffer, vhdx_file_indentifier, VHDX_FILE_IDENTIFIER_OFFSET);
    WriteBufferWithOffset(buffer, vhdx_header, VHDX_HEADER1_OFFSET);
    WriteBufferWithOffset(buffer, vhdx_header, VHDX_HEADER2_OFFSET);
    WriteBufferWithOffset(buffer, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER1_OFFSET);
    WriteBufferWithOffset(buffer, vhdx_region_table_header, VHDX_REGION_TABLE_HEADER2_OFFSET);
    WriteBufferWithOffset(buffer, vhdx_metadata_table_header, VHDX_METADATA_LOCATION);
    WriteBufferWithOffset(buffer, vhdx_metadata_packed, VHDX_METADATA_LOCATION + VHDX_METADATA_START_OFFSET);
    WriteBufferWithOffset(buffer, vhdx_block_allocation_table.get(), vhdx_table_write_size, VHDX_BAT_LOCATION);
  }

  void WriteHeaderToFile(HANDLE image) const
  {
    std::vector<BYTE> buffer;
    WriteHeaderToBuffer(buffer);
    WriteFileWithOffset(image, buffer.data(), (ULONG) buffer.size(), 0);
  }
  void WriteFooterToBuffer(std::vector<BYTE> &buffer) const
  {
    buffer.clear();
  }


  bool IsAligned() const
  {
    if (require_alignment <= VHDX_MINIMUM_ALIGNMENT)
    {
      return true;
    }
    else
    {
      SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
      die();
    }
  }
  bool IsFixed() const
  {
    return !!vhdx_metadata_packed.VhdxFileParameters.LeaveBlocksAllocated;
  }
  PCSTR GetImageTypeName() const
  {
    return "VHDX";
  }
  UINT64 GetDiskSize() const
  {
    return vhdx_metadata_packed.VirtualDiskSize;
  }
  UINT32 GetSectorSize() const
  {
    return vhdx_metadata_packed.LogicalSectorSize;
  }
  UINT32 GetBlockSize() const
  {
    return vhdx_metadata_packed.VhdxFileParameters.BlockSize;
  }
  UINT32 GetTableEntriesCount() const
  {
    return vhdx_data_blocks_count;
  }
  std::optional<UINT64> operator[](UINT32 index) const
  {
    if (index > vhdx_data_blocks_count)
    {
      _CrtDbgBreak();
      die(L"BUG");
    }
    index += index / vhdx_chuck_ratio;
    if (vhdx_block_allocation_table[index].State == PAYLOAD_BLOCK_FULLY_PRESENT)
    {
      return vhdx_block_allocation_table[index].FileOffsetMB * VHDX_BLOCK_1MB;
    }
    else
    {
      return std::nullopt;
    }
  }

};


