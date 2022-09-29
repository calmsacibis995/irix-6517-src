/*
 *  $Id: tc_isps.h,v 1.5 1997/09/09 00:25:11 mwang Exp $
 */

/*
 * User-visible data structures and definitions for ISPS code.
 *
 * Copyright 1994, 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _net_isps_h
#define _net_isps_h

/* Type representing the externally visible specifier for ISPS objects
 * - flows, predictive service classes, and nodes in the share tree. A
 * handle is returned when one of these objects is created, and is
 * used to specify the object for all further operations.
 * 
 * HANDLE_NONE is used when creating create the root share tree
 * node. When this handle is specified as the "parent node" to the
 * ISPS_ADDSHARE function of the isps system call, a new sharing tree
 * and root node is created, and a handle for the root node is
 * returned.
 */
typedef u_int32_t isps_handle_t;	
#define HANDLE_NONE (isps_handle_t) -1

/* Packet filter descriptor - describes packets belonging to a flow.
 * Currently, link level information cannot be used to classify packets.
 * Currently, only PF_INET (IPv4) is implemented.
 *
 * IPv4 disambiguation rules:
 *  Destaddr, protocol, destport must be specified.
 *  Srcaddr may be specified or wildcarded (INADDR_ANY). If srcaddr is
 *   wildcarded, srcport is ignored, and packets from any source
 *   will match the filter.
 *  If srcaddr is specified, srcport may be specified or wildcarded (0).
 *   If srcport is wildcarded, packets with the given source address
 *   and any source port will match the filter.
 *  A more specific filter overrides a less specific filter for packets
 *   which match both.
 */
typedef struct _filt isps_filt_t;
struct _filt {
    int f_pf;				/* Family (only PF_INET now) */
    union {
	struct {
	    struct in_addr _f_destaddr;	/* IP dest address */
	    struct in_addr _f_srcaddr;	/* IP source address */
	    u_int16_t _f_destport;	/* TCP/UDP dest port */
	    u_int16_t _f_srcport;	/* TCP/UDP source port */
	    u_int8_t _f_protocol;	/* Xport protocol (IPPROTO_UDP, etc) */
	} _f_ipv4;
    } _f_spec;
};
#define f_ipv4_destaddr	_f_spec._f_ipv4._f_destaddr
#define f_ipv4_srcaddr	_f_spec._f_ipv4._f_srcaddr
#define f_ipv4_destport	_f_spec._f_ipv4._f_destport
#define f_ipv4_srcport	_f_spec._f_ipv4._f_srcport
#define f_ipv4_protocol	_f_spec._f_ipv4._f_protocol

/* Service specification ("flowspec") - describes quality of service
 * desired for a flow.
 */

/* Basic type of service: determines scheduling algorithm */
typedef enum {
    TOS_GUARANTEED = 1,
    TOS_PREDICTIVE,
    TOS_CNTRLD_LOAD,
    TOS_DATAGRAM
} isps_tos_t;

/* Per-service RSPEC definitions */
typedef struct {
    u_int32_t g_rate;		/* GUARANTEED: flow rate parameter	*/
} isps_g_rspec_t;

typedef struct {
    u_int32_t p_dly;		/* PREDICTIVE: delay target parameter	*/
    				/* May also be used as priority..	*/
} isps_p_rspec_t;

/* No Rspec for TOS_CNTRLD_LOAD */

typedef union {
    isps_g_rspec_t rs_g;	/* TOS_GUARANTEED			*/
    isps_p_rspec_t rs_p;	/* TOS_PREDICTIVE			*/
				/* No Rspec for TOS_CNTRLD_LOAD 	*/
} isps_rspec_t;

/* Traffic spec (TSPEC) definitions */
typedef struct {
    u_int32_t tbf_bkt;		/* tbf bucket size: bits		*/
    u_int32_t tbf_rate;		/* tbf rate: bits/sec			*/
    u_int32_t tbf_buf;		/* tbf reshaping buffer size (currently MBZ)*/
} isps_tbf_t;

typedef struct {
	u_int32_t c_rate;	/* Token bucket rate (B/sec) */
	u_int32_t c_bkt;	/* Token bucket depth (B) */
	u_int32_t c_m;		/* Min Policed Unit (B) */
	u_int32_t c_M;		/* Max pkt size (B) */
} isps_c_tspec_t;

typedef union {
    isps_tbf_t ts_tb;
    isps_c_tspec_t ts_c;	/* TOS_CNTRLD_LOAD			*/
} isps_tspec_t;

/* Policing/reshaping algorithm selector */
typedef enum {
    PLC_NONE,			/* No policing				*/
    PLC_TBF2,			/* Two-parameter token bucket filter	*/
    PLC_TBF3			/* Three-param TBF -unimplemented-	*/
} isps_plc_t;

/* 
 * Missing: PSPEC.  At the moment TSPEC information is used both to
 * drive admission control and to drive policing and reshaping. This
 * is almost certainly incorrect, and a new PSPEC structure will
 * eventually be required...
 */

