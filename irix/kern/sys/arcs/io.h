/* defs for arcs firmware support
 */
#ifndef _ARCS_IO_H
#define _ARCS_IO_H

#ident "$Revision: 1.33 $"

#include <arcs/large.h>
#include <arcs/hinv.h>

#define StandardIn	0
#define StandardOut	1

#define ReadOnlyFile	1
#define HiddenFile	2
#define SystemFile	4
#define ArchiveFile	8
#define DirectoryFile	16
#define DeleteFile	32

#define PathNameMax		(255)
#define FileNameLengthMax	(32)

typedef enum openmode {
	OpenReadOnly,
	OpenWriteOnly,
	OpenReadWrite,
	CreateWriteOnly,
	CreateReadWrite,
	SupersedeWriteOnly,
	SupersedeReadWrite,
	OpenDirectory,
	CreateDirectory
} OPENMODE;
typedef enum seekmode { SeekAbsolute, SeekRelative } SEEKMODE;
typedef enum mountoperation { LoadMedia, UnloadMedia } MOUNTOPERATION;

typedef LARGE LARGEINTEGER;			/* see <arcs/large.h> */
#define LowPart		lo
#define HighPart	hi

typedef struct fileinformation {
	LARGEINTEGER	StartingAddress;
	LARGEINTEGER	EndingAddress;
	LARGEINTEGER	CurrentAddress;
	CONFIGTYPE	Type;
	ULONG		FilenameLength;
	UCHAR		Attributes;
	CHAR		FileName[32];
} FILEINFORMATION;

/* non-ARCS defines: */
#define ARCS_FOPEN_MAX	20

/* default blocksize for devices */
#define BLKSIZE		512
#define BLKSIZEHIGH	(((ULONG)0xffffffff/BLKSIZE)+1)

/* ARCS compliant routines */
extern LONG Open(CHAR *, OPENMODE, ULONG *);
extern LONG Close(ULONG);
extern LONG Read(ULONG, void *, ULONG, ULONG *);
extern LONG GetReadStatus(ULONG);
extern LONG Write(ULONG, void *, ULONG, ULONG *);
extern LONG Seek(ULONG, LARGEINTEGER *, SEEKMODE);
extern LONG Mount(CHAR *, MOUNTOPERATION);

extern LONG GetFileInformation(ULONG, FILEINFORMATION *);
extern LONG SetFileInformation(ULONG, ULONG, ULONG);

extern LONG SetEnvironmentVariable(CHAR *, CHAR *);
extern CHAR *GetEnvironmentVariable(CHAR *);

extern LONG Load(CHAR *, ULONG, ULONG *, ULONG *);
extern LONG Invoke(ULONG, ULONG, LONG, CHAR *[], CHAR *[]);
extern LONG Execute(CHAR *, LONG, CHAR *[], CHAR *[]);

extern VOID FlushAllCaches(VOID);

#ifndef NULL
#define NULL		0
#endif

#endif
