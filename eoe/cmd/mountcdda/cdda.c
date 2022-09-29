/*
 *  cdda.c
 *
 *  Description:
 *     Routines called from nfs_server that fake Unix into thinking the CD
 *     is in some file system format.
 *
 *  Notes:
 *     I present the CD tracks as .aiff files.  This enables a bunch of utilities 
 *     like soundplayer and cp to "just work".  To my knowledge, the cdda format is 
 *     not used on disk.  The subcode info becomes nonsense in that context.
 *     
 */

#include <dslib.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/socket.h>
#include <sys/sysmacros.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <rpcsvc/ypclnt.h>

#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <filehdr.h>
#include <ctype.h>
#include <exportent.h>
#include <netdb.h>
#include <elf.h>

#include <ulocks.h>

#include "key.h"
#include "cdda.h"
#include "util.h"
#include "main.h"

#define TERM_COOKIE (-1)
   
#define ROOT_DIR() getfd(cd->dsp)

#define PERM_RALL   0444
#define PERM_XALL   0111
#define PERM_RXALL  0555


#define max(a,b) ((a) < (b) ? (b) : (a))

#define CHARSTOLONG(chars) ((chars)[0] << 24 | (chars)[1] << 16 \
			    | (chars)[2] << 8 | (chars)[3])
#define CHARSTOSHORT(chars) ((chars)[0] << 8 | (chars)[1])

/* #define CDDADBG(x) {x;}  */

#define CDDADBG(x) {}

#define MAX_INFO_STRING_SIZE  200
#define AIFF_HEADER_SIZE 54
/*
 * Sometimes we need to differentiate between iso9660 and high sierra
 */
typedef enum cdtype { CDDA, ISO, HSFS } cdtype_t;

static char dot[] = ".";
static char dotdot[] = "..";
static char disctrackname[] = "info.disc";
/*
 * if notranslate == 1, user will see raw, ugly file names
 */
static int  notranslate = 0;
/*
 * if setx == 1, we'll blindly set execute permissions for every file.
 */
static int  setx = 0;
static int  def_num_blocks = 256;

/* These are internal to player.c so I need to copy them here. */

enum drivetype { UNKNOWN, TOSHIBA, SONY, TOSHIBA_SCSI2 };

typedef struct {
	int min, sec, frame;
}   TRACK;

struct cdplayer {
  unsigned int    paused : 1; /* For Sony; drive automatically pauses,
			       * so we use this to tell if we should set
			       * pause state or not
			       */
  int             track;      /* Current track being played */
  int index;
  int             first;      /* Number of first track on CD */
  int             last;       /* Number of last track on CD */
  int             min;        /* Minutes of current track */
  int             sec;        /* Seconds of current track */
  int             frame;      /* Frame of current track */
  int             abs_min;    /* Minutes of CD */
  int             abs_sec;    /* Seconds of CD */
  int             abs_frame;  /* Frames of CD */
  int             total_min;  /* Total minutes on CD */
  int             total_sec;  /* Total seconds on CD */
  int             total_frame;/* Total frames on CD */
  int		scsi_audio; /* Player can do SCSI audio transfer */
  unsigned long	cur_block;  /* Logical block no. of current location */
  struct dsreq    *dsp;       /* Struct for doing SCSI requests */
  int		excl_id;    /* Exclusive use id from mediad */
  int             state;      /* State of player */
  enum drivetype  type;       /* Type of the CDROM drive */
  TRACK           *toc;       /* Table of contents */
  char            *dev;       /* name of dev scsi device */
};


/* extern CDPLAYER *cd; */

int cdFirst, cdLast;

CDTRACKINFO cdInfo[100];
 
/*
 * Number of blocks in the mounted file system
 */
static int fsBlocks;
/*
 * Type of file system - iso9660 or high sierra
 */
static cdtype_t fsType;
/*
 * The root file handle of the mounted file system
 */
static fhandle_t rootfh;
/*
 * The path name of the mount point of the file system
 */
static char* mountPoint;

/* The name of the scsi device */
static char* devName;

/* These guys are used for our own attr (semi)caching. */

int cachedFno;
fattr* cachedFattr;


char keyString[KEY_SIZE];

char infoFileString[MAX_NFS_BUFFER_SIZE];

