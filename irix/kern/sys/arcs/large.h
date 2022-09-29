/*
 * large.h - Standalone support for 64 bit integers
 *
 * $Revision: 1.3 $
 */
#ifndef _LARGE_H
#define _LARGE_H

/* structures for 64 bit operations */
#ifdef	_MIPSEB
typedef struct large {
    long hi;
    unsigned long lo;
} LARGE;

typedef struct ularge {
    unsigned long hi;
    unsigned long lo;
} ULARGE;
#else	/* _MIPSEL */
typedef struct large {
    unsigned long lo;
    long hi;
} LARGE;

typedef struct ularge {
    unsigned long lo;
    unsigned long hi;
} ULARGE;
#endif	/* _MIPSEL */

/* 64 bit adds return 1 on overflow */
extern int addlarge(LARGE *,LARGE *,LARGE *);
extern int addlongtolarge(long, LARGE *);
extern int addulongtolarge(unsigned long, LARGE *);
extern void longtolarge(long, LARGE *);
extern void ulongtolarge(unsigned long, LARGE *);

#define addoffset(iob,off) addlarge(&iob.Offset,&off,&iob.Offset)
#define addoffsetp(iob,off) addlarge(iob->Offset,&off,iob->Offset)

#endif
