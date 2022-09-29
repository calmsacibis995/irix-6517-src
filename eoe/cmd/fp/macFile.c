/* 
   (C) Copyright Digital Instrumentation Technology, Inc., 1990 - 1993 
       All Rights Reserved
*/

/*

    TransferPro macFile.c - Functions which support Mac file I/O.

*/

#include <malloc.h>
#include "macSG.h"
#include "macPort.h"
#include "dm.h"

static char *Module = "macFile";

static struct m_fd *FdTable[MAX_OPEN_FILES] = { (struct m_fd *)0 };


/*--------------------------- macOpenFile ------------------------------

    Opens a file in the Macintosh file system.

------------------------------------------------------------------------*/

int macOpenFile ( volume, fileRec, fileMode, fileType, fd)
struct m_volume *volume;
struct file_record *fileRec;
int fileMode;
int fileType;
int *fd;
{
    int retval = E_NONE;
    struct m_fd *openFd = (struct m_fd *)0;

    /* find free file descriptor */

    if ( (*fd = macNewFd()) >= 0 && (openFd = macGetFd (*fd)) )
       {
        openFd->fdVolume = volume;
        openFd->fdOffset = 0;
        openFd->fdExtOffset = 0;
        openFd->fdExtBlock = 0;
        if ( (openFd->fdExtentLoc = (struct extent_descriptor *)malloc 
         (sizeof (struct extent_descriptor))) == (struct extent_descriptor *)0 
         || (openFd->fdFileRec = (struct file_record *)malloc 
         (sizeof (struct file_record))) == (struct file_record *)0 )
           {
            retval = set_error (E_MEMORY, "macFile", "macOpenFile", 
                                "Out of memory");
           }
        else
           {
            openFd->fdExtentLoc->ed_start = 0;
            openFd->fdExtentLoc->ed_length = 0;
            openFd->fdExtent = 0;
            openFd->fdEbufOffset = 0;
            openFd->fdEbufSize = 0;
            openFd->fdEbufWritePending = NO;
            memcpy (openFd->fdFileRec, fileRec, sizeof(struct file_record));
            openFd->fdExtentRec = 0;
            openFd->fdExtentNew = NO;
            openFd->fdEOF = 0;
            openFd->fdMode = fileMode;
            if ( fileType == DO_DATA_FORK )
               {
                openFd->fdFork = DATA_FORK;
                openFd->fdLogicalLen = &openFd->fdFileRec->filLgLen;
                openFd->fdPhysicalLen = &openFd->fdFileRec->filPyLen;
                openFd->fdExtents = &openFd->fdFileRec->filExtRec[0];
               }
            else if ( fileType == DO_RESOURCE_FORK )
               {
                openFd->fdFork = RESOURCE_FORK;
                openFd->fdLogicalLen = &openFd->fdFileRec->filRLgLen;
                openFd->fdPhysicalLen = &openFd->fdFileRec->filRPyLen;
                openFd->fdExtents = &openFd->fdFileRec->filRExtRec[0];
               }
            else
               {
                openFd->fdFork = FILE_REC;
                openFd->fdLogicalLen = (unsigned int *)0;
                openFd->fdPhysicalLen = (unsigned int *)0;
                openFd->fdExtents = (struct extent_descriptor *)0;
               }
           }
        if ( retval == E_NONE && openFd->fdFork != FILE_REC &&
             !(fileMode & DT_RDONLY) && !(fileMode & DT_APPEND)
             && *(openFd->fdLogicalLen) > 0 )
           {
            if ( (retval = macFreeFile( openFd )) != E_NONE )
               {
                free ( openFd );
                FdTable[*fd] = (struct m_fd *)0;
                *fd = -1;
               }
           }
       }
    return ( retval );
}

/* -------------------------- macGetFd -----------------------------

   Returns a pointer to the Mac file descriptor.

-----------------------------------------------------------------------*/

struct m_fd *macGetFd (fd)
int fd;
{
    struct m_fd *fileDescriptor = (struct m_fd *)0;

    if ( fd >= 0 && fd < MAX_OPEN_FILES )
       {
        fileDescriptor = FdTable[fd];
       }

    return ( fileDescriptor );
}

/*--------------------------- macNewFd --------------------------------

    Initializes a new file descriptor and returns its index.

-----------------------------------------------------------------------*/

int macNewFd()
   {
    int fd = -1;
    int i = 0;

    while ( i < MAX_OPEN_FILES && FdTable[i] != (struct m_fd *)0)
       {
        i++;
       }

    if ( i < MAX_OPEN_FILES )
       {
        if ( FdTable[i] = (struct m_fd *)malloc (sizeof (struct m_fd)) ) 
           {
            fd = i;
           }
       }

    return (fd);
   }

/* -------------------------- macReadFile ------------------------------

    Reads a file from a MacIntosh file system.

 ------------------------------------------------------------------------*/
int macReadFile (fd, targetBuf, count)
    struct m_fd *fd;
    char *targetBuf;
    unsigned int count;
   {
    int retval = E_NONE;
    unsigned int bytes_read = 0L;
    unsigned int bytes_to_copy = 0;

    /* While there is more to read... */

    if ( fd->fdFork == FILE_REC )
       {
        retval = macReadFileRecord ( fd, targetBuf, count);
       }
    else
       {

        while ( retval == E_NONE && bytes_read < count )
           {
            if ( (retval = macSetReadExtent(fd)) != E_NONE )
               {
                ;
               }
            /* make sure the bytes we want are in memory */
            else if ( (retval = macSetReadExtentBuffer(fd)) != E_NONE )
               {
                ;
               }
            else
               {
                /* calculate number of bytes to read */
                bytes_to_copy = (fd->fdExtentLoc->ed_length * 
                       fd->fdVolume->vib->drAlBlkSiz) - fd->fdExtOffset;
    
                if( bytes_to_copy > (count - bytes_read) )
                   {
                    bytes_to_copy = count - bytes_read;
                   }
                if ( bytes_to_copy > (fd->fdEbufSize+fd->fdEbufOffset) -
                    fd->fdExtOffset )
                   {
                    bytes_to_copy = (fd->fdEbufSize+fd->fdEbufOffset) -
                        fd->fdExtOffset;
                   }
        
                /* Copy bytes from extent buffer to target buffer */
                memcpy( targetBuf+bytes_read,
                    fd->fdExtent+(fd->fdExtOffset-fd->fdEbufOffset),
                    bytes_to_copy);
    
                bytes_read += bytes_to_copy;
                fd->fdOffset += bytes_to_copy;
                fd->fdExtOffset += bytes_to_copy;
                fd->fdExtBlock = fd->fdExtOffset/fd->fdVolume->vib->drAlBlkSiz;
               }
           }
        if ( *fd->fdLogicalLen < fd->fdOffset )
           {
            *fd->fdLogicalLen = fd->fdOffset;
           }
       }

    return (retval);
   }

