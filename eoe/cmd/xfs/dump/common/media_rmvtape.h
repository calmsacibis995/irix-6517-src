#ifndef MEDIA_RMVTAPE_H
#define MEDIA_RMVTAPE_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/media_rmvtape.h,v 1.5 1994/09/15 20:48:16 cbullis Exp $"

/* media_rmvtape.h - removalable tape media abstraction
 */

/* This structure is overlayed on the mh_specific field of the media_hdr 
 * structure. If is a maximum of 128 byptes long.
 */
struct media_rmvtape_spec {
	int32_t mrmv_flags;				/*  4	0 */
		/* flags field for the rmv media layer */
	char 	mrmv_pad[124];				/* 124 	4 */
		/* remainder of media specific header  */
};

typedef struct media_rmvtape_spec media_rmvtape_spec_t;

/* media context specific to the rmvtape media driver
 *
 */
struct media_context {
	uuid_t	mc_mediaid;
	uuid_t	mc_dumpid;
	char	mc_medialabel[GLOBAL_HDR_STRING_SZ];
	char	mc_dumplabel[GLOBAL_HDR_STRING_SZ];
};

typedef struct media_context media_context_t;

/* flags defined in the rmv media layer
 *
 */
#define	RMVMEDIA_TERMINATOR_BLOCK	0x00000001

#define TERM_IS_SET(rmv_hdrp)	(rmv_hdrp->mrmv_flags & RMVMEDIA_TERMINATOR_BLOCK)


#define CAN_OVERWRITE( drivep )	(drivep->d_capabilities & DRIVE_CAP_OVERWRITE)
#define CAN_APPEND( drivep )	(drivep->d_capabilities & DRIVE_CAP_APPEND)
#define CAN_BSF( drivep )	(drivep->d_capabilities & DRIVE_CAP_BSF)

#endif
