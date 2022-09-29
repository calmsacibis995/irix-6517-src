#ifndef GLOBAL_H
#define GLOBAL_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/global.h,v 1.4 1997/07/30 20:45:23 prasadb Exp $"

/* global_hdr_t - first page of every media file
 */
#define GLOBAL_HDR_SZ		PGSZ
#define GLOBAL_HDR_MAGIC	"xFSdump0"
#define GLOBAL_HDR_MAGIC_SZ	8
#ifdef EXTATTR
#define GLOBAL_HDR_VERSION_0	0
#define GLOBAL_HDR_VERSION_1	1
#define GLOBAL_HDR_VERSION_2	2
	/* version 2 adds encoding of holes and a change to on-tape inventory format.
	 * version 1 adds extended file attribute dumping.
	 * version 0 xfsrestore can't handle media produced
	 * by version 1 xfsdump. 
	 */
#define GLOBAL_HDR_VERSION	GLOBAL_HDR_VERSION_2
#define GLOBAL_HDR_VERSION_PREV	1
#else /* EXTATTR */
#define GLOBAL_HDR_VERSION_0	0
#ifndef BANYAN
#define GLOBAL_HDR_VERSION_1	1
#endif /* ! BANYAN */
#define GLOBAL_HDR_VERSION	GLOBAL_HDR_VERSION_0
#define GLOBAL_HDR_VERSION_PREV	( -1 )
#endif /* EXTATTR */
#define GLOBAL_HDR_STRING_SZ	0x100
#define GLOBAL_HDR_TIME_SZ	4
#define GLOBAL_HDR_UUID_SZ	0x10

struct global_hdr {
	char gh_magic[ GLOBAL_HDR_MAGIC_SZ ];		/*   8    8 */
		/* unique signature of xfsdump */
	u_int32_t gh_version;				/*   4    c */
		/* header version */
	u_int32_t gh_checksum;				/*   4   10 */
		/* 32-bit unsigned additive inverse of entire header */
	time_t gh_timestamp;				/*   4   14 */
		/* time_t of dump */
	char gh_pad1[ 4 ];				/*   4   18 */
		/* alignment */
	u_int64_t gh_ipaddr;				/*   8   20 */
		/* from gethostid(2), room for expansion */
	uuid_t gh_dumpid;				/*  10   30 */
		/* ID of dump session	 */
	char gh_pad2[ 0xd0 ];				/*  d0  100 */
		/* alignment */
	char gh_hostname[ GLOBAL_HDR_STRING_SZ ];	/* 100  200 */
		/* from gethostname(2) */
	char gh_dumplabel[ GLOBAL_HDR_STRING_SZ ];	/* 100  300 */
		/* label of dump session */
	char gh_pad3[ 0x100 ];				/* 100  400 */
		/* padding */
	char gh_upper[ GLOBAL_HDR_SZ - 0x400 ];		/* c00 1000 */
		/* header info private to upper software layers */
};

typedef struct global_hdr global_hdr_t;


/* used by main( ) to allocate and populate a global header template.
 * drive managers will copy this into the write header.
 */
extern global_hdr_t * global_hdr_alloc( intgen_t argc, char *argv[ ] );


/* used by main( ) to free the global header template after drive ini.
 */
extern void global_hdr_free( global_hdr_t *ghdrp );


/* global_hdr_checksum_set - fill in the global media file header checksum.
 * utility function for use by drive-specific strategies.
 */
extern void global_hdr_checksum_set( global_hdr_t *hdrp );


/* global_hdr_checksum_check - check the global media file header checksum.
 * utility function for use by drive-specific strategies.
 * returns BOOL_TRUE if ok, BOOL_FALSE if bad
 */
extern bool_t global_hdr_checksum_check( global_hdr_t *hdrp );

/* global_version_check - if we know this version number, return BOOL_TRUE 
 * else return BOOL_FALSE
 */

extern bool_t global_version_check( u_int32_t version );


#endif /* GLOBAL_H */
