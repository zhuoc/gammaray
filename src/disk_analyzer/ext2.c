#include "ext2.h"

char* s_creator_os_LUT[] = {
                                "EXT2_OS_LINUX","EXT2_OS_HURD","EXT2_OS_MASIX",
                                "EXT2_OS_FREEBSD","EXT2_OS_LITES"
                           };

char* s_rev_level_LUT[] = {
                                "EXT2_GOOD_OLD_REV","EXT2_DYNAMIC_REV"
                          };

char* s_state_LUT[] = {
                                "","EXT2_VALID_FS","EXT2_ERROR_FS"
                      };

char* s_errors_LUT[] = {
                                "","EXT2_ERRORS_CONTINUE","EXT2_ERRORS_RO",
                                "EXT2_ERRORS_PANIC"
                       };

int print_inode_mode(uint16_t i_mode)
{
    fprintf_yellow(stdout, "\t(  ");

    /* file format */
    if ((i_mode & 0xc000) == 0xc000)
        fprintf_blue(stdout, "EXT2_S_IFSOCK | ");
    if ((i_mode & 0xa000) == 0xa000)
        fprintf_blue(stdout, "EXT2_S_IFLNK | ");
    if (i_mode & 0x8000)
        fprintf_blue(stdout, "EXT2_S_IFREG | ");
    if ((i_mode & 0x6000) == 0x6000)
        fprintf_blue(stdout, "EXT2_S_IFBLK | ");
    if (i_mode & 0x4000)
        fprintf_blue(stdout, "EXT2_S_IFDIR | ");
    if (i_mode & 0x2000)
        fprintf_blue(stdout, "EXT2_S_IFCHR | ");
    if (i_mode & 0x1000)
        fprintf_blue(stdout, "EXT2_S_IFIFO | ");

    /* process execution/group override */
    if (i_mode & 0x0800)
        fprintf_blue(stdout, "EXT2_S_ISUID | ");
    if (i_mode & 0x0400)
        fprintf_blue(stdout, "EXT2_S_ISGID | ");
    if (i_mode & 0x0200)
        fprintf_blue(stdout, "EXT2_S_ISVTX | ");

    /* access control */
    if (i_mode & 0x0100)
        fprintf_blue(stdout, "EXT2_S_IRUSR | ");
    if (i_mode & 0x0080)
        fprintf_blue(stdout, "EXT2_S_IWUSR | ");
    if (i_mode & 0x0040)
        fprintf_blue(stdout, "EXT2_S_IXUSR | ");
    if (i_mode & 0x0020)
        fprintf_blue(stdout, "EXT2_S_IRGRP | ");
    if (i_mode & 0x0010)
        fprintf_blue(stdout, "EXT2_S_IWGRP | ");
    if (i_mode & 0x0008)
        fprintf_blue(stdout, "EXT2_S_IXGRP | ");
    if (i_mode & 0x0004)
        fprintf_blue(stdout, "EXT2_S_IROTH | ");
    if (i_mode & 0x0002)
        fprintf_blue(stdout, "EXT2_S_IWOTH | ");
    if (i_mode & 0x0001)
        fprintf_blue(stdout, "EXT2_S_IXOTH | ");


    fprintf_yellow(stdout, "\b\b )\n");
    return 0;
}

int print_inode_flags(uint16_t i_flags)
{
    fprintf_yellow(stdout, "\t(  ");
    if (i_flags & 0x1)
        fprintf_blue(stdout, "EXT2_SECRM_FL | ");
    if (i_flags & 0x2)
        fprintf_blue(stdout, "EXT2_UNRM_FL | ");    
    if (i_flags & 0x4)
        fprintf_blue(stdout, "EXT2_COMPR_FL | ");
    if (i_flags & 0x8)
        fprintf_blue(stdout, "EXT2_SYNC_FL | ");

    /* compression */
    if (i_flags & 0x10)
        fprintf_blue(stdout, "EXT2_IMMUTABLE_FL | ");
    if (i_flags & 0x20)
        fprintf_blue(stdout, "EXT2_APPEND_FL | ");
    if (i_flags & 0x40)
        fprintf_blue(stdout, "EXT2_NODUMP_FL | ");
    if (i_flags & 0x80)
        fprintf_blue(stdout, "EXT2_NOATIME_FL | ");

    if (i_flags & 0x100)
        fprintf_blue(stdout, "EXT2_DIRTY_FL | ");
    if (i_flags & 0x200)
        fprintf_blue(stdout, "EXT2_COMPRBLK_FL | ");
    if (i_flags & 0x400)
        fprintf_blue(stdout, "EXT2_NOCOMPR_FL | ");
    if (i_flags & 0x800)
        fprintf_blue(stdout, "EXT2_ECOMPR_FL | ");

    if (i_flags & 0x1000)
        fprintf_blue(stdout, "EXT2_BTREE_FL | ");
    if (i_flags & 0x2000)
        fprintf_blue(stdout, "EXT2_INDEX_FL | ");
    if (i_flags & 0x4000)
        fprintf_blue(stdout, "EXT2_IMAGIC_FL | ");
    if (i_flags & 0x8000)
        fprintf_blue(stdout, "EXT3_JOURNAL_DATA_FL | ");

    if (i_flags & 0x80000000)
        fprintf_blue(stdout, "EXT2_RESERVED_FL | ");

   fprintf_yellow(stdout, "\b\b )\n");
   return 0;
}

