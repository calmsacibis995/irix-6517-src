/*
|| dslib.c - library routines for /dev/scsi
||
|| Copyright 1988, 1989, by
||   Gene Dronek (Vulcan Laboratory) and
||   Rich Morin  (Canta Forda Computer Laboratory).
|| All rights reserved.
*/
#ident	"dslib.c: $Revision: 1.21 $"

#include <stdio.h>
#include <sys/types.h>

#include "dslib.h"
#ifdef aux
#include <sys/vio.h>
#include <sys/scsireq.h>
#endif
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <errno.h>

/* local forward declarations */

struct opttab {
	char	*opt;
	int	*var;
};
int *opttovar(char *ostr, struct opttab *table);


int dsdebug=0;
uint dsreqflags;	/* flag bits always set by filldsreq */
int ds_default_timeout = 10;  /* timeout (in seconds) used by dsfillreq();
	may be changed globally, or set in individual functions after 
	dsfillreq() is called for specific commands. */

#define min(i,j)  ( (i) < (j) ? (i) : (j) )


/*
|| Startup/shutdown -----------------------------------------------
*/


/*
|| dsopen - open device, set up structures
*/

struct dsreq *
dsopen(const char *opath, uint oflags)
{
    
  struct dsreq *dsp;
  struct context *cp;
  int fd;
  DSDBG(fprintf(stderr,"dsopen(%s,%x) ", opath, oflags));

  fd = open(opath, oflags);
  if (fd < 0)						
    return NULL;  			/* can't open	*/
  cp = (struct context *) calloc(1, sizeof(struct context));
  if (cp == NULL)				      /* can't allocate	*/
    return NULL;
  cp->dsc_fd = fd;
  dsp = &(cp->dsc_dsreq);

  dsp->ds_flags =	0;
  dsp->ds_time =	ds_default_timeout * 1000; /* only matters if
	callers never set it themselves, and never call filldsreq() */
  dsp->ds_private =	(__psint_t) cp;	/* pointer back to context */
  dsp->ds_cmdbuf = 	cp->dsc_cmd;
  dsp->ds_cmdlen = 	sizeof cp->dsc_cmd;
  dsp->ds_databuf = 	0;
  dsp->ds_datalen = 	0;
  dsp->ds_sensebuf =	cp->dsc_sense;
  dsp->ds_senselen = 	sizeof cp->dsc_sense;
  DSDBG(fprintf(stderr,"=>cp %p, dsp %p\n", cp, dsp));
  return dsp;
}


/*
|| dsclose - close device, release context struct.
*/

void
dsclose(struct dsreq *dsp)
{
  if(dsp && (struct context *)dsp->ds_private) {
      (void)close(getfd(dsp));
      free((void *)dsp->ds_private);
      dsp->ds_private = 0;
  }
}


/*
|| Generic SCSI CCS Command functions ------------------------------------
||
|| dsp		dsreq pointer
|| data		data buffer pointer
|| datalen	data buffer length
|| lba		logical block address
|| vu		vendor unique bits
*/

/*
|| testunitready00 - issue group 0 "Test Unit Ready" command (0x00)
*/

int
testunitready00(struct dsreq *dsp)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_TEST, 0, 0, 0, 0, 0);
  filldsreq(dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}

