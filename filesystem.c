//
//  Homework 4
//  Created by Daniel Dao on 11/16/14.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Define TRUE and FALSE if they have not already been defined
#ifndef FALSE
#define FALSE (-1)
#endif
#ifndef TRUE
#define TRUE (0)
#endif

#define DISK_NAME "MY DISK" //name of the disk (Can't exceed 31 bytes and should be stored as a null-terminated string)

typedef struct list_item{
    int val;
    struct list_item *next;
}LIST_ITEM_T;

typedef struct single_list {
   LIST_ITEM_T *head;
   LIST_ITEM_T *tail;
} LINKED_LIST_T;

typedef struct  __attribute__ ((__packed__)) {
    uint16_t sector_size;   // size of a sector in bytes
    uint16_t cluster_size;  // size of the cluster in sectors
    uint16_t disk_size;     // size of the disk in clusters
    uint16_t fat_start;     // start of the FAT area on the disk
    uint16_t fat_len;       // length of the FAT area on the disk
    uint16_t data_start;    // start of the Data area on the disk
    uint16_t data_len;      // length of the Data area on the disk
    char     disk_name[32];
} MBR_T;

typedef struct __attribute__ ((__packed__)) {
    uint8_t		entry_type;     // indicates if this is a file/directory (0 - file, 1 - directory)
    uint16_t	creation_time;
    uint16_t    creation_date;
    uint8_t		name_len;       // length of entry name
    char		name[16];       // the file/directory name
    uint32_t	size;           // the size of the file in bytes. Should be zero for directories
} ENTRY_T;

typedef struct __attribute__ ((__packed__)) {
    uint8_t	 type;      // pointer type:
                        // 0 = pointer to a file,
                        // 1 = pointer to a directory,
                        // 2 = pointer to another entry describing more children for this directory
    uint8_t  reserved;  // 0xFF to indicate that it's the last child in the cluster. Otherwise, the value is 0.
    uint16_t start;     // points to the start of the entry describing the child (Ex: root directory starts at cluster data.start+0)
} ENTRY_PTR_T;


typedef struct __attribute__ ((__packed__)) {
    uint16_t entry; //entry for the coresponding cluster in the Data area
} FAT_ENTRY_T;

MBR_T *myMBR; // MBR
FAT_ENTRY_T *fat_table; // FAT table
ENTRY_T *data; // Containing all files and directory entry.
LINKED_LIST_T *op_writing_flist; // Keep track on which file is being opened in writing mode
LINKED_LIST_T *op_reading_flist; // Keep track on which file is being opened in reading mode

int loaded = FALSE; // indate whether the disk has been loaded in memory.

