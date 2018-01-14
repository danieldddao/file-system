# Implemented a file system that resembles FAT using C

### The file system will be stored in a binary file within the current directory. Overall, the file system has the following setup:
<img src="https://github.com/danieldddao/file-system/blob/master/img.png" width="500">

The file is organized in clusters that are composed of multiple sectors. The number of sectors per cluster and the size of sectors (in bytes) are set during the formatting process.

### The MBR is guaranteed to start on the first cluster of the “disk”. The MBR contains the following information:
* sector_size (2 bytes) - the size of a sector in bytes (at least 64 bytes)
* cluster_size (2 bytes) - the size of the cluster in sectors (at least 1)
* disk_size (2 bytes) - the size of the disk in clusters
* fat_start, fat_length (2 bytes) - the start/length of the FAT area on the disk
* data_start, data_length (2 bytes) - the start/length of the Data area on the disk
* disk_name - the name of the disk.

### The FAT area is used to keep track the allocation of clusters on disk. The FAT will contain one entry for each cluster in the data area. The value of the FAT entry is a 16-bit integer that either indicates that the cluster is unallocated (0xFFFF) or points to the next cluster that is allocated to a file/directory. The pointer is calculated relative to the start of the data area. Thus, a value of 0 indicates the first cluster in the data area that will be physically stored at data_start. In general, a value of x indicates cluster x that will be stored physically in cluster data_start + x. 
The root directory is guaranteed to occupy the first cluster in the data area. The directory and files are stored in entries that have the following format:
* entry_type (1 byte) - indicates if this is a file/directory (0 - file, 1 - directory)
* creation_time (2 bytes) - format described below
* creation_date (2 bytes) - format described below
* length of entry name (1 byte)  
* entry name (16 bytes) - the file/directory name
* size (4 bytes) - the size of the file in bytes. Should be zero for directories:
* pointers to children files or directories
  
  ** pointer type (1 byte) - (0 = pointer to a file, 1 = pointer to a directory, 2 = pointer to  =another entry describing more children for this directory, 3 = a free entry)
  
  ** reserved (1 byte)
* start_pointer (2 bytes) - points to the start of the entry describing the child 

