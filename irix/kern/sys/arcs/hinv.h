/* ARCS hardware/memory inventory/configuration and system ID definitions.
 */
#ifndef __ARCS_HINV_H__
#define __ARCS_HINV_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.43 $"

#include <arcs/types.h>

/* configuration query defines */
typedef enum configclass {
	SystemClass,
	ProcessorClass,
	CacheClass,
#ifndef	_NT_PROM
	MemoryClass,
	AdapterClass,
	ControllerClass,
	PeripheralClass
#else	/* _NT_PROM */
	AdapterClass,
	ControllerClass,
	PeripheralClass,
	MemoryClass
#endif	/* _NT_PROM */
} CONFIGCLASS;

typedef enum configtype {
	ARC,
	CPU,
	FPU,
	PrimaryICache,
	PrimaryDCache,
	SecondaryICache,
	SecondaryDCache,
	SecondaryCache,
#ifndef	_NT_PROM
	Memory,
#endif
	EISAAdapter,
	TCAdapter,
	SCSIAdapter,
	DTIAdapter,
	MultiFunctionAdapter,
	DiskController,
	TapeController,
	CDROMController,
	WORMController,
	SerialController,
	NetworkController,
	DisplayController,
	ParallelController,
	PointerController,
	KeyboardController,
	AudioController,
	OtherController,
	DiskPeripheral,
	FloppyDiskPeripheral,
	TapePeripheral,
	ModemPeripheral,
	MonitorPeripheral,
	PrinterPeripheral,
	PointerPeripheral,
	KeyboardPeripheral,
	TerminalPeripheral,
	LinePeripheral,
	NetworkPeripheral,
#ifdef	_NT_PROM
	Memory,
#endif
	OtherPeripheral,

	/* new stuff for IP30 */
	/* added without moving anything */
	/* except ANONYMOUS. */

	XTalkAdapter,
	PCIAdapter,
	GIOAdapter,
	TPUAdapter,

	Anonymous
} CONFIGTYPE;

typedef enum {
	Failed = 1,
	ReadOnly = 2,
	Removable = 4,
	ConsoleIn = 8,
	ConsoleOut = 16,
	Input = 32,
	Output = 64
} IDENTIFIERFLAG;

#ifndef NULL			/* for GetChild(NULL); */
#define	NULL	0
#endif

union key_u {
	struct {
#ifdef	_MIPSEB
		unsigned char  c_bsize;		/* block size in lines */
		unsigned char  c_lsize;		/* line size in bytes/tag */
		unsigned short c_size;		/* cache size in 4K pages */
#else	/* _MIPSEL */
		unsigned short c_size;		/* cache size in 4K pages */
		unsigned char  c_lsize;		/* line size in bytes/tag */
		unsigned char  c_bsize;		/* block size in lines */
#endif	/* _MIPSEL */
	} cache;
	ULONG FullKey;
};

#if _MIPS_SIM == _ABI64
#define SGI_ARCS_VERS	64			/* sgi 64-bit version */
#define SGI_ARCS_REV	0			/* rev .00 */
#else
#define SGI_ARCS_VERS	1			/* first version */
#define SGI_ARCS_REV	10			/* rev .10, 3/04/92 */
#endif

typedef struct component {
	CONFIGCLASS	Class;
	CONFIGTYPE	Type;
	IDENTIFIERFLAG	Flags;
	USHORT		Version;
	USHORT		Revision;
	ULONG 		Key;
	ULONG		AffinityMask;
	ULONG		ConfigurationDataSize;
	ULONG		IdentifierLength;
	char		*Identifier;
} COMPONENT;

/* internal structure that holds pathname parsing data */
struct cfgdata {
	char *name;			/* full name */
	int minlen;			/* minimum length to match */
	CONFIGTYPE type;		/* type of token */
};

/* System ID */
typedef struct systemid {
	CHAR VendorId[8];
	CHAR ProductId[8];
} SYSTEMID;

/* memory query functions */
typedef enum memorytype {
	ExceptionBlock,
	SPBPage,			/* ARCS == SystemParameterBlock */
#ifndef	_NT_PROM
	FreeContiguous,
	FreeMemory,
	BadMemory,
	LoadedProgram,
	FirmwareTemporary,
	FirmwarePermanent
#else	/* _NT_PROM */
	FreeMemory,
	BadMemory,
	LoadedProgram,
	FirmwareTemporary,
	FirmwarePermanent,
	FreeContiguous
#endif	/* _NT_PROM */
} MEMORYTYPE;

typedef struct memorydescriptor {
	MEMORYTYPE	Type;
	LONG		BasePage;
	LONG		PageCount;
} MEMORYDESCRIPTOR;

#define NBPP            _PAGESZ		/* Number of bytes per page */

#if	NBPP == 4096
#define BPPSHIFT	12		/* Shift for page number from addr. */
#endif
#if	NBPP == 16384
#define BPPSHIFT	14		/* Shift for page number from addr. */
#endif

#define btop(x)		(((ulong_t)(x)+(NBPP-1))>>BPPSHIFT)
#define btopt(x)	((ulong_t)(x)>>BPPSHIFT)
#define	ptob(x)		((__psunsigned_t)((x)<<BPPSHIFT))

/*  We keep memory lists with 4k pages even though the actual system page
 * size may be larger.
 */
#define ARCS_NBPP	4096
#define ARCS_BPPSHIFT	12		/* Shift for page number from addr. */
#define arcs_btop(x)	(((ulong_t)(x)+(ARCS_NBPP-1))>>ARCS_BPPSHIFT)
#define arcs_btopt(x)	((ulong_t)(x)>>ARCS_BPPSHIFT)
#define	arcs_ptob(x)	((__psunsigned_t)((x)<<ARCS_BPPSHIFT))

extern COMPONENT *GetPeer(COMPONENT *);
extern COMPONENT *GetChild(COMPONENT *);
extern COMPONENT *GetParent(COMPONENT *);
extern LONG GetConfigurationData(void *,COMPONENT *);
extern COMPONENT *AddChild(COMPONENT *, COMPONENT *, void *);
extern LONG DeleteComponent(COMPONENT *);
extern COMPONENT *GetComponent(CHAR *);
extern LONG SaveConfiguration(void);

extern LONG	RegisterInstall(COMPONENT *, void (*)(COMPONENT*));

extern SYSTEMID *GetSystemId(void);

extern MEMORYDESCRIPTOR *GetMemoryDescriptor(MEMORYDESCRIPTOR *);
extern MEMORYDESCRIPTOR *md_alloc(unsigned long, unsigned long, MEMORYTYPE);
extern MEMORYDESCRIPTOR *md_add(unsigned long, unsigned long, MEMORYTYPE);
extern LONG md_dealloc(unsigned long, unsigned long);

#ifdef __cplusplus
}
#endif

#endif /* __ARCS_HINV_H__ */
