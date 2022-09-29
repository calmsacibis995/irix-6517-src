/*
|| dslib.c - library routines for /dev/scsi
||
|| Copyright 1988, 1989, by
||   Gene Dronek (Vulcan Laboratory) and
||   Rich Morin  (Canta Forda Computer Laboratory).
|| All rights reserved.
*/
#ident	"dslib.c: $Revision: 1.1 $"

#include <stdio.h>
#include <sys/types.h>

#include "dslib.h"
#ifdef aux
#include <sys/vio.h>
#include <sys/scsireq.h>
#endif
#include <stdarg.h>

/* local forward declarations */

void bprint(caddr_t s, int n, int nperline, int space);
struct opttab {
	char	*opt;
	int	*var;
};
int *opttovar(char *ostr, struct opttab *table);
void ds_panic(char *fmt, ...);
void ds_zot(char *message);


int dsdebug=0;
long dsreqflags = 0;	/* flag bits always set by filldsreq */

#define min(i,j)  ( (i) < (j) ? (i) : (j) )


/*
|| Startup/shutdown -----------------------------------------------
*/

static struct context *dsc[FDSIZ];


/*
|| dsopen - open device, set up structures
*/

struct dsreq *
dsopen(const char *opath, long oflags)
{
    
  struct dsreq *dsp;
  struct context *cp;
  int fd;
  DSDBG(fprintf(stderr,"dsopen(%s,%x) ", opath, oflags));

  fd = open(opath, oflags);
  if (fd < 0)						
    return NULL;  			/* can't open	*/
  if (dsc[fd] != NULL)		        /* already in use */
    ds_zot("dsopen: fd already in use");

  cp = (struct context *) calloc(1, sizeof(struct context));
  if (cp == NULL)				      /* can't allocate	*/
    ds_zot("dsopen: can't allocate space");
  dsc[fd] = cp;
  cp->dsc_fd = fd;
  dsp = &(cp->dsc_dsreq);

  dsp->ds_flags =	0;
  dsp->ds_time =	120 * 1000;	/* 120 second default timeout */
  dsp->ds_private =	(ulong) cp;	/* pointer back to context */
  dsp->ds_cmdbuf = 	cp->dsc_cmd;
  dsp->ds_cmdlen = 	sizeof cp->dsc_cmd;
  dsp->ds_databuf = 	0;
  dsp->ds_datalen = 	0;
  dsp->ds_sensebuf =	cp->dsc_sense;
  dsp->ds_senselen = 	sizeof cp->dsc_sense;
  DSDBG(fprintf(stderr,"=>cp %x, dsp %x\n", cp, dsp));
  return dsp;
}


/*
|| dsclose - close device, release context struct.
*/

void
dsclose(dsp)
  struct dsreq *dsp;
{
  int fd;
  struct context *cp;

  if (dsp == NULL)
    ds_zot("dsclose: dsp is NULL");

  cp = (struct context *)dsp->ds_private;
  fd = getfd(dsp);
  (void)close(fd);
  if ( cp == NULL )
    ds_zot("dsclose: private is NULL");

  cfree(cp);
  dsc[fd] = (struct context *)NULL;
  return;
}

/*
|| Support functions ----------------------------------------------------
*/

/*
|| fillg0cmd - Fill a Group 0 (actually, any 6 byte) command buffer
*/

void
fillg0cmd(struct dsreq *dsp, caddr_t cmd, uchar_t b0, uchar_t b1,
	uchar_t b2, uchar_t b3, uchar_t b4, uchar_t b5)
{
  caddr_t c = cmd;
  DSDBG(fprintf(stderr,"fillg0cmd(%x,%x, %02x %02x %02x %02x %02x %02x)\n",
		dsp, cmd, b0,b1,b2,b3,b4,b5));
  *c++ = b0, *c++ = b1, *c++ = b2, *c++ = b3, *c++ = b4, *c++ = b5;
	
  CMDBUF(dsp) = (caddr_t) cmd;
  CMDLEN(dsp) = 6;
}


/*
|| fillg1cmd - Fill a Group 1 (actually, any 10 byte) command buffer
*/

void
fillg1cmd(struct dsreq *dsp, caddr_t cmd, uchar_t b0, uchar_t b1,
	uchar_t b2, uchar_t b3, uchar_t b4, uchar_t b5, uchar_t b6,
	uchar_t b7, uchar_t b8, uchar_t b9)
{
  caddr_t c = cmd;
  DSDBG(fprintf(stderr,
    "fillg1cmd(%x,%x, %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x)\n",
		dsp, cmd, b0,b1,b2,b3,b4,b5,b6,b7,b8,b9));

  *c++ = b0, *c++ = b1, *c++ = b2, *c++ = b3, *c++ = b4, *c++ = b5;
  *c++ = b6, *c++ = b7, *c++ = b8, *c++ = b9;
	
  CMDBUF(dsp) = (caddr_t) cmd;
  CMDLEN(dsp) = 10;
}

