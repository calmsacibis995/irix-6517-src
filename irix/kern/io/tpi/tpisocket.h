/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved				       |
 * |-----------------------------------------------------------|
 * |     Restricted Rights Legend                              |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |     MIPS Computer Systems, Inc.                           |
 * |     950 DeGuigne Avenue                                   |
 * |     Sunnyvale, California 94088-3650, USA                 |
 * |-----------------------------------------------------------|
 */
/* $Revision: 1.19 $ */

/*
 *	tpisocket
 *		provide TPI interface above sockets-based network
 *		protocol stacks
 *
 *	tpisocket is a library module used by protocol-specific
 *	streams pseudo-drivers, such as tpitcp, tpiudp, and so on.
 *	Almost all of the functionality of such pseudo-drivers
 *	is present in tpisocket, with only protocol-specific data
 *	conversions and the actual streams module descriptions
 *	defined outside tpisocket.
 */

#ifndef _IO_TPI_TPISOCKET_H_
#define _IO_TPI_TPISOCKET_H_ 1

#include "sys/param.h"
#include "sys/errno.h"
#include "sys/stream.h"
#include "sys/stropts.h"
#include "sys/strids.h"
#include "sys/strmp.h"		/* for multiprocessor streams */
#include "sys/xti.h"		/* includes tiuser.h */
#include "sys/tihdr.h"
#include "sys/timod.h"
#include "sys/debug.h"
#include "bsd/net/soioctl.h"
#include "sys/mbuf.h"
#include "sys/socket.h"
#include "sys/socketvar.h"
#include "sys/protosw.h"
#include "sys/vsocket.h"
#include "sys/kmem.h"
#include "ksys/vfile.h"
#include "sys/uio.h"
#include "sys/cred.h"

#ifndef	_XTI_MODE
#define        _XTI_MODE 0x4381
#endif

/*
 *	tpisocket_control
 *
 *		per-protocol static description of relevant data
 *		and procedure addresses
 */

struct tpisocket_control {
	int	*tsc_nummajor_p;	/* address of count of major numbers */
	int	*tsc_majors;		/* array of major numbers */
	struct tpisocket ***tsc_devices_p; /* address of pointer to array of control blocks */
	int	*tsc_maxdevices_p;	/* address of count of devices */
	int	tsc_domain;		/* AF_* */
	int	tsc_socket_type;	/* SOCK_* */
	int	tsc_protocol;		/* IPPROTO_*, ... */
	int	(*tsc_address_size)(struct tpisocket *, int, struct socket *);
					/* size of an address */
	int	(*tsc_convert_address)(struct tpisocket *, char *, int, int,
			       	       struct mbuf *, struct socket *);
					/* get local address */
	int	tsc_flags;		/* miscellaneous flags */
	int	tsc_transport_type;	/* TLI T_* type */
	int	tsc_tsdu_size;
	int	tsc_etsdu_size;
	int	tsc_cdata_size;
	int	tsc_ddata_size;
	int	tsc_tidu_size;
	int	tsc_provider_flag;
	int	(*tsc_function)(struct tpisocket *, int, void *);
					/* miscellaneous functions */
	struct tpisocket_control *tsc_next; /* next registered protocol */	
};

#define TSC_NUMMAJOR (*(tpictl->tsc_nummajor_p))
#define TSC_MAJORS (tpictl->tsc_majors)
#define TSC_DEVICES (*(tpictl->tsc_devices_p))
#define TSC_MAXDEVICES (*(tpictl->tsc_maxdevices_p))
#define TSC_DOMAIN (tpictl->tsc_domain)
#define TSC_SOCKET_TYPE (tpictl->tsc_socket_type)
#define TSC_PROTOCOL (tpictl->tsc_protocol)
#define TSC_ADDRESS_SIZE(tpiso,code,nso) ((*tpictl->tsc_address_size)(tpiso,code,nso))
#define TSC_CONVERT_ADDRESS(tpiso,ptr,mode,size,m,nso) ((*tpictl->tsc_convert_address)(tpiso,ptr,mode,size,m,nso))
#define TSC_FLAGS (tpictl->tsc_flags)
#define TSC_TRANSPORT_TYPE (tpictl->tsc_transport_type)
#define TSC_TSDU_SIZE (tpictl->tsc_tsdu_size)
#define TSC_ETSDU_SIZE (tpictl->tsc_etsdu_size)
#define TSC_CDATA_SIZE (tpictl->tsc_cdata_size)
#define TSC_DDATA_SIZE (tpictl->tsc_ddata_size)
#define TSC_TIDU_SIZE (tpictl->tsc_tidu_size)
#define TSC_PROVIDER_FLAG (tpictl->tsc_provider_flag)
#define TSC_FUNCTION(tpiso,code,arg) ((*tpictl->tsc_function)(tpiso,code,arg))