/* This function is a blackbox to me. I don't dare change anything, because if the
   key generated any differently, then the existing SGI cd database would be invalid.
*/
void set_key()
{
  int i;
  int nCDTracks = cd->last - cd->first + 1;
  int len = 0;
  int min = 0;
  int sec = 0;
  CDTRACKINFO	info;
  int nIDTracks;
  char* tmpStr = keyString;

  /* map defined in key.h is a macro which produces a side effect on keyString */
  map(tmpStr, len, ((nCDTracks>>4)&0xf) );
  map(tmpStr, len, (nCDTracks&0xf) );

  if (nCDTracks <= DB_ID_NTRACKS) {
    nIDTracks = nCDTracks;
  }
  else {
    nIDTracks = DB_ID_NTRACKS - 1;
    for (i = cd->first; i <= cd->last; i++) {
      CDgettrackinfo(cd, i, &info );
      min += info.total_min;
      sec += info.total_sec;
    }
    min+= sec / 60;
    sec = sec % 60;
    map(tmpStr, len, min);
    map(tmpStr, len, sec);
  }

  for (i = 0; i < nIDTracks; i++) {
    if (cd->first+i <= cd->last) {
      CDgettrackinfo( cd, cd->first+i, &info );
      map(tmpStr, len, info.total_min );
      map(tmpStr, len, info.total_sec );
    }
  }
  *tmpStr++ = '\0';
} 


/*
 * Forward declarations of static functions
 */
/* static int set_blocks(); */
static void add_entry(entry *entries, int fd, char *name, int
		      name_len, int cookie);

/*
 *  int
 *  cdda_numblocks(unsigned int *blocks)
 *
 *  Description:
 *      Fill in blocks with the number of blocks in the file system
 *
 *  Parameters:
 *      blocks  receives # of blocks
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */
int cdda_numblocks(unsigned int *blocks)
{
  int  error, changed;
  *blocks = (unsigned int)fsBlocks;
  CDDADBG(fprintf(stderr, "cdda_numblocks() == %d\n", *blocks));
  return 0;
}

/*
 *  int
 *  cdda_isfd(int fd)
 *
 *  Description:
 *      Determine whether fd is a file descriptor that we care about, so
 *      that the caller knows not to close it
 *
 *  Parameters:
 *      fd      file descriptor
 *
 *  Returns:
 *      1 if we care, 0 if we don't
 */

int
cdda_isfd(int fd)
{
  return (cd->dsp && getfd( cd->dsp ) == fd);
}

void virtual_info_file(char* data)
{
  char tmpString[MAX_INFO_STRING_SIZE];
  int fill, i, n;

  fill = 0;
  bzero(infoFileString, MAX_NFS_BUFFER_SIZE);
  set_key();

  sprintf(tmpString, "album.key: %s\n", keyString);
  n = strlen(tmpString);
  strncpy(data,tmpString, n);
  fill += n;
  sprintf(tmpString, "album.duration: %02d:%02d:%02d\n",
	  cd->total_min, cd->total_sec, cd->total_frame);
  n = strlen(tmpString);
  strncpy(data + fill,tmpString, n);
  fill += n;
  sprintf(tmpString, "album.tracks: %d\n", cd->last);
  n = strlen(tmpString);
  strncpy(data + fill,tmpString, n);
  fill += n;

  for (i = cd->first; i <= cd->last; i++) {
    sprintf(tmpString, "track%d.start: %02d:%02d:%02d\ntrack%d.duration: %02d:%02d:%02d\n",
	    i,cdInfo[i].start_min, cdInfo[i].start_sec, cdInfo[i].start_frame,
	    i,cdInfo[i].total_min, cdInfo[i].total_sec, cdInfo[i].total_frame);
    n = strlen(tmpString);
    strncpy(data + fill,tmpString, n);
    fill += n;
  }
}

/* Used primarily to give files new modification times across ejects.
 */
int openTime;


/* This our hack to prevent multiple reads.  Zero means ok-to-read, one means
   don't.   
 */
int readLock;			

/*
 *  void
 *  cdda_removefs()
 *
 *  Description:
 *      CDDA 9660 specific removal of a generic file system
 *
 *  Parameters:
 */

void cdda_removefs()
{
  if (cd) {
    CDclose(cd);
    cd = NULL;
    cdLast = 0;
  }

}

/* link the frames together for seamless buffering
 */
int frameBufferIndex, frameAudioIndex,  readFrameCount;
int bufferMinOffset, bufferMaxOffset, offsetCache;
int trackNumCache; 
unsigned long initialTrackBlock;


