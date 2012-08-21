#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbr.h"
#include "ext4.h"

int main(int argc, char* argv[])
{
    FILE* disk;
    struct mbr mbr;
    struct ext4_superblock superblock;
    struct ext4_inode inode;
    int64_t partition_offset;
    char buf[SECTOR_SIZE];
    uint64_t inode_num;
    int i;

    fprintf_blue(stdout, "File System inode Explorer -- By: Wolfgang Richter "
                         "<wolf@cs.cmu.edu>\n");

    if (argc < 2)
    {
        fprintf_light_red(stderr, "Usage: %s <raw disk file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fprintf_cyan(stdout, "Analyzing Disk: %s\n\n", argv[1]);

    disk = fopen(argv[1], "r");
    
    if (disk == NULL)
    {
        fprintf_light_red(stderr, "Error opening raw disk file. "
                                  "Does it exist?\n");
        return EXIT_FAILURE;
    }

    if (parse_mbr(disk, &mbr))
    {
        fprintf_light_red(stdout, "Error reading MBR from disk.  Aborting\n");
        return EXIT_FAILURE;
    }

    for (i = 0; i < 4; i++)
    {
        if ((partition_offset = mbr_partition_offset(mbr, i)) >= 0)
        {
            if (ext4_probe(disk, partition_offset, &superblock))
            {
                fprintf_light_red(stderr, "ext4 probe failed.\n");
                continue;
            }
            else
            {
                fprintf_light_green(stdout, "--- Found ext4 Partition at "
                                            "Offset 0x%.16"PRIx64" ---\n",
                                            partition_offset);
                fprintf(stdout, "Would you like to explore this partition "
                                "[y/n]? ");
                fscanf(stdin, "%c", buf);
                if (buf[0] == 'y' || buf[0] == 'Y')
                {
                    while (1)
                    {
                        fprintf_light_blue(stdout, "> ");
                        fscanf(stdin, "%s", buf);
                        if(strcmp(buf, "show") == 0)
                        {
                            fscanf(stdin, "%"PRIu64, &inode_num);
                            fprintf_cyan(stdout, "Examining inode: %"PRIu64
                                                 ".\n", inode_num);
                            ext4_read_inode(disk, partition_offset, superblock,
                                            inode_num, &inode);
                            ext4_print_inode(inode);
                        }
                        if (strcmp(buf, "exit") == 0)
                        {
                            fprintf_cyan(stdout, "Goodbye.\n");
                            fclose(disk);
                            return EXIT_SUCCESS; 
                        }
                    }
                }
            }
        }
    }

    fclose(disk);

    return EXIT_SUCCESS;
}
