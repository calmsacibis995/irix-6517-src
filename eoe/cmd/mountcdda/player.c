/*
 *  player.c
 *
 *  Description:
 *      Code to perform functions of a CD player using the CD-ROM driver
 *
 *  History:
 *	cook	    5/5/92	added CDatotime, CDframetotc, CDframetomsf
 *	msc	    10/14/91	Changed names; added digital audio support
 *      rogerc      10/29/90    Created
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/scsi.h>

#include <stdio.h>
#include <dslib.h>
#include <fcntl.h>
#include <invent.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>
#include <alloca.h>
#include "cdaudio.h"

/*
 * Buffer lengths
 */

#define CD_GRP0_CMDLEN  6
#define CD_GRP6_CMDLEN  10
#define CD_STATUSLEN    11
#define CD_SENSELEN     18
#define MSELPARMLEN 12

#define STA_RETRY       (-1)

/*
 * SCSI commands
 *
 * T_ for Toshiba
 * S_ for Sony
 * S2_ for SCSI II
 * SGI_ for SGI specific firmware stuff
 */

#define T_TRSEARCH          0xc0
#define T_PLAY              0xc1
#define T_STILL             0xc2
#define T_EJECT             0xc4
#define T_SUBCODE           0xc6
#define T_READDISC          0xc7

#define S_SETADDRFMT        0xc0
#define S_TOC               0xc1
#define S_PLAY_MSF          0xc7

#define S2_SUBCODE          0x42
#define S2_TOC              0x43
#define S2_PLAYMSF          0x47
#define S2_PLAYTRACK        0x48
#define S2_PAUSE            0x4b

#define SGI_CDROM           0xc9

#define G0_REZERO           0x01

/*
 * While we've got the device, we'll use CDROM_BLKSIZE.  When
 * we're done, we'll return it to UNIX_BLKSIZE
 */
#define CDROM_BLKSIZE       2048
#define UNIX_BLKSIZE        512

#define CD_PSTATUS_PLAYING  0x00
#define CD_PSTATUS_STILL    0x01
#define CD_PSTATUS_PAUSE    0x02
#define CD_PSTATUS_OTHER    0x03

/*
 * Filling command buffers for vendor-specific commands
 */
#define fillg6cmd fillg1cmd

/*
 * Filling command buffers for SCSI-2 commands
 */
#define fills2cmd fillg1cmd

/*
 * Filling command buffers for SGI specific commands
 */
#define fillsgicmd fillg1cmd

/*
 *  Convert to and from Binary Coded Decimal
 */
#define  TWODIGITBCDTOINT( bcd )    (((bcd) >> 4) * 10 + ((bcd) & 0xf))
#define  INTTOTWODIGITBCD( i )      ((((i) / 10) << 4) | ((i) % 10))

#ifdef DEBUG
#define CDDBG(x) {if (cddbg) {x;}}
#define PRINTSENSE(dsp) { \
	int i;\
	if (STATUS(dsp) == STA_CHECK) {\
		for (i = 0; i < CD_SENSELEN; i++)\
			fprintf( stderr, "%02x ", SENSEBUF(dsp)[i] );\
		fprintf( stderr, "\n" );\
	}}
static int cddbg = 1;
#else
#define CDDBG(x)
#define PRINTSENSE(dsp)
#endif

enum drivetype { UNKNOWN, TOSHIBA, SONY, TOSHIBA_SCSI2 };

typedef struct {
	int min, sec, frame;
}   TRACK;

/*
 * private data structure representing a cd player and its state
 */

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
  unsigned long	cur_block;  /* Logical block no. of current location */  struct dsreq    *dsp;       /* Struct for doing SCSI requests */
  int		excl_id;    /* Exclusive use id from mediad */
  int             state;      /* State of player */
  enum drivetype  type;       /* Type of the CDROM drive */
  TRACK           *toc;       /* Table of contents */
  char            *dev;       /* name of dev scsi device */
};


/*
 * Convert from the CD six-bit format to ASCII.
 */
static unchar sbtoa[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9',  0,   0,   0,   0,   0,   0,
    '0', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z',  0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
};

/*
 * private function declarations
 */

static int read_disc( CDPLAYER *cdplayer );
static void act_like_a_cdrom( struct dsreq *dsp );
static int read_toc( CDPLAYER *cd );
static int set_blksize( CDPLAYER *cd, int blksize );
static void toshiba_modesense( struct dsreq *dsp, char *params, int size );
static void scsi2_modesense( struct dsreq *dsp, char *params, int size );

/*
 * Modesense is a little different between Sony and Toshiba
 */
static void (*modesense[])( struct dsreq *, char *, int ) =
	{ NULL, toshiba_modesense, scsi2_modesense, scsi2_modesense };

