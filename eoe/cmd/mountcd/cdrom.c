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

#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/scsi.h>
#include <sys/sema.h>
#include <sys/dvh.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/dksc.h>
#include <sys/dkio.h>
#include <sys/iograph.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/conf.h>
#include <sys/time.h>
#include <errno.h>
#include <dslib.h>
#include <fcntl.h>
#include "cdrom.h"
#include "util.h"

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

enum Brand { GENERIC, TOSHIBA, SONY, TOSHIBA_SCSI2, TOSHIBA_PHOTO };
char *BrandName[] =
	{ "GENERIC", "TOSHIBA", "SONY", "TOSHIBA_SCSI2", "TOSHIBA_PHOTO" };


struct cdrom {
	int		dkfd;
	char		scsiname[PATH_MAX];
	char		*cache;			/* Store for the cache */
	int		*block_num;		/* Array of cache block #'s */
	int		cache_blocks;
	enum Brand	type;
	int		blksize;
	char		density;
	unsigned long	voldesc;		/* loc. of volume descriptor */
	u_char		pglengths[ALL+1];	/* lengths of each page for  */
						/* mode select, sense.	     */
						/* len of 0 means page not   */
						/* supported		     */
};

typedef struct tagMselParams {
	/*
	 * four bytes of header info...
	 */
	unsigned char res1;
	unsigned char mediumType;
	unsigned char devSpecific;
	unsigned char blockDescLen;

	/*
	 * Block descriptor
	 */
	unsigned int densityCode : 8;
	unsigned int numBlocks : 24;
	unsigned int res2 : 8;
	unsigned int blockSize : 24;
} MSELPARAM;


typedef struct mode_sense_data sndata_t;

/*
 * Determines whether or not the device really is a CD-ROM device
 * vs. some other device which contains an ISO9660 filesystem, like
 * a disk for example.
*/
int really_is_CDROM = 1;

/*
 * This goes in the blockDescLen field above
 */
#define BLOCKDESCLEN 8

int  cddebug	= 0;
int  cachedebug	= 0;

/*
 *  Caching statistics
 */
int cache_hits;
int cache_misses;

static int
read_block(CDROM *cd, long block, char *buf, int num_blocks, int use_cache,
char *reason, int lineno);

static int
set_blksize( CDROM *cd, int blksize );

static int
set_up_volume_header(int dkfd);

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
act_like_a_cdrom( CDROM *cd, struct dsreq *dsp );

static
scsi_modesense(CDROM *cd, sndata_t *addr, u_char pgcode);

static void
scsi_getsupported(CDROM *cd);

static void
prevent_removal( struct dsreq *dsp );

static void
allow_removal( struct dsreq *dsp );

extern int fd_to_scsi_name(int, char*);


/*
 * used to convert all the 3 byte sequences from scsi
 * modesense, etc. to uints for printing, etc.
 */
static uint
sc_3bytes_uint(u_char *a)
{
	return (((uint)a[0]) << 16) + (((uint)a[1]) << 8) +  (uint)a[2];
}


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