/*-- macSetReadExtent ------------------
 *
 * This code assures that the current extent pointed to by the fptr
 * is the same one we need to be reading from for the current file offset.
 * 
 */

int macSetReadExtent( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE;
    struct extent_descriptor *temp_ed = (struct extent_descriptor *)0;
    char *funcname = "macSetReadExtent";

    /* Find extent of current offset */
    if ( (retval = macFindExtent(fd, &temp_ed)) != E_NONE )
       {
        ;         /* Can't find extent, return error... */
       }

    /* Process this extent. */
    else if( temp_ed == (struct extent_descriptor *)0 )
       {
        retval = set_error (E_READ, Module, funcname,"Unexpected End-of-File.");
       }

    /* macFindExtent will assign us a pointer to the current extent
       (whether it be catalog or extents tree) into the file record
       or extents record, as appropriate. We can test this to see if
       we're already set */

    else if ( temp_ed->ed_start == fd->fdExtentLoc->ed_start &&
              temp_ed->ed_length == fd->fdExtentLoc->ed_length )
       {
        ;        /* It's already set... */
       }

    /* set up for new current extent */
    else
       { 
        fd->fdEbufSize = 0;
        fd->fdExtentLoc->ed_start = temp_ed->ed_start;
        fd->fdExtentLoc->ed_length = temp_ed->ed_length;
       }

    return( retval );
   }

/*-- macSetReadExtentBuffer ----------------
 *
 * This code ensures that the section of the file that the user wants
 * to access is actually in memory. It is also responsible for keeping
 * a reasonable amount in memory, up to the maximum amount that we are
 * allowing in memory at any given time.
 *
 */

int macSetReadExtentBuffer( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE;
    char *funcname = "macSetReadExtentBuffer";
    unsigned int curAmount = 0;
    unsigned int alblock_start;
    unsigned int alblock_length;
    unsigned int albyte_count;
    struct extent_descriptor temp_ed;

    /* check to see if we've got bytes to read */
    if ( fd->fdEbufSize == 0 ||
        fd->fdExtOffset < fd->fdEbufOffset ||
        fd->fdExtOffset >= fd->fdEbufOffset+fd->fdEbufSize )
       {
        /* align the amount on allocation block boundaries */
        alblock_start = fd->fdExtBlock + fd->fdExtentLoc->ed_start;
        alblock_length = fd->fdExtentLoc->ed_length-fd->fdExtBlock;
        if ( (albyte_count = alblock_length * fd->fdVolume->vib->drAlBlkSiz)
            > MAX_EBUFSIZE )
           {
            albyte_count =
                (int)(MAX_EBUFSIZE/fd->fdVolume->vib->drAlBlkSiz)*fd->fdVolume->vib->drAlBlkSiz;
            alblock_length = albyte_count / fd->fdVolume->vib->drAlBlkSiz;
           }

        /* Make sure our buffer is big enough */
        if ( fd->fdExtent == 0 &&
            (fd->fdExtent = malloc((unsigned)(albyte_count))) == 0 )
           {
            retval = set_error( E_MEMORY, Module, funcname, "" );
           }
        else if ( (fd->fdExtent = realloc(fd->fdExtent,
            (unsigned)(curAmount + (albyte_count-curAmount)))) == 0 )
           {
            retval = set_error( E_MEMORY, Module, funcname, "" );
           }
        else
           {
            temp_ed.ed_start = alblock_start;
            temp_ed.ed_length = alblock_length;
            if ( (retval = macReadExtent(fd->fdVolume, &temp_ed, fd->fdExtent))
                == E_NONE )
               {
                fd->fdEbufOffset = fd->fdExtBlock *
                    fd->fdVolume->vib->drAlBlkSiz;
                fd->fdEbufSize = albyte_count;
               }
           }
       }

    return retval;
   }

/*--------------------- macReadExtent ------------------------------
    Reads an extent from the disk.
 ---------------------------------------------------------------------*/

int macReadExtent (volume,  ed, buffer)
    struct m_volume *volume;
    struct extent_descriptor *ed;
    char *buffer;
   {
    int retval = E_NONE;
    char *funcname = "macReadExtent";
    unsigned int  block;
    unsigned int  offset = 0;
    unsigned int  length;
    unsigned int bytes_to_xfer = 0;
    int blockFactor = volume->vib->drAlBlkSiz/SCSI_BLOCK_SIZE;

    /* Find actual block number. */

    /* physical block number in the floppy */
    block = au_to_partition_block (volume->vib, ed->ed_start, 
                                           volume->vstartblk);

    length = ed->ed_length * volume->vib->drAlBlkSiz;

    /* au_to_block will return physical block number in the volume */ 
    if (au_to_block(volume->vib, ed->ed_start) > 
	(unsigned int)(volume->vib->drNmAlblks*blockFactor+
        volume->vib->drAlBlSt) || 
        ed->ed_length > volume->vib->drNmAlblks)
       {
        retval = set_error( E_RANGE, Module, funcname,
            "Can't read extent: BLK=%d, LEN=%d", block, length );
       }

    while( retval == E_NONE && offset < length )
       {
        if( (length - offset) > MAX_TRANSFER )
           {
            bytes_to_xfer = MAX_TRANSFER;
           }
        else
           {
            bytes_to_xfer = (length - offset);
           }
 
        retval = macReadBlockDevice (volume->device, buffer+offset, 
                 block, bytes_to_xfer/SCSI_BLOCK_SIZE);

        block += bytes_to_xfer/SCSI_BLOCK_SIZE;
        offset += bytes_to_xfer;
       }
    return (retval);
}
/*------------------------- macFindExtent ----------------------------------
    Finds the extent descriptor corresponding to the current offset
    in the given file descriptor.
 ----------------------------------------------------------------------------*/

