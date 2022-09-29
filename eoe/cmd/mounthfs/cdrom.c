/*
 *  cdrom.c
 *
 *  Description:
 *      Low-level cdrom support for mountiso9660
 *
 *  History:
 *      rogerc      12/18/90    Created
 *      rogerc      02/12/92    Added support for 512 byte CDs
 *      rogerc      04/01/92    Added support for mode 2 CDs
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <unistd.h>
#include <errno.h>
#include <dslib.h>
#include <fcntl.h>
#include "cdrom.h"

#define G6_DISCINFO 0xc7

#define CD_SENSELEN 18
#define STA_RETRY   (-1)

#define BLOCK_LIMIT     0x1000  /* max blocks transferable in 1 read op */

#ifdef DEBUG
#define CDDBG(x)    {if (cddebug) {x;}}
#define CACHEDBG(x) {if (cachedebug) {x;}}
#else
#define CDDBG(x)
#define CACHEDBG(x)
#endif

/*
 * If we are to support discs with block sizes larger than 2048, a LOT
 * of stuff is going to get more complicated.  Beware that some automatic
 * buffers are of size 2048 and would be overflowed, and since as of
 * this writing the CD-ROM drives don't support block sizes other than
 * these three, reading a block will get more complicated.
 *
 * If block sizes smaller than 512 are ever supported, the caching
 * scheme will have to be reworked as well.
 */
#define ISVALIDBLKSIZE(sz) ((sz) == 2048 || (sz) == 1024 || (sz) == 512)

#define MSELPARMLEN 12

#define EFS_BLKSIZE	512

#define min(a,b) ((a) < (b) ? (a) : (b))

enum drivetype { UNKNOWN, TOSHIBA, SONY, TOSHIBA_SCSI2, TOSHIBA_PHOTO };

struct cdrom {
	struct dsreq        *dsp;
	char                *cache; /* Store for the cache */
	int                 *block_num;   /* Array of block nums for cache */
	int                 cache_blocks;
	enum drivetype      type;
	int                 blksize;
	char                density;
	unsigned long       voldesc; /* location of volume descriptor */
};

#ifdef DEBUG
static int  cddebug = 0;
static int  cachedebug = 0;
#endif

/*
 *  Caching statistics
 */
int cache_hits;
int cache_misses;

static int
read_block( CDROM *cd, long block, char *buf, int num_blocks,
 int use_cache );

static int
set_blksize( CDROM *cd, int blksize );

static int
read_from_cache( CDROM *cd, int start_block, int num_blocks,
 char *buf );

static char *
find_block( CDROM *cd, int block );

static void
write_to_cache( CDROM *cd, int start_block, int num_blocks, char *buf );

static void
insert_block( CDROM *cd, int block, char *buf );

static void
act_like_a_cdrom( struct dsreq *dsp );

static void
toshiba_modesense( struct dsreq *dsp, char *params, int size );

static void
scsi2_modesense( struct dsreq *dsp, char *params, int size );

static void
prevent_removal( struct dsreq *dsp );

static void
allow_removal( struct dsreq *dsp );

/*
 * modesense is a little different between Sony and Toshiba
 */
static void (*modesense[])( struct dsreq *, char *, int ) =
	{ NULL, toshiba_modesense, scsi2_modesense, scsi2_modesense,
	      scsi2_modesense  };

/*
 *  void
 *  cd_init( )
 *
 *  Description:
 *
 *  Parameters:
 */

void
cd_init( )
{
	return;
}

/*
 *  Convert to and from Binary Coded Decimal
 */
#define  TWODIGITBCDTOINT( bcd )    (((bcd) >> 4) * 10 + ((bcd) & 0xf))
#define  INTTOTWODIGITBCD( i )      ((((i) / 10) << 4) | ((i) % 10))

typedef struct tag_discinfo {
    u_char discid;
    u_char minute;
    u_char sec;
    u_char frame;
} DISCINFO;

