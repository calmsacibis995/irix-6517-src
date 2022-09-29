/* i960Dma.h - Intel i960 DMA definitions */

/* Copyright 1984-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01b,12feb93,ccc  added cplusplus and ASMLANUGAGE.
01a,22jan93,ccc  created for TP960V SCSI support
*/

/*
This file contains definitions associated with the i960's on-board DMA
controller, and the data structures used by the DMA interface routines
in "sysALib.s".  See Intel's "I960CA User's Manual", Chapter 13 - but beware,
there seem to be many typos.

These definitions (and the routines in "sysALib.s") are hardware-independent
and should really be in architecture-wide places rather than the BSP.
*/

#ifndef	INCi960Dmah
#define	INCi960Dmah

#ifdef	__cplusplus
extern	"C" {
#endif

#ifndef	_ASMLANGUAGE

#include "vxworks.h"

/*
 *  Bit positions and masks in DMA Command Register (DMAC)
 *
 *  Note: 'chan' must be in range 0 to 3
 */

#define	DMAC_CHAN_ENABLE_BIT(chan)  	((chan) +  0)
#define	DMAC_CHAN_TERM_BIT(chan)  	((chan) +  4)
#define	DMAC_CHAN_ACTIVE_BIT(chan)  	((chan) +  8)
#define	DMAC_CHAN_DONE_BIT(chan)  	((chan) + 12)
#define	DMAC_CHAN_WAIT_BIT(chan)  	((chan) + 16)

#define DMAC_CHAN_ENABLE(chan)	    	(1 << DMAC_CHAN_ENABLE_BIT (chan))
#define DMAC_CHAN_TERM(chan)	    	(1 << DMAC_CHAN_TERM_BIT   (chan))
#define DMAC_CHAN_ACTIVE(chan)	    	(1 << DMAC_CHAN_ACTIVE_BIT (chan))
#define DMAC_CHAN_DONE(chan)	    	(1 << DMAC_CHAN_DONE_BIT   (chan))
#define DMAC_CHAN_WAIT(chan)	    	(1 << DMAC_CHAN_WAIT_BIT   (chan))

#define DMAC_PRIORITY	    	    	0x00100000
#define	DMAC_THROTTLE   	    	0x00200000


/* Bit masks in DMA Control Word */

#define	DMACW_XFER_8_TO_8   	    	 0  	/* transfer mode specs   */
#define DMACW_XFER_8_TO_16  	    	 1
#define DMACW_XFER_RESERVED_1	    	 2
#define DMACW_XFER_8_TO_32  	    	 3

#define	DMACW_XFER_16_TO_8   	    	 4
#define DMACW_XFER_16_TO_16  	    	 5
#define DMACW_XFER_RESERVED_2	    	 6
#define DMACW_XFER_16_TO_32  	    	 7

#define	DMACW_XFER_8_FLYBY  	    	 8
#define DMACW_XFER_16_FLYBY 	    	 9
#define DMACW_XFER_128_FLYBY_QUAD   	10
#define DMACW_XFER_32_FLYBY 	    	11

#define	DMACW_XFER_32_TO_8   	    	12  	/* Intel says 32 to 16 ?? */
#define DMACW_XFER_32_TO_16  	    	13
#define DMACW_XFER_128_TO_128_QUAD  	14
#define DMACW_XFER_32_TO_32  	    	15

#define	DMACW_XFER_MASK	    	0x0000000F  	/* select one of the above   */

#define DMACW_DST_HOLD	    	0x00000010  	/* hold / increment dst addr */
#define DMACW_DST_INCR	    	0x00000000

#define	DMACW_SRC_HOLD	    	0x00000020  	/* hold / increment src addr */
#define DMACW_SRC_INCR	    	0x00000000

#define DMACW_SYNC_DST	    	0x000000C0  	/* sync w. dst / src / none  */
#define DMACW_SYNC_SRC	    	0x00000080  	/* (NB - Intel docs reverse  */
#define DMACW_SYNC_NONE	    	0x00000000  	/* src/dst and sync/none.)   */

#define	DMACW_TERM_COUNT    	0x00000100  	/* end on count / EOP input  */
#define DMACW_TERM_EOP	    	0x00000000

#define DMACW_DST_CHAIN	    	0x00000200  	/* dst chaining when set     */

#define DMACW_SRC_CHAIN	    	0x00000400  	/* src chaining when set     */

#define DMACW_CHAIN_INT	    	0x00000800  	/* intr on chain when set    */

#define DMACW_CHAIN_WAIT    	0x00001000  	/* wait on chain when set    */


/* Parameters for "sysDmaChanSetup()" */

typedef struct dmaParams
    {
    UINT32              count;		/* byte count to be transferred   */
    volatile UINT8 *    srcAddr;	/* initial source address         */
    volatile UINT8 *    dstAddr;	/* initial destination address    */
    struct  dmaParams * next;		/* link to next transfer in chain */
    } DMA_PARAMS;

#endif	/* _ASMLANGUAGE */

#ifdef	__cplusplus
}
#endif

#endif	/* INCi960Dmah */