LIST_ITEM_T *create_list(LINKED_LIST_T *list, int val)
{
    LIST_ITEM_T *ptr = (LIST_ITEM_T*)malloc(sizeof(LIST_ITEM_T));
    if(NULL == ptr)
    {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->val = val;
    ptr->next = NULL;
    
    list->head = list->tail = ptr;
    //printf("Added [%d] to the list\n",val);
    return ptr;
}

LIST_ITEM_T *add_to_list(LINKED_LIST_T *list, int val)
{
    if(NULL == list->head)
        return (create_list(list, val));
    
    
    LIST_ITEM_T *ptr = (LIST_ITEM_T*)malloc(sizeof(LIST_ITEM_T));
    if(ptr == NULL)
    {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->val = val;
    ptr->next = NULL;
    
    list->tail->next = ptr;
    list->tail = ptr;
    //printf("Added  [%d]  to the list\n",val);
    return ptr;
}

LIST_ITEM_T *search_in_list(LINKED_LIST_T *list, int val, LIST_ITEM_T **prev)
{
    LIST_ITEM_T *ptr = list->head;
    LIST_ITEM_T *tmp = NULL;
    int found = FALSE;
    while(ptr != NULL)
    {
        if(ptr->val == val)
        {
            found = TRUE;
            break;
        }
        else
        {
            tmp = ptr;
            ptr = ptr->next;
        }
    }
    
    if(TRUE == found)
    {
        if(prev)
            *prev = tmp;
        return ptr;
    }
    else
    {
        return NULL;
    }
}

int delete_from_list(LINKED_LIST_T *list, int val)
{
    LIST_ITEM_T *prev = NULL;
    LIST_ITEM_T *del = NULL;
    int value = val;
    del = search_in_list(list,value,&prev);

    if(del == NULL)
    {
        printf("File hasn't been opened! Can't close the file\n");
        return FALSE;
    }
    else
    {
        if(prev != NULL)
            prev->next = del->next;
        
        if(del == list->tail)
        {
            list->tail = prev;
            if(list->head->next == NULL)
                list->head = prev;
        }
        else if(del == list->head)
        {
            list->head = del->next;
        }
    }
    
    free(del);
    del = NULL;
    printf("Closing the file succesfully!\n\n");
    return TRUE;
}

void print_list(LINKED_LIST_T *list)
{
    LIST_ITEM_T *ptr = list->head;
    
    printf("\n -------Printing list Start------- \n");
    while(ptr != NULL)
    {
        printf(" [%d] ",ptr->val);
        ptr = ptr->next;
    }
    printf("\n -------Printing list End------- \n\n");
}

/* return date as integer corresponding to 16-bit field in big-endian format */
uint16_t fs_getdate() {
    time_t t = time(NULL);
    struct tm *tptr = localtime(&t);
    uint16_t mday,mon,year;
    uint16_t date,tmp;
    
    mday = tptr->tm_mday;
    mon  = tptr->tm_mon;
    year = tptr->tm_year;
    year -= 80;
    mon += 1;
    if ((mday<1|mday>31) | (mon<1|mon>12) | (year<0|year>127)) {
        printf("date is not correct!\n");
        exit(1);
    }

    date = (mday<<0) | (mon<<5) | (year<<9);
    // convert to big-endian format.
    date = htons(date);
    return date;
}

/* return time as integer corresponding to 16-bit field in big-endian format */
uint16_t fs_gettime() {
    time_t t = time(NULL);
    struct tm *tptr = localtime(&t);
    uint16_t min,sec,hr;
    uint16_t time,tmp;
    
    hr   = tptr->tm_hour;
    min  = tptr->tm_min;
    sec  = tptr->tm_sec;
    sec /= 2;
    if ((hr<0|hr>23) | (min<0|min>59) | (sec<0 | sec>29)) {
        printf("time is not correct!\n");
        exit(1);
    }

    time = (sec<<0) | (min<<5) | (hr<<11);
    // convert to big-endian format.
    //tmp = time;
    //tmp <<= 8;
    //time >>= 8;
    //time |= tmp;
    time = htons(time);
    return time;
}

/* Write the given data to the binary file starting from position 'offset' */
void ffwrite(int offset, ssize_t size, void *ptr) {
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!");
        exit(1);
    }
    fseek(file, offset, SEEK_SET);
    while (size > 0) {
        long r = fwrite(ptr, 1, size,file);
        if (r <= 0) {
            printf("Error! Can't write the data to file\n");
            exit(2);
        }
        ptr = &ptr[r];
        size = size - r;
    }
    fclose(file);
}

/* Initialize all the pointers of the directory and save data to disk.
   type = 0xFF to indicate that this pointer points to nowhere
 */
void intial_pointer(uint16_t start, int is_start_cluster) {
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!");
        exit(1);
    }
    int max_child;
    ENTRY_PTR_T pt;
    pt.type = 0xFF;
    pt.reserved = 0;
    pt.start = 0;
    if(is_start_cluster == TRUE) {
        max_child = (int)(myMBR->cluster_size*myMBR->sector_size - sizeof(ENTRY_T))/sizeof(ENTRY_PTR_T);
        fseek(file, start*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
        for (int i=0; i<max_child; i++) {
            if (i == max_child-1)
                pt.reserved = 0xFF; // Indicate the last child (pointer)
            fwrite(&pt, 1, sizeof(ENTRY_PTR_T),file);
        }
    } else {
        max_child = (int)(myMBR->cluster_size*myMBR->sector_size)/sizeof(ENTRY_PTR_T);
        fseek(file, start*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
        for (int i=0; i<max_child; i++) {
            if (i == 0)
                pt.reserved = 0xFE; // Indicate the first child (pointer)
            if (i == max_child-1)
                pt.reserved = 0xFF; // Indicate the last child (pointer)
            fwrite(&pt, 1, sizeof(ENTRY_PTR_T),file);
            pt.reserved = 0;
        }
    }
    fclose(file);
}

/* Return FALSE if the absolute_path is not in correct format (Correct format: /folder/childfolder/grandchildfolder) */
int check_abs_path(char *absolute_path) {
    if (strlen(absolute_path) == 0) // Ex: ''
        return FALSE;
    if (absolute_path[0] != '/')  // Ex:'folder/childfolder' or ' '
        return FALSE;
    if (strlen(absolute_path) >1) {
        if(absolute_path[strlen(absolute_path)-1] == '/') // Ex:'/folder/childfolder/'
            return FALSE;
        else for (int i=0; i<(strlen(absolute_path)-2); i++)
            if (absolute_path[i] == '/' && absolute_path[i+1] == '/') // Ex: '//folder' or '/folder//childfolder'
                return FALSE;
    }
    return TRUE;
}

/* check if there's a free cluster based on the FAT area */
int free_cluster_check() {
    for (int i=0; i<myMBR->data_len; i++)
        if (fat_table[i].entry == 0xFFFF)
            return  i;
    return FALSE;
}

/* Search whether the 'child_name' directory exists */
ENTRY_PTR_T *search_child(char *child_name, uint16_t start, int type) {
    // Check if length of child_name is longer than 16 characters.
    if (strlen(child_name) > 16) {
        printf("Error! child name is too long\n");
        exit(1);
    }
    // Check if the path exists.
    if (start < 0 || start >= myMBR->data_len) {
        printf("Error! The starting cluster is not correct\n");
        exit(2);
    }
    if (type != 0 && type != 1) {
        printf("Error! Type must be 0 or 1\n");
        exit(3);
    }
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(4);
    }
    
    ENTRY_PTR_T *buffer = malloc(sizeof(ENTRY_PTR_T));
    if (buffer == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
        exit(5);
    }
    fseek(file, (myMBR->data_start+start)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
    fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    uint16_t i = start;
    
    while (buffer->type != 0xFF) {
        // If buffer pointers to a directory
        if (buffer->type == type) {
            long x = ftell(file);
            ENTRY_T buffer_child;
            fseek(file, (myMBR->data_start+buffer->start)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            fread(&buffer_child, 1, sizeof(ENTRY_T), file);
            if (strcmp(child_name, buffer_child.name) == 0){
                fclose(file);
                return buffer;
            }
            fseek(file, x, SEEK_SET); // seek back to the pointer.
        }
        // If reaching the last child in cluster
        if (buffer->reserved == 0xFF) { // Last child in cluster
            if (fat_table[i].entry == i) { // No additional cluster
                fclose(file);
                return NULL;
            } else {
                i = fat_table[i].entry;
                fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            }
        }
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    }
    fclose(file);
    free(buffer);
    return NULL;
}

/*
    FORMAT
*/
void format(uint16_t sector_size, uint16_t cluster_size, uint16_t disk_size) {
    
    if (sector_size < 64 | cluster_size < 1 | disk_size < 3) {
        printf("Can't initialize the MBR and FAT data structures\n");
        EXIT_FAILURE;
    }
    // Create file sytem binay file.
    FILE *file = fopen(DISK_NAME, "wb");
    if (!file) {
        printf("Unable to open file!");
        exit(1);
    }
    uint8_t vl = 0xEE;
    for (int i=0; i<(sector_size*cluster_size*disk_size); i++)
        fwrite(&vl, 1, 1, file);
    fclose(file);
    
    myMBR = malloc(sizeof(MBR_T));
    if (myMBR == NULL)
    {
        printf("Failed to allocate %lu bytes memory\n", sizeof(MBR_T));
        exit(2);
    }
    
    // MBR area
    myMBR->sector_size   = sector_size;
    myMBR->cluster_size  = cluster_size;
    myMBR->disk_size     = disk_size;
    uint16_t num_cluster = (sector_size*cluster_size)*(disk_size-1)/(2+sector_size*cluster_size);
    myMBR->fat_start = 1;
    myMBR->fat_len   = (disk_size -1-num_cluster);
    
    // FAT area
    fat_table = malloc(sizeof(FAT_ENTRY_T[num_cluster]));
    if (fat_table == NULL)
    {
        printf("Failed to allocate %lu bytes memory\n", sizeof(FAT_ENTRY_T[num_cluster]));
        exit(2);
    }
    for (int i = 0; i < num_cluster; i++)
        fat_table[i].entry =  0xFFFF;

    // Data area
    myMBR->data_start = myMBR->fat_len + 1;
    myMBR->data_len   = num_cluster;
    strcpy(myMBR->disk_name,DISK_NAME);
    data = malloc(sizeof(ENTRY_T[myMBR->data_len]));
    if (data == NULL)
    {
        printf("Failed to allocate %lu bytes memory\n", sizeof(ENTRY_T[myMBR->data_len]));
        exit(3);
    }

    /* Root directory */
    ENTRY_T root;
    root.entry_type = 1;
    root.creation_time  = fs_gettime();
    root.creation_date = fs_getdate();
    char name[16] = "Root";
    strcpy(root.name,name);
    root.name_len = strlen(root.name);
    root.size = 0;
    fat_table[0].entry = 0;

    data[0] = root;
    data[1].entry_type = 0xFF;
    
    /* save information to disk */
    ffwrite(0, sizeof(MBR_T), myMBR); // save MBR
    ffwrite(myMBR->cluster_size*myMBR->sector_size, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, fat_table); // save FAT Area
    ffwrite((myMBR->data_start + 0)*myMBR->cluster_size*myMBR->sector_size,sizeof(ENTRY_T), &root); // Save root directory
    intial_pointer(myMBR->data_start, TRUE); // pointers to children of root directory
    loaded = TRUE;
    printf("\nFORMAT IS DONE!\n\n");
}

/*
    LOAD_DISK
*/
void load_disk(char *disk_file) {
    if (strcmp(disk_file, DISK_NAME) != 0) {
        printf("The disk name is not correct! Can't load the disk.\n");
        exit(1);
    }
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(1);
    }
    
    // Load MBR Area
    myMBR = malloc(sizeof(MBR_T));
    if (myMBR == NULL)
    {
        printf("Can't load MBR Area! Failed to allocate %d bytes memory\n", (int)sizeof(MBR_T));
        exit(2);
    }
    fseek(file, 0, SEEK_SET);
    fread(myMBR, 1, sizeof(MBR_T), file);

    // Load FAT Area
    fat_table = malloc(sizeof(FAT_ENTRY_T[myMBR->data_len]));
    if (fat_table == NULL)
    {
        printf("Can't load FAT Area! Failed to allocate %d bytes memory\n", myMBR->data_len);
        exit(3);
    }
    fseek(file, myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
    fread(fat_table, 1, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, file);
    
    // Load Data Area
    data = malloc(sizeof(ENTRY_T[myMBR->data_len]));
    if (data == NULL)
    {
        printf("Failed to allocate %lu bytes memory\n", sizeof(ENTRY_T[myMBR->data_len]));
        exit(4);
    }

    fseek(file, (myMBR->data_start + 0)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
    fread(&data[0], 1, sizeof(ENTRY_T), file);
    int x = 1;
    int table[myMBR->data_len];
    for (int i = 1; i<myMBR->data_len; i++)
        table[i] = 0;
    
    for (int i = 1; i <myMBR->data_len; i++) {
        if (fat_table[i].entry != 0xFFFF) {
            if (table[i] == 0) {
                fseek(file, (myMBR->data_start + i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
                fread(&data[x], 1, sizeof(ENTRY_T), file);
                x++;
            }
            int j = i;
            while (fat_table[j].entry != j) {
                j = fat_table[j].entry;
                table[j] = -1;
            }
        }
    }
    data[x].entry_type = 0xFF; // Indicate end of array.
    fclose(file);
    loaded = TRUE;
    printf("\nLoaded disk '%s' succesfully!\n\n",disk_file);
}


/*
    FS_OPENDIR
*/
// Find the start of directory from given absolute_path (Ex: '/' starts at 0) */
// Return -1 if the absolute_path doesn't exist //
int fs_opendir(char *absolute_path) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    char *buffer = malloc(strlen(absolute_path));
    if (buffer == 0)
    {
        printf("Failed to allocate %d bytes memory\n", (int)strlen(absolute_path));
        exit(2);
    }
    strcpy(buffer, absolute_path);
    if (check_abs_path(buffer) == FALSE) {
        printf("Error! The absolute path is not in correct format\n");
        exit(3);
    }
    
    char *pch;
    ENTRY_PTR_T *ptr;
    int child_start = 0;
    pch = strtok(buffer,"/");
    while (pch != NULL) {
        ptr = search_child(pch, child_start, 1);
        if (ptr == NULL)
            return -1;
        pch = strtok(NULL, "/");
        child_start = ptr->start;
    }
    free(buffer);
    return child_start;
}

/*
    FS_MKDIR
*/
void fs_mkdir(int dh, char *child_name) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    // Check if the path exists.
    if (dh < 0 || dh >= myMBR->data_len) {
        printf("Error! The absolute path doesn't exist\n");
        exit(2);
    }
    // Check if length of child_name is longer than 16 characters.
    if (strlen(child_name) > 16) {
        printf("Error! child name is too long\n");
        exit(4);
    }
    printf("Absolute path starts at cluster %d\n",dh);
    // Check if free cluster is available.
    int x = free_cluster_check();
    if (x == FALSE) {
        printf("Error! There's no free cluster. Can't create directory '%s'\n",child_name);
        exit(3);
    }
    
    // Create new directory entry
    ENTRY_T *new_dir = malloc(sizeof(ENTRY_T));
    if (new_dir == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_T));
        exit(5);
    }

    new_dir->entry_type = 1;
    new_dir->creation_time  = fs_gettime();
    new_dir->creation_date = fs_getdate();
    new_dir->name_len = strlen(child_name);
    strcpy(new_dir->name,child_name);
    new_dir->size = 0;
    
    // Update FAT area
    fat_table[x].entry = x;
    ffwrite(myMBR->cluster_size*myMBR->sector_size, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, fat_table);

    // Create new pointer entry
    ENTRY_PTR_T *new_dir_ptr = malloc(sizeof(ENTRY_PTR_T));
    if (new_dir_ptr == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
        exit(6);
    }
    new_dir_ptr->type = 1;
    new_dir_ptr->reserved = 0;
    new_dir_ptr->start = x; // starts at cluster x.
    
    /* Save information of new directory to disk */
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(7);
    }
    ENTRY_PTR_T *buffer = malloc(sizeof(ENTRY_PTR_T));
    if (buffer == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
        exit(8);
    }
    // Find where to save new pointer entry
    fseek(file, (myMBR->data_start+dh)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
    fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    int index = 0;
    int saved = FALSE;
    uint16_t i = dh;
    
    while (buffer->type != 0xFF && buffer->type != 0xFE) {
        // If reaching the last child in cluster
        if (buffer->reserved == 0xFF) { // Last child in cluster
            if (fat_table[i].entry == i) { // No additional cluster
                fclose(file);
                int y = free_cluster_check();
                if (y == FALSE) {
                    printf("Error! Can't find a free cluster to store the pointer to new directory '%s'\n",child_name);
                    exit(7);
                }
                printf("Pointer can't fill in the cluster %d, It'll fill in new cluster %d\n",i,y);
                intial_pointer(myMBR->data_start + y, FALSE);

                // Save pointer to the new cluster
                new_dir_ptr->reserved = 0xFE;
                ffwrite((myMBR->data_start + y)*myMBR->cluster_size*myMBR->sector_size, sizeof(ENTRY_PTR_T),new_dir_ptr);
                
                // Update FAT table
                fat_table[i].entry = y;
                fat_table[y].entry = y;
                
                ffwrite(myMBR->cluster_size*myMBR->sector_size, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, fat_table);
                saved = TRUE;
                break;
            } else {
                index = -1;
                i = fat_table[i].entry;
                fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            }
        }
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
        index++;
    }
    fclose(file);
    if (saved != TRUE) {
        if (buffer->reserved == 0xFE)
            new_dir_ptr->reserved = 0xFE;
        if (buffer->reserved == 0xFF)
            new_dir_ptr->reserved = 0xFF;

        if (i == dh) {
            ffwrite((myMBR->data_start + i)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T) + sizeof(ENTRY_PTR_T)*index, sizeof(ENTRY_PTR_T),new_dir_ptr);
        } else {
            ffwrite((myMBR->data_start + i)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_PTR_T)*index, sizeof(ENTRY_PTR_T),new_dir_ptr);
        }
    }
    
    // Save new directory entry and intialize its pointers
    ffwrite((myMBR->data_start + x)*myMBR->cluster_size*myMBR->sector_size,sizeof(ENTRY_T), new_dir);
    intial_pointer(myMBR->data_start + x, TRUE); // pointers to children

    printf("Sucessfully created the directory '%s' at cluster %d\n\n",child_name, x);
    free(new_dir);
    free(new_dir_ptr);
    free(buffer);
}

