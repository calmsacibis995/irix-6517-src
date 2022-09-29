#ident	"$Revision: 1.17 $"

/*
 * fat.c - FAT filesystem
 */

#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/fs.h>
#include <arcs/dirent.h>
#include <sys/fat.h>
#include <saio.h>
#include <libsc.h>
#include <libsk.h>
#include <string.h>

/****************************************************************************
 ***************************************************************************/

/* temporary buffer */
static UCHAR *buffer;

/* buffer current fat sector */
static UCHAR *fatbuf;

static LONG lastfid;

extern STATUS FatStrategy(FSBLOCK *Fsb);
extern LONG Find(FSBLOCK *Fsb, BPB *params, OPENINFO *info);
extern LONG ValidateFat(FSBLOCK *Fsb, BPB *params, OPENINFO *info);
extern LONG FatCheck(FSBLOCK *Fsb);
extern LONG FatClose(FSBLOCK *Fsb);
extern LONG FatGetDents(FSBLOCK *Fsb);
extern LONG FatGetReadStatus(FSBLOCK *Fsb);
extern LONG FatRead(FSBLOCK *Fsb);
extern LONG FatOpen(FSBLOCK *Fsb);
extern LONG FatInit();

#define TRUE 1
#define FALSE 0
#define FAILURE -1
#define LBUFFERSZ 2
#define SECTORSZ 512

#define GetFileDesc(f)	((OPENINFO *)((f)->FsPtr))

#define LASTCLUSTER(type, cluster) (((type != DOS12) && (cluster == 0xfff)) ||\
     ((type == DOS12) && ((cluster & 0xff8) == 0xff8)) ? TRUE : FALSE)

/****************************************************************************

   Function : FatInstall

   Description :
       This function tries to initialize the file system.  If the
       initialization is successful then the file system is registered
       with the system executive.

 ***************************************************************************/

LONG FatInstall(VOID)
{
    LONG rc;
    if ((rc = FatInit()) == ESUCCESS)
	rc = fs_register(FatStrategy, "fat", DTFS_FAT);

    return (rc);
}

/****************************************************************************

   Function : FatStrategy

   Description :

 ***************************************************************************/

STATUS FatStrategy(FSBLOCK *Fsb)
{
    STATUS rc;

    switch (Fsb->FunctionCode) {
    case FS_OPEN:
	rc = FatOpen(Fsb);
	break;
    case FS_CLOSE:
	rc = FatClose(Fsb);
	break;
    case FS_READ:
	rc = FatRead(Fsb);
	break;
    case FS_CHECK:
	rc = FatCheck(Fsb);
	break;
    case FS_GETDIRENT:
	rc = FatGetDents(Fsb);
	break;
    case FS_GETREADSTATUS:
	rc = FatGetReadStatus(Fsb);
	break;
    default:
	rc = EINVAL;
	break;
    }

    return (rc);
}

/****************************************************************************

   Function : NextCluster

   Description :

 ***************************************************************************/