/*
 *  int
 *  cdda_openfs(char *dev, char *mntpnt, int flags, int partition)
 *
 *  description:
 *	Get ready to mount the dev on mntpnt
 *
 *  Parameters:
 *      dev         Device for CD-ROM drive
 *      mntpnt      The mount point
 *      root        gets root file handle
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */
int cdda_openfs(char *dev, char *mntpnt, fhandle_t *root)
{
  int error = 0;
  int i;
  static dev_t        fakedev = 1;
  struct stat         sb;
  struct dsreq*  dsp2;
    
  CDDADBG(fprintf(stderr, "cdda_openfs(%s)\n", dev));

  cachedFno = -1;
  cachedFattr = (fattr*)malloc(sizeof(fattr));
  frameBufferIndex = frameAudioIndex = readFrameCount = 0;
  bufferMinOffset = bufferMaxOffset = offsetCache = 0;
  trackNumCache = 0;
  initialTrackBlock = 0;

  if (cd) {
    CDclose(cd);
  }

  cd = CDopen(dev, "r");
  devName = dev;

  if (!cd) {
    fprintf(stderr,"fstat failed...\n");
    return (errno);
  }
  CDpreventremoval(cd);
    
  if (fstat( getfd( cd->dsp), &sb ) < 0) {
    fprintf(stderr,"fstat failed...\n");
    return (errno);
  }

  rootfh.fh_dev = S_ISREG(sb.st_mode) ? 0xff00 | (fakedev++ & 0xff)
 : sb.st_rdev;
  mountPoint = strdup(mntpnt);
    
  fsType = CDDA;
  CDDADBG(fprintf(stderr, "  cdda_openfs: Disc is in CDDA format\n"));
    
  fsBlocks = cd->total_frame + (cd->total_sec * 75) + (cd->total_min * 60 *75);
    

  rootfh.fh_fno = ROOT_DIR();
  rootfh.fh_fid.lfid_len = sizeof rootfh.fh_fid -
 sizeof rootfh.fh_fid.lfid_len;

  /*
   * We need to do this so that the user can eject.
   */
  if (error) {
    CDclose(cd);
  } else {
    bcopy(&rootfh, root, sizeof *root);
  }

  cdFirst = cd->first;
  cdLast = cd->last;

  bzero(cdInfo, 99*sizeof(CDTRACKINFO));
  for (i = cdFirst; i <= cdLast; i++) 
    CDgettrackinfo( cd, i, &cdInfo[i]);

  /* "make" the info.disc" file */
  virtual_info_file(infoFileString);

  CDclose(cd); /* prevents removal too */
  free(cd);
  cd = NULL;

  openTime = time(NULL);
  readLock = 0;
  return error;
}


/*
 *  int
 *  cdda_lookup(fhandle_t *fh, char *name, fhandle_t *fh_ret)
 *
 *  Description:
 *      Look for name in fh (a directory); if found, fill in fh_ret
 *
 *  Parameters:
 *      fh      file handle of directory in which to look
 *      name    name of file we're looking for
 *      fh_ret  Receive handle for name
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */
int cdda_lookup(fhandle_t *fh, char *name, fhandle_t *fh_ret)
{
  int blksize;
  int trackNum;
  int retval;

  CDDADBG(fprintf(stderr, "cdda_lookup(%d, %s)\n", fh->fh_fno, name));

  if (fh->fh_fno != rootfh.fh_fno) { /* this is the only directory we got */
    retval= ENOTDIR;
  }
  else if (strcmp(name, dot) == 0) {
    *fh_ret = *fh;
    retval = 0;
  }
  else if (strcmp(name, disctrackname) == 0) {
    bzero(fh_ret, sizeof *fh_ret);
    fh_ret->fh_dev = fh->fh_dev;
    fh_ret->fh_fid.lfid_fno = rootfh.fh_fno + cdLast + 1;
    sizeof fh_ret->fh_fid - sizeof fh_ret->fh_fid.lfid_len;
    retval = 0;
  }
  else {
    /* Names are easy.  They're just track numbers.*/

    sscanf(name, "Track%02d.aiff", &trackNum);

    if (cdLast && trackNum >= cdFirst && trackNum <= cdLast) {
      bzero(fh_ret, sizeof *fh_ret);
      fh_ret->fh_dev = fh->fh_dev;
      fh_ret->fh_fid.lfid_fno = rootfh.fh_fno + trackNum;
      fh_ret->fh_fid.lfid_len =
	sizeof fh_ret->fh_fid - sizeof fh_ret->fh_fid.lfid_len;
      retval = 0;
    }
    else
      retval = ENOENT;
  }
  return retval;
}
		


/* This is very cute.  Its the byte pattern to represent 44.1k in IEEE 754
   (Apple SANE).
 */
