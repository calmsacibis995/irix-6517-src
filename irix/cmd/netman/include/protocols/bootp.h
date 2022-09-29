/*
 * "vendor" data permitted for Stanford boot clients.
 */

struct vend {
	u_char	v_magic[4];	/* magic number */
	u_long	v_flags;	/* flags/opcodes, etc. */
	u_char	v_unused[56];	/* currently unused */
};

/*
 * Bootstrap Protocol (BOOTP).  RFC 951.
 */

struct bootp {
	u_char	bp_op;		/* packet opcode type */
#define	BOOTREQUEST	1
#define	BOOTREPLY	2
	u_char	bp_htype;	/* hardware addr type */
	u_char	bp_hlen;	/* hardware addr length */
	u_char	bp_hops;	/* gateway hops */
	u_long	bp_xid;		/* transaction ID */
	u_short	bp_secs;	/* seconds since boot began */	
	u_short	bp_unused;
	iaddr_t	bp_ciaddr;	/* client IP address */
	iaddr_t	bp_yiaddr;	/* 'your' IP address */
	iaddr_t	bp_siaddr;	/* server IP address */
	iaddr_t	bp_giaddr;	/* gateway IP address */
	u_char	bp_chaddr[16];	/* client hardware address */
	u_char	bp_sname[64];	/* server host name */
	u_char	bp_file[128];	/* boot file name */
	union {
		u_char	vend_unused[64];
		struct	vend	sgi_vadmin;
	} rfc1048;
#define	bp_vend		rfc1048.vend_unused		/* rfc951 field */
#define vd_magic	rfc1048.sgi_vadmin.v_magic	/* magic #	*/
#define	vd_flags	rfc1048.sgi_vadmin.v_flags	/* opcodes	*/
#define	vd_clntname	rfc1048.sgi_vadmin.v_unused	/* client name	*/
};

/*
 * UDP port numbers, server and client.
 */
#define	IPPORT_BOOTPS		67
#define	IPPORT_BOOTPC		68

#define	VM_STANFORD	"STAN"		/* v_magic for Stanford */
#define	VM_SGI		0xc01a3309	/* v_magic for Silicon Graphics Inc., */
#define	VM_AUTOREG	0xc01a330a	/* v_magic for Silicon Graphics Inc., */

/* v_flags values */
#define	VF_PCBOOT	1	/* an IBMPC or Mac wants environment info */
#define	VF_HELP		2	/* help me, I'm not registered */
#define	VF_GET_IPADDR	3	/* Request for client IP address */
#define	VF_RET_IPADDR	4	/* Response for client IP address */
#define	VF_NEW_IPADDR	5	/* Response for client IP address */