/*
   FS_LS
*/
// Return entr_t of (childnum+1)-th child.
// Ex: fs_ls(fs_opendir("/"), 0); will return entry_t of 1st child if it exists.
// Note: Return NULL if (childnum+1)-th child doesn't  exist.
ENTRY_T *fs_ls(int dh, int child_num) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    if (dh <0 || dh >= myMBR->data_len) {
        printf("Error! The absolute path doesn't exist\n");
        exit(2);
    }
    if (child_num < 0) {
        printf("Error! Child number is not correct!\n");
        exit(3);
    }
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(4);
    }
    
    ENTRY_PTR_T *buffer = malloc(sizeof(ENTRY_PTR_T));
    if (buffer == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
        exit(6);
    }
    
    fseek(file, (myMBR->data_start+dh)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
    fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    int index = 0;
    uint16_t i = dh;

    while (buffer->type != 0xFF) {
        // If buffer pointers to a directory or file
        if (buffer->type == 0 || buffer->type == 1) {
            if (index == child_num) {
                ENTRY_T *child = malloc(sizeof(ENTRY_T));
                if (child == NULL)
                {
                    printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
                    exit(5);
                }
                fseek(file, (myMBR->data_start+buffer->start)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
                fread(child, 1, sizeof(ENTRY_T), file);
                fclose(file);
                free(buffer);
                return child;
            }
        }
        // If reaching the last child in cluster
        if (buffer->reserved == 0xFF) { // Last child in cluster
            if (fat_table[i].entry == i) { // No additional cluster
                fclose(file);
                return NULL;
            } else {
                i = fat_table[i].entry;
                fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            }
        }
        if (buffer->type != 0xFE)
            index++;
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    }

    fclose(file);
    free(buffer);
    return NULL;
}

/*
    FS_OPEN
*/
// Mode = "w" or "r". A file is created by opening it in “w” mode. Opening a file in “r” mode would result in an error.
// Return the starting cluster of the file. Otherwise, return -1 if the file is being opened.
int fs_open(char *absolute_path, char *mode) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    if (strcmp(mode, "w") != 0 && strcmp(mode, "r") != 0) {
        printf("Error! The mode is not correct!\n");
        exit(2);
    }
    char *buffer_path = malloc(strlen(absolute_path));
    if (buffer_path == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)strlen(absolute_path));
        exit(3);
    }
    strcpy(buffer_path, absolute_path);
    if (check_abs_path(buffer_path) == FALSE) {
        printf("Error! The absolute path is not in correct format\n");
        exit(4);
    }
    char *pch, *tmp;
    ENTRY_PTR_T *dir_ptr = NULL;
    ENTRY_PTR_T *file_ptr = malloc(sizeof(ENTRY_PTR_T));
    uint16_t child_start = 0;
    pch = strtok(buffer_path,"/");
    if (pch == NULL) {
        printf("Error! The absolute path is not correct\n");
        exit(4);
    }
    while (pch != NULL) {
        tmp = strtok(NULL, "/");
        // Reach the file name
        if (tmp == NULL) {
            file_ptr = search_child(pch, child_start, 0);
            break;
        } else
            dir_ptr = search_child(pch, child_start, 1);
        if (dir_ptr == NULL) {
            printf("Error! The directory containing the file doesn't exist\n");
            exit(5);
        }
        pch = tmp;
        child_start = dir_ptr->start;
    }

    if (file_ptr == NULL){
        if(strcmp(mode, "w") == 0) {
            printf("File doesn't exist. Creating file '%s'\n",pch);
            // Check if free cluster is available.
            int x = free_cluster_check();
            if (x == FALSE) {
                printf("Error! There's no free cluster. Can't create file '%s'\n",pch);
                exit(6);
            }
            
            // Create new file entry
            ENTRY_T *new_dir = malloc(sizeof(ENTRY_T));
            if (new_dir == NULL)
            {
                printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_T));
                exit(7);
            }
            
            new_dir->entry_type = 0;
            new_dir->creation_time  = fs_gettime();
            new_dir->creation_date = fs_getdate();
            new_dir->name_len = strlen(pch);
            strcpy(new_dir->name,pch);
            new_dir->size = 0;
            
            // Update FAT area
            fat_table[x].entry = x;
            ffwrite(myMBR->cluster_size*myMBR->sector_size, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, fat_table);
            
            // Create new pointer entry
            ENTRY_PTR_T *new_dir_ptr = malloc(sizeof(ENTRY_PTR_T));
            if (new_dir_ptr == NULL)
            {
                printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
                exit(8);
            }
            new_dir_ptr->type = 0;
            new_dir_ptr->reserved = 0;
            new_dir_ptr->start = x; // starts at cluster x.
            
            /* Save information of new file to disk */
            FILE *file = fopen(DISK_NAME, "r+b");
            if (!file) {
                printf("Unable to open file!\n");
                exit(9);
            }
            ENTRY_PTR_T *buffer = malloc(sizeof(ENTRY_PTR_T));
            if (buffer == NULL)
            {
                printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
                exit(10);
            }
            // Find where to save new pointer entry
            fseek(file, (myMBR->data_start+child_start)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
            fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
            int index = 0;
            int saved = FALSE;
            uint16_t i = child_start;
            
            while (buffer->type != 0xFF) {
                // If reaching the last child in cluster
                if (buffer->reserved == 0xFF) { // Last child in cluster
                    if (fat_table[i].entry == i) { // No additional cluster
                        fclose(file);
                        printf("Pointer can't fill in the cluster %d, Finding a new cluster...\n",i);
                        int y = free_cluster_check();
                        if (y == FALSE) {
                            printf("Error! Can't find a free cluster to store the pointer to new file '%s'\n",pch);
                            exit(11);
                        }
                        
                        // Save pointer to the new cluster
                        ffwrite((myMBR->data_start + y)*myMBR->cluster_size*myMBR->sector_size, sizeof(ENTRY_PTR_T),new_dir_ptr);
                        
                        fat_table[i].entry = y;
                        fat_table[y].entry = y;
                        
                        ffwrite(myMBR->cluster_size*myMBR->sector_size, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, fat_table);
                        saved = TRUE;
                        break;
                    } else {
                        index = -1;
                        i = fat_table[i].entry;
                        fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
                    }
                }
                fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
                index++;
            }
            fclose(file);
            
            if (saved != TRUE) {
                if (buffer->reserved == 0xFF)
                    new_dir_ptr->reserved = 0xFF;
                if (i == child_start) {
                    ffwrite((myMBR->data_start + i)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T) + sizeof(ENTRY_PTR_T)*index, sizeof(ENTRY_PTR_T),new_dir_ptr);
                } else {
                    ffwrite((myMBR->data_start + i)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_PTR_T)*index, sizeof(ENTRY_PTR_T),new_dir_ptr);
                }
            }
            
            ffwrite((myMBR->data_start + x)*myMBR->cluster_size*myMBR->sector_size,sizeof(ENTRY_T), new_dir);
            printf("Sucessfully created the file '%s' at cluster %d\n",pch, x);
            int val = x;
            if (op_writing_flist->head == NULL)
                create_list(op_writing_flist, val);
            else
                add_to_list(op_writing_flist, val);
            
            int sum = mode[0]<<16;
            sum = sum+val;
            free(new_dir);
            free(new_dir_ptr);
            free(file_ptr);
            free(buffer);
            free(buffer_path);
            return sum;
            
        } else {
            printf("Error! File doesn't exist\n");
            exit(6);
        }
    }
    
    int val = file_ptr->start;
    LIST_ITEM_T *srch;
    srch = search_in_list(op_writing_flist,val,NULL);
    if(srch != NULL)
    {
        printf("Warning! File is being opened in 'w' mode\n");
        return -1;
    }
    srch = search_in_list(op_reading_flist,val,NULL);
    if(srch != NULL)
    {
        printf("Warning! File is being opened in 'r' mode\n");
        return -1;
    }
    
    if (strcmp(mode, "w") == 0) {
        if (op_writing_flist->head == NULL)
            create_list(op_writing_flist, val);
        else
            add_to_list(op_writing_flist, val);
    } else {
        if (op_reading_flist->head == NULL)
            create_list(op_reading_flist, val);
        else
            add_to_list(op_reading_flist, val);
    }
    
    int sum = mode[0]<<16;
    sum = sum+val;
    
    free(buffer_path);
    free(file_ptr);
    return sum;
}

