# include "fx.h"

extern MENU db_menu;
static struct debug
{
    daddr_t doff;
    int count;
    uint bufsize;
    unchar *buf;
    daddr_t nblocks;
    uint bo, lastbo;
    int itype;
    union { daddr_t l; ushort s; unchar *c; } j;
} D;

/* a 'large' value */
#define MAXDB_blocks 1024
#define MAXDB_bytes (MAXDB_blocks * DP(&vh)->dp_secbytes)

static int charline(unchar *src, int len, unchar *tgt, int n);
static int shortline(ushort *src, int len, unchar *tgt, int n);
static int longline(uint *src, int len, unchar *tgt, int n);
static int longlongline(uint64_t *src, int len, unchar *tgt, int n);
static void prdata(int offset, void *src, int len, int isize);
static void spyline(unchar *src, int len, unchar *tgt, int n);

/*
 * initialize debug globals
 */
void
init_db(void)
{
    bzero(&D, sizeof D);
}

/* check to see if buffer allocated and large enough; re alloc
 * larger if needed, alloc on first use.  Want the buffer to
 * hang around if the same size, for editting, etc.
*/
static int
dbgetbuf(void)
{
	if(!D.j.c || D.lastbo > D.bufsize) {
		if(D.j.c) free(D.j.c);
    	D.bufsize = D.lastbo;
		D.buf = D.j.c = malloc(D.bufsize);
		if(D.buf) bzero(D.buf, D.bufsize);
	}
	return D.buf ? 0 : 1;
}


/*
 * menu item to interactively set the read/write offset
 */
void
seek_func(void)
{
    daddr_t l, lastbn;

    lastbn = DP(&vh)->dp_drivecap;
    argbn(&l, D.doff, lastbn, "blocknum");
    argcheck();
    D.doff = l;
}
/*
 * menu item to read from disk into buf
 */
void
readbuf_func(void)
{
    register daddr_t maxblocks;

    argnum(&D.bo, D.bo, MAXDB_bytes, "buf offset");
    maxblocks = MAXDB_blocks;
    if( D.nblocks <= 0 )
	D.nblocks = 1;
    if( D.nblocks > maxblocks )
	D.nblocks = maxblocks;
    argbn(&D.nblocks, D.nblocks, maxblocks+1, "nblocks");
    argcheck();
    D.lastbo = D.bo + stob(D.nblocks);
    if( D.count < D.lastbo )
	D.count = D.lastbo;
	if(dbgetbuf())
		return;
    if( gread(D.doff, D.buf+D.bo, (int)D.nblocks) < 0 )
    {
	errwarn("can't read %u", D.doff);
	return;
    }
}

/*
 * menu item to write from buf to disk
 */
void
writebuf_func(void)
{
    register daddr_t maxblocks;

    argnum(&D.bo, D.bo, MAXDB_bytes, "buf offset");
    maxblocks = MAXDB_blocks;
    if( D.nblocks <= 0 )
	D.nblocks = 1;
    if( D.nblocks >= maxblocks )
	D.nblocks = maxblocks;
    argbn(&D.nblocks, D.nblocks, maxblocks+1, "nblocks");
    argcheck();
    D.lastbo = D.doff + stob(D.nblocks);
	if(dbgetbuf())
		return;
    if( gwrite(D.doff, D.buf+D.bo, (int)D.nblocks) < 0 )
    {
	if(errno != EROFS)
		errwarn("can't write %u", D.doff);
	/* else msg already printed */
	return;
    }
}

/*
 * fill the buf with some string
 */
static void
fillsub(unchar *tgt, uint tlen, unchar *src, uint slen)
{
    register uint c;
    register unchar *sp;
    
    while( tlen > 0 )
    {
	c = tlen;
	if( c > slen )
	    c = slen;
	tlen -= c;
	sp = src;
	while( --c > 0 )
	    *tgt++ = *sp++;
    }
}
/*
 * menu item to fill buf with junk
 */