static LONG NextCluster(FSBLOCK *Fsb, OPENINFO *thisfile, USHORT *cluster)
{
    USHORT nextcluster;
    static USHORT lastsector;
    USHORT sector;

    /* Check if at the end of cluster chain (shouldn't happen). */
    if (LASTCLUSTER(thisfile->FatType, *cluster) == TRUE)
	return ESUCCESS; /* do nothing */

    /* Compute the sector offset into FAT */
    if (thisfile->FatType == DOS12)
	/* 12 bit FAT -> offset = 1.5 * cluster / bytes per sector */
	sector = (*cluster + (*cluster / 2)) / SECTORSZ;
    else
	/* 16 bit FAT -> offset = 2 * cluster / bytes per sector */
	sector = (*cluster * 2) / SECTORSZ;

    /* Only read FAT if different open file or if not in the last
      sector read */
    if (((lastfid == (LONG)Fsb->FsPtr) && (lastsector != sector)) ||
        (lastfid != (LONG)Fsb->FsPtr)) {
	lastfid = (LONG)Fsb->FsPtr;
	lastsector = sector;

	/* set up for the read */
	Fsb->IO->FunctionCode = FC_READ;
	Fsb->IO->StartBlock = thisfile->FatStart + sector;
	Fsb->IO->Count = LBUFFERSZ * SECTORSZ;
	Fsb->IO->Address = fatbuf;

#ifdef FATDEBUG
	printf("Reading FAT sector %d Cluster %d\\n", sector, *cluster);
#endif         
	/* do the read */
	if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
#ifdef FATDEBUG
	    printf("IO Error Reading FAT\n");
#endif
	    return !ESUCCESS;
	}

    }

    if (thisfile->FatType == DOS12) {
	nextcluster = stosi(fatbuf + ((*cluster + (*cluster / 2)) % SECTORSZ));

	/* If cluster is an even number keep low order 12 bits, otherwise
         * keep high order 12 bits. 
	 */
	if (*cluster & 0x0001)
	    nextcluster = (nextcluster & 0xfff0) >> 4;
	else
	    nextcluster = (nextcluster & 0x0fff);
    } else
	nextcluster = stosi(fatbuf + (*cluster % 256) * 2);

    *cluster = nextcluster;
    return ESUCCESS;
}

/****************************************************************************

   Function : FatInit

   Description :
      This function initializes local buffers.

 ***************************************************************************/

LONG FatInit()
{
#ifdef FATDEBUG
    printf("fatinit\n");
#endif

    /* allocate word aligned buffers */
    if (!(buffer = (UCHAR *)dmabuf_malloc(LBUFFERSZ * SECTORSZ)))
	return ENOMEM;

    /* allocate word aligned buffers */
    if (!(fatbuf = (UCHAR *)dmabuf_malloc(LBUFFERSZ * SECTORSZ)))
	return ENOMEM;

    return ESUCCESS;
}

/****************************************************************************

   Function : fatopen

   Description :

 ***************************************************************************/

LONG FatOpen(FSBLOCK *Fsb)
{
    OPENINFO *thisfile;
    BPB params;

#ifdef FATDEBUG
    printf("FAT Open\n");
#endif                

    /* allocate memory to store open file information */
    if (!(Fsb->FsPtr = (void *)malloc(sizeof(OPENINFO))))
	return (Fsb->IO->ErrorNumber = EAGAIN);

    thisfile = (OPENINFO *)Fsb->FsPtr;

    /* HACK - get partition type from Count field of Fsb */
    thisfile->FatType = Fsb->Count;

    /* read the boot sector */
    Fsb->IO->FunctionCode = FC_READ;
    Fsb->IO->StartBlock = 0;
    Fsb->IO->Count = SECTORSZ;
    Fsb->IO->Address = buffer;

    if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
	/* try again, just in case there was a media change */
	Fsb->IO->Count = 1;
	if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
#ifdef FATDEBUG
	    printf("Error Reading Boot Sector\n");
#endif
	    FatClose(Fsb);
	    return EIO;
	}
    }

    /* parse bios parameter block */
    ParseBpb(&params, (UCHAR *)Fsb->IO->Address);

#ifdef FATDEBUG
    PrintBpb(&params);
#endif

    /* save information needed for later */
    thisfile->BytesPerSector = params.BytesPerSector;
    thisfile->SectorsPerCluster = params.SectorsPerCluster;
    thisfile->BytesPerCluster = params.SectorsPerCluster * SECTORSZ;
    thisfile->DataOffset = (USHORT)(params.ReservedSectors + 
        ((USHORT)params.FatCount * params.SectorsPerFat) + 
        ((params.RootDirEntries * DIRENTRYSZ) / SECTORSZ));

    /* validate FAT */
    if (ValidateFat(Fsb, &params, thisfile)) {
#ifdef FATDEBUG
	printf("Could Not Locate Valid FAT\n");
#endif
	FatClose(Fsb);
	return (Fsb->IO->ErrorNumber = ENODEV);
    }

    /* try to findthe requested file */
    if (Find(Fsb, &params, thisfile)) {
#ifdef FATDEBUG
	printf("Could Not Locate FAT file\n");
#endif
	FatClose(Fsb);
	return (Fsb->IO->ErrorNumber = ENOENT);
    }

