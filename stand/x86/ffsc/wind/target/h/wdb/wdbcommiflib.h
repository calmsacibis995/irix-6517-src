/* wdbCommIfLib.h - header file for the WDB agents communication interface */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,03jun95,ms	changed read/write to sendto/rcvfrom
01b,05apr95,ms  new data types.
01a,23sep94,ms  written.
*/

#ifndef __INCwdbCommIfLibh
#define __INCwdbCommIfLibh

/* includes */

#include "wdb/wdb.h"
#include "netinet/in.h"

/* definitions */

#define WDB_COMM_MODE_POLL      1
#define WDB_COMM_MODE_INT       2

/* data types */

/*
 * WDB_COMM_IF - interface used by the agent to talk to the host.
 */

typedef void * WDB_COMM_ID;

typedef struct wdb_comm_if
    {
    WDB_COMM_ID	      commId;
    int (*rcvfrom)    (WDB_COMM_ID commId, caddr_t addr, uint_t len,
			struct sockaddr_in *pAddr, struct timeval *tv);
    int (*sendto)     (WDB_COMM_ID commId, caddr_t addr, uint_t len,
			struct sockaddr_in *pAddr);
    int (*modeSet)    (WDB_COMM_ID commId, uint_t newMode);
    int (*cancel)     (WDB_COMM_ID commId);
    int (*hookAdd)    (WDB_COMM_ID commId, void (*rout)(), uint_t arg);
    int (*notifyHost) (WDB_COMM_ID commId);
    } WDB_COMM_IF;

/*
 * WDB_DRV_IF - interface between a driver and the agents pseudo-UDP/IP stack
 *
 * The default agent configuration uses the native VxWorks UDP/IP stack which
 * has its own interface to a driver (a net_if structure).
 * The agent can also be configured to use a pseudo-UDP/IP stack that
 * is independant of the OS. The pseudo-UDP/IP stack is used for
 * communication with the system mode agent.
 * The WDB_DRV_IF structure allows new drivers to be installed in the
 * pseudo-UDP/IP stack.
 */

typedef struct wdb_drv_if
    {
    uint_t	mode;			/* available device modes */
    uint_t  	mtu;			/* maximum transmit packet size */ 
    void    	(*stackRcv)();		/* callback when pkt arrives */

    /* driver routines */

    void *   	devId;			/* device ID */
    STATUS	(*pollRtn)();		/* poll device for data */
    STATUS 	(*pktTxRtn)();		/* transmit a packet */
    STATUS 	(*modeSetRtn)();	/* toggle driver mode */
    int		(*ioctl)();		/* misc */
    } WDB_DRV_IF;

#endif  /* __INCwdbCommIfLibh */