char IEEEfor44_1k[10] = { 0x40, 0x0e, 0xac, 0x44, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00 };

int mfs_to_bytes(int m, int s, int f)
{
  return CDDA_DATASIZE * (f + (s*75) + (m*75*60));
}

/* Writes an integer a byte at a time into "data" and updates the pointer into "data"
 */
void byte_write(char* data, int* fill, int value, int numBytes)
{
  int i;
  for (i = numBytes-1; i >= 0; i--, (*fill)++) 
    data[*fill] = (char)(value >> (8*i));
}

unsigned long offset_to_cdblock(unsigned long offset, int* audioIndex)
{
  unsigned long dataOffset = offset - AIFF_HEADER_SIZE;

  /* The modulus 4 term is to get the pointer back on a stereo sample */
  /* boundary. */
  *audioIndex = (dataOffset % CDDA_DATASIZE) - (dataOffset % 4); 
  return dataOffset / CDDA_DATASIZE;
}


int cdblock_to_offset(unsigned long blockNum)
{
  return ((int)(blockNum-initialTrackBlock))*CDDA_DATASIZE + AIFF_HEADER_SIZE;
}

/* double buffering data structures */

#define CDFRAMEBUFFERSIZE 12

CDFRAME cdFrameBuffer[CDFRAMEBUFFERSIZE];


/* these get carried across calls to cdda_read()
 */
int dataSize, fileSize;

/* Fake the aiff header 
*/
void virtual_aiff_header(char* data, int* fill)
{
  int i;
  /* Form Chunk */
  strcpy(data,"FORM");
  *fill += 4;
  byte_write(data, fill, (fileSize - 8), 4);
  strcpy(data+(*fill),"AIFF");
  *fill += 4;
  /* Common Chunk */
  strcpy(data+(*fill),"COMM");			
  *fill += 4;
  byte_write(data, fill, 18, 4);		/* ckDataSize */
  byte_write(data, fill, (short)2, 2);		/* numChannels */
  byte_write(data, fill, (dataSize / 4), 4);	/* numSampleFrames */
  byte_write(data, fill, (short)16, 2);		/* SampleSize */
  for (i = 0; i < 10; i++) 
    data[*fill+i] = IEEEfor44_1k[i];		/* SampleRate (hard-wired float) */
  *fill += 10;
  /* Sound Data Chunk */
  strcpy(data+(*fill), "SSND");
  *fill += 4;
  byte_write(data, fill, (dataSize + 8), 4);
  byte_write(data, fill, 0, 4);
  byte_write(data, fill, 0, 4);

}


bool_t readingCdBuffer = FALSE;
pid_t bufferingThread = NULL;

void FillCdFrameBuffer()
{
  int i;

  bufferMinOffset = cdblock_to_offset(cd->cur_block);
  readFrameCount =
    CDreadda(cd, cdFrameBuffer, CDFRAMEBUFFERSIZE);
  bufferMaxOffset = cdblock_to_offset(cd->cur_block);
  CDDADBG(fprintf(stderr,"filling %d blocks: minOffset = %d, maxOffset = %d, cur_block = %d\n",
		  readFrameCount, bufferMinOffset, bufferMaxOffset, (int)cd->cur_block));
  if (readFrameCount != CDFRAMEBUFFERSIZE) 
    CDDADBG(fprintf(stderr,"warning: asked for %d blocks, got %d blocks.\n",
		    CDFRAMEBUFFERSIZE, readFrameCount));
  frameBufferIndex = frameAudioIndex = 0; 
}


/*
 *  int
 *  cdda_read(fhandle_t *fh, int offset, int count, char *data,
 *           unsigned int *amountRead)
 *
 *  Description:
 *      Read count bytes from file fh into buffer data
 *
 *  Parameters:
 *      fh          file handle of file from which to read
 *      offset      offset into file to start reading
 *      count       number of bytes to read including aiff header
 *      data        buffer to read bytes into
 *      amountRead  Amount of data actually read
 *
 *  Returns:
 * 0 if successful, error code otherwise
 */
