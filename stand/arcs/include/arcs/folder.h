/*
 * ARCS EISA folder structures and definitions
 */

#ifndef _ARCS_FOLDER_H
#define _ARCS_FOLDER_H

#ident "$Revision: 1.9 $"

#include <arcs/large.h>
#include <arcs/hinv.h>

/* The EISA Callback vector */
typedef struct eisacallback {
    STATUS	(*ProcessEndOfInterrupt) ();
    BOOLEAN	(*TestEISAInterrupt) ();
    STATUS	(*RequestEISADMATransfer) ();
    STATUS	(*AbortEISADMATransfer) ();
    STATUS	(*GetEISADMATransferStatus) ();
    VOID	(*DoEISALockedOperation) ();
    STATUS	(*TranslateCPUAddressToBusAddress) ();
    STATUS      (*ReleaseTranslatedCPUAddress) ();
    STATUS      (*TranslateBusAddressToCPUAddress) ();
    STATUS      (*ReleaseTranslatedBusAddress) ();
    VOID	(*FlushCache) ();
    VOID	(*InvalidateCache) ();
    STATUS	(*GetCounterAddress) ();
    VOID	(*BeginCriticalSection) ();
    VOID	(*EndCriticalSection) ();
    STATUS	(*GenerateTone) ();
    VOID	(*FlushWriteBuffer) ();
    BOOLEAN	(*Yield) ();
} EISACallBack;

/* The EISA Adapter Details structure */
typedef struct adapterdetails {
    ULONG      NumberOfSlots;
    PVOID      IOStart;
    ULONG      IOSize;
} EISA_ADAPTER_DETAILS, *PEISA_ADAPTER_DETAILS;


/* Driver Callback structure - passed to DriverInstall */
typedef struct eisadrivercallback {
    PVOID       (*AllocateMemory)();
    STATUS      (*FreeMemory)();
    STATUS      (*RegisterDriverStrategy)();
} EISA_DRIVER_CALLBACK, *PEISA_DRIVER_CALLBACK;

/* 
 * The folder DriverInstall function prototype
 */
typedef STATUS (DRIVERINSTALL), (*PDRIVERINSTALL)
       ( ULONG, COMPONENT *, PULONG, ULONG, PEISA_DRIVER_CALLBACK );

/* IOBlock function codes */
typedef enum {
    FC_INITIALIZE,
    FC_OPEN,
    FC_CLOSE,
    FC_READ,
    FC_WRITE,
    FC_GETEXTERRORNUMBER,
    FC_GETREADSTATUS,
    FC_POLL,
    FC_RESET,
    FC_GET_DEV_INFO,
    FC_MOUNT,
    FC_GETBUFFER,
    FC_FREEBUFFER,
    FC_REGRECEIVE,
    FC_DEREGRECEIVE,
    FC_DO_SCSI_IO,
    /* SGI extensions */
    FC_IOCTL,
    FC_GETFILEINFO
} FUNCTIONCODE;

/* ARCS-specific iob */

typedef	struct ioblock {
    LONG	FunctionCode;
    LONG	Unit;		/* device number */
    LONG	Partition;	/* disk partition number */
    PVOID	Address;	/* memory address of user i/o buffer */
    LONG	Count;		/* byte count for transfer */
    LARGE	Offset;		/* 64 bit seek value -- must be positive! */
    LONG	StartBlock;
    STATUS	ErrorNumber;
    LONG	Controller;	/* controller number */
    PVOID 	DevPtr;		/* driver dependent info */
    LONG	Flags;
} IOBLOCK, *PIOBLOCK;

/* Piggyback ioctl arguments on top of existing structure elements
 */
#define IoctlCmd	Address
#define IoctlArg	Count
#define IOCTL_CMD	PVOID
#define IOCTL_ARG	LONG

/* The driverStrategy function prototype */
typedef STATUS (DRIVERSTRATEGY), (*PDRIVERSTRATEGY)
        ( COMPONENT *, PIOBLOCK );

/* Diag callback structure - passed to DiagInstall */
typedef struct diag_callback {
    STATUS    (*DiagStrategy) ();
} DIAG_CALLBACK, *PDIAG_CALLBACK;

/* The folder DiagInstall function prototype */
typedef STATUS (DIAGINSTALL), (*PDIAGINSTALL)
        ( ULONG, COMPONENT *, LONG, PDIAG_CALLBACK );

/* DIAGBlock function codes - some already defined in FUNCTIONCODE above */
typedef enum {
    /* FC_INITIALIZE, */
    /* RESERVED, */
    FC_SELFTEST_LEVEL_1 = 2,
    FC_QUERY,
    FC_INVOKE,
    /* FC_GETEXTERRORNUMBER, */
    FC_QUIESCE = 6
} DIAGCODE;

/* The DIAGBlock structure */
typedef struct diagblock {
    LONG       FunctionCode;
    LONG       TestNumber;
    PVOID      Address;
    LONG       Count;
    STATUS     ErrorNumber;
} DIAGBLOCK, *PDIAGBLOCK;

/* The EXT_ERROR_NUMBER structure */
typedef struct ext_error_number
    {
    ULONG   ErrorCode;
    ULONG   ErrorClass;
    ULONG   ErrorAction;
    ULONG   ErrorLocus;
    } EXT_ERROR_NUMBER, *PEXT_ERROR_NUMBER;

/* The diagStrategy function prototype */
typedef STATUS (DIAGSTRATEGY), (*PDIAGSTRATEGY)
        ( COMPONENT *, PDIAGBLOCK );

#endif