/*
    FS_CLOSE
*/
// Return TRUE if file is closed succesfully and FALSE if file can't be closed.
int fs_close(int fh) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    if (fh <0) {
        printf("Error! The parameter is not correct! Can't close the file.\n");
        exit(2);
    }
    
    char mode = fh>>16;
    int val = mode<<16;
    val = fh - val;
    if (val <0 || val >= myMBR->data_len || (mode != 'w' && mode != 'r')) {
        printf("Error! The parameter is not correct! Can't close the file.\n");
        exit(3);
    }
    if (mode == 'w') {
        return delete_from_list(op_writing_flist, val);
    } else
        return delete_from_list(op_reading_flist, val);
}

/*
    FS_WRITE
*/
// Return the number of bytes that is written successfully to the file.
int fs_write( const void *buffer, int count, int stream ) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    if (count < 1 || count > strlen(buffer)) {
        printf("Number of bytes to write is not correct! Can't start writing\n");
        exit(2);
    }
    if (buffer == NULL) {
        printf("Buffer is empty! Can't start writing\n");
        exit(3);
    }
    char mode = stream>>16;
    if (mode != 'w') {
        printf("File isn't being opened in 'w' mode, Can't start writing\n");
        exit(4);
    }
    
    int val = mode<<16;
    val = stream - val;
    LIST_ITEM_T *srch = search_in_list(op_writing_flist,val,NULL);
    if(srch == NULL)
    {
        printf("Error! File hasn't been opened in 'w' mode! Can't start writing\n");
        exit(5);
    }

    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(6);
    }
    
    ENTRY_T *file_entry = malloc(sizeof(ENTRY_T));
    if (file_entry == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_T));
        exit(7);
    }
    fseek(file, (myMBR->data_start+val)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
    fread(file_entry, 1, sizeof(ENTRY_T), file);
    fclose(file);

    // Find how many byted used in the last cluster
    uint16_t last_cluster = val;
    int num_cluster = 1;
    while (fat_table[last_cluster].entry != last_cluster) {
        last_cluster = fat_table[last_cluster].entry;
        num_cluster++;
    }
    int lcluster_byte_used = file_entry->size - (num_cluster-1)*myMBR->cluster_size*myMBR->sector_size;
    int num_new_cluster = 0;
    unsigned long buf_len = count;
    int total_write = 0;
    
    while (buf_len > (myMBR->cluster_size*myMBR->sector_size)-lcluster_byte_used) {
        int byte_write; // Number of bytes to write to file
        if (last_cluster == val) {
            byte_write = (myMBR->cluster_size*myMBR->sector_size)-sizeof(ENTRY_T)-lcluster_byte_used;
            ffwrite((myMBR->data_start + last_cluster)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T) + lcluster_byte_used, byte_write, (void *)buffer);
        }else {
            byte_write = (myMBR->cluster_size*myMBR->sector_size)-lcluster_byte_used;
            ffwrite((myMBR->data_start + last_cluster)*myMBR->cluster_size*myMBR->sector_size + lcluster_byte_used, byte_write, (void *)buffer);
        }
        buffer = &buffer[byte_write];
        buf_len = buf_len - byte_write;
         total_write += byte_write;
        
        // Find new cluster
        int y = free_cluster_check();
        if (y == FALSE) {
            printf("Error! Can't find a free cluster to store the buffer '%s'\n",buffer);
            return total_write;
        }
        printf("Buffer can't fill up the cluster %d, It's fill in new cluster %d\n",last_cluster, y);

        fat_table[last_cluster].entry = y;
        fat_table[y].entry = y;
        ffwrite(myMBR->cluster_size*myMBR->sector_size, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, fat_table);
        num_new_cluster++;
        lcluster_byte_used = 0;
        last_cluster = y;
    }// end while
    
    if (last_cluster == val) {
        ffwrite((myMBR->data_start + last_cluster)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T) + lcluster_byte_used, buf_len, (void *)buffer);
    } else
        ffwrite((myMBR->data_start + last_cluster)*myMBR->cluster_size*myMBR->sector_size + lcluster_byte_used, buf_len, (void *)buffer);
    
    total_write += buf_len;
    // Update size of file
    file_entry->size = file_entry->size + total_write;
    ffwrite((myMBR->data_start + val)*myMBR->cluster_size*myMBR->sector_size, sizeof(ENTRY_T), file_entry);
    printf("Writing the buffer to the file successfully!\n\n");
    free(file_entry);
    return total_write;
}