#ifdef FATDEBUG
    PrintInfo(thisfile);
#endif
    return ESUCCESS;
}

/****************************************************************************

   Function : fatread

   Description :
       Need to add 64 bit offset support.

 ***************************************************************************/

LONG FatRead(FSBLOCK *Fsb)
{
    LONG rc = 0, to_read, i, offset, transfer_cnt;
    USHORT thiscluster;
    register OPENINFO *thisfile;
    Fsb->Buffer = Fsb->IO->Address;
    Fsb->Count = Fsb->IO->Count;


#ifdef FATDEBUG
    printf("FAT Read\n");
    printf("File Offset %d\n", Fsb->IO->Offset.lo);
#endif
    /* locate information associated with this file */
    if ((thisfile = GetFileDesc(Fsb)) == NULL) {
	Fsb->IO->ErrorNumber = EBADF;
	return FAILURE;
    }

    /* if file is a directory fail */
    if (thisfile->Attribute & DIRECTORY) {
	Fsb->IO->ErrorNumber = EINVAL;
	return FAILURE;
    }


    /* check if trying to read past eof */
    if (Fsb->IO->Offset.lo > thisfile->FileLength)
	return (rc); /* zero bytes read */

    /* only read to the eof */
    if ((Fsb->IO->Offset.lo + Fsb->Count) > thisfile->FileLength)
	Fsb->Count = thisfile->FileLength - Fsb->IO->Offset.lo;


    /* find the starting Sector of the file */
    /* start by locating the cluster */
    offset = Fsb->IO->Offset.lo / thisfile->BytesPerCluster;
    for (i = 0, thiscluster = thisfile->StartingCluster; i < offset; i++) {
	if (NextCluster(Fsb, thisfile, &thiscluster) != ESUCCESS) {
	    return FAILURE;
	}
    }

    /* read the remaining portion of this cluster */
    /* compute offset within cluster */
    offset = Fsb->IO->Offset.lo % thisfile->BytesPerCluster;
    if (offset) {
	/* compute starting sector in cluster */
	Fsb->IO->FunctionCode = FC_READ;
	Fsb->IO->StartBlock = thisfile->DataOffset +
	    ((thiscluster - 2) * thisfile->SectorsPerCluster);
	Fsb->IO->StartBlock += offset / SECTORSZ;
	Fsb->IO->Address = buffer;
	Fsb->IO->Count = SECTORSZ;

	to_read = thisfile->SectorsPerCluster - (offset / SECTORSZ);
	offset %= SECTORSZ;

	for (i = 0; i < to_read && Fsb->Count; i++) {
	    if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
#ifdef FATDEBUG
		printf("IO Error Reading Sector\n");
#endif
		return FAILURE;
	    }

	    transfer_cnt = (Fsb->Count < (SECTORSZ - offset) ?
	        Fsb->Count : (SECTORSZ - offset));

	    /* move data from temporary buffer to real buffer */
	    bcopy((VOID *)((CHAR *)(CHAR *)Fsb->IO->Address + offset), Fsb->Buffer, transfer_cnt);

	    /* update counters and buffer pointer to reflect characters read */
	    rc += transfer_cnt;
	    Fsb->Buffer += transfer_cnt;
	    Fsb->Count -= transfer_cnt;
	    Fsb->IO->StartBlock++; /* next sector please */
	    offset = 0;
	}

	/* done reading the from this cluster, get next cluster */
	if (NextCluster(Fsb, thisfile, &thiscluster) != ESUCCESS) {
	    return FAILURE;
	}

    }

    /* compute number of full clusters to read */
    to_read = Fsb->Count / thisfile->BytesPerCluster;

