#ifndef TYPES_H
#define TYPES_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/types.h,v 1.7 1997/12/19 18:33:27 kcm Exp $"

/* fundamental page size - probably should not be hardwired, but
 * for now we will
 */
#define PGSZLOG2	12
#define PGSZ		( 1 << PGSZLOG2 )
#define PGMASK		( PGSZ - 1 )

/* integers
 */
typedef u_int32_t size32_t;
#ifndef BANYAN
typedef __int64_t int64_t;
#endif /* ! BANYAN */
typedef __uint64_t u_int64_t;
typedef u_int64_t size64_t;
typedef char char_t;
typedef unsigned char u_char_t;
typedef int intgen_t;
typedef unsigned int u_intgen_t;
typedef long long_t;
typedef unsigned long u_long_t;
typedef size_t ix_t;

/* limits
 */
#define	MKMAX( t, s )	( ( t )						\
			  ( ( ( 1ull					\
			        <<					\
			        ( ( unsigned long long )sizeof( t )	\
				  *					\
				  ( unsigned long long )NBBY		\
			          -					\
			          ( s + 1ull )))			\
			      -						\
			      1ull )					\
			    *						\
			    2ull					\
			    +						\
			    1ull ))
#define MKSMAX( t )	MKMAX( t, 1ull )
#define MKUMAX( t )	MKMAX( t, 0ull )
#define INT32MAX	MKSMAX( int32_t )
#define UINT32MAX	MKUMAX( u_int32_t )
#define SIZE32MAX	MKUMAX( size32_t )
#define INT64MAX	MKSMAX( int64_t )
#define UINT64MAX	MKUMAX( u_int64_t )
#define SIZE64MAX	MKUMAX( size64_t )
#define INO64MAX	MKUMAX( ino64_t )
#define OFF64MAX	MKSMAX( off64_t )
#define INTGENMAX	MKSMAX( intgen_t )
#define UINTGENMAX	MKUMAX( u_intgen_t )
#define OFFMAX		MKSMAX( off_t )
#define SIZEMAX		MKUMAX( size_t )
#define IXMAX		MKUMAX( size_t )
#define INOMAX		MKUMAX( ino_t )
#define TIMEMAX		MKSMAX( time_t )
#define INT16MAX	0x7fff
#define UINT16MAX	0xffff

/* boolean
 */
typedef intgen_t bool_t;
#define BOOL_TRUE	1
#define BOOL_FALSE	0
#define BOOL_UNKNOWN	( -1 )
#define BOOL_ERROR	( -2 )

/* useful return code scheme
 */
typedef enum { RV_OK,		/* mission accomplished */
	       RV_NOTOK,	/* request denied */
	       RV_NOMORE,	/* no more work to do */
	       RV_EOD,		/* ran out of data */
	       RV_EOF,		/* hit end of media file */
	       RV_EOM,		/* hit end of media object */
	       RV_ERROR,	/* operator error or resource exhaustion */
	       RV_DONE,		/* return early because someone else did work */
	       RV_INTR,		/* cldmgr_stop_requested( ) */
	       RV_CORRUPT,	/* stopped because corrupt data encountered */
	       RV_QUIT,		/* stop using resource */
	       RV_DRIVE,	/* drive quit working */
	       RV_TIMEOUT,	/* operation timed out */
	       RV_MEDIA,	/* no media object in drive */
	       RV_WRITEPOTECTED,/* want to write but write-protected */
	       RV_CORE		/* really bad - dump core! */
} rv_t;

/* typedefs I'd like to see ...
 */
typedef struct stat stat_t;
typedef struct stat64 stat64_t;
typedef struct getbmapx getbmapx_t;
typedef struct fsdmidata fsdmidata_t;

#endif /* TYPES_H */