int print_ext2_inode(struct ext2_inode inode)
{
    fprintf_yellow(stdout, "i_mode: 0x%"PRIx16"\n",
                           inode.i_mode);
    print_inode_mode(inode.i_mode);
    fprintf_yellow(stdout, "i_uid: %"PRIu16"\n",
                           inode.i_uid);
    fprintf_yellow(stdout, "i_size: %"PRIu32"\n",
                           inode.i_size);
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
                           inode.i_blocks);
    fprintf_yellow(stdout, "i_flags: %"PRIu32"\n",
                           inode.i_flags);
    print_inode_flags(inode.i_flags);
    fprintf_yellow(stdout, "i_osd1: %"PRIu32"\n",
                           inode.i_osd1);
    fprintf_yellow(stdout, "i_block: %"PRIu32"\n",
                           inode.i_block[0]); /* uint32_t i_block[15]; */
    fprintf_yellow(stdout, "i_generation: %"PRIu32"\n",
                           inode.i_generation);
    fprintf_yellow(stdout, "i_file_acl: 0%.3"PRIo32"\n",
                           inode.i_file_acl);
    fprintf_yellow(stdout, "i_dir_acl: 0%.3"PRIo32"\n",
                           inode.i_dir_acl);
    fprintf_yellow(stdout, "i_faddr: %"PRIu32"\n",
                           inode.i_faddr);
    /* uint8_t i_osd2[8] */
    return 0;
}

int print_ext2_block_group_descriptor(struct ext2_block_group_descriptor bgd)
{
    fprintf_yellow(stdout, "bg_block_bitmap: %"PRIu32"\n",
                           bgd.bg_block_bitmap);
    fprintf_yellow(stdout, "bg_inode_bitmap: %"PRIu32"\n",
                           bgd.bg_inode_bitmap);
    fprintf_yellow(stdout, "bg_inode_table: %"PRIu32"\n",
                           bgd.bg_inode_table);
    fprintf_yellow(stdout, "bg_free_blocks_count: %"PRIu16"\n",
                           bgd.bg_free_blocks_count);
    fprintf_yellow(stdout, "bg_free_inodes_count: %"PRIu16"\n",
                           bgd.bg_free_inodes_count);
    fprintf_yellow(stdout, "bg_used_dirs_count: %"PRIu16"\n",
                           bgd.bg_used_dirs_count);
    /* uint16_t bg_pad; */
    /* uint8_t bg_reserved[12]; */
    return 0;
}

int print_ext2_superblock(struct ext2_superblock superblock)
{
    fprintf_yellow(stdout, "s_inodes_count: %"PRIu32"\n",
                           superblock.s_inodes_count);
    fprintf_yellow(stdout, "s_blocks_count: %"PRIu32"\n",
                           superblock.s_blocks_count);
    fprintf_yellow(stdout, "s_r_blocks_count: %"PRIu32"\n",
                           superblock.s_r_blocks_count);
    fprintf_yellow(stdout, "s_free_blocks_count: %"PRIu32"\n",
                           superblock.s_free_blocks_count);
    fprintf_yellow(stdout, "s_free_inodes_count: %"PRIu32"\n",
                           superblock.s_free_inodes_count);
    fprintf_yellow(stdout, "s_first_data_block: %"PRIu32"\n",
                           superblock.s_first_data_block);
    fprintf_yellow(stdout, "s_log_block_size: %"PRIu32"\n",
                           superblock.s_log_block_size);
    fprintf_yellow(stdout, "s_log_frag_size: %"PRIu32"\n",
                           superblock.s_log_frag_size);
    fprintf_yellow(stdout, "s_blocks_per_group: %"PRIu32"\n",
                           superblock.s_blocks_per_group);
    fprintf_yellow(stdout, "s_frags_per_group: %"PRIu32"\n",
                           superblock.s_frags_per_group);
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
        fprintf_light_green(stdout, "Magic value matches EXT_SUPER_MAGIC\n"); 
    }
    else
    {
        fprintf_light_red(stdout, "Magic value does not match EXT_SUPER_MAGIC\n");
    }
    fprintf_yellow(stdout, "s_state: %"PRIu16"\n",
                           superblock.s_state);
    fprintf_light_yellow(stdout, "File System State: %s\n",
                                 s_state_LUT[superblock.s_state]);
    fprintf_yellow(stdout, "s_errors: %"PRIu16"\n",
                           superblock.s_errors);
    fprintf_light_yellow(stdout, "Error State: %s\n",
                                 s_errors_LUT[superblock.s_state]);
    fprintf_yellow(stdout, "s_minor_rev_level: %"PRIu16"\n",
                           superblock.s_minor_rev_level);
    fprintf_yellow(stdout, "s_lastcheck: %"PRIu32"\n",
                           superblock.s_lastcheck);
    fprintf_yellow(stdout, "s_checkinterval: %"PRIu32"\n",
                           superblock.s_checkinterval);
    fprintf_yellow(stdout, "s_creator_os: %"PRIu32"\n",
                           superblock.s_creator_os);
    fprintf_light_yellow(stdout, "Resolved OS: %s\n",
                                 s_creator_os_LUT[superblock.s_creator_os]);
    fprintf_yellow(stdout, "s_rev_level: %"PRIu32"\n",
                           superblock.s_rev_level);
    fprintf_light_yellow(stdout, "Revision Level: %s\n",
                                 s_rev_level_LUT[superblock.s_rev_level]);
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
                           superblock.s_algo_bitmap);
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
                           superblock.s_default_mount_options);
    fprintf_yellow(stdout, "s_first_meta_bg: %"PRIu32"\n",
                           superblock.s_first_meta_bg);
    return 0;
}