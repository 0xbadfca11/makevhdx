#include "VHD.h"

void VHD::ReadHeader()
{
	LARGE_INTEGER fsize;
	THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(image_file, &fsize));
	THROW_WIN32_IF(ERROR_VHD_INVALID_FILE_SIZE, std::cmp_less(fsize.QuadPart, VHD_BLOCK_ALLOC_TABLE_LOCATION + VHD_SECTOR_SIZE + sizeof(VHD_FOOTER)));
	vhd_footer.Reserved[std::size(vhd_footer.Reserved) - 1] = 0;
	ReadFileWithOffset(image_file, &vhd_footer, sizeof vhd_footer - 1, round_up(fsize.QuadPart - VHD_FOOTER_OFFSET, VHD_FOOTER_ALIGN));
	if (vhd_footer.Cookie != VHD_COOKIE || !VHDChecksumValidate(&vhd_footer))
	{
		ReadFileWithOffset(image_file, &vhd_footer, VHD_HEADER_LOCATION);
		THROW_WIN32_IF(ERROR_VHD_DRIVE_FOOTER_MISSING, vhd_footer.Cookie != VHD_COOKIE);
		THROW_WIN32_IF(ERROR_VHD_DRIVE_FOOTER_CHECKSUM_MISMATCH, !VHDChecksumValidate(&vhd_footer));
	}
	THROW_WIN32_IF(ERROR_VHD_DRIVE_FOOTER_CORRUPT, WI_IsAnyFlagSet(vhd_footer.Features, ~VHD_VALID_FEATURE_MASK));
	THROW_WIN32_IF(ERROR_VHD_FORMAT_UNSUPPORTED_VERSION, vhd_footer.FileFormatVersion != VHD_VERSION);
	vhd_disk_size = std::byteswap(vhd_footer.CurrentSize);
	THROW_WIN32_IF(ERROR_VHD_INVALID_SIZE, vhd_disk_size == 0 || vhd_disk_size % VHD_SECTOR_SIZE != 0);
	if (vhd_footer.DiskType == VHDType::Fixed)
	{
		THROW_WIN32_IF(ERROR_VHD_INVALID_FILE_SIZE, std::cmp_less(round_up(vhd_disk_size - VHD_FOOTER_OFFSET, VHD_FOOTER_ALIGN), fsize.QuadPart));
		vhd_block_size = std::max(1U << std::min(std::countr_zero(vhd_disk_size), 31), require_alignment);
		vhd_table_entries_count = ceil_div(vhd_disk_size, vhd_block_size);
		return;
	}
	THROW_WIN32_IF(ERROR_VHD_INVALID_SIZE, vhd_disk_size > VHD_MAX_DYNAMIC_DISK_SIZE);
	THROW_WIN32_IF(ERROR_VHD_FORMAT_UNKNOWN, vhd_footer.DiskType != VHDType::Difference && vhd_footer.DiskType != VHDType::Dynamic);
	THROW_WIN32_IF(ERROR_VHD_INVALID_FILE_SIZE, std::cmp_less(fsize.QuadPart, std::byteswap(vhd_footer.DataOffset)));
	ReadFileWithOffset(image_file, &vhd_dyn_header, sizeof vhd_dyn_header, std::byteswap(vhd_footer.DataOffset));
	THROW_WIN32_IF(ERROR_VHD_SPARSE_HEADER_CORRUPT, vhd_dyn_header.Cookie != VHD_DYNAMIC_COOKIE);
	THROW_WIN32_IF(ERROR_VHD_SPARSE_HEADER_CHECKSUM_MISMATCH, !VHDChecksumValidate(&vhd_dyn_header));
	THROW_WIN32_IF(ERROR_VHD_SPARSE_HEADER_CORRUPT, vhd_dyn_header.DataOffset != VHD_INVALID_OFFSET);
	THROW_WIN32_IF(ERROR_VHD_SPARSE_HEADER_UNSUPPORTED_VERSION, vhd_dyn_header.HeaderVersion != VHD_DYNAMIC_VERSION);
	vhd_block_size = std::byteswap(vhd_dyn_header.BlockSize);
	THROW_WIN32_IF(ERROR_VHD_INVALID_BLOCK_SIZE, vhd_block_size < VHD_SECTOR_SIZE || !std::has_single_bit(vhd_block_size));
	vhd_bitmap_actual_size = round_up(vhd_block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_SECTOR_SIZE);
	vhd_table_entries_count = std::byteswap(vhd_dyn_header.MaxTableEntries);
	vhd_block_allocation_table = std::make_unique_for_overwrite<VHD_BAT_ENTRY[]>(vhd_table_entries_count);
	ReadFileWithOffset(image_file, vhd_block_allocation_table.get(), vhd_table_entries_count * sizeof(VHD_BAT_ENTRY), std::byteswap(vhd_dyn_header.TableOffset));
}
void VHD::ConstructHeader(UINT64 disk_size, UINT32 block_size, UINT32 sector_size, bool fixed)
{
	if (disk_size < MINIMUM_DISK_SIZE)
	{
		throw std::invalid_argument("VHD disk size is less than 3MB.");
	}
	if (sector_size != VHD_SECTOR_SIZE)
	{
		throw std::invalid_argument("Unsuported VHD sector size.");
	}
	if (disk_size % VHD_SECTOR_SIZE != 0)
	{
		throw std::invalid_argument("VHD disk size is not multiple of sector.");
	}
	if (block_size == 0)
	{
		block_size = VHD_DEFAULT_BLOCK_SIZE;
	}
	THROW_WIN32_IF(ERROR_VHD_INVALID_BLOCK_SIZE, vhd_block_size < VHD_SECTOR_SIZE || !std::has_single_bit(block_size));
	if (fixed)
	{
		if (std::cmp_greater(disk_size, std::numeric_limits<decltype(FILE_END_OF_FILE_INFO::EndOfFile.QuadPart)>::max() - sizeof vhd_footer))
		{
			throw std::invalid_argument("Exceed maximum file size.");
		}
		vhd_disk_size = disk_size;
		vhd_block_size = std::max(1U << std::min(std::countr_zero(disk_size), 31), require_alignment);
		THROW_WIN32_IF(ERROR_ARITHMETIC_OVERFLOW, GetDiskSize() / GetBlockSize() > UINT32_MAX);
		vhd_table_entries_count = ceil_div(vhd_disk_size, vhd_block_size);
		memset(&vhd_footer, 0, sizeof vhd_footer);
		vhd_footer.Cookie = VHD_COOKIE;
		vhd_footer.Features = VHD_FEATURE_RESERVED_MUST_ALWAYS_ON;
		vhd_footer.FileFormatVersion = VHD_VERSION;
		vhd_footer.DataOffset = VHD_INVALID_OFFSET;
		// [^1]: because older QEMU uses whitelist method.
		// https://gitlab.com/qemu-project/qemu/-/commit/558d8eb7f32dda8634e0d3e82ac36e00bf61c9ee
		vhd_footer.CreatorApplication = std::byteswap('qem2');
		vhd_footer.CurrentSize = std::byteswap(disk_size);
		vhd_footer.DiskGeometry = CHSCalculate(disk_size);
		vhd_footer.DiskType = VHDType::Fixed;
		THROW_IF_FAILED(CoCreateGuid(&vhd_footer.UniqueId));
		VHDChecksumUpdate(&vhd_footer);
		FILE_END_OF_FILE_INFO eof_info = { {.QuadPart = static_cast<LONGLONG>(disk_size + sizeof vhd_footer) } };
		THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info));
		return;
	}
	if (disk_size > VHD_MAX_DYNAMIC_DISK_SIZE)
	{
		throw std::invalid_argument("Exceeded maximum dynamic VHD disk size.");
	}
	vhd_disk_size = disk_size;
	vhd_block_size = block_size;
	vhd_bitmap_actual_size = round_up(block_size / (VHD_SECTOR_SIZE * CHAR_BIT), VHD_SECTOR_SIZE);
	vhd_bitmap_aligned_size = round_up(vhd_bitmap_actual_size, require_alignment);
	vhd_bitmap_padding_size = vhd_bitmap_aligned_size - vhd_bitmap_actual_size;
	vhd_table_entries_count = ceil_div(disk_size, block_size);
	vhd_table_sector_aligned_count = round_up(vhd_table_entries_count, VHD_SECTOR_ALIGNED_BYTES);
	vhd_block_allocation_table = std::make_unique<VHD_BAT_ENTRY[]>(vhd_table_sector_aligned_count);
	vhd_next_free_address = round_up(VHD_BLOCK_ALLOC_TABLE_LOCATION + vhd_table_sector_aligned_count * sizeof(VHD_BAT_ENTRY), require_alignment);
	vhd_template_bitmap_address = 0;
	memset(&vhd_footer, 0, sizeof vhd_footer);
	vhd_footer.Cookie = VHD_COOKIE;
	vhd_footer.Features = VHD_FEATURE_RESERVED_MUST_ALWAYS_ON;
	vhd_footer.FileFormatVersion = VHD_VERSION;
	vhd_footer.DataOffset = std::byteswap(VHD_DYNAMIC_HEADER_LOCATION);
	// [^1]:
	vhd_footer.CreatorApplication = std::byteswap('qem2');
	vhd_footer.CurrentSize = std::byteswap(disk_size);
	vhd_footer.DiskGeometry = CHSCalculate(disk_size);
	vhd_footer.DiskType = VHDType::Dynamic;
	THROW_IF_FAILED(CoCreateGuid(&vhd_footer.UniqueId));
	VHDChecksumUpdate(&vhd_footer);
	memset(&vhd_dyn_header, 0, sizeof vhd_dyn_header);
	vhd_dyn_header.Cookie = VHD_DYNAMIC_COOKIE;
	vhd_dyn_header.DataOffset = VHD_INVALID_OFFSET;
	vhd_dyn_header.TableOffset = std::byteswap(VHD_BLOCK_ALLOC_TABLE_LOCATION);
	vhd_dyn_header.HeaderVersion = VHD_DYNAMIC_VERSION;
	vhd_dyn_header.MaxTableEntries = std::byteswap(vhd_table_entries_count);
	vhd_dyn_header.BlockSize = std::byteswap(block_size);
	VHDChecksumUpdate(&vhd_dyn_header);
}
void VHD::WriteHeader() const
{
	if (vhd_footer.DiskType == VHDType::Fixed)
	{
		WriteFileWithOffset(image_file, vhd_footer, vhd_disk_size);
		THROW_IF_WIN32_BOOL_FALSE(FlushFileBuffers(image_file));
		return;
	}
	if (vhd_footer.DiskType == VHDType::Dynamic)
	{
		WriteFileWithOffset(image_file, vhd_footer, VHD_HEADER_LOCATION);
		WriteFileWithOffset(image_file, vhd_dyn_header, VHD_DYNAMIC_HEADER_LOCATION);
		WriteFileWithOffset(image_file, vhd_block_allocation_table.get(), vhd_table_sector_aligned_count * sizeof(VHD_BAT_ENTRY), VHD_BLOCK_ALLOC_TABLE_LOCATION);
		WriteFileWithOffset(image_file, vhd_footer, vhd_next_free_address + require_alignment - sizeof vhd_footer);
#ifdef _DEBUG
		LARGE_INTEGER fsize;
		_ASSERT(GetFileSizeEx(image_file, &fsize));
		_ASSERT(fsize.QuadPart % require_alignment == 0);
#endif
		THROW_IF_WIN32_BOOL_FALSE(FlushFileBuffers(image_file));
		return;
	}
	_CrtDbgBreak();
	THROW_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
}
void VHD::CheckConvertible() const
{
	if (IsFixed())
	{
		THROW_WIN32_IF(ERROR_ARITHMETIC_OVERFLOW, GetDiskSize() / GetBlockSize() > UINT32_MAX);
		return;
	}
	if (vhd_footer.DiskType == VHDType::Difference)
	{
		throw std::runtime_error("Differencing VHD is not supported.");
	}
	if (vhd_block_size < require_alignment)
	{
		throw std::runtime_error("VHD block size is smaller than required alignment.");
	}
	for (UINT32 i = 0; i < vhd_table_entries_count; i++)
	{
		if (vhd_block_allocation_table[i] != VHD_UNUSED_BAT_ENTRY)
		{
			if ((vhd_block_allocation_table[i] * VHD_SECTOR_SIZE + vhd_bitmap_actual_size) % require_alignment != 0)
			{
				throw std::runtime_error("VHD data blocks is not aligned.");
			}
		}
	}
}
std::optional<UINT64> VHD::ProbeBlock(UINT32 index) const
{
	_ASSERT(index < GetTableEntriesCount());
	if (vhd_footer.DiskType == VHDType::Fixed)
	{
		return static_cast<UINT64>(GetBlockSize()) * index;
	}
	if (vhd_footer.DiskType == VHDType::Dynamic)
	{
		if (UINT64 block_address = vhd_block_allocation_table[index]; block_address != VHD_UNUSED_BAT_ENTRY)
		{
			return block_address * VHD_SECTOR_SIZE + vhd_bitmap_actual_size;
		}
		return std::nullopt;
	}
	_CrtDbgBreak();
	THROW_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
}
UINT64 VHD::AllocateBlock(UINT32 index)
{
	if (const auto offset = ProbeBlock(index))
	{
		return *offset;
	}
	FILE_END_OF_FILE_INFO eof_info = { {.QuadPart = static_cast<LONGLONG>(vhd_next_free_address) + vhd_bitmap_aligned_size + vhd_block_size } };
	_ASSERT(std::cmp_greater(eof_info.EndOfFile.QuadPart, VHD_BLOCK_ALLOC_TABLE_LOCATION));
	THROW_WIN32_IF(ERROR_ARITHMETIC_OVERFLOW, eof_info.EndOfFile.QuadPart > static_cast<LONGLONG>(UINT32_MAX) * VHD_SECTOR_SIZE);
	THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(image_file, FileEndOfFileInfo, &eof_info, sizeof eof_info));
	ULONG _;
	DUPLICATE_EXTENTS_DATA dup_extent = {
		.FileHandle = image_file,
		.SourceFileOffset = {.QuadPart = static_cast<LONGLONG>(vhd_template_bitmap_address) },
		.TargetFileOffset = {.QuadPart = static_cast<LONGLONG>(vhd_next_free_address) },
		.ByteCount = {.QuadPart = require_alignment }
	};
	if (vhd_template_bitmap_address == 0 || !DeviceIoControl(image_file, FSCTL_DUPLICATE_EXTENTS_TO_FILE, &dup_extent, sizeof dup_extent, nullptr, 0, &_, nullptr))
	{
		_ASSERT(vhd_template_bitmap_address == 0 || GetLastError() == ERROR_BLOCK_TOO_MANY_REFERENCES);
		vhd_template_bitmap_address = vhd_next_free_address;
		const auto vhd_bitmap_buffer = std::make_unique<std::byte[]>(vhd_bitmap_aligned_size); // 0 fill to expect compression by the SSD.
		memset(vhd_bitmap_buffer.get() + vhd_bitmap_padding_size, 0xFF, vhd_bitmap_actual_size);
		WriteFileWithOffset(image_file, vhd_bitmap_buffer.get(), vhd_bitmap_aligned_size, vhd_next_free_address);
	}
	vhd_block_allocation_table[index] = static_cast<UINT32>((vhd_next_free_address + vhd_bitmap_padding_size) / VHD_SECTOR_SIZE);
	vhd_next_free_address += vhd_bitmap_aligned_size + vhd_block_size;
	_ASSERT(vhd_next_free_address % require_alignment == 0);
	return vhd_next_free_address - vhd_block_size;
}
std::unique_ptr<Image> VHD::DetectImageFormatByData(HANDLE file)
{
	LARGE_INTEGER fsize;
	THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file, &fsize));
	if (std::cmp_less(fsize.QuadPart, VHD_BLOCK_ALLOC_TABLE_LOCATION + VHD_SECTOR_SIZE + sizeof(VHD_FOOTER)))
	{
		return nullptr;
	}
	decltype(VHD_FOOTER::Cookie) vhd_cookie;
	ReadFileWithOffset(file, &vhd_cookie, round_up(fsize.QuadPart - VHD_FOOTER_OFFSET, VHD_FOOTER_ALIGN));
	if (vhd_cookie == VHD_COOKIE)
	{
		return std::unique_ptr<Image>(new VHD);
	}
	return nullptr;
}
UINT32 VHD::VHDChecksumUpdate(auto* header)
{
	header->Checksum = 0;
	UINT32 checksum = 0;
	for (UINT32 counter = 0; counter < sizeof(*header); counter++)
	{
		checksum += reinterpret_cast<PBYTE>(header)[counter];
	}
	return header->Checksum = std::byteswap(~checksum);
}
bool VHD::VHDChecksumValidate(auto* header)
{
	const UINT32 checksum = header->Checksum;
	return VHDChecksumUpdate(header) == checksum;
}
UINT32 VHD::CHSCalculate(UINT64 disk_size)
{
	const int totalSectors = static_cast<int>(std::min(disk_size / VHD_SECTOR_SIZE, 65535ULL * 16 * 255));
	int cylinderTimesHeads;
	int heads;
	int sectorsPerTrack;
	if (totalSectors >= 65535 * 16 * 63)
	{
		sectorsPerTrack = 255;
		heads = 16;
		cylinderTimesHeads = totalSectors / sectorsPerTrack;
	}
	else
	{
		sectorsPerTrack = 17;
		cylinderTimesHeads = totalSectors / sectorsPerTrack;
		heads = std::max((cylinderTimesHeads + 1023) / 1024, 4);
		if (cylinderTimesHeads >= heads * 1024 || heads > 16)
		{
			sectorsPerTrack = 31;
			heads = 16;
			cylinderTimesHeads = totalSectors / sectorsPerTrack;
		}
		if (cylinderTimesHeads >= heads * 1024)
		{
			sectorsPerTrack = 63;
			heads = 16;
			cylinderTimesHeads = totalSectors / sectorsPerTrack;
		}
	}
	int cylinders = cylinderTimesHeads / heads;
	return std::byteswap(cylinders << 16 | heads << 8 | sectorsPerTrack);
}