static void
find_voldesc(CDROM *cd)
{
    dsreq_t *dsp = cd->dsp;
    int minute, sec, frame;
    DISCINFO info;

    cd->voldesc = 16 * CDROM_BLKSIZE;

    if (cd->type == TOSHIBA_PHOTO) {
	fillg1cmd(dsp, CMDBUF(dsp), G6_DISCINFO, 0x3, 0, 0, 0, 0, 0, 0, 0, 0);
	filldsreq(dsp, (unsigned char *)&info, sizeof(info),
		  DSRQ_READ|DSRQ_SENSE );
	if (doscsireq(getfd(dsp), dsp) < 0 || STATUS(dsp) != STA_GOOD) {
	    return;
	}
	
	minute = TWODIGITBCDTOINT(info.minute);
	sec = TWODIGITBCDTOINT(info.sec);
	frame = TWODIGITBCDTOINT(info.frame);
	if (minute || sec || frame) {
	    cd->voldesc = (((minute * 60) + sec) * 75 + frame - 150 + 16)
		* CDROM_BLKSIZE;
	}
    }
}

unsigned long
cd_voldesc(CDROM *cd)
{
    return cd->voldesc;
}
    
/*
 *  int
 *  cd_open( char *dev, int num_blocks, CDROM **cdp )
 *
 *  Description:
 *      Open dev with dsopen, to use devscsi interface.  Pointer to devscsi
 *      dsreq struct is stored in static global dsp
 *
 *  Parameters:
 *      dev         device to open
 *      num_blocks  number of blocks to cache for this device
 *      cdp         Gets pointer to CDROM structure if successful
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
cd_open( char *dev, int num_blocks, CDROM **cdp )
{
    unsigned char   inquiry[98];
    static char     *toshiba = "TOSHIBA CD";
    static char     *sony = "SONY    CD";
    struct dsreq    *dsp;
    CDROM           *cd;
    enum drivetype      type = UNKNOWN;
    int             model;
    
    dsp = dsopen( dev, O_RDONLY );
    if (dsp) {
	if (inquiry12( dsp, inquiry, sizeof (inquiry), 0 ) == 0) {
	    if (strncmp(inquiry + 8, toshiba, strlen(toshiba)) == 0) {
		/*
		 * This is really ugly.  The purpose of doing this
		 * model number hack as opposed to simply doing a
		 * strcmp with "TOSHIBA CD-ROM XM-3401TA" is to
		 * support multi-session CDs on future versions of the
		 * drive.
		 */
		inquiry[30] = 0; /* Null terminate model for atoi. */
				 /* This blows away full model info */
		model = atoi(inquiry + 26);
		if (model >= 3401) {
		    type = TOSHIBA_PHOTO;
		} else {
		    type = ((inquiry[2] & 0x7) == 2) ?
			TOSHIBA_SCSI2 : TOSHIBA;
		}
	    } else if (strncmp(inquiry + 8,
			       sony, strlen(sony)) == 0) {
		type = SONY;
	    }
	}
    }
    if (dsp && type == UNKNOWN) {
	type = SONY;			/* Close enough. */
    }
    if (!dsp)
	return (errno);
    fcntl( getfd( dsp ), F_SETFD, FD_CLOEXEC );
    cd = (CDROM*)safe_malloc( sizeof (*cd) );
    bzero(cd, sizeof *cd);
    cd->cache_blocks = num_blocks;
    act_like_a_cdrom( dsp );
    cd->dsp = dsp;
    cd->type = type;
    /*
     * This might get reset later.  It needs to start as CDROM_BLKSIZE,
     * so we can read the volume descriptor from the disc.  After
     * reading the volume descriptor, iso.c will figure out the block
     * size to use from then on out and set it using cd_set_blksize.
     */
    cd->blksize = CDROM_BLKSIZE;
    if (num_blocks) {
	cd->cache = (char*)safe_malloc(CDROM_BLKSIZE * num_blocks);
	/*
	 * Allocate 4 times as much in case we have 512 byte
	 * blocks; then we'll need 4 times as many block numbers
	 * if we don't want to waste 75% of the cache.
	 */
	cd->block_num = (int*)safe_malloc(sizeof(int) * num_blocks * 4);
	bzero(cd->block_num, sizeof(int) * num_blocks * 4);
    }
    prevent_removal( dsp );
    /*
     * The Toshiba SCSI2 drives support CD-ROM XA discs, which can
     * contain ISO 9660 file systems.  These discs use different
     * ECCs and headers, and we may need to use modeselect appropriately.
     */
    cd->density = 0;
    if (cd->type == TOSHIBA_SCSI2 || cd->type == TOSHIBA_PHOTO) {
	char header[8];
	char params[12];
	
	set_blksize(cd, cd->blksize);
	fillg1cmd( dsp, CMDBUF(dsp), 0x44, 2, 0, 0,
		  0, 16, 0, 0, 8, 0 );
	filldsreq( dsp, header, sizeof (header), DSRQ_READ|DSRQ_SENSE );
	doscsireq( getfd( dsp ), dsp );
	
	if (header[0] == 2) {
	    cd->density = 0x81;
	    memset( params, '\0', sizeof (params) );
	    params[3] = 0x08;
	    *(int *)&params[8] = cd->blksize;
	    params[4] = cd->density;
	    
	    /*  If modeselect fails, try SCSI-2 mode select command. */
	    modeselect( cd->dsp, params, sizeof (params), 0, 0 );
	    if (STATUS(dsp) == STA_CHECK)
		modeselect(cd->dsp, params, sizeof params, 0x10, 0);
	}
    }
    /*
     * Put drive in high speed mode
     */
    if (cd->type == TOSHIBA_PHOTO) {
	struct {
	    unsigned char reserved1;
	    unsigned char mediumType;
	    unsigned char deviceParam;
	    unsigned char blockDescLen;

	    unsigned char pageCode;
	    unsigned char pageLength;
	    unsigned char speed;
	} params;
	
	bzero(&params, sizeof params);
	
	params.pageCode = 0x20;
	params.pageLength = 1;
	params.speed = 1;
	fillg0cmd(cd->dsp, CMDBUF(cd->dsp), G0_MSEL, 0x10, 0, 0,
		  sizeof params, 0);
	filldsreq(cd->dsp, (void *)&params, sizeof params,
		  DSRQ_WRITE | DSRQ_SENSE);
	doscsireq(getfd(cd->dsp), cd->dsp);
    }

    find_voldesc(cd);

    *cdp = cd;
    return (0);
}

