/*
 *  dsreq.h -- /dev/scsi user interface definitions and structures
 *
 *	Copyright 1988, 1989, by
 *	Gene Dronek (Vulcan Laboratory) and
 *	Rich Morin  (Canta Forda Computer Laboratory).
 *	All Rights Reserved.
 *	Ported to SGI by Dave Olson 3/89
 *
 *	This file is used by the kernel and by user programs
 */
#ident "sys/dsreq.h: $Revision: 1.24 $"

/*
 *	These implementation constants are the only system-dependent
 *	things that should be in this file!
 */

#define I_MAX SCSI_MAXTARG	/* number ids per bus  		      */
#define L_MAX 8			/* number luns per id  		      */
#define R_MAX 1			/* 1=>init role only, 2=>both roles   */
#define V_MAX 1			/* max number dma io vectors          */

#include <sys/types.h>	/* primarily for user programs */

/*	
 * 	DEVSCSI ioctl request codes.  
 *      Ioctl commands for various devscsi actions are created here.
 */
#include <sys/ioctl.h>
#define D(y,t) _IOWR('D',y,t)

#define DS_ENTER  D(101, struct dsreq)	    /* enter a request       */
#define DS_CANCEL D(102, struct dsreq)	    /* cancel a request      */
#define DS_POLL   D(103, struct dsreq) 	    /* sample async request  */
#define DS_CONTIN D(104, struct dsreq)	    /* continue a request    */
#define DS_SET	  D(106, int)		    /* set permanent flags (per device)   */
#define DS_CLEAR  D(107, int)	            /* clear permanent flags */
#define DS_GET	  D(108, int)		    /* get permanent flags  (per device)  */
#define DS_CONF	  D(110, int)		    /* get flags supported by host */
#define DS_ABORT  D(111, int) 		  /* send abort message to "idle" target.
	Only useful if no commands active on the target; as it otherwise
	delays until no commands are active.  Commands like rewind immediate
	and other commands with the immediate bit can be aborted in this
	way */
#define DS_MKALIAS	D(112, int)

#if _KERNEL
#define IRIX5_DS_ENTER  D(101, struct irix5_dsreq)   /* enter a request       */
/* The other dsreq'd DS ioctls aren't used. */
#endif /* _KERNEL */


/* flag values returned by DS_GET (for SGI, these may vary from system
 * to system, as they are inherently system specific) */
#define DSG_CANTSYNC    0x01	/* sync xfer negotiations attempted, but 
	* device doesn't support sync; BADSYNC will be set if the negotation
	* itself also failed */
#define DSG_SYNCXFR     0x02	/* sync xfer mode in effect; async if not set;
	* async mode is the default for all devscsi devices; see also
	* DSRQ_SYNXFR and DSRQ_ASYNXFR bits in ds_flags. */
#define DSG_TRIEDSYNC   0x04	/* did sync negotations at some point.  If
	* DSG_CANTSYNC is not also set, the device supports sync mode. */
#define DSG_BADSYNC     0x08	/* device doesn't handle sync negotations
	* correctly; DSG_CANTSYNC will normally be set whenever this bit
	* is set */
#define DSG_NOADAPSYNC  0x10	/* scsi adapter  doesn't handle sync
	* negotations or system has been configured to not allow sync xfers
	* on this target */
#define DSG_WIDE  0x20	/* scsi adapter supports wide SCSI */
#define DSG_DISC  0x40	/* scsi adapter supports disconnect and is configured
	* for it */
#define DSG_TAGQ  0x80	/* scsi adapter supports tagged queuing and is
	* configured for it */

/*
 *	DEVSCSI dma scatter-gather control.
 *	Dma control is system-dependent; its implementation is optional
 */
typedef struct dsiovec {
    caddr_t  iov_base;		/* i/o base */
    int      iov_len;		/* i/o length */
} dsiovec_t;

#if _KERNEL
struct irix5_dsiovec {
    app32_ptr_t  iov_base;		/* i/o base */
    int		iov_len;		/* i/o length */
};
#endif /* _KERNEL */



/*
 * 	User DEVSCSI ioctl structure.
 *	This structure is passed and returned as the ioctl argument.
 */