/* Overall flow spec passed to scheduler. */
typedef struct {
    isps_tos_t fs_tos;		/* service type				*/
    isps_rspec_t fs_rspec;	/* rspec parameters for requested svc	*/
    isps_tspec_t fs_tspec;	/* policing parameters			*/
    isps_plc_t fs_plc;		/* policing algorithm selector		*/
    isps_handle_t fs_handle;	/* handle of share tree node associated */
				/*   with this flow			*/
} isps_fs_t;


/* Share tree node types.  These types define the "role" of the
 * node. Note that the role of a node defines how the node's -parent-
 * will behave - for example if a parent finds that it has WFQ
 * children, it will WFQ them. It is illegal to attach nodes with
 * different roles to the same parent node.
 */
typedef enum {
    ISPS_ST_ROOT,		/* tree root				*/
    ISPS_ST_WFQ,		/* WFQ element				*/
    ISPS_ST_PRIORITY		/* priority q element			*/
} isps_stnode_t;

/* Slightly different set of values specify the type of a node's
 * children.  This has no operational effect, but exists purely for
 * syntactic checking.  A leaf node implicitly manages a fifo queue of
 * packets.
 */
typedef enum {
    ISPS_STCT_WFQ,		/* tree root */
    ISPS_STCT_PRIORITY,		/* WFQ element */
    ISPS_STCT_NONE		/* Leaf node; no children */
} isps_stct_t;

/* Share descriptor - used to manipulate nodes in the link sharing
 * tree.
 */
typedef struct {
    isps_handle_t sd_parent;	/* handle of parent 			   */
    isps_stnode_t sd_type; 	/* type of this node 			   */
    isps_stct_t sd_ctype;	/* expected type of children		   */
    u_int32_t sd_param;		/* weight or priority, depending on type   */
} isps_stdesc_t;

/* Max number of nodes (of all kinds) in the share tree.
 */
#define ST_MAXNODE 100	

/*
 * Normalized full capacity of a WFQ sharing level.  That is, when
 * creating WFQ nodes you should set the sd_param field, above, to
 * (ST_NORMALIZED_BW * <percentage_of_bw_allocated_to_this_node>)
 */
#define ST_NORMALIZED_BW 10000

/*
 * Interface information descriptor - specifys interface parameters
 * needed for scheduling. XXX will shortly be done differently.
 */
typedef struct {
    int ii_qsize;		/* max queue size in packets (attempts to   */
				/* queue more will trigger dropping)        */
    u_int32_t ii_bw;		/* interface bandwidth, bits/sec	    */
    u_int32_t ii_res_bw;	/* b/w reservable by isps services	    */
} isps_ifinfo_t;

/*
 * Descriptor used to perform per-interface configuration at startup.
 */
typedef struct {
    int id_enabled;		/* nonzero to enable ISPS services	*/
    isps_ifinfo_t id_info;	/* information about interface 		*/
} isps_intdesc_t;

#define id_qsize	id_info.ii_qsize
#define id_bw		id_info.ii_bw
#define id_res_bw	id_info.ii_res_bw

/*
 * SGI - structure used to get and set packet scheduler configuration 
 * information.  The following fields are settable:
 *	max_resv_bandwidth
 *	flags (see rsvp.h)
 *      num_batch_pkts
 */
typedef struct psif_config_info {
	uint	int_bandwidth;		/* interface bandwidth		 */
	uint	max_resv_bandwidth;	/* max reservable bdw		 */
	uint	cur_resv_bandwidth;	/* current reserved bdw		 */
	uint	num_batch_pkts;
	uint	flags;			/* psif_flags			 */
	uint	mtu;			/* interface mtu		 */
	ushort	drv_qlen;		/* driver tx queue length	 */
	ushort	cl_qlen;		/* packet scheduler CL q length	 */
	ushort	be_qlen;		/* packet scheduler BE q length  */
	ushort	nc_qlen;		/* packet scheduler NC q length  */
} psif_config_info_t;

/*
 * SGI - structure used to get stats for the entire packet scheduling
 * interface.
 */
typedef struct psif_stats {
	int	txq_len;
	int	clq_len;
	int	beq_len;
	int	ncq_len;
	uint	be_pkts;
	uint	cl_pkts;
	uint	nc_pkts;
} psif_stats_t;

/*
 * SGI - structure used to find out from the kernel what flows and
 * filters are installed on an interface.  The kernel copies
 * all the flows and their filters to the buffer given by the user.
 * The format of the buffer is 
 *    psif_flow_info_t
 *    isps_fs_t
 *    isps_filt_t (there may be multple filters for a flow)
 *    psif_flow_info_t
 *    isps_fs_t
 *    and so on.
 */
typedef struct psif_flow_list{
	int		buf_size;	/* size of user buffer		*/
	uint32_t	buf_addr;	/* user buffer for kernel info	*/
	ushort		num_flows;	/* num flows copied out by kernel */
	ushort		more_info;	/* kernel could not copy out all flows */
} psif_flow_list_t;