int
cd_close( CDROM *cd )
{
	errno = 0;
	allow_removal( cd->dsp );
	/*
         * This is a hack; it makes set_blksize call modeselect with the
	 * right parameters to read mode 1 discs instead of mode 2 discs
	 * in case we had previously set it for mode 1 discs.
	 */
	cd->density = 0;
	set_blksize( cd, EFS_BLKSIZE );
	dsclose( cd->dsp );
	return (errno);
}

/*
 *  int
 *  cd_stat( CDROM *cd, struct stat *sb )
 *
 *  Description:
 *      Do a stat() on the file descriptor for cd->dsp
 *
 *  Parameters:
 *      cd      CDROM device
 *      sb      buffer for stat info
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
cd_stat( CDROM *cd, struct stat *sb )
{
	if (fstat( getfd( cd->dsp ), sb ) < 0)
		return (errno);
	return (0);
}

/*
 *  int
 *  cd_is_dsp_fd( CDROM *cd, int fd )
 *
 *  Description:
 *      Find out if we care about fd
 *
 *  Parameters:
 *      cd
 *      fd
 *
 *  Returns:
 *      1 if we care, 0 if we don't
 */

int
cd_is_dsp_fd( CDROM *cd, int fd )
{
	return (cd->dsp && getfd( cd->dsp ) == fd);
}

/*
 *  int
 *  cd_read( CDROM *cd, long offset, void *vdata, long count )
 *
 *  Description:
 *      Read count bytes starting at offset into vdata from CD-ROM
 *
 *  Parameters:
 *      cd      CD to read from
 *      offset  offset on CD from which to read
 *      vdata   buffer to receive bytes
 *      count   Number of bytes to read
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
cd_read( CDROM *cd, unsigned long offset, void *vdata,
 unsigned long count, int use_cache )
{
	unsigned int    slop;
	int             error, num_read;
	char            buf[CDROM_BLKSIZE], *data = vdata;

	/*
	 *  Read that portion of the beginning that is not aligned on a
	 *  block boundary
	 */
	slop = offset % cd->blksize;
	if (slop) {
		error = read_block( cd, offset / cd->blksize, buf, 1,
		 use_cache );
		if (error)
			return (error);
		num_read = min( cd->blksize - slop, count );
		count -= num_read;
		memcpy( data, buf + slop, num_read );
		data += num_read;
		offset += cd->blksize - 1;
		offset -= offset % cd->blksize;
	}

	/*
	 *  Read the block aligned middle portion
	 */
	if (count / cd->blksize) {
		error = read_block( cd, offset / cd->blksize, data,
		 count / cd->blksize, use_cache );
		if (error)
			return (error);
		offset += count - count % cd->blksize;
	}

	/*
	 *  Read that portion of the end that is not aligned on a block
	 *  boundary.  This avoids writing past the buffer
	 */
	slop = count % cd->blksize;

	if (slop) {
		data += count - slop;
		offset += count / cd->blksize;
		error = read_block( cd, offset / cd->blksize, buf,
		 1, use_cache );
		if (error)
			return (error);
		memcpy( data, buf, slop );
	}
	return (0);
}

