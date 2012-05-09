#define _FILE_OFFSET_BITS 64

#include "color.h"
#include "ntfs.h"

#include <errno.h>
#include <iconv.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define SECTOR_SIZE 512

wchar_t* ignore_files[] = { L"$MFT",
                            L"$MFTMirr",
                            L"$LogFile",
                            L"$Volume",
                            L"$AttrDef",
                            L"$Bitmap",
                            L"$Boot",
                            L"$BadClus",
                            L"$Quota",
                            L"$Secure",
                            L"$UpCase",
                            L"$Extend",
                            L"$ObjId",
                            L"$Reparse",
                            L"$UsnJrnl",
                            L"$Repair",
                            L"$Tops",
                            L"$Config",
                            L"$Delete",
                            L"$ObjId",
                            L"$Quota",
                            L"$Repair.log",
                            L"$RmMetadata",
                            L"$Txf",
                            L"$TxfLog",
                            L"$TXFLO~1",
                            L"$TXFLO~2",
                            NULL 
                          };

bool ntfs_ignore_file(wchar_t* fname)
{
    int i = 0;
    while (ignore_files[i])
    {
        if (wcscmp(ignore_files[i++], fname) == 0)
            return true;
    }
    return false;
}

int ntfs_print_standard_attribute_header(struct ntfs_standard_attribute_header* sah)
{
    fprintf_yellow(stdout, "sah.attribute_type: 0x%"PRIx32"\n", sah->attribute_type);
    fprintf_yellow(stdout, "sah.length: %"PRIu32"\n", sah->length);
    fprintf_yellow(stdout, "sah.non_resident_flag: 0x%"PRIx8"\n", sah->non_resident_flag);
    fprintf_yellow(stdout, "sah.name_length: %"PRIu8"\n", sah->name_length);
    fprintf_yellow(stdout, "sah.name_offset: %"PRIu16"\n", sah->name_offset);
    fprintf_yellow(stdout, "sah.flags: 0x%"PRIx16"\n", sah->flags);
    fprintf_yellow(stdout, "sah.attribute_id: %"PRIu16"\n", sah->attribute_id);
    fprintf_yellow(stdout, "sah.length_of_attribute: %"PRIu32"\n", sah->length_of_attribute);
    fprintf_yellow(stdout, "sah.offset_of_attribute: %"PRIu16"\n", sah->offset_of_attribute);
    fprintf_yellow(stdout, "sah.indexed_flag: 0x%"PRIx8"\n", sah->indexed_flag);
    return EXIT_SUCCESS;
}

int ntfs_print_standard_information(struct ntfs_standard_information* si)
{
    fprintf_yellow(stdout, "c_time: %"PRIu64"\n", si->c_time);
    fprintf_yellow(stdout, "a_time: %"PRIu64"\n", si->a_time);
    fprintf_yellow(stdout, "m_time: %"PRIu64"\n", si->m_time);
    fprintf_yellow(stdout, "r_time: %"PRIu64"\n", si->r_time);
    return EXIT_SUCCESS;
}

int ntfs_print_file_record(struct ntfs_file_record* rec)
{
    uint8_t* magic = (uint8_t*) &(rec->magic);
    fprintf_light_blue(stdout, "file_record.magic: %.4s\n", magic);
    fprintf_yellow(stdout, "file_record.offset_update_seq: %"PRIu16"\n", rec->offset_update_seq);
    fprintf_yellow(stdout, "file_record.size_usn: %"PRIu16"\n", rec->size_usn);
    fprintf_yellow(stdout, "file_record.lsn: %"PRIu64"\n", rec->lsn);
    fprintf_yellow(stdout, "file_record.seq_num: %"PRIu16"\n", rec->seq_num);
    fprintf_yellow(stdout, "file_record.hard_link_count: %"PRIu16"\n", rec->hard_link_count);
    fprintf_yellow(stdout, "file_record.offset_first_attributes: %"PRIu16"\n", rec->offset_first_attribute);
    fprintf_yellow(stdout, "file_record.flags: %"PRIx16"\n", rec->flags);
    fprintf_yellow(stdout, "file_record.real_size: %"PRIu16"\n", rec->real_size);
    fprintf_yellow(stdout, "file_record.allocated_size: %"PRIu16"\n", rec->allocated_size);
    fprintf_yellow(stdout, "file_record.file_ref_base: %"PRIu16"\n", rec->file_ref_base);
    fprintf_yellow(stdout, "file_record.next_attr_id: %"PRIu16"\n", rec->next_attr_id);
    fprintf_light_yellow(stdout, "file_record.rec_num: %"PRIu32"\n", rec->rec_num);
    return EXIT_SUCCESS;
}

