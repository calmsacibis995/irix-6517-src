#ident "$Revision: 1.4 $"

#define	bitize(s)	((s) * NBBY)
#define	bitsz(t)	bitize(sizeof(t))
#define	bitszof(x,y)	bitize(szof(x,y))
#define	byteize(s)	((s) / NBBY)
#define	bitoffs(s)	((s) % NBBY)

#define	BVUNSIGNED	0
#define	BVSIGNED	1

extern __int64_t	getbitval(void *obj, int bitoff, int nbits, int flags);
extern void             setbitval(void *obuf, int bitoff, int nbits, void *ibuf);
