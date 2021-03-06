/*****************************************************************************
 * ext4.c                                                                    *
 *                                                                           *
 * This file contains function implementations that can read and interpret an*
 * ext4 file system.                                                         *
 *                                                                           *
 *                                                                           *
 *   Authors: Wolfgang Richter <wolf@cs.cmu.edu>                             *
 *                                                                           *
 *                                                                           *
 *   Copyright 2013 Carnegie Mellon University                               *
 *                                                                           *
 *   Licensed under the Apache License, Version 2.0 (the "License");         *
 *   you may not use this file except in compliance with the License.        *
 *   You may obtain a copy of the License at                                 *
 *                                                                           *
 *       http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                           *
 *   Unless required by applicable law or agreed to in writing, software     *
 *   distributed under the License is distributed on an "AS IS" BASIS,       *
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 *   See the License for the specific language governing permissions and     *
 *   limitations under the License.                                          *
 *****************************************************************************/
#define _FILE_OFFSET_BITS 64

#include "bson.h"
#include "mbr.h"
#include "ext4.h"
#include "util.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h> 
#include <sys/types.h>

/* for s_flags */
#define EXT2_FLAGS_TEST_FILESYS                 0x0004

/* for s_feature_compat */
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL         0x0004

/* for s_feature_ro_compat */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER     0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE       0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR        0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE        0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM         0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK        0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE      0x0040

/* for s_feature_incompat */
#define EXT2_FEATURE_INCOMPAT_FILETYPE          0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER           0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV       0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG           0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS           0x0040 /* extents support */
#define EXT4_FEATURE_INCOMPAT_64BIT             0x0080
#define EXT4_FEATURE_INCOMPAT_MMP               0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG           0x0200