int macFindExtent (fd, ed)
    struct m_fd *fd;
    struct extent_descriptor **ed;
   {
    int retval = E_NONE;
    unsigned int blockCt = 0;
    unsigned int tempBlock = 0;
    unsigned int currBlock = 0;
    int i = 0, xRecNum = UNASSIGNED_REC;
    unsigned int xNodeNum = UNASSIGNED_NODE;
    char *funcname="macFindExtent";
    struct extent_descriptor *filerecExtents = (fd->fdFork == DO_DATA_FORK)?
        fd->fdFileRec->filExtRec : fd->fdFileRec->filRExtRec;

    /* Check file record for corresponding extent. */
    *ed = (struct extent_descriptor *)0;

    while ( retval == E_NONE && *ed == (struct extent_descriptor *)0 && i < 3)
       {
        if ( filerecExtents[i].ed_length == 0 )
           {
            retval = set_error( E_NOTFOUND, Module, funcname, "" );
           }
        else if (fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz <  (blockCt + 
            filerecExtents[i].ed_length) )
           {
            *ed = &filerecExtents[i];
            fd->fdExtBlock = fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz - 
                             blockCt;
            fd->fdExtOffset = (long) (fd->fdOffset - blockCt *
                             fd->fdVolume->vib->drAlBlkSiz);
           }
        else
           {
            blockCt += filerecExtents[i].ed_length;
            i++;
           }
       }
  
    /* If not found in file record, check in extent tree. */
    if ( retval == E_NONE && *ed == (struct extent_descriptor *)0 )
       {
        if ( fd->fdExtentRec &&
            (blockCt = fd->fdExtentRec->xkrRecKey.key.xkr.xkrFABN) <
            (currBlock = fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz) )
           {
            /* look in the current extent first */
            for ( i = 0; i < 3 && !*ed; )
               {
                if (currBlock < (blockCt + 
                    fd->fdExtentRec->xkrExtents[i].ed_length) )
                   {
                    *ed = &fd->fdExtentRec->xkrExtents[i];
                    fd->fdExtBlock = fd->fdOffset/
                                     fd->fdVolume->vib->drAlBlkSiz - blockCt;
                    fd->fdExtOffset = (long) (fd->fdOffset - blockCt*
                                     fd->fdVolume->vib->drAlBlkSiz);
                   }
                else
                   {
                    blockCt += fd->fdExtentRec->xkrExtents[i].ed_length;
                    i++;
                   }
               }
           }
        if ( !*ed && 
            (retval = macFindExtRec(fd, &xNodeNum,
                                      (unsigned int *)&xRecNum)) == E_NONE )
           {
            currBlock = (unsigned int)(fd->fdOffset/fd->fdVolume->vib->drAlBlkSiz);
            fd->fdExtentRec = (struct extent_record *)fd->fdVolume->
                extents[xNodeNum]->BtNodes.BtLeaf.lnRecList[xRecNum];
            tempBlock = ((struct extent_record *)fd->
                fdVolume->extents[xNodeNum]->BtNodes.
                BtLeaf.lnRecList[xRecNum])->xkrRecKey.key.xkr.xkrFABN;

            for ( i = 0; i < 3 && !*ed; )
               {
                if ( currBlock >= tempBlock +
                   fd->fdExtentRec->xkrExtents[i].ed_length )
                   {
                    tempBlock += fd->fdExtentRec->xkrExtents[i].ed_length;
                    i++;
                   }
                else
                   {
                    *ed = &fd->fdExtentRec->xkrExtents[i];
                   }
               }
            fd->fdExtBlock = currBlock - tempBlock;
            fd->fdExtOffset = (long) (fd->fdOffset - tempBlock * 
                                      fd->fdVolume->vib->drAlBlkSiz);
           }
       }
    if( !*ed )
       {
        retval = set_error(E_NOTFOUND,Module,funcname,
                           "Can't find extent descriptor");
       }
      
    return (retval);
}

/*--------------------- macReadFileRecord ------------------------
    Reads from the file record of a file.
------------------------------------------------------------------*/

int macReadFileRecord (fd, targetBuf, count)
struct m_fd *fd;
char *targetBuf;
unsigned int count;
   {
    int retval = E_NONE;
    unsigned int bytes_to_copy;

    bytes_to_copy = count > (sizeof(struct file_record) - fd->fdOffset) ? 
                    sizeof (struct file_record) - fd->fdOffset : count;

    memcpy ( targetBuf, fd->fdFileRec + fd->fdOffset, bytes_to_copy );
    
    fd->fdOffset += bytes_to_copy;

    return ( retval );
   }

/*--------------------- macWriteFile ------------------------------
    Writes a file chunk to disk (buffered). Does NOT flush vib, vbm, extents.
    call macFlush...() to flush the current extent to disk
  ---------------------------------------------------------------------*/

int macWriteFile (fd, sourceBuf, count )
    struct m_fd      *fd;
    char             *sourceBuf;
    unsigned int     count;
   {
    int retval = E_NONE;
    unsigned int bytes_written = 0L;
    unsigned int bytes_to_copy = 0;


    if ( fd->fdFork == FILE_REC )
       {
        retval = macWriteFileRecord (fd, sourceBuf, count);
       }

    else
       {
        /* While there is more to write... */
        while ( retval == E_NONE && bytes_written < count )
           {
            /* make sure we have a current extent with some space left */
            if ( (retval = macSetWriteExtent(fd)) != E_NONE )
               {
                ;
               }
            
            else if ( (retval = macSetWriteExtentBuffer(fd)) != E_NONE )
               {
                ;    /* Can't get memory for extent, return error... */
               }
    
            else
               {
                /* calculate number of bytes to copy */
                bytes_to_copy = (fd->fdExtentLoc->ed_length * 
                                 fd->fdVolume->vib->drAlBlkSiz) - fd->fdExtOffset;
    
                if( bytes_to_copy > (count - bytes_written) )
                   {
                    bytes_to_copy = count - bytes_written;
                   }
    
                /* be sure always write exactly up to an alignment boundary */
                if ( bytes_to_copy >
                    (fd->fdEbufSize+fd->fdEbufOffset) - fd->fdExtOffset )
                   {
                    bytes_to_copy = (fd->fdEbufSize+fd->fdEbufOffset) -
                        fd->fdExtOffset;
                   }
    
                /* Copy bytes from buffer to target buffer */
                memcpy( fd->fdExtent+(fd->fdExtOffset- fd->fdEbufOffset),
                    sourceBuf+bytes_written, bytes_to_copy);
    
                bytes_written += bytes_to_copy;
                fd->fdOffset += bytes_to_copy;
                fd->fdExtOffset += bytes_to_copy;
                fd->fdExtBlock = fd->fdExtOffset/fd->fdVolume->vib->drAlBlkSiz;
                fd->fdEbufWritePending = YES;
               }
           }
        if ( *fd->fdLogicalLen < fd->fdOffset )
           {
            *fd->fdLogicalLen = fd->fdOffset;
           }
       }

    return (retval);
   }

/*-- macSetWriteExtent -----------------
 *
 * This function makes sure the current extent has some room.
 * It flushes any current extent and starts a new one if not.
 *
 */

int macSetWriteExtent( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE;

    if ( ((fd->fdExtentLoc->ed_length * fd->fdVolume->vib->drAlBlkSiz) - 
           fd->fdExtOffset) <= 0 )
       {
        if ( (retval = macExtendCurrentExtent(fd)) == E_NONE )
           {
            fd->fdExtentNew = YES;
           }
       }
    if ( retval == E_NOTFOUND )
       {
        retval = clear_error();

        /* flush current extent buffer */
        if ( fd->fdExtentLoc->ed_length > 0 &&
            (retval = macFlushFileExtent(fd)) != E_NONE )
           {
            ;         /* flush failed, return error... */
           }

        /* Initialize extent descriptor and allocate new extent buffer. */
        else if ( (retval = macAllocFileExtent(fd)) != E_NONE )
           {
            ;         /* Can't initialize new extent, return error... */
           }
       }
 
    return (retval);
   }

