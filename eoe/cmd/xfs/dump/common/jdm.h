#ifndef JDM_H
#define JDM_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/jdm.h,v 1.2 1997/12/15 19:35:53 kcm Exp $"

struct	attrlist_cursor;

typedef void jdm_fshandle_t;

extern jdm_fshandle_t *jdm_getfshandle( char *mntpnt );

extern intgen_t jdm_open( jdm_fshandle_t *fshandlep,
			  xfs_bstat_t *statp,
			  intgen_t oflags );

extern intgen_t jdm_readlink( jdm_fshandle_t *fshandlep,
			      xfs_bstat_t *statp,
			      char *bufp,
			      size_t bufsz );
extern intgen_t jdm_attr_multi( jdm_fshandle_t *fshandlep,
				xfs_bstat_t *statp,
              			char *bufp, int rtrvcnt,
				int flags);

extern intgen_t	jdm_attr_list( jdm_fshandle_t *fshandlep,
		xfs_bstat_t *statp,
		char *bufp, size_t bufsz, int flags, 
		struct attrlist_cursor *cursor);
#endif /* JDM_H */