/*
 *  int
 *  cd_read_file( CDROM *cd, CD_FILE *fp, unsigned int offset,
 *   unsigned int count, void *vbuf )
 *
 *  Description:
 *      Read from a file, using a CD_FILE structure.  This function knows
 *      how to read in interleaved mode, and takes extended attributes in
 *      to consideration
 *
 *  Parameters:
 *      cd          Device to read  from
 *      fp          File to read from
 *      offset      into file
 *      count       bytes to read
 *      vbuf        buffer to read into
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
cd_read_file( CDROM *cd, CD_FILE *fp, unsigned int offset,
 unsigned int count, void *vbuf )
{
	int             block, error, num_read, block_offset, num_blocks;
	unsigned int    slop;
	char            slop_buf[CDROM_BLKSIZE], *buf;

	buf = vbuf;

	slop = offset % cd->blksize;
	if (slop) {
		block_offset = offset / cd->blksize;
		block = fp->block + fp->xattr_len +
		 (fp->xattr_len ? fp->int_gap : 0) + block_offset +
		 (fp->fu_size ? ((block_offset / fp->fu_size) * fp->int_gap)
		 : 0);

		error = read_block( cd, block, slop_buf, 1, 0 );
		if (error)
			return (error);
		num_read = min( cd->blksize - slop, count );
		count -= num_read;
		memcpy( buf, slop_buf + slop, num_read );
		buf += num_read;
		offset += cd->blksize - 1;
		offset -= offset % cd->blksize;
	}

	while (count >= cd->blksize) {
		block_offset = offset / cd->blksize;
		block = fp->block + fp->xattr_len +
		 (fp->xattr_len ? fp->int_gap : 0) + block_offset +
		 (fp->fu_size ?
		 ((block_offset / fp->fu_size) * fp->int_gap) : 0);
		slop = fp->fu_size ? (offset %
		 (cd->blksize * fp->fu_size)) : 0;
		num_blocks = fp->fu_size ?
		 fp->fu_size - slop : count / cd->blksize;
		if (num_blocks > count / cd->blksize)
			num_blocks = count / cd->blksize;
		error = read_block( cd, block, buf, num_blocks, 0 );
		if (error)
			return (error);
		num_read = num_blocks * cd->blksize;
		buf += num_read;
		offset += num_read;
		count -= num_read;
	}

	/*
	 *  Read that portion of the end that is not aligned on a block
	 *  boundary.  This avoids writing past the buffer
	 */

	if (count) {
		block_offset = offset / cd->blksize;
		block = fp->block + fp->xattr_len +
		 (fp->xattr_len ? fp->int_gap : 0) + block_offset +
		 (fp->fu_size ? ((block_offset / fp->fu_size) *
		 fp->int_gap) : 0);
		error = read_block( cd, block, slop_buf, 1, 0 );
		if (error)
			return (error);
		memcpy( buf, slop_buf, count );
	}
	return (0);
}

/*
 *  int
 *  cd_media_changed( CDROM *cd, int *changed )
 *
 *  Description:
 *      Find out if the media could have changed recently
 *      Also makes sure there's a disc in the drive
 *
 *  Parameters:
 *      cd
 *      changed     receives 0 if not changed, 1 if changed
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

int
cd_media_changed( CDROM *cd, int *changed )
{
	char    params[12];
	int                     retries;
	register struct dsreq   *dsp = cd->dsp;

	DATABUF(dsp) = params;
	DATALEN(dsp) = sizeof (params);

	*changed = 0;
	retries = 2;
	while (retries--) {
		testunitready00( dsp );
		if (STATUS( dsp ) == STA_CHECK
		 && SENSEBUF(dsp)[2] == 6) {
			*changed = 1;
			if (SENSEBUF(dsp)[12] == 0x29)
				act_like_a_cdrom( dsp );
		}
		else if (STATUS( dsp ) == STA_GOOD)
			break;
		else {
			cd_flush_cache( cd );
			return (EIO);
		}
	}

	(*modesense[cd->type])( dsp, params, sizeof (params) );
	
	CDDBG(
		fprintf( stderr, "status = %d\n", STATUS(dsp) );
		if (STATUS(dsp) == STA_CHECK) {
			fprintf( stderr, "sense[2] = %x\n", SENSEBUF(dsp)[2] );
		}
	);

	if (STATUS(dsp) == STA_GOOD) {
		if (*(int *)&params[8] != cd->blksize) {
			*changed = 1;
			if (!set_blksize( cd, cd->blksize ))
				return (EIO);
		}
		return (0);
	}

	cd_flush_cache( cd );
	return (EIO);
}

/*
 *  int
 *  cd_set_blksize(CDROM *cd, int blksize)
 *
 *  Description:
 *	Set the block size to be used with this particular
 *	CD-ROM drive
 *
 *  Parameters:
 *      cd       CD-ROM drive of which to set block size
 *      blksize  The block size
 *
 *  Returns:
 *	0 if successful, errno otherwise
 */