/*
    FS_READ
*/
// Return the number of bytes that is read successfully from the file.
int fs_read(const void *buffer, int count, int stream ) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    if (count < 1) {
        printf("Number of bytes to read <1! Can't start reading\n");
        exit(3);
    }
    char mode = stream>>16;
    if (mode != 'r') {
        printf("File isn't being opened in 'r' mode, Can't start reading\n");
        exit(4);
    }
    
    int val = mode<<16;
    val = stream - val;
    LIST_ITEM_T *srch = search_in_list(op_reading_flist,val,NULL);
    if(srch == NULL)
    {
        printf("Error! File hasn't been opened in 'r' mode! Can't start writing\n");
        exit(5);
    }
    
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(6);
    }
    
    ENTRY_T *file_entry = malloc(sizeof(ENTRY_T));
    if (file_entry == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_T));
        exit(7);
    }
    fseek(file, (myMBR->data_start+val)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
    fread(file_entry, sizeof(ENTRY_T), 1, file);
    int buf_len = count;
    if (count > file_entry->size) {
        printf("The 'number of bytes to read' is larger than the 'size of file'! Will ONLY read all the content in the file\n");
        buf_len = file_entry->size;
    }
    uint16_t cur_cluster = val;
    int byte_read = 0; // Number of bytes to read to file

    while (byte_read < count) {
        if (cur_cluster == val){
            fseek(file, (myMBR->data_start+val)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
            if(buf_len <= (myMBR->cluster_size*myMBR->sector_size - sizeof(ENTRY_T))) {
                fread((void*)buffer, 1, buf_len, file);
                byte_read += buf_len;
                break;
            } else {
                int scluster = myMBR->cluster_size*myMBR->sector_size - sizeof(ENTRY_T);
                fread((void*)buffer, 1, scluster, file);
                byte_read += scluster;
                buf_len -= scluster;
                buffer = &buffer[scluster];
                if (fat_table[cur_cluster].entry != cur_cluster)
                    cur_cluster = fat_table[cur_cluster].entry;
            }
        } else {
            fseek(file, (myMBR->data_start+cur_cluster)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            if(buf_len <= (myMBR->cluster_size*myMBR->sector_size)) {
                fread((void*)buffer, 1, buf_len, file);
                byte_read += buf_len;
                break;
            } else {
                int ccluster = myMBR->cluster_size*myMBR->sector_size;
                fread((void*)buffer, 1, ccluster, file);
                byte_read +=  ccluster;
                buf_len -= ccluster;
                buffer = &buffer[ccluster];
            }
        }

    }
    printf("Reading from file successfully!\n\n");
    fclose(file);
    return byte_read;
}

/* Unallocate all entries except entry at start_cluster from FAT Area */
void rm_fentry(int start_cluster){
    int i = start_cluster;
    while (fat_table[i].entry != i) {
        rm_fentry(fat_table[i].entry);
        fat_table[i].entry = i;
    }
    if (fat_table[i].entry == i)
        fat_table[i].entry = 0xFFFF;
}

/* Unallocate all children of the directory starting at start_cluster */
FILE *rm_dentry(int start_cluster){
     if (start_cluster < 1){
        printf("Error! Can't remove the directory!\n");
        exit(1);
    }
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(2);
    }
    ENTRY_PTR_T *buffer = malloc(sizeof(ENTRY_PTR_T));
    if (buffer == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
        exit(3);
    }
    fseek(file, (myMBR->data_start+start_cluster)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
    fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    uint16_t i = start_cluster;
    
    while (buffer->type != 0xFF) {
        // If buffer points to a file
        if (buffer->type == 0) {
            rm_fentry(buffer->start);
        }
        // If buffer points to a directory
        else if (buffer->type == 1) {
            rm_dentry(buffer->start);
        }
        // If reaching the last child in cluster
        if (buffer->reserved == 0xFF) { // Last child in cluster
            if (fat_table[i].entry == i) { // No additional cluster
                break;
            } else {
                i = fat_table[i].entry;
                fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            }
        }
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    }
    rm_fentry(i);
    free(buffer);
    return file;
}

/* Remove directory */
void rm_dir(char *absolute_path){
    int dir_start = fs_opendir(absolute_path);
    if (dir_start < 0) {
        printf("Error! The absolute path for directory doesn't exist\n");
        exit(1);
    }
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(2);
    }

    ENTRY_PTR_T *buffer = malloc(sizeof(ENTRY_PTR_T));
    if (buffer == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
        exit(3);
    }
    char *pch, *tmp;
    uint16_t child_start = 0;
    int i = child_start;
    
    pch = strtok(absolute_path,"/");
    tmp = strtok(NULL,"/");
    while (tmp != NULL) {
        fseek(file, (myMBR->data_start+child_start)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
        
        while (buffer->type != 0xFF) {
            // If buffer pointers to a directory
            if (buffer->type == 1) {
                ENTRY_T buffer_child;
                fseek(file, (myMBR->data_start+buffer->start)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
                fread(&buffer_child, 1, sizeof(ENTRY_T), file);
                if (strcmp(pch, buffer_child.name) == 0) {
                    child_start = buffer->start;
                    break;
                }
            }
            i = child_start;
            // If reaching the last child in cluster
            if (buffer->reserved == 0xFF) { // Last child in cluster
                    i = fat_table[i].entry;
                    fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            }
            fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
        }
        pch = tmp;
        tmp = strtok(NULL,"/");
    }

    fseek(file, (myMBR->data_start+child_start)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
    long index = ftell(file);
    fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    while (buffer->start != dir_start) {
        index = ftell(file);
        i = child_start;
        if (buffer->reserved == 0xFF) { // Last child in cluster
            if (fat_table[i].entry == i)
                break;
            i = fat_table[i].entry;
            fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
        }
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    }
    fclose(file);
    //If it's the first pointer in cluster.
    if (buffer->reserved == 0xFE) {
        fat_table[child_start].entry = child_start;
        fat_table[i].entry = 0xFFFF;
    } else {
        buffer->type = 0xFE;
        ffwrite((int)index, sizeof(ENTRY_PTR_T), buffer);
    }
    
    // Remove all children of directory.
    FILE *f = rm_dentry(dir_start);
    fclose(f);
    free(buffer);
}

/* Remove file */
void rm_file (char *absolute_path){
    FILE *file = fopen(DISK_NAME, "r+b");
    if (!file) {
        printf("Unable to open file!\n");
        exit(1);
    }
    ENTRY_PTR_T *buffer = malloc(sizeof(ENTRY_PTR_T));
    if (buffer == NULL)
    {
        printf("Failed to allocate %d bytes memory\n", (int)sizeof(ENTRY_PTR_T));
        exit(2);
    }
    char *pch, *tmp;
    uint16_t child_start = 0;
    int i = child_start;
    pch = strtok(absolute_path,"/");
    tmp = strtok(NULL,"/");

    // Find directory containing the file
    while (tmp != NULL) {
        fseek(file, (myMBR->data_start+child_start)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
        long index = ftell(file);
        while (buffer->type != 0xFF) {
            // If buffer pointers to a directory
            if (buffer->type == 1) {
                ENTRY_T buffer_child;
                fseek(file, (myMBR->data_start+buffer->start)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
                fread(&buffer_child, 1, sizeof(ENTRY_T), file);
                if (strcmp(pch, buffer_child.name) == 0) {
                    child_start = buffer->start;
                    break;
                }
            }
            fseek(file, index, SEEK_SET);
            i = child_start;
            // If reaching the last child in cluster
            if (buffer->reserved == 0xFF) { // Last child in cluster
                i = fat_table[i].entry;
                fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            }
            fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
        }
        pch = tmp;
        tmp = strtok(NULL,"/");
    }

    // Find starting cluster of file
    fseek(file, (myMBR->data_start+child_start)*myMBR->cluster_size*myMBR->sector_size + sizeof(ENTRY_T), SEEK_SET);
    long index = ftell(file);
    fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    if (buffer->type == 0xFF) {
        printf("The absolute path doesn't exist\n");
        exit(3);
    }
    while (buffer->type != 0xFF) {
        index = ftell(file);
        i = child_start;
        if (buffer->type == 0) {
            ENTRY_T buffer_child;
            fseek(file, (myMBR->data_start+buffer->start)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
            fread(&buffer_child, 1, sizeof(ENTRY_T), file);
            if (strcmp(pch, buffer_child.name) == 0) {
                child_start = buffer->start;
                break;
            }
        }
        fseek(file, index, SEEK_SET);
        if (buffer->reserved == 0xFF) { // Last child in cluster
            i = fat_table[i].entry;
            fseek(file,(myMBR->data_start+i)*myMBR->cluster_size*myMBR->sector_size, SEEK_SET);
        }
        fread(buffer, 1, sizeof(ENTRY_PTR_T), file);
    }
    fclose(file);
    index -= 4;
    //If pointer to file isn't the first pointer in cluster.
    if (buffer->reserved == 0xFE) {
        fat_table[child_start].entry = child_start;
        fat_table[i].entry = 0xFFFF;
    } else {
        buffer->type = 0xFE;
        ffwrite((int)index, sizeof(ENTRY_PTR_T), buffer);
    }
    free(buffer);
    rm_fentry(child_start);
}

/*
    FS_RM
 */
// Remove a directory or file. Return TRUE if absolute path exists and directory is removed successfully.
// Type: 0-file, 1-directory.
int fs_rm (const char *absolute_path, int type) {
    if (loaded != TRUE) {
        printf("Disk hasn't been loaded! Please load the disk first!\n");
        exit(1);
    }
    if (type != 0 && type != 1) {
        printf("Error! The type is not correct!\n");
        exit(2);
    }
    char *buffer = malloc(strlen(absolute_path));
    if (buffer == 0)
    {
        printf("Failed to allocate %d bytes memory\n", (int)strlen(absolute_path));
        exit(3);
    }
    strcpy(buffer, absolute_path);
    if (check_abs_path(buffer) == FALSE) {
        printf("Error! The absolute path is not in correct format\n");
        exit(4);
    }
    
    if (type == 1) {
        if (strlen(absolute_path) == 1) {
            printf("Error! Can't remove Root directory\n");
            exit(5);
        }
        printf("Removing directory from absolute path '%s'\n",buffer);
        rm_dir(buffer);
    } else {
        if (strlen(absolute_path) == 1) {
            printf("Error! Absolute path doesn't contain any file name\n");
            exit(6);
        }
        
        char *buf = malloc(strlen(absolute_path));
        if (buf == 0)
        {
            printf("Failed to allocate %d bytes memory\n", (int)strlen(absolute_path));
            exit(7);
        }
        strcpy(buf, absolute_path);
        // Check if file exists
        int ofile = fs_open(buf, "r");
        fs_close(ofile);
        free(buf);
        
        printf("Removing file from absolute path '%s'\n",buffer);
        rm_file(buffer);
    }
    ffwrite(myMBR->cluster_size*myMBR->sector_size, myMBR->fat_len*myMBR->cluster_size*myMBR->sector_size, fat_table);
    free(buffer);
    printf("Removed successfully!\n\n");
    return TRUE;
}

int main(int argc, const char * argv[]) {
    // Initialize lists for opening and reading files.
    op_writing_flist = malloc(sizeof(LINKED_LIST_T));
    op_writing_flist->head = NULL;
    op_writing_flist->tail = NULL;
    op_reading_flist = malloc(sizeof(LINKED_LIST_T));
    op_reading_flist->head = NULL;
    op_reading_flist->tail = NULL;
    
    /* FORMAT */
    format(64, 2, 1024);
    
    /* LOAD DISK */
    //load_disk(DISK_NAME);
    
    
    /* Create directories */
    char dir[20];
    for (int i =1; i<28; i++){
        sprintf(dir, "Folder %d", i); // puts string into buffer
        fs_mkdir(fs_opendir("/"), dir); // create a new directory in ROOT directory.
    }

    /* Create files */
    /*
    int file1 = fs_open("/file1", "w");
    //fs_close(file1);
    */
    
    /* Write files */
    /*
    char *buffer = "This is a content";
    fs_write(buffer, (int)strlen(buffer), file1);

    fs_close(file1);
    */
    
    /* Read files */
    /*
    file1 = fs_open("/file1", "r");
    char *buf = malloc(sizeof(char));
    if (buf == NULL)
    {
        printf("Failed to allocate %lu bytes memory\n", sizeof(char));
        exit(1);
    }
    int read = fs_read(buf, 200, file1);
    printf("%s\n", buf);
    fs_close(file1);
    */
    
    
    /* Remove files or directories */ //0-file, 1-directory.
    //fs_rm("/Folder 2", 1);
    //fs_rm("/file1", 0);

    
    /* Test fs_ls */
    /*
    ENTRY_T *child = fs_ls(fs_opendir("/"), 0); // 0 = first child
    if  (child == NULL)
        printf("NULL\n");
    else printf("[Type:%d]  [Name:%s] [Name length:%d]  [Size:%d]  [Date: %d] [Time: %d]\n\n", child->entry_type, child->name,child->name_len, child->size, child->creation_date, child->creation_time);
    */
    
    /*
     printf("sector: %d\n", myMBR->sector_size);
     printf("cluster: %d\n", myMBR->cluster_size);
     printf("disk: %d\n", myMBR->disk_size);
     printf("fat start: %d\n", myMBR->fat_start);
     printf("fat len: %d\n", myMBR->fat_len);
     printf("data start: %d\n", myMBR->data_start);
     printf("data len: %d\n", myMBR->data_len);
     printf("name: %s\n", myMBR->disk_name);
     for (int i=0; i<myMBR->data_len; i++)
     printf("Fat[%d] = %d\n",i,fat_table[i].entry);
     */
    
    free(myMBR);
    free(fat_table);
    free(data);
    return 0;
}