int cdda_read(fhandle_t* fh, int offset, int count, char* data,
	      unsigned int* amountRead)
{
  int i, fill, trackNum, frames;
  bool_t initBuffer = FALSE;
  TRACK* trackPtr;
  int statptr;
  *amountRead = 0;

  CDDADBG(fprintf(stderr, "cdda_read(%d, %d, %d)\n", fh->fh_fno, offset, count));

  if (readLock)			/* block multiple reads from cdrom */
    return EBUSY;
  else
    readLock++;

  trackNum = fh->fh_fno - rootfh.fh_fno;
  fill = 0;			/* fill is the byte-pointer into the file. */

  /* handle the info file */

  if (trackNum == cdLast+1) {
    if ((offset != 0) || (count < KEY_SIZE)) {
      fprintf(stderr,"Not handling key file...\n");
      readLock = 0;
      return EIO;
    }
    strcpy(data, infoFileString);
    *amountRead = strlen(infoFileString);
    readLock = 0;
    return 0;
  }

  if (!cd)
    cd = CDopen(devName, "r");

  if (!cd) {
    readLock = 0;		/* Busy, but not because of NFS read. */
    return (EBUSY);
  }
  schedctl(NDPRI, 0, NDPHIMAX);

  if (offset == 0 && count <= AIFF_HEADER_SIZE) {
    fprintf(stderr,"Read smaller than AIFF header size (%d bytes)...\n", count);
    readLock = 0;
    return EIO;
  }

  if (offset == 0  || trackNum != trackNumCache ) {
    initBuffer = TRUE;
    dataSize = mfs_to_bytes(cdInfo[trackNum].total_min,cdInfo[trackNum].total_sec,
			    cdInfo[trackNum].total_frame);
    fileSize = dataSize + AIFF_HEADER_SIZE;
    CDseektrack(cd, trackNum);
    initialTrackBlock = cd->cur_block;
    frameAudioIndex = 0;
    /* when the offset is 0 we write an aiff header */      
    if (offset == 0)
      virtual_aiff_header(data, &fill);
    else
      CDseekblock(cd, initialTrackBlock + offset_to_cdblock((unsigned long)offset, 
							    &frameAudioIndex));

  } else if (offset >= fileSize) {
    /* The offset overshot the file end  */
    CDDADBG(fprintf(stderr, "cdda_read: offset(%d) > filesize(%d); returning 0", offset,
		    fileSize));
    
    *amountRead = 0;
    readLock = 0;
    return 0;

  } else if (offset < bufferMinOffset || offset >= bufferMaxOffset) {
    /* The offset came back out of the buffer's range */
    initBuffer = TRUE;
    CDseekblock(cd, initialTrackBlock + offset_to_cdblock((unsigned long)offset, 
 							  &frameAudioIndex));
  } else if (offset != offsetCache) {
    /* offset is off but still in the buffer */
    frameBufferIndex = offset_to_cdblock(offset-bufferMinOffset, &frameAudioIndex) ;
    CDDADBG(fprintf(stderr,
		    "offset is off but still in buffer: offsetCache = %d,  offset = %d, bufferMinOffset = %d, bufferMaxOffset = %d\n",
		    offsetCache, offset, bufferMinOffset, bufferMaxOffset));
  }

  /*
   * FIGURE out where the end of the file is, and adjust count
   * accordingly.
   */  
  count = MIN(count, fileSize - offset);

  if (initBuffer) {

    CDDADBG(fprintf(stderr,
		    "initializing CD buffer:  offset = %d, bufferMinOffset = %d, bufferMaxOffset = %d\n",
		    offset, bufferMinOffset, bufferMaxOffset));
    bufferMinOffset = bufferMaxOffset = offset;

    /* buffer up a mess 'o audio data */
    FillCdFrameBuffer(); 
    if (readFrameCount < 0) { 
      printf("bogus readFrameCount of %d...\n", readFrameCount); 
      readLock = 0;
      return oserror(); 
    } 
  }
    
  for (; frameBufferIndex < readFrameCount && fill < count;
       frameBufferIndex++) {
    for (; fill < count && frameAudioIndex < CDDA_DATASIZE; fill += 2, frameAudioIndex += 2) {
      data[fill+1] = cdFrameBuffer[frameBufferIndex].audio[frameAudioIndex];
      data[fill] = cdFrameBuffer[frameBufferIndex].audio[frameAudioIndex+1];
      if ((frameAudioIndex >= CDDA_DATASIZE-2 &&
	   frameBufferIndex >= readFrameCount-1)
	  || (offset + fill >= bufferMaxOffset)) { 
	/* we ran off the end of the cd buffer */
	/* we are empty, fill the buffer synchronously */
	FillCdFrameBuffer();
	frameBufferIndex = 0;
	frameAudioIndex = -2;
	if (readFrameCount == 0) /* CDreadda() returned 0.  No more frames. */
	  break;
      }
      if (offset+fill > bufferMaxOffset)
	CDDADBG(fprintf(stderr, "offset + fill %d > bufferMaxOffset %d.\n",
			offset+fill, bufferMaxOffset));
    }
    if (frameAudioIndex == CDDA_DATASIZE)
      frameAudioIndex = 0;
  }
  frameBufferIndex--;
  *amountRead = fill;
  offsetCache = offset+fill;
  trackNumCache = trackNum;
  if (offsetCache > bufferMaxOffset)
    CDDADBG(fprintf(stderr, "offsetCache %d > bufferMaxOffset %d.\n", offsetCache, bufferMaxOffset));
  CDDADBG(fflush(stderr));
  schedctl(NDPRI, 0, 0);

  readLock = 0;
  return 0;
}