/*-- macSetWriteExtentBuffer -------------
 *
 * This function makes sure the extent buffer for the current extent
 * has some room in it. It flushes any current buffer and checks allocation
 * if not. It is responsible for making sure the buffer is as close to
 * the maximum in-memory file amount as possible. 
 *
 * Note: macExtendCurrentExtent() will realloc the buffer when extending
 * the current extent to minimize disk flushing.
 * 
 */

int macSetWriteExtentBuffer( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE;
    unsigned int alloc_amount;
    unsigned int size;
    char *funcname = "macSetWriteExtentBuffer";
    struct extent_descriptor temp_ed;

    /* if a current buffer and no space left, flush it */
    if ( fd->fdEbufSize > 0 &&
        fd->fdEbufSize <= fd->fdExtOffset-fd->fdEbufOffset )
       {
        retval = macFlushFileExtentBuffer( fd );
       }

    if ( fd->fdEbufSize == 0 )
       {
        /* allocate our buffer up to an alignment boundary */
        if ( (alloc_amount = (fd->fdExtentLoc->ed_length *
            fd->fdVolume->vib->drAlBlkSiz) - (fd->fdExtBlock *
            fd->fdVolume->vib->drAlBlkSiz)) > MAX_EBUFSIZE )
           {
            alloc_amount =
                (int)(MAX_EBUFSIZE/fd->fdVolume->vib->drAlBlkSiz)*fd->fdVolume->vib->drAlBlkSiz;
           }

        if (  !fd->fdExtent &&
            (fd->fdExtent = malloc((unsigned)(alloc_amount))) == 0 )
           {
            retval = set_error( E_MEMORY, Module, funcname, "" );
           }
        else if ( (fd->fdExtent = realloc(fd->fdExtent,
            (unsigned)(alloc_amount))) == 0 )
           {
            retval = set_error( E_MEMORY, Module, funcname, "" );
           }
        else
           {
            fd->fdEbufOffset = fd->fdExtBlock * fd->fdVolume->vib->drAlBlkSiz;
            fd->fdEbufSize = alloc_amount;
            if ( !fd->fdExtentNew )
               {
                temp_ed.ed_start = fd->fdExtentLoc->ed_start+fd->fdExtBlock;
                temp_ed.ed_length =
                    fd->fdEbufSize/fd->fdVolume->vib->drAlBlkSiz;
                retval = macReadExtent( fd->fdVolume, &temp_ed, fd->fdExtent );
               }
           }
       }

    return retval;
   }

/*-- macFlushFileExtent -----------
 *
 *
 */

int macFlushFileExtent( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE;
    unsigned int extentBlocks;

    if ( (retval = macFlushFileExtentBuffer(fd)) == E_NONE )
       {
        if ( fd->fdExtentNew )
           {
            retval = clear_error();

            /* set the final used quantity of the extent */
            extentBlocks = (fd->fdExtOffset+
                ((fd->fdExtOffset % fd->fdVolume->vib->drAlBlkSiz)?
                fd->fdVolume->vib->drAlBlkSiz : 0)) /
                fd->fdVolume->vib->drAlBlkSiz;

            macAddExtentToVib(fd->fdVolume->vib, (int)extentBlocks);
            macRemoveExtentFromVbm(fd->fdVolume->vbm,
                          fd->fdExtentLoc->ed_start+extentBlocks, 
                          fd->fdExtentLoc->ed_length-extentBlocks);
            retval = macAddExtentToFile(fd, 
                fd->fdExtentLoc->ed_start,extentBlocks);
           }
       }		

    return( retval );
   }

/*-- macFlushFileExtentBuffer -----------
 *
 * This function flushes the current extent buffer. It does not otherwise
 * disturb the current extent, or update any of the volume structures
 * such as catalog and vib.
 *
 */

int macFlushFileExtentBuffer( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE;
    struct extent_descriptor temp_ed;
    unsigned int temp_length;

    /* since we only allow the creation of an aligned extent buffer,
       let us assume that we can just flush our buffer to disk */
    if ( fd->fdEbufSize > 0 )
       {
        temp_ed.ed_start = fd->fdExtentLoc->ed_start+
            (fd->fdEbufOffset/fd->fdVolume->vib->drAlBlkSiz);
        temp_length = (fd->fdExtOffset-fd->fdEbufOffset);
        temp_length += (temp_length % fd->fdVolume->vib->drAlBlkSiz)?
            fd->fdVolume->vib->drAlBlkSiz : 0;
        temp_length = (unsigned int)(temp_length/fd->fdVolume->vib->drAlBlkSiz);
        temp_ed.ed_length = temp_length;

        retval = macWriteExtent( fd->fdVolume, &temp_ed, fd->fdExtent );
	fd->fdEbufOffset = fd->fdExtBlock * fd->fdVolume->vib->drAlBlkSiz;
        fd->fdEbufSize = 0;
        fd->fdEbufWritePending = NO;
       }
    
    return retval;
   }

/*--------------------- macWriteExtent ------------------------------
    Writes an extent to disk.
 ---------------------------------------------------------------------*/

int macWriteExtent ( volume, ed, buffer )
    struct m_volume *volume;
    struct extent_descriptor *ed;
    char *buffer;
   {
    int retval = E_NONE;
    unsigned int  block;
    unsigned int  offset = 0;
    unsigned int  length;
    unsigned int  bytes_to_xfer = 0;

    /* Find actual block number. */
    block = au_to_partition_block (volume->vib, ed->ed_start, 
                                           volume->vstartblk);

    length = ed->ed_length * volume->vib->drAlBlkSiz;
    while( retval == E_NONE && offset < length )
       {
        if( (length - offset) > MAX_TRANSFER )
           {
            bytes_to_xfer = MAX_TRANSFER;
           }
        else
           {
            bytes_to_xfer = (length - offset);
           }
 
        retval = macWriteBlockDevice( volume->device, buffer+offset, 
                 block, bytes_to_xfer/SCSI_BLOCK_SIZE);

        block += bytes_to_xfer/SCSI_BLOCK_SIZE;
        offset += bytes_to_xfer;
       }

    return (retval);
   }

/*--------------------- macWriteFileRecord ------------------------
    Reads from the file record of a file.
------------------------------------------------------------------*/

int macWriteFileRecord (fd, sourceBuf, count)
struct m_fd *fd;
char *sourceBuf;
unsigned int count;
   {
    int retval = E_NONE;
    unsigned int bytes_to_copy;

    bytes_to_copy = count > (sizeof(struct file_record) - fd->fdOffset) ? 
                    sizeof (struct file_record) - fd->fdOffset : count;

    memcpy ( fd->fdFileRec + fd->fdOffset, sourceBuf, bytes_to_copy );
    
    fd->fdOffset += bytes_to_copy;

    return (retval);
   }

/*--- macFreeFile ----------------------------------
 *    
 */