/* TSC_FUNCTION codes */

#define TSCF_OPEN	1
#define TSCF_CLOSE	2
#define TSCF_UNBIND	3
#define TSCF_OPTMGMT	4
#define TSCF_DEFAULT_VALUE 5

#define TSC_OPEN(tpiso) TSC_FUNCTION(tpiso,TSCF_OPEN,NULL)
#define TSC_CLOSE(tpiso) TSC_FUNCTION(tpiso,TSCF_CLOSE,NULL)
#define TSC_UNBIND(tpiso) TSC_FUNCTION(tpiso,TSCF_UNBIND,NULL)
#define TSC_OPTMGMT(tpiso,more_to_do) TSC_FUNCTION(tpiso,TSCF_OPTMGMT,more_to_do)
#define TSC_DEFAULT_VALUE(tpiso,code) TSC_FUNCTION(tpiso,TSCF_DEFAULT_VALUE,(void *)(code))

/* TSC_OPTMGMT more_to_do codes */

#define TSCO_UNKNOWN	0		/* not supported */
#define TSCO_DONE 	1		/* done; make own ack */
#define TSCO_DONE_DO_ACK 2		/* done; req converted to ack */
#define TSCO_DONE_NO_ACK 3		/* done; release buffer */
#define TSCO_MORE	4		/* not done */
#define TSCO_DONE_XTI	5		/* XTI done; just clear pending flg */

/* TSC_DEFAULT_VALUE codes */

#define TSCD_SNDBUF 1
#define TSCD_RCVBUF 2
#define TSCD_SNDLOWAT 3
#define TSCD_RCVLOWAT 4

/* TSC_CONVERT_ADDRESS and TSC_ADDRESS_SIZE mode codes: */

#define TSCA_CODE	0x0FFF
#define TSCA_LOCAL_ADDRESS 1
#define TSCA_REMOTE_ADDRESS 2
#define TSCA_OPTIONS	3
#define TSCA_DEFAULT_ADDRESS 4
#define TSCA_ACTUAL_LOCAL_ADDRESS 5
#define TSCA_ACTUAL_REMOTE_ADDRESS 6
#define TSCA_ACTUAL_OPTIONS 7

/* TSC_CONVERT_ADDRESS mode flags: */

#define TSCA_MODE		0xF000
#define TSCA_MODE_READ		0x1000 /* fetch item */
#define TSCA_MODE_TO_SOCKETS	0x2000 /* convert streams to mbuf format (in m) */
#define TSCA_MODE_TO_STREAMS	0x4000 /* convert socket to streams format (from m) */
#define TSCA_MODE_WRITE		0x8000 /* store item */

/* TSC_FLAGS bit values: */

#define TSCF_LINK_SOCKET	0x0001	/* do not create socket; wait for I_LINK */
#define TSCF_NO_SEAMS		0x0002	/* don't ever have MORE_flags set */
#define TSCF_NO_ORDREL		0x0004	/* no orderly release for SOCK_STREAM */
#define TSCF_CONVERT_DISCON	0x0008  /* translate DISCON_IND reasons */
#define TSCF_DEFAULT_ADDRESS	0x0010  /* generate default address */
#define TSCF_NO_CONV_ADDRESS	0x0020  /* do not fixup address to sosend */
#define TSCF_ALLOW_SENDFD	0x0040	/* allow I_SENDFD */

/*
 *	tpisocket
 *
 *		per-open structure to record pointers to streams and
 *		sockets structures
 */

