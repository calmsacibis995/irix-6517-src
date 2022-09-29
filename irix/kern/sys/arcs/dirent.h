/* ARCS get directory definitions
 */
#ifndef _ARCS_GETDENT_H
#define _ARCS_GETDENT_H

#ident "$Revision: 1.6 $"

typedef struct {
	ULONG FileNameLength;
	UCHAR FileAttribute;
	CHAR  FileName[FileNameLengthMax];
} DIRECTORYENTRY;

extern LONG GetDirectoryEntry(ULONG, DIRECTORYENTRY *, ULONG, ULONG *);

#endif