#ifdef FATDEBUG
    printf("Number of Characters Read %d\n", rc);
    printf("Clusters to read %d\n", to_read);
    if ((ULONG)Fsb->Buffer & 0x00000003L) {
	printf("Buffer Not Word Aligned\n");
    }
#endif
    /* need to make sure that the buffer is word aligned */
    /* do the cluster reads */
    while (to_read) {

#ifdef FATDEBUG
	printf("Thiscluster = %d\n", thiscluster);
#endif
	/* convert cluster to logical Sector */
	Fsb->IO->FunctionCode = FC_READ;
	Fsb->IO->StartBlock = thisfile->DataOffset +
	    ((thiscluster - 2) * thisfile->SectorsPerCluster);
	Fsb->IO->Count = thisfile->SectorsPerCluster * SECTORSZ;
	Fsb->IO->Address = Fsb->Buffer;

	if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
#ifdef FATDEBUG
	    printf("\nIO Error Reading Cluster");
#endif
	    return FAILURE;
	}

	/* update buffer address */
	Fsb->Buffer += Fsb->IO->Count;
	rc += Fsb->IO->Count;
	Fsb->Count -= Fsb->IO->Count;

	/* get next cluster number */
	if (NextCluster(Fsb, thisfile, &thiscluster) != ESUCCESS) {
	    return FAILURE;
	}

	if (LASTCLUSTER(thisfile->FatType, thiscluster) == TRUE)
	    to_read = 0;
	else
	    to_read--;
    }

    /* Check if at the end of cluster chain (shouldn't happen). */
    if (LASTCLUSTER(thisfile->FatType, thiscluster) == TRUE) {
	Fsb->IO->Offset.lo += rc;
	return (rc); /* done */
    }

    /* read in partial cluster */
    Fsb->IO->FunctionCode = FC_READ;
    Fsb->IO->StartBlock = thisfile->DataOffset +
        ((thiscluster - 2) * thisfile->SectorsPerCluster);
    Fsb->IO->Address = buffer;
    Fsb->IO->Count = SECTORSZ;

    to_read = (Fsb->Count + SECTORSZ - 1) / SECTORSZ;

    /* do read, sector by sector */
    for (i = 0; i < to_read && Fsb->Count; i++) {

	if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
#ifdef FATDEBUG
	    printf("\nIO Error Reading Sector");
#endif
	    return FAILURE;
	}

	transfer_cnt = (Fsb->Count < SECTORSZ ? Fsb->Count : SECTORSZ);

	/* move data from temporary buffer to real buffer */
	bcopy((CHAR *)Fsb->IO->Address, Fsb->Buffer, transfer_cnt);

	/* update counters and buffer pointer to reflect characters read */
	rc += transfer_cnt;
	Fsb->Buffer += transfer_cnt;
	Fsb->Count -= transfer_cnt;
	Fsb->IO->StartBlock++; /* next sector please */
    }

    /* update the file offset */
    Fsb->IO->Offset.lo += rc;
    Fsb->IO->Count = rc;

    /* return the actual number of character read */
    return (ESUCCESS);
}

/****************************************************************************

   Function : FatGetReadStatus

   Description :
       Support 64 bit offset

 ***************************************************************************/

LONG FatGetReadStatus(FSBLOCK *Fsb)
{
    OPENINFO *thisfile;
    if ((thisfile = GetFileDesc(Fsb)) == NULL)
	return EBADF;

    /* if file is a directory fail */
    if (thisfile->Attribute & DIRECTORY) {
	return EBADF;
    }

    if (Fsb->IO->Offset.lo < thisfile->FileLength)
	return ESUCCESS;
    else
	return EAGAIN;
}

/****************************************************************************

   Function : fatgetdents

   Description :
      Assumption: Always have an end of directory entry (???)

 ***************************************************************************/

