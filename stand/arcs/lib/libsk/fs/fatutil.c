#include <arcs/types.h>
#include <sys/fat.h>
#include <libsc.h>
#include <libsk.h>

size_t strcspn (const char * string, const char * control)
    {
    unsigned char map[32];
    short count;

/*
 *                      Clear out bit map
 */

    for (count=0; count<32; count++)
	map[count] = 0;

/*
 *                      Set bits in control map
 */

    while (*control)
	{
	map[*control >> 3] |= (1 << (*control & 7));
	control++;
	}

/*
 *                      1st char in control map stops search
 */
    count=0;
    map[0] |= 1;	/* null chars not considered */
    while (!(map[*string >> 3] & (1 << (*string & 7))))
	{
	count++;
	string++;
	}
    return(count);
    }

/*********
    Input : char *
    Output : unsigned short int
    Purpose : Convert a string of characters to a short integer
 *********/
USHORT stosi(UCHAR *buf)
{
   USHORT rc;
   rc = buf[0] | (buf[1] << 8); 
   return (rc);
}

/*********
    Input : char *
    Output : unsigned int
    Purpose : Convert a string of characters to an integer
 *********/
ULONG stoi(buf)
char *buf;
{
   ULONG rc;
   rc = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
   return (rc);
}

   
#ifdef	notdef
/*********
    Input : char *
    Output : unsigned short integer
    Purpose : Convert a characters to a short integer
 *********/
USHORT ctos(buf)
char *buf;
{
   USHORT rc = 0;
   rc = (USHORT)buf[0];
   return (rc);
}
#endif	/* notdef */

VOID ParseBpb(BPB *param_block, UCHAR *buffer)
{
   /* fill structure */
   param_block->BytesPerSector = stosi(&buffer[BYTES_PER_SECTOR]);
   param_block->SectorsPerCluster = buffer[SECTOR_PER_CLUSTER];
   param_block->ReservedSectors = stosi(&buffer[RESERVED_SECTOR]);
   param_block->FatCount = buffer[FAT_COUNT];
   param_block->RootDirEntries = stosi(&buffer[ROOTDIR_ENTRIES]);
   param_block->TotalSectors = stosi(&buffer[TOTAL_SECTORS]);
   param_block->MediaDesc = buffer[MEDIA_DESC];
   param_block->SectorsPerFat = stosi(&buffer[SECTORS_PER_FAT]);
   param_block->SectorsPerTrack = stosi(&buffer[SECTORS_PER_TRACK]);
   param_block->HeadCount = stosi(&buffer[HEAD_COUNT]);
   param_block->HiddenSectors = stosi(&buffer[HIDDEN_SECTORS]);
}

#ifdef	FATDEBUG
VOID PrintBpb(BPB *info)
{
   printf("\nBIOS PARAMETER BLOCK\n");
   printf("Bytes Per Sector = %d\n", (int)info->BytesPerSector);
   printf("Sectors Per Cluster = %d\n", (int)info->SectorsPerCluster);
   printf("Reversed Sectors = %d\n", (int)info->ReservedSectors);
   printf("Number of FATs = %d\n", (int)info->FatCount);
   printf("Number of Root Directory Entries = %d\n", (int)info->RootDirEntries);
   printf("Total Number of Sectors = %d\n", (int)info->TotalSectors);
   printf("Media Descriptor Byte = %x\n", info->MediaDesc);
   printf("Number of Sectors per FAT = %d\n", (int)info->SectorsPerFat);
   printf("Number of Sectors per Track = %d\n", (int)info->SectorsPerTrack);
   printf("Number of Heads = %d\n", (int)info->HeadCount);
   printf("Number of Hidden Sectors = %d\n", (int)info->HiddenSectors);
}

VOID PrintInfo(OPENINFO *info)
{
   printf("\nOPEN FILE INFORMATION\n");
   printf("File ID %d\n", info->Fid);
   printf("Bytes Per Sector %d\n", info->BytesPerSector);
   printf("Sectors Per Cluster %d\n", info->SectorsPerCluster);
   printf("File Data Offset %d\n", info->DataOffset);
   printf("Fat Type %d\n", info->FatType);
   printf("Fat Start %d\n", info->FatStart);
   printf("Starting Cluster %x\n", info->StartingCluster);
   printf("File Length %x\n", info->FileLength);
   printf("Attribute Byte %x\n", info->Attribute);
}
#endif	/* FATDEBUG */