int
cd_set_blksize(CDROM *cd, int blksize)
{
	if (!ISVALIDBLKSIZE(blksize))
		return EIO;
	/*
	 * Scale the number of blocks to be stored in the cache
	 * according to the size of the blocks.  This will not work
	 * as currently coded if you try to support a block size
	 * that's smaller than 512 bytes.
	 */
	cd->cache_blocks = cd->cache_blocks * cd->blksize / blksize;
	cd->blksize = blksize;
	set_blksize(cd, blksize);
	return 0;
}

/*
 *  int
 *  cd_get_blksize(CDROM *cd)
 *
 *  Description:
 *	Return cd->blksize; keep CDROM struct opaque
 *
 *  Parameters:
 *      cd
 *
 *  Returns:
 *	block size we're using for this CD-ROM drive
 */

int
cd_get_blksize(CDROM *cd)
{
	return cd->blksize;
}

/*
 *  void
 *  flush_cache(CDROM *cd)
 *
 *  Description:
 *      Empty out the cache - next read guaranteed to miss the cache
 *
 *  Parameters:
 *      cd
 */

void
cd_flush_cache(CDROM *cd)
{
    bzero(cd->block_num, sizeof(int) * cd->cache_blocks);	
}

/*
 *  static int
 *  read_block( CDROM *cd, long block, char *buf, int num_blocks,
 *   int use_cache )
 *
 *  Description:
 *      Read num_block blocks from CD-ROM into buf, starting at block
 *
 *  Parameters:
 *      cd
 *      block       starting block from which to read
 *      buf         buffer to receive blocks
 *      num_blocks  number of blocks to read
 *      use_cache   Whether or not to write blocks to cache
 *
 *  Returns:
 *      0 if successful, error code otherwise
 */

static int
read_block( CDROM *cd, long block, char *buf, int num_blocks,
 int use_cache )
{
	char    *orgbuf = buf;
	int     blocks, orgblock = block, orgnum_blocks = num_blocks;

	/*
	 *  We'll look in the cache even if use_cache == 0; there's no point
	 *  in not checking.  We will not write to cache in this case.
	 */
	if (read_from_cache( cd, block, num_blocks, buf )) {
		cache_hits += num_blocks;
		return (0);
	}

	cache_misses += num_blocks;

	/*
	 *  Do the read, in increments of BLOCK_LIMIT, which is the largest
	 *  number of blocks we can read with one call.  Check each time for
	 *  success
	 */
	for ( ; num_blocks > 0;
	 num_blocks -= BLOCK_LIMIT, buf += BLOCK_LIMIT, block += BLOCK_LIMIT) {
		blocks = min( num_blocks, BLOCK_LIMIT );
		filldsreq( cd->dsp, buf, blocks * cd->blksize,
		 DSRQ_READ | DSRQ_SENSE );
		cd->dsp->ds_time = 60 * 1000; /* bug 313075: 60 seconds */
		fillg1cmd( cd->dsp, CMDBUF(cd->dsp), G1_READ, 0, block >> 24,
		 (block & 0x00ff0000) >> 16, (block & 0x0000ff00) >> 8,
		 block & 0xff, 0, (blocks & 0xff00) >> 8, blocks & 0xff, 0 );
		doscsireq( getfd( cd->dsp ), cd->dsp );
		if (STATUS(cd->dsp) != STA_GOOD)
			return (EIO);
	}

	if (use_cache)
		write_to_cache( cd, orgblock, orgnum_blocks, orgbuf );
		
	return (0);
}