uint64_t ntfs_lcn_to_offset(struct ntfs_boot_file* bootf, int64_t partition_offset,
                            uint64_t lcn)
{
    uint64_t bytes_per_cluster = bootf->bytes_per_sector *
                                 bootf->sectors_per_cluster;
    return (lcn*bytes_per_cluster) + partition_offset;
}

int ntfs_print_boot_file(struct ntfs_boot_file* bootf, int64_t partition_offset)
{
    fprintf_light_blue(stdout, "-- Analyzing Boot File $Boot --\n");
    fprintf_yellow(stdout, "boot->sys_id: %4s\n", bootf->sys_id);
    fprintf_yellow(stdout, "boot->bytes_per_sector: %"PRIu16"\n",
                           bootf->bytes_per_sector);
    fprintf_yellow(stdout, "boot->sectors_per_cluster: %"PRIu8"\n",
                           bootf->sectors_per_cluster);
    fprintf_light_yellow(stdout, "boot->media: 0x%"PRIx8"\n", bootf->media);
    fprintf_yellow(stdout, "boot->sectors_per_track: %"PRIu16"\n", bootf->sectors_per_track);
    fprintf_yellow(stdout, "boot->number_of_heads: %"PRIu16"\n", bootf->number_of_heads);
    fprintf_yellow(stdout, "boot->signature: 0x%"PRIx32"\n", bootf->signature);
    fprintf_yellow(stdout, "boot->sectors_in_volume: %"PRIu64"\n", bootf->sectors_in_volume);
    fprintf_light_yellow(stdout, "boot->lcn_mft: %"PRIu64"\n", bootf->lcn_mft);
    fprintf_green(stdout, "\tlcn_mft offset: %"PRIu64"\n", ntfs_lcn_to_offset(bootf, partition_offset, bootf->lcn_mft));
    fprintf_light_yellow(stdout, "boot->lcn_mftmirr: %"PRIu64"\n", bootf->lcn_mftmirr);
    fprintf_green(stdout, "\tlcn_mft offset: %"PRIu64"\n", ntfs_lcn_to_offset(bootf, partition_offset, bootf->lcn_mftmirr));
    fprintf_yellow(stdout, "boot->clusters_per_mft: %"PRIu32"\n", bootf->clusters_per_mft);
    fprintf_yellow(stdout, "boot->volume_serial: %"PRIu32"\n", bootf->volume_serial);
    
    return EXIT_SUCCESS;
}

int ntfs_probe(FILE* disk, int64_t partition_offset,
               struct ntfs_boot_file* bootf)
{
    if (fseeko(disk, partition_offset, SEEK_SET))
    {
        fprintf_light_red(stderr, "Error seeking to partition offset and $MFT "
                                  "position while NTFS probing.\n");
        return EXIT_FAILURE;
    }

    if (fread(bootf, 1, sizeof(*bootf), disk) != sizeof(*bootf))
    {
        fprintf_light_red(stderr, "Error reading FILE Record.\n");
        return EXIT_FAILURE;
    }

    if (strncmp((char*) bootf->sys_id, "NTFS", 4) != 0)
    {
        fprintf_light_red(stderr, "NTFS probe failed.\n");
        return EXIT_FAILURE;
    }

    ntfs_print_boot_file(bootf, partition_offset);

    return EXIT_SUCCESS;
}

int ntfs_read_file_record(FILE* disk, struct ntfs_boot_file* bootf,
                          int64_t partition_offset,
                          struct ntfs_file_record* rec, uint64_t record_num)
{
    //if (record_num > 10)
    //    return -1; /* don't support arbitrary records yet */

    uint64_t offset = ntfs_lcn_to_offset(bootf, partition_offset, bootf->lcn_mft);

