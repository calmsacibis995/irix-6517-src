#ifndef	_MTR_H
#define	_MTR_H

#include <net/if.h>
#include <sys/syslog.h>

/*
 *	$Header: /proj/irix6.5.7m/isms/irix/cmd/tokenring/mtr/RCS/mtr.h,v 1.1 1997/03/20 18:53:56 lguo Exp $
 *
 *
 *	__WATCH OUT__:
 *
 *	Some of the definitions here are from driver, if_mtr.c.
 *	They MUST BE consistent!
 */

#define	MAX_MSG_SIZE		256
#define	NO_ERROR		0
#define	REAL_ROOT_REQUIRED	1
#define	MAC_ADDR_STRING_SIZE	80
#define	IP_ADDR_STRING_SIZE	64

#ifndef TRMTU_16M
        #define TRMTU_16M       17800
#endif  /* TRMTU_16M */
#ifndef TRMTU_4M
        #define TRMTU_4M	4472
#endif  /* TRMTU_4M */

/*	
 *	From if_mtr.c
 */
typedef struct tokenbufhead {
        struct ifheader         tbh_ifh;
        struct snoopheader      tbh_snoop;
} TOKENBUFHEAD;
#define TRPAD                   sizeof(TOKENBUFHEAD)
#define TR_IF_HEAER_SIZE        TRPAD - sizeof(struct snoopheader)
#define TR_RAW_MAC_SIZE 	sizeof(TR_MAC) + sizeof(TR_RII)
#define TR_PACKET_HDR_SIZE      sizeof(TR_MAC) + sizeof(TR_RII) +       \
                                sizeof(LLC1) + sizeof(SNAP)
#define TR_MAC_HEADER_PAD_SIZE  TR_PACKET_HDR_SIZE +                    \
        RAW_HDRPAD(TR_PACKET_HDR_SIZE + TR_IF_HEAER_SIZE) - sizeof(TR_MAC)

/*	
 *	From if_mtr.c: mtr_drain_input():
 */
typedef struct tr_drain_s {
        TR_MAC                  mac;
        u_char                  pad[TR_MAC_HEADER_PAD_SIZE];
        struct snoopheader      sh;             /* _16_ bytes */
        u_char                  data[TRMTU_16M];
} tr_drain_t;

/*
 *	From if_mtr.c: mtr_snoop_input():
 */
typedef struct tr_packet_s {
        TR_MAC                  mac;
        TR_RII                  rii;
        LLC1                    llc1;
        SNAP                    snap;
        u_char                  data[TRMTU_16M];
} tr_packet_t;

/*
 *	From if_mtr.c: mtr_snoop_input():
 */
typedef struct {
        struct snoopheader      snoop;
        tr_packet_t             token;
} mtr_snoop_pkt_t;

/*	
 *	From if_mtr.c: additional ioctl() for PCI tokenring driver: 		
 */
#define SIOCGIFSPEED    	_IOR( 'S', 87, struct ifreq)	/* get ring speed */
#define SIOCSIFSPEED    	_IOW( 'S', 88, struct ifreq)
#define SIOCGBIA        	_IOWR('S', 89, struct ifreq)    /* get BIA */
#define SIOCGTRIFSTATS  	_IOWR('S', 90, struct ifreq)
#ifndef	SIOC_MTR_GETFUNC
#define SIOC_MTR_GETFUNC 	_IOWR('S', 91, struct ifreq)
#define SIOC_MTR_ADDFUNC 	_IOWR('S', 92, struct ifreq)
#define SIOC_MTR_DELFUNC 	_IOWR('S', 93, struct ifreq)
#endif	/* SIOC_MTR_GETFUNC */

/*	
 *	From if_mtr.c: for SIOC_MTR_...FUNC	
 */