void
fillbuf_func(void)
{
    STRBUF s;
    uint len, nbytes, maxbytes;

    argnum(&D.bo, D.bo, MAXDB_bytes, "buf offset");
    maxbytes = MAXDB_bytes - D.bo;
    argstring(s.c, "", "fill string");
    if( (len = strlen(s.c)) <= 0 )
	len = 1;
    nbytes = D.lastbo - D.bo;
    if( !(0 < nbytes && nbytes <= maxbytes) )
	nbytes = maxbytes;
    argnum(&nbytes, nbytes, maxbytes+1, "nbytes");
    argcheck();
    D.lastbo = D.bo + nbytes;
    if( D.count < D.lastbo )
	D.count = D.lastbo;
	if(dbgetbuf())
		return;
    fillsub(D.buf+D.bo, nbytes, (unchar *)s.c, len);
}

void
diffbuf(unchar *a, unchar *b, uint nbytes)
{
    register int i, nbad;
    register unchar *cpa, *cpb;
	static int maxmismatch = 5;	/* so it can be changed with dbx */

    cpa = (unchar *)a; cpb = (unchar *)b;
    nbad = 0;
    for( i = 0; i < nbytes; i++ )
    {
	if( *cpa != *cpb )
	{
	    if( nbad++ >= maxmismatch )
	    {
		printf("max mismatch exceeded...\n");
		break;
	    }
	    printf("data miscompare at byte #%d 0x%02x 0x%02x\n",
		    i, *cpa, *cpb);
	}
	cpa++; cpb++;
    }
}
/*
 * menu item to compare 2 buffers
 */
void
cmpbuf_func(void)
{
    uint bob, nbytes, maxbytes;

	if(!D.j.c) {
		printf("no buffer ops yet\n");
		return;
	}

    argnum(&D.bo, D.bo, D.bufsize, "buf offset A");
    argnum(&bob, 0, D.bufsize, "buf offset B");
    maxbytes = D.bufsize - (D.bo > bob ? D.bo : bob);
    nbytes = D.lastbo - D.bo;
    if( !(0 < nbytes && nbytes <= maxbytes) )
	nbytes = maxbytes;
    argnum(&nbytes, nbytes, maxbytes+1, "nbytes");
    argcheck();
    D.lastbo = D.bo + nbytes;
    diffbuf(D.buf+D.bo, D.buf+bob, nbytes);
}

/*
 * fetch an object from the buf
 */
static int
peekbuf(unchar *addr, int itype)
{
    switch(itype)
    {
    case 0: return *(unchar *)addr;
    case 1: return *(short *)addr;
    case 2: return *(long *)addr;
    case 3: return *(uint64_t *)addr;
    default: return 0;
    }
}
/*
 * set an object in the buf
 */
static void
pokebuf(unchar *addr, int itype, uint64_t val)
{
    switch(itype)
    {
    case 0: *(unchar *)addr = val; break;
    case 1: *(short *)addr = val; break;
    case 2: *(long *)addr = val; break;
    case 3: *(uint64_t *)addr = val; break;
    }
}


static ITEM itype_items[] =
{
	{"bytes", 0},
	{"shorts", 1},
	{"longs", 2},
	{"uint64_ts", 3},	/* also daddr_t */
	{0}
};
static short itype_sizes[] = { sizeof (char), sizeof (short), sizeof (long),
	sizeof (uint64_t) };
static MENU itype_menu = {itype_items, "itypes"};


/*
 * menu item to edit buf
 */