priv_modeselect15(dsp, data, datalen, save, vu)
  struct dsreq *dsp;
  caddr_t data;
  long datalen;
  char save, vu;
{
  fillg0cmd(dsp, CMDBUF(dsp), G0_MSEL, save, 0, 0, B1(datalen), B1(vu<<6));
  filldsreq(dsp, data, datalen, DSRQ_WRITE|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}

/*
 *  CDPLAYER *CDopen( dev, dir )
 *
 *  Description:
 *      Intitializes a data structure for a cdplayer, and returns a pointer to
 *      it.
 *
 *  Parameters:
 *      dev     Device to use for playing CD's; if NULL, attempts to locate
 *              a CD-ROM drive in /dev/scsi
 *
 *	dir	"r", "w" or "rw" indicating what you will do with the drive.
 *		This is for future writable CD devices
 *
 *  Returns:
 *      Pointer to a CDPLAYER struct if successful, NULL if error
 */

CDPLAYER *CDopen( char const *devscsi, char const *dir )
{
	struct dsreq    *dsp = 0;
#define INQUIRY_SIZE 98
	unsigned char   *inquiry = (unsigned char *)alloca(INQUIRY_SIZE);
	CDPLAYER        *cdplayer = 0;
	char            devbuf[300];
	inventory_t     *inv;
	static char     *toshiba = "TOSHIBA CD";
	static char     *sony = "SONY    CD";
	enum drivetype  type;
#define MSELCMD_SIZE 28
	unsigned char	*mselcmd = (unsigned char *)alloca(MSELCMD_SIZE);
	CDTRACKINFO	info;
	int 		excl_id, changed;
       

	if (devscsi) {
	  excl_id = 1;
/* 	  Mediad_Get_Exclusiveuse(Devscsi, "Libcdaudio"); */
/* 	  fprintf(stderr,"grabbing mediad_exclusiveuse\n"); */

	  if (excl_id >= 0) {
	    dsp = dsopen( devscsi, O_RDONLY);
	  } else {
	    setoserror(EACCES);
	    return NULL;
	  }
	} else {
	  setinvent( );
	  for (inv = getinvent( ); inv; inv = getinvent( ))
	    if (inv->inv_class == INV_SCSI
		&& inv->inv_type == INV_CDROM)
	      break;

	  if (inv) {
	    sprintf(devbuf, "/dev/scsi/sc%dd%dl%d",
		    inv->inv_controller, inv->inv_unit,
		    (inv->inv_state & 0xff00) >> 8 );
	    devscsi = devbuf;
	    excl_id = 1;
	    /* this was messing up mediad */
/* 	    mediad_get_exclusiveuse(devscsi, "libcdaudio"); */
/* 	    fprintf(stderr,"grabbing mediad_exclusiveuse\n"); */

	    if (excl_id < 0) {
	      setoserror(EACCES);
	      endinvent();
	      return(NULL);
	    }
	    dsp = dsopen( devscsi, O_RDONLY );
	  } else {
	    setoserror(ENODEV); /* Can't locate a CD-ROM drive */
	    endinvent( );
	    return (NULL);
	  }
	  endinvent( );
	}

	/* if dsopen failed, it should have set errno. */
	if (dsp) {
		if (inquiry12( dsp, inquiry, INQUIRY_SIZE, 0 ) == 0) {
			if (strncmp( inquiry + 8, toshiba, strlen( toshiba ) ) == 0)

				type = ((inquiry[2] & 0x7) == 2) ? TOSHIBA_SCSI2 : TOSHIBA;
			else if (strncmp( inquiry + 8, sony, strlen( sony ) ) == 0)
				type = SONY;
			else {
				/*
				 * Default to TOSHIBA_SCSI2, so that
				 * users connecting non-SGI CD-ROM
				 * drives to our systems have some
				 * chance of working.  Note that the
				 * modeselect command for scsi audio
				 * below will probably fail.
				 */
				type = TOSHIBA_SCSI2;
			}
		} else {
		    setoserror(ENODEV); /* Device didn't respond to inquiry */
		    return (NULL);
		}

		if (!(SENSEBUF( dsp ) = (char *)malloc( CD_SENSELEN ) )) {
			setoserror(ENOMEM);
			return (NULL);
		}
		SENSELEN( dsp ) = CD_SENSELEN;

		act_like_a_cdrom( dsp );
		if (!(cdplayer = (CDPLAYER *)malloc( sizeof(CDPLAYER) ))) {
			free( SENSEBUF( dsp ) );
			setoserror(ENOMEM);
			return (NULL);
		}
		memset( cdplayer, 0, sizeof (CDPLAYER) );
		cdplayer->dsp = dsp;
		cdplayer->type = type;
		cdplayer->excl_id = excl_id;
		cdplayer->dev = strdup(devscsi);
		CDtestready( cdplayer, &changed );
		cdplayer->state = STATUS(cdplayer->dsp) == STA_GOOD ? CD_READY :
		 STATUS(cdplayer->dsp) == STA_CHECK ? CD_NODISC : CD_ERROR;

		CDupdatestatus( (CDPLAYER *)cdplayer );
		if (cdplayer->state == CD_READY) CDpreventremoval( cdplayer );
		if (type == SONY) {
		    set_blksize( cdplayer, CDROM_BLKSIZE );
		    /* Position DA read address at start of disc */
		    cdplayer->cur_block = 0;
		} else if (type == TOSHIBA_SCSI2) {
		    /*
		     * Switch the drive to reading audio data
		     */
		    bzero(mselcmd, MSELCMD_SIZE);
		    mselcmd[3] = 8;
		    mselcmd[4] = 0x82;
		    mselcmd[9] = CDDA_BLOCKSIZE >> 16;
		    mselcmd[10] = (CDDA_BLOCKSIZE & 0x0000ff00) >> 8;
		    mselcmd[11] = CDDA_BLOCKSIZE & 0x000000ff;
		    mselcmd[12] = 2;	/* modeselect page 2 */
		    mselcmd[13] = 0xe;	/* length of page 2 */ 
		    mselcmd[14] = 0x6f; /* buffer full ratio - reconnect after
					/* 16 blocks of data */
 		    priv_modeselect15(cdplayer->dsp, mselcmd, MSELCMD_SIZE, 0x10, 0); 
/*  		    modeselect15(cdplayer->dsp, mselcmd, MSELCMD_SIZE, 0x10, 0);  */
		    if (STATUS( dsp ) == STA_GOOD) {
			cdplayer->scsi_audio++;
			if (cdplayer->state == CD_READY) {
			    /* Position DA read address at start of disc */
			    CDgettrackinfo( cdplayer, 1, &info);
			    cdplayer->cur_block = CDmsftoblock(cdplayer,
						       info.start_min,
						       info.start_sec,
						       info.start_frame);
			}
		    }
		}
	}

	return (cdplayer);
}

/*
 * Return the CD-ROM's file descriptor for use with select.
 */
int CDgetfd( CDPLAYER *cd )
{
    getfd( cd->dsp );
}

/*
 * Return the ideal number of frames to read to get continuous
 * streaming data.  This is related to the buffer full ratio
 * used in CDopen above.
 */
int CDbestreadsize( CDPLAYER *cd )
{
    return 12;
}

/*
 * Convert minutes, seconds and frames of to a CD frame number.
 */
unsigned long
CDmsftoframe(int m, int s, int f)
{
    return (m*60*75 + s*75 + f);
}

unsigned long
CDtctoframe(struct cdtimecode* tc)
{
    return CDmsftoframe(tc->mhi * 10 + tc->mlo,
			tc->shi * 10 + tc->slo,
			tc->fhi * 10 + tc->flo);
}

/*
 * Convert an ASCII string to a struct {cd,mt}timecode which can then be
 * used as an argument to a search ioctl.
 *
 * This returns 0 if the string does not represent a valid timecode,
 * non-zero otherwise.
 */
int CDatotime(struct cdtimecode *tp, const char *loc)
{
    int m, s, f;
    
    if (!CDatomsf(loc, &m, &s, &f)) {
	return 0;
    }
    
    /*
     * put the fields into the timecode struct
     */
    tp->mhi = m / 10;
    tp->mlo = m % 10;
    tp->shi = s / 10;
    tp->slo = s % 10;
    tp->fhi = f/ 10;
    tp->flo = f % 10;
    return 1;                                           /* valid timecode */
}

int CDatomsf(const char *loc,  int *m,  int *s, int *f)
{
    char buf[80];
    char *tok;
    int field[3];
    int fno = 0;
    char *str = buf;

    /*
     * zero out the fields. Fields which are not specified thus default
     * to zero.
     */
    bzero(field, 3 * sizeof(int));
    /*
     * copy to a temporary buffer; strtok is destructive
     * our token separators are arbitrary here; likely most people will
     * use the ':'
     */
    strncpy(buf, loc, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    while ((tok = strtok(str,":-/#|,.;*")) && fno < 3) {
        int i;
        str = 0;
        field[fno++] = atoi(tok);
    }

    /*
     * check fields for validity
     */
    if (field[0] < 0 || field[0] > 119 ||               /* check minutes */
        field[1] < 0 || field[1] > 59 ||                /* check seconds */
        field[2] < 0 || field[2] > 74) {                /* check frames */
        return 0;
    }
    /*
     * put the fields into msf
     */
    *m = field[0];
    *s = field[1];
    *f = field[2];
    return 1;                                           /* valid timecode */
}



void
CDframetotc(unsigned long fr, struct cdtimecode* tc)
{
    int m,s,f;
    CDframetomsf(fr,&m,&s,&f);
    /*
     * now set the msf fields in the timecode. These are in BCD;
     */
    tc->mhi = m / 10;
    tc->mlo = m % 10;
    tc->shi = s / 10;
    tc->slo = s % 10;
    tc->fhi = f / 10;
    tc->flo = f % 10;
}

void
CDframetomsf(unsigned long fr, int *m, int *s, int *f)
{
    unsigned long mrem;
    *m = fr / (60*75);
    mrem = fr % (60*75);
    *s = mrem/75;
    *f = mrem % 75;
}

/*
 * Convert minutes, seconds and frames of absolute time code to
 * a logical block number on the device.
 */
unsigned long
CDmsftoblock(CDPLAYER *cd, int m, int s, int f)
{
    if (cd->type == TOSHIBA_SCSI2)
	return (m*60*75 + s*75 + f - 150);
    else
	return (m*60*75 + s*75 + f);
}

void
CDblocktomsf(CDPLAYER *cd, unsigned long fr, int *m, int *s, int *f)
{
    unsigned long mrem;
    if (cd->type == TOSHIBA_SCSI2)
	fr += 150;
    *m = fr / (60*75);
    mrem = fr % (60*75);
    *s = mrem/75;
    *f = mrem % 75;
}

/*
 * Seek to a given sub code frame
 */
unsigned long CDseek( CDPLAYER *cd, int m, int s, int f)
{
    CDTRACKINFO info;
    unsigned long newblock;

    if (!cd->scsi_audio) {
	setoserror(ENXIO);
	return (-1);
    }
    newblock = CDmsftoblock(cd, m, s, f);
    CDgettrackinfo(cd, cd->first, &info);
    if (CDmsftoblock(cd, info.start_min, info.start_sec, info.start_frame)
	<= newblock) {
        CDgettrackinfo(cd, cd->last, &info);
        if (CDmsftoblock(cd,
			 info.start_min + info.total_min,
			 info.start_sec + info.total_sec,
			 info.start_frame + info.total_frame)
	    >= newblock) {
    	    cd->cur_block = newblock;
	    return newblock;
	}
    }
    setoserror(EINVAL);
    return (-1);
}

/*
 * Seek to a given block
 */
unsigned long CDseekblock( CDPLAYER *cd, unsigned long block)
{
    CDTRACKINFO info;

    if (!cd->scsi_audio) {
	setoserror(ENXIO);
	return (-1);
    }
    CDgettrackinfo(cd, cd->first, &info);
    if (CDmsftoblock(cd, info.start_min, info.start_sec, info.start_frame)
	<= block) {
        CDgettrackinfo(cd, cd->last, &info);
        if (CDmsftoblock(cd,
			 info.start_min + info.total_min,
			 info.start_sec + info.total_sec,
			 info.start_frame + info.total_frame)
	    >= block) {
    	    cd->cur_block = block;
	    return block;
	}
    }
    setoserror(EINVAL);
    return (-1);
}

/*
 * Seek to the start of the given program number (track) on the CD.
 */
int CDseektrack( CDPLAYER *cd, int t)
{
    CDTRACKINFO info;

    if (!cd->scsi_audio) {
	setoserror(ENXIO);
	return (-1);
    }
    if (CDgettrackinfo( cd, t, &info )) {
        cd->cur_block = CDmsftoblock(cd,
				     info.start_min,
				     info.start_sec,
				     info.start_frame);
	return (cd->cur_block);
    }
    setoserror(EIO);
    return (-1);
}

/*
 * Read num_frames subcode frames of digital audio data starting at current
 * position. Update position after the read.
 */
int CDreadda( CDPLAYER *cd, CDFRAME *buf, int num_frames )
{
  int changed;
  unsigned char	sense[CD_SENSELEN], *cmd;

  if (!cd->scsi_audio) {
    setoserror(ENXIO);
    return (-1);
  }
  if (!cd->dsp) {
    setoserror(EBADF);
    return (-1);
  }

  CDtestready(cd, &changed );

  if (STATUS(cd->dsp) != STA_GOOD) {
    setoserror(EAGAIN);
    return (-1);
  }

  if (cd->state != CD_READY) {
    setoserror(EIO);
    return (-1);
  }

  cmd = CMDBUF( cd->dsp );
  CMDLEN( cd->dsp ) = 12;	/* 12 byte vendor specific command */
  filldsreq( cd->dsp, (char *)buf,
	     num_frames * (cd->type == SONY ? CDDA_DATASIZE : CDDA_BLOCKSIZE),
	     DSRQ_READ | DSRQ_SENSE );
  cmd[0] = cd->type == SONY ? 0xd8 : 0xa8;	    
  cmd[1] = 0;
  cmd[2] = cd->cur_block >> 24;
  cmd[3] = (cd->cur_block & 0x00ff0000) >> 16;
  cmd[4] = (cd->cur_block & 0x0000ff00) >> 8;
  cmd[5] = cd->cur_block & 0xff;
  if (cd->type == SONY) {
    cmd[6] = 0;
    cmd[7] = num_frames >> 24;
    cmd[8] = (num_frames & 0x00ff0000) >> 16;
    cmd[9] = (num_frames & 0x0000ff00) >> 8;
    cmd[10] = num_frames & 0xff;
    cmd[11] = 0;
    /* Sony drive currently doesn't deliver subcodes */
    bzero(&buf->subcode, sizeof(buf->subcode));
  } else {
    cmd[6] = num_frames >> 24;
    cmd[7] = (num_frames & 0x00ff0000) >> 16;
    cmd[8] = (num_frames & 0x0000ff00) >> 8;
    cmd[9] = num_frames & 0xff;
    cmd[10] = 0;
    cmd[11] = 0;
  }
  doscsireq( getfd( cd->dsp ), cd->dsp );
  if (STATUS(cd->dsp) != STA_GOOD)
    return (0);		/* EOF */
  cd->cur_block += num_frames;
  return (DATASENT(cd->dsp)
	  / (cd->type == SONY ? CDDA_DATASIZE : CDDA_BLOCKSIZE));
}

/*
 *  int CDplaytrack( CDPLAYER *cd, int track )
 *
 *  Description:
 *      Play one track of the CD.  Play stops at end of track
 *
 *  Parameters:
 *      cd          pointer to CDPLAYER struct
 *      track       the track number of the track to play
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

int CDplaytrack( CDPLAYER *cdplayer, int track, int play )
{
	register struct dsreq   *dsp = cdplayer->dsp;
	int                     stop, bcd_track, changed;
	CDTRACKINFO             info, first_info;

	CDtestready( cdplayer, &changed);

	if (STATUS(cdplayer->dsp) != STA_GOOD)
		return (0);

	if (cdplayer->first > track || cdplayer->last < track)
		return (0);

	if (cdplayer->type == TOSHIBA) {
		bcd_track = INTTOTWODIGITBCD( track );
		fillg6cmd( dsp, CMDBUF(dsp), T_TRSEARCH, 0, bcd_track,
		 0, 0, 0, 0, 0, 0, 0x80 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );

		if (STATUS(dsp) != STA_GOOD) {
			CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
			cdplayer->state = CD_ERROR;
			return (0);
		}

		stop = track < cdplayer->last ? INTTOTWODIGITBCD( track + 1 ) : 0;
		fillg6cmd( dsp, CMDBUF(dsp), T_PLAY, 4, stop, 0, 0, 0, 0, 0, 0, 0x80 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );
	}
	else {
		fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), S2_PLAYTRACK, 0, 0, 0,
		 track, 1, 0, track, 1, 0 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
	}

	if (STATUS(dsp) != STA_GOOD) {
		CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
		cdplayer->state = CD_ERROR;
		return (0);
	}
	cdplayer->state = CD_PLAYING;

	if (!play)
		CDtogglepause( cdplayer );

	return (1);
}

/*
 *  int
 *  CDplaytrackabs( CDPLAYER *cdplayer, int track, int min, int sec,
 *	 int frame, int play )
 *
 *  Description:
 *		Play one track, starting at min:sec:frame within that track
 *
 *  Parameters:
 *      cdplayer
 *      track
 *      min
 *      sec
 *      frame
 *      play		if non-zero, play; if 0, pause here.
 *
 *  Returns:
 *		non-zero if successful, 0 if error
 */

int
CDplaytrackabs( CDPLAYER *cdplayer, int track, int min, int sec,
 int frame, int play )
{
	register struct dsreq   *dsp = cdplayer->dsp;
	CDTRACKINFO             info;
	int                     block, stop, stop_min, stop_sec, stop_frame, changed;

	CDtestready( cdplayer, &changed );

	if (STATUS(cdplayer->dsp) != STA_GOOD)
		return (0);

	CDgettrackinfo( cdplayer, track, &info );
	frame = info.start_frame + frame + 75 *
	 (info.start_sec + sec + 60 * (info.start_min + min));
	sec = frame / 75;
	min = sec / 60;
	sec %= 60;
	frame %= 75;

	if (cdplayer->type == TOSHIBA) {
		min = INTTOTWODIGITBCD( min );
		sec = INTTOTWODIGITBCD( sec );
		frame = INTTOTWODIGITBCD( frame );

		fillg6cmd( dsp, CMDBUF(dsp), T_TRSEARCH, 0, min, sec,
		 frame, 0, 0, 0, 0, 0x40 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );

		stop = track < cdplayer->last ? INTTOTWODIGITBCD( track + 1 ) : 0;
		fillg6cmd( dsp, CMDBUF(dsp), T_PLAY, 4, stop, 0, 0, 0, 0, 0, 0, 0x80 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );

		if (!play)
			CDtogglepause( cdplayer );

	}
	else {
		stop_frame = info.start_frame + info.total_frame + 75 *
		 (info.start_sec + info.total_sec + 60 *
		 (info.start_min + info.total_min));
		stop_sec = stop_frame / 75;
		stop_min = stop_sec / 60;
		stop_sec %= 60;
		stop_frame %= 75;

		/*
		 * Goto pause state if we're playing
		 */
		if (cdplayer->state == CD_PLAYING) {
			fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), S2_PAUSE, 0, 0, 0, 0, 0,
			 0, 0, 0, 0 );
			filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
			doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
		}

		fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), S2_PLAYMSF, 0,
		 0, min, sec, frame, stop_min, stop_sec, stop_frame, 0 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );

		PRINTSENSE( cdplayer-> dsp );

		/*
		 * Stop playing if play == 0
		 */
		if (STATUS( cdplayer->dsp ) == STA_GOOD && !play) {
			fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp),
			 S2_PAUSE, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
			filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
			doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
		}
	}

	if (STATUS(dsp) != STA_GOOD) {
		CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
		cdplayer->state = CD_ERROR;
		return (0);
	}

	cdplayer->state = play ? CD_PLAYING : CD_PAUSED;
	cdplayer->paused = cdplayer->state == CD_PAUSED;
	return (1);
}