    if (fseeko(disk, ntfs_lcn_to_offset(bootf, partition_offset, bootf->lcn_mft), SEEK_SET))
    {
        fprintf_light_red(stderr, "Error seeking to partition offset and $MFT "
                                  "position while NTFS probing.\n");
        return -1;
    }

    if (fread(rec, 1, sizeof(*rec), disk) != sizeof(*rec))
    {
        fprintf_light_red(stderr, "Error reading FILE Record.\n");
        return -1;
    }

    offset += rec->allocated_size * record_num;

    if (fseeko(disk, offset, SEEK_SET))
    {
        fprintf_light_red(stderr, "Error seeking to partition offset and $MFT "
                                  "position while NTFS probing.\n");
        return -1;
    }

    if (fread(rec, 1, sizeof(*rec), disk) != sizeof(*rec))
    {
        fprintf_light_red(stderr, "Error reading FILE Record.\n");
        return -1;
    }

    if (strncmp((char*) &(rec->magic), "FILE", 4) != 0)
    {
        fprintf_light_cyan(stdout, "Reached end of MFT, not FILE magic.\n");
        return -1;
    }
    ntfs_print_file_record(rec);

    return 1;
}

int ntfs_print_file_names(struct ntfs_file_name* rec)
{
    fprintf_yellow(stdout, "rec->c_time: %"PRIu64"\n", rec->c_time);
    fprintf_yellow(stdout, "rec->a_time: %"PRIu64"\n", rec->a_time);
    fprintf_yellow(stdout, "rec->m_time: %"PRIu64"\n", rec->m_time);
    fprintf_yellow(stdout, "rec->r_time: %"PRIu64"\n", rec->r_time);
    fprintf_yellow(stdout, "rec->allocated_size: %"PRIu64"\n", rec->allocated_size);
    fprintf_yellow(stdout, "rec->real_size: %"PRIu64"\n", rec->real_size);
    fprintf_yellow(stdout, "rec->flags: %"PRIx32"\n", rec->flags);
    fprintf_yellow(stdout, "rec->name_len: %"PRIu8"\n", rec->name_len);
    return EXIT_SUCCESS;
}

int ntfs_print_file_name(FILE* disk, struct ntfs_boot_file* bootf,
                         int64_t partition_offset,
                         struct ntfs_file_record* rec)
{
    struct ntfs_standard_attribute_header sah;
    uint64_t offset = ntfs_lcn_to_offset(bootf, partition_offset, bootf->lcn_mft);
    struct ntfs_file_name fname;
    wchar_t file_name[512];
    wchar_t* file_namep = file_name;
    wchar_t** file_namepp = &file_namep;
    char file_name_encoded[512];
    char* file_name_encodedp = file_name_encoded;
    char** file_name_encodedpp = &file_name_encodedp;
    iconv_t cd = iconv_open("WCHAR_T", "UTF-16");
    size_t outbytes = 1024;
    size_t inbytes; 

    offset += (rec->allocated_size) * (rec->rec_num); /* at _least_ win xp needed */
    offset += (rec->offset_first_attribute); /* skip to first attr */

    memset(file_name, 0, 512);
    memset(file_name_encoded, 0, 512);
    
    if (cd < 0)
    {
        fprintf_light_red(stderr, "Error creating conversion struct.\n");
        return -1;
    } 

    if (fseeko(disk, offset, SEEK_SET))
    {
        fprintf_light_red(stderr, "Error seeking to partition offset and $MFT "
                                  "position while NTFS probing.\n");
        return -1;
    }

    if (fread(&sah, 1, sizeof(sah), disk) != sizeof(sah))
    {
        fprintf_light_red(stderr, "Error reading FILE Record.\n");
        return -1;
    }

    ntfs_print_standard_attribute_header(&sah);

    offset += sah.length;

    if (fseeko(disk, offset, SEEK_SET))
    {
        fprintf_light_red(stderr, "Error seeking to partition offset and $MFT "
                                  "position while NTFS probing.\n");
        return -1;
    }

    if (fread(&sah, 1, sizeof(sah), disk) != sizeof(sah))
    {
        fprintf_light_red(stderr, "Error reading FILE Record.\n");
        return -1;
    }

    ntfs_print_standard_attribute_header(&sah);