void
editbuf_func(void)
{
    uint val;
	register int a;

    argchoice(&D.itype, 0, &itype_menu, "itype");
    argnum(&D.bo, D.bo, MAXDB_bytes, "buf offset");
	if(!D.j.c) {
		argnum(&D.lastbo, D.lastbo, MAXDB_bytes, "buffer size");
		if(dbgetbuf())
			return;
	}

    a = itype_sizes[itype_menu.items[D.itype].value];
    if( D.bo % a )
	argerr("can't edit at unaligned buf offset %d", D.bo);

    do
    {
	if( D.bo >= D.bufsize )
	    argerr("can't edit past end of buffer");
	val = peekbuf(D.buf+D.bo, D.itype);
	argnum(&val, val, 0x100, "value");
	pokebuf(D.buf+D.bo, D.itype, val);
	D.bo += a;
	D.lastbo = D.bo;
	if( D.bo > D.count )
	    D.count = D.bo;
    }
    while( noargs() );
}

/*
 * menu item to dump buf
 */
void
dumpbuf_func(void)
{
    uint ndump, maxdump, a, maxbytes;
    STRBUF s;

	if(!D.j.c) {
		printf("no buffer ops yet\n");
		return;
	}

    argchoice(&D.itype, 0, &itype_menu, "itype");
    argnum(&D.bo, D.bo, D.bufsize, "buf offset");
    maxbytes = D.bufsize - D.bo;
    a = itype_sizes[itype_menu.items[D.itype].value];
    maxdump = (maxbytes + a - 1)/a;
    ndump = (D.lastbo - D.bo + a - 1)/a;
    if( !(0 < ndump && ndump <= maxdump) )
	ndump = maxdump;
    sprintf(s.c, "n%s", itype_menu.items[D.itype].name);
    argnum(&ndump, ndump, maxdump+1, s.c);
    argcheck();
    D.lastbo = D.bo + ndump * a;
    (void)setintr(1);
    prdata(D.bo, D.buf+D.bo, ndump, a);
}
/*
 * menu item to conver a number to various bases
 */
void
number_func(void)
{
    daddr_t b;

    do
    {
	b = -1;
	argbn(&b, b, b, "number");
	printf("%lld == 0%llo == 0x%llx == '%c'\n", b, b, b, (unsigned char)b);
    }
    while( noargs() );
}


# define NBNIB		4
# define msk(x)		(~(~0L<<(x)))
static unchar digits[] = "0123456789ABCDEF";
/*
 * print arbitrary data in readable format.
 XX XX XX XX XX XX XX XX  XX XX XX XX XX XX XX XX  cccccccccccccccc
 * or
 XXXX XXXX XXXX XXXX  XXXX XXXX XXXX XXXX  cccccccccccccccc
 * or
 XXXXXXXX XXXXXXXX  XXXXXXXX XXXXXXXX  cccccccccccccccc
 */
static void
prdata(int offset, void *src, int len, int isize)
{
    static unchar *linebuf;	/* because likely to be interrupted */
	uint swidth = getscreenwidth();
    register unchar *lp;
    register int a;
    uint valsperline;
    uint denominator;
    uint numerator;


    if(linebuf)
	    free(linebuf);

    if(!(linebuf = (unchar *)malloc(swidth))) {
	    scerrwarn("Can't malloc buffer memory");
	    return;
    }

    /* offset, 'extra' white space; 2 nibbles/byte, space, and ascii */
        numerator = swidth - 12;
        switch(isize)  {
            case 1:
                denominator = 4;
                break;
            case 2:
                denominator = 7;
                break;
            case 4:
                denominator = 13;
                break;
            case 8:	/* not used yet, but may as well get ready */
                denominator = 22;
                break;
            default:
                numerator = 0;    /* to make valsperline 1 in this case */
                denominator = 1;
                break;
            }
               
    valsperline = numerator/denominator;

    if(!valsperline)
        valsperline = 1;


    a = (__psint_t)src % isize;
    offset -= a;
    src = (void *)((unchar *)src - a);
    len *= isize;

    while( len > 0 )
    {
	lp = linebuf;
	switch(isize)
	{
	case sizeof (char):
	    lp += charline(src, len, lp, valsperline);
	    src = (void *)((unchar *)src + valsperline);
	    break;
	case sizeof (short):
	    lp += shortline((ushort *)src, len, lp, valsperline);
	    src = (void *)((ushort *)src + valsperline);
	    break;
	case sizeof (int):
	    lp += longline((uint *)src, len, lp, valsperline);
	    src = (void *)((uint *)src + valsperline);
	    break;
	case sizeof (uint64_t):
	    lp += longlongline((uint64_t *)src, len, lp, valsperline);
	    src = (void *)((uint64_t *)src + valsperline);
	    break;
	}
	len -= valsperline * isize;
	spyline((unchar *)src, len, lp, valsperline * isize);
	printf("0x%04x> %s\n", offset, linebuf);
	offset += valsperline * isize;
    }
}


