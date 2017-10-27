# MakeVHDX
Converting a VHD/VHDX to VHD/VHDX using [block cloning](https://docs.microsoft.com/windows-server/storage/refs/block-cloning) to share used data blocks.
This is proof of concept.
```
Make VHD/VHDX that shares data blocks with source.

MakeVHDX [-fixed | -dynamic] [-bN] [-sparse] Source [Destination]

Source       Specifies conversion source.
Destination  Specifies conversion destination.
             If not specified, use file extension exchanged with ".vhd" and ".vhdx".
-fixed       Make output image is fixed file size type.
-dynamic     Make output image is variable file size type.
             If neither is specified, will be same type as source.
-b           Specifies output image block size by 1MB. It must be power of 2.
             Ignore this indication when output is fixed VHD.
-sparse      Make output image is sparse file.
```
## Requirements and Limitations
- Source and destination must have placed on same ReFS v2 volume.
### Convertion from VHD
- [VHD must be aligned to 4 KB.](https://msdn.microsoft.com/en-us/library/windows/hardware/dn567657.aspx#VHD_FORMAT)
- [ReFS must be formatted with 4 KB cluster size.](https://blogs.technet.microsoft.com/filecab/2017/01/13/cluster-size-recommendations-for-refs-and-ntfs/)
### Convertion to VHD
- When cluster size is 64 KB, alignment will be 64 KB. Will break alignment when update with any VHD parser.
- Block size less than 1 MB can not be specified.
- Block size other than 2 MB can be specified, but only 2 MB can be used by Microsoft VHD parser.
### Convertion from Fixed type to Dynamic type
- Output image will be large. This tool does not inspect file system free space in image, or zero-ed data block.

## License
MIT License  
Except `crc32c.cpp`, `crc32c.h` and `generated-constants.cpp`.
[These files under the zlib license.](https://crc32c.angeloflogic.com/license/)