int macFreeFile( fd )
    struct m_fd *fd;
   {
    int        retval = E_NONE,
               xRecNum = UNASSIGNED_REC;
    unsigned int xNodeNum = UNASSIGNED_NODE;
    unsigned int        filePtrBlocks = 0;
    unsigned short *forkStartBlock;
    unsigned int   *forkLogicalLen;
    unsigned int   *forkPhysicalLen;
    struct extent_descriptor *forkExtents; /* extent pointer (for first three) */

    /* free extents in file pointer */
    /* assign fork pointers */
    if ( fd->fdFork == DATA_FORK )
       {
        forkStartBlock = &fd->fdFileRec->filStBlk;
        forkLogicalLen = &fd->fdFileRec->filLgLen;
        forkPhysicalLen = &fd->fdFileRec->filPyLen;
        forkExtents = fd->fdFileRec->filExtRec;
       }
    else
       {
        forkStartBlock = &fd->fdFileRec->filRStBlk;
        forkLogicalLen = &fd->fdFileRec->filRLgLen;
        forkPhysicalLen = &fd->fdFileRec->filRPyLen;
        forkExtents = fd->fdFileRec->filRExtRec;
       }
    macFreeExtentArray (fd->fdVolume->vib, fd->fdVolume->vbm, 
                                 forkExtents, &filePtrBlocks );
    fd->fdOffset = (long) (filePtrBlocks * fd->fdVolume->vib->drAlBlkSiz);

    /* find and free all extents in extents tree */
    while( retval == E_NONE && fd->fdOffset < *forkPhysicalLen )
       {
        if( (retval = macFindExtRec(fd, &xNodeNum, 
				    (unsigned int *)&xRecNum)) == E_NONE )
           {
            macFreeExtentArray (fd->fdVolume->vib, fd->fdVolume->vbm, 
                     ((struct extent_record *)fd->fdVolume->extents[xNodeNum]->
                     BtNodes.BtLeaf.lnRecList[xRecNum])->xkrExtents, 
                     &filePtrBlocks);
            retval = macDelRecFromTree( fd->fdVolume, xNodeNum, xRecNum, 
                                        EXTENTS_TREE );
           }
        if( retval == E_NONE )
           {
            fd->fdOffset += filePtrBlocks * fd->fdVolume->vib->drAlBlkSiz;
           }
       }

    fd->fdExtBlock = 0; 
    fd->fdExtOffset = 0; 
    fd->fdOffset = 0; 
    fd->fdEOF = 1; 
    fd->fdExtentRec = 0;
    *forkStartBlock = 0; /* alloc. block 1 of data fork (obs.) */
    *forkLogicalLen = 0; /* logical end-of-file of data fork */
    *forkPhysicalLen = 0; /* physical end-of-file of data fork */
    fd->fdFileRec->filMdDat = macDate(); /* date and time of last mod */

    return(retval);
   }


/*---  macRenameFileRec ----------------------------------
 *  rename file record and place renamed record in tree 
 */

int macRenameFileRec( oldVolume, oldname, oldDirId, newVolume, newname, 
                      newDirId, fileptr, newNodeNum, newRecNum)
    struct m_volume *oldVolume;
    char       *oldname;
    int         oldDirId;
    struct m_volume *newVolume;
    char       *newname;
    int         newDirId;
    struct file_record *fileptr;
    int         newNodeNum;
    int         newRecNum;
   {
    int         retval = E_NONE;
    struct file_record newFileRec;
    int         nodeNum;
    int         recNum;

    /* build new file record - save old record info */
    memcpy( (char *)&newFileRec, fileptr, sizeof(struct file_record) );
               
    buildRecKey( (struct record_key *)(&newFileRec), (int)newDirId, newname );

    /* add new file record to catalog tree */

    retval = macAddRecToTree( newVolume, &newNodeNum, (void *)&newFileRec,
                                      &newRecNum, CATALOG_TREE );

    /* remove old file record from catalog tree */
    if( retval == E_NONE )
       {
        if ( (retval = macFindFile(oldVolume,&nodeNum,&recNum,&fileptr,
                                   oldname, oldDirId)) == E_NONE )
           {
            retval = macDelRecFromTree( oldVolume, nodeNum, recNum, 
                                           CATALOG_TREE );
           }
       }

       /* if all is cool, update disk */
    if( retval == E_NONE )
       {
        retval = macUpdateDisk( oldVolume );
       }
    if ( oldVolume != newVolume && retval == E_NONE )
       {
        retval = macUpdateDisk( newVolume );
       }

    return( retval );
   }


/*---  macRenameDirectory ----------------------------------
 *  rename directory record, update thread record, and place 
 *  renamed record in tree 
 */

int macRenameDirectory(oldVolume, oldname, oldDirId, newVolume, newname, 
                       newDirId, dirptr, newNodeNum, newRecNum)
    struct m_volume *oldVolume;
    char       *oldname;
    int         oldDirId;
    struct m_volume *newVolume;
    char       *newname;
    int         newDirId;
    struct dir_record *dirptr;
    int         newNodeNum;
    int         newRecNum;
   {
    int         retval = E_NONE,
                recNum,
                thdRecNum = UNASSIGNED_REC;
    unsigned int  nodeNum,
                thdNodeNum = UNASSIGNED_NODE;
    struct dir_record newDirRec;
    struct thread_record currThdRec;

    if ( oldVolume != newVolume )
       {
        retval = set_error (E_RENAME, Module, "macRenameDirectory",
                    "Cannot move directories across volumes.");
       }

    /* add new directory record to catalog tree */
    if( retval == E_NONE )
       {
        /* build new directory record with old record info */
        memcpy( (char *)&newDirRec,  dirptr, sizeof(struct dir_record ) );
        buildRecKey( (struct record_key *)(&newDirRec), (int)newDirId, newname );

        /* find and update thread record for this directory */
        buildRecKey(&currThdRec.thdRecKey, 
                             (int)newDirRec.dirDirID, "" );
        retval = searchTree(newVolume, &currThdRec.thdRecKey, 
                    CATALOG_TREE, &thdRecNum, &thdNodeNum);
        if( retval == E_NONE )
           {
            /* change name in thread record */
            strncpy( ((struct thread_record *)newVolume->catalog[thdNodeNum]->
                   BtNodes.BtLeaf.lnRecList[thdRecNum])->thdCName, 
                   newDirRec.dirRecKey.key.ckr.ckrCName,
                   (int)newDirRec.dirRecKey.key.ckr.ckrNameLen );
            ((struct thread_record *)newVolume->catalog[thdNodeNum]->
                   BtNodes.BtLeaf.lnRecList[thdRecNum])->thdCNameLen = 
                   newDirRec.dirRecKey.key.ckr.ckrNameLen;
            ((struct thread_record *)newVolume->catalog[thdNodeNum]->
                   BtNodes.BtLeaf.lnRecList[thdRecNum])->
                   thdParID = newDirId;
            macSetBitmapRec (&newVolume->catBitmap, thdNodeNum);

            /* add new directory record to tree */
            retval = macAddRecToTree( newVolume, &newNodeNum, 
                    (void *)&newDirRec, &newRecNum, CATALOG_TREE );
           }

       /* remove old directory record from catalog tree */
        if( retval == E_NONE )
           {
            if( (retval = macFindDir(oldVolume, &nodeNum, &recNum, &dirptr,
                                     oldname, oldDirId)) == E_NONE )
               {
                retval = macDelRecFromTree( oldVolume, nodeNum, recNum, 
                                            CATALOG_TREE );
               }
           }

       /* if all is cool, update disk */
        if( retval == E_NONE )
           {
            retval = macUpdateDisk( oldVolume );
           }
       }

    return( retval );
   }