    if (sah.attribute_type == 0x30) /* file name */
    {
        if (fseeko(disk, sah.offset_of_attribute - sizeof(sah), SEEK_CUR))
        {
            fprintf_light_red(stderr, "Error seeking to offset while pulling"
                                      "file_name attribute.\n");
            return -1;
        }

        if (fread(&fname, 1, sizeof(fname), disk) != sizeof(fname))
        {
            fprintf_light_red(stderr, "Error reading file name struct.\n");
            return -1;
        }

        ntfs_print_file_names(&fname);

        if (fread(file_name_encoded, 2, fname.name_len, disk) != fname.name_len)
        {
            fprintf_light_red(stderr, "Error reading the file name string.\n");
            return -1;
        }

        file_name_encodedp = file_name_encoded + 1; /* skip namespace */
        file_name_encodedpp = &file_name_encodedp;
        inbytes = 2*fname.name_len;

        if (iconv(cd, (char**) file_name_encodedpp, &inbytes, (char**) file_namepp, &outbytes) == (size_t) -1)
        {
            fprintf_light_red(stderr, "bytes: %x %x %x %x %x %x %x %x\n",
                                      file_name_encodedp[0],
                                      file_name_encodedp[1],
                                      file_name_encodedp[2],
                                      file_name_encodedp[3],
                                      file_name_encodedp[4],
                                      file_name_encodedp[5],
                                      file_name_encodedp[6],
                                      file_name_encodedp[7]
                                      );

            fprintf_light_red(stderr, "Error converting to wchar_t.\n");
            switch (errno)
            {
                case E2BIG:
                    fprintf_light_red(stderr, "There is not sufficient room at *outbuf\n");
                    break;
                case EILSEQ:
                    fprintf_light_red(stderr, "An invalid multibyte sequence has been encountered in the input.\n");
                    break;
                case EINVAL:
                    fprintf_light_red(stderr, "An incomplete multibyte sequence has been encountered in the input.\n");
                    break;
                default:
                    fprintf_light_red(stderr, "An unknown iconv error was encountered.\n");
            };

            return -1;
        }
        else
        {
            fprintf_light_cyan(stdout, "found fname[len=%d]: %ls\n", wcslen(file_name), file_name);
            if (ntfs_ignore_file(file_name))
            {
                fprintf(stdout, "ignoring file.\n");
                iconv_close(cd);
                return EXIT_SUCCESS;
            }
        }
    }
    iconv_close(cd);

    offset += sah.length;

    if (fseeko(disk, offset, SEEK_SET))
    {
        fprintf_light_red(stderr, "Error seeking to partition offset and $MFT "
                                  "position while NTFS probing.\n");
        return -1;
    }

    if (fread(&sah, 1, sizeof(sah), disk) != sizeof(sah))
    {
        fprintf_light_red(stderr, "Error reading FILE Record.\n");
        return -1;
    }

    ntfs_print_standard_attribute_header(&sah);

    offset += sah.length;

    if (fseeko(disk, offset, SEEK_SET))
    {
        fprintf_light_red(stderr, "Error seeking to partition offset and $MFT "
                                  "position while NTFS probing.\n");
        return -1;
    }

    if (fread(&sah, 1, sizeof(sah), disk) != sizeof(sah))
    {
        fprintf_light_red(stderr, "Error reading FILE Record.\n");
        return -1;
    }

    ntfs_print_standard_attribute_header(&sah);
    if (sah.attribute_type == 0x80) /* file name */
    {
        fprintf_light_blue(stdout, "File DATA\n");
        exit(0);
        return EXIT_SUCCESS;
    }
    fprintf_light_red(stdout, "NO FILE DATA!?\n");
    return EXIT_SUCCESS;
}

int ntfs_walk_mft(FILE* disk, struct ntfs_boot_file* bootf,
                  int64_t partition_offset)
{
    struct ntfs_file_record rec;
    uint64_t num = 0;
    while (ntfs_read_file_record(disk, bootf, partition_offset, &rec, num) > 0)
    {
        if (!(rec.flags & 0x02))
        {
            ntfs_print_file_name(disk, bootf, partition_offset, &rec);
            //return 0;
        }
        num++;
    }
    return EXIT_SUCCESS;
}
