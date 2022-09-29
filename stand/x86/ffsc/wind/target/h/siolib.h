/* sioLib.h - header file for binary interface serial drivers */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,14jan97,db  added SIO_OPEN and SIO_HUP for modem control(SPR #7637).
01d,24oct95,ms  removed "static __inline__" (SPR #4500)
01c,15jun95,ms  Added SIO_MODE_SET, SIO_MODE_GET, SIO_AVAIL_MODES_GET iotcl's
		Renamed SIO_CHAN to SIO_CHAN
		Changed prototype and name of callbackInstall()
		Added documentation.
01b,22may95,ms  removed unneeded include file.
01a,21feb95,ms  written.
*/

#ifndef __INCsioLibh
#define __INCsioLibh

#include "types/vxtypes.h"
#include "types/vxtypesold.h"

#ifdef __cplusplus
extern "C" {
#endif

/* serial device I/O controls */

/*
#define SIO_RESET		0x1001
*/

#define SIO_BAUD_SET		0x1003
#define SIO_BAUD_GET		0x1004

#define SIO_HW_OPTS_SET		0x1005
#define SIO_HW_OPTS_GET		0x1006

#define SIO_MODE_SET		0x1007
#define SIO_MODE_GET		0x1008
#define SIO_AVAIL_MODES_GET	0x1009

/* The following macros are used to open/close modem connection */

#define SIO_OPEN		0x100A 
#define SIO_HUP			0x100B

/* the following macros control the receive buffer hardware flow control */
#define	SIO_RTS_SUSPEND	0x100C
#define	SIO_RTS_RESUME		0x100D

/* callback types */

#define SIO_CALLBACK_GET_TX_CHAR	1
#define SIO_CALLBACK_PUT_RCV_CHAR	2

/* options to SIO_MODE_SET */

#define	SIO_MODE_POLL	1
#define	SIO_MODE_INT	2

/* options to SIOBAUDSET */
/* use the number, not a macro */

/* options to SIO_HW_OPTS_SET (ala POSIX), bitwise or'ed together */

#define CLOCAL		0x1	/* ignore modem status lines */
#define CREAD		0x2	/* enable device reciever */

#define CSIZE		0xc	/* bits 3 and 4 encode the character size */
#define CS5		0x0	/* 5 bits */
#define CS6		0x4	/* 6 bits */
#define CS7		0x8	/* 7 bits */
#define CS8		0xc	/* 8 bits */

#define HUPCL		0x10	/* hang up on last close */
#define STOPB		0x20	/* send two stop bits (else one) */
#define PARENB		0x40	/* parity detection enabled (else disabled) */
#define PARODD		0x80	/* odd parity  (else even) */

/* serial device data structures */

typedef struct sio_drv_funcs SIO_DRV_FUNCS;

typedef struct sio_chan				/* a serial channel */
    {
    SIO_DRV_FUNCS * pDrvFuncs;
    /* device data */
    } SIO_CHAN;

struct sio_drv_funcs				/* driver functions */
    {
    int		(*ioctl)
			(
			SIO_CHAN *	pSioChan,
			int		cmd,
			void *		arg
			);

    int		(*txStartup)
			(
			SIO_CHAN *	pSioChan
			);

    int		(*callbackInstall)
			(
			SIO_CHAN *	pSioChan,
			int		callbackType,
			STATUS		(*callback)(),
			void *		callbackArg
			);

    int		(*pollInput)
			(
			SIO_CHAN *	pSioChan,
			char *		inChar
			);

    int		(*pollOutput)
			(
			SIO_CHAN *	pSioChan,
			char 		outChar
			);
    };

/* macros */

#define sioIoctl(pSioChan, cmd, arg) ((pSioChan)->pDrvFuncs->ioctl (pSioChan, cmd, arg))
#define sioTxStartup(pSioChan)    ((pSioChan)->pDrvFuncs->txStartup (pSioChan))
#define sioCallbackInstall(pSioChan, callbackType, callback, callbackArg) \
	((pSioChan)->pDrvFuncs->callbackInstall (pSioChan, callbackType, \
			callback, callbackArg))
#define sioPollInput(pSioChan, inChar) ((pSioChan)->pDrvFuncs->pollInput (pSioChan, inChar))
#define sioPollOutput(pSioChan, thisChar) ((pSioChan)->pDrvFuncs->pollOutput (pSioChan, thisChar))

#ifdef __cplusplus
}
#endif

#endif  /* __INCsioLibh */