/*--- macRmDir ----------------------------------
 *  removes directory record and associated thread 
 *  record from catalog
 */

int macRmDir( volume, nodeNum, recNum, CWDid)
    struct m_volume *volume;
    unsigned int  nodeNum;
    int         recNum;
    int         CWDid;
   {
    int         retval = E_NONE,
                thdRecNum = UNASSIGNED_REC; 
    unsigned int  thdNodeNum = UNASSIGNED_NODE;
    struct thread_record currThdRec;

    /* find and delete thread record for this directory */

    buildRecKey(&currThdRec.thdRecKey, 
               (int)(((struct dir_record *)volume->catalog[nodeNum]->
               BtNodes.BtLeaf.lnRecList[recNum])->dirDirID), "" );

    retval = searchTree(volume, &currThdRec.thdRecKey, 
                 CATALOG_TREE, &thdRecNum, &thdNodeNum);

    /* delete thread record */
    if( retval == E_NONE )
       {
        retval = macDelRecFromTree( volume, thdNodeNum, thdRecNum, 
                                    CATALOG_TREE );
       }
    /* remove directory record from catalog tree */
    if( retval == E_NONE )
       {
        retval = macDelRecFromTree( volume, nodeNum, recNum, CATALOG_TREE );
       }

    /* if all is cool, update disk */
    if( retval == E_NONE )
       {
        volume->vib->drDirCnt--;

        if( CWDid == ROOT_LEVEL )
           {
            volume->vib->drNmRtDirs--;
           }

        retval = macUpdateDisk( volume );
       }

    return( retval );
   }

/*--- macCloseFile ----------------------------------
 *  redds up file ptr after writing to disk
 */

int macCloseFile( fd )
    int fd;
   {
    int retval = E_NONE;
    struct m_fd *openFd = (struct m_fd *)0;
    struct file_record *fileRec = (struct file_record *)0;
    int recNum;
    int nodeNum;

    if ( (openFd = macGetFd (fd)) != (struct m_fd *)0 )
       {
        if ( openFd->fdMode & (DT_WRONLY | DT_APPEND) )
           {
            if ( openFd->fdFork != FILE_REC )
               {
                retval = macFlushFileExtent( openFd );
               }
            if( retval ==  E_NONE )
               {
                if ( openFd->fdFork != FILE_REC &&
                    *openFd->fdLogicalLen > *openFd->fdPhysicalLen )
                   {
                    if( (*openFd->fdLogicalLen %
                        openFd->fdVolume->vib->drAlBlkSiz) == 0 )
                       {
                        *openFd->fdPhysicalLen = *openFd->fdLogicalLen;
                       }
                    else
                       {
                        *openFd->fdPhysicalLen = (((*openFd->fdLogicalLen)/
                             openFd->fdVolume->vib->drAlBlkSiz) + 1) * 
                             openFd->fdVolume->vib->drAlBlkSiz;
                       }
                   }

               /* update file record in the catalog tree */

               if( (retval = searchTree(openFd->fdVolume, 
                             &openFd->fdFileRec->filRecKey, CATALOG_TREE,
                             &recNum, (unsigned int *)&nodeNum)) != E_NONE )
                  {
                   retval = set_error(E_NOTFOUND, Module, "macCloseFile", 
                                      "File record not found.");
                  }
               else
                  {
                   fileRec = (struct file_record *)
                             openFd->fdVolume->catalog[nodeNum]->
                             BtNodes.BtLeaf.lnRecList[recNum];
                   if ( openFd->fdFork == DATA_FORK)
                      {
                       fileRec->filPyLen = openFd->fdFileRec->filPyLen;
                       fileRec->filLgLen = openFd->fdFileRec->filLgLen;
                       fileRec->filStBlk = openFd->fdFileRec->filStBlk;
                       memcpy (fileRec->filExtRec, 
                               openFd->fdFileRec->filExtRec, 
                               sizeof (struct extent_descriptor) * 3);
                      }
                   else if ( openFd->fdFork == RESOURCE_FORK)
                      {
                       fileRec->filRPyLen = openFd->fdFileRec->filRPyLen;
                       fileRec->filRLgLen = openFd->fdFileRec->filRLgLen;
                       fileRec->filRStBlk = openFd->fdFileRec->filRStBlk;
                       memcpy (fileRec->filRExtRec, 
                               openFd->fdFileRec->filRExtRec, 
                               sizeof (struct extent_descriptor) * 3);
                      }
                   else
                      {
                       memcpy (fileRec, openFd->fdFileRec,
                               sizeof (struct file_record) );
                      }
                   openFd->fdFileRec->filMdDat = macDate(); 
                   macSetBitmapRec (&openFd->fdVolume->catBitmap, nodeNum);
                   retval = macUpdateDisk ( openFd->fdVolume );
                  }
               }
           }
        if ( openFd->fdExtentLoc )
           {
            free (openFd->fdExtentLoc);
           }
        if ( openFd->fdExtent )
           {
            free (openFd->fdExtent);
           }
        if ( openFd->fdFileRec )
           {
            free (openFd->fdFileRec);
           }
        free ( openFd );

        FdTable[fd] = (struct m_fd *)0;
       }

    return( retval );
   }

/*--- initFileRec ----------------------------------
 *  initializes file ptr for writing to disk
 */

