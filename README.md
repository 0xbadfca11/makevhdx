# MakeVHDX [![Build status](https://ci.appveyor.com/api/projects/status/9ugasf69fmwo4gs6)](https://ci.appveyor.com/project/0xbadfca11/makevhdx)
Converting a VHD/VHDX to VHD/VHDX using [block cloning](https://docs.microsoft.com/windows-server/storage/refs/block-cloning) to share used data blocks.
This is proof of concept.
```
Make VHD/VHDX that shares data blocks with source.

MakeVHDX [-fixed | -dynamic] [-bN] [-sparse] Source [Destination]

Source       Specifies conversion source.
Destination  Specifies conversion destination.
             If not specified, will use file extension
             exchanged with ".vhd" when the source is ".vhdx", exchanged with ".vhdx" otherwise.
-fixed       Make output image is fixed file size type.
-dynamic     Make output image is variable file size type.
             If neither is specified, will be same type as source.
-b           Specifies output image block size by 1MB. It must be power of 2.
             Silently ignore, if output is image type that doesn't use blocks. (Such as fixed VHD)
-sparse      Make output image is sparse file.

Supported Image Types and File Extensions
 VHDX : .vhdx (.avhdx Disallowed)
 VHD  : .vhd  (.avhd  Disallowed)
 RAW  : .* (Other than above)
```
## Requirements and Limitations
- Source and destination must have placed on same ReFS v2 volume.
- Differencing type can not be source and/or destination.
### Convertion from VHD
- [VHD must be aligned to 4 KB.](https://docs.microsoft.com/en-us/windows-server/administration/performance-tuning/role/hyper-v-server/storage-io-performance#vhd-format)
- [ReFS must be formatted with 4 KB cluster size.](https://blogs.technet.microsoft.com/filecab/2017/01/13/cluster-size-recommendations-for-refs-and-ntfs/)
### Convertion to VHD
- When cluster size is 64 KB, alignment will be 64 KB. Will break alignment when update with any VHD parser.
- Block size less than 1 MB can not be specified.
- Block size other than 2 MB can be specified, but only 2 MB can be used by Microsoft VHD parser.
### Convertion from Fixed type to Dynamic type
- Output image will be large. This tool does not inspect file system free space in image, or zero-ed data block.

## License
MIT License