LONG FatGetDents(FSBLOCK *Fsb)
{
    LONG done = FALSE, i = 0, rc = 0, rootdir = FALSE, offset;
    char filename[13], *ext, *j;
    register OPENINFO *dir;
    USHORT thiscluster;
    DIRECTORYENTRY *dest = (DIRECTORYENTRY *)Fsb->IO->Address;
    char ch;

    Fsb->Count = Fsb->IO->Count;

    if ((dir = GetFileDesc(Fsb)) == NULL) {
	Fsb->IO->ErrorNumber = EBADF;
	return FAILURE;
    }

    /* check that file is associated with a directory */
    if (!(dir->Attribute & DIRECTORY)) {
	Fsb->IO->ErrorNumber = EBADF;
	return FAILURE;
    }

    /* HACK - subdirectories have length of zero */
    if (dir->FileLength)
	rootdir = TRUE;

    /* compute the starting sector to read */
    if (rootdir) {
	Fsb->IO->StartBlock = 
	    dir->StartingCluster + (Fsb->IO->Offset.lo / SECTORSZ);
	offset = Fsb->IO->Offset.lo % SECTORSZ;
	Fsb->IO->Count = LBUFFERSZ * SECTORSZ;
    } else {
	offset = Fsb->IO->Offset.lo / dir->BytesPerCluster;

	for (i = 0, thiscluster = dir->StartingCluster; i < offset; i++) {
	    if (NextCluster(Fsb, dir, &thiscluster) != ESUCCESS) {
		return FAILURE;
	    }
	}

	if (LASTCLUSTER(dir->FatType, thiscluster) == TRUE) {
	    Fsb->IO->ErrorNumber = ENOTDIR;
	    return (rc);
	}

	offset = Fsb->IO->Offset.lo % dir->BytesPerCluster;
	i = offset;
	Fsb->IO->StartBlock = dir->DataOffset +
	    ((thiscluster - 2) * dir->SectorsPerCluster);
	Fsb->IO->StartBlock += offset / SECTORSZ;
	Fsb->IO->Count = _min(2,(dir->SectorsPerCluster - (offset / SECTORSZ)));
	offset = offset % SECTORSZ;
    }

    Fsb->IO->FunctionCode = FC_READ;
    Fsb->IO->Address = buffer;

    while (!done) {

	if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
#ifdef FATDEBUG
	    printf("\nIO Error Reading Directory\n");
#endif
	    return FAILURE;
	}

	/* parse the entries */
	for (j = (char *)Fsb->IO->Address + offset ;
		j < (char *)Fsb->IO->Address + (Fsb->IO->Count * SECTORSZ) &&
		!done; j += DIRENTRYSZ) {
	    switch ((UCHAR)*j) {
	    case 0x00:
		/* we have reached the end of
                  the used directory entries */
		done = TRUE;
		break;
	    case 0xE5: /* file has been erased */
	    case 0x2E: /* alias for parent or current directory */
		Fsb->IO->Offset.lo += DIRENTRYSZ;
		break;
	    case 0x05:
		/* first character is 0xE5 */
		*j = 0xE5;
		/* do default now */
	    default:
		/* get name and attribute */
		bzero(filename, 13);
		strncpy((char *)filename, (const char *)(j + FILENAME), 8);
		ext = index((char *)filename, ' ');
		if (ext != NULL)
		    *ext = '\0';
		ch = *(const char *)(j + EXTENSION);
		if (ch != ' ' && ch != '\0') {
		    if (ext != NULL) {
			/* name padded */
			*ext = '.';
			ext++;
		    } else {
			filename[8] = '.';
			ext = &filename[9];
		    }
		    /* get extension */
		    strncpy(ext, (const char *)(j + EXTENSION), 3);
		    *(ext + 3) = '\0';
		    ext = index(filename, ' ');
		    if (ext != NULL)
			*ext = '\0';
		}

		/* write directory information 
                  UCHAR attribute
                  UCHAR filename length
                  char[] filename */
		dest->FileAttribute = *(j + ATTRIBUTE);
		dest->FileNameLength = (UCHAR)strlen(filename);
		strcpy((char *)dest->FileName, filename);
		dest++;
		rc++;
		if (rc == Fsb->Count)
		    done = TRUE;
	    }
	}

	/* update sector to read */
	if (!done) {
	    /* update logical sector */
	    if (rootdir) {
		Fsb->IO->Count = LBUFFERSZ * SECTORSZ;
		Fsb->IO->StartBlock += Fsb->IO->Count;
	    } else {
		i += Fsb->IO->Count;
		if (i >= dir->SectorsPerCluster) {
		    if (NextCluster(Fsb, dir, &thiscluster) != ESUCCESS) {
			return FAILURE;
		    }

		    if (LASTCLUSTER(dir->FatType, thiscluster) == TRUE)
			done = TRUE;
		    Fsb->IO->StartBlock = dir->DataOffset +
		        ((thiscluster - 2) * dir->SectorsPerCluster);
		    i = 0;
		    Fsb->IO->Address = buffer;
		    Fsb->IO->Count = 2;
		} else 
		    Fsb->IO->StartBlock += Fsb->IO->Count;
	    }
	    offset = 0;
	}
    }

    Fsb->IO->Offset.lo += rc * DIRENTRYSZ;
    Fsb->IO->Count = rc;
    return (rc);
}