/*
 *  int CDplayabs( CDPLAYER *cd, int min, int sec, int frame, int play )
 *
 *  Description:
 *      Start playing at an absolute time on the CD
 *
 *  Parameters:
 *      cd          Pointer to a CDPLAYER struct
 *      min         absolute time to start playing
 *      sec         absolute seconds to start playing
 *      frame       absolute frames to start playing
 *      play        start playing if 1, pause if 0
 *
 *  Returns:
 *      1 if successful, 0 if error
 */

int CDplayabs( CDPLAYER *cdplayer, int min, int sec, int frame, int play )
{
	register struct dsreq   *dsp = cdplayer->dsp;
	CDTRACKINFO             info;
	int                     block, changed;

	CDtestready( cdplayer, &changed );

	if (STATUS(cdplayer->dsp) != STA_GOOD)
		return (0);

	if (cdplayer->type == TOSHIBA || cdplayer->type == TOSHIBA_SCSI2) {
		min = INTTOTWODIGITBCD( min );
		sec = INTTOTWODIGITBCD( sec );
		frame = INTTOTWODIGITBCD( frame );

		fillg6cmd( dsp, CMDBUF(dsp), T_TRSEARCH, play ? 1 : 0, min, sec,
		 frame, 0, 0, 0, 0, 0x40 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );
	}
	else {
		/*
		 * Goto pause state if we're playing
		 */
		if (cdplayer->state == CD_PLAYING) {
			fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), S2_PAUSE, 0, 0, 0, 0, 0,
			 0, 0, 0, 0 );
			filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
			doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
		}

		/*
		 * Seek to the right place
		 */
		block = frame +
		 (75 * (sec + 60 * min));
		
		/*
		 * Subtract the address of the first track.  I'm not sure
		 * why this is necessary, but it is.
		 */
		if (CDgettrackinfo( cdplayer, cdplayer->first, &info ))
			block -= info.start_frame +
			 (75 * (info.start_sec + 60 * info.start_min));
		fillg1cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), G1_SEEK, 0, B4( block ),
		 0, 0, 0, 0 );
		filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );

		/*
		 * Start playing if play != 0
		 */
		if (STATUS( cdplayer->dsp ) == STA_GOOD && play) {
			fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), S2_PAUSE, 0, 0, 0, 0, 0,
			 0, 0, 1, 0 );
			filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
			doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
		}
	}

	if (STATUS(dsp) != STA_GOOD) {
		PRINTSENSE(cdplayer->dsp);
		CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
		cdplayer->state = CD_ERROR;
		return (0);
	}

	cdplayer->state = play ? CD_PLAYING : CD_PAUSED;
	cdplayer->paused = cdplayer->state == CD_PAUSED;
	return (1);
}