int initFileRec( volume, fptr, filename, CWDid ) 
    struct m_volume *volume;
    struct file_record *fptr;
    char      *filename;
    int CWDid;
   {

    int i, retval = E_NONE;
    char *funcname="initFileRec";

    buildRecKey(&fptr->filRecKey, CWDid, filename);

    fptr->cdrType = FILE_REC;       /* always 2 for file records */
    fptr->cdrReserv2 = '\0';        /* used internally */
    fptr->filFlags = 0;             /* file flags */
    fptr->filType = 0;              /* file version number (s/b 0) */

    /* info used by the Finder */
    for( i = 0; i < sizeof(struct FInfo); i++ )
       {
        *((char *)&fptr->filUsrWds+i) = '\0';
       }

    strncpy( (char *)fptr->filUsrWds.fdType, "TEXT", 4 );
    strncpy( (char *)fptr->filUsrWds.fdCreator, "ttxt", 4 );

    if ( (fptr->filFlNum = macGetNextID(volume->vib)) == -1 )
       {
        retval = set_error(E_NOTFOUND, Module,funcname,"Can't get new file id");
       }
    else
       {

        fptr->filStBlk = 0;           /* 1st alloc. block of data fork (obs.) */
        fptr->filLgLen = 0;           /* logical end-of-file of data fork */
        fptr->filPyLen = 0;           /* physical end-of-file of data fork */
        fptr->filRStBlk = 0;          /* 1st alloc. block of res. fork (obs.) */
        fptr->filRLgLen = 0;          /* logical end-of-file of res. fork */
        fptr->filRPyLen = 0;          /* physical end-of-file of res. fork */

        fptr->filCrDat = macDate(); /* date and time of file creation */
        fptr->filMdDat = macDate(); /* date and time of last modification */
        fptr->filBkDat = (long)0;      /* date and time of last backup */

        for( i = 0; i < sizeof(struct FXInfo); i++ )
           {
            *((char *)&fptr->filFndrInfo+i) = '\0';
           }
        fptr->filFndrInfo.fdPutAway = CWDid;

        fptr->filClpSize = 0;        /* zero out file clump size */

        for( i = 0; i < 3; i++ )
           {
            fptr->filExtRec[i].ed_length=(short)0; /* length for data fork */
            fptr->filRExtRec[i].ed_length=(short)0; /* length for res. fork */
            fptr->filExtRec[i].ed_start=(short)0;   /* start for data fork */
            fptr->filRExtRec[i].ed_start=(short)0;  /* start for res. fork */
           }

        fptr->filReserv = '\0';            /* used internally */
        fptr->filRecKey.key.ckr.ckrKeyLen 
           = ( strlen(filename) + sizeof(int) + 2*sizeof(char));
        fptr->filRecKey.key.ckr.ckrResrvl = '\0';
        fptr->filRecKey.key.ckr.ckrParID = CWDid;
        fptr->filRecKey.key.ckr.ckrNameLen = strlen(filename);
        strncpy(fptr->filRecKey.key.ckr.ckrCName, filename, strlen(filename));
       }

    return(retval);
   }


/*--- initDirRec ----------------------------------
 *  initializes dir ptr for adding to catalog tree
 */

int initDirRec( vib, dirptr, dirname, CWDid ) 
    struct m_VIB *vib;
    struct dir_record *dirptr;
    char      *dirname;
    int        CWDid;
   {

    int i, retval = E_NONE;
    char *funcname="initDirRec";

    buildRecKey(&dirptr->dirRecKey, CWDid, dirname);

    dirptr->dirType = DIRECTORY_REC;  /* always 1 for directory records */
    dirptr->dirReserv2 = '\0';        /* used internally */
    dirptr->dirFlags = 0;             /* flags */
    dirptr->dirVal  = 0;              /* valence: # of files in this directory */

    if( (dirptr->dirDirID = macGetNextID(vib)) == -1 )    /* ID number */
       {
        retval = set_error(E_RANGE, Module,funcname,
                           "Can't get new directory ID");
       }
    else
       {
        dirptr->dirCrDat = macDate();        /* date/time of file creation */
        dirptr->dirMdDat = dirptr->dirCrDat; /* date/time of last modification */
        dirptr->dirBkDat = (long)0;          /* date/time of last backup */


        /* info used by the Finder */
       for( i = 0; i < sizeof(struct DInfo); i++ )
           {
            *((char *)&dirptr->dirUsrInfo+i) = '\0';
           }

       for( i = 0; i < sizeof(struct DXInfo); i++ )
           {
            *((char *)&dirptr->dirFndrInfo+i) = '\0';
           }
       /* WHAT ABOUT frOpenChain? */
       dirptr->dirFndrInfo.frPutAway = CWDid;

       for( i = 0; i < TYP_FNDR_SIZE; i++ )
           {
            *((char *)dirptr->dirReserv+i) = '\0';
           }
       }

    return(retval);
   }
/*--- initThdRec ----------------------------------
 *  initializes thread ptr for adding to catalog tree
 */

int initThdRec( dirptr, thdptr ) 
    struct dir_record *dirptr;
    struct thread_record *thdptr;
   {

    int i, retval = E_NONE;

    buildRecKey(&thdptr->thdRecKey, dirptr->dirDirID, "");

    thdptr->thdType = THREAD_REC;            /* always 3 for thread records */
    thdptr->thdReserv2 = '\0';               /* used internally */

    for( i = 0; i < THREAD_RES_SIZE; i++ )
       {
        *(thdptr->thdReserv + i) = '\0';
       }

    thdptr->thdParID = dirptr->dirRecKey.key.ckr.ckrParID;

    strncpy(thdptr->thdCName, dirptr->dirRecKey.key.ckr.ckrCName,
            (int)dirptr->dirRecKey.key.ckr.ckrNameLen );
    thdptr->thdCNameLen = dirptr->dirRecKey.key.ckr.ckrNameLen;

    return(retval);
   }

/*--- macGetNextID ----------------------------------
 *  increments vib->drNxtCNID and returns original value
 */

int macGetNextID( vib ) 
    struct m_VIB *vib;
   {
    vib->drNxtCNID++;
    return(vib->drNxtCNID-1);
   }



/*-- macAddExtentToVib --------------
 *
 */

void macAddExtentToVib( vib, aucount )
    struct m_VIB *vib;
    int aucount;
   {
    int temp;
  
    temp= vib->drFreeBks - aucount;
    vib->drFreeBks=temp; 

   }

/*-- macAddExtentToVbm --------------
 *
 */

void macAddExtentToVbm( vbm, austart, aucount )
    struct m_VBM *vbm;
    unsigned int     austart;
    unsigned int     aucount;
   {
    int a, b;

    for ( a = austart, b = austart+aucount; a < b; a++ ) 
       {
        macSetBit( (char *)vbm, a );
       }

   }

/*-- macRemoveExtentFromVbm --------------
 *
 */

void macRemoveExtentFromVbm( vbm, austart, aucount )
    struct m_VBM *vbm;
    unsigned int     austart;
    unsigned int     aucount;
   {
    int a, b;

    for ( a = austart, b = austart+aucount; a < b; a++ ) 
       {
        macClearBit( (char *)vbm, a );
       }

   }

/*-- macAddExtentToFile --------------
 *
 */

