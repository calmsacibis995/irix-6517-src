/* ARCS transfer vector definition and SGI private transfer definition.
 */
#ifndef _ARCS_TVECTORS_H
#define _ARCS_TVECTORS_H

#ident "$Revision: 1.20 $"

#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/time.h>

/* ARCS Firmware Vector */
typedef struct {
	LONG		(*Load)(CHAR *, ULONG, ULONG *, ULONG *);
	LONG		(*Invoke)(ULONG, ULONG, LONG, CHAR *[], CHAR *[]);
	LONG		(*Execute)(CHAR *, LONG, CHAR *[], CHAR *[]);
	void		(*Halt)(void);
	void		(*PowerDown)(void);
	void		(*Restart)(void);
	void		(*Reboot)(void);
	void		(*EnterInteractiveMode)(void);
	int		reserved1;
	COMPONENT	*(*GetPeer)(COMPONENT *);
	COMPONENT	*(*GetChild)(COMPONENT *);
	COMPONENT	*(*GetParent)(COMPONENT *);
	LONG		(*GetConfigData)(void *, COMPONENT *);
	COMPONENT	*(*AddChild)(COMPONENT *, COMPONENT *, void *);
	LONG		(*DeleteComponent)(COMPONENT *);
	COMPONENT	*(*GetComponent)(CHAR *);
	LONG		(*SaveConfiguration)(void);
	SYSTEMID	*(*GetSystemId)(void);
	MEMORYDESCRIPTOR *(*GetMemoryDesc)(MEMORYDESCRIPTOR *);
	long		reserved2;
	TIMEINFO	*(*GetTime)(void);
	ULONG		(*GetRelativeTime)(void);
	LONG		(*GetDirEntry)(ULONG, DIRECTORYENTRY *, ULONG, ULONG *);
	LONG		(*Open)(CHAR *, OPENMODE, ULONG *);
	LONG		(*Close)(ULONG);
	LONG		(*Read)(ULONG, void *, ULONG, ULONG *);
	LONG		(*GetReadStatus)(ULONG);
	LONG		(*Write)(ULONG, void *, ULONG, ULONG *);
	LONG		(*Seek)(ULONG, LARGEINTEGER *, SEEKMODE);
	LONG		(*Mount)(CHAR *, MOUNTOPERATION);
	CHAR		*(*GetEnvironmentVariable)(CHAR *);
	LONG		(*SetEnvironmentVariable)(CHAR *, CHAR *);
	LONG		(*GetFileInformation)(ULONG, FILEINFORMATION *);
	LONG		(*SetFileInformation)(ULONG, ULONG, ULONG);
	VOID		(*FlushAllCaches)(void);
} FirmwareVector;

#if __USE_SPB32 || (defined(_K64PROM32) && !defined(_STANDALONE))
/* 32-bit ARCS Firmware Vector */
typedef struct {
	__int32_t		Load;
	__int32_t		Invoke;
	__int32_t		Execute;
	__int32_t		Halt;
	__int32_t		PowerDown;
	__int32_t		Restart;
	__int32_t		Reboot;
	__int32_t		EnterInteractiveMode;
	__int32_t		reserved1;
	__int32_t		GetPeer;
	__int32_t		GetChild;
	__int32_t		GetParent;
	__int32_t		GetConfigData;
	__int32_t		AddChild;
	__int32_t		DeleteComponent;
	__int32_t		GetComponent;
	__int32_t		SaveConfiguration;
	__int32_t		GetSystemId;
	__int32_t		GetMemoryDesc;
	__int32_t		reserved2;
	__int32_t		GetTime;
	__int32_t		GetRelativeTime;
	__int32_t		GetDirEntry;
	__int32_t		Open;
	__int32_t		Close;
	__int32_t		Read;
	__int32_t		GetReadStatus;
	__int32_t		Write;
	__int32_t		Seek;
	__int32_t		Mount;
	__int32_t		GetEnvironmentVariable;
	__int32_t		SetEnvironmentVariable;
	__int32_t		GetFileInformation;
	__int32_t		SetFileInformation;
	__int32_t		FlushAllCaches;
} FirmwareVector32;
#endif /* __USE_SPB32 || (defined(_K64PROM32) && !defined(_STANDALONE)) */


/* old ARCS Firmware Vector version 1 -- in many beta IP20 systems */
typedef struct {
	LONG		(*Load)();
	LONG		(*Invoke)();
	LONG		(*Execute)();
	void		(*Halt)();
	void		(*PowerDown)();
	void		(*Restart)();
	void		(*Reboot)();
	void		(*EnterInteractiveMode)();
	COMPONENT	*(*GetPeer)();
	COMPONENT	*(*GetChild)();
	COMPONENT	*(*GetParent)();
	LONG		(*GetConfigData)();
	COMPONENT	*(*AddChild)();
	LONG		(*DeleteComponent)();
	COMPONENT	*(*GetComponent)();
	LONG		(*SaveConfiguration)();
	SYSTEMID	*(*GetSystemId)();
	MEMORYDESCRIPTOR *(*GetMemoryDesc)();
	TIMEINFO	*(*GetTime)();
	ULONG		(*GetRelativeTime)();
	LONG		(*GetDirEntry)();
	LONG		(*Open)();
	LONG		(*Close)();
	LONG		(*Read)();
	LONG		(*GetReadStatus)();
	LONG		(*Write)();
	LONG		(*Seek)();
	LONG		(*Mount)();
	LONG		(*GetFileInformation)();
	LONG		(*SetFileInformation)();
	CHAR		*(*GetGlobalVariable)();
	LONG		(*SetGlobalVariable)();
} oldFirmwareVector_1;

/*  Convert from old vector layout to new.  Kernel only uses portion of
 * arcs functionality, so keep update minimal.
 */
#ifdef _STANDALONE
#define oldtv1_2_latest(spb,old,new)				\
	new->GetFileInformation = old->GetFileInformation;	\
	new->SetFileInformation = old->SetFileInformation;	\
	new->Mount = old->Mount;				\
	new->Seek = old->Seek;					\
	new->Write = old->Write;				\
	new->GetReadStatus = old->GetReadStatus;		\
	new->Read = old->Read;					\
	new->Close = old->Close;				\
	new->Open = old->Open;					\
	new->GetDirEntry = old->GetDirEntry;			\
	new->GetRelativeTime = old->GetRelativeTime;		\
	new->GetTime = old->GetTime;				\
	new->GetMemoryDesc = old->GetMemoryDesc;		\
	new->GetSystemId = old->GetSystemId;			\
	new->SaveConfiguration = old->SaveConfiguration;	\
	new->GetComponent = old->GetComponent;			\
	new->DeleteComponent = old->DeleteComponent;		\
	new->AddChild = old->AddChild;				\
	new->GetConfigData = old->GetConfigData;		\
	new->GetParent = old->GetParent;			\
	new->GetChild = old->GetChild;				\
	new->GetPeer = old->GetPeer;				\
	new->reserved1 = new->reserved2 = 0;			\
	spb->TVLength = sizeof(FirmwareVector);
#else
#define oldtv1_2_latest(spb,old,new)				\
	new->Write = old->Write;				\
	new->GetChild = old->GetChild;				\
	new->GetPeer = old->GetPeer;				\
	spb->TVLength = sizeof(FirmwareVector);
#endif	/* _STANDALONE */
	
#endif