/*
 *  int CDstop( CDPLAYER *cd )
 *
 *  Description:
 *      Stops play of the CD
 *
 *  Parameters:
 *      cd          pointer to a CDPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

int CDstop( CDPLAYER *cdplayer )
{
	register struct dsreq   *dsp = cdplayer->dsp;

	cdplayer->paused = 0;
	if (cdplayer->type == TOSHIBA) {
		fillg0cmd( dsp, CMDBUF(dsp), G0_REZERO, 0, 0, 0, 0, 0 );
		filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );
	}
	else {
		fillg0cmd( dsp, CMDBUF(dsp), G0_STOP, 0, 0, 0, 0, 0 );
		filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );

		fillg0cmd( dsp, CMDBUF(dsp), G0_SEEK, 0, B2( 0 ), 0, 0 );
		filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
		doscsireq( getfd( dsp ), dsp );
	}

	if (STATUS(dsp) != STA_GOOD) {
		CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
		cdplayer->state = CD_ERROR;
		return (0);
	}

	return (1);
}

/*
 *  int CDtogglepause( CDPLAYER *cd )
 *
 *  Description:
 *      if CD is playing, pause it.  If CD is paused, resume play
 *
 *  Parameters:
 *      cd          pointer to a CDPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 if not
 */

