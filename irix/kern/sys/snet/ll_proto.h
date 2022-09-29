#ident "$Revision: 1.3 $"
/******************************************************************
 *
 *  SpiderX25 Protocol Streams Component
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  LL_PROTO.H
 *
 *    LAPB Top-level STREAMS interface.
 *
 ******************************************************************/

/*
 *	ll_proto.h
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/include/sys/snet/0/s.ll_proto.h
 *	@(#)ll_proto.h	1.12
 *
 *	Last delta created	16:45:20 2/13/92
 *	This file extracted	14:53:28 11/13/92
 *
 */

#ifndef X25_LL_PROTO_
#define X25_LL_PROTO_
/* Interface structures */
struct ll_reg {
    uint8	ll_type;
    uint8	ll_class;
    uint8	ll_regstatus;
    uint8	ll_spare;
    uint32	ll_snid;
    uint8	ll_mymacaddr[6];
    uint8	ll_normalSAP;
    uint8	ll_loopbackSAP;
};

struct ll_msg {
    uint8	ll_type;
    uint8	ll_command;
    uint16	ll_connID;
    caddr_t	ll_yourhandle;
    uint32	ll_status;
};

struct ll_msgc {
    uint8	ll_type;
    uint8	ll_command;
    uint16	ll_connID;
    caddr_t	ll_yourhandle;
    caddr_t	ll_myhandle;
    uint16	ll_service_class;
    uint8	ll_remsize;
    uint8	ll_locsize;
    uint8	ll_remaddr[12];
    uint8	ll_locaddr[12];
};

/* Values for 'll_type' */
#define LL_REG		 50
#define LL_CTL		 51
#define LL_DAT		 52

/* Values for 'll_command' */
#define LC_CONNECT        1
#define LC_CONCNF         2
#define LC_DATA		  3
#define LC_UDATA          4
#define LC_DISC		  5
#define LC_DISCNF         6
#define LC_RESET          7
#define LC_RSTCNF         8
#define LC_REPORT         9
#define LC_CONOK         10
#define LC_CONNAK        11
#define LC_PRGBRA        12
#define LC_PRGKET        13

/* Values of 'll_class' in 'll_reg' */
#define LC_LLC1          15
#define LC_LLC2          16
#define LC_LAPBDTE       17
#define LC_LAPBXDTE      18
#define LC_LAPBDCE       19
#define LC_LAPBXDCE      20
#define LC_LAPDTE        21
#define LC_LAPDCE        22

/* Values in 'll_regstatus' and 'll_status' */
#define LS_SUCCESS        1
#define LS_RESETTING      2
#define LS_RESETDONE      3
#define LS_DISCONNECT     4
#define LS_FAILED         5
#define LS_CONFLICT       6
#define LS_RST_FAILED     7
#define LS_RST_REFUSED    8
#define LS_RST_DECLINED   9
#define LS_FMR_RECEIVED  10
#define LS_FMR_SENT      11
#define LS_REM_BUSY      12
#define LS_REM_NOT_BUSY  13
#define LS_EXHAUSTED     14
#define LS_SSAPINUSE     15
#define LS_LSAPINUSE     16
#define LS_DUPLICATED    17
#define LS_LSAPWRONG     18
#endif /* X25_LL_PROTO_ */
