#ifndef JDM_H
#define JDM_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/rfind/include/RCS/jdm.h,v 1.1 1995/07/21 17:43:47 sunita Exp $"

typedef void jdm_fshandle_t;

extern jdm_fshandle_t *jdm_getfshandle( char *mntpnt );

extern intgen_t jdm_open( jdm_fshandle_t *fshandlep,
			  xfs_bstat_t *statp,
			  intgen_t oflags );

extern intgen_t jdm_readlink( jdm_fshandle_t *fshandlep,
			      xfs_bstat_t *statp,
			      char *bufp,
			      size_t bufsz );

#endif /* JDM_H */
