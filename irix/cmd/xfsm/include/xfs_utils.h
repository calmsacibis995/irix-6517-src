/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef	_xfs_utils_h
#define	_xfs_utils_h

#ident "$Revision: 1.4 $"

#include	<sys/types.h>
#include	<rpc/rpc.h>

/* Prototypes of common utility functions */
extern FILE * xfs_popen(const char* cmd, const char* mode, int attach_stream);
extern int xfs_pclose(FILE *ptr);
extern void add_to_buffer(char** buffer,char* str);

/* Debug */
extern void open_debug_file(const char* filename);
extern void dump_to_debug_file(const char* buffer);
extern void close_debug_file(void);

extern boolean_t	xfsParseObjectSignature(const char *, const char *,
				char *, char *, char *, char *, char **);
extern boolean_t	xfsRpcCallInit(char *, CLIENT **, char **);
extern boolean_t	xfsConvertNameToDevice(char *, char *, boolean_t);
extern boolean_t	xfsmGetKeyValue(char *, char **, char **);

#endif	/* _xfs_utils_h */