LONG FatClose(FSBLOCK *Fsb)
{
    if (!Fsb->FsPtr)		/* This should never happen */
	return EINVAL;
    else {
	free (Fsb->FsPtr);
	Fsb->FsPtr = 0;
	lastfid = 0;
	return ESUCCESS;
    }
}

/****************************************************************************

   Function : FatReadMBR

   Description :
       Read the disk's master boot record into the given buffer,
       returning ESUCCESS if it can be read, EIO otherwise.
   
****************************************************************************/

static LONG
FatReadMBR (FSBLOCK *Fsb, UCHAR *mbr)
{
    ULONG part, rc;

    /* save original device partition number */
    part = Fsb->IO->Partition;

    /* set partition to zero to indicate raw partition */
    /* XXX -- IRIX dksc driver takes 10 as raw disk. */
    Fsb->IO->Partition = 10;
    Fsb->IO->FunctionCode = FC_OPEN;
    if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
	Fsb->IO->Partition = part;
	return EINVAL;
    }

    /* read sector 0 of disk */
    Fsb->IO->FunctionCode = FC_READ;
    Fsb->IO->Address = mbr;
    Fsb->IO->Count = SECTORSZ;		/* one disk sector */
    Fsb->IO->StartBlock = 0;

    rc = (*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO);

    /* close partition 0 */
    Fsb->IO->FunctionCode = FC_CLOSE;
    (*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO);

    /* restore real partition number */
    Fsb->IO->Partition = part;

    return rc;
}

/****************************************************************************

   Function : FatCheck

   Description :
       This function returns ESUCCESS if the partition supports the FAT file
       system. The partition type is stored in the count field of the fsb for
       uses by FatOpen.
   
   Note:
      Need to recognize extended partitions.  
 ***************************************************************************/

LONG FatCheck(FSBLOCK *Fsb)
{
#ifdef FATDEBUG
    printf("FAT check\n");
#endif

    if (Fsb->Device->Type == FloppyDiskPeripheral) {
	/* For now asssume floppy always 12 bit FAT */
	Fsb->Count = DOS12;
	return ESUCCESS;
    } else if (Fsb->Device->Type == DiskPeripheral) {
	/* for fixed disk, open device in raw mode and read the
	 * partition table from sector zero 
	 */
	if (FatReadMBR (Fsb, buffer) != ESUCCESS)
	    return EINVAL;

	/* check for a valid signature */
	if (stosi(buffer + 0x1fe) != 0xAA55)
	    return EINVAL;

	/* finally get partition type & check validity */
	Fsb->Count = *(buffer + 0x1c2 + (Fsb->IO->Partition - 1) * 16);
#ifdef FATDEBUG
	printf("System Indicator %d\n", Fsb->Count);
#endif
	if ((Fsb->Count == DOS16) || (Fsb->Count == DOS331)) {
	    Fsb->Count = DOS16;
	    return ESUCCESS;
	}
    }

    return EIO;
}

