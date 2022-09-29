/* Stream module ID and M_CTL registration
 *
 *	AT&T does not seem to care what numbers anything uses.
 *	This is an SGI hack to reduce chaos.
 *
 * $Revision: 3.19 $
 */

#ifndef _SYS_STRIDS_H
#define _SYS_STRIDS_H


/* SGI stream module IDs
 */
#define STRID_DUART	9001		/* stream duart driver */
#define STRID_STTY_LD	9002		/* stream line discipline */
#define STRID_WIN	9003		/* stream windows */
#define STRID_PTC	9004		/* stream control pty */
#define STRID_PTS	9005		/* stream slave pty */
#define	STRID_CD3608	9006		/* stream central data serial driver */
#define	STRID_ISI16	9007		/* stream ISI 16 port serial driver */
#define STRID_PROM	9008		/* stream PROM console driver */
#define	STRID_GM	9009		/* stream GM control port */
#define	STRID_WM_QUEUE	9010		/* stream window mgr queue module */
#define STRID_IF_CY	9011		/* cypress network interface */
#define	STRID_TPORT	9012		/* iris textport driver */
#define STRID_IF_SL	9013		/* SLIP module */
#define STRID_CONSOLE	9014		/* console module */
#define STRID_AUDIO	9015		/* stream audio */
#define	STRID_SNATR	9016		/* stream Token Ring SNA module */
#define STRID_TRLINK	9017		/* stream Token Ring pseudo driver */
/* SVR4 compatibility modules */
#define STRID_LDTERM	9018		/* stream ldterm module */
#define STRID_PTEM	9019		/* stream PTEM module */
#define STRID_SYNC	9020		/* digital media sync driver */
#define STRID_DRVLOAD	9021		/* unloaded streams driver */
#define STRID_STRLOAD	9022		/* unloaded streams module */
/* ISDN */
#define STRID_ISDN	9023		/* isdn loopback and debug driver */
#define STRID_PHLME	9024		/* isdn physical layer mang. ent. */
#define STRID_ISDNSME	9025		/* isdn system mang. ent. */
#define STRID_LAPD	9028		/* isdn lapd dlpi driver */
#define STRID_SPLI	9029		/* isdn streams-pli driver */
#define STRID_Q931MUX	9030		/* isdn q931 appl multiplexor */
#define STRID_ISDNPASS	9031		/* isdn message pass-through module */
#define STRID_Q931PC	9032		/* isdn q931 protocol control module */
#define STRID_Q931CC	9033		/* isdn q931 call control module */
#define STRID_ISDNASI	9034		/* isdn ASI module */

#define STRID_PPP	9035		/* PPP module */
#define STRID_PPP_FRAME	9036		/* PPP framer */
#define STRID_LLC2	9037		/* DLPI llc2 driver */
#define STRID_SNIF	9038		/* DLPI snif driver */
#define STRID_SDLC	9039		/* SDLC driver */
#define STRID_WAN_PPP	9040		/* WAN sync bd to PPP glue module */
#define STRID_SIO	9041		/* serial i/o driver */
#define STRID_PRI	9042		/* PRI ISDN driver */

/* IDs for M_CTL messages
 *	One of these ints should be the first 4 bytes of an M_CTL message
 */
#define STRCTL__			/* none defined yet */

#endif /* _SYS_STRIDS_H */

