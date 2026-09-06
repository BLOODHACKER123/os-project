#ifndef FS_H
#define FS_H

#include <stdint.h>

#define RAMDISK_SIZE        (1024 * 1024)
#define FS_BLOCK_SIZE       512
#define FS_MAX_BLOCKS       (RAMDISK_SIZE / FS_BLOCK_SIZE)

#define FS_MAX_INODES       64
#define FS_MAX_FILENAME     32

#define FS_MAGIC            0x53454E47

typedef enum {
    INODE_FREE = 0,
    INODE_FILE = 1
} inode_type_t;

/*
 * Superblock
 *
 * Stores basic filesystem information.
 */
typedef struct {
    uint32_t magic;

    uint32_t total_blocks;
    uint32_t free_blocks;

    uint32_t total_inodes;
    uint32_t free_inodes;

    uint32_t inode_table_block;
    uint32_t data_start_block;
} fs_superblock_t;


/*
 * Inode
 *
 * Represents one file.
 */
typedef struct {
    uint32_t used;
    inode_type_t type;

    uint32_t size;

    /*
     * For the first version of the filesystem,
     * each file can use up to 8 direct blocks.
     */
    uint32_t direct_blocks[8];

    char name[FS_MAX_FILENAME];
} fs_inode_t;



void fs_init(void);

int fs_create(const char *name);

int fs_write(const char *name,
             const char *data);

int fs_read(const char *name,
            char *buffer,
            uint32_t buffer_size);

int fs_delete(const char *name);

fs_inode_t *fs_find(const char *name);

fs_superblock_t *fs_get_superblock(void);

fs_inode_t *fs_get_inode(uint32_t index);

#endif