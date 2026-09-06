#include "fs.h"


static uint8_t ramdisk[RAMDISK_SIZE];
static fs_superblock_t superblock;
static fs_inode_t inode_table[FS_MAX_INODES];
static uint8_t block_bitmap[FS_MAX_BLOCKS / 8];

/* Forward declarations */
static int fs_alloc_block(void);
static void fs_free_block(uint32_t block);



static int fs_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}




static void fs_copy_name(char *dest, const char *src) {
    uint32_t i = 0;

    while (src[i] != '\0' && i < FS_MAX_FILENAME - 1) {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}



static uint32_t fs_strlen(const char *str) {
    uint32_t length = 0;

    if (str == 0) {
        return 0;
    }

    while (str[length] != '\0') {
        length++;
    }

    return length;
}



static void block_set_used(uint32_t block) {
    uint32_t byte_index = block / 8;
    uint32_t bit_index = block % 8;

    block_bitmap[byte_index] |=
        (uint8_t)(1u << bit_index);
}


static int block_is_used(uint32_t block) {
    uint32_t byte_index = block / 8;
    uint32_t bit_index = block % 8;

    return (block_bitmap[byte_index] &
            (uint8_t)(1u << bit_index)) != 0;
}


static void block_set_free(uint32_t block) {
    uint32_t byte_index = block / 8;
    uint32_t bit_index = block % 8;

    block_bitmap[byte_index] &=
        (uint8_t)~(1u << bit_index);
}



static void fs_clear(void) {
    uint32_t i;

    for (i = 0; i < RAMDISK_SIZE; i++) {
        ramdisk[i] = 0;
    }

    for (i = 0; i < (FS_MAX_BLOCKS / 8); i++) {
        block_bitmap[i] = 0;
    }

    for (i = 0; i < FS_MAX_INODES; i++) {
        inode_table[i].used = 0;
        inode_table[i].type = INODE_FREE;
        inode_table[i].size = 0;
        inode_table[i].name[0] = '\0';

        for (uint32_t j = 0; j < 8; j++) {
            inode_table[i].direct_blocks[j] = 0;
        }
    }
}



    void fs_init(void) {
    fs_clear();

    
    superblock.magic = FS_MAGIC;

    superblock.total_blocks = FS_MAX_BLOCKS;
    superblock.total_inodes = FS_MAX_INODES;

    superblock.inode_table_block = 2;
    superblock.data_start_block = 10;

       for (uint32_t block = 0;
         block < superblock.data_start_block;
         block++) {

        block_set_used(block);
    }

    superblock.free_blocks =
        FS_MAX_BLOCKS - superblock.data_start_block;

    superblock.free_inodes =
        FS_MAX_INODES;
}


fs_inode_t *fs_find(const char *name) {
    if (name == 0 || *name == '\0') {
        return 0;
    }

    for (uint32_t i = 0; i < FS_MAX_INODES; i++) {

        if (!inode_table[i].used) {
            continue;
        }

        if (fs_strcmp(inode_table[i].name, name) == 0) {
            return &inode_table[i];
        }
    }

    return 0;
}



int fs_create(const char *name) {
    if (name == 0 || *name == '\0') {
        return -1;
    }


    if (fs_find(name) != 0) {
        return -2;
    }

    
    for (uint32_t i = 0; i < FS_MAX_INODES; i++) {

        if (inode_table[i].used) {
            continue;
        }

        inode_table[i].used = 1;
        inode_table[i].type = INODE_FILE;
        inode_table[i].size = 0;

        fs_copy_name(inode_table[i].name, name);

        for (uint32_t j = 0; j < 8; j++) {
            inode_table[i].direct_blocks[j] = 0;
        }

        if (superblock.free_inodes > 0) {
            superblock.free_inodes--;
        }

        return 0;
    }

    return -3;
}


/*
 * Returns:
 *   0  = success
 *  -1  = invalid arguments
 *  -2  = file not found
 *  -3  = file too large
 *  -4  = not enough free blocks
 */

int fs_write(const char *name, const char *data) {
    if (name == 0 || data == 0) {
        return -1;
    }

    fs_inode_t *inode = fs_find(name);

    if (inode == 0) {
        return -2;
    }

    uint32_t length = fs_strlen(data);

   
    if (length > (8 * FS_BLOCK_SIZE)) {
        return -3;
    }

    
    for (uint32_t i = 0; i < 8; i++) {
        if (inode->direct_blocks[i] != 0) {
            fs_free_block(inode->direct_blocks[i]);
            inode->direct_blocks[i] = 0;
        }
    }

    inode->size = 0;

   
    if (length == 0) { /* Writing an empty string requires no data blocks.*/
        return 0;
    }

    uint32_t blocks_needed =
        (length + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;


    if (blocks_needed > superblock.free_blocks) {/* Check space before starting the write.*/
        return -4;
    }

    uint32_t bytes_written = 0;

    for (uint32_t i = 0; i < blocks_needed; i++) {
        int block = fs_alloc_block();

        if (block < 0) {
            /*
             * Roll back blocks allocated during this write.
             */
            for (uint32_t j = 0; j < i; j++) {
                if (inode->direct_blocks[j] != 0) {
                    fs_free_block(inode->direct_blocks[j]);
                    inode->direct_blocks[j] = 0;
                }
            }

            inode->size = 0;
            return -4;
        }

        inode->direct_blocks[i] = (uint32_t)block;

        uint32_t offset =
            (uint32_t)block * FS_BLOCK_SIZE;

        for (uint32_t j = 0;
             j < FS_BLOCK_SIZE && bytes_written < length;
             j++) {

            ramdisk[offset + j] = (uint8_t)data[bytes_written];
            bytes_written++;
        }
    }

    inode->size = length;

    return 0;
}


    int fs_read(const char *name,
            char *buffer,
            uint32_t buffer_size) {

    if (name == 0 || buffer == 0 || buffer_size == 0) {
        return -1;
    }

    fs_inode_t *inode = fs_find(name);

    if (inode == 0) {
        return -2;
    }

    uint32_t bytes_to_read = inode->size;

    if (bytes_to_read >= buffer_size) {
        bytes_to_read = buffer_size - 1;
    }

    uint32_t bytes_read = 0;

    for (uint32_t i = 0;
         i < 8 && bytes_read < bytes_to_read;
         i++) {

        uint32_t block = inode->direct_blocks[i];

        if (block == 0) {
            break;
        }

        uint32_t offset = block * FS_BLOCK_SIZE;

        for (uint32_t j = 0;
             j < FS_BLOCK_SIZE &&
             bytes_read < bytes_to_read;
             j++) {

            buffer[bytes_read] =
                (char)ramdisk[offset + j];

            bytes_read++;
        }
    }

    buffer[bytes_read] = '\0';

    return (int)bytes_read;
}


    /*
 * Delete a file from the RAM disk.
 *
 * This:
 *   1. Finds the inode
 *   2. Frees all data blocks
 *   3. Clears the inode
 *   4. Returns the inode to the free pool
 *
 * Returns:
 *   0  = success
 *  -1  = invalid filename
 *  -2  = file not found
 */
int fs_delete(const char *name) {
    if (name == 0 || *name == '\0') {
        return -1;
    }

    fs_inode_t *inode = fs_find(name);

    if (inode == 0) {
        return -2;
    }

    /*
     * Release all data blocks owned by this file.
     */
    for (uint32_t i = 0; i < 8; i++) {
        if (inode->direct_blocks[i] != 0) {
            fs_free_block(inode->direct_blocks[i]);
            inode->direct_blocks[i] = 0;
        }
    }

    /* Clear inode metadata.*/

    inode->used = 0;
    inode->type = INODE_FREE;
    inode->size = 0;
    inode->name[0] = '\0';

    superblock.free_inodes++;

    return 0;
}


   
    static int fs_alloc_block(void) {
    for (uint32_t block = superblock.data_start_block;
         block < superblock.total_blocks;
         block++) {

        if (!block_is_used(block)) {
            block_set_used(block);

            if (superblock.free_blocks > 0) {
                superblock.free_blocks--;
            }

            
            uint32_t offset = block * FS_BLOCK_SIZE;

            for (uint32_t i = 0; i < FS_BLOCK_SIZE; i++) {
                ramdisk[offset + i] = 0;
            }

            return (int)block;
        }
    }

    return -1;
}


static void fs_free_block(uint32_t block) {
  
    if (block < superblock.data_start_block ||
        block >= superblock.total_blocks) {
        return;
    }

   
    if (!block_is_used(block)) {
        return;
    }

    block_set_free(block);
    superblock.free_blocks++;

   
    uint32_t offset = block * FS_BLOCK_SIZE;

    for (uint32_t i = 0; i < FS_BLOCK_SIZE; i++) {
        ramdisk[offset + i] = 0;
    }
}


    fs_superblock_t *fs_get_superblock(void) {
    return &superblock;
    }


    fs_inode_t *fs_get_inode(uint32_t index) {
    if (index >= FS_MAX_INODES) {
        return 0;
    }

    return &inode_table[index];
    }