int CDtogglepause( CDPLAYER *cdplayer )
{
  int changed;
  register struct dsreq   *dsp = cdplayer->dsp;

  CDtestready( cdplayer, &changed );

  if (STATUS(cdplayer->dsp) != STA_GOOD)
    return (0);

  if (cdplayer->state == CD_ERROR)
    return (0);

  if (cdplayer->type == TOSHIBA)
    fillg6cmd( dsp, CMDBUF(dsp), cdplayer->state == CD_PLAYING ?
	       T_STILL : T_PLAY, cdplayer->state == CD_PLAYING ? 0 : 4,
	       0, 0, 0, 0, 0, 0, 0, 0xc0 );
  else
    fills2cmd( dsp, CMDBUF(dsp), S2_PAUSE, 0, 0, 0, 0, 0, 0, 0,
	       cdplayer->state == CD_PLAYING ? 0 : 1, 0 );

  filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
  doscsireq( getfd( dsp ), dsp );

  if (STATUS(dsp) == STA_GOOD) {
    cdplayer->state = cdplayer->state == CD_PLAYING ?
      CD_PAUSED : CD_PLAYING;
    cdplayer->paused = cdplayer->state == CD_PAUSED;
  }
  else {
    CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
    cdplayer->state = CD_ERROR;
    return (0);
  }
  return (1);
}

/*
 *  int CDeject( CDPLAYER *cd  )
 *
 *  Description:
 *      Ejects the CD caddy from the CD-ROM drive
 *
 *  Parameters:
 *      cd          pointer to a CDPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

int CDeject( CDPLAYER *cdplayer  )
{
	int             open = 0;
	int             status, changed;

	CDtestready( cdplayer, &changed );

	if (STATUS(cdplayer->dsp) != STA_GOOD) {
		setoserror(EIO);
		return (0);
	}

	if (cdplayer->type == TOSHIBA) {
		fillg6cmd(cdplayer->dsp, CMDBUF(cdplayer->dsp),
			  T_EJECT, 1, 0, 0, 0, 0, 0, 0, 0, 0);
		filldsreq(cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE);
		doscsireq(getfd( cdplayer->dsp ), cdplayer->dsp);
		status = STATUS(cdplayer->dsp) == STA_GOOD;
	}
	else {
		CDallowremoval( cdplayer );
		fillg0cmd(cdplayer->dsp, CMDBUF(cdplayer->dsp),
			  G0_LOAD, 1, 0, 0, 2, 0);
		filldsreq(cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE);
		doscsireq(getfd(cdplayer->dsp), cdplayer->dsp);
		status = STATUS(cdplayer->dsp) == STA_GOOD;
		CDpreventremoval(cdplayer);
	}

	cdplayer->paused = 0;

	if (!status) {
		setoserror(EIO);
	}

	return status;
}

/*
 *  int CDclose( CDPLAYER *cd  )
 *
 *  Description:
 *      Closes the dsp's file descriptor, and frees previously allocated
 *      memory
 *
 *  Parameters:
 *      cd          Pointer to a CDPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

int CDclose( CDPLAYER *cdplayer  )
{

  if (cdplayer->dsp) {
    CDallowremoval( cdplayer );
    if (cdplayer->type == SONY || cdplayer->type == TOSHIBA_SCSI2)
      set_blksize( cdplayer, UNIX_BLKSIZE );
    free( SENSEBUF( cdplayer->dsp ) );
    dsclose( cdplayer->dsp );
  }
/*   if (cdplayer->excl_id) { */
/*     fprintf(stderr,"releasing mediad_exclusiveuse\n"); */
/*     mediad_release_exclusiveuse(cdplayer->excl_id); */
/*   }     */
  free(cdplayer->dev);
  free(cdplayer);
  return (1);
}

/*
 *  static int CDupdatestatus( CDPLAYER *cd )
 *
 *  Description:
 *      Check the status of the CD-ROM drive, updating our data structures
 *
 *  Parameters:
 *      cd          pointer to a CDPLAYER structure
 *
 *  Returns:
 *      non-zero if successful, 0 if error
 */