#define EXT2_FEATURE_RO_COMPAT_SUPP (EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
                                       EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
                                       EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_INCOMPAT_SUPP  (EXT2_FEATURE_INCOMPAT_FILETYPE| \
                                       EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED ~EXT2_FEATURE_INCOMPAT_SUPP
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED      ~EXT2_FEATURE_RO_COMPAT_SUPP

#define EXT3_FEATURE_RO_COMPAT_SUPP (EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
                                       EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
                                       EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT3_FEATURE_INCOMPAT_SUPP  (EXT2_FEATURE_INCOMPAT_FILETYPE| \
                                       EXT3_FEATURE_INCOMPAT_RECOVER| \
                                       EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT3_FEATURE_INCOMPAT_UNSUPPORTED ~EXT3_FEATURE_INCOMPAT_SUPP
#define EXT3_FEATURE_RO_COMPAT_UNSUPPORTED      ~EXT3_FEATURE_RO_COMPAT_SUPP



char* ext4_s_creator_os_LUT[] = {
                                "EXT4_OS_LINUX","EXT4_OS_HURD","EXT4_OS_MASIX",
                                "EXT4_OS_FREEBSD","EXT4_OS_LITES"
                           };

char* ext4_s_rev_level_LUT[] = {
                                "EXT4_GOOD_OLD_REV","EXT4_DYNAMIC_REV"
                          };

char* ext4_s_state_LUT[] = {
                                "","EXT4_VALID_FS","EXT4_ERROR_FS","",
                                "EXT4_ORPHAN_FS"
                      };

char* ext4_s_errors_LUT[] = {
                                "","EXT4_ERRORS_CONTINUE","EXT4_ERRORS_RO",
                                "EXT4_ERRORS_PANIC"
                       };

int ext4_print_block(uint8_t* buf, uint32_t block_size)
{
    hexdump(buf, block_size); 
    return 0;
}

uint64_t ext4_s_blocks_count(struct ext4_superblock superblock)
{
    uint32_t s_blocks_count_lo = superblock.s_blocks_count_lo;
    uint32_t s_blocks_count_hi = superblock.s_blocks_count_hi;

    return (((uint64_t) s_blocks_count_hi) << 32) | s_blocks_count_lo;
}

uint64_t ext4_s_r_blocks_count(struct ext4_superblock superblock)
{
    uint32_t s_r_blocks_count_lo = superblock.s_r_blocks_count_lo;
    uint32_t s_r_blocks_count_hi = superblock.s_r_blocks_count_hi;

    return (((uint64_t) s_r_blocks_count_hi) << 32) | s_r_blocks_count_lo;
}

uint64_t ext4_s_free_blocks_count(struct ext4_superblock superblock)
{
    uint32_t s_free_blocks_count_lo = superblock.s_free_blocks_count_lo;
    uint32_t s_free_blocks_count_hi = superblock.s_free_blocks_count_hi;

    return (((uint64_t) s_free_blocks_count_hi) << 32) |
            s_free_blocks_count_lo;
}

int ext4_print_features(struct ext4_superblock* superblock)
{

    if (superblock->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)
        fprintf_yellow(stdout, "\tEXT2_FEATURE_RO_COMPAT_SPARSE_SUPER\n");

    if (superblock->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL)
        fprintf_yellow(stdout, "\tEXT3_FEATURE_COMPAT_HAS_JOURNAL\n");

    if (superblock->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE)
        fprintf_yellow(stdout, "\tEXT2_FEATURE_RO_COMPAT_LARGE_FILE\n");
    if (superblock->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
        fprintf_yellow(stdout, "\tEXT2_FEATURE_RO_COMPAT_BTREE_DIR\n");
    if (superblock->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_HUGE_FILE)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_RO_COMPAT_HUGE_FILE\n");
    if (superblock->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_RO_COMPAT_GDT_CSUM\n");
    if (superblock->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_DIR_NLINK)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_RO_COMPAT_DIR_NLINK\n");
    if (superblock->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE\n");

    if (superblock->s_feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE)
        fprintf_yellow(stdout, "\tEXT2_FEATURE_INCOMPAT_FILETYPE\n");

    if (superblock->s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER)
        fprintf_yellow(stdout, "\tEXT3_FEATURE_INCOMPAT_RECOVER\n");
    if (superblock->s_feature_incompat & EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)
        fprintf_yellow(stdout, "\tEXT3_FEATURE_INCOMPAT_JOURNAL_DEV\n");
    if (superblock->s_feature_incompat & EXT2_FEATURE_INCOMPAT_META_BG)
        fprintf_yellow(stdout, "\tEXT2_FEATURE_INCOMPAT_META_BG\n");
    if (superblock->s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_INCOMPAT_EXTENTS\n");
    if (superblock->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_INCOMPAT_64BIT\n");
    if (superblock->s_feature_incompat & EXT4_FEATURE_INCOMPAT_MMP)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_INCOMPAT_MMP\n");
    if (superblock->s_feature_incompat & EXT4_FEATURE_INCOMPAT_FLEX_BG)
        fprintf_yellow(stdout, "\tEXT4_FEATURE_INCOMPAT_FLEX_BG\n");
    return EXIT_SUCCESS;
}

int ext4_print_superblock(struct ext4_superblock superblock)
{
    fprintf_yellow(stdout, "s_inodes_count: %"PRIu32"\n",
                           superblock.s_inodes_count);
    fprintf_yellow(stdout, "s_blocks_count: %"PRIu64"\n",
                           ext4_s_blocks_count(superblock));
    fprintf_yellow(stdout, "s_r_blocks_count: %"PRIu32"\n",
                           ext4_s_r_blocks_count(superblock));
    fprintf_yellow(stdout, "s_free_blocks_count: %"PRIu32"\n",
                           ext4_s_free_blocks_count(superblock));
    fprintf_yellow(stdout, "s_free_inodes_count: %"PRIu32"\n",
                           superblock.s_free_inodes_count);
    fprintf_yellow(stdout, "s_first_data_block: %"PRIu32"\n",
                           superblock.s_first_data_block);
    fprintf_yellow(stdout, "s_log_block_size: %"PRIu32"\n",
                           superblock.s_log_block_size);
    fprintf_yellow(stdout, "s_log_cluster_size: %"PRIu32"\n",
                           superblock.s_log_cluster_size);
    fprintf_yellow(stdout, "s_blocks_per_group: %"PRIu32"\n",
                           superblock.s_blocks_per_group);
    fprintf_yellow(stdout, "s_clusters_per_group: %"PRIu32"\n",
                           superblock.s_clusters_per_group);
    fprintf_yellow(stdout, "s_inodes_per_group: %"PRIu32"\n",
                           superblock.s_inodes_per_group);
    fprintf_yellow(stdout, "s_mtime: %"PRIu32"\n",
                           superblock.s_mtime);
    fprintf_yellow(stdout, "s_wtime: %"PRIu32"\n",
                           superblock.s_wtime);
    fprintf_yellow(stdout, "s_mnt_count: %"PRIu16"\n",
                           superblock.s_mnt_count);
    fprintf_yellow(stdout, "s_max_mnt_count: %"PRIu16"\n",
                           superblock.s_max_mnt_count);
    fprintf_yellow(stdout, "s_magic: %"PRIx16"\n",
                           superblock.s_magic);
    if (superblock.s_magic == 0xef53)
    {
        fprintf_light_green(stdout, "Magic value matches EXT4_SUPER_MAGIC\n"); 
    }
    else
    {
        fprintf_light_red(stdout,
                          "Magic value does not match EXT4_SUPER_MAGIC\n");
    }
    fprintf_yellow(stdout, "s_state: %"PRIu16"\n",
                           superblock.s_state);
    fprintf_light_yellow(stdout, "File System State: %s\n",
                                 ext4_s_state_LUT[superblock.s_state]);
    fprintf_yellow(stdout, "s_errors: %"PRIu16"\n",
                           superblock.s_errors);
    fprintf_light_yellow(stdout, "Error State: %s\n",
                                 ext4_s_errors_LUT[superblock.s_errors]);
    fprintf_yellow(stdout, "s_minor_rev_level: %"PRIu16"\n",
                           superblock.s_minor_rev_level);
    fprintf_yellow(stdout, "s_lastcheck: %"PRIu32"\n",
                           superblock.s_lastcheck);
    fprintf_yellow(stdout, "s_checkinterval: %"PRIu32"\n",
                           superblock.s_checkinterval);
    fprintf_yellow(stdout, "s_creator_os: %"PRIu32"\n",
                           superblock.s_creator_os);
    fprintf_light_yellow(stdout, "Resolved OS: %s\n",
                               ext4_s_creator_os_LUT[superblock.s_creator_os]);
    fprintf_yellow(stdout, "s_rev_level: %"PRIu32"\n",
                           superblock.s_rev_level);
    fprintf_light_yellow(stdout, "Revision Level: %s\n",
                                 ext4_s_rev_level_LUT[superblock.s_rev_level]);
    fprintf_yellow(stdout, "s_def_resuid: %"PRIu16"\n",
                           superblock.s_def_resuid);
    fprintf_yellow(stdout, "s_def_resgid: %"PRIu16"\n",
                           superblock.s_def_resgid);
    fprintf_yellow(stdout, "s_first_ino: %"PRIu32"\n",
                           superblock.s_first_ino);
    fprintf_yellow(stdout, "s_inode_size: %"PRIu16"\n",
                           superblock.s_inode_size);
    fprintf_yellow(stdout, "s_block_group_nr: %"PRIu16"\n",
                           superblock.s_block_group_nr);
    fprintf_yellow(stdout, "s_feature_compat: %"PRIu32"\n",
                          superblock.s_feature_compat);
    fprintf_yellow(stdout, "s_feature_incompat: %"PRIu32"\n",
                           superblock.s_feature_incompat);
    fprintf_yellow(stdout, "s_feature_ro_compat: %"PRIu32"\n",
                           superblock.s_feature_ro_compat);
    //uint8_t s_uuid[16];
    //uint8_t s_volume_name[16];
    fprintf_light_yellow(stdout, "Last mounted as: '%s'\n",
                                superblock.s_last_mounted);
    //uint8_t s_last_mounted[64];
    fprintf_yellow(stdout, "s_algorithm_usage_bitmap: %"PRIu32"\n",
                           superblock.s_algorithm_usage_bitmap);
    fprintf_yellow(stdout, "s_prealloc_blocks: %"PRIu8"\n",
    superblock.s_prealloc_blocks);                       
    fprintf_yellow(stdout, "s_prealloc_blocks: %"PRIu8"\n",
                           superblock.s_prealloc_blocks);
    //uint8_t alignment[2];
    //uint8_t s_journal_uuid[16];
    fprintf_yellow(stdout, "s_journal_inum: %"PRIu32"\n",
                           superblock.s_journal_inum);
    fprintf_yellow(stdout, "s_journal_dev: %"PRIu32"\n",
                           superblock.s_journal_dev);
    fprintf_yellow(stdout, "s_last_orphan: %"PRIu32"\n",
                           superblock.s_last_orphan);
    //uint32_t s_hash_seed[4];
    fprintf_yellow(stdout, "s_def_hash_version: %"PRIu8"\n",
                           superblock.s_def_hash_version);  
    //uint8_t padding[3];
    fprintf_yellow(stdout, "s_default_mount_options: %"PRIu32"\n",
                           superblock.s_default_mount_opts);
    fprintf_yellow(stdout, "s_first_meta_bg: %"PRIu32"\n",
                           superblock.s_first_meta_bg);
    fprintf_yellow(stdout, "s_mkfs_time: %"PRIu32"\n", superblock.s_mkfs_time);
    fprintf_yellow(stdout, "s_log_groups_per_flex: %"PRIu8"\n",
                            superblock.s_log_groups_per_flex);
    return 0;
}

uint64_t ext4_block_size(struct ext4_superblock superblock)
{
    return ((uint64_t) 1024) << superblock.s_log_block_size;
}

uint64_t ext4_num_block_groups(struct ext4_superblock superblock)
{
    uint64_t blocks = ext4_s_blocks_count(superblock);
    uint64_t blocks_per_group = superblock.s_blocks_per_group;

    return (blocks + blocks_per_group - 1) / blocks_per_group;
}

uint32_t ext4_next_block_group_descriptor(FILE* disk,
                                     int64_t partition_offset,
                                     struct ext4_superblock superblock,
                                     struct ext4_block_group_descriptor* bgd,
                                     uint32_t* sector,
                                     uint32_t* s_offset,
                                     uint8_t* bcache)
{
    static uint32_t i = 0;
    uint64_t offset = (superblock.s_first_data_block+1) *
                      ext4_block_size(superblock);
    uint64_t num_block_groups = ext4_num_block_groups(superblock);

    for (; i < num_block_groups;)
    {
        if (bcache)
        {
            *bgd = *((struct ext4_block_group_descriptor*)
                    &(bcache[i * sizeof(struct ext4_block_group_descriptor)]));
        }
        else
        {
            if (fseeko(disk, partition_offset + offset +
                             (i) *
                             sizeof(struct ext4_block_group_descriptor), 0))
            {
                fprintf_light_red(stderr, "error seeking to position 0x%lx.\n",
                                  offset);
                return 0;
            }

            if (fread(bgd, 1, sizeof(*bgd), disk) != sizeof(*bgd))
            {
                fprintf_light_red(stderr, "error while trying to read ext4 "
                                          "block group descriptor.\n");
                return 0;
            }
        }

        *sector = (partition_offset + offset +
                   i*sizeof(struct ext4_block_group_descriptor)) / SECTOR_SIZE;
        *sector /= (ext4_block_size(superblock) / SECTOR_SIZE);
        *sector *= (ext4_block_size(superblock) / SECTOR_SIZE);
        *s_offset = (partition_offset + offset +
                   i*sizeof(struct ext4_block_group_descriptor)) %
                            ext4_block_size(superblock);
        i++;
        return *sector;
    }

    return 0; 
}

int ext4_print_dir_entry(uint32_t entry, struct ext4_dir_entry dir)
{
    fprintf_yellow(stdout, "%d ext4_dir_entry.inode: %"PRIu32"\n", entry,
                           dir.inode);
    fprintf_yellow(stdout, "%d ext4_dir_entry.rec_len: %"PRIu16"\n", entry,
                           dir.rec_len);
    fprintf_yellow(stdout, "%d ext4_dir_entry.name_len: %"PRIu8"\n", entry,
                           dir.name_len);
    if (dir.name_len < 256)
        dir.name[dir.name_len] = '\0';
    else
        dir.name[0] = '\0';
    fprintf_yellow(stdout, "%d ext4_dir_entry.name: %s\n", entry, dir.name);
    fprintf(stdout, "\n\n");
    return 0;
} 

int ext4_print_dir_entries(uint8_t* bytes, uint32_t len)
{
    uint32_t i;
    uint32_t num_entries = len / sizeof(struct ext4_dir_entry);

    for (i = 0; i < num_entries; i++)
        ext4_print_dir_entry(i, *((struct ext4_dir_entry*)
                                  (bytes + i*sizeof(struct ext4_dir_entry))));
    return 0;
}

uint64_t ext4_block_offset(uint64_t block_num,
                           struct ext4_superblock superblock)
{
    uint64_t block_size = ext4_block_size(superblock);
    return block_size * block_num;
}

int ext4_read_block(FILE* disk, int64_t partition_offset, 
                    struct ext4_superblock superblock, uint64_t block_num, 
                    uint8_t* buf)
{
    uint64_t block_size = ext4_block_size(superblock);
    uint64_t offset = ext4_block_offset(block_num, superblock);
    offset += partition_offset;

    if (fseeko(disk, offset, 0))
    {
        fprintf_light_red(stderr, "Error while reading block seeking to "
                                  "position 0x%lx.\n", offset);
        return -1;
    }

    if (fread(buf, 1, block_size, disk) != block_size)
    {
        fprintf_light_red(stderr, "Error while trying to read block.\n");
        return -1;
    }

    return 0;
}

uint32_t ext4_bgd_free_blocks_count(struct ext4_block_group_descriptor bgd)
{
    uint16_t bg_free_blocks_count_lo = bgd.bg_free_blocks_count_lo;
    uint16_t bg_free_blocks_count_hi = 0;
    return ((uint32_t) bg_free_blocks_count_hi << 16) |
            bg_free_blocks_count_lo;
}

uint32_t ext4_bgd_free_inodes_count(struct ext4_block_group_descriptor bgd)
{
    uint16_t bg_free_inodes_count_lo = bgd.bg_free_inodes_count_lo;
    uint16_t bg_free_inodes_count_hi = 0;
    return ((uint32_t) bg_free_inodes_count_hi << 16) |
            bg_free_inodes_count_lo;
}

uint32_t ext4_bgd_used_dirs_count(struct ext4_block_group_descriptor bgd)
{
    uint16_t bg_used_dirs_count_lo = bgd.bg_used_dirs_count_lo;
    uint16_t bg_used_dirs_count_hi = 0;
    return ((uint32_t) bg_used_dirs_count_hi << 16) | bg_used_dirs_count_lo;
}

uint64_t ext4_bgd_block_bitmap(struct ext4_block_group_descriptor bgd)
{
    uint32_t bg_block_bitmap_lo = bgd.bg_block_bitmap_lo;
    uint32_t bg_block_bitmap_hi = 0;
    return ((uint64_t) bg_block_bitmap_hi << 32) | bg_block_bitmap_lo;
}

uint64_t ext4_bgd_inode_bitmap(struct ext4_block_group_descriptor bgd)
{
    uint32_t bg_inode_bitmap_lo = bgd.bg_inode_bitmap_lo;
    uint32_t bg_inode_bitmap_hi = 0;
    return ((uint64_t) bg_inode_bitmap_hi << 32) | bg_inode_bitmap_lo;
}

uint64_t ext4_bgd_inode_table(struct ext4_block_group_descriptor bgd)
{
    uint32_t bg_inode_table_lo = bgd.bg_inode_table_lo;
    uint32_t bg_inode_table_hi = 0;
    return ((uint64_t) bg_inode_table_hi << 32) | bg_inode_table_lo;
}

int ext4_read_bgd(FILE* disk, int64_t partition_offset,
                 struct ext4_superblock superblock,
                 uint32_t block_group,
                 struct ext4_block_group_descriptor* bgd,
                 uint8_t* bcache)
{
    uint64_t offset = (superblock.s_first_data_block+1) *
                      ext4_block_size(superblock) +
                      block_group*sizeof(struct ext4_block_group_descriptor);

    if (bcache)
    {
        *bgd = *((struct ext4_block_group_descriptor*)
          &(bcache[block_group * sizeof(struct ext4_block_group_descriptor)]));
        return 0;
    }

    if (fseeko(disk, partition_offset + offset, 0))
    {
        fprintf_light_red(stderr, "Error seeking to position 0x%lx.\n",
                          offset);
        return -1;
    }

    if (fread(bgd, 1, sizeof(struct ext4_block_group_descriptor), disk) !=
        sizeof(struct ext4_block_group_descriptor))
    {
        fprintf_light_red(stderr, 
                          "Error while trying to read ext4 Block Group "
                          "Descriptor.\n");
        return -1;
    }

    return 0;
}

int ext4_read_inode_serialized(FILE* disk, int64_t partition_offset,
                               struct ext4_superblock superblock,
                               uint32_t inode_num, struct ext4_inode* inode,
                               struct bson_info* bson,
                               uint8_t* icache, uint8_t* bcache)
{
    uint64_t block_group = (inode_num - 1) / superblock.s_inodes_per_group;
    struct ext4_block_group_descriptor bgd;
    uint64_t inode_table_offset, icache_offset;
    uint64_t inode_offset = (inode_num - 1) % superblock.s_inodes_per_group;
    inode_offset *= superblock.s_inode_size;
    struct bson_kv val;
    uint64_t sector, offset;
    uint64_t sectors_per_block = ext4_block_size(superblock) / SECTOR_SIZE;

    if (ext4_read_bgd(disk, partition_offset, superblock,
                      block_group, &bgd, bcache))
    {
        fprintf(stderr, "Error retrieving block group descriptor %"PRIu64".\n",
                                                                  block_group);
        return -1;
    }

    inode_table_offset = ext4_block_offset(ext4_bgd_inode_table(bgd),
                                           superblock);

    icache_offset = block_group * (superblock.s_inode_size *
                                   superblock.s_inodes_per_group);
    icache_offset += inode_offset;

    *inode = *((struct ext4_inode*) &(icache[icache_offset]));

    val.type = BSON_STRING;
    val.size = strlen("file");
    val.key = "type";
    val.data = "file";

    bson_serialize(bson, &val);
    
    sector = (partition_offset + inode_table_offset + inode_offset) /
             SECTOR_SIZE;
    sector /= sectors_per_block;
    sector *= sectors_per_block;
    val.type = BSON_INT64;
    val.key = "inode_sector";
    val.data = &sector;

    bson_serialize(bson, &val);

    offset = (partition_offset + inode_table_offset + inode_offset) %
             ext4_block_size(superblock);
    val.type = BSON_INT64;
    val.key = "inode_offset";
    val.data = &offset;

    bson_serialize(bson, &val);

    val.type = BSON_INT32;
    val.key = "inode_num";
    val.data = &inode_num;

    bson_serialize(bson, &val);

    return 0;
}

int64_t ext4_sector_from_block(uint64_t block, struct ext4_superblock super,
                               int64_t partition_offset)
{
    return (block * ext4_block_size(super) + partition_offset) / SECTOR_SIZE;
}

uint64_t ext4_extent_start(struct ext4_extent extent)
{
    uint64_t start = (uint64_t) extent.ee_start_hi << 48;
    return start | extent.ee_start_lo;
}

uint64_t ext4_extent_index_leaf(struct ext4_extent_idx idx)
{
    uint64_t leaf = (uint64_t) idx.ei_leaf_hi << 48;
    return leaf | idx.ei_leaf_lo;
}

uint64_t ext4_sector_extent_block(FILE* disk, int64_t partition_offset,
                           struct ext4_superblock superblock,
                           uint32_t block_num, struct ext4_inode inode)
{
    int i;
    struct ext4_extent_header* hdr; 
    struct ext4_extent_idx idx = {};
    struct ext4_extent_idx idx2 = {}; /* lookahead when searching for block_num */
    struct ext4_extent* extent;
    uint8_t buf[ext4_block_size(superblock)];

    memcpy(buf, inode.i_block, (size_t) 60);
    hdr = (struct ext4_extent_header*) buf;
    idx.ei_block = (uint32_t) 2 << 31;

    for (i = 0; i < hdr->eh_entries; i++)
    {
        if (hdr->eh_depth)
        {
            /* TODO */
            idx2 =  * ((struct ext4_extent_idx*)
                            &(buf[sizeof(struct ext4_extent_header) +
                                  sizeof(struct ext4_extent_idx)*i])); 
            if (hdr->eh_entries == 1)
            {
                ext4_read_block(disk, partition_offset, superblock,
                                ext4_extent_index_leaf(idx2), buf);
                i = -1; /* allow loop-expr to run (++) */
                hdr = (struct ext4_extent_header*) buf;
                idx.ei_block = (uint32_t) 2 << 31;
                continue;
            }

            if ((block_num < idx2.ei_block &&
                block_num >= idx.ei_block))
            {
                ext4_read_block(disk, partition_offset, superblock,
                                ext4_extent_index_leaf(idx), buf);
                i = -1; /* allow loop-expr to run (++) */
                hdr = (struct ext4_extent_header*) buf;
                idx.ei_block = (uint32_t) 2 << 31;
                continue;
            }
            idx = idx2;
        }
        else
        {
            extent = (struct ext4_extent*)
                            &(buf[sizeof(struct ext4_extent_header) +
                                  sizeof(struct ext4_extent)*i]); 
            if (extent->ee_block <= block_num &&
                block_num < extent->ee_block + extent->ee_len)
            {
                block_num -= extent->ee_block; /* rebase */
                return ext4_sector_from_block(ext4_extent_start(*extent)
                                              + block_num,
                                              superblock,
                                              partition_offset);
            }
        }
    }

    return 0; 
}

int ext4_read_extent_block(FILE* disk, int64_t partition_offset,
                           struct ext4_superblock superblock,
                           uint32_t block_num, struct ext4_inode inode,
                           uint8_t* buf)
{
    int i;
    struct ext4_extent_header hdr; 
    struct ext4_extent_idx idx = {};
    struct ext4_extent_idx idx2 = {}; /* lookahead when searching block_num */
    struct ext4_extent extent;

    memcpy(buf, inode.i_block, (size_t) 60);
    hdr = *((struct ext4_extent_header*) buf);
    idx.ei_block = (uint32_t) 2 << 31;

    for (i = 0; i < hdr.eh_entries; i++)
    {
        if (hdr.eh_depth)
        {
            /* TODO */
            idx2 =  * ((struct ext4_extent_idx*)
                            &(buf[sizeof(struct ext4_extent_header) +
                                  sizeof(struct ext4_extent_idx)*i])); 
            if (hdr.eh_entries == 1)
            {
                ext4_read_block(disk, partition_offset, superblock,
                                ext4_extent_index_leaf(idx2), buf);
                i = -1; /* allow loop-expr to run (++) */
                hdr = *((struct ext4_extent_header*) buf);
                idx.ei_block = (uint32_t) 2 << 31;
                continue;
            }

            if ((block_num < idx2.ei_block &&
                block_num >= idx.ei_block))
            {
                ext4_read_block(disk, partition_offset, superblock,
                                ext4_extent_index_leaf(idx), buf);
                i = -1; /* allow loop-expr to run (++) */
                hdr = *((struct ext4_extent_header*) buf);
                idx.ei_block = (uint32_t) 2 << 31;
                continue;
            }
            idx = idx2;
        }
        else
        {
            extent = * ((struct ext4_extent*)
                            &(buf[sizeof(struct ext4_extent_header) +
                                  sizeof(struct ext4_extent)*i])); 
            if (extent.ee_block <= block_num &&
                block_num < extent.ee_block + extent.ee_len)
            {
                block_num -= extent.ee_block; /* rebase */
                ext4_read_block(disk, partition_offset, superblock,
                                ext4_extent_start(extent) + block_num,
                                (uint8_t*) buf);
                return 0;
            }
        }
    }

    memset(buf, 0, (size_t) ext4_block_size(superblock)); /* assuming hole */
    return 0; 
}

uint64_t ext4_sector_file_block(FILE* disk, int64_t partition_offset,
                         struct ext4_superblock superblock, uint64_t block_num,
                         struct ext4_inode inode)
{
    fprintf_light_blue(stderr, "in ext4_read_file_block, block_num = %"
                               PRIu64"\n", block_num);
    uint64_t block_size = ext4_block_size(superblock);
    uint64_t addresses_in_block = block_size / 4;
    
    /* ranges for lookup */
    uint64_t direct_low = 0;
    uint64_t direct_high = 11;
    uint64_t indirect_low = direct_high + 1;
    uint64_t indirect_high = direct_high + (addresses_in_block);
    uint64_t double_low = indirect_high + 1;
    uint64_t double_high = indirect_high + (addresses_in_block)*
                                           (addresses_in_block);
    uint64_t triple_low = double_high + 1;
    uint64_t triple_high = double_high + (addresses_in_block)*
                                         (addresses_in_block)*
                                         (addresses_in_block);
    uint8_t buf[block_size];

    if (block_num < direct_low || block_num > triple_high)
    {
        fprintf_light_red(stderr, "File block outside of range of inode.\n");
        return 0;
    }

    /* figure out type of block lookup (direct, indirect, double, treble) */
    /* DIRECT */
    if (block_num <= direct_high)
    {
        if (inode.i_block[block_num] == 0)
            return 0; /* finished */
        return ext4_sector_from_block(inode.i_block[block_num],
                                      superblock,
                                      partition_offset);
    }

    /* INDIRECT */
    if (block_num <= indirect_high)
    {
        block_num -= indirect_low; /* rebase, 0 is beginning indirect block
                                      range */
        
        if (inode.i_block[12] == 0)
            return 0; /* finished */

        ext4_read_block(disk, partition_offset, superblock, inode.i_block[12],
                        (uint8_t*) buf);

        if (buf[block_num] == 0)
            return 0;

        return ext4_sector_from_block(buf[block_num],
                                      superblock,
                                      partition_offset);
    }

    /* DOUBLE */
    if (block_num <= double_high)
    {
        block_num -= double_low;

        if (inode.i_block[13] == 0)
            return 0;

        ext4_read_block(disk, partition_offset, /* double */
                        superblock, inode.i_block[13], (uint8_t*) buf);

        if (buf[block_num / addresses_in_block] == 0)
            return 0;

        ext4_read_block(disk, partition_offset, /* indirect */
                        superblock, buf[block_num / addresses_in_block],
                        (uint8_t*) buf);

        if (buf[block_num % addresses_in_block] == 0)
            return 0;

        return ext4_sector_from_block(buf[block_num % addresses_in_block],
                                      superblock,
                                      partition_offset);
    }

    /* TRIPLE */
    if (block_num <= triple_high)
    {
        block_num -= triple_low;

        if (inode.i_block[14] == 0)
            return 0;

        ext4_read_block(disk, partition_offset, /* triple */
                        superblock, inode.i_block[14], (uint8_t*) buf);

        if (buf[block_num / (addresses_in_block*addresses_in_block)] == 0)
            return 0;
        
        ext4_read_block(disk, partition_offset, /* double */
                        superblock,
                        buf[block_num /
                        (addresses_in_block*addresses_in_block)],
                        (uint8_t*) buf);

        if (buf[block_num / addresses_in_block] == 0)
            return 0;

        ext4_read_block(disk, partition_offset, /* indirect */
                        superblock, buf[block_num / addresses_in_block],
                        (uint8_t*) buf);

        if (buf[block_num % addresses_in_block] == 0)
            return 0;

        return ext4_sector_from_block(buf[block_num % addresses_in_block],
                                      superblock,
                                      partition_offset);
    }

    return 0;
}

int ext4_read_file_block(FILE* disk, int64_t partition_offset,
                         struct ext4_superblock superblock, uint64_t block_num,
                         struct ext4_inode inode, uint32_t* buf)
{
    fprintf_light_blue(stderr, "in ext4_read_file_block, block_num = %"
                               PRIu64"\n", block_num);
    uint64_t block_size = ext4_block_size(superblock);
    uint64_t addresses_in_block = block_size / 4;
    
    /* ranges for lookup */
    uint64_t direct_low = 0;
    uint64_t direct_high = 11;
    uint64_t indirect_low = direct_high + 1;
    uint64_t indirect_high = direct_high + (addresses_in_block);
    uint64_t double_low = indirect_high + 1;
    uint64_t double_high = indirect_high + (addresses_in_block)*
                                           (addresses_in_block);
    uint64_t triple_low = double_high + 1;
    uint64_t triple_high = double_high + (addresses_in_block)*
                                         (addresses_in_block)*
                                         (addresses_in_block);

    if (block_num < direct_low || block_num > triple_high)
    {
        fprintf_light_red(stderr, "File block outside of range of inode.\n");
        return -1;
    }

    /* figure out type of block lookup (direct, indirect, double, treble) */
    /* DIRECT */
    if (block_num <= direct_high)
    {
        if (inode.i_block[block_num] == 0)
            return 1; /* finished */
        ext4_read_block(disk, partition_offset, superblock,
                        inode.i_block[block_num], (uint8_t*) buf);
        return 0;
    }

    /* INDIRECT */
    if (block_num <= indirect_high)
    {
        block_num -= indirect_low; /* rebase, 0 is beginning indirect
                                      block range */
        
        if (inode.i_block[12] == 0)
            return 1; /* finished */

        ext4_read_block(disk, partition_offset, superblock, inode.i_block[12],
                        (uint8_t*) buf);

        if (buf[block_num] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, superblock, buf[block_num],
                        (uint8_t*) buf);
        return 0;
    }

    /* DOUBLE */
    if (block_num <= double_high)
    {
        block_num -= double_low;

        if (inode.i_block[13] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, /* double */
                        superblock, inode.i_block[13], (uint8_t*) buf);

        if (buf[block_num / addresses_in_block] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, /* indirect */
                        superblock, buf[block_num / addresses_in_block],
                        (uint8_t*) buf);

        if (buf[block_num % addresses_in_block] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, /* real */
                        superblock, buf[block_num % addresses_in_block],
                        (uint8_t*) buf);

        return 0;
    }

    /* TRIPLE */
    if (block_num <= triple_high)
    {
        block_num -= triple_low;

        if (inode.i_block[14] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, /* triple */
                        superblock, inode.i_block[14], (uint8_t*) buf);

        if (buf[block_num / (addresses_in_block*addresses_in_block)] == 0)
            return 1;
        
        ext4_read_block(disk, partition_offset, /* double */
                        superblock,
                        buf[block_num /
                        (addresses_in_block*addresses_in_block)],
                        (uint8_t*) buf);

        if (buf[block_num / addresses_in_block] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, /* indirect */
                        superblock, buf[block_num / addresses_in_block],
                        (uint8_t*) buf);

        if (buf[block_num % addresses_in_block] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, /* real */
                        superblock, buf[block_num % addresses_in_block],  
                        (uint8_t*) buf);

        return 0;
    }

    return -1;
}

int ext4_read_dir_entry(uint8_t* buf, struct ext4_dir_entry* dir)
{
    memcpy(dir, buf, sizeof(struct ext4_dir_entry));
    return 0;
}

uint64_t ext4_file_size(struct ext4_inode inode)
{
    uint64_t total_size = ((uint64_t) inode.i_size_high) << 32;
    total_size |= inode.i_size_lo;
    return total_size;
}

int ext4_print_inode_mode(uint16_t i_mode)
{
    fprintf_yellow(stdout, "\t(  ");

    /* file format */
    if ((i_mode & 0xc000) == 0xc000)
        fprintf_blue(stdout, "EXT4_S_IFSOCK | ");
    if ((i_mode & 0xa000) == 0xa000)
        fprintf_blue(stdout, "EXT4_S_IFLNK | ");
    if (i_mode & 0x8000)
        fprintf_blue(stdout, "EXT4_S_IFREG | ");
    if ((i_mode & 0x6000) == 0x6000)
        fprintf_blue(stdout, "EXT4_S_IFBLK | ");
    if (i_mode & 0x4000)
        fprintf_blue(stdout, "EXT4_S_IFDIR | ");
    if (i_mode & 0x2000)
        fprintf_blue(stdout, "EXT4_S_IFCHR | ");
    if (i_mode & 0x1000)
        fprintf_blue(stdout, "EXT4_S_IFIFO | ");

    /* process execution/group override */
    if (i_mode & 0x0800)
        fprintf_blue(stdout, "EXT4_S_ISUID | ");
    if (i_mode & 0x0400)
        fprintf_blue(stdout, "EXT4_S_ISGID | ");
    if (i_mode & 0x0200)
        fprintf_blue(stdout, "EXT4_S_ISVTX | ");

    /* access control */
    if (i_mode & 0x0100)
        fprintf_blue(stdout, "EXT4_S_IRUSR | ");
    if (i_mode & 0x0080)
        fprintf_blue(stdout, "EXT4_S_IWUSR | ");
    if (i_mode & 0x0040)
        fprintf_blue(stdout, "EXT4_S_IXUSR | ");
    if (i_mode & 0x0020)
        fprintf_blue(stdout, "EXT4_S_IRGRP | ");
    if (i_mode & 0x0010)
        fprintf_blue(stdout, "EXT4_S_IWGRP | ");
    if (i_mode & 0x0008)
        fprintf_blue(stdout, "EXT4_S_IXGRP | ");
    if (i_mode & 0x0004)
        fprintf_blue(stdout, "EXT4_S_IROTH | ");
    if (i_mode & 0x0002)
        fprintf_blue(stdout, "EXT4_S_IWOTH | ");
    if (i_mode & 0x0001)
        fprintf_blue(stdout, "EXT4_S_IXOTH | ");


    fprintf_yellow(stdout, "\b\b )\n");
    return 0;
}

int ext4_print_inode_flags(uint16_t i_flags)
{
    fprintf_yellow(stdout, "\t(  ");
    if (i_flags & 0x1)
        fprintf_blue(stdout, "EXT4_SECRM_FL | ");
    if (i_flags & 0x2)
        fprintf_blue(stdout, "EXT4_UNRM_FL | ");    
    if (i_flags & 0x4)
        fprintf_blue(stdout, "EXT4_COMPR_FL | ");
    if (i_flags & 0x8)
        fprintf_blue(stdout, "EXT4_SYNC_FL | ");

    /* compression */
    if (i_flags & 0x10)
        fprintf_blue(stdout, "EXT4_IMMUTABLE_FL | ");
    if (i_flags & 0x20)
        fprintf_blue(stdout, "EXT4_APPEND_FL | ");
    if (i_flags & 0x40)
        fprintf_blue(stdout, "EXT4_NODUMP_FL | ");
    if (i_flags & 0x80)
        fprintf_blue(stdout, "EXT4_NOATIME_FL | ");

    if (i_flags & 0x100)
        fprintf_blue(stdout, "EXT4_DIRTY_FL | ");
    if (i_flags & 0x200)
        fprintf_blue(stdout, "EXT4_COMPRBLK_FL | ");
    if (i_flags & 0x400)
        fprintf_blue(stdout, "EXT4_NOCOMPR_FL | ");
    if (i_flags & 0x800)
        fprintf_blue(stdout, "EXT4_ECOMPR_FL | ");

    if (i_flags & 0x1000)
        fprintf_blue(stdout, "EXT4_BTREE_FL | ");
    if (i_flags & 0x2000)
        fprintf_blue(stdout, "EXT4_INDEX_FL | ");
    if (i_flags & 0x4000)
        fprintf_blue(stdout, "EXT4_IMAGIC_FL | ");
    if (i_flags & 0x8000)
        fprintf_blue(stdout, "EXT3_JOURNAL_DATA_FL | ");

    if (i_flags & 0x80000000)
        fprintf_blue(stdout, "EXT4_RESERVED_FL | ");

   fprintf_yellow(stdout, "\b\b )\n");
   return 0;
}

int print_ext4_inode_osd2(uint8_t osd2[12])
{
    fprintf_yellow(stdout, "i_osd2 --\\/\n");
    fprintf_yellow(stdout, "\tl_i_frag: %"PRIu8"\n", osd2[0]);
    fprintf_yellow(stdout, "\tl_i_fsize: %"PRIu8"\n", osd2[1]);
    /* osd2[2-3] are reserved on Linux */
    fprintf_yellow(stdout, "\tl_i_uid_high: %"PRIu16"\n", (uint16_t) osd2[4]);
    fprintf_yellow(stdout, "\tl_i_gid_high: %"PRIu16"\n", (uint16_t) osd2[6]);
    /* osd2[8-11] are reserved on Linux */
    return 0;
}

int ext4_print_inode_permissions(uint16_t i_mode)
{
    fprintf_yellow(stdout, "\tPermissions: 0%"PRIo16"\n", i_mode &
                                             (0x01c0 | 0x0038 | 0x007));
    return 0;
}

int ext4_print_inode(struct ext4_inode inode)
{
    int ret = 0;
    fprintf_yellow(stdout, "i_mode: 0x%"PRIx16"\n",
                           inode.i_mode);
    ext4_print_inode_mode(inode.i_mode);
    ext4_print_inode_permissions(inode.i_mode);
    if (inode.i_mode & 0x4000)
        ret = inode.i_block[0];
    fprintf_yellow(stdout, "i_uid: %"PRIu16"\n",
                           inode.i_uid);
    fprintf_yellow(stdout, "i_size: %"PRIu32"\n",
                           inode.i_size_lo);
    fprintf_yellow(stdout, "i_atime: %"PRIu32"\n",
                           inode.i_atime);
    fprintf_yellow(stdout, "i_ctime: %"PRIu32"\n",
                           inode.i_ctime);
    fprintf_yellow(stdout, "i_mtime: %"PRIu32"\n",
                           inode.i_mtime);
    fprintf_yellow(stdout, "i_dtime: %"PRIu32"\n",
                           inode.i_dtime);
    fprintf_yellow(stdout, "i_gid: %"PRIu16"\n",
                           inode.i_gid);
    fprintf_yellow(stdout, "i_links_count: %"PRIu16"\n",
                           inode.i_links_count);
    fprintf_yellow(stdout, "i_blocks: %"PRIu32"\n",
                           inode.i_blocks_lo);
    fprintf_yellow(stdout, "i_flags: %"PRIu32"\n",
                           inode.i_flags);
    ext4_print_inode_flags(inode.i_flags);
    fprintf_yellow(stdout, "i_osd1: %"PRIu32"\n",
                           inode.i_osd1);
    fprintf_yellow(stdout, "i_block[0]; direct: %"PRIu32"\n",
                           inode.i_block[0]); /* uint32_t i_block[15]; */
    fprintf_yellow(stdout, "i_block[1]; direct: %"PRIu32"\n",
                           inode.i_block[1]);
    fprintf_yellow(stdout, "i_block[2]; direct: %"PRIu32"\n",
                           inode.i_block[2]);
    fprintf_yellow(stdout, "i_block[3]; direct: %"PRIu32"\n",
                           inode.i_block[3]);
    fprintf_yellow(stdout, "i_block[4]; direct: %"PRIu32"\n",
                           inode.i_block[4]);
    fprintf_yellow(stdout, "i_block[5]; direct: %"PRIu32"\n",
                           inode.i_block[5]);
    fprintf_yellow(stdout, "i_block[6]; direct: %"PRIu32"\n",
                           inode.i_block[6]);
    fprintf_yellow(stdout, "i_block[7]; direct: %"PRIu32"\n",
                           inode.i_block[7]);
    fprintf_yellow(stdout, "i_block[8]; direct: %"PRIu32"\n",
                           inode.i_block[8]);
    fprintf_yellow(stdout, "i_block[9]; direct: %"PRIu32"\n",
                           inode.i_block[9]);
    fprintf_yellow(stdout, "i_block[10]; direct: %"PRIu32"\n",
                           inode.i_block[10]);
    fprintf_yellow(stdout, "i_block[11]; direct: %"PRIu32"\n",
                           inode.i_block[11]);
    fprintf_yellow(stdout, "i_block[12]; indirect: %"PRIu32"\n",
                           inode.i_block[12]);
    fprintf_yellow(stdout, "i_block[13]; doubly-indirect: %"PRIu32"\n",
                           inode.i_block[13]);
    fprintf_yellow(stdout, "i_block[14]; triply-indirect: %"PRIu32"\n",
                           inode.i_block[14]);
    fprintf_yellow(stdout, "i_generation: %"PRIu32"\n",
                           inode.i_generation);
    fprintf_yellow(stdout, "i_file_acl_lo: 0%.3"PRIo32"\n",
                           inode.i_file_acl_lo);
    fprintf_yellow(stdout, "i_faddr: %"PRIu32"\n",
                           inode.i_obso_faddr);
    print_ext4_inode_osd2(inode.i_osd2);
    return ret;
}

int print_ext4_block_group_descriptor(struct ext4_block_group_descriptor bgd)
{
    fprintf_light_cyan(stdout, "--- Analyzing Block Group Descriptor ---\n");
    fprintf_yellow(stdout, "bg_block_bitmap: %"PRIu64"\n",
                           ext4_bgd_block_bitmap(bgd));
    fprintf_yellow(stdout, "bg_inode_bitmap: %"PRIu64"\n",
                           ext4_bgd_inode_bitmap(bgd));
    fprintf_yellow(stdout, "bg_inode_table: %"PRIu64"\n",
                           ext4_bgd_inode_table(bgd));
    fprintf_yellow(stdout, "bg_free_blocks_count: %"PRIu32"\n",
                           ext4_bgd_free_blocks_count(bgd));
    fprintf_yellow(stdout, "bg_free_inodes_count: %"PRIu16"\n",
                           ext4_bgd_free_inodes_count(bgd));
    fprintf_yellow(stdout, "bg_used_dirs_count: %"PRIu16"\n",
                           ext4_bgd_used_dirs_count(bgd));
    /* uint16_t bg_pad; */
    /* uint8_t bg_reserved[12]; */
    return 0;
}

int print_ext4_superblock(struct ext4_superblock superblock)
{
    fprintf_yellow(stdout, "s_inodes_count: %"PRIu32"\n",
                           superblock.s_inodes_count);
    fprintf_yellow(stdout, "s_blocks_count: %"PRIu32"\n",
                           ext4_s_blocks_count(superblock));
    fprintf_yellow(stdout, "s_r_blocks_count: %"PRIu32"\n",
                           ext4_s_r_blocks_count(superblock));
    fprintf_yellow(stdout, "s_free_blocks_count: %"PRIu32"\n",
                           ext4_s_free_blocks_count(superblock));
    fprintf_yellow(stdout, "s_free_inodes_count: %"PRIu32"\n",
                           superblock.s_free_inodes_count);
    fprintf_yellow(stdout, "s_first_data_block: %"PRIu32"\n",
                           superblock.s_first_data_block);
    fprintf_yellow(stdout, "s_log_block_size: %"PRIu32"\n",
                           superblock.s_log_block_size);
    fprintf_yellow(stdout, "s_log_frag_size: %"PRIu32"\n",
                           superblock.s_log_cluster_size);
    fprintf_yellow(stdout, "s_blocks_per_group: %"PRIu32"\n",
                           superblock.s_blocks_per_group);
    fprintf_yellow(stdout, "s_frags_per_group: %"PRIu32"\n",
                           superblock.s_clusters_per_group);
    fprintf_yellow(stdout, "s_inodes_per_group: %"PRIu32"\n",
                           superblock.s_inodes_per_group);
    fprintf_yellow(stdout, "s_mtime: %"PRIu32"\n",
                           superblock.s_mtime);
    fprintf_yellow(stdout, "s_wtime: %"PRIu32"\n",
                           superblock.s_wtime);
    fprintf_yellow(stdout, "s_mnt_count: %"PRIu16"\n",
                           superblock.s_mnt_count);
    fprintf_yellow(stdout, "s_max_mnt_count: %"PRIu16"\n",
                           superblock.s_max_mnt_count);
    fprintf_yellow(stdout, "s_magic: %"PRIx16"\n",
                           superblock.s_magic);
    if (superblock.s_magic == 0xf30a)
    {
        fprintf_light_green(stdout, "Magic value matches EXT_SUPER_MAGIC\n"); 
    }
    else
    {
        fprintf_light_red(stdout, "Magic value does not match "
                                  "EXT_SUPER_MAGIC\n");
    }
    fprintf_yellow(stdout, "s_state: %"PRIu16"\n",
                           superblock.s_state);
    fprintf_light_yellow(stdout, "File System State: %s\n",
                                 ext4_s_state_LUT[superblock.s_state]);
    fprintf_yellow(stdout, "s_errors: %"PRIu16"\n",
                           superblock.s_errors);
    fprintf_light_yellow(stdout, "Error State: %s\n",
                                 ext4_s_errors_LUT[superblock.s_state]);
    fprintf_yellow(stdout, "s_minor_rev_level: %"PRIu16"\n",
                           superblock.s_minor_rev_level);
    fprintf_yellow(stdout, "s_lastcheck: %"PRIu32"\n",
                           superblock.s_lastcheck);
    fprintf_yellow(stdout, "s_checkinterval: %"PRIu32"\n",
                           superblock.s_checkinterval);
    fprintf_yellow(stdout, "s_creator_os: %"PRIu32"\n",
                           superblock.s_creator_os);
    fprintf_light_yellow(stdout, "Resolved OS: %s\n",
                               ext4_s_creator_os_LUT[superblock.s_creator_os]);
    fprintf_yellow(stdout, "s_rev_level: %"PRIu32"\n",
                           superblock.s_rev_level);
    fprintf_light_yellow(stdout, "Revision Level: %s\n",
                                 ext4_s_rev_level_LUT[superblock.s_rev_level]);
    fprintf_yellow(stdout, "s_def_resuid: %"PRIu16"\n",
                           superblock.s_def_resuid);
    fprintf_yellow(stdout, "s_def_resgid: %"PRIu16"\n",
                           superblock.s_def_resgid);
    fprintf_yellow(stdout, "s_first_ino: %"PRIu32"\n",
                           superblock.s_first_ino);
    fprintf_yellow(stdout, "s_inode_size: %"PRIu16"\n",
                           superblock.s_inode_size);
    fprintf_yellow(stdout, "s_block_group_nr: %"PRIu16"\n",
                           superblock.s_block_group_nr);
    fprintf_yellow(stdout, "s_feature_compat: %"PRIu32"\n",
                           superblock.s_feature_compat);
    fprintf_yellow(stdout, "s_feature_incompat: %"PRIu32"\n",
                           superblock.s_feature_incompat);
    fprintf_yellow(stdout, "s_feature_ro_compat: %"PRIu32"\n",
                           superblock.s_feature_ro_compat);
    //uint8_t s_uuid[16];
    //uint8_t s_volume_name[16];
    //uint8_t s_last_mounted[64];
    fprintf_yellow(stdout, "s_algo_bitmap: %"PRIu32"\n",
                           superblock.s_algorithm_usage_bitmap);
    fprintf_yellow(stdout, "s_prealloc_blocks: %"PRIu8"\n",
    superblock.s_prealloc_blocks);                       
    fprintf_yellow(stdout, "s_prealloc_blocks: %"PRIu8"\n",
                           superblock.s_prealloc_blocks);
    //uint8_t alignment[2];
    //uint8_t s_journal_uuid[16];
    fprintf_yellow(stdout, "s_journal_inum: %"PRIu32"\n",
                           superblock.s_journal_inum);
    fprintf_yellow(stdout, "s_journal_dev: %"PRIu32"\n",
                           superblock.s_journal_dev);
    fprintf_yellow(stdout, "s_last_orphan: %"PRIu32"\n",
                           superblock.s_last_orphan);
    //uint32_t s_hash_seed[4];
    fprintf_yellow(stdout, "s_def_hash_version: %"PRIu8"\n",
                           superblock.s_def_hash_version);  
    //uint8_t padding[3];
    fprintf_yellow(stdout, "s_default_mount_options: %"PRIu32"\n",
                           superblock.s_default_mount_opts);
    fprintf_yellow(stdout, "s_first_meta_bg: %"PRIu32"\n",
                           superblock.s_first_meta_bg);
    return 0;
}

int ext4_probe(FILE* disk, int64_t partition_offset,
               struct ext4_superblock* superblock)
{
    if (partition_offset == 0)
    {
        fprintf_light_red(stderr, "ext4 probe failed on partition at offset: "
                                  "0x%.16"PRIx64".\n", partition_offset);
        return -1;
    }

    partition_offset += EXT4_SUPERBLOCK_OFFSET;

    if (fseeko(disk, partition_offset, 0))
    {
        fprintf_light_red(stderr, "Error seeking to position 0x%.16"
                                  PRIx64".\n", partition_offset);
        return -1;
    }

    if (fread(superblock, 1, sizeof(struct ext4_superblock), disk) !=
        sizeof(struct ext4_superblock))
    {
        fprintf_light_red(stderr, 
                          "Error while trying to read ext4 superblock.\n");
        return -1;
    }

    if (superblock->s_magic != 0xef53 || !(superblock->s_feature_ro_compat &
                                           EXT3_FEATURE_RO_COMPAT_UNSUPPORTED)
                                      ||
                                         !(superblock->s_feature_incompat &
                                           EXT3_FEATURE_INCOMPAT_UNSUPPORTED))
    {
        fprintf_light_red(stderr, "ext4 superblock s_magic[0x%0.4"PRIx16
                                  "] mismatch.\n", superblock->s_magic);
        return -1;
    }

    return 0;
}

int ext4_serialize_bgd_sectors(struct bson_info* serialized,
                               struct ext4_block_group_descriptor bgd,
                               struct ext4_superblock* superblock,
                               struct bitarray* bits, 
                               int64_t offset)
{
    uint64_t block_size = ext4_block_size(*superblock);
    uint64_t sector, inode_table_start, i;
    struct bson_kv value;

    value.type = BSON_INT64;

    sector = (ext4_bgd_block_bitmap(bgd) * block_size + offset) / SECTOR_SIZE;
    value.key = "block_bitmap_sector_start";
    value.data = &sector; 

    bitarray_set_bit(bits, sector / block_size);
    bson_serialize(serialized, &value);

    sector = (ext4_bgd_inode_bitmap(bgd) * block_size + offset) / SECTOR_SIZE;
    value.key = "inode_bitmap_sector_start";
    value.data = &sector; 

    bitarray_set_bit(bits, sector / block_size);
    bson_serialize(serialized, &value);
    
    sector = (ext4_bgd_inode_table(bgd) * block_size + offset) / SECTOR_SIZE;
    value.key = "inode_table_sector_start";
    value.data = &sector; 

    inode_table_start = sector;

    bson_serialize(serialized, &value);
    
    sector = (ext4_bgd_inode_table(bgd) * block_size + offset + 
              superblock->s_inodes_per_group * 
              sizeof(struct ext4_inode)) / SECTOR_SIZE - 1;

    for (i = 0; i < sector; i += block_size /  SECTOR_SIZE)
    {
        bitarray_set_bit(bits, (inode_table_start + i) / block_size);
    }

    return EXIT_SUCCESS;
}

char* ext4_last_mount_point(struct ext4_superblock* superblock)
{
    return (char *) (superblock->s_last_mounted);
}

int ext4_serialize_fs(struct ext4_superblock* superblock,
                      int64_t offset,
                      int32_t pte_num,
                      struct bitarray* bits,
                      char* mount_point, 
                      FILE* serializedf)
{
    /* round up integer arithmetic */
    int32_t num_block_groups = (ext4_s_blocks_count(*superblock) +
                                (superblock->s_blocks_per_group - 1)) /
                                superblock->s_blocks_per_group;
    /* plus 2 because need to rebase on fist usable inode; also
     * the '/' root inode is inside the reserved inodes---always inode 2 */
    int32_t num_files = superblock->s_inodes_count -
                        superblock->s_free_inodes_count -
                        superblock->s_first_ino + 2;
    uint64_t block_size = ext4_block_size(*superblock);
    uint64_t blocks_per_group = superblock->s_blocks_per_group;
    uint64_t inodes_per_group = superblock->s_inodes_per_group;
    uint64_t inode_size = superblock->s_inode_size;
    struct bson_info* serialized;
    struct bson_info* sectors;
    struct bson_kv value;

    serialized = bson_init();
    sectors = bson_init();

    value.type = BSON_STRING;
    value.size = strlen("fs");
    value.key = "type";
    value.data = "fs";

    bson_serialize(serialized, &value);

    value.type = BSON_INT32;
    value.key = "pte_num";
    value.data = &(pte_num);

    bson_serialize(serialized, &value);

    value.type = BSON_STRING;
    value.size = strlen("ext4");
    value.key = "fs";
    value.data = "ext4";

    bson_serialize(serialized, &value);

    value.type = BSON_STRING;
    value.key = "mount_point";
    value.size = strlen(mount_point);
    value.data = mount_point;

    bson_serialize(serialized, &value);

    value.type = BSON_INT32;
    value.key = "num_block_groups";
    value.data = &(num_block_groups);

    bson_serialize(serialized, &value);

    value.type = BSON_INT32;
    value.key = "num_files";
    value.data = &(num_files);

    bson_serialize(serialized, &value);

    value.type = BSON_INT64;
    value.key = "superblock_sector";
    offset /= SECTOR_SIZE;
    value.data = &(offset);

    bitarray_set_bit(bits, offset / block_size);

    bson_serialize(serialized, &value);

    value.type = BSON_INT64;
    value.key = "superblock_offset";
    offset = SECTOR_SIZE*2;
    value.data = &(offset);

    bson_serialize(serialized, &value);

    value.key = "block_size";
    value.type = BSON_INT64;
    value.data = &(block_size);

    bson_serialize(serialized, &value);

    value.key = "blocks_per_group";
    value.type = BSON_INT64;
    value.data = &(blocks_per_group);

    bson_serialize(serialized, &value);

    value.key = "inodes_per_group";
    value.type = BSON_INT64;
    value.data = &(inodes_per_group);

    bson_serialize(serialized, &value);

    value.key = "inode_size";
    value.type = BSON_INT64;
    value.data = &(inode_size);

    bson_serialize(serialized, &value);

    bson_finalize(serialized);
    bson_writef(serialized, serializedf);
    bson_cleanup(sectors);
    bson_cleanup(serialized);

    return EXIT_SUCCESS;
}

int ext4_serialize_bgds(FILE* disk, int64_t partition_offset,
                        struct ext4_superblock* superblock,
                        struct bitarray* bits, FILE* serializef,
                        uint8_t* bcache)
{
    struct ext4_block_group_descriptor bgd;
    struct bson_info* serialized;
    struct bson_kv v_type, v_bgd, v_sector, v_offset;
    uint32_t sector = 0, offset = 0;
    uint64_t block_size = ext4_block_size(*superblock);

    serialized = bson_init();
    
    v_type.type = BSON_STRING;
    v_type.key = "type";
    v_type.size = strlen("bgd");
    v_type.data = "bgd";
    
    v_sector.type = BSON_INT32;
    v_sector.key = "sector";
    v_sector.data = &sector;

    v_offset.type = BSON_INT32;
    v_offset.key = "offset";
    v_offset.data = &offset;

    while ((ext4_next_block_group_descriptor(disk,
                                             partition_offset,
                                             *superblock,
                                             &bgd,
                                             &sector,
                                             &offset,
                                             bcache)) > 0)
    {

        bson_serialize(serialized, &v_type);
        bson_serialize(serialized, &v_bgd);
        bson_serialize(serialized, &v_sector);
        bson_serialize(serialized, &v_offset);

        bitarray_set_bit(bits, sector / block_size);
        ext4_serialize_bgd_sectors(serialized, bgd, superblock, bits,
                                   partition_offset);

        bson_finalize(serialized);
        bson_writef(serialized, serializef);

        bson_reset(serialized);
    }

    bson_cleanup(serialized);

    return EXIT_SUCCESS;
}

int ext4_serialize_file_extent_sectors(FILE* disk, int64_t partition_offset,
                                       struct ext4_superblock superblock,
                                       struct bitarray* bits,
                                       uint32_t block_num,
                                       struct ext4_inode inode,
                                       struct bson_info* sectors,
                                       struct bson_info* data,
                                       struct bson_info* extents,
                                       bool save_data,
                                       bool save_extents)
{
    int i;
    struct ext4_extent_header* hdr; 
    struct ext4_extent_idx idx = {};
    struct ext4_extent_idx idx2 = {}; /* lookahead when searching block_num */
    struct ext4_extent extent;
    uint64_t block_size = ext4_block_size(superblock);
    uint8_t buf[block_size];

    struct bson_kv value, data_value, extent_value;
    uint64_t sector;
    uint64_t sectors_per_block = block_size / SECTOR_SIZE;
    char count[32];
    value.type = BSON_INT32;
    value.key = count;

    if (save_data)
    {
        data_value.key = count;
        data_value.type = BSON_BINARY;
        data_value.data = buf;
        data_value.size = block_size;
    }

    if (save_extents)
    {
        extent_value.key = count;
        extent_value.type = BSON_INT64;
        extent_value.data = &sector;
    }

    memcpy(buf, inode.i_block, (size_t) 60);
    hdr = (struct ext4_extent_header*) buf;
    idx.ei_block = (uint32_t) 2 << 31;

    for (i = 0; i < hdr->eh_entries; i++)
    {
        if (hdr->eh_depth)
        {
            idx2 =  * ((struct ext4_extent_idx*)
                            &(buf[sizeof(struct ext4_extent_header) +
                                  sizeof(struct ext4_extent_idx)*i])); 
            if (hdr->eh_entries == 1)
            {
                ext4_read_block(disk, partition_offset, superblock,
                                ext4_extent_index_leaf(idx2), buf);
                
                if (save_extents && block_num == idx2.ei_block)
                {
                    sector = ext4_extent_index_leaf(idx2) * sectors_per_block +
                             partition_offset / SECTOR_SIZE;

                    bitarray_set_bit(bits, sector / block_size);
                    
                    snprintf(count, 32, "%"PRIu64, sector);
                    bson_serialize(extents, &extent_value);
                }
                
                i = -1; /* allow loop-expr to run (++) */
                hdr = (struct ext4_extent_header*) buf;
                idx.ei_block = (uint32_t) 2 << 31;
                continue;
            }

            if ((block_num < idx2.ei_block &&
                block_num >= idx.ei_block))
            {
                ext4_read_block(disk, partition_offset, superblock,
                                ext4_extent_index_leaf(idx), buf);

                if (save_extents && idx.ei_block == block_num)
                {
                    sector = ext4_extent_index_leaf(idx) * sectors_per_block +
                             partition_offset / SECTOR_SIZE;

                    bitarray_set_bit(bits, sector / block_size);
                    
                    snprintf(count, 32, "%"PRIu64, sector);
                    bson_serialize(extents, &extent_value);
                }

                i = -1; /* allow loop-expr to run (++) */
                hdr = (struct ext4_extent_header*) buf;
                idx.ei_block = (uint32_t) 2 << 31;
                continue;
            }
            idx = idx2;
        }
        else
        {
            extent = * ((struct ext4_extent*)
                            &(buf[sizeof(struct ext4_extent_header) +
                                  sizeof(struct ext4_extent)*i])); 
            if (extent.ee_block <= block_num &&
                block_num < extent.ee_block + extent.ee_len)
            {
                block_num -= extent.ee_block; /* rebase */
                sector = (ext4_extent_start(extent) + block_num);
                sector *= ext4_block_size(superblock);
                sector += partition_offset;
                sector /= SECTOR_SIZE;

                snprintf(count, 32, "%"PRIu32, block_num + extent.ee_block);
                value.data = &sector;
                bson_serialize(sectors, &value);

                if (save_data)
                {
                    snprintf(count, 32, "%"PRIu64, sector);
                    bitarray_set_bit(bits, sector / block_size);
                    ext4_read_block(disk, partition_offset, superblock,
                                    (ext4_extent_start(extent) + block_num),
                                    (uint8_t*) buf);
                    bson_serialize(data, &data_value);
                }

                return 0;
            }
        }
    }

    return -1;
}

int ext4_serialize_file_block_sectors(FILE* disk, int64_t partition_offset,
                                      struct ext4_superblock superblock,
                                      struct bitarray* bits,
                                      uint32_t block_num,
                                      struct ext4_inode inode,
                                      struct bson_info* sectors,
                                      struct bson_info* data,
                                      struct bson_info* extents,
                                      bool data_save, bool save_extents)
{
    uint32_t block_size = ext4_block_size(superblock);
    uint32_t addresses_in_block = block_size / 4;
    uint32_t buf[addresses_in_block];
    uint32_t sectors_per_block = block_size / SECTOR_SIZE;
    
    /* ranges for lookup */
    uint32_t direct_low = 0;
    uint32_t direct_high = 11;
    uint32_t indirect_low = direct_high + 1;
    uint32_t indirect_high = direct_high + (addresses_in_block);
    uint32_t double_low = indirect_high + 1;
    uint32_t double_high = indirect_high + (addresses_in_block)*
                                           (addresses_in_block);
    uint32_t triple_low = double_high + 1;
    uint32_t triple_high = double_high + (addresses_in_block)*
                                         (addresses_in_block)*
                                         (addresses_in_block);

    struct bson_kv value, data_value, extent_value;
    char count[11];
    uint32_t sector;
    value.type = BSON_INT32;
    value.key = count;

    if (data_save)
    {
        data_value.key = count;
        data_value.type = BSON_BINARY;
        data_value.data = buf;
        data_value.size = ext4_block_size(superblock);
    }

    if (save_extents)
    {
        extent_value.key = count;
        extent_value.type = BSON_BINARY;
        extent_value.data = buf;
        extent_value.size = ext4_block_size(superblock);
    }

    if (block_num < direct_low || block_num > triple_high)
    {
        fprintf_light_red(stderr, "File block outside of range of inode.\n");
        return -1;
    }

    /* figure out type of block lookup (direct, indirect, double, treble) */
    /* DIRECT */
    if (block_num <= direct_high)
    {
        if (inode.i_block[block_num] == 0)
            return 1; /* finished */

        sector = (inode.i_block[block_num] * ext4_block_size(superblock) +
                  partition_offset) / SECTOR_SIZE;

        snprintf(count, 11, "%"PRIu32, (block_num * sectors_per_block));
        value.data = &sector;
        bson_serialize(sectors, &value);

        if (data_save)
        {
            bitarray_set_bit(bits, sector / block_size);
            ext4_read_block(disk, partition_offset, superblock,
                            inode.i_block[block_num], (uint8_t*) buf);
            bson_serialize(data, &data_value);
        }

        return 0;
    }

    /* INDIRECT */
    if (block_num <= indirect_high)
    {
        block_num -= indirect_low; /* rebase, 0 is beginning indirect
                                      block range */
        
        if (inode.i_block[12] == 0)
            return 1; /* finished */

        ext4_read_block(disk, partition_offset, superblock, inode.i_block[12],
                        (uint8_t*) buf);

        if (save_extents && block_num == 0)
        {
            sector = (uint32_t) (inode.i_block[12] * sectors_per_block +
                                 partition_offset / SECTOR_SIZE);
            bitarray_set_bit(bits, sector / block_size);
            snprintf(count, 11, "%"PRIu32, sector);
            bson_serialize(extents, &extent_value);
        }

        if (buf[block_num] == 0)
            return 1;

        sector = (buf[block_num] * ext4_block_size(superblock) +
                  partition_offset) / SECTOR_SIZE;

        snprintf(count, 11, "%"PRIu32, ((block_num + indirect_low) *
                                         sectors_per_block));
        value.data = &sector;
        bson_serialize(sectors, &value);

        if (data_save)
        {
            bitarray_set_bit(bits, sector / block_size);
            ext4_read_block(disk, partition_offset, superblock,
                            buf[block_num], (uint8_t*) buf);
            bson_serialize(data, &data_value);
        }

        return 0;
    }

    /* DOUBLE */
    if (block_num <= double_high)
    {
        block_num -= double_low;

        if (inode.i_block[13] == 0)
            return 1;

        ext4_read_block(disk, partition_offset, /* double */
                        superblock, inode.i_block[13], (uint8_t*) buf);

        if (buf[block_num / addresses_in_block] == 0)
            return 1;

        if (save_extents && block_num / addresses_in_block == 0)
        {
            sector = (uint32_t) (inode.i_block[13] * sectors_per_block +
                                 partition_offset / SECTOR_SIZE);
            bitarray_set_bit(bits, sector / block_size);
            snprintf(count, 11, "%"PRIu32, sector);
            bson_serialize(extents, &extent_value);
        }

        sector = (buf[block_num / addresses_in_block] *
                  ext4_block_size(superblock) + partition_offset) /
                  SECTOR_SIZE;

        ext4_read_block(disk, partition_offset, /* indirect */
                        superblock, buf[block_num / addresses_in_block],
                        (uint8_t*) buf);

        if (buf[block_num % addresses_in_block] == 0)
            return 1;

        if (save_extents && block_num % addresses_in_block == 0)
        {
            bitarray_set_bit(bits, sector / block_size);
            snprintf(count, 11, "%"PRIu32, sector);
            bson_serialize(extents, &extent_value);
        }

        sector = (buf[block_num % addresses_in_block] *
                  ext4_block_size(superblock) + partition_offset) /
                  SECTOR_SIZE;

        snprintf(count, 11, "%"PRIu32, ((block_num + double_low) *
                                       sectors_per_block));
        value.data = &sector;
        bson_serialize(sectors, &value);
    
        if (data_save)
        {
            bitarray_set_bit(bits, sector / block_size);
            ext4_read_block(disk, partition_offset, superblock,
                            buf[block_num % addresses_in_block],
                            (uint8_t*) buf);
            bson_serialize(data, &data_value);
        }

        return 0;
    }

    /* TRIPLE */
    if (block_num <= triple_high)
    {
        block_num -= triple_low;

        if (inode.i_block[14] == 0)
            return 1;


        ext4_read_block(disk, partition_offset, /* triple */
                        superblock, inode.i_block[14], (uint8_t*) buf);

        if (buf[block_num / (addresses_in_block*addresses_in_block)] == 0)
            return 1;
        
        if (save_extents && block_num /
                            (addresses_in_block*addresses_in_block) == 0)
        {
            sector = (uint32_t) (inode.i_block[14] * sectors_per_block +
                                 partition_offset / SECTOR_SIZE);
            bitarray_set_bit(bits, sector / block_size);
            snprintf(count, 11, "%"PRIu32, sector);
            bson_serialize(extents, &extent_value);
        }
        
        sector = (buf[block_num / (addresses_in_block*addresses_in_block)] *
                  ext4_block_size(superblock) + partition_offset) /
                  SECTOR_SIZE;

        ext4_read_block(disk, partition_offset, /* double */
                        superblock,
                        buf[block_num / (addresses_in_block*
                                         addresses_in_block)],
                        (uint8_t*) buf);

        if (buf[block_num / addresses_in_block] == 0)
            return 1;
        
        if (save_extents && block_num / addresses_in_block == 0)
        {
            bitarray_set_bit(bits, sector / block_size);
            snprintf(count, 11, "%"PRIu32, sector);
            bson_serialize(extents, &extent_value);
        }
        
        sector = (buf[block_num / addresses_in_block] *
                  ext4_block_size(superblock) + partition_offset) /
                  SECTOR_SIZE;

        ext4_read_block(disk, partition_offset, /* indirect */
                        superblock, buf[block_num / addresses_in_block],
                        (uint8_t*) buf);

        if (buf[block_num % addresses_in_block] == 0)
            return 1;
        
        if (save_extents && block_num % addresses_in_block == 0)
        {
            bitarray_set_bit(bits, sector / block_size);
            snprintf(count, 11, "%"PRIu32, sector);
            bson_serialize(extents, &extent_value);
        }

        sector = (buf[block_num % addresses_in_block] *
                  ext4_block_size(superblock) + partition_offset) /
                  SECTOR_SIZE;

        snprintf(count, 11, "%"PRIu32, ((block_num + triple_low) *
                                         sectors_per_block));
        value.data = &sector;
        bson_serialize(sectors, &value);
    
        if (data_save)
        {
            bitarray_set_bit(bits, sector / block_size);
            ext4_read_block(disk, partition_offset, superblock,
                            buf[block_num % addresses_in_block],
                            (uint8_t*) buf);
            bson_serialize(data, &data_value);
        }

        return 0;
    }

    return -1;
}

int ext4_serialize_file_sectors(FILE* disk, int64_t partition_offset,
                                struct ext4_superblock superblock, 
                                struct bitarray* bits,
                                struct ext4_inode inode,
                                struct bson_info* serialized,
                                bool save_data, bool save_extents)
{
    uint64_t block_size = ext4_block_size(superblock),
             file_size = ext4_file_size(inode);
    struct bson_info* sectors = NULL;
    struct bson_info* data = NULL;
    struct bson_info* extents = NULL;
    struct bson_kv value, data_value, extent_value;
    uint64_t num_blocks;
    uint64_t count;

    int ret_check;

    if (file_size == 0)
    {
        num_blocks = 0;
    }
    else
    {
        num_blocks = file_size / block_size;
        if (file_size % block_size != 0)
            num_blocks += 1;
    }

    value.type = BSON_ARRAY;
    value.key = "sectors";

    if (save_data)
    {
        data_value.type = BSON_ARRAY;
        data_value.key = "data";
    }

    if (save_extents)
    {
        extent_value.type = BSON_ARRAY;
        extent_value.key = "extents";
    }
    
    sectors = bson_init();

    if (save_data)
        data = bson_init();

    if (save_extents)
        extents = bson_init();

    if ((inode.i_mode & 0xa000) == 0xa000)
        goto skip;

    /* go through each valid block of the inode */
    count = 0;
    while (num_blocks) 
    {
        if (inode.i_flags & 0x80000) /* check if extents in use */
            ret_check = ext4_serialize_file_extent_sectors(disk,
                                                           partition_offset,
                                                           superblock, bits,
                                                           count, inode,
                                                           sectors, data,
                                                           extents, save_data,
                                                           save_extents);
        else
            ret_check = ext4_serialize_file_block_sectors(disk,
                                                          partition_offset,
                                                          superblock, bits,
                                                          count, inode,
                                                          sectors, data,
                                                          extents, save_data,
                                                          save_extents);
        
        if (ret_check < 0) /* error reading */
        {
            count++;
            num_blocks--;
            continue;
        }
        else if (ret_check > 0) /* no more blocks? */
        {
            fprintf_light_red(stderr, "Premature ending of file blocks.\n");
            exit(1);
            return -1;
        }

        count++;
        num_blocks--;
    }

skip:

    bson_finalize(sectors);
    value.data = sectors;
    bson_serialize(serialized, &value);
    bson_cleanup(sectors);

    if (save_data)
    {
        bson_finalize(data);
        data_value.data = data;
        bson_serialize(serialized, &data_value);
        bson_cleanup(data);
    }

    if (save_extents)
    {
        bson_finalize(extents);
        extent_value.data = extents;
        bson_serialize(serialized, &extent_value);
        bson_cleanup(extents);
    }

   return 0;
}

int ext4_serialize_tree(FILE* disk, int64_t partition_offset, 
                        struct ext4_superblock superblock,
                        struct bitarray* bits,
                        struct ext4_inode root_inode,
                        char* prefix,
                        FILE* serializef,
                        struct bson_info* bson,
                        uint8_t* icache,
                        uint8_t* bcache)
{
    struct ext4_inode child_inode;
    struct ext4_dir_entry dir;
    uint64_t block_size = ext4_block_size(superblock), position = 0;
    uint8_t buf[block_size];
    uint64_t num_blocks, fsize = ext4_file_size(root_inode);
    uint64_t i, mode, link_count, uid, gid, atime, mtime, ctime, inode_num,
             sector;
    int ret_check;
    char path[8192];
    char count[32];
    uint8_t xray_dentry_buf[4096];

    struct bson_info* dentries = NULL, *bson2 = NULL;
    struct bson_kv value, dentry_value;
    bool is_dir = (root_inode.i_mode & 0x4000) == 0x4000;
    
    if ((root_inode.i_mode & 0xa000) == 0xa000) /* symlink */
    {
        value.type = BSON_STRING;
        value.size = strlen(prefix);
        value.key = "path";
        value.data = prefix;

        bson_serialize(bson, &value);

        value.type = BSON_BOOLEAN;
        value.key = "is_dir";
        value.data = &is_dir;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "size";
        value.data = &(fsize);

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "mode";
        mode = root_inode.i_mode;
        value.data = &mode;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "link_count";
        link_count = root_inode.i_links_count;
        value.data = &link_count;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "uid";
        uid = root_inode.i_uid;
        value.data = &uid;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "gid";
        gid = root_inode.i_gid;
        value.data = &gid;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "atime";
        atime = root_inode.i_atime;
        value.data = &atime;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "mtime";
        mtime = root_inode.i_mtime;
        value.data = &mtime;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "ctime";
        ctime = root_inode.i_ctime;
        value.data = &ctime;

        bson_serialize(bson, &value);

        value.type = BSON_STRING;
        value.size = fsize;
        value.key = "link_name";

        if (fsize < 60)
        {
            value.data = root_inode.i_block;

        }
        else if (fsize < 4096)
        {
            ext4_read_extent_block(disk, partition_offset, superblock, 0,
                                   root_inode, buf);
            value.data = buf;
        }
        else
        {
            fprintf_light_red(stderr, "Warning: link name >= 4096!\n");
        }

        bson_serialize(bson, &value);

        ext4_serialize_file_sectors(disk, partition_offset, superblock, bits,
                                    root_inode, bson, false, true);

        bson_finalize(bson);
        bson_writef(bson, serializef);
        bson_cleanup(bson);
        return 0;
    }
    else if ((root_inode.i_mode & 0x8000) == 0x8000) /* file, no dir
                                                        entries more */
    {
        value.type = BSON_STRING;
        value.size = strlen(prefix);
        value.key = "path";
        value.data = prefix;

        bson_serialize(bson, &value);

        value.type = BSON_BOOLEAN;
        value.key = "is_dir";
        value.data = &is_dir;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "size";
        value.data = &(fsize);

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "mode";
        mode = root_inode.i_mode;
        value.data = &mode;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "link_count";
        link_count = root_inode.i_links_count;
        value.data = &link_count;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "uid";
        uid = root_inode.i_uid;
        value.data = &uid;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "gid";
        gid = root_inode.i_gid;
        value.data = &gid;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "atime";
        atime = root_inode.i_atime;
        value.data = &atime;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "mtime";
        mtime = root_inode.i_mtime;
        value.data = &mtime;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "ctime";
        ctime = root_inode.i_ctime;
        value.data = &ctime;

        bson_serialize(bson, &value);

        ext4_serialize_file_sectors(disk, partition_offset, superblock, bits,
                                    root_inode, bson, false, true);
        
        bson_finalize(bson);
        bson_writef(bson, serializef);
        bson_cleanup(bson);
        return 0;
    }
    else if ((root_inode.i_mode & 0x4000) == 0x4000) /* dir */
    {
        value.type = BSON_STRING;
        value.size = strlen(prefix);
        value.key = "path";
        value.data = prefix;

        bson_serialize(bson, &value);

        value.type = BSON_BOOLEAN;
        value.key = "is_dir";
        value.data = &is_dir;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "size";
        value.data = &(fsize);

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "mode";
        mode = root_inode.i_mode;
        value.data = &mode;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "link_count";
        link_count = root_inode.i_links_count;
        value.data = &link_count;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "uid";
        uid = root_inode.i_uid;
        value.data = &uid;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "gid";
        gid = root_inode.i_gid;
        value.data = &gid;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "atime";
        atime = root_inode.i_atime;
        value.data = &atime;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "mtime";
        mtime = root_inode.i_mtime;
        value.data = &mtime;

        bson_serialize(bson, &value);

        value.type = BSON_INT64;
        value.key = "ctime";
        ctime = root_inode.i_ctime;
        value.data = &ctime;

        bson_serialize(bson, &value);

        /* true, serialize data with sectors */
        ext4_serialize_file_sectors(disk, partition_offset,
                                    superblock, bits, root_inode, bson, false,
                                    true);

    }

    if (ext4_file_size(root_inode) == 0)
    {
        num_blocks = 0;
    }
    else
    {
        num_blocks = ext4_file_size(root_inode) / block_size;
        if (ext4_file_size(root_inode) % block_size != 0)
            num_blocks += 1;
    }

    value.type = BSON_ARRAY;
    value.key = "files";

    dentry_value.key = count;
    dentry_value.type = BSON_BINARY;
    dentry_value.data = xray_dentry_buf;

    dentries = bson_init();

    sector = 0;

    /* go through each valid block of the inode */
    for (i = 0; i < num_blocks; i++)
    {
        if (root_inode.i_flags & 0x80000)
            ret_check = ext4_read_extent_block(disk, partition_offset,
                                               superblock, i, root_inode, buf);
        else
            ret_check = ext4_read_file_block(disk, partition_offset,
                                             superblock, i, root_inode,
                                             (uint32_t*) buf);

        if (root_inode.i_flags & 0x80000)
            sector = ext4_sector_extent_block(disk, partition_offset,
                                              superblock, i, root_inode);
        else
            sector = ext4_sector_file_block(disk, partition_offset, superblock,
                                            i, root_inode);

        if (ret_check < 0) /* error reading */
        {
            fprintf_light_red(stderr, "Error reading inode dir block. "
                                      "Assuming  hole.\n");
            continue;
        }
        else if (ret_check > 0) /* no more blocks? */
        {
            return 0;
        }

        position = 0;

        while (position < block_size)
        {
            strcpy(path, prefix);
            if (strlen(path) > 1)
                strcat(path, "/");

            if (ext4_read_dir_entry(&buf[position], &dir))
            {
                fprintf_light_red(stderr, "Error reading dir entry from "
                                          "block.\n");
                return -1;
            }

            if (dir.inode == 0)
            {
                position += dir.rec_len;
                continue;
            }

            dentry_value.size = sizeof(uint64_t) + dir.name_len;
            inode_num = dir.inode;
            memcpy(xray_dentry_buf, &inode_num, sizeof(uint64_t));
            memcpy(&(xray_dentry_buf[sizeof(uint64_t)]), dir.name,
                   dir.name_len);
            snprintf(count, 32, "%"PRIu64, sector);
            bson_serialize(dentries, &dentry_value);

            bson2 = bson_init();

            if (ext4_read_inode_serialized(disk, partition_offset, superblock,
                               dir.inode, &child_inode, bson2, icache, bcache))
            {
               fprintf_light_red(stderr, "Error reading child inode.\n");
               return -1;
            } 

            dir.name[dir.name_len] = 0;
            strcat(path, (char*) dir.name);
            
            if (strcmp((const char *) dir.name, ".") != 0 &&
                strcmp((const char *) dir.name, "..") != 0)
            {
                if (child_inode.i_mode & 0x4000)
                {
                }
                else if (child_inode.i_mode & 0x8000)
                {
                }
                else
                {
                    fprintf_red(stderr, "Not directory or file: %s\n", path);
                }
                ext4_serialize_tree(disk, partition_offset, superblock, bits,
                                    child_inode, path, serializef,
                                    bson2, icache, bcache); /* recursive
                                                               call */
            }

            position += dir.rec_len;
        }
    }

    bson_finalize(dentries);
    value.data = dentries;
    bson_serialize(bson, &value);
    bson_cleanup(dentries);
        
    bson_finalize(bson);
    bson_writef(bson, serializef);
    bson_cleanup(bson);

    return 0;
}

int ext4_serialize_fs_tree(FILE* disk, int64_t partition_offset,
                           struct ext4_superblock* superblock,
                           struct bitarray* bits, char* mount,
                           FILE* serializef, uint8_t* icache, uint8_t* bcache)
{
    struct ext4_inode root;
    struct bson_info* bson;
    char* buf = malloc(strlen(mount) + 1);

    if (buf == NULL)
    {
        fprintf_light_red(stderr, "Error allocating root dir path string.\n");
        return -1;
    }

    memcpy(buf, mount, strlen(mount));
    buf[strlen(mount) + 1] = '\0';

    bson = bson_init();

    if (ext4_read_inode_serialized(disk, partition_offset, *superblock, 2,\
                                   &root, bson, icache, bcache))
    {
        free(buf);
        fprintf(stderr, "Failed getting root fs inode.\n");
        return -1;
    }

    if (ext4_serialize_tree(disk, partition_offset, *superblock, bits, root,
                            buf, serializef, bson, icache, bcache))
    {
        free(buf);
        fprintf(stdout, "Error listing fs tree from root inode.\n");
        return -1;
    }

    free(buf);

    return 0;
}

int ext4_cache_bgds(FILE* disk, int64_t partition_offset,
                    struct ext4_superblock* superblock, uint8_t** cache)
{
    uint64_t num_block_groups = ext4_num_block_groups(*superblock);
    uint32_t i = 0;
    uint8_t* cachep;

    *cache = malloc(sizeof(struct ext4_block_group_descriptor) *
                    num_block_groups);
    cachep = *cache;

    if (cachep == NULL)
    {
        fprintf_light_red(stderr, "Failed allocating inode cache.\n");
        return EXIT_FAILURE;
    }

    for (i = 0 ; i < num_block_groups; i++)
    {
        ext4_read_bgd(disk, partition_offset, *superblock, i,
                      (struct ext4_block_group_descriptor*) cachep, NULL);

        cachep += sizeof(struct ext4_block_group_descriptor);
    }

    return EXIT_SUCCESS;
}

int ext4_cache_inodes(FILE* disk, int64_t partition_offset,
                      struct ext4_superblock* superblock,
                      uint8_t** cache, uint8_t* bcache)
{
    struct ext4_block_group_descriptor bgd;
    uint8_t* cachep;
    uint32_t i = 0;
    uint64_t num_block_groups = ext4_num_block_groups(*superblock);
    uint64_t block_size = ext4_block_size(*superblock), inode_table_start = 0;
    uint64_t inode_table_size = superblock->s_inode_size *
                                superblock->s_inodes_per_group;
    int ret;

    *cache = malloc(inode_table_size * num_block_groups);
    cachep = *cache;

    if (cachep == NULL)
    {
        fprintf_light_red(stderr, "Failed allocating inode cache.\n");
        return EXIT_FAILURE;
    }

    for (i = 0 ; i < num_block_groups; i++)
    {
        ext4_read_bgd(disk, partition_offset, *superblock, i, &bgd, bcache);
        inode_table_start = (ext4_bgd_inode_table(bgd) * block_size +
                             partition_offset);

        if ((inode_table_start + inode_table_size) >=
            (partition_offset + block_size * ext4_s_blocks_count(*superblock)))
        {
            fprintf_light_white(stderr, "WARNING: BGD %d claims inode table "
                                        "outside of partition boundary.\n");
            inode_table_size -=  (inode_table_start + inode_table_size) -
                                 (partition_offset + block_size *
                                  ext4_s_blocks_count(*superblock));
        }

        if (fseeko(disk, inode_table_start, 0))
        {
            fprintf_light_red(stderr, "Error seeking to inode table 0x%lx.\n",
                              inode_table_start);
            return EXIT_FAILURE;
        }

        if ((ret = fread(cachep, 1, inode_table_size, disk)) !=
                                                              inode_table_size)
        {
            fprintf_light_red(stderr, "Error trying to read inode table.\n");
            return EXIT_FAILURE;
        }

        cachep += superblock->s_inode_size * superblock->s_inodes_per_group;
    }

    return EXIT_SUCCESS;
}

int ext4_serialize_journal(FILE* disk, int64_t partition_offset,
                           struct ext4_superblock* superblock,
                           struct bitarray* bits, char* mount,
                           FILE* serializef, uint8_t* icache, uint8_t* bcache)
{
    struct ext4_inode root;
    struct bson_info* bson;
    char* buf = malloc(strlen(mount) + 1);

    if (buf == NULL)
    {
        fprintf_light_red(stderr, "Error allocating root dir path string.\n");
        return -1;
    }

    memcpy(buf, mount, strlen(mount));
    buf[strlen(mount) + 1] = '\0';

    bson = bson_init();

    if (ext4_read_inode_serialized(disk, partition_offset, *superblock, 8,\
                                   &root, bson, icache, bcache))
    {
        free(buf);
        fprintf(stderr, "Failed getting journal inode.\n");
        return -1;
    }

    if (ext4_serialize_tree(disk, partition_offset, *superblock, bits, root,
                            buf, serializef, bson, icache, bcache))
    {
        free(buf);
        fprintf(stdout, "Error listing fs tree from journal inode.\n");
        return -1;
    }

    free(buf);

    return 0;
}
