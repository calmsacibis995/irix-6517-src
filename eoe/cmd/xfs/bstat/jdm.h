#ifndef JDM_H
#define JDM_H

#ident "$Id: jdm.h,v 1.1 1999/04/28 22:12:01 cwf Exp $"

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