/*
 *  static void
 *  add_entry(entry *entries, int fd, char *name, int name_len, int cookie)
 *
 *  Description:
 *      Helper function for cdda_readdir().  Puts fd and name into an entry,
 *      and sets entries->nextentry to point to the place for the next
 *      entry.
 *
 *  Parameters:
 *      entries     buffer for entries
 *      fd          file descriptor to add to entries
 *      name        name of file to add to entries
 *      name_len    lengthe of name
 *      cookie      cookie for next entry
 */

static void
add_entry(entry *entries, int fd, char *name, int name_len, int cookie)
{
    char *ptr;
    
    entries->fileid = fd;
    /*
     *  We copy the name to the memory immediately following this entry.
     *  We'll set entries->nextentry to the first byte afterwards that we
     *  can use, taking alignment into consideration.
     */
    entries->name = (char *)(entries + 1);
    strncpy(entries->name, name, name_len);
    entries->name[name_len] = '\0';
    *(int *)entries->cookie = cookie;
    entries->nextentry = (entry *)((char *)(entries + 1) + name_len + 1);
    ptr = (char *)entries->nextentry;
    /*
     *  Fix alignment problems
     */
    if ((int)ptr % 4) {
	ptr = ptr + 4 - ((int)ptr % 4);
	entries->nextentry = (entry *)ptr;
    }
}


/*
 *  int
 *  cdda_readdir(fhandle_t *fh, entry *entries, int count, int cookie,
 *   bool_t *eof, int *nread)
 *
 *  Description:
 *      Read up to count byes worth of directory entries from the directory
 *      described by fd
 *
 *  Parameters:
 *      fh          file handle of directory whose entries we want
 *      entries     Buffer to receive entries
 *      count       Maximum # of bytes to read into entries
 *      cookie      offset into directory entry from which to start
 *      eof         set to 1 if we've read the last entrie
 *      nread       set to the number of entries read
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int cdda_readdir(fhandle_t *fh, entry *entries, int count, int cookie,
		 bool_t *eof, int *nread)
{
    dir_t  *dirp, *contents, *dp, *newdirp;
    int error, fileid, len, newfd;
    char uname[NFS_MAXNAMLEN];
    entry *lastentry = 0, *entrybase = entries;
    int blksize;
    
    CDDADBG(fprintf(stderr, "cdda_readdir(%d, %d) == \n", fh->fh_fno, cookie));


    if (fh->fh_fno != rootfh.fh_fno) { /* this is the only directory we got */
    return ENOTDIR;
    }

    /*
     *  We do this to clue the nfs_server module into the fact that there
     *  are no entries left to be read.  It turns out that the NFS client
     *  won't listen to us when we set *eof == 0; the nfs_server module
     *  has to actually return a NULL entry pointer in order to convince
     *  the client that there are no more entries.
     */
    *nread = 0;
    if (cookie == TERM_COOKIE) {
/*       closeDsp(cd);  */
      return 0;
    }

    blksize = cdda_getblksize();
    
    /* handle "./"  */

    add_entry(entries, fh->fh_fno, dot, strlen(dot), 0);
    lastentry = entries; 
    *nread = 1;

    /*
     *  Now do the rest
     */
    for (; *nread <= cdLast; (*nread)++) {
      entries = entries->nextentry;
      fileid = rootfh.fh_fno + *nread;
      sprintf(uname,"Track%02d.aiff", *nread);
      CDDADBG(fprintf(stderr,"readdir of %s\n", uname));      
      add_entry(entries, fileid, uname, strlen (uname), 0);
      lastentry = entries;
    }
    
    /* handle "../" and the keyfile */
    
    (*nread)++;
    entries = entries->nextentry;
    add_entry(entries, fh->fh_fno, dotdot, strlen(dotdot), 0);
    lastentry = entries; 
    (*nread)++;
    entries = entries->nextentry;
    add_entry(entries, fh->fh_fno, disctrackname, strlen(disctrackname), 0);
    lastentry = entries; 

    *eof = 1;

    if (lastentry) {
	lastentry->nextentry = 0;
	if (*eof) {
	    *(int *)lastentry->cookie = TERM_COOKIE;
	}
    }
    return 0;
}