/*
 *  static int
 *  set_blksize( CDROM *cd, blksize )
 *
 *  Description:
 *      Set the block size of dsp to blksize
 *
 *  Parameters:
 *      cd      devscsi struct of device to set block size of
 *      blksize the new size
 *
 *  Returns:
 *      1 if successful, 0 otherwise
 */

static int
set_blksize( CDROM *cd, int blksize )
{
	char                    params[12];
	int                     retries;
	register struct dsreq   *dsp = cd->dsp;

	retries = 10;
	while (retries--) {
		(*modesense[cd->type])( dsp, params, sizeof (params) );
		CDDBG(
			fprintf( stderr, "status = %d\n", STATUS(dsp) );
			if (STATUS(dsp) == STA_CHECK) {
				fprintf( stderr, "sense[2] = %x\n",
				 SENSEBUF(dsp)[2] );
			}
		);

		if (STATUS(dsp) == STA_GOOD) {
			if (*(int *)&params[8] == blksize)
				return (1);
			else
				break;
		}
		else if (STATUS(dsp) == STA_CHECK || STATUS(dsp) == STA_RETRY)
			continue;
		else
			return (0);
	}

	retries = 10;
	while (retries--) {
		memset( params, '\0', sizeof (params) );
		params[3] = 0x08;
		params[4] = cd->density;
		*(int *)&params[8] = blksize;

		/*  If modeselect fails, try SCSI-2 mode select command. */
		modeselect( dsp, params, sizeof (params), 0, 0 );
		if (STATUS(dsp) == STA_CHECK)
		    modeselect(cd->dsp, params, sizeof params, 0x10, 0);

		if (STATUS(dsp) == STA_GOOD)
			return (1);
		else if (STATUS(dsp) == STA_CHECK || STATUS(dsp) == STA_RETRY)
			continue;
		else
			return (0);
	}

	return (0);
}

/*
 *  The cache:
 *
 *  We cache num_blocks blocks.  This number is set by the user.
 *  For every block we read, we store its contents in
 *  cache[block % num_blocks].block.
 */

/*
 *  static int
 *  read_from_cache( CDROM *cd, int start_block, int num_blocks, char *buf )
 *
 *  Description:
 *      Read num_blocks blocks from the cache into buf, starting with
 *      start_block.  This function only returns success if all blocks
 *      were in the cache; if any block from the range is not in the cache,
 *      all blocks in the range will be read.
 *
 *  Parameters:
 *      cd
 *      start_block block to start reading from
 *      num_blocks  number of blocks to read
 *      buf         buffer to receive blocks
 *
 *  Returns:
 *      1 if cache hit, 0 if cache miss.
 */

static int
read_from_cache( CDROM *cd, int start_block, int num_blocks, char *buf )
{
	int     i;
	char    *block;

	if (!cd->cache_blocks)
		return (0);

	for (i = start_block; i - start_block < num_blocks; i++) {
		block = find_block( cd, i );
		if (!block) {
			CDDBG(fprintf( stderr, "Cache miss( %d, %d )\n",
			 start_block, num_blocks ));
			return (0);
		}
		bcopy( block, buf, cd->blksize );
		buf += cd->blksize;
	}

	CDDBG(fprintf( stderr, "Cache hit( %d, %d )\n", start_block,
	 num_blocks ));
	return (1);
}

/*
 *  static char *
 *  find_block( CDROM *cd, int block )
 *
 *  Description:
 *      Look for block in the cache.  If it's there, return a pointer to
 *      the data.  Otherwise, return 0
 *
 *  Parameters:
 *      cd
 *      block   block number to look for
 *
 *  Returns:
 *      pointer to block's data if it's in the cache, 0 otherwise
 */

static char *
find_block( CDROM *cd, int block )
{
	if (cd->block_num[block % cd->cache_blocks] == block)
		return (cd->cache + (block % cd->cache_blocks)
		 * cd->blksize);

	return (0);
}

/*
 *  static void
 *  write_to_cache( CDROM *cd, int start_block, int num_blocks, char *buf )
 *
 *  Description:
 *      Write num_blocks from buf into the cache
 *
 *  Parameters:
 *      cd
 *      start_block first block to write
 *      num_blocks  number of blocks to write
 *      buf         buffer containing data to write to the cache
 */