union tpisocket_buffer_p {
	struct mbuf 	*mbuf;
	mblk_t 		*mblk;
	};

struct tpisocket {
	queue_t		*tso_rq;	/* receive queue */
	queue_t		*tso_wq;	/* transmit queue */
	dev_t		tso_rdev;	/* external dev number for this open */
	u_long		tso_index;	/* index in devices table */

	u_long		tso_count;	/* reference count */
	struct vsocket 	*tso_vsocket; 	/* socket for this open */
	u_long		tso_flags;	/* status flags */
	struct tpisocket_control *tso_control; /* tpisocket descripter */

	union tpisocket_buffer_p tso_pending_recv; /* pending input message */
	union tpisocket_buffer_p tso_pending_recv_addr;
					/* address for pending input message */
	union tpisocket_buffer_p tso_pending_recv_control;
					/* control data for pending input    */
					/* message			     */
	int	tso_pending_recv_flags; /* MSG_* and TSOM_* flags for pending*/
					/* input message		     */

	union tpisocket_buffer_p tso_pending_send; /* pending output message */
	union tpisocket_buffer_p tso_pending_send_addr;
	union tpisocket_buffer_p tso_pending_send_control;
	mblk_t		*tso_pending_send_header;

	int		tso_pending_send_flags;
	int		tso_pending_send_error;	
	mblk_t		*tso_pending_ioctl_req;
	mblk_t		*tso_pending_request;

	cred_t		*tso_cred;
	struct mbuf 	*tso_temporary_mbuf;
	mblk_t		*tso_pending_optmgmt_req;
	struct mbuf 	*tso_pending_optmgmt_mbuf;

	caddr_t		tso_private;		/* private data for protocol */
	toid_t		tso_mbufcall_timeout;
	int		tso_pending_ioctl;
	u_int		tso_outcnt;	/* T_CONN_INDs w/o responses */
	int		tso_fd;		/* fd for sat */
	struct proc *   tso_procp;	/* owning process for sat */
	mblk_t		*tso_saved_conn_opts;	/* saved options for CONN_CON */
	/*
	 * If this is a listening endpoint, this is a monotonically increasing
	 * sequence number.  This is stored in the so_data field of the
	 * embryonic socket during T_CONN_IND/T_CONN_RES processing.  The
	 * assumption is that this won't collide with NFS's usage of so_data.
	 */
	int 		tso_connseq; 	/* connection sequence number */
	int		tso_state;	/* let's do this right */
	int		tso_abi;	/* ABI of application */
};

#define TSOF_ISOPEN		 0x00000001 /* currently open 		     */
#define TSOF_CLONE_OPEN 	 0x00000002 /* opened via clone device 	     */
#define TSOF_EXCLUSIVE		 0x00000004 /* exclusive open 		     */
#define TSOF_OPENING		 0x00000008 /* currently opening 	     */
#define TSOF_CLOSING		 0x00000010 /* currently closing 	     */
#define TSOF_OPEN_WAIT		 0x00000020 /* someone waiting for open      */
#define TSOF_RECV_PENDING 	 0x00000040 /* recv processing queued 	     */
#define TSOF_SEND_PENDING 	 0x00000080 /* send processing queue 	     */
#define TSOF_QUEUE_OUTPUT 	 0x00000100 /* queue all output 	     */
#define TSOF_IOCTL_PENDING 	 0x00000200 /* tpisocket ioctl op. pending   */
#define TSOF_CONN_REQ_PENDING 	 0x00000400 /* connection request pending    */
#define TSOF_OUTPUT_BUFCALL 	 0x00000800 /* output side bufcall pending   */
#define TSOF_INPUT_BUFCALL 	 0x00001000 /* output side bufcall pending   */
#define TSOF_RECV_ORDREL 	 0x00002000 /* orderly release passed upward */
#define TSOF_RECV_DISCON 	 0x00004000 /* disconnection passed upward   */
#define TSOF_UNUSED		 0x00008000 /* (can be accept target)        */