typedef struct dsreq {

/* 	devscsi prefix 					        */
  uint    	ds_flags;	/* see flags defined below 	*/
  uint     	ds_time; 	/* timeout in milliseconds   	*/
  __psint_t   	ds_private;	/* for private use by caller 	*/

/*	scsi request	  				        */
  caddr_t  	ds_cmdbuf;	/* command buffer 		*/
  uchar_t    	ds_cmdlen;	/* command buffer length 	*/
  caddr_t	ds_databuf;	/* data buffer start 		*/
  uint		ds_datalen;	/* total data length 		*/
  caddr_t       ds_sensebuf;	/* sense buffer 		*/
  uchar_t   	ds_senselen;	/* sense buffer length 		*/

/*	miscellaneous 		      				*/
  dsiovec_t    *ds_iovbuf;	/* scatter-gather dma control   */
  ushort   	ds_iovlen;	/* length of scatter-gather	*/
  struct dsreq *ds_link;	/* for linked requests          */
  ushort	ds_synch;	/* synchronous xfer control 	*/
  uchar_t	ds_revcode;	/* devscsi version code 	*/

/*	return portion 						*/
  uchar_t       ds_ret;		/* devscsi return code 		*/
  uchar_t       ds_status;	/* device status byte value 	*/
  uchar_t       ds_msg;		/* device message byte value 	*/
  uchar_t       ds_cmdsent;	/* actual length command 	*/
  uint          ds_datasent;	/* actual length user data 	*/
  uchar_t       ds_sensesent;	/* actual length sense data 	*/
} dsreq_t;

#if _KERNEL
struct irix5_dsreq {

/* 	devscsi prefix 					        */
  uint    	ds_flags;	/* see flags defined below 	*/
  uint     	ds_time; 	/* timeout in milliseconds   	*/
  app32_ptr_t  	ds_private;	/* for private use by caller 	*/

/*	scsi request	  				        */
  app32_ptr_t  	ds_cmdbuf;	/* command buffer 		*/
  uchar_t    	ds_cmdlen;	/* command buffer length 	*/
  app32_ptr_t	ds_databuf;	/* data buffer start 		*/
  uint		ds_datalen;	/* total data length 		*/
  app32_ptr_t   ds_sensebuf;	/* sense buffer 		*/
  uchar_t   	ds_senselen;	/* sense buffer length 		*/

/*	miscellaneous 		      				*/
  app32_ptr_t   ds_iovbuf;	/* scatter-gather dma control   */
  ushort   	ds_iovlen;	/* length of scatter-gather	*/
  app32_ptr_t   ds_link;	/* for linked requests          */
  ushort	ds_synch;	/* synchronous xfer control 	*/
  uchar_t	ds_revcode;	/* devscsi version code 	*/

/*	return portion 						*/
  uchar_t       ds_ret;		/* devscsi return code 		*/
  uchar_t       ds_status;	/* device status byte value 	*/
  uchar_t       ds_msg;		/* device message byte value 	*/
  uchar_t       ds_cmdsent;	/* actual length command 	*/
  uint          ds_datasent;	/* actual length user data 	*/
  uchar_t       ds_sensesent;	/* actual length sense data 	*/
};
#endif /* _KERNEL */




/*
 * 	DEVSCSI ds_flags option bits.
 *	These bits change the course of the devscsi bus transaction.
 */

/* 	devscsi options 				 	     */
#define DSRQ_ASYNC   0x1 	/* no (yes) sleep until request done */
#define DSRQ_SENSE   0x2	/* yes (no) auto get sense on status */
#define DSRQ_TARGET  0x4	/* target (initiator) role           */

/* 	select options  					     */
#define DSRQ_SELATN  0x8	/* select with (without) atn         */
#define DSRQ_DISC    0x10	/* identify disconnect (not) allowed */
#define DSRQ_SYNXFR  0x20	/* negotiate synchronous scsi xfer */
#define DSRQ_ASYNXFR  0x200000	/* negotiate asynchronous scsi xfer (this
	* is the default, and so is only needed if a transition back to
	* async scsi is necessary after using DSRQ_SYNXFR) */
#define DSRQ_SELMSG  0x40	/* send supplied (generate) message  */