static void
write_to_cache( CDROM *cd, int start_block, int num_blocks, char *buf )
{
	int i;

	if (!cd->cache_blocks)
		return;

	for (i = start_block; i - start_block < num_blocks; i++) {
		insert_block( cd, i, buf );
		buf += cd->blksize;
	}
}

/*
 *  static void
 *  insert_block( CDROM *cd, int block, char *buf )
 *
 *  Description:
 *      Insert a single block into the cache
 *
 *  Parameters:
 *      cd
 *      block   block to insert
 *      buf     buffer containing data for the cache
 */

static void
insert_block( CDROM *cd, int block, char *buf )
{
	if (!cd->cache_blocks)
		return;

	CACHEDBG(
		int slot = block % cd->cache_blocks;
		if (cd->block_num[slot]) {
			fprintf(stderr, "Replacing block %d with block %d\n",
			 cd->block_num[slot], block);
		}
	)
	cd->block_num[block % cd->cache_blocks] = block;
	bcopy(buf, cd->cache + (block % cd->cache_blocks) * cd->blksize,
	 cd->blksize);
}

/*
 *  static int act_like_a_cdrom( struct dsreq *dsp )
 *
 *  Description:
 *      This command, which must be called after a SCSI reset occurs,
 *      instructs the CD-ROM drive to quit acting like a disk drive and
 *      start acting like a CD-ROM drive.
 *
 *  Parameters:
 *      dsp     devscsi
 */

static void
act_like_a_cdrom( struct dsreq *dsp )
{
	fillg1cmd( dsp, CMDBUF(dsp), 0xc9, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	filldsreq( dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE );
	doscsireq( getfd( dsp ), dsp );
}

static void
toshiba_modesense( struct dsreq *dsp, char *params, int size )
{
	modesense1a( dsp, params, size, 0, 0, 0 );
}

/*
 *  void
 *  scsi2_modesense( struct dsreq *dsp, char *params, int size )
 *
 *  Description:
 *      Get the basic mode sense data from a scsi2 CDROM drive.  It
 *      turns out that you must specify a mode sense page code, or
 *      this will fail, in contrast to the Toshiba.
 *
 *  Parameters:
 *      dsp     devscsi
 *      params  will receive the mode sense data
 *      size    size of params buffer
 */

static void
scsi2_modesense( struct dsreq *dsp, char *params, int size )
{
	char    sony_params[MSELPARMLEN + 8], *databuf;
	int     datalen;

	databuf = DATABUF( dsp );
	datalen = DATALEN( dsp );
	/*
	 * Amazingly enough, modesense fails if you don't request any
	 * page code information
	 */
	fillg0cmd( dsp, CMDBUF( dsp ), G0_MSEN, 0, 1, 0,
	 sizeof (sony_params) , 0 );
	filldsreq( dsp, sony_params, sizeof (sony_params),
	 DSRQ_READ | DSRQ_SENSE );
	doscsireq( getfd( dsp ), dsp );

	if (STATUS( dsp ) == STA_GOOD)
		bcopy( sony_params, params, size );
	
	DATABUF( dsp ) = databuf;
	DATALEN( dsp ) = datalen;
}

/*
 *  int
 *  prevent_removal( struct dsreq *dsp )
 *
 *  Description:
 *      Disable the eject button on the CD ROM drive
 *
 *  Parameters:
 *      dsp     devscsi
 *
 *  Returns:
 *      Status information is in STATUS(dsp)
 */

static void
prevent_removal( struct dsreq *dsp )
{
	fillg0cmd( dsp, CMDBUF( dsp ), 0x1e, 0, 0, 0, 1, 0 );
	filldsreq( dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE );
	doscsireq( getfd( dsp ), dsp );
}

static void
allow_removal( struct dsreq *dsp )
{
	fillg0cmd( dsp, CMDBUF( dsp ), 0x1e, 0, 0, 0, 0, 0 );
	filldsreq( dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE );
	doscsireq( getfd( dsp ), dsp );
}

int
modeselect(struct dsreq *dsp, caddr_t data, long datalen, int save, int vu)
{
    fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), G0_MSEL,
	      save, 0, 0, B1(datalen), B1(vu<<6));
    filldsreq(dsp, (uchar_t *) data, datalen, DSRQ_WRITE|DSRQ_SENSE);
    return(doscsireq(getfd(dsp), dsp));
}

