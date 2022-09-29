/* HPUX_ID: @(#)drand48.c	27.1     84/11/14  */
static unsigned short x[3] = { 0x330E, 0xABCD, 0x1234 };
static unsigned short a[3] = { 0xE66D, 0xDEEC, 0x0005 };
static unsigned short c = 0xB;
static unsigned short lastx[3];
static void next();

double drand48()
{
#ifdef hp9000s200
	/* On the 200 it is faster to "bit-twiddle" than to do the multiplies */
	register unsigned char *p, *q;
	register char shift;
	char norm;
	register char byte;
	union {
		unsigned char b[6];
		unsigned short s[3];
	} arg;
	union {
		unsigned short s;
		unsigned char b[8];
		unsigned long l[2];
		double d;
	} res;

	next();			/* get next 48 bit pseudo-random number */
	arg.s[0] = x[2];	/* store away number */
	arg.s[1] = x[1];
	arg.s[2] = x[0];
	p = arg.b;		/* will be scanning bytes */
	for (shift = 6;;) {	/* need to know which byte has MSB */
		if (*p++) break;		/* is it here? */
		if (!--shift) return(0);	/* if no MSB, must be 0 */
	};
	norm = 6 - shift << 3;	/* number of bits effectively shifted */
	byte = *--p;		/* get byte with MSB */
	res.l[0] = 0;		/* clear out result area */
	res.l[1] = 0;
	q = &res.b[2];		/* start moving bytes to here */
	do { *q++ = *p++; } while (--shift);	/* move'm while we got'm */
	shift = 5;		/* will have to shift left at least 5 */
	while (byte > 0) { 	/* where is that MSB in the byte? */
		shift++;	/* not found; one more shift */
		byte += byte;	/* shift toward sign bit */
	};
	res.l[0] = res.l[0] << shift | res.l[1] >> 32 - shift;	/* shift mantissa correct amount */
	res.l[1] <<= shift;
	res.s += (0x402 - shift - norm) << 4;	/* adjust exponent */
	return(res.d);		/* return the double that this represents */
#else
	double two16m = 1.0 / (1L << 16);
	next();			/* get next 48 bit pseudo-random number */
	return (two16m * (two16m * (two16m * x[0] + x[1]) + x[2]));
#endif
}

double erand48(xsubi) register unsigned short *xsubi;
{
	unsigned short temp[3];
	double v;
	temp[0] = x[0];		/* save old seed */
	temp[1] = x[1];
	temp[2] = x[2];
	x[0] = *xsubi++;	/* store new seed */
	x[1] = *xsubi++;
	x[2] = *xsubi++;
	v = drand48();		/* get random double */
	*--xsubi = x[2];	/* store for returning new seed */
	*--xsubi = x[1];
	*--xsubi = x[0];
	x[0] = temp[0];		/* restore old seed */
	x[1] = temp[1];
	x[2] = temp[2];
	return(v);
}

long jrand48(xsubi) register unsigned short *xsubi;
{
	unsigned short temp[3];
	union {
		long l;
		unsigned short s[2];
	} res;

	temp[0] = x[0];		/* save old seed */
	temp[1] = x[1];
	temp[2] = x[2];
	x[0] = *xsubi++;	/* get new seed */
	x[1] = *xsubi++;
	x[2] = *xsubi++;
	next();			/* get next 48 bit pseudo-random number */
	res.s[0] = x[2];	/* form the long value */
	res.s[1] = x[1];
	*--xsubi = x[2];	/* get new seed for return */
	*--xsubi = x[1];
	*--xsubi = x[0];
	x[0] = temp[0];		/* restore old seed */
	x[1] = temp[1];
	x[2] = temp[2];
	return (res.l);
}

void lcong48(param) unsigned short param[7];
{
	x[0] = param[0];	/* get parameters */
	x[1] = param[1];
	x[2] = param[2];
	a[0] = param[3];
	a[1] = param[4];
	a[2] = param[5];
	c = param[6];
}

long lrand48()
{
	union {
		unsigned long l;
		unsigned short s[2];
	} res;

	next();			/* get next 48 bit pseudo-random number */
	res.s[0] = x[2];	/* form long from result */
	res.s[1] = x[1];
	return (res.l >> 1);	/* 0 sign bit */
}

long mrand48()
{
	union {
		long l;
		unsigned short s[2];
	} res;

	next();			/* get next 48 bit pseudo-random number */
	res.s[0] = x[2];	/* form long from result */
	res.s[1] = x[1];
	return (res.l);
}

long nrand48(xsubi) register unsigned short *xsubi;
{
	unsigned short temp[3];
	union {
		unsigned long l;
		unsigned short s[2];
	} res;

	temp[0] = x[0];		/* save old seed */
	temp[1] = x[1];
	temp[2] = x[2];
	x[0] = *xsubi++;	/* store new seed */
	x[1] = *xsubi++;
	x[2] = *xsubi++;
	next();			/* get next 48 bit pseudo-random number */
	res.s[0] = x[2];	/* form long from result */
	res.s[1] = x[1];
	*--xsubi = x[2];	/* store new seed for return */
	*--xsubi = x[1];
	*--xsubi = x[0];
	x[0] = temp[0];		/* restore old seed */
	x[1] = temp[1];
	x[2] = temp[2];
	return (res.l >> 1);	/* 0 for sign bit */
}

unsigned short *seed48(seed16v) unsigned short seed16v[3];
{
	lastx[2] = x[2];	/* store seed for return */
	lastx[1] = x[1];
	lastx[0] = x[0];
	x[2] = seed16v[2];	/* set new seed */
	x[1] = seed16v[1];
	x[0] = seed16v[0];
	a[2] = 0x0005;		/* set new multiplier */
	a[1] = 0xDEEC;
	a[0] = 0xE66D;
	c = 0xB;		/* set new addend */
	return(lastx);		/* return old seed */
}

void srand48(seedval) long seedval;
{
	union {
		long l;
		unsigned short s[2];
	} arg;

	arg.l = seedval;	/* store to break into shorts */
	x[2] = arg.s[0];	/* get shorts for seed */
	x[1] = arg.s[1];
	x[0] = 0x330E;		/* defined least significant word */
	a[2] = 0x0005;		/* restore multiplier */
	a[1] = 0xDEEC;
	a[0] = 0xE66D;
	c = 0xB;		/* restore addend */
}

static void next()
{
	unsigned short p0;
	union {
		unsigned long l;
		unsigned short s[2];
	} arg;

#ifdef hp9000s500
	/* turn off traps for integer overflow */
	asm("
		pshr	4
		lni	0x800001
		and
		setr	4
	");
#endif
	/* The new seed will be: (old_seed * multiplier + addend) mod 2^48 */
	/* The product looks like:

		----p2----  ----p0----
		      ----p1----
		      |
		      |<----only up to here is needed
        */

	p0 = arg.l = x[0] * a[0];	/* generate p0 */
	arg.s[1] = arg.s[0];	/* shift p0 right 16 */
	arg.s[0] = x[0] * a[2] + x[1] * a[1] + x[2] * a[0]; /* accumulate useable part of p2 */
	arg.l += x[0] * a[1] + x[1] * a[0];	/* add p1 */
	if ((x[0] = p0 + c) < c) arg.l++;	/* add addend and propagate carry */
	x[2] = arg.s[0];	/* most significant word */
	x[1] = arg.s[1];	/* 2nd word */
}