#define TR_FUNCTIONAL_ADDR_AM   0x0001  /* Active Monitor */
#define TR_FUNCTIONAL_ADDR_RPS  0x0002  /* Ring Parameter Server */
#define TR_FUNCTIONAL_ADDR_REM  0x0008  /* Ring Error Monitor */
#define TR_FUNCTIONAL_ADDR_CRS  0x0010  /* Configuration Report Server */

/*	
 *	From if_mtr.c: for SIOCGTRIFSTATS 	
 */
#ifdef  STRUCT_ERROR_LOG
typedef struct STRUCT_ERROR_LOG error_log_t;
#else   /* STRUCT_ERROR_LOG */
        typedef struct STRUCT_ERROR_LOG {
                u_char        line_errors;
                u_char        reserved_1;
                u_char        burst_errors;
                u_char        ari_fci_errors;
                u_char        reserved_2;
                u_char        reserved_3;
                u_char        lost_frame_errors;
                u_char        congestion_errors;
                u_char        frame_copied_errors;
                u_char        reserved_4;
                u_char        token_errors;
                u_char        reserved_5;
                u_char        dma_bus_errors;
                u_char        dma_parity_errors;
        } error_log_t;
#endif  /* STRUCT_ERROR_LOG */

#ifndef min
#define min( _x, _y )   ((_x) <= (_y) ? (_x) : (_y))
#endif  /* min */

#define	DEFAULT_DUMP_SIZE	128
#define	IDENT			"    "

typedef	struct symtable_s {
	u_char	type;
	char	*string;
} symtable_t;

#define	DEAULT_IP_HDR_SIZE	20		/* IPV4 at least has 20 bytes for hdr */

#define DUMP_FORMAT_MASK        0xfffff
#define DUMP_SNOOP_HDR          0x00001          /* SNOOP hdr */
#define DUMP_HEX_ONLY           0x00002          /* no any format */

#define DUMP_MAC                0x000f0
#define DUMP_MAC_PKT_ONLY       0x00010          /* only MAC packet */
#define DUMP_MAC_PKT            0x00020          /* MAC packet */

#define DUMP_MAC_LLC_SNAP_IP    0x0ff00
#define DUMP_MAC_HDR            0x00100          /* MAC HDR */
#define DUMP_LLC_SNAP_HDR       0x00200          /* LLC1, SNAP */
#define DUMP_IP_HDR             0x00400          /* IP hdr */
#define DUMP_TCP_HDR            0x00800          /* TCP hdr */
#define DUMP_UDP_HDR            0x01000          /* UDP hdr */
#define DUMP_ICMP               0x02000          /* ICMP hdr */

#define DUMP_TR_ALL_EVENTS      0x10000         /* including those SMPs and AMPs */

extern	int	GotSignal;
extern	int	DebugMode;
extern 	char	*optarg;
extern	int	optind;
extern	int	errno;
extern	int	DumpSize;
extern	int	MonitorMode;
extern 	int	DumpMode;
extern	char	*Interface;
extern	char	*MyName;

/*
 *	Routines defined in mtr_common.c:
 */
extern	void	sig_handler(int sig);
extern	void	Perror(char *string);
extern	void	tr_ntoa(u_char *n, char *a);
extern	int	tr_aton(char *a, char *n);
extern	void	Log(int log_level, char *msg);
extern	int	Begin(int argc, char **argv, int root_require);
extern	void	dump_hex(caddr_t cp, int cnt, int offset);
extern	void	dump_mac_hdr( TR_MAC *pkt_p );
extern	void	dump_mac_pkt( TR_MAC *pkt_p, TR_MACMV *tr_mac_mv_p, int pkt_size, 
		int hdr_size, char *interface, int pkt_cnt );
extern 	void 	dump_snoop_hdr( int pkt_cnt, int pkt_size, 
		char *who, char *interace, struct snoopheader *sh_p);
extern	void	dump_ip_pkt( caddr_t hex_ptr, int hex_off);
extern	void	dump_arp_pkt( struct arphdr *arp_p );
extern	void	dump_icmp_pkt( caddr_t hex_ptr, int hex_off);

#endif	/* _MTR_H */