/****************************************************************************

   Function : ValidateFat

   Description :
      This function tries to locate a valid copy of the fat.  A copy of
      the fat is consider valid if the 1st byte is the same as the media
      descriptor byte, the 2nd and 3rd (and 4th for 16 bit) are 0xff. A
      return value of zero indictates a valid copy of the fat was found.

 ***************************************************************************/

LONG ValidateFat(FSBLOCK *Fsb, BPB *params, OPENINFO *info)
{
    LONG i, rc = !ESUCCESS;

    /* read 1st sector of FAT */
    Fsb->IO->FunctionCode = FC_READ;
    Fsb->IO->StartBlock = 1;
    Fsb->IO->Count = SECTORSZ;
    Fsb->IO->Address = buffer;

    for (i = 1; (i <= params->FatCount) && (rc == !ESUCCESS); i++) {

	/* read in 1st Sector of FAT */
	if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
	    return (rc);
	}

	/* validate FAT */
	/* 1st byte equals media desciptor byte
         2nd, 3rd, and 4th (if 16 bit) equal 0xff
       */

	if (buffer[0] != params->MediaDesc) {
	    rc = !ESUCCESS;
	} else {
	    if ((buffer[1] == 0xff) && (buffer[2] == 0xff)) {
		if (info->FatType == DOS12)
		    rc = ESUCCESS;
		else
		    /* must be 16 bit fat */
		    if (buffer[3] == 0xff)
			rc = ESUCCESS;
		    else
			rc = !ESUCCESS;
	    }
	}

	/* 1st copy is bad, if there is another copy try again */
	if ((rc != ESUCCESS) && (params->FatCount > 1))
	    Fsb->IO->StartBlock = params->SectorsPerFat*i + 1;
	else
	    /* need to keep the location of valid FAT */
	    info->FatStart = Fsb->IO->StartBlock;
    } /* end for */

    return (rc);

}

/****************************************************************************

   Function : Find

   Description :
         
 ***************************************************************************/
extern char *ParseDir(char *filename, char *buffer, LONG bufsize);

