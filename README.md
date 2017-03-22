# MakeVHDX
Create a VHDX using [block cloning](https://technet.microsoft.com/en-us/windows-server-docs/storage/refs/block-cloning) to share used data blocks with VHD.
This is proof of concept.

## Requirements and Limitations
- Source VHD and destination VHDX must have placed on same ReFS v2 volume.
- [VHD must be aligned to 4 KB.](https://msdn.microsoft.com/en-us/library/windows/hardware/dn567657.aspx#VHD_FORMAT)
- [ReFS must be formatted with 4 KB cluster size.](https://blogs.technet.microsoft.com/filecab/2017/01/13/cluster-size-recommendations-for-refs-and-ntfs/)
- Only dynamic VHD is supported.
- Type of VHDX will be dynamic.
- Block size of VHD is supported only 2 MB.
- Block size of VHDX will be 2 MB.
- VHD must not be sparse attributes.
- VHD must not have integrity stream.

### License
MIT License  
Except `crc32c.cpp`, `crc32c.h` and `generated-constants.cpp`.
[These files under the zlib license.](https://crc32c.angeloflogic.com/license/)