int macAddExtentToFile( fd, austart, aucount )
    struct m_fd *fd;
    unsigned int     austart;
    unsigned int     aucount;
   {
    int retval = E_NONE;
    int a; 
  
    /* update fork information */
    for ( a = 0; a < 3; a++ )
       {
	if ((fd->fdExtents[a].ed_start == 0 ) || 
			(fd->fdExtents[a].ed_start == austart))
           {
            fd->fdExtents[a].ed_length = aucount;
            fd->fdExtents[a].ed_start = austart;

            /* mark the extent node dirty if necessary */
            if ( fd->fdExtentRec &&
                fd->fdExtents == fd->fdExtentRec->xkrExtents )
               {
                macSetBitmapRec( &fd->fdVolume->extBitmap, fd->fdExtentNode );
               }
            break;
           }
       }

    /* if necessary, allocate a new set of extent pointers */
    if ( a == 3 )
       {
        if ( (retval = macAddExtentsList(fd)) == E_NONE )
           {
            fd->fdExtents[0].ed_length = aucount;
            fd->fdExtents[0].ed_start = austart;
           }
       }
    fd->fdExtentNew = NO;

    return( retval );
   }


/*--- macFreeExtentArray ----------------------------------
 * 
 */

void macFreeExtentArray(  vib, vbm, edArray, blockCount ) 
    struct m_VIB  *vib;
    struct m_VBM  *vbm;
    struct extent_descriptor edArray[];
    unsigned int  *blockCount;
   {
    unsigned int  startBlk = 0, 
        index = 0,
        currCt = 0;
 
    *blockCount = 0;

    for( index = 0; index < 3 && edArray[index].ed_start; index++ )
       {
        startBlk = edArray[index].ed_start;

        for( currCt = 0; currCt < edArray[index].ed_length; currCt++ )
           {
            macClearBit( (char *)vbm, startBlk + currCt );
           }

        /* update vib->drFreeBks count */
        vib->drFreeBks += edArray[index].ed_length;

        /* update blockCount */
        *blockCount += edArray[index].ed_length;

        edArray[index].ed_start = 0;
        edArray[index].ed_length = 0;
       }
   }

/*------------------ macAllocFileExtent ----------------------
 *
 * Find largest extent, initialize extent descriptor,
 * and allocate buffer for extent
 *
 * The current allocation scheme attempts to extend an extent by
 * vib->drClpSiz bytes before locating a new extent. In order to
 * prevent crosslinking of files, the VBM is marked as it is allocated,
 * then any unused amount is unmarked in macFlushFileExtent() using
 * macRemoveExtentFromVbm().
 *
 */

int macAllocFileExtent( fd ) 
    struct m_fd      *fd;
   {
    int        retval = E_NONE;
    struct extent_descriptor *temp_ed;

    if ( (retval = macFindExtent(fd,&temp_ed)) == E_NONE )
       {
        fd->fdExtentLoc->ed_start = temp_ed->ed_start;
        fd->fdExtentLoc->ed_length = temp_ed->ed_length;
        fd->fdEbufSize = 0;
        fd->fdEbufOffset = 0;
       }
    if ( retval == E_NOTFOUND )
       {
        retval = clear_error();
        if ( (retval = macFindBiggestExtent(fd->fdVolume->vib,
                   fd->fdVolume->vbm,fd->fdExtentLoc)) == E_NONE )
           {
            if ( fd->fdExtentLoc->ed_length*fd->fdVolume->vib->drAlBlkSiz >
                fd->fdVolume->vib->drClpSiz )
               {
                fd->fdExtentLoc->ed_length =
                    fd->fdVolume->vib->drClpSiz/fd->fdVolume->vib->drAlBlkSiz;
               }

            macAddExtentToVbm(fd->fdVolume->vbm, fd->fdExtentLoc->ed_start,
                fd->fdExtentLoc->ed_length);
            fd->fdExtOffset = 0;
            fd->fdExtBlock = 0;
            fd->fdEbufSize = 0;
            fd->fdEbufOffset = 0;
            fd->fdExtentNew = YES;
           }
       }

    return(retval);
   }


/*-- macFindBiggestExtent -------
 *
 */

int macFindBiggestExtent( vib, vbm, ed )
    struct m_VIB *vib;
    struct m_VBM *vbm;
    struct extent_descriptor *ed;
   {
    int retval = E_NONE;
    struct extent_descriptor tempextent;
    unsigned short in_extent,a;
    char *funcname="macFindBiggestExtent";

    ed->ed_start = 0;
    ed->ed_length = 0;
    tempextent.ed_length = 0;

    for ( a = 0, in_extent = 0; a < vib->drNmAlblks; a++ )
       {
        if ( !macGetBit((char *)vbm,a) )
           {
            if ( in_extent )
               {
                tempextent.ed_length++;
               }
            else
               {
                tempextent.ed_start = a;
                tempextent.ed_length = 1;
               }
            in_extent = 1;
           }
        if ( macGetBit((char *)vbm,a) || a+1 == vib->drNmAlblks )
           {
            if ( in_extent && ed->ed_length < tempextent.ed_length )
               {
                ed->ed_start = tempextent.ed_start;
                ed->ed_length = tempextent.ed_length;
               }
            in_extent = 0;
           }
       }

    if ( ed->ed_length <= 0 )
       {
        retval = set_error( E_SPACE, Module, funcname, 
                           "Can't find extent large enough" );
       }

    return( retval );
   }

/*-- macExtendCurrentExtent -------
 *
 */

int macExtendCurrentExtent( fd )
    struct m_fd *fd;
   {
    int retval = E_NONE, a;
    char *funcname="macExtendCurrentExtent";
    short new_amount = 0;
    unsigned int alloc_amount;
    int size;

    if ( fd->fdExtentLoc->ed_length > 0 ) /* there is a current extent */
       {
        for ( a = fd->fdExtentLoc->ed_start+fd->fdExtentLoc->ed_length,
            new_amount = 0;
            new_amount*fd->fdVolume->vib->drAlBlkSiz <
            fd->fdVolume->vib->drClpSiz &&
            a < (int)fd->fdVolume->vib->drNmAlblks;
            a++ )
           {
            if ( !macGetBit((char *)(fd->fdVolume->vbm),a) )
               {
                new_amount++;
               }
            else
               {
                break;
               }
           }
       }

    if ( new_amount <= 0 )
       {
        retval = set_error( E_NOTFOUND, Module, funcname, "" );
       }
    else
       {
        macAddExtentToVbm(fd->fdVolume->vbm, fd->fdExtentLoc->ed_start +
            fd->fdExtentLoc->ed_length, new_amount);
        fd->fdExtentLoc->ed_length += new_amount;

        /* realloc extent buffer if possible */
        /* allocate our buffer up to an alignment boundary */
        if ( (alloc_amount = (fd->fdEbufSize +
            (new_amount*fd->fdVolume->vib->drAlBlkSiz))) > MAX_EBUFSIZE )
           {
            alloc_amount =
                (int)(MAX_EBUFSIZE/fd->fdVolume->vib->drAlBlkSiz)*fd->fdVolume->vib->drAlBlkSiz;
           }

        else if ( (fd->fdExtent = realloc(fd->fdExtent,
            (unsigned)(alloc_amount))) == 0 )
           {
            retval = set_error( E_MEMORY, Module, funcname, "" );
           }
        else
           {
            fd->fdEbufSize = alloc_amount;
           }
       }

    return( retval );
   }


