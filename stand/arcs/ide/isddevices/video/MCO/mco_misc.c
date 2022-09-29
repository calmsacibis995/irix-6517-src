/*
 *	misc - 
 *		Miscellaneous stuff that does not rely on the gl.
 *
 *			    Paul Haeberli - 1988
 */
#include "stdio.h"
#include "sys/types.h"
/*
#include "sys/times.h"
 */
#include "sys/param.h"
#include <arcs/io.h>
#include "ide_msg.h"
#include "mco_diag.h"

strtolower(str)
register char *str;
{
    while(*str) {
	*str = tolower(*str);
	str++;
    }
}

ctos(cptr,sptr,n)
register unsigned char *cptr;
register unsigned short *sptr;
register int n;
{
    while(n--) {
	if(n>=8) {
	    sptr[0] = cptr[0];
	    sptr[1] = cptr[1];
	    sptr[2] = cptr[2];
	    sptr[3] = cptr[3];
	    sptr[4] = cptr[4];
	    sptr[5] = cptr[5];
	    sptr[6] = cptr[6];
	    sptr[7] = cptr[7];
	    sptr+=8; 
	    cptr+=8;
	    n -= 7;
	} else {
	    *sptr++ = *cptr++;
	}
    }
}

stoc(sptr,cptr,n)
register unsigned short *sptr;
register unsigned char *cptr;
register int n;
{
    while(n--) {
	if(n>=8) {
	    cptr[0] = sptr[0];
	    cptr[1] = sptr[1];
	    cptr[2] = sptr[2];
	    cptr[3] = sptr[3];
	    cptr[4] = sptr[4];
	    cptr[5] = sptr[5];
	    cptr[6] = sptr[6];
	    cptr[7] = sptr[7];
	    sptr+=8; 
	    cptr+=8;
	    n -= 7;
	} else {
	    *cptr++ = *sptr++;
	}
    }
}

off_t ipaste_lseek(int filedesc, 
		unsigned char *ip_dataptr,   /* pointer to ipaste data */
		off_t offset, 		     /* # of bytes to offset */
		int whence) 		     /* 0 => absolute offset,
					      * 1 => current ptr + offset
					      */
{
    switch (whence) {
	case SeekAbsolute: 
		ip_dataptr = &ipaste_data[offset];
		break;
	case SeekRelative: 
		ip_dataptr += offset;
		break;
	default:
		msg_printf(VRB, "ipaste_lseek: ERROR - illegal whence parameter (%d)\n", whence);
    }

}

#ifdef NOTYET
delay(secs) 
float secs;
{
    int ticks;
    unsigned int total_us;

    if(secs>=0) {
        ticks = (secs*HZ)+0.5;
#ifdef ORIG
	sginap(ticks);
#endif /* ORIG */
	total_us = (unsigned int)(ticks * US_PER_SEC)/HZ;
	us_delay(total_us);
    }
}

unsigned long getltime()
{
    struct tms ct;

    return times(&ct);
}

unsigned long waittill(t)
unsigned long t;
{
    unsigned long curt;

    while(1) {
	curt = getltime();
	if(curt>t)
	    return curt;
    }
}

static long stime;
static float inittime; 
static int firsted;

float uptime()
{
    if(stime == 0)
	stime = getltime();
    return (getltime()-stime)/((float)HZ);
}

cleartime()
{
    stime = 0;
    inittime = uptime();
    firsted = 1;
}

float gettime()
{
    if(!firsted) {
	cleartime();
	firsted = 1;
    }
    return uptime()-inittime;
}

timefunc(f,n)
int (*f)();
int n;
{
    float t;
    int i;

    cleartime();
    for(i=0; i<n; i++)
	(f)();
    t = gettime();
    fprintf(stderr,"timefunc: %f seconds per call\n",t/n);
}

/*
 *	tpercent - 
 *		Make a row of dots that show percent done
 *
 */
static int started = 0;
static int pos;
static FILE *outf;

#define NDOTS	66

tpercentfile(f)
FILE *f;
{
    outf = f;
}

tpercentdone(p)
float p;
{
    int newpos;
    FILE *outfile;

    if(!outf)
	outfile = stderr;
    else
	outfile = outf;
    p = p/100.0;
    if(!started && p <= 0.01) {
	fprintf(outfile,"working: [");
	fflush(outfile);
	started = 1;
	pos = 0;
	return;
    }
    if(started) {
	if(p<0.999) 
	    newpos = NDOTS*p;
	else
	    newpos = NDOTS;
        if(newpos>pos) {
	    while(pos<newpos) {
	        fprintf(outfile,".");
		pos++;
	    }
	    fflush(outfile);
        }
	if(p>0.999) {
	    fprintf(outfile,"]\n");
	    fflush(outfile);
	    started = 0;
	}
    }
}

int mnoise, maxmalloc;

mallocnoise(n)
int n;
{
    mnoise = n;
}

malloclimit(n)
int n;
{
    maxmalloc = n;
}

unsigned char *mymalloc(n)
int n;
{
    unsigned char *cptr;

    if(maxmalloc && n>maxmalloc) {
	 fprintf(stderr,"attemt to malloc %d bytes max is %d\n",n,maxmalloc);
	 exit(1);
    }
    cptr = (unsigned char *)malloc(n);
    if(mnoise) 
	fprintf(stderr,"malloc of %d bytes\n",n);
    if(!cptr) {
	 fprintf(stderr,"malloc can't get %d bytes\n",n);
	 exit(1);
    }
    return cptr;
}
#endif NOTYET