/*
|| rewind01- issue group 0 "rewind/rezero" command (0x01)
*/
int
rewind01(struct dsreq *dsp)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_REWI, 0, 0, 0, 0, 0);
  filldsreq(dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 240;
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| requestsense03 - issue group 0 "Request Sense" command (0x03)
*/

int
requestsense03(struct dsreq *dsp, caddr_t data, uint datalen, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_REQU, 0, 0, 0, B1(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ);
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| read08 - issue group 0 "Read" command (0x08)
Note that this is the form for disks; tapes typically
have 'B3(datalen)', instead of 'B2(lba), B1(datalen)'
Other devices may have yet other forms of the basic
read and write scsi cmds!
Also note that many devices want the scsi cmd to have
the number of blocks, while the ds driver (filldsreq) needs
the number of bytes.  For that reason, this function will generally
not be able to be used as is; you may need to modify it such that
it takes a block count, and the datalen passed to filldsreq is
multiplied by the block count (probably also passed as an argument).
See also readextended28().

You might also want to increase the timeout proportional to the
amount of data read, especially for slow devices.
*/

int
read08(struct dsreq *dsp, caddr_t data, uint datalen, uint lba, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_READ, 0, B2(lba), B1(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time	= 30 * 1000;	/* default to 30 seconds */
  /* DATALEN(dsp)	= datalen * your_device_blocksize; */
  return(doscsireq(getfd(dsp), dsp));
}

/*
|| write0a - issue group 0 "Write" command (0x0a)
Note that this is the form for disks; tapes typically
have 'B3(datalen)', instead of 'B2(lba), B1(datalen)'
Other devices may have yet other forms of the basic
read and write scsi cmds!
Also note that many devices want the scsi cmd to have
the number of blocks, while the ds driver (filldsreq) needs
the number of bytes.  For that reason, this function will generally
not be able to be used as is; you may need to modify it such that
it takes a block count, and the datalen passed to filldsreq is
multiplied by the block count (probably also passed as an argument).
See also writeextended2a().

You might also want to increase the timeout proportional to the
amount of data read, especially for slow devices.
*/

int
write0a(struct dsreq *dsp, caddr_t data, uint datalen, uint lba, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_WRIT, 0, B2(lba), B1(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_WRITE|DSRQ_SENSE);
  dsp->ds_time	= 30 * 1000;	/* default to 30 seconds */
  /* DATALEN(dsp)	= datalen * your_device_blocksize; */
  return(doscsireq(getfd(dsp), dsp));
}

/*
|| inquiry12 - issue group 0 "Inquiry" command (0x12)
*/

int
inquiry12(struct dsreq *dsp, caddr_t data, uint datalen, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_INQU, 0, 0, 0, B1(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| modeselect15 - issue group 0 "Mode Select" command (0x15)
||
|| save		0 - don't save saveable pages
|| 		1 - save saveable pages
|| you may also need to OR 0x10 with byte 1 (same byte as save)
|| to indicate that you comply with the SCSI 2 spec as to the
|| format of the data.
*/

int
modeselect15(struct dsreq *dsp, caddr_t data, uint datalen, int save, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_MSEL, save&0x11, 0, 0, B1(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_WRITE|DSRQ_SENSE);
  dsp->ds_time = 1000 * 30;
  return(doscsireq(getfd(dsp), dsp));
}

/*
|| reservunit16 - issue group 0 "Reserve" command (0x16)
*/

int
reservunit16(struct dsreq *dsp, caddr_t data, uint datalen, int tpr,
	     int tpdid, int extent, int res_id, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_RESU, ((tpr&1)<<4) | ((tpdid&7)<<1) | (extent&1), res_id, B2(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_WRITE|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}

/*
|| releaseunit17 - issue group 0 "releaseunit" command (0x17)
*/

int
releaseunit17(struct dsreq *dsp, int tpr, int tpdid, int extent,
	      int res_id, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_RELU, ((tpr&1)<<4) | ((tpdid&7)<<1) | (extent&1), res_id, 0, 0, B1(vu<<6));
  filldsreq(dsp, 0, 0, DSRQ_READ|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}

/*
|| modesense1a - issue group 0 "Mode Sense" command (0x1a)
||
|| pagectrl	0 - current values
||		1 - changeable values
||		2 - default values
||		3 - saved values
||
|| pagecode	0   - vendor unique
||		1   - error recovery
||		2   - disconnect/reconnect
||		3   - direct access dev. fmt.
||		4   - rigid disk geometry
||		5   - flexible disk
||		6-9 - see specific dev. types
||		0a  - implemented options
||		0b  - medium types supported
||		3f  - return all pages
*/

int
modesense1a(struct dsreq *dsp, caddr_t data, uint datalen,
	    int pagectrl, int pagecode, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_MSEN, 0,
	  ((pagectrl&3)<<6) | (pagecode&0x3F), 0, B1(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 30;
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| senddiagnostic1d - issue group 0 "Send Diagnostic" command (0x1d)
||
|| self		0 - run test, hold results
||		1 - run test, return status
||
|| dofl		0 - device online
||		1 - device offline
||
|| uofl		0 - unit online
||		1 - unit offline
*/

int
senddiagnostic1d(struct dsreq *dsp, caddr_t data, uint datalen,
		 int self, int dofl, int uofl, int vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_SD,
    (self&1)<<2 | (dofl&1)<<1 | (uofl&1),
    0, B2(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE);
  dsp->ds_time = 1000 * 120; /* 120 seconds; devices taking a longer and
	* longer on this one... */
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| readcapacity25 - issue group 1 "Read Capacity" command (0x25)
||
|| pmi		0 - return last logical block, entire unit
||		1 - return last logical block, current track
*/

int
readcapacity25(struct dsreq *dsp, caddr_t data, uint datalen, uint lba,
	       int pmi, int vu)
{
  fillg1cmd(dsp, (uchar_t*)CMDBUF(dsp), G1_RCAP, 0, B4(lba), 0, 0, pmi&1, B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE
    /* |DSRQ_CTRL2 */ );
  dsp->ds_time = 1000 * 30;
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| readextended28 - issue group 1 "Read Extended" command (0x28)
*  IMPORTANT: read the comments at read08, they apply to this function
*  also.
*/

int
readextended28(struct dsreq *dsp, caddr_t data, uint datalen, 
	       uint lba, int vu)
{
  fillg1cmd(dsp, (uchar_t*)CMDBUF(dsp), G1_READ, 0, B4(lba), 0, B2(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_READ|DSRQ_SENSE
    /* |DSRQ_CTRL2 */ );
  dsp->ds_time = 1000 * 30;
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| writeextended2a - issue group 1 "Write Extended" command (0x2a)
*  IMPORTANT: read the comments at write0a, they apply to this function
*  also.
*/

int
writeextended2a(struct dsreq *dsp, caddr_t data, uint datalen,
		uint lba, int vu)
{
  fillg1cmd(dsp, (uchar_t*)CMDBUF(dsp), G1_WRIT, 0, B4(lba), 0, B2(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t *)data, datalen, DSRQ_WRITE|DSRQ_SENSE
    /* |DSRQ_CTRL2 */ );
  dsp->ds_time = 1000 * 30;
  return(doscsireq(getfd(dsp), dsp));
}


/*
|| Support functions ----------------------------------------------------
*/

/*
|| fillg0cmd - Fill a Group 0 (actually, any 6 byte) command buffer
*/

void
fillg0cmd(struct dsreq *dsp, uchar_t *cmd, uchar_t b0, uchar_t b1,
	uchar_t b2, uchar_t b3, uchar_t b4, uchar_t b5)
{
  uchar_t *c = cmd;
  DSDBG(fprintf(stderr,"fillg0cmd(%p,%p, %02x %02x %02x %02x %02x %02x)\n",
		dsp, cmd, b0,b1,b2,b3,b4,b5));
  *c++ = b0, *c++ = b1, *c++ = b2, *c++ = b3, *c++ = b4, *c++ = b5;
	
  CMDBUF(dsp) = (caddr_t) cmd;
  CMDLEN(dsp) = 6;
}


/*
|| fillg1cmd - Fill a Group 1 (actually, any 10 byte) command buffer
*/

void
fillg1cmd(struct dsreq *dsp, uchar_t *cmd, uchar_t b0, uchar_t b1,
	uchar_t b2, uchar_t b3, uchar_t b4, uchar_t b5, uchar_t b6,
	uchar_t b7, uchar_t b8, uchar_t b9)
{
  uchar_t *c = cmd;
  DSDBG(fprintf(stderr,
    "fillg1cmd(%p,%p, %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x)\n",
		dsp, cmd, b0,b1,b2,b3,b4,b5,b6,b7,b8,b9));

  *c++ = b0, *c++ = b1, *c++ = b2, *c++ = b3, *c++ = b4, *c++ = b5;
  *c++ = b6, *c++ = b7, *c++ = b8, *c++ = b9;
	
  CMDBUF(dsp) = (caddr_t) cmd;
  CMDLEN(dsp) = 10;
}



/*
|| filldsreq - Fill a dsreq structure
*/

void
filldsreq(struct dsreq *dsp, uchar_t *data, uint datalen, uint flags)
{
  DSDBG(fprintf(stderr,"filldsreq(%p,%p,%d,%x) cmdlen %d\n",
		dsp,data,datalen,flags,CMDLEN(dsp)));
  dsp->ds_flags	= flags | dsreqflags |
	  (((dsdebug&1) ? DSRQ_TRACE : 0) |
	  ((dsdebug&2) ? DSRQ_PRINT : 0));
  dsp->ds_time	= ds_default_timeout * 1000;
  dsp->ds_link	= 0;
  dsp->ds_synch	= 0;
  dsp->ds_ret  	= 0;

  DATABUF(dsp) 	= (caddr_t) data;
  DATALEN(dsp)	= datalen;
}


/*
|| bprint - print array of bytes, in hex.
*/

#define hex(x) "0123456789ABCDEF" [ (x) & 0xF ]

static void
bprint(caddr_t sarg, int n, int nperline, int space)
{
	int   i, x;
	char  *sp = (space) ? " ": "";
	unsigned char *s = (unsigned char *)sarg;

	for(i=0;i<n;i++)  {
		x = s[i];
		fprintf(stderr,((i%4==3)?"%c%c%s%s":"%c%c%s"),
			hex(x>>4), hex(x), sp, sp);
		if ( i%nperline == (nperline - 1) )
			fprintf(stderr,"\n");
	}
	if ( space )
		fprintf(stderr,"\n");
}


/* used primarily when you do not want to have dsdebug!=0, but want to
 * print most of the "useful" info after an error occurs.  Used basicly
 * as:
 *		if(a_devscsi_cmd(dsp))
 *			ds_showcmd(dsp);
 * Not called from any of the normal code, intended for the use of
 * applications programs.
*/
void
ds_showcmd(dsreq_t *dsp)
{
	caddr_t sp = SENSEBUF(dsp);

	if (SENSESENT(dsp)) {	/* decode sense data returned */
		fprintf(stderr, "sense key %x - %s\n", SENSEKEY(sp),
			ds_ctostr( (uint)SENSEKEY(sp), sensekeytab));
		bprint(SENSEBUF(dsp), SENSESENT(dsp), 16, 1);
	}
	fprintf(stderr, "SCSI status=%x, devscsi return=%x, timeout=%d", STATUS(dsp),
		RET(dsp), TIME(dsp));
	if(RET(dsp))
		fprintf(stderr, " (%s)", ds_vtostr((uint)RET(dsp),dsrtnametab));
	fprintf(stderr, "\n");
	fprintf(stderr, "datalen=%x, datasent=%x\n", DATALEN(dsp),
		DATASENT(dsp));
	fprintf(stderr,"Command was: %s: ",
		ds_vtostr((uint)(CMDBUF(dsp))[0], cmdnametab));
	bprint(CMDBUF(dsp),CMDLEN(dsp),16,1);
}

/*
|| doscsireq - issue scsi command, return status or -1 error.
*/

int
doscsireq( int fd, struct dsreq *dsp)
  /* fd -  ioctl file descriptor */
  /* dsp - devscsi request packet */
{
  int	cc;
  int	retries = 4;
  uchar_t	sbyte;

  DSDBG(fprintf(stderr,"doscsireq(%d,%p) %x ---- %s\n",fd,dsp,
    (CMDBUF(dsp))[0],
    ds_vtostr((uint)(CMDBUF(dsp))[0], cmdnametab)));

  /*
   *  loop, issuing command
   *    until done, or further retry pointless
   */

  while ( --retries > 0 )  {

   caddr_t sp;

    sp =  SENSEBUF(dsp);
	if(DATALEN(dsp))
		DSDBG(fprintf(stderr,"data xfer %s, ",  dsp->ds_flags&DSRQ_READ ?
			"in" : "out"));
    DSDBG(fprintf(stderr,"cmdbuf   =  ");
		bprint(CMDBUF(dsp),CMDLEN(dsp),16,1));
    if ( (dsp->ds_flags & DSRQ_WRITE) )
      DSDBG(bprint(DATABUF(dsp), min(50,DATALEN(dsp)),16,1 ));
  	
DSDBG(fprintf(stderr,"databuf datalen %p %d\n",DATABUF(dsp), DATALEN(dsp)));
    cc = ioctl( fd, DS_ENTER, dsp);
    if ( cc < 0)  {
        RET(dsp) = DSRT_DEVSCSI;
        return -1;
    }
  	
	DSDBG(fprintf(stderr,"cmdlen after ioctl=%d\n",CMDLEN(dsp)));
    DSDBG(fprintf(stderr,"ioctl=%d ret=%x %s",
      cc, RET(dsp), 
      RET(dsp) ? ds_vtostr((uint)RET(dsp),dsrtnametab) : ""));
    DSDBG(if (SENSESENT(dsp)) fprintf(stderr," sensesent=%d",
      SENSESENT(dsp)));

    DSDBG(fprintf(stderr,
      " cmdsent=%d datasent=%d sbyte=%x:%s timeout=%d\n",
      CMDSENT(dsp), DATASENT(dsp), STATUS(dsp),
      ds_vtostr((uint)STATUS(dsp), cmdstatustab), TIME(dsp)));
    DSDBG(if ( FLAGS(dsp) & DSRQ_READ )
      bprint(DATABUF(dsp), min(16*16,DATASENT(dsp)), 16,1));

#ifdef aux
  /*
   *  check for AUX bus-error 
   *  we retry with poll-dma
   */
    if ( RET(dsp) == DSRT_AGAIN )  {
      int n = SDC_RDPOLL|SDC_WRPOLL;
      DSDBG(fprintf(stderr,"setting rd/wr-poll"));
      cc = ioctl( fd, DS_SET, n);	/* set bits */
      if ( cc != 0 )
        return -1;
    }
#endif

    if ( RET(dsp) == DSRT_NOSEL )
      continue;		/* retry noselect 3X */

    /* decode sense data returned */
    if ( SENSESENT(dsp) )  {
      DSDBG(
        fprintf(stderr, "sense key %x - %s\n",
          SENSEKEY(sp),
          ds_ctostr( (uint)SENSEKEY(sp), sensekeytab));
        bprint( SENSEBUF(dsp),
          min(100, SENSESENT(dsp)),
          16,1);
      );
    }
    DSDBG(fprintf(stderr, "sbyte %x\n", STATUS(dsp)));

    /* decode scsi command status byte */
    sbyte = STATUS(dsp);
    switch (sbyte)  {
      case 0x08:		/*  BUSY */
      case 0x18:		/*  RESERV CONFLICT */
    	sleep(2);
    	continue;
      case 0x00:		/*  GOOD */
      case 0x02:		/*  CHECK CONDITION */
      case 0x10:		/*  INTERM/GOOD */
      default:
    	return sbyte;
    }
  }
  return -1;	/* fail retry limit */
}


/*
|| opttovar - lookup option in table, return var addr (NULL if fail)
*/

int *
opttovar(char *ostr, struct opttab *table)
{
  register struct opttab *tp;

  for (tp=table; (tp->var); tp++)
    if ( strncmp( ostr, tp->opt, 3) == 0 )
      break;

  if ( !tp->var )
    fprintf(stderr,"unknown option %s", ostr);
	
  return (tp->var);
}


/*
|| ds_vtostr - lookup value in vtab table to return string pointer
*/
char *
ds_vtostr(uint v, struct vtab *table)
{
  register struct vtab *tp;

  for (tp=table; (tp->string); tp++)
    if ( v == tp->val )
      break;
	
  return (tp->string) ? tp->string : "";
}


/*
|| ds_ctostr - lookup value in ctab table to return string pointer
*/

char *
ds_ctostr(uint v, struct ctab *table)
{
  register struct ctab *tp;

  for (tp=table; (tp->string); tp++)
    if ( v == tp->val )
      break;
	
  return (tp->string) ? tp->string : "";
}