/*
 *  int
 *  cdda_readraw(unsigned long block, void *buf, unsigned long count)
 *
 *  Description:
 *      Read raw data from the CD.  This is here for debugainging support
 *      for the testcd test program.
 *
 *  Parameters:
 *      block   block on the CD to read from
 *      buf     memory to read into
 *      count   number of bytes to read
 *
 *  Returns:
 *	0 if successful, error code if error
 */

int cdda_readraw(unsigned long offset, void *buf, unsigned long count)
{

    /*
     * If the test program is trying to read the volume descriptor,
     * the block number will be 16, and we should use CDROM_BLKSIZE.
     * Otherwise, it's asking for data, and we should use the
     * block size we got from the volume descriptor.
     */ 
/*     return cd_read(cd, offset == 16 ? 16 * CDDA_BLOCKSIZE : */
/* 		   offset * cd_getblksize(), buf, count, 0); */
  printf("In readraw...\n");
  return 0;
}

/*
 *  void
 *  cdda_disable_name_translations(void)
 *
 *  Description:
 *      Make it so we don't muck with the filenames on the CD at all.  This
 *      results in very ugly file names in all caps, sometimes ending with
 *      a semi-colon and then a number
 */

void cdda_disable_name_translations(void)
{
  notranslate = 1;
}


/*
 *  void
 *  cdda_setx(void)
 *
 *  Description:
 *      Make it so we automatically set execute permission on every
 *      file.  This speeds up attribute checking and enables execution
 *      of files that our algorithm may not give execute permission to.
 */

void cdda_setx(void)
{
  setx = 1;
}

/*
 *  int
 *  cdda_getfs(char *path, fhandle_t *fh, struct svc_req *rq )
 *
 *  Description:
 *	Get a file handle given its mount point.  Also, if rq is
 *	non-NULL, perform check to see if path should be exported to
 *	the requestor represented by rq.
 *
 *  Parameters:
 *      path	Mount point in which we're interested
 *      fh	receives file handle
 *      rq	NFS service request
 *
 *  Returns:
 *	0 if successful, error code otherwise
 */

int cdda_getfs(char *path, fhandle_t *fh, struct svc_req *rq)
{
  FILE *fp;
  struct exportent *xent;
  int	error;
  struct sockaddr_in *sin;
  struct hostent *hp;
  char *access, **aliases, *host;

  /*
   * Make sure they're asking about the file system that we've got
   * mounted.
   */
  if (strcmp(path, mountPoint) != 0) {
    return ENOENT;
  }

  /*
   * Check to see if letting the requestor see this file system is
   * allowed (someone must run exportfs(1M).
   *
   * Note: this code is derived from code found in cmd/sun/rpc.mountd.c
   */
  fp = setexportent();
    
  if (!fp) {
    return EACCES;
  }
    
  error = EACCES;
  while (error && (xent = getexportent(fp))) {
    if (strcmp(path, xent->xent_dirname) == 0) {
      sin = svc_getcaller(rq->rq_xprt);
      hp = gethostbyaddr(&sin->sin_addr, sizeof sin->sin_addr,
			 AF_INET);
      /*
       *	Derived from cmd/sun/rpc.mountd.c
       */
      access = getexportopt(xent, ACCESS_OPT);
      if (!access) {
	error = 0;
      } else {
	while ((host = strtok(access, ":")) != NULL) {
	  access = NULL;
	  if (strcasecmp(host, hp->h_name) == 0
	      || innetgr(host, hp->h_name, NULL, _yp_domain))
	    error = 0;
	  else
	    for (aliases = hp->h_aliases; *aliases && error;
		 aliases++)
	      if (strcasecmp(host, *aliases) == 0
		  || innetgr(access, *aliases, NULL,
			     _yp_domain))
		error = 0;
	}
      }	    

      if (error) {
	access = getexportopt(xent, ROOT_OPT);
      }
      if (access) {
	while ((host = strtok(access, ":")) != NULL) {
	  access = NULL;
	  if (strcasecmp(host, hp->h_name) == 0)
	    error = 0;
	}
	    
      }

      if (error) {
	access = getexportopt(xent, RW_OPT);
      }
      if (access) {
	while ((host = strtok(access, ":")) != NULL) {
	  access = NULL;
	  if (strcasecmp(host, hp->h_name) == 0)
	    error = 0;
	}
      }
    }
  }
  endexportent(fp);
    
  if (!error) {
    bcopy(&rootfh, fh, sizeof *fh);
  }
    
  return error;
}