int CDupdatestatus( CDPLAYER *cdplayer )
{
#define TRACK_INFO_SIZE 4
  int             min;
  int             sec;
  int changed;
  unsigned char   *track_info = (unsigned char *)alloca(TRACK_INFO_SIZE);

  CDtestready( cdplayer, &changed );
  if (STATUS(cdplayer->dsp) != STA_GOOD) {
    cdplayer->state = CD_NODISC;
    return (1);
  }

  if (!read_disc( cdplayer )) {
    if (cdplayer->state == CD_CDROM)
      return (1);
    else {
      CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
      cdplayer->state = CD_ERROR;
      return (0);
    }
  }

  if (cdplayer->type == TOSHIBA) {
    unsigned char   *status = (unsigned char *)alloca(CD_STATUSLEN);

    fillg6cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), T_SUBCODE, CD_STATUSLEN,
	       0, 0, 0, 0, 0, 0, 0, 0 );
    filldsreq( cdplayer->dsp, status, CD_STATUSLEN,
	       DSRQ_READ | DSRQ_SENSE );
    doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );

    if (STATUS(cdplayer->dsp) == STA_GOOD) {
      switch (status[0]) {

      case CD_PSTATUS_PLAYING:
	cdplayer->state = CD_PLAYING;
	break;

      case CD_PSTATUS_STILL:
	cdplayer->state = CD_STILL;
	break;

      case CD_PSTATUS_PAUSE:
	cdplayer->state = CD_PAUSED;
	break;

      case CD_PSTATUS_OTHER:
	cdplayer->state = CD_READY; /* just a guess for now */
	break;
      }

      cdplayer->track = TWODIGITBCDTOINT( status[2] );
      cdplayer->min = TWODIGITBCDTOINT( status[4] );
      cdplayer->sec = TWODIGITBCDTOINT( status[5] );
      cdplayer->frame = TWODIGITBCDTOINT( status[6] );
      cdplayer->abs_min = TWODIGITBCDTOINT( status[7] );
      cdplayer->abs_sec = TWODIGITBCDTOINT( status[8] );
      cdplayer->abs_frame = TWODIGITBCDTOINT( status[9] );
    }
    fillg6cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), T_READDISC, 1, 0, 0, 0,
	       0, 0, 0, 0, 0 );
    filldsreq( cdplayer->dsp, track_info, TRACK_INFO_SIZE,
	       DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );

    if (STATUS(cdplayer->dsp) == STA_GOOD) {
      cdplayer->total_min = TWODIGITBCDTOINT( track_info[0] );
      cdplayer->total_sec = TWODIGITBCDTOINT( track_info[1] );
      cdplayer->total_frame = TWODIGITBCDTOINT( track_info[2] );
    }
  }
  else { /* SCSI-2 && SONY */
#define TOC_INFO_SIZE 12
    unsigned char   *toc_info = (unsigned char *)alloca(TOC_INFO_SIZE);
#define SUB_CHANNEL_SIZE 16
    unsigned char   *sub_channel = (unsigned char *)alloca(SUB_CHANNEL_SIZE);
    int             lead_out;

    fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), S2_SUBCODE, 2, 0x40, 1, 0, 0, 0,
	       B2( SUB_CHANNEL_SIZE ), 0 );
    filldsreq( cdplayer->dsp, sub_channel, SUB_CHANNEL_SIZE,
	       DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );

    if (STATUS( cdplayer->dsp ) != STA_GOOD)
      return (0);

    switch (sub_channel[1]) {

    case 0x11:
      cdplayer->state = CD_PLAYING;
      break;

    case 0x12:
      /*
       * The Sony drive likes to always report that it's in
       * pause mode; therefore, it is necessary to make a
       * distinction between "the drive is in paused mode"
       * and "the user pressed the pause button".  The
       * paused field gets turned on in situations where
       * the user pressed the pause button.
       */
      cdplayer->state = cdplayer->paused ? CD_PAUSED : CD_READY;
      break;

    case 0:
    case 0x13:
    case 0x15:
      cdplayer->state = CD_READY;
      break;

    case 0x14:
    default:
      CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
      cdplayer->state = CD_ERROR;
      break;
    }

    cdplayer->track = sub_channel[6];
    cdplayer->min = sub_channel[13];
    cdplayer->sec = sub_channel[14];
    cdplayer->frame = sub_channel[15];
    cdplayer->abs_min = sub_channel[9];
    cdplayer->abs_sec = sub_channel[10];
    cdplayer->abs_frame = sub_channel[11];

    fills2cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), S2_TOC, 2, 0, 0, 0, 0, 0xaa,
	       B2( TOC_INFO_SIZE ), 0 );
    filldsreq( cdplayer->dsp, toc_info, TOC_INFO_SIZE,
	       DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
    if (STATUS( cdplayer->dsp ) != STA_GOOD)
      return (0);
    if (toc_info[5] & 0x04) {
      cdplayer->state = CD_CDROM;
    } else {
      cdplayer->total_min = toc_info[9];
      cdplayer->total_sec = toc_info[10];
      cdplayer->total_frame = toc_info[11];
    }
  }

  return (1);
}


/*
 *  int CDgetstatus( CDPLAYER *cd, CDSTATUS *status )
 *
 *  Description:
 *      Copy some of the fields from the CDPLAYER structure into the
 *      CDSTATUS structure supplied by the caller.  This allows the caller
 *      see many of the values of CDPLAYER without being able to access
 *      them directly
 *
 *  Parameters:
 *      cd          Pointer to a CDPLAYER struct
 *      status      Pointer to a CDSTATUS struct, to be filled in so caller
 *                  can see the values
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

int CDgetstatus( CDPLAYER *cdplayer, CDSTATUS *status )
{
	CDTRACKINFO info;

	CDupdatestatus( cdplayer );

	status->state = cdplayer->state;
	status->scsi_audio = cdplayer->scsi_audio;
	/*
	 * This is pretty gross, but seems to be required by the behavior
	 * of the Sony drive.
	 *
	 * When you tell the drive to play the beginning of a track and
	 * then immediately pause, it backs off a little bit, so that
	 * when you read subcodes you get data from the last little bit
	 * of the previous track.  This looks pretty ugly when we're trying
	 * to give the user feedback about tracks and time, so if we're
	 * paused at the very end of a track, we report that we're actually
	 * at the beginning of the next track.
	 */
	if (cdplayer->type == SONY && cdplayer->track < cdplayer->last &&
	 CDgettrackinfo( cdplayer, cdplayer->track, &info ) &&
	 cdplayer->min == info.total_min && cdplayer->sec >= info.total_sec - 1) {
		status->track = cdplayer->track + 1;
		status->min = status->sec = status->frame = 0;
	}
	else {
		status->track = cdplayer->track;
		status->min = cdplayer->min;
		status->sec = cdplayer->sec;
		status->frame = cdplayer->frame;
	}
	status->abs_min = cdplayer->abs_min;
	status->abs_sec = cdplayer->abs_sec;
	status->abs_frame = cdplayer->abs_frame;
	status->total_min = cdplayer->total_min;
	status->total_sec = cdplayer->total_sec;
	status->total_frame = cdplayer->total_frame;
	status->first = cdplayer->first;
	status->last = cdplayer->last;
	status->cur_block = cdplayer->cur_block;
	return (1);
}