static dsreq_t *
cd_dsopen(CDROM *cd)
{
	int i;

	CDDBG(fprintf(stderr, ">>>>cd: cd_dsopen:\n"));

	/*
	 * retry for ~20 seconds
	 */
	for (i = 0; i < 200; i++) {
		dsreq_t *dsp;


		dsp = dsopen(cd->scsiname, O_RDONLY);

		if (dsp)
			return dsp;

		if (errno != EBUSY) {
			CDDBG(fprintf(stderr,
			 ">>>>cd: cd_dsopen: dsopen(\"%s\") failed, errno/%d\n",
						cd->scsiname, errno));
			break;
		}

		sginap(i);
	}

	return NULL;
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
find_voldesc(CDROM *cd, dsreq_t *dsp)
{
	int		minute, sec, frame;
	int		diskInfoBuf[(sizeof(DISCINFO) + 3) / sizeof(int)];
	DISCINFO	*info = (DISCINFO *)diskInfoBuf;


	cd->voldesc = 16 * CDROM_BLKSIZE;

	CDDBG(fprintf(stderr, ">>>>cd: find_voldesc: default = %d\n",
							cd->voldesc));

	if (cd->type == TOSHIBA_PHOTO) {

		fillg1cmd(dsp, (uchar_t *)CMDBUF(dsp), G6_DISCINFO,
						0x3, 0, 0, 0, 0, 0, 0, 0, 0);

		filldsreq(dsp, (unsigned char *)info, sizeof(DISCINFO),
							DSRQ_READ|DSRQ_SENSE );

		if (   doscsireq(getfd(dsp), dsp) < 0
		    || STATUS(dsp) != STA_GOOD) {

			return;
		}
	
		minute = TWODIGITBCDTOINT(info->minute);
		sec    = TWODIGITBCDTOINT(info->sec);
		frame  = TWODIGITBCDTOINT(info->frame);

		if (minute || sec || frame) {
			cd->voldesc =
				( ((minute * 60) + sec) * 75 + frame - 150 + 16)
						* CDROM_BLKSIZE;
		}
	}

	CDDBG(fprintf(stderr, ">>>>cd: find_voldesc: cd->voldesc = %d\n",
								cd->voldesc));
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
 *      Open dev with dsopen, to use devscsi interface.
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
	static char		*toshiba = "TOSHIBA CD";
	static char		*sony = "SONY    CD";

	int			model;
	int			dkfd;
	char			scsiname[PATH_MAX];
	unsigned char		inquiry[98];
	enum Brand		type = GENERIC;
	CDROM			*cd;
	struct dk_ioctl_data	arg;
	struct dsreq		*dsp;
	int			initially_is_CDROM = 1; /* assume it is initially */
    
	CDDBG(fprintf(stderr, ">>>>cd: cd_open: dsdebug set to 0%o\n",
								dsdebug));
	dkfd = open(dev, O_RDONLY);

	if (dkfd < 0) {
		fprintf(stderr,
    "mount_iso9660: ERROR: Unable to open(\"%s\"), errno = %d\n", dev, errno);

		return errno;
	}

	if (fd_to_scsi_name(dkfd, scsiname) < 0) {
		fprintf(stderr,
"mount_iso9660: ERROR: Unable to translate to scsiname for dev(\"%s\"), errno = %d\n",
						dev, errno);
		return errno ? errno : ENOENT;
	}

	dsp = dsopen(scsiname, O_RDONLY);

	if (dsp) {

		if (inquiry12(dsp, (caddr_t)inquiry,
				   sizeof (inquiry), 0 ) == 0) {

			/* snap initial peripheral device type and removable media bit */
			initially_is_CDROM = (inquiry[0] & 0x1f) == 5/* CD-ROM device */
					|| (inquiry[1] & 8/* removable media */);

			if (strncmp((const char *)inquiry + 8,
					toshiba, strlen(toshiba)) == 0) {
				/*
				 * This is really ugly.  The purpose of doing
				 * this model number hack as opposed to simply
				 * doing a strcmp with "TOSHIBA CD-ROM
				 * XM-3401TA" is to support multi-session CDs
				 * on future versions of the drive.
				 */

				/*
				 * Null terminate model for atoi.
				 */
				inquiry[30] = 0;

				/*
				 * This blows away full model info
				 */
				model = atoi(inquiry + 26);

				if (model >= 3401) {
					type = TOSHIBA_PHOTO;
				} else {
					type = ((inquiry[2] & 0x7) == 2)
							? TOSHIBA_SCSI2
							: TOSHIBA;
				}
			} else if (strncmp((const char *)inquiry + 8,
						sony, strlen(sony)) == 0) {
				type = SONY;
			}
		} else {
			type = GENERIC;
		}
	} else {
		fprintf(stderr,
	"mount_iso9660: ERROR: Unable to dsopen(\"%s\"), errno = %d\n",
							scsiname, errno);
		return errno;
	}

	if (type == GENERIC) {
		really_is_CDROM = initially_is_CDROM;
		/*
		 * too many customers with 3rd party drives took this as
		 * an error message, so make it clearer that it's just a
		 * warning
		 */
		if (really_is_CDROM)
			fprintf(stderr, "mount_iso9660: Note: unknown CD-ROM type may"
				" not function correctly,\n\tInquiry12 type = <%s>\n",
				inquiry + 8);
		else
			fprintf(stderr, "mount_iso9660: Note: ISO9660 filesystem on SCSI"
				" disk may not function correctly,\n\tInquiry12"
				" type = <%s>\n", inquiry + 8);
	}

	if (set_up_volume_header(dkfd) == 0) {
		fprintf(stderr, "mount_iso9660: ERROR: set_up_volume_header failed for"
			" dkfd = %d, errno = %d\n", dkfd, errno);

		dsclose(dsp);

		CDDBG(fprintf(stderr,
		  ">>>>cd: cd_open: dsclose called @ line #%d\n", __LINE__));

		return errno;
	}

	fcntl(getfd(dsp), F_SETFD, FD_CLOEXEC );

	cd = safe_malloc(sizeof (*cd));

	bzero(cd, sizeof *cd);

	cd->cache_blocks = num_blocks;

	cd->dkfd = dkfd;
	cd->type = type;

	strcpy(cd->scsiname, scsiname);

	CDDBG(fprintf(stderr,
		">>>>cd: cd_open: cd->type is %s\n", BrandName[cd->type]));

	act_like_a_cdrom(cd, dsp);

	/*
	 * Do another inquiry to see if we can once and forall determine
	 * if this is really a CD-ROM device.
	*/
	if (!initially_is_CDROM &&
            inquiry12(dsp, (caddr_t)inquiry, sizeof (inquiry), 0 ) == 0) {
          really_is_CDROM = (inquiry[0] & 0x1f) == 5/* CD-ROM device */
			|| (inquiry[1] & 8/* removable media */);
	}

	scsi_getsupported(cd);

	/*
	 * This might get reset later.  It needs to start as CDROM_BLKSIZE,
	 * so we can read the volume descriptor from the disc.  After
	 * reading the volume descriptor, iso.c will figure out the block
	 * size to use from then on out and set it using cd_set_blksize.
	 */
	cd->blksize = CDROM_BLKSIZE;

	if (num_blocks) {
		cd->cache = safe_malloc(CDROM_BLKSIZE * num_blocks);

		/*
		 * Allocate 4 times as much in case we have 512 byte
		 * blocks; then we'll need 4 times as many block numbers
		 * if we don't want to waste 75% of the cache.
		 */
		cd->block_num = safe_malloc(sizeof(int) * num_blocks * 4);

		bzero(cd->block_num, sizeof(int) * num_blocks * 4);
	}

	prevent_removal(dsp);

	/*
	 * XXX The logic from here until the comment "Put drive in high
	 * speed mode" should be cleaned up; The call to set_blksize
	 * should come after the CD-ROM XA specific code, which would
	 * render the modeselect15 unnecessary.
	 * 
	 * I'm not fixing this now because it's very close to the time
	 * we're shipping 5.2; I just pulled this call to set_blksize out
	 * of the if statement to fix a problem with the Sony drive and I
	 * noticed this.  -- rogerc 1/28/94
	 */
	cd->density = 0;

	if ( ! set_blksize(cd, cd->blksize)) {

		fprintf(stderr,
"mount_iso9660: ERROR: set_blksize failed for blksize = %d, errno = %d\n",
						cd->blksize, errno);
		dsclose(dsp);

		CDDBG(fprintf(stderr,
		    ">>>>cd: cd_open: dsclose called @ line #%d\n", __LINE__));

		free(cd);

		return EIO;
	}

	/*
	 * The Toshiba SCSI2 drives support CD-ROM XA discs, which can
	 * contain ISO 9660 file systems.  These discs use different
	 * ECCs and headers, and we may need to use modeselect appropriately.
	 */
	if (   cd->type == TOSHIBA_SCSI2
	    || cd->type == TOSHIBA_PHOTO) {
#define HEADER_SIZE 8
		int		headerBuf[(HEADER_SIZE + 3) / sizeof(int)];
		uchar_t		*header = (uchar_t *)headerBuf;
		MSELPARAM	params;

	
		CDDBG(fprintf(stderr,
			">>>>cd: TOSHIBA_SCSI2/TOSHIBA_PHOTO 0x44 cmd:\n"));

		fillg1cmd( dsp, (uchar_t *)CMDBUF(dsp),
					0x44, 2, 0, 0, 0, 16, 0, 0, 8, 0 );

		filldsreq( dsp, header, HEADER_SIZE, DSRQ_READ|DSRQ_SENSE );

		doscsireq( getfd( dsp ), dsp );
	
		if (header[0] == 2) {
			cd->density = 0x81;

			bzero(&params, sizeof params);

			params.blockDescLen = BLOCKDESCLEN;
			params.blockSize    = cd->blksize;
			params.densityCode  = cd->density;

			arg.i_addr = (caddr_t)&params;
			arg.i_len  = sizeof params;
			arg.i_page = 0;

			if (ioctl(cd->dkfd, DIOCSELFLAGS, 0x10) != 0) {
				fprintf(stderr,
	"mount_iso9660: ERROR: ioctl(DIOCSELFLAGS, 0x10) failed, errno = %d\n",
								errno);
				return EIO;
			}

			if (ioctl(cd->dkfd, DIOCSELECT, &arg)) {
				fprintf(stderr,
	"mount_iso9660: ERROR: ioctl(DIOCSELECT) failed, errno = %d\n", errno);

				return EIO;
			}
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
	
		params.pageCode   = 0x20;
		params.pageLength = 1;
		params.speed      = 1;

		arg.i_addr = (caddr_t)&params;
		arg.i_len  = sizeof params;
		arg.i_page = 0x20;

		if (ioctl(cd->dkfd, DIOCSELFLAGS, 0x10) != 0) {
				fprintf(stderr,
	"mount_iso9660: ERROR: ioctl(DIOCSELFLAGS, 0x10) failed, errno = %d\n",
								errno);
			return EIO;
		}

		if (ioctl(cd->dkfd, DIOCSELECT, &arg) != 0) {
				fprintf(stderr,
	"mount_iso9660: ERROR: ioctl(DIOCSELECT) failed, errno = %d\n", errno);

			perror("cd_open DIOCSELECT");

			(void)close(cd->dkfd);

			return EIO;
		}
	}

	find_voldesc(cd, dsp);

	*cdp = cd;

	dsclose(dsp);

	CDDBG(fprintf(stderr,
		">>>>cd: cd_open: dsclose called @ line #%d\n", __LINE__));

	CDDBG(
	{
		int i;


		fprintf(stderr, ">>>>cd:.. cd->dkfd/%d\n", cd->dkfd);
		fprintf(stderr, ">>>>cd:.. cd->scsiname<%s>\n", cd->scsiname);
		fprintf(stderr, ">>>>cd:.. cd->cache/0x%x\n", cd->cache);
		fprintf(stderr, ">>>>cd:.. cd->block_num/0x%x\n",
								cd->block_num);
		fprintf(stderr, ">>>>cd:.. cd->cache_blocks/%d\n", cd->cache_blocks);
		fprintf(stderr, ">>>>cd:.. cd->type/%d\n", cd->type);
		fprintf(stderr, ">>>>cd:.. cd->blksize/%d\n", cd->blksize);
		fprintf(stderr, ">>>>cd:.. cd->density/%d\n", cd->density);
		fprintf(stderr, ">>>>cd:.. cd->voldesc/%u\n", cd->voldesc);
		for (i = 0; i < (ALL+1); i++)
			fprintf(stderr, ">>>>cd:.. pg[0x%x]: length/%d\n",
							i, cd->pglengths[i]);
	} );

	return 0;
}

int
cd_close( CDROM *cd )
{
	struct dsreq	*dsp;


	CDDBG(fprintf(stderr, ">>>>cd: cd_close:\n"));

	errno = 0;

	CDDBG(fprintf(stderr,
	    ">>>>cd: cd_close: calling cd_dsopen from line #%d\n", __LINE__));

	if ((dsp = cd_dsopen(cd)) == NULL) {
		    fprintf(stderr,
	    "mount_iso9660: Unable to re-dsopen(\"%s\"), errno = %d\n",
							cd->scsiname, errno);
		return errno;
	}

	allow_removal(dsp);

	dsclose(dsp);

	CDDBG(fprintf(stderr,
		">>>>cd: cd_close: dsclose called @ line #%d\n", __LINE__));


	/*
         * This is a hack; it makes set_blksize call modeselect with the
	 * right parameters to read mode 1 discs instead of mode 2 discs
	 * in case we had previously set it for mode 1 discs.
	 */
	cd->density = 0;

	set_blksize(cd, EFS_BLKSIZE );

	return errno;
}


/*
 *  int
 *  cd_stat( CDROM *cd, struct stat *sb )
 *
 *  Description:
 *      Do a stat() on the file descriptor for cd->dkfd
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
	CDDBG(fprintf(stderr, ">>>>cd: cd_stat:\n"));

	if (fstat( cd->dkfd, sb ) < 0)
		return errno;

	return 0;
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
	return fd == cd->dkfd;
}

/*
 *  cd_read
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
	 unsigned long count, int use_cache, char *reason, int lineno)
{
	unsigned int    slop;
	int             error, num_read;
	char            buf[CDROM_BLKSIZE], *data = vdata;

	CDDBG(fprintf(stderr,
">>>>cd: cd_read: offset/%u, vdata/0x%x, count/%u, use_cache/%d line/%d<%s>\n",
					offset, vdata, count, use_cache,
					lineno, reason));
	/*
	 *  Read that portion of the beginning that is not aligned on a
	 *  block boundary
	 */
	slop = offset % cd->blksize;

	if (slop) {
		error = read_block( cd, offset / cd->blksize,
							buf, 1, use_cache,
							"cd_read", __LINE__);
		if (error) {
			CDDBG(fprintf(stderr,
		 ">>>>cd: cd_read: read_block returned error/%d @ line # %d\n",
							error, __LINE__));
			return (error);
		}

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
					count / cd->blksize, use_cache,
					"cd_read", __LINE__);
		if (error) {
			CDDBG(fprintf(stderr,
		  ">>>>cd: cd_read: read_block returned error/%d @ line # %d\n",
							error, __LINE__));
			return (error);
		}

		offset += count - count % cd->blksize;
	}

	/*
	 *  Read that portion of the end that is not aligned on a block
	 *  boundary.  This avoids writing past the buffer
	 */
	slop = count % cd->blksize;

	if (slop) {
		data   += count - slop;
		offset += count / cd->blksize;

		error = read_block( cd, offset / cd->blksize,
							buf, 1, use_cache,
							"cd_read", __LINE__);
		if (error) {
			CDDBG(fprintf(stderr,
		  ">>>>cd: cd_read: read_block returned error/%d @ line # %d\n",
							error, __LINE__));
			return (error);
		}

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

	CDDBG(fprintf(stderr,
">>>>cd: cd_read_file: fpblock/%d, fpxattrlen/%d, fpintgap/%d, fpfusize/%d offset/%u, count/%u, vbuf/0x%x\n",
					fp->block, fp->xattr_len, fp->int_gap,
					fp->fu_size, offset, count, vbuf));
	buf = vbuf;

	slop = offset % cd->blksize;
	if (slop) {
		block_offset = offset / cd->blksize;
		block = fp->block + fp->xattr_len +
		 (fp->xattr_len ? fp->int_gap : 0) + block_offset +
		 (fp->fu_size ? ((block_offset / fp->fu_size) * fp->int_gap)
		 : 0);

		error = read_block( cd, block, slop_buf, 1, 0,
						"cd_read_file", __LINE__);
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
		error = read_block( cd, block, buf, num_blocks, 0,
						"cd_read_file", __LINE__);
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
		error = read_block( cd, block, slop_buf, 1, 0,
						"cd_read_file", __LINE__);
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
 *
 *  Because the open is moderately expensive on most systems
 *  and even testunitready costs 2-4 msecs, we only do this every
 *  few seconds.  We can't just keep the devscsi device open,
 *	because that can cause problems for other programs, like SoftWindows.
 *  Should probably be removed altogether, but it's a bit late in the
 *  release cycle for that.
 *  See bug #544222
 *	Olson, 11/97
 */

int
cd_media_changed( CDROM *cd, int *changed )
{
	int		retries;
	static time_t	lasttime;
	time_t		thistime;
	struct dsreq	*dsp;

	CDDBG(fprintf(stderr, ">>>>cd: cd_media_changed:\n"));

	*changed = 0;

	thistime = time(NULL);

	if (thistime < (lasttime+3))
		return 0;

	CDDBG(fprintf(stderr,
		">>>>cd: cd_media_changed: calling cd_dsopen from line #%d\n",
								__LINE__));
	if ((dsp = cd_dsopen(cd)) == NULL) {
		    fprintf(stderr,
	    "mount_iso9660: Unable to re-dsopen(\"%s\"), errno = %d\n",
							cd->scsiname, errno);
		return errno;
	}

	retries = 2;

	while (retries--) {
		testunitready00( dsp );

		if (   STATUS(dsp) == STA_CHECK
		    && SENSEBUF(dsp)[2] == 6) {

			*changed = 1;

			if (SENSEBUF(dsp)[12] == 0x29)
				act_like_a_cdrom(cd, dsp);

		} else if (STATUS(dsp) == STA_GOOD) {
			break;
		} else {
			cd_flush_cache(cd);

			dsclose(dsp);

			CDDBG(fprintf(stderr,
			">>>>cd: cd_media_changed: dsclose called @ line #%d\n",
								__LINE__));
			return EIO;
		}
	}

	lasttime = thistime;	/* only reset on good status returns */

	dsclose(dsp);

	CDDBG(fprintf(stderr,
		">>>>cd: cd_media_changed: dsclose called @ line #%d\n",
								__LINE__));
	return 0;
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
	CDDBG(fprintf(stderr, ">>>>cd: cd_set_blksize: blksize/%d\n", blksize));

	if (!ISVALIDBLKSIZE(blksize)) {
		CDDBG(fprintf(stderr,
			  ">>>>cd: cd_set_blksize: ! valid blksize %d\n",
								blksize));
		return EIO;
	}

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
	CDDBG(fprintf(stderr, ">>>>cd: cd_flush_cache:\n"));

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
/* ARGSUSED */
static int
read_block( CDROM *cd, long block, char *buf, int num_blocks, int use_cache,
	    char *reason, int lineno)
{
	int     blocks, orgblock, orgnum_blocks;
	int	rc;
	char    *orgbuf;

	orgblock	= block;
	orgnum_blocks	= num_blocks;
	orgbuf		= buf;


	CDDBG(fprintf(stderr,
">>>>cd: read_block: block/%d, buf/0x%x, #_blocks/%d, use_cache/%d line/%d<%s>\n",
					block, buf, num_blocks, use_cache,
					lineno, reason));
	/*
	 *  We'll look in the cache even if use_cache == 0; there's no point
	 *  in not checking.  We will not write to cache in this case.
	 */
	if (read_from_cache( cd, block, num_blocks, buf )) {

		CDDBG(fprintf(stderr,
			     ">>>>cd: read_block: read_from_cache success!\n"));

		cache_hits += num_blocks;

		return (0);
	}

	cache_misses += num_blocks;

	if (lseek(cd->dkfd, block * cd->blksize, L_SET) == -1) {

		CDDBG(fprintf(stderr,
				">>>>cd: read_block: lseek returned errno/%d\n",
									errno));
	    return errno;
	}

	/*
	 *  Do the read, in increments of BLOCK_LIMIT, which is the largest
	 *  number of blocks we can read with one call.  Check each time for
	 *  success
	 */
	for ( ; num_blocks > 0; num_blocks -= BLOCK_LIMIT,
				buf	   += BLOCK_LIMIT,
				block	   += BLOCK_LIMIT) {

		blocks = min( num_blocks, BLOCK_LIMIT );

		rc = read(cd->dkfd, buf, blocks * cd->blksize);

		if (rc < 0) {

			CDDBG(fprintf(stderr,
				">>>>cd: read_block: read returned errno/%d\n",
									errno));
			return errno;
		} else {
			CDDBG(fprintf(stderr,
			      ">>>>cd: read_block: read returned bytes/%d\n",
									rc));
		}
	}

	if (use_cache)
		write_to_cache( cd, orgblock, orgnum_blocks, orgbuf );
		
	return (0);
}

/*
 *  static int
 *  set_blksize( CDROM *cd, struct dsreq *dsp, blksize )
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
	MSELPARAM		params;
	sndata_t		data;
	struct dk_ioctl_data	arg;

	if (!really_is_CDROM) {
		return 1;
	}

	CDDBG(fprintf(stderr, ">>>>cd: set_blksize: cd/0x%x, blksize/%d\n",
								cd, blksize));
	/*
	 * some devices, like maxtor optical won't return default if no media
	 * in drive (or unformatted), but will return current... This is
	 * supposed to be fixed in new firmware soon, but it doesn't cost
	 * anything but a kernel error message to try both.
	 */
	if (   scsi_modesense(cd, &data, ALL|CURRENT)
	    && scsi_modesense(cd, &data, ALL|DEFAULT) ) {

		fprintf(stderr,
			"Unable to modesense the drive for blocksize.\n");
		return 0;
	}

	if (data.bd_len > 7) {
		/*
		 * Note that blockSize is the *logical* block size, and if
		 * available, is preferred over the *physical* sector size
		 * in f_bytes_sec, which may be different.  (In fact, this
		 * is the case with syquest drives, and also with the
		 * Maxtor Tahiti II.) 
		 */
		params.densityCode = data.block_descrip[0];
		params.blockSize   = sc_3bytes_uint(&data.block_descrip[5]);

	} else {
		fprintf(stderr,
		    "Unable to determine blocksize/density for the drive.\n");

		return 0;
	}

	/*
	 * If things are already set to what they're supposed to be,
	 * we don't need to do anything.
	 */
	if (   params.blockSize   == blksize
	    && params.densityCode == cd->density) {
		return 1;
	}

	CDDBG(fprintf(stderr,
">>>>cd: set_blksize: params.blockSize/%d != blksize/%d || params.densityCode/%d != cd->density/%d\n",
					params.blockSize, blksize,
					params.densityCode, cd->density));
	/*
	 * set the new blksize if we can.
	 */
	bzero(&params, sizeof params);

	params.blockDescLen = BLOCKDESCLEN;
	params.densityCode  = cd->density;
	params.blockSize    = blksize;

	arg.i_addr = (caddr_t) &params;
	arg.i_len  = sizeof params;
	arg.i_page = 0;

	if (ioctl(cd->dkfd, DIOCSELFLAGS, 0x10) != 0) {

		CDDBG(fprintf(stderr,
	">>>>cd: set_blksize: ioctl(DIOCSELFLAGS, 0x10) failed, errno/%d\n",
								errno));
	    return 0;
	}

	if (ioctl(cd->dkfd, DIOCSELECT, &arg) != 0) {

		CDDBG(fprintf(stderr,
	">>>>cd: set_blksize: ioctl(DIOCSELECT, &arg) failed, errno/%d\n",
								errno));
	    return 0;
	}

	return 1;			/* success */
}

static int
set_up_volume_header(int dkfd)
{
	int			capacity = 0;
	struct volume_header	vh;

	/*
	 *  Always create a volume header.  The previous disc may have been
	 * smaller or larger than this one.
	 *
	 *  The setting of heads and sectors is a gross hack for sw95
	 * and softwindows up to v4.0, because they tried to figure out
	 * if a CDROM was really a  CDROM by looking for the old dp_secs
	 * and dp_trks0 fields (AKA _dp_heads and _dp_sect in 6.4).
	 * The algorithm was rather flawed, but without this, those versions
	 * won't work at all.  v4.0 fixed the problem from insignia's side.
	 *  See bug #463795.
	 */

	const int byte_per_sec = 512;


	if (ioctl(dkfd, DIOCREADCAPACITY, &capacity) != 0) {

		CDDBG(fprintf(stderr,
	">>>>cd: set_up_volume_header: DIOCREADCAPACITY failed errno/%d\n",
									errno));
		return 0;
	}

	CDDBG(fprintf(stderr, ">>>>cd: set_up_volume_header: capacity/%d\n",
								capacity));

	bzero(&vh, sizeof vh);

	vh.vh_magic		= VHMAGIC;
	vh.vh_dp.dp_secbytes	= byte_per_sec;
	vh.vh_dp._dp_heads	= 1;			/* bug #463795 */
	vh.vh_dp._dp_sect	= 32;			/* bug #463795 */
	vh.vh_csum		= 0;

	vh.vh_pt[10].pt_nblks	 = capacity;
	vh.vh_pt[10].pt_firstlbn = 0;
	vh.vh_pt[10].pt_type	 = PTYPE_VOLUME;

	vh.vh_pt[8].pt_nblks	= 64;
	vh.vh_pt[8].pt_firstlbn	= 0;
	vh.vh_pt[8].pt_type	= PTYPE_VOLHDR;

	vh.vh_csum = -vhchksum(&vh);

	if (ioctl(dkfd, DIOCSETVH, &vh) != 0) {

		CDDBG(fprintf(stderr,
		">>>>cd: set_up_volume_header: DIOCSETVH failed errno/%d\n",
									errno));
		return 0;
	}

	return 1;
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

	CDDBG(fprintf(stderr,
	   ">>>>cd: read_from_cache: start_block/%d, num_blocks/%d, buf/0x%x\n",
						start_block, num_blocks, buf));
	if (!cd->cache_blocks)
		return (0);

	for (i = start_block; i - start_block < num_blocks; i++) {

		block = find_block( cd, i );

		if (!block) {
			CACHEDBG(fprintf(stderr,
					 ">>>>cache: Cache miss( %d, %d )\n",
						 start_block, num_blocks ));
			return (0);
		}

		bcopy( block, buf, cd->blksize );

		buf += cd->blksize;
	}

	CACHEDBG(fprintf( stderr, ">>>>cache: Cache hit( %d, %d )\n",
						start_block, num_blocks ));
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
	if (cd->block_num[block % cd->cache_blocks] == block) {
		CDDBG(fprintf(stderr,
			">>>>cd: find_block: block/%d FOUND\n", block));

		return (cd->cache + (block % cd->cache_blocks) * cd->blksize);
	}

	CDDBG(fprintf(stderr, ">>>>cd: find_block: block/%d !FOUND\n", block));

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


	CDDBG(fprintf(stderr,
	    ">>>>cd: write_to_cache: start_block/%d, num_blocks/%d, buf/0x%x\n",
						start_block, num_blocks, buf));

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
	CDDBG(fprintf(stderr,
		">>>>cd: insert_block: _block/%d, buf/0x%x\n", block, buf));

	if (!cd->cache_blocks)
		return;

	CACHEDBG(
		int slot = block % cd->cache_blocks;
		if (cd->block_num[slot]) {
			fprintf(stderr,
				">>>>cache: Replacing block %d with block %d\n",
						 cd->block_num[slot], block);
		}
	)
	cd->block_num[block % cd->cache_blocks] = block;
	bcopy(buf, cd->cache + (block % cd->cache_blocks) * cd->blksize,
	 cd->blksize);
}


/*
 *  static int act_like_a_cdrom( CDROM *cd, struct dsreq *dsp )
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
act_like_a_cdrom( CDROM *cd, struct dsreq *dsp )
{
	if (!really_is_CDROM) {
		return;
	}

	CDDBG(fprintf(stderr, ">>>>cd: act_like_a_cdrom:\n"));

	/*
	 * devices other than these 4 (and possibly some later SONY drives?)
	 * may do "bad" things and/or timeout on a c9 command, so only send
	 * it to drives that we think know what to do with it; see bug #314535.
	 */
	if (cd->type == TOSHIBA || cd->type == TOSHIBA_SCSI2 ||
            cd->type == TOSHIBA_PHOTO) {

		fillg1cmd( dsp, (uchar_t *)CMDBUF(dsp),
					0xc9, 0, 0, 0, 0, 0, 0, 0, 0, 0 );

		filldsreq( dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE );

		doscsireq( getfd( dsp ), dsp );
	}
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
	if (!really_is_CDROM) {
		return;
	}

	CDDBG(fprintf(stderr, ">>>>cd: prevent_removal: entered\n"));

	fillg0cmd( dsp, (uchar_t *)CMDBUF( dsp ), 0x1e, 0, 0, 0, 1, 0 );

	filldsreq( dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE );

	doscsireq( getfd( dsp ), dsp );
}


static void
allow_removal( struct dsreq *dsp )
{
	if (!really_is_CDROM) {
		return;
	}

	CDDBG(fprintf(stderr, ">>>>cd: allow_removal:\n"));

	fillg0cmd( dsp, (uchar_t *)CMDBUF( dsp ),
					0x1e, 0, 0, 0, 0, 0 );

	filldsreq( dsp, NULL, 0, DSRQ_READ|DSRQ_SENSE );

	doscsireq( getfd( dsp ), dsp );
}


/*
 * len is length of additional data plus 4 byte sense header 
 * plus 8 byte block descriptor + 2 byte page header
 */
#define SENSE_LEN_ADD (8+4+2)

static
scsi_modesense(CDROM *cd, sndata_t *addr, u_char pgcode)
{
	struct dk_ioctl_data	modesense_data;


	CDDBG(fprintf(stderr,
		">>>>cd: scsi_modesense: control/0x%x, pgcode/0x%x\n",
					(pgcode & ~ALL) >> 6, pgcode & ALL));

	modesense_data.i_page = pgcode;	/* before AND ! */

	pgcode &= ALL;

	if ( ! cd->pglengths[pgcode]) {

		CDDBG(fprintf(stderr, 
			">>>>cd: page 0x%xd not supported for MODE SENSE\n",
								pgcode));

		return 1;
	}

	modesense_data.i_addr = (caddr_t)addr;

	/*
	 * len is length of additional data plus 4 byte sense header 
	 * plus 8 byte block descriptor + 2 byte page header
	 */
	modesense_data.i_len = cd->pglengths[pgcode] + SENSE_LEN_ADD;

				/* only one byte! (don't want modulo) */
	if (modesense_data.i_len > 0xff)
		modesense_data.i_len = 0xff;

	if (ioctl(cd->dkfd, DIOCSENSE, &modesense_data) < 0) {
		/*
		 * don't complain if ALL, caller gives slightly different msg
		 */
		if (pgcode != ALL)
			fprintf(stderr,
			"Warning: problem reading drive parameters (page %d)",
								pgcode);
			/*
			 * else get 'better' message in scsi_getsupported()
			 */
		return 1;
	}

	return 0;
}

/*
 * find supported pages and lengths
 */
static void
scsi_getsupported(CDROM *cd)
{
	register	i, maxd, pgnum;
	sndata_t	data;
	u_char		*d = (u_char *)&data;


	CDDBG(fprintf(stderr, ">>>>cd: scsi_getsupported:\n"));

	bzero(&data, sizeof(data));
					/* in case not first call */
	bzero(cd->pglengths, sizeof(cd->pglengths));

	cd->pglengths[ALL] = sizeof(data) - (1 + SENSE_LEN_ADD);

	/*
	 * some devices, like maxtor optical won't return default if no media
	 * in drive (or unformatted), but will return current... This is
	 * supposed to be fixed in new firmware soon, but it doesn't cost
	 * anything but a kernel error message to try both.
	 */
	if (   scsi_modesense(cd, &data, ALL|CURRENT)
	    && scsi_modesense(cd, &data, ALL|DEFAULT) ) {

		cd->pglengths[ALL] = 0;

		fprintf(stderr,
			"Unable to get list of supported configuration pages");
		return;
	}

	/*
	 * sense_len doesn't include itself;
	 * set cd->pglengths[ALL] for completeness
	 */
	cd->pglengths[ALL] = maxd = data.sense_len + 1;

	if (data.bd_len > 7)
		i = 4 + data.bd_len;	/* skip header and block desc. */
	else
		i = 4;			/* skip just the header */

	while (i < maxd) {
		pgnum = d[i] & ALL;

		cd->pglengths[pgnum] = d[i+1];

		i += cd->pglengths[pgnum] + 2;	/* +2 for header */
	}
}
