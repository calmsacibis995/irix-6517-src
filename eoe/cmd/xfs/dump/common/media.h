#ifndef MEDIA_H
#define MEDIA_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/media.h,v 1.1 1995/06/21 22:47:18 cbullis Exp $"

/* media.[hc] - media abstraction
 */

/* media_hdr_t - media file header
 *
 * This header structure will be placed at the beginning of the
 * media object files by the drive manager do_begin_write() operator,
 * and will be extracted from the beginnining of the media object files by
 * the do_begin_read() operator. A media header_t has three parts: generally
 * useful info, media strategy-specific info, and upper layer info. The hdr
 * argument of the mo_begin_write() operator will be stuffed into the
 * upper layer info, and extracted for the upper layer by mo_begin_read().
 */
#define MEDIA_HDR_SZ		sizeofmember( drive_hdr_t, dh_upper )

struct media_hdr {
	char mh_medialabel[ GLOBAL_HDR_STRING_SZ ];	/* 100  100 */
		/* label of media object containing file */
	char mh_prevmedialabel[ GLOBAL_HDR_STRING_SZ ];	/* 100  200 */
		/* label of upstream media object */
	char mh_pad1[ GLOBAL_HDR_STRING_SZ ];		/* 100  300 */
		/* in case more labels needed */
	uuid_t mh_mediaid;				/*  10  310 */
		/* ID of media object 	*/
	uuid_t mh_prevmediaid;				/*  10  320 */
		/* ID of upstream media object */
	char mh_pad2[ GLOBAL_HDR_UUID_SZ ];		/*  10  330 */
		/* in case more IDs needed */
	u_int32_t mh_mediaix;				/*   4  334 */
		/* 0-based index of this media object within the dump stream */
	u_int32_t mh_mediafileix;			/*   4  338 */
		/* 0-based index of this file within this media object */
	u_int32_t mh_dumpfileix;			/*   4  33c */
		/* 0-based index of this file within this dump stream */
	u_int32_t mh_dumpmediafileix;			/*   4  340 */
		/* 0-based index of file within dump stream and media object */
	u_int32_t mh_dumpmediaix;			/*   4  344 */
		/* 0-based index of this dump within the media object */
	int32_t mh_strategyid;				/*   4  348 */
		/* ID of the media strategy used to produce this dump */
	char mh_pad3[ 0x38 ];				/*  38  380 */
		/* padding */
	char mh_specific[ 0x80 ];			/*  80  400 */
		/* media strategy-specific info */
	char mh_upper[ MEDIA_HDR_SZ - 0x400 ];		/* 400  800 */
		/* header info private to upper software layers */
};

typedef struct media_hdr media_hdr_t;

/* macros to mark a media file as a terminator. artifact of original
 * media_rmvtape media strategy
 */
#define MEDIA_TERMINATOR_CHK( mrhp )	( mrhp->mh_specific[ 0 ] & 1 )
#define MEDIA_TERMINATOR_SET( mwhp )	( mwhp->mh_specific[ 0 ] |= 1 )

/* media strategy IDs. artifactis of first version of xfsdump
 */
#define MEDIA_STRATEGY_SIMPLE	0
#define MEDIA_STRATEGY_RMVTAPE	1

#endif /* MEDIA_H */