static int
charline(unchar *src, int len, unchar *tgt, int n)
{
    register unchar bbb;
    register unchar *lp;
    int extrasp = n/2 - 1;

    lp = tgt;
    while(n-- > 0) {
	*lp++ = ' ';
	if(n == extrasp)
		*lp++ = ' ';
	if( (len -= sizeof *src) < 0 )
	{
	    *lp++ = ' '; *lp++ = ' ';
	}
	else
	{
	    bbb = *src++;
	    *lp++ = digits[ (bbb >> 1*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 0*NBNIB)&msk(NBNIB) ];
	}
    }
    return lp - tgt;
}

static int
longlongline(uint64_t *src, int len, unchar *tgt, int n)
{
    register uint64_t bbb;
    register unchar *lp;
    int extrasp = n/2 - 1;

    lp = tgt;
    while(n-- > 0) {
	*lp++ = ' ';
	if(n == extrasp)
		*lp++ = ' ';
	if( (len -= sizeof *src) < 0 )
	{
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	}
	else
	{
	    bbb = *src++;
	    *lp++ = digits[ (bbb >> 15*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 14*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 13*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 12*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 11*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 10*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 9*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 8*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 7*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 6*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 5*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 4*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 3*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 2*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 1*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 0*NBNIB)&msk(NBNIB) ];
	}
    }
    return lp - tgt;
}

static int
longline(uint *src, int len, unchar *tgt, int n)
{
    register uint bbb;
    register unchar *lp;
    int extrasp = n/2 - 1;

    lp = tgt;
    while(n-- > 0) {
	*lp++ = ' ';
	if(n == extrasp)
		*lp++ = ' ';
	if( (len -= sizeof *src) < 0 )
	{
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	}
	else
	{
	    bbb = *src++;
	    *lp++ = digits[ (bbb >> 7*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 6*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 5*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 4*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 3*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 2*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 1*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 0*NBNIB)&msk(NBNIB) ];
	}
    }
    return lp - tgt;
}

static int
shortline(ushort *src, int len, unchar *tgt, int n)
{
    register ushort bbb;
    register unchar *lp;
    int extrasp = n/2 - 1;

    lp = tgt;
    while(n-- > 0) {
	*lp++ = ' ';
	if(n == extrasp)
		*lp++ = ' ';
	if( (len -= sizeof *src) < 0 )
	{
	    *lp++ = ' '; *lp++ = ' ';
	    *lp++ = ' '; *lp++ = ' ';
	}
	else
	{
	    bbb = *src++;
	    *lp++ = digits[ (bbb >> 3*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 2*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 1*NBNIB)&msk(NBNIB) ];
	    *lp++ = digits[ (bbb >> 0*NBNIB)&msk(NBNIB) ];
	}
    }
    return lp - tgt;
}
static void
spyline(unchar *src, int len, unchar *tgt, int n)
{
    register unsigned char bbb;
    register unchar *lp;
    lp = tgt;
	*lp++ = ' ';
	*lp++ = ' ';
    while( --n >= 0 )
    {
	if( --len < 0 )
	{
	    *lp++ = ' ';
	}
	else
	{
# define isprintable(c)	(040<(c)&&(c)<0177)
	    bbb = *src++;
	    *lp++ = isprintable(bbb)?bbb:'.';
# undef isprintable
	}
    }
    *lp = '\0';
}