/*
 *  int
 *  CDgettrackinfo( CDPLAYER *cd, int track, CDTRACKINFO *info )
 *
 *  Description:
 *      Get relevant information about track on cd
 *
 *  Parameters:
 *      cd      The CD to get info about
 *      track   Number of track to get info about
 *      info    Pointer to buffer to receive the information
 *
 *  Returns:
 *      1 if successful, 0 otherwise
 */

int
CDgettrackinfo( CDPLAYER *cd, int track, CDTRACKINFO *info )
{
	register struct dsreq   *dsp = cd->dsp;
	int                     frames, this_track, next_track, changed;
	TRACK                   *trackp;
	
	CDtestready( cd, &changed );

	if (STATUS(cd->dsp) != STA_GOOD)
		return (0);

	if (!cd->toc && !read_toc( cd ))
		return (0);

	if (track < cd->first || track > cd->last)
		return (0);
	
	trackp = &cd->toc[track - cd->first];
	info->start_min = trackp->min;
	info->start_sec = trackp->sec;
	info->start_frame = trackp->frame;

	trackp++;
	frames = ((trackp->min * 60) + trackp->sec) * 75 + trackp->frame;
	frames -= ((info->start_min * 60) + info->start_sec) * 75 +
	 info->start_frame;

	info->total_frame = frames % 75;
	frames /= 75;
	info->total_sec = frames % 60;
	info->total_min = frames / 60;

	return (1);
}

/*
 *  int CDsetvolume( CDPLAYER *cd, CDVOLUME *vol )
 *
 *  Description:
 *      Set the volume of the output
 *
 *  Parameters:
 *      cd      pointer to a CDPLAYER struct
 *      vol     volume structure
 *
 *  Returns:
 *      non-zero iff successful
 */

int CDsetvolume( CDPLAYER *cd, CDVOLUME *vol )
{
	return (0);
}

/*
 *  int CDgetvolume( CDPLAYER *cd, CDVOLUME *vol )
 *
 *  Description:
 *      Get the volume of the output
 *
 *  Parameters:
 *      cd      pointer to a CDPLAYER struct
 *      vol     volume structure
 *
 *  Returns:
 *      non-zero iff successful
 */

int CDgetvolume( CDPLAYER *cd, CDVOLUME *vol )
{
	return (0);
}

/*
 *  static int read_disc( CDPLAYER *cdplayer )
 *
 *  Description:
 *      Read parameters from the CD-ROM drive
 *
 *  Parameters:
 *      cdplayer        pointer to a CDPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

static int read_disc( CDPLAYER *cdplayer )
{
#define TRACK_INFO_SIZE 4
	unsigned char	*trackInfo = (unsigned char *)alloca(TRACK_INFO_SIZE);
#define S2_TRACK_INFO_SIZE 12
	unsigned char	*s2trackInfo =
				(unsigned char *)alloca(S2_TRACK_INFO_SIZE);
	int		i;
	struct dsreq	*dsp = cdplayer->dsp;

	if (cdplayer->type == TOSHIBA) {
		fillg6cmd( dsp, CMDBUF(dsp), T_READDISC, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
		filldsreq( dsp, trackInfo, TRACK_INFO_SIZE,
		 DSRQ_SENSE | DSRQ_READ );
		doscsireq( getfd( dsp ), dsp );
		if (STATUS(dsp) != STA_GOOD)
			return (0);
		if (trackInfo[3] & 0x04) {
		    cdplayer->state = CD_CDROM;
		    return 0;
		} else {
		    cdplayer->first = TWODIGITBCDTOINT( trackInfo[0] );
		    cdplayer->last = TWODIGITBCDTOINT( trackInfo[1] );
		}
	}
	else {
		fillg6cmd( dsp, CMDBUF(dsp), S2_TOC, 0, 0, 0, 0, 0, 1,
		 B2( S2_TRACK_INFO_SIZE ), 0 );
		filldsreq( dsp, s2trackInfo, S2_TRACK_INFO_SIZE,
		 DSRQ_SENSE | DSRQ_READ );
		doscsireq( getfd( dsp ), dsp );
		if (STATUS(dsp) != STA_GOOD)
			return (0);
		if (s2trackInfo[5] & 0x04) {
		    cdplayer->state = CD_CDROM;
		    return 0;
		} else {
		    cdplayer->first = s2trackInfo[2];
		    cdplayer->last = s2trackInfo[3];
		}
	}

	return (1);
}

void CDtestready( CDPLAYER *cd, int* changed )
{
  int             keep_trying;
  register dsreq_t    *dsp = cd->dsp; 

  *changed = 0;

  fillg0cmd( dsp, CMDBUF(dsp), G0_TEST, 0, 0, 0, 0, 0 );
  filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
  doscsireq( getfd( dsp ), dsp );
    
  if (STATUS( dsp ) == STA_CHECK
      && (SENSEBUF( dsp )[2] & 0xf) == 6) {
    *changed = 1;
    if (SENSEBUF( dsp )[12] == 0x29) {
      act_like_a_cdrom( dsp );
    }
  }

  if ((*changed || STATUS( dsp ) != STA_GOOD) && cd->toc) {
    free (cd->toc);
    cd->toc = 0;
  }
}

int CDsbtoa(char *s, const unchar *sb, int count)
{
	int i;

	/*
	 * XXX Error checking ?
	 */
	for (i = 0; i < count; i++)
		*s++ = sbtoa[sb[i]];
	*s = '\0';
	return i;
}

void CDtimetoa(char *s, struct cdtimecode *tp)
{
    s[0] = tp->mhi == 0xA ? '-' : '0' + tp->mhi;
    s[1] = tp->mlo == 0xA ? '-' : '0' + tp->mlo;
    s[3] = tp->shi == 0xA ? '-' : '0' + tp->shi;
    s[4] = tp->slo == 0xA ? '-' : '0' + tp->slo;
    s[6] = tp->fhi == 0xA ? '-' : '0' + tp->fhi;
    s[7] = tp->flo == 0xA ? '-' : '0' + tp->flo;
}