/*	data transfer options            			     */
#define DSRQ_IOV     0x80       /* scatter-gather (not) specified    */
#define DSRQ_READ    0x100	/* input data from scsi bus          */
#define DSRQ_WRITE   0x200	/* output data to  scsi bus          */
#define DSRQ_BUF     0x400	/* buffered (direct) data xfer       */

/*	progress/continuation callbacks      			     */
#define DSRQ_CALL    0x800 	/* notify progress (completion-only) */
#define DSRQ_ACKH    0x1000	/* hold (don't) ACK asserted         */
#define DSRQ_ATNH    0x2000	/* hold (don't) ATN asserted         */
#define DSRQ_ABORT   0x4000	/* abort msg until bus clear         */

/*	host options (likely non-portable) 			     */
#define DSRQ_TRACE   0x8000 	/* trace (no) this request           */
#define DSRQ_PRINT   0x10000 	/* print (no) this request           */
#define DSRQ_CTRL1   0x20000	/* request with host control bit 1   */
#define DSRQ_CTRL2   0x40000	/* request with host control bit 2   */

/*	additional flags					     */
#define DSRQ_MIXRDWR   0x100000	/* request can both read and write   */
/* #define DSRQ_ASYNXFR  0x200000 : defined above; duplicated here 
 * so this value is shown as used in numeric sequence */



/*
 *	DEVSCSI ds_ret return codes.
 *	All error and 'abnormal' codes are non-zero; 0 is
 *	returned in ds_ret on success.  Not all codes are returned
 *	by all implementations.
 */

/*	devscsi software returns 				      */
#define DSRT_DEVSCSI 0x47	/* general devscsi failure            */
#define DSRT_MULT    0x46	/* request rejected                   */
#define DSRT_CANCEL  0x45	/* lower request cancelled            */
#define	DSRT_REVCODE 0x44	/* software obsolete must recompile   */
#define	DSRT_AGAIN   0x43	/* try again, recoverable bus error   */

/*	host scsi software returns 				      */
#define	DSRT_HOST    0x37	/* general host failure               */
#define	DSRT_NOSEL   0x36	/* no unit responded to select        */
#define DSRT_SHORT   0x35	/* incomplete xfer (not an error)     */
#define DSRT_OK      0x34	/* complete  xfer w/ no error status  */
#define	DSRT_SENSE   0x33	/* cmd w/ status, sense data gotten   */
#define	DSRT_NOSENSE 0x32	/* cmd w/ status, error getting sense */
#define	DSRT_TIMEOUT 0x31	/* request idle longer than requested */
#define DSRT_LONG    0x30	/* target overran data bounds         */

/*	protocol failures		                              */
#define DSRT_PROTO   0x27	/* misc. protocol failure             */
#define DSRT_EBSY    0x26	/* busy dropped unexpectedly	      */
#define	DSRT_REJECT  0x25	/* message reject                     */
#define DSRT_PARITY  0x24	/* parity error on SCSI bus           */
#define DSRT_MEMORY  0x23	/* host memory error                  */
#define DSRT_CMDO    0x22	/* error during command phase         */
#define DSRT_STAI    0x21	/* error during status  phase         */
#define DSRT_UNIMPL  0x20	/* not implemented 		      */


/*
 *  SCSI device status byte codes returned in ds_status:
 */
#define  STA_GOOD	0x00	/* scsi cmd finished normally  */
#define  STA_CHECK	0x02	/* scsi cmd aborted, check     */
#define  STA_BUSY	0x08	/* scsi cmd aborted, unit busy */
#define  STA_IGOOD	0x10	/* scsi cmd with link finished */
#define  STA_RESERV	0x18	/* scsi unit aborted, reserved */


/*
 *  SCSI device message byte codes returned in ds_msg:
 */
#define  MSG_COMPL	0x00	/* command complete */
#define  MSG_XMSG	0x01	/* extended message */
#define  MSG_SAVEP	0x02	/* save pointers */
#define  MSG_RESTP	0x03	/* restore pointers */
#define  MSG_DISC	0x04	/* disconnect */
#define  MSG_IERR	0x05	/* initiator error */
#define  MSG_ABORT	0x06	/* abort */
#define  MSG_REJECT	0x07	/* message reject */
#define  MSG_NOOP	0x08	/* no operation */
#define  MSG_MPARITY	0x09	/* message parity */
#define  MSG_LINK	0x0A	/* linked command completed */
#define  MSG_LINKF	0x0B	/* linked w/flag completed */
#define  MSG_BRESET	0x0C	/* bus device reset */
#define  MSG_IDENT	0x80	/* 80 thru FF Identify */