LONG Find(FSBLOCK *Fsb, BPB *params, OPENINFO *info)
{
    LONG done = FALSE, rootdir = TRUE, count, found = FALSE;
    USHORT thiscluster;
    char *next, *ep, buf[256], *filename;
    filename = buf;

    /* make a copy of the filename */
    /*   filename = strdup((char *)Fsb->Filename); */
    strcpy(filename, (char *)Fsb->Filename);

    if ((*filename == '\\') || (*filename == '/'))
	filename++;

#ifdef FATDEBUG
    printf("\nFind File %s\n", filename);
#endif

    /* no filename specified */
    if (!(*filename)) {
	info->StartingCluster = 1 + params->FatCount * params->SectorsPerFat;
	info->FileLength = params->RootDirEntries;
	info->Attribute = DIRECTORY;
	return ESUCCESS;
    }

    /* parse subdirectory */
    do {
	/* get the next path element */
	count = strcspn(filename, "\\/");
	next = (filename + count);
	if (*next) {
	    /* found a path deliminator */
	    *next = '\0';
	    next++;
	}

#ifdef FATDEBUG
	printf("Path Element = %s\n", next);
#endif

	/* compute starting logical sector to read */
	if (rootdir) {
	    Fsb->IO->StartBlock = 1 + params->FatCount * params->SectorsPerFat;
	    Fsb->IO->Count = LBUFFERSZ * SECTORSZ;
	} else {
	    count = 0;
	    thiscluster = info->StartingCluster;
	    Fsb->IO->StartBlock = info->DataOffset +
	        ((thiscluster - 2) * info->SectorsPerCluster);
	    Fsb->IO->Count = _min(LBUFFERSZ * SECTORSZ, info->SectorsPerCluster);
	}
	Fsb->IO->Address = buffer;
	done = found = FALSE;
	while (!done) {

	    if ((*Fsb->DeviceStrategy)(Fsb->Device, Fsb->IO) == FAILURE) {
		return FAILURE;
	    }

	    /* Search this sector */
	    ep = ParseDir(filename, (char *)buffer, Fsb->IO->Count);

	    if (ep != NULL)  {
		done = TRUE; /* end of directory found */
		if (*ep) {
		    /* entry found */
		    info->StartingCluster = stosi((UCHAR *)(ep + STARTCLUSTER));
		    info->FileLength = stoi((char *)(ep + FILESIZE));
		    info->Attribute = *(ep + ATTRIBUTE);
		    found = TRUE;
		}
	    }

	    if (!done) {
		/* update logical sector */
		if (rootdir) {
		    Fsb->IO->StartBlock += LBUFFERSZ;
		    Fsb->IO->Count = LBUFFERSZ * SECTORSZ;
		} else {
		    count += Fsb->IO->Count * SECTORSZ;
		    if (count == (info->SectorsPerCluster * SECTORSZ)) {
			if (NextCluster(Fsb, info, &thiscluster) != ESUCCESS) {
			    return FAILURE;
			}

			if (LASTCLUSTER(info->FatType, thiscluster) == TRUE)
			    done = TRUE;

			Fsb->IO->StartBlock = info->DataOffset +
			    ((thiscluster - 2) * info->SectorsPerCluster);
			Fsb->IO->Count = LBUFFERSZ * SECTORSZ;
			Fsb->IO->Address = buffer;
			count = 0;
		    } else
			Fsb->IO->StartBlock += Fsb->IO->Count;
		}
	    } else {
		/* we either found the requested element or found the end of
               the directory */
		if (found)
		    if (*next) { /* more path remains */
			if (!(info->Attribute & DIRECTORY))
			    found = FALSE;
		    }

		if (!found)
		    *next = '\0';
	    }

	} /* end while */
	rootdir = FALSE;
    } while (*(filename = next));

    if (found)
	return (ESUCCESS);
    else
	return (FAILURE);
}

char *ParseDir(char *filename, char *buffer, LONG bufsize)
{
    char name[11], *cp, *rc = NULL; 
    LONG length, done = FALSE;

    /* convert the null terminated filename string to the
      format used a directory entry */
    for (length = 0; length < 11; length++)
	name[length] = 0x20; /* fill with space characters */

    cp = index(filename, '.');
    if (cp != NULL)
	length = _min(cp - filename, 8);
    else
	length = _min(strlen(filename), 8);

    /* write filename portion to buffer */
    strncpy(name, filename, length);

    if (cp != NULL) {
	cp++;
	/* move extension to offset 8 of the buffer */
	length = _min(strlen(cp), 3);
	strncpy(&name[8], cp, length);
    }

    /* scan buffer looking for an entry with requested filename */
    for (cp = buffer; cp < (buffer + bufsize) && !done; cp += DIRENTRYSZ)
	switch (*cp) {
	case 0x00: /* end of directory */
	    done = TRUE;
	    rc = cp;
	    break;
	case 0xE5: /* file has been erased */
	    break;
	case 0x2E: /* parent or current directory */
	    break;
	case 0x05: /* first character is 0xE5 */
	    *cp = 0xE5;
	    /* do default now */
	default:
	    /* compare filename */
	    if (!strncasecmp(name, cp, 11)) {
		/* entry is found */
		done = TRUE;
		rc = cp;
	    }
	}

    /* If the requested file entry was found or at the end of
      the directory a pointer to the entry is returned. NULL
      return value indictates element not found and not at end
      of directory */

    return (rc);
}
