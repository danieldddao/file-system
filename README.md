# Implemented a file system that resembles FAT using C

### The file system will be stored in a binary file within the current directory. Overall, the file system has the following setup:

The file is organized in clusters that are composed of multiple sectors. The number of sectors per cluster and the size of sectors (in bytes) are set during the formatting process.

### The MBR is guaranteed to start on the first cluster of the “disk”. The MBR contains the following information:
* sector_size (2 bytes) - the size of a sector in bytes (at least 64 bytes)
* cluster_size (2 bytes) - the size of the cluster in sectors (at least 1)
* disk_size (2 bytes) - the size of the disk in clusters
* fat_start, fat_length (2 bytes) - the start/length of the FAT area on the disk
* data_start, data_length (2 bytes) - the start/length of the Data area on the disk
* disk_name - the name of the disk.