typedef struct psif_flow_info {
	isps_handle_t	flow_handle;	/* kernel flow handle		*/
	int		num_filters;	/* num filters for this flow	*/
} psif_flow_info_t;

/*
 * SGI - structure used to get stats for a particular flow.
 */
typedef struct psif_flow_stats{
	uint	cl_pkts;		/* pkts which got cl service	*/
	uint	nc_pkts;		/* # of non-conforming pkts	*/
} psif_flow_stats_t;


/*
 * Argument to ISPS ioctl - see isps_doc for usage.
 * NOTE that the beginning of this structure (iq_name) must be
 * identical to the layout of the ifreq structure defined in
 * <net/if.h>. IFNAMSIZ is defined in <net/if.h>; include that
 * first.
 */
struct	ispsreq {
    char iq_name[IFNAMSIZ];		/* if name, e.g. "en0"       */
    short iq_function;			/* function code             */
    short iq_flags;			/* further info about fcn    */
    isps_handle_t iq_handle;		/* call/ret - object handle  */
    isps_handle_t iq_fhdl;		/* filter handle	     */
    union {
	isps_fs_t _iq_fs;		/* ADDFLW: flow spec	     */
	isps_filt_t _iq_filt;		/* SETFILT: filter           */
	unsigned int _iq_delay;		/* ADDCLS: delay target      */
	isps_stdesc_t _iq_sdesc;	/* ADDSHARE: share desc      */
	isps_intdesc_t _iq_idesc;	/* CTL: i'face desc	     */
	psif_config_info_t _iq_config;	/* SGI get/set config info   */
	psif_stats_t _iq_stats;		/* SGI get if stats	     */
	psif_flow_list_t _iq_flist;	/* SGI get flow & filt list  */
	psif_flow_stats_t _iq_fstats;	/* SGI get flow stats	     */
    } _iq_iqu;
};

#define iq_fs _iq_iqu._iq_fs
#define iq_filt _iq_iqu._iq_filt
#define iq_delay _iq_iqu._iq_delay
#define iq_sdesc _iq_iqu._iq_sdesc
#define iq_intdesc _iq_iqu._iq_idesc
#define iq_config _iq_iqu._iq_config
#define iq_stats _iq_iqu._iq_stats
#define iq_flist _iq_iqu._iq_flist
#define iq_fstats _iq_iqu._iq_fstats


/* Longest delay a class creation call (SIOCIFADDCLS) can request */
#define ISPS_CLASS_MAX_DELAY	4096	/* milliseconds */

/* Smallest rate a flow creation / modification call can request. */
/* Smaller rates get EBADRSPEC or EBADTSPEC, depending on where
 * they are seen */ 
 /* XXX value will change */
#define ISPS_FLOW_MIN_RATE	32000	/* bits/sec */

/* Largest TBF bucketsize a flow create / mod call can request. */
/* Larger buckets get EBADTSPEC */
#define ISPS_FLOW_MAX_BKT	10000000 /* bits */

/* Functions (in iq_function) */
#define IF_IFCTL	0x01	/* enable / disable ISPS for interface	*/
#define	IF_RESET	0x02	/* reset reservation state		*/
#define	IF_ADDCLS	0x03	/* add predictive svc  class		*/
#define	IF_DELCLS	0x04	/* delete predictive svc class		*/
#define	IF_ADDSHARE	0x05	/* add share tree node			*/
#define	IF_DELSHARE	0x06	/* delete share tree node		*/
#define	IF_ADDFLW	0x07	/* add flow				*/
#define	IF_DELFLW	0x08	/* delete flow				*/
#define	IF_MODFLW	0x09	/* modify existing flow's parameters	*/
#define	IF_SETFILT	0x0a	/* set packet filter for flow		*/
#define IF_DELFILT	0x0b	/* delete filter from flow		*/

#define IF_GET_CONFIG	0xee01	/* SGI get packet sched. config		*/
#define IF_SET_CONFIG	0xee02	/* SGI set packet sched. config		*/
#define IF_GET_STATS	0xee03	/* SGI get packet sched. stats		*/
#define IF_GET_FLOW_LIST 0xee04 /* SGI get list of flows for this if	*/
#define IF_GET_FLOW_STATS 0xee05/* SGI get stats for this flow		*/

/* Flags (in iq_flags) */
#define IF_F_LOCKED	0x01	/* IF_ADDFLW: locked against IF_RESET	*/

/* should be in <sys/sockio.h> */
#ifndef SIOCISISPSCTL
#if defined(sun) && !defined(__GNUC__)
#define	SIOCIFISPSCTL	_IOWR(i, 70, struct ispsreq)
#else
#define	SIOCIFISPSCTL	_IOWR('i', 70, struct ispsreq)
#endif
#endif

#endif /* _net_isps_h */