void CDpreventremoval( CDPLAYER *cdplayer )
{
	fillg0cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), G0_PREV, 0, 0, 0, 1, 0 );
	filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
	doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
}

void CDallowremoval( CDPLAYER *cdplayer )
{
	fillg0cmd( cdplayer->dsp, CMDBUF(cdplayer->dsp), G0_PREV, 0, 0, 0, 0, 0 );
	filldsreq( cdplayer->dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
	doscsireq( getfd( cdplayer->dsp ), cdplayer->dsp );
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

static void act_like_a_cdrom( struct dsreq *dsp )
{
	fillsgicmd( dsp, CMDBUF(dsp), SGI_CDROM, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
	doscsireq( getfd( dsp ), dsp );
}

struct toc_header {
	unsigned short  length;
	unsigned char   first;
	unsigned char   last;
};

struct toc_entry {
	unsigned char   reserved;
	unsigned char   control;
	unsigned char   track;
	unsigned char   reserved2;
	unsigned char   reserved3;
	unsigned char   min;
	unsigned char   sec;
	unsigned char   frame;
};

struct toc {
	struct toc_header   head;
	struct toc_entry    tracks[1];
};

#define MAXTOCLEN   (100 * sizeof (struct toc_entry) \
 + sizeof (struct toc_header))

static int
read_toc( CDPLAYER *cd )
{
  dsreq_t       *dsp = cd->dsp;
  unsigned char *track_info = (unsigned char *)alloca(TRACK_INFO_SIZE);
  unsigned char *sony_track_info = (unsigned char *)alloca(MAXTOCLEN);
  struct toc    *tocp;
  int           track, this_track, frames;

  cd->toc = (TRACK *)malloc( sizeof (TRACK) * (cd->last - cd->first + 2) );
  if (!cd->toc)
    return (0);
  if (cd->type == TOSHIBA) {
    this_track = INTTOTWODIGITBCD( track );
    for (track = cd->first; track <= cd->last; track++) {
      this_track = INTTOTWODIGITBCD( track );
      fillg6cmd( dsp, CMDBUF(dsp), T_READDISC, 2, this_track,
		 0, 0, 0, 0, 0, 0, 0 );
      filldsreq( dsp, track_info, TRACK_INFO_SIZE, DSRQ_SENSE |
		 DSRQ_READ );
      doscsireq( getfd( dsp ), dsp );
      if (STATUS(dsp) != STA_GOOD) {
	CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
	cd->state = CD_ERROR;
	free( cd->toc );
	cd->toc = 0;
	return (0);
      }
      if (track_info[3] & 0x04) {
	cd->state = CD_CDROM;
	return 0;
      }
      cd->toc[track - cd->first].min =
	TWODIGITBCDTOINT( track_info[0] );
      cd->toc[track - cd->first].sec =
	TWODIGITBCDTOINT( track_info[1] );
      cd->toc[track - cd->first].frame =
	TWODIGITBCDTOINT( track_info[2] );
    }
    cd->toc[cd->last + 1 - cd->first].min = cd->total_min;
    cd->toc[cd->last + 1 - cd->first].sec = cd->total_sec;
    cd->toc[cd->last + 1 - cd->first].frame = cd->total_frame;
  }
  else { /* SCSI-2 and SONY */
    fills2cmd( dsp, CMDBUF(dsp), S2_TOC, 2, 0, 0, 0, 0, 0,
	       B2( MAXTOCLEN ), 0 );
    filldsreq( dsp, sony_track_info, MAXTOCLEN,
	       DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( dsp ), dsp );
    if (STATUS(dsp) != STA_GOOD) {
      CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
      cd->state = CD_ERROR;
      free( cd->toc );
      cd->toc = 0;
      return (0);
    }
    if (sony_track_info[5] & 0x04) {
      cd->state = CD_CDROM;
      return 0;
    } else {
      tocp = (struct toc *)sony_track_info;
      for (track = cd->first; track <= cd->last + 1; track++) {
	cd->toc[track - cd->first].min =
	  tocp->tracks[track - cd->first].min;
	cd->toc[track - cd->first].sec =
	  tocp->tracks[track - cd->first].sec;
	cd->toc[track - cd->first].frame =
	  tocp->tracks[track - cd->first].frame;
      }
    }
  }
  /*
   * This is because specifying the starting address of the lead-out
   * track as the end-of-play address (for Sony) causes an error; we
   * must back off by one frame.  We do it for Toshiba as well to
   * make the same database work for both.
   */
  frames = cd->toc[cd->last - cd->first + 1].frame + 75 *
    (cd->toc[cd->last - cd->first + 1].sec + 60 *
     (cd->toc[cd->last - cd->first + 1].min));
  frames--;
  cd->toc[cd->last - cd->first + 1].frame = frames % 75;
  frames /= 75;
  cd->toc[cd->last - cd->first + 1].sec = frames % 60;
  cd->toc[cd->last - cd->first + 1].min = frames / 60;
  return (1);
}

static int
set_blksize( CDPLAYER *cd, int blksize )
{
	char                    params[12];
	int                     retries;
	register struct dsreq   *dsp = cd->dsp;


	retries = 10;
	while (retries--) {
		(*modesense[cd->type])( dsp, params, sizeof (params) );

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
		*(int *)&params[8] = blksize;

		modeselect15( dsp, params, sizeof (params), 0, 0 );

		if (STATUS(dsp) == STA_GOOD)
			return (1);
		else if (STATUS(dsp) == STA_CHECK || STATUS(dsp) == STA_RETRY)
			continue;
		else
			return (0);
	}

	return (0);
}

static void
scsi2_modesense( struct dsreq *dsp, char *params, int size )
{
#define SONY_PARAMS_SIZE (MSELPARMLEN + 8)
	char    *sony_params = (char *)alloca(SONY_PARAMS_SIZE);
	char	*databuf;
	int     datalen;

	databuf = DATABUF( dsp );
	datalen = DATALEN( dsp );
	/*
	 * Amazingly enough, modesense fails if you don't request any
	 * page code information
	 */
	fillg0cmd( dsp, CMDBUF( dsp ), G0_MSEN, 0, 1, 0,
	 SONY_PARAMS_SIZE , 0 );
	filldsreq( dsp, sony_params, SONY_PARAMS_SIZE,
	 DSRQ_READ | DSRQ_SENSE );
	doscsireq( getfd( dsp ), dsp );

	if (STATUS( dsp ) == STA_GOOD)
		bcopy( sony_params, params, size );
	
	DATABUF( dsp ) = databuf;
	DATALEN( dsp ) = datalen;
}

static void
toshiba_modesense( struct dsreq *dsp, char *params, int size )
{
	modesense1a( dsp, params, size, 0, 0, 0 );
}