/*
 *  int
 *  cdda_getblksize(void)
 *
 *  Description:
 *      Get the CD block size for the nfs_server module (which does
 *      not have access to the cd pointer)
 *
 *  Returns:
 *      block size of our file system
 */

int cdda_getblksize(void)
{
    return CDDA_DATASIZE ;
}


/*
 *  int
 *  cdda_getattr(fhandle_t *fh, fattr *fa)
 *
 *  Description:
 *      Fill in fa with the attributes of fh
 *
 *  Parameters:
 *      fh      file handle whose attributes we want
 *      fa      Buffer to receive attributes
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */
int cdda_getattr(fhandle_t* fh,  fattr* fa)
{
  int  blksize, changed;
  char* dev;
    
  CDDADBG(fprintf(stderr, "cdda_getattr(%d)...\n", fh->fh_fno));

  if (cachedFno == fh->fh_fno) {
    memcpy(fa, cachedFattr, sizeof(fattr));
    return 0;
  }
  else {
    cachedFno = fh->fh_fno;
  }


/*   CDtestready(cd, &changed);  */
/*   if (changed) { */
/*     CDDADBG(fprintf(stderr,"CD has changed...\n")); */
/*     dev = cd->dev; */
/*     if (cd)  */
/*       CDclose(cd); */
/*     cd = CDopen(dev, "r"); */
/*     virtual_info_file(infoFileString); */
/*   } */

  /* Attributes common to both regular files and directories */
  fa->blocksize = blksize = cdda_getblksize();
  fa->fsid = fh->fh_dev;
  fa->fileid = fh->fh_fno;
  fa->nlink = 1;
  fa->uid = 0;
  fa->gid = 0;
  /*     fa->uid = NFS_UID_NOBODY; */
  /*     fa->gid = NFS_GID_NOBODY; */
  fa->ctime.seconds = fa->ctime.useconds = 0;
  fa->atime.useconds = fa->mtime.useconds = 0;
  fa->mtime.seconds = fa->atime.seconds =  openTime;

  if (fh->fh_fno == rootfh.fh_fno) { /* we are THE directory */
    fa->blocks = 0;
    fa->type = NFDIR;
    fa->mode = S_IFDIR | PERM_RXALL;
    fa->size = cdLast;
  }
  else {
    fa->type = NFREG;
    fa->mode = (setx) ? PERM_RXALL : PERM_RALL;
    if (fh->fh_fno == rootfh.fh_fno + cdLast + 1) { /* the info.disc file */
      fa->blocks = 1;
      fa->size = strlen(infoFileString);
    }
    else {
      int trackNum = fh->fh_fno - rootfh.fh_fno;
/*       CDgettrackinfo(cd, fh->fh_fno - ROOT_DIR(), &info); */
      fa->blocks = (cdInfo[trackNum].total_frame
		    + (75 * cdInfo[trackNum].total_sec)
		    + (75 * 60 * cdInfo[trackNum].total_min));
      /* total size in bytes */
      fa->size = (fa->blocks * CDDA_DATASIZE) + AIFF_HEADER_SIZE;
    }
  }
  memcpy(cachedFattr, fa, sizeof(fattr));
  return 0;
}

/*
 *  int
 *  cdda_readlink(fhandle_t *fh, char **link)
 *
 *  Description:
 *      Get the value of a symbolic link from the Rock Ridge
 *      extensions.
 *
 *  Parameters:
 *      fh    File handle to get link for
 *      link  Gets link value
 *
 *  Returns:
 *      0 if successful, errno otherwise.
 */

int
cdda_readlink(fhandle_t *fh, char **link)
{
  printf("shouldn't call cdda_readlink...\n");
  return ENXIO;
}

	
/*
 *  static int
 *  get_parent_fd(int dir, int *fdp)
 *
 *  Description:
 *      Get the file descriptor of this directory's parent
 *
 *  Parameters:
 *      dir     File descriptor of directory we want parent of
 *      fdp     receives file descriptor of parent
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */
static int get_parent_fd(int dir, int *fdp)
{
  *fdp = rootfh.fh_fno;
  return 0;
}