/*
|| fillg2cmd - Fill a Group 2 (actually, any 12 byte) command buffer
*/

void
fillg2cmd(struct dsreq *dsp, caddr_t cmd, uchar_t b0, uchar_t b1,
	uchar_t b2, uchar_t b3, uchar_t b4, uchar_t b5, uchar_t b6,
	uchar_t b7, uchar_t b8, uchar_t b9, uchar_t b10, uchar_t b11 )
{
  caddr_t c = cmd;
  DSDBG(fprintf(stderr,
    "fillg2cmd(%x,%x, %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x)\n",
		dsp, cmd, b0,b1,b2,b3,b4,b5,b6,b7,b8,b9,b10,b11));

  *c++ = b0, *c++ = b1, *c++ = b2, *c++ = b3, *c++ = b4, *c++ = b5;
  *c++ = b6, *c++ = b7, *c++ = b8, *c++ = b9, *c++ = b10, *c++ = b11;
	
  CMDBUF(dsp) = (caddr_t) cmd;
  CMDLEN(dsp) = 12;
}



/*
|| filldsreq - Fill a dsreq structure
*/

void
filldsreq(struct dsreq *dsp, uchar_t *data, long datalen, long flags)
{
  DSDBG(fprintf(stderr,"filldsreq(%x,%x,%d,%x) cmdlen %d\n",
		dsp,data,datalen,flags,CMDLEN(dsp)));
  dsp->ds_flags	= flags | dsreqflags |
	  (((dsdebug&1) ? DSRQ_TRACE : 0) |
	  ((dsdebug&2) ? DSRQ_PRINT : 0));
  dsp->ds_time	= 120 * 1000;	/* default to 120 seconds */
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

void
bprint(caddr_t s, int n, int nperline, int space)
{
	int   i, x;
	char  *sp = (space) ? " ": "";

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

  DSDBG(fprintf(stderr,"doscsireq(%d,%x) %x ---- %s\n",fd,dsp,
    (CMDBUF(dsp))[0],
    ds_vtostr( (unsigned long)(CMDBUF(dsp))[0], cmdnametab)));

  /*
   *  loop, issuing command
   *    until done, or further retry pointless
   */

  while ( --retries > 0 )  {

   caddr_t sp;

    sp =  SENSEBUF(dsp);
    DSDBG(fprintf(stderr,"cmdbuf   =  ");
		bprint(CMDBUF(dsp),CMDLEN(dsp),16,1));
    if ( (dsp->ds_flags & DSRQ_WRITE) )
      DSDBG(bprint( DATABUF(dsp), min(256, DATALEN(dsp)),16,1 ));
  	
DSDBG(fprintf(stderr,"databuf datalen %x %d\n",DATABUF(dsp), DATALEN(dsp)));
    cc = ioctl( fd, DS_ENTER, dsp);
    if ( cc < 0)  {
        RET(dsp) = DSRT_DEVSCSI;
        return -1;
    }
  	
	DSDBG(fprintf(stderr,"cmdlen after ioctl=%d\n",CMDLEN(dsp)));
    DSDBG(fprintf(stderr,"ioctl=%d ret=%x %s",
      cc, RET(dsp), 
      RET(dsp) ? ds_vtostr((unsigned long)RET(dsp),dsrtnametab) : ""));
    DSDBG(if (SENSESENT(dsp)) fprintf(stderr," sensesent=%d",
      SENSESENT(dsp)));

    DSDBG(fprintf(stderr,
      " cmdsent=%d datasent=%d sbyte=%x %s\n",
      CMDSENT(dsp), DATASENT(dsp), STATUS(dsp),
      ds_vtostr((unsigned long)STATUS(dsp), cmdstatustab)));
    DSDBG(if ( FLAGS(dsp) & DSRQ_READ )
      bprint( DATABUF(dsp), min(256, DATASENT(dsp)), 16,1));

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
          ds_ctostr( (unsigned long)SENSEKEY(sp), sensekeytab));
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
opttovar( ostr, table)
  char *ostr;
  struct opttab *table;
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
ds_vtostr(unsigned long v, struct vtab *table)
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
ds_ctostr(unsigned long v, struct ctab *table)
{
  register struct ctab *tp;

  for (tp=table; (tp->string); tp++)
    if ( v == tp->val )
      break;
	
  return (tp->string) ? tp->string : "";
}


/*
|| ds_panic - yelp, leave...
*/

void
ds_panic(char *fmt, ...)
{
  va_list ap;
  extern errno;

  va_start(ap, fmt);

  vfprintf(stderr, fmt, ap);
  fprintf(stderr,"\nerrno = %d\n",errno);
  va_end(ap);
  exit(1);
}


/*
|| ds_zot - go away, with a message.
*/

void
ds_zot(message)
  char *message;
{
  fprintf(stderr, "%s\n", message);
  exit(1);
}