#define TSOF_DISCON_REQ_PENDING  0x00010000 /* disconnection request pending */
#define TSOF_ADDRESS_BOUND 	 0x00020000 /* local address bound           */
#define TSOF_OPTMGMT_REQ_PENDING 0x00040000 /* optmgmt request pending       */
#define TSOF_SO_IMASOCKET 	 0x00080000 /* SO_IMASOCKET defined          */
#define TSOF_WSRV_QUEUED 	 0x00100000 /* tso_wq qenabled               */
#define TSOF_RSRV_QUEUED         0x00200000 /* tso_rq qenabled               */
#define TSOF_INPUT_MBUFCALL      0x00400000 /* input mbuf wait               */
#define TSOF_OUTPUT_MBUFCALL     0x01000000 /* output mbuf wait              */
#define TSOF_OOB_INPUT_AVAILABLE 0x02000000 /* oob input queued              */
#define TSOF_CONN_REQ_FAILED     0x04000000 /* connection request failed     */
#define TSOF_XOPTMGMT_REQ_PNDNG  0x10000000 /* XTI optmgmt request pending   */
#define TSOF_XTI		 0x20000000 /* in XTI mode                   */

#define TSOF_ANY_MBUFCALL   (TSOF_INPUT_MBUFCALL   | TSOF_OUTPUT_MBUFCALL)
#define TSOF_SET_RECV_CHECK (TSOF_CONN_REQ_PENDING | TSOF_DISCON_REQ_PENDING)
				/* set of special rsrv states */

/* code in tso_pending_ioctl */
#define TSOI_NULL	0x0		/* none */
#define TSOI_TI_NAME	0x1	 	/* tpisocket_ti_name */
#define TSOI_IFIOCTL	0x2		/* tpisocket_ifioctl */
#define TSOI_RTIOCTL	0x4		/* tpisocket_rtioctl */

/* flags in tso_pending_recv_flags */
#define TSOM_MSG_PRESENT     0x10000
#define TSOM_MSG_CONVERTED   0x20000 /* tso_pending_recv is mblk */
#define TSOM_ADDR_CONVERTED  0x40000 /* tso_pending_recv_addr is mblk */
#define TSOM_CONTROL_CONVERTED 0x80000 /* tso_pending_recv_control is mblk */

#define TSOM_MSG_FLAGS_MASK  0x0FFFF

extern int tpisocket_open(queue_t *, dev_t *, int, int, cred_t *,
			      struct tpisocket_control *);
extern int tpisocket_rsrv(queue_t *);
extern int tpisocket_close(queue_t *, int, cred_t *);
extern int tpisocket_wput(queue_t *wq, mblk_t *);
extern int tpisocket_wsrv(queue_t *);
extern int tpisocket_address_size(struct tpisocket *, int, struct socket *);
extern int tpisocket_convert_address(struct tpisocket *, char *, int, int,
				struct mbuf *, struct socket *);
extern int tpisocket_optmgmt(struct tpisocket *, struct T_optmgmt_req *,
				struct opthdr *, mblk_t *);
extern int tpisocket_register(struct tpisocket_control *);
extern int tpisocket_makeopt( mblk_t *, int, int, void *, int);
extern int tpisocket_do_optmgmt( struct tpisocket *, int *, struct opproc *);

extern mblk_t *mbuf_to_mblk(struct mbuf *, int);
extern struct mbuf *mblk_to_mbuf(mblk_t *, int, int, int);

/*
 *	stream id declarations
 */

#ifndef STRID_TPITCP
#define STRID_TPITCP 9101	/* TPI TCP pseudo-driver */
#endif
#ifndef STRID_TPIUDP
#define STRID_TPIUDP 9102	/* TPI UDP pseudo-driver */
#endif
#ifndef STRID_TPIIP
#define STRID_TPIIP 9103	/* TPI IP pseudo-driver */
#endif
#ifndef STRID_TPIICMP
#define STRID_TPIICMP 9104	/* TPI ICMP pseudo-driver */
#endif
#ifndef STRID_TPIRAWIP
#define STRID_TPIRAWIP 9104	/* TPI RAWIP pseudo-driver */
#endif

/*
 *  These things could be in socket.h, but are only used for TPI.
 */

#endif /* _IO_TPI_TPISOCKET_H_ */