typedef  struct  dsconf  {
  uint	  dsc_flags;		/* DSRQ flags implemented 	  */
  uint	  dsc_preset;		/* DSRQ flags always defaulted on */
  u_char  dsc_bus;		/* number scsi busses configured  */
  u_char  dsc_imax;		/* number ID-s per bus		  */
  u_char  dsc_lmax;		/* number LUN-s per ID		  */
  uint	  dsc_iomax;		/* maximum length i/o on this system */
  uint	  dsc_biomax;		/* maximum length buffered i/o	  */
} dsconf_t;

/*
 *	DEVSCSI public access macros.  We use macros internally to
 *	access the fields of the dsreq packet.  This facilitates
 *	future additions or changes to the underlying packet.
 */

/* devscsi prefix */
#define FLAGS(dp)      	((dp)->ds_flags)
#define TIME(dp)      	((dp)->ds_time)
#define PRIVATE(dp)    	((dp)->ds_private)

/* base addresses */
#define CMDBUF(dp)      (dp)->ds_cmdbuf
#define DATABUF(dp)     (dp)->ds_databuf
#define IOVBUF(dp)      ((caddr_t) (dp)->ds_iovbuf)
#define SENSEBUF(dp)    (dp)->ds_sensebuf

/* item lengths */
#define CMDLEN(dp)      ((dp)->ds_cmdlen)
#define DATALEN(dp)     ((dp)->ds_datalen)
#define IOVLEN(dp)      ((dp)->ds_iovlen)
#define SENSELEN(dp)    ((dp)->ds_senselen)

/* devscsi returns */
#define CMDSENT(dp)     ((dp)->ds_cmdsent)
#define DATASENT(dp)    ((dp)->ds_datasent)
#define SENSESENT(dp)   ((dp)->ds_sensesent)
#define STATUS(dp)      ((dp)->ds_status)
#define RET(dp)      	((dp)->ds_ret)
#define MSG(dp)      	((dp)->ds_msg)

/* decompose minimal request-sense return struct */
#define SENSEKEY(p)     (((uchar_t *)(p))[2] & 0xF)
#define ADDSENSECODE(p) (((uchar_t *)(p))[12])
#define FIELDPTR(p)     V2( & ((uchar_t *)(p)) [16] )
#define INFO(p)         V4( & ((uchar_t *)(p)) [3] )


/* additional runtime filler functions */
#define FILLCMD(dp,b,l)   ( CMDBUF(dp) = (b), CMDLEN(dp)=(l) )
#define FILLIOV(iov,i,b,l) { (iov)[i].iov_base = (b); (iov)[i].iov_len = (l); }
#define FILLSENSE(dp,b,l) ( SENSEBUF(dp) = (b), SENSELEN(dp)=(l) )


/* macros for building scsi msb array parameter lists */
#define B(s,i) ((uchar_t)((s) >> i))
#define B1(s)                           ((u_char)(s))
#define B2(s)                       B((s),8),   B1(s)
#define B3(s)            B((s),16), B((s),8),   B1(s)
#define B4(s) B((s),24), B((s),16), B((s),8),   B1(s)


/* macros for converting scsi msb array to binary */
#define V1(s)       (s)[0]
#define V2(s)     (((s)[0] << 8) | (s)[1])
#define V3(s)   (((((s)[0] << 8) | (s)[1]) << 8) | (s)[2])
#define V4(s) (((((((s)[0] << 8) | (s)[1]) << 8) | (s)[2]) << 8) | (s)[3])


/* macros for converting binary into scsi msb array */
#define MSB1(s,v)                                               *(s)=B1(v)
#define MSB2(s,v)                                *(s)=B(v,8), (s)[1]=B1(v)
#define MSB3(s,v)               *(s)=B(v,16),  (s)[1]=B(v,8), (s)[2]=B1(v)
#define MSB4(s,v) *(s)=B(v,24),(s)[1]=B(v,16), (s)[2]=B(v,8), (s)[3]=B1(v)
