/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.6 $
 */

/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
#ifdef KINETICS
#include "gw.h"
#include "fp4/pbuf.h"
#include "fp4/cmdmacro.h"
#include "ab.h"
#include "glob.h"
#endif

#if (defined(unix) && !defined(KINETICS))
#include <sys/types.h>
#include <netinet/in.h>
#ifndef NULL
#define NULL 0
#endif
#endif


#include <inet.h>
#include <smt_asn1.h>
#include <smt_snmp.h>
#include <smt_snmp_impl.h>
#include <smt_mib.h>
#include <smt_snmp_vars.h>
/*
 *	Each variable name is placed in the variable table, without the terminating
 * substring that determines the instance of the variable.  When a string is found that
 * is lexicographicly preceded by the input string, the function for that entry is
 * called to find the method of access of the instance of the named variable.  If
 * that variable is not found, NULL is returned, and the search through the table
 * continues (it should stop at the next entry).  If it is found, the function returns
 * a character pointer and a length or a function pointer.  The former is the address
 * of the operand, the latter is an access routine for the variable.
 *
 * u_char *
 * findVar(name, length, exact, var_len, access_method)
 * oid	    *name;	    IN/OUT - input name requested, output name found
 * int	    length;	    IN/OUT - number of sub-ids in the in and out oid's
 * int	    exact;	    IN - TRUE if an exact match was requested.
 * int	    len;	    OUT - length of variable or 0 if function returned.
 * int	    access_method; OUT - 1 if function, 0 if char pointer.
 *
 * accessVar(rw, var, varLen)
 * int	    rw;	    IN - request to READ or WRITE the variable
 * u_char   *var;   IN/OUT - input or output buffer space
 * int	    *varLen;IN/OUT - input and output buffer len
 */

struct variable {
    oid		    name[16];	    /* object identifier of variable */
    u_char	    namelen;	    /* length of above */
    char	    type;	    /* type of variable, INTEGER or (octet) STRING */
    u_char	    magic;	    /* passed to function as a hint */
    u_short	    acl;	    /* access control list for variable */
    u_char	    *(*findVar)();  /* function that finds variable */
};

char		version_descr[30] = "Kinetics FastPath4";
oid		version_id[] = {1, 3, 6, 1, 4, 1, 3, 1, 1};
int		version_id_len = sizeof(version_id);
u_long		uptime;
long		cfg_nnets = MAX_INTERFACES;
long		long_return;
u_char		return_buf[64];


struct mib_ifEntry  mib_ifEntry_proto[MAX_INTERFACES] = {
    {1, "Kinetics KFPS2 Ethernet", MIB_IFTYPE_ETHERNETCSMACD, 
    	1500, 10000000L, "", 
	6, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
    {2, "Kinetics KFPS2 Appletalk", MIB_IFTYPE_OTHER,
    	1500, 230000L, "", 
	3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
struct mib_ifEntry mib_ifEntry[MAX_INTERFACES];

struct mib_ip mib_ip_proto = {
    1, IPFRAGTTL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
struct mib_ip mib_ip;

#define ROUTE_DEFAULT	0
#define ROUTE_LOCAL	1
struct mib_ipRouteEntry mib_ipRouteEntry_proto[2] = {
    {0, 1, 1, -1, -1, -1, 0, MIB_IPROUTETYPE_REMOTE, MIB_IPROUTEPROTO_LOCAL, 0},    /* default route */
    {0, 1, 0, -1, -1, -1, 0, MIB_IPROUTETYPE_DIRECT, MIB_IPROUTEPROTO_LOCAL, 0}	    /* local route */
};
struct mib_ipRouteEntry mib_ipRouteEntry[2];

struct mib_udp mib_udp_proto = {
    0, 0, 0, 0
};
struct mib_udp mib_udp;

long	mib_icmpInMsgs;
long	mib_icmpOutMsgs;
long	mib_icmpInErrors;	/* not checked in KIP */
long	mib_icmpOutErrors;	/* not checked in KIP */
long	mib_icmpInCount[ICMP_MAXTYPE + 1];
long	mib_icmpOutCount[ICMP_MAXTYPE + 1];


init_snmp(){
    bcopy((char *)mib_ifEntry_proto, (char *)mib_ifEntry, sizeof(mib_ifEntry));
    bcopy((char *)&mib_ip_proto, (char *)&mib_ip, sizeof(mib_ip));
    bcopy((char *)mib_ipRouteEntry_proto, (char *)mib_ipRouteEntry, sizeof(mib_ipRouteEntry));
    bcopy((char *)&mib_udp_proto, (char *)&mib_udp, sizeof(mib_udp));
}

/*
 * These are byte offsets into their structures.
 * This really should be computed by the compiler, but the
 * compiler I'm using doesn't want to do this.
 */
#define VERSION_DESCR	0
#define VERSION_ID	32
#define CFG_NNETS	48
#define UPTIME		52

#define IFINDEX		0
#define IFDESCR		4
#define IFTYPE		36
#define IFMTU		40
#define IFSPEED		44
#define IFPHYSADDRESS	48
#define IFADMINSTATUS	60
#define IFOPERSTATUS	64
#define IFLASTCHANGE	68
#define IFINOCTETS	72
#define IFINUCASTPKTS	76
#define	IFINNUCASTPKTS	80
#define	IFINDISCARDS	84
#define	IFINERRORS	88
#define	IFINUNKNOWNPROTOS   92
#define	IFOUTOCTETS	96
#define	IFOUTUCASTPKTS	100
#define	IFOUTNUCASTPKTS	104
#define	IFOUTDISCARDS	108
#define	IFOUTERRORS	112
#define	IFOUTQLEN	116

#define ATIFINDEX	0
#define ATPHYSADDRESS	4
#define ATNETADDRESS	16

#define IPFORWARDING	0
#define IPDEFAULTTTL	4
#define IPINRECEIVES	8
#define IPINHDRERRORS	12
#define IPINADDRERRORS	16
#define IPFORWDATAGRAMS	20
#define IPINUNKNOWNPROTOS   24
#define IPINDISCARDS	28
#define IPINDELIVERS	32
#define IPOUTREQUESTS	36
#define IPOUTDISCARDS	40
#define IPOUTNOROUTES	44
#define IPREASMTIMEOUT	48
#define IPREASMREQDS	52
#define IPREASMOKS	56
#define IPREASMFAILS	60
#define IPFRAGOKS	64
#define IPFRAGFAILS	68
#define IPFRAGCREATES	72

#define IPADADDR	0
#define IPADIFINDEX	4
#define IPADNETMASK	8
#define IPADBCASTADDR	12

#define IPROUTEDEST	0
#define IPROUTEIFINDEX	4
#define IPROUTEMETRIC1	8
#define IPROUTEMETRIC2	12
#define IPROUTEMETRIC3	16
#define IPROUTEMETRIC4	20
#define IPROUTENEXTHOP	24
#define IPROUTETYPE	28
#define IPROUTEPROTO	32
#define IPROUTEAGE	36

#define	ICMPINMSGS	    0
#define	ICMPINERRORS	    4
#define	ICMPINDESTUNREACHS  8
#define	ICMPINTIMEEXCDS	    12
#define	ICMPINPARMPROBS	    16
#define	ICMPINSRCQUENCHS    20
#define	ICMPINREDIRECTS	    24
#define	ICMPINECHOS	    28
#define	ICMPINECHOREPS	    32
#define	ICMPINTIMESTAMPS    36
#define	ICMPINTIMESTAMPREPS 40
#define	ICMPINADDRMASKS	    44
#define	ICMPINADDRMASKREPS  48
#define	ICMPOUTMSGS	    52
#define	ICMPOUTERRORS	    56
#define	ICMPOUTDESTUNREACHS 60
#define	ICMPOUTTIMEEXCDS    64
#define	ICMPOUTPARMPROBS    68
#define	ICMPOUTSRCQUENCHS   72
#define	ICMPOUTREDIRECTS    76
#define	ICMPOUTECHOS	    80
#define	ICMPOUTECHOREPS	    84
#define	ICMPOUTTIMESTAMPS   88
#define	ICMPOUTTIMESTAMPREPS	92
#define	ICMPOUTADDRMASKS    96
#define	ICMPOUTADDRMASKREPS 100

#define UDPINDATAGRAMS	    0
#define UDPNOPORTS	    4
#define	UDPINERRORS	    8
#define UDPOUTDATAGRAMS	    12

struct variable	    variables[] = {
    /* these must be lexicographly ordered by the name field */
    {{MIB, 1, 1, 0},		9, STRING,  VERSION_DESCR, RONLY, var_system },
    {{MIB, 1, 2, 0},		9, OBJID,   VERSION_ID, RONLY, var_system },
    {{MIB, 1, 3, 0},		9, TIMETICKS, UPTIME, RONLY, var_system },
    {{MIB, 2, 1, 0},		9, INTEGER, CFG_NNETS, RONLY, var_system },
    {{MIB, 2, 2, 1, 1, 0xFF},  11, INTEGER, IFINDEX, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 2, 0xFF},  11, STRING,  IFDESCR, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 3, 0xFF},  11, INTEGER, IFTYPE, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 4, 0xFF},  11, INTEGER, IFMTU, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 5, 0xFF},  11, GAUGE,   IFSPEED, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 6, 0xFF},  11, STRING,  IFPHYSADDRESS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 7, 0xFF},  11, INTEGER, IFADMINSTATUS, RWRITE, var_ifEntry },
    {{MIB, 2, 2, 1, 8, 0xFF},  11, INTEGER, IFOPERSTATUS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 9, 0xFF},  11, TIMETICKS, IFLASTCHANGE, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 10, 0xFF}, 11, COUNTER, IFINOCTETS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 11, 0xFF}, 11, COUNTER, IFINUCASTPKTS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 12, 0xFF}, 11, COUNTER, IFINNUCASTPKTS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 13, 0xFF}, 11, COUNTER, IFINDISCARDS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 14, 0xFF}, 11, COUNTER, IFINERRORS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 15, 0xFF}, 11, COUNTER, IFINUNKNOWNPROTOS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 16, 0xFF}, 11, COUNTER, IFOUTOCTETS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 17, 0xFF}, 11, COUNTER, IFOUTUCASTPKTS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 18, 0xFF}, 11, COUNTER, IFOUTNUCASTPKTS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 19, 0xFF}, 11, COUNTER, IFOUTDISCARDS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 20, 0xFF}, 11, COUNTER, IFOUTERRORS, RONLY, var_ifEntry },
    {{MIB, 2, 2, 1, 21, 0xFF}, 11, GAUGE,   IFOUTQLEN, RONLY, var_ifEntry },
    {{MIB, 3, 1, 1, 1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 16, INTEGER,    ATIFINDEX, RWRITE, var_atEntry }, 
    {{MIB, 3, 1, 1, 2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 16, STRING,	    ATPHYSADDRESS, RWRITE, var_atEntry }, 
    {{MIB, 3, 1, 1, 3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 16, IPADDRESS,  ATNETADDRESS, RWRITE, var_atEntry },
    {{MIB, 4, 1, 0},	    9, INTEGER, IPFORWARDING, RONLY, var_ip },
    {{MIB, 4, 2, 0},	    9, INTEGER, IPDEFAULTTTL, RWRITE, var_ip },
    {{MIB, 4, 3, 0},	    9, COUNTER, IPINRECEIVES, RONLY, var_ip },
    {{MIB, 4, 4, 0},	    9, COUNTER, IPINHDRERRORS, RONLY, var_ip },
    {{MIB, 4, 5, 0},	    9, COUNTER, IPINADDRERRORS, RONLY, var_ip },
    {{MIB, 4, 6, 0},	    9, COUNTER, IPFORWDATAGRAMS, RONLY, var_ip },
    {{MIB, 4, 7, 0},	    9, COUNTER, IPINUNKNOWNPROTOS, RONLY, var_ip },
    {{MIB, 4, 8, 0},	    9, COUNTER, IPINDISCARDS, RONLY, var_ip },
    {{MIB, 4, 9, 0},	    9, COUNTER, IPINDELIVERS, RONLY, var_ip },
    {{MIB, 4, 10, 0},	    9, COUNTER, IPOUTREQUESTS, RONLY, var_ip },
    {{MIB, 4, 11, 0},	    9, COUNTER, IPOUTDISCARDS, RONLY, var_ip },
    {{MIB, 4, 12, 0},	    9, COUNTER, IPOUTNOROUTES, RONLY, var_ip },
    {{MIB, 4, 13, 0},	    9, INTEGER, IPREASMTIMEOUT, RONLY, var_ip },
    {{MIB, 4, 14, 0},	    9, COUNTER, IPREASMREQDS, RONLY, var_ip },
    {{MIB, 4, 15, 0},	    9, COUNTER, IPREASMOKS, RONLY, var_ip },
    {{MIB, 4, 16, 0},	    9, COUNTER, IPREASMFAILS, RONLY, var_ip },
    {{MIB, 4, 17, 0},	    9, COUNTER, IPFRAGOKS, RONLY, var_ip },
    {{MIB, 4, 18, 0},	    9, COUNTER, IPFRAGFAILS, RONLY, var_ip },
    {{MIB, 4, 19, 0},	    9, COUNTER, IPFRAGCREATES, RONLY, var_ip },
    {{MIB, 4, 20, 1, 1, 0xFF, 0xFF, 0xFF, 0xFF}, 14, IPADDRESS, IPADADDR, RONLY, var_ipAddrEntry },
    {{MIB, 4, 20, 1, 2, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPADIFINDEX, RONLY, var_ipAddrEntry },
    {{MIB, 4, 20, 1, 3, 0xFF, 0xFF, 0xFF, 0xFF}, 14, IPADDRESS, IPADNETMASK, RONLY, var_ipAddrEntry },
    {{MIB, 4, 20, 1, 4, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPADBCASTADDR, RONLY, var_ipAddrEntry },
    {{MIB, 4, 21, 1, 1, 0xFF, 0xFF, 0xFF, 0xFF}, 14, IPADDRESS, IPROUTEDEST, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 2, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTEIFINDEX, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 3, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTEMETRIC1, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 4, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTEMETRIC2, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 5, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTEMETRIC3, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 6, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTEMETRIC4, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 7, 0xFF, 0xFF, 0xFF, 0xFF}, 14, IPADDRESS, IPROUTENEXTHOP, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 8, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTETYPE, RWRITE, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 9, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTEPROTO, RONLY, var_ipRouteEntry },
    {{MIB, 4, 21, 1, 10, 0xFF, 0xFF, 0xFF, 0xFF}, 14, INTEGER,	IPROUTEAGE, RWRITE, var_ipRouteEntry },
    {{MIB, 5, 1, 0},	    9, COUNTER, ICMPINMSGS, RONLY, var_icmp },
    {{MIB, 5, 2, 0},	    9, COUNTER, ICMPINERRORS, RONLY, var_icmp },
    {{MIB, 5, 3, 0},	    9, COUNTER, ICMPINDESTUNREACHS, RONLY, var_icmp },
    {{MIB, 5, 4, 0},	    9, COUNTER, ICMPINTIMEEXCDS, RONLY, var_icmp },
    {{MIB, 5, 5, 0},	    9, COUNTER, ICMPINPARMPROBS, RONLY, var_icmp },
    {{MIB, 5, 6, 0},	    9, COUNTER, ICMPINSRCQUENCHS, RONLY, var_icmp },
    {{MIB, 5, 7, 0},	    9, COUNTER, ICMPINREDIRECTS, RONLY, var_icmp },
    {{MIB, 5, 8, 0},	    9, COUNTER, ICMPINECHOS, RONLY, var_icmp },
    {{MIB, 5, 9, 0},	    9, COUNTER, ICMPINECHOREPS, RONLY, var_icmp },
    {{MIB, 5, 10, 0},	    9, COUNTER, ICMPINTIMESTAMPS, RONLY, var_icmp },
    {{MIB, 5, 11, 0},	    9, COUNTER, ICMPINTIMESTAMPREPS, RONLY, var_icmp },
    {{MIB, 5, 12, 0},	    9, COUNTER, ICMPINADDRMASKS, RONLY, var_icmp },
    {{MIB, 5, 13, 0},	    9, COUNTER, ICMPINADDRMASKREPS, RONLY, var_icmp },
    {{MIB, 5, 14, 0},	    9, COUNTER, ICMPOUTMSGS, RONLY, var_icmp },
    {{MIB, 5, 15, 0},	    9, COUNTER, ICMPOUTERRORS, RONLY, var_icmp },
    {{MIB, 5, 16, 0},	    9, COUNTER, ICMPOUTDESTUNREACHS, RONLY, var_icmp },
    {{MIB, 5, 17, 0},	    9, COUNTER, ICMPOUTTIMEEXCDS, RONLY, var_icmp },
    {{MIB, 5, 18, 0},	    9, COUNTER, ICMPOUTPARMPROBS, RONLY, var_icmp },
    {{MIB, 5, 19, 0},	    9, COUNTER, ICMPOUTSRCQUENCHS, RONLY, var_icmp },
    {{MIB, 5, 20, 0},	    9, COUNTER, ICMPOUTREDIRECTS, RONLY, var_icmp },
    {{MIB, 5, 21, 0},	    9, COUNTER, ICMPOUTECHOS, RONLY, var_icmp },
    {{MIB, 5, 22, 0},	    9, COUNTER, ICMPOUTECHOREPS, RONLY, var_icmp },
    {{MIB, 5, 23, 0},	    9, COUNTER, ICMPOUTTIMESTAMPS, RONLY, var_icmp },
    {{MIB, 5, 24, 0},	    9, COUNTER, ICMPOUTTIMESTAMPREPS, RONLY, var_icmp },
    {{MIB, 5, 25, 0},	    9, COUNTER, ICMPOUTADDRMASKS, RONLY, var_icmp },
    {{MIB, 5, 26, 0},	    9, COUNTER, ICMPOUTADDRMASKREPS, RONLY, var_icmp },
    {{MIB, 7, 1, 0},	    9, COUNTER, UDPINDATAGRAMS, RONLY, var_udp }, 
    {{MIB, 7, 2, 0},	    9, COUNTER, UDPNOPORTS, RONLY, var_udp },
    {{MIB, 7, 3, 0},	    9, COUNTER, UDPINERRORS, RONLY, var_udp }, 
    {{MIB, 7, 4, 0},	    9, COUNTER, UDPOUTDATAGRAMS, RONLY, var_udp }
};




/*
 * getStatPtr - return a pointer to the named variable, as well as it's
 * type, length, and access control list.
 *
 * If an exact match for the variable name exists, it is returned.  If not,
 * and exact is false, the next variable lexicographically after the
 * requested one is returned.
 *
 * If no appropriate variable can be found, NULL is returned.
 */
u_char  *
getStatPtr(name, namelen, type, len, acl, exact, access_method)
    oid		*name;	    /* IN - name of var, OUT - name matched */
    int		*namelen;   /* IN -number of sub-ids in name, OUT - subid-is in matched name */
    u_char	*type;	    /* OUT - type of matched variable */
    int		*len;	    /* OUT - length of matched variable */
    u_short	*acl;	    /* OUT - access control list */
    int		exact;	    /* IN - TRUE if exact match wanted */
    int		*access_method; /* OUT - 1 if function, 0 if char * */
{

    register struct variable	*vp;

    register int	x;
    register u_char	*access;
    int			result;

    for(x = 0, vp = variables; x < sizeof(variables)/sizeof(struct variable); vp++, x++){
	/*
	 * compare should be expanded inline.
	 */
	result = compare(name, *namelen, vp->name, (int)vp->namelen);
	if ((result < 0) || (exact && (result == 0))){
	    access = (*(vp->findVar))(vp, name, namelen, exact, len, access_method);
	    if (access != NULL)
		break;
	}
    }
    if (x == sizeof(variables)/sizeof(struct variable))
	return NULL;

    /* vp now points to the approprate struct */
    *type = vp->type;
    *acl = vp->acl;
    return access;
}



int
compare(name1, len1, name2, len2)
    register oid	    *name1, *name2;
    register int	    len1, len2;
{
    register int    len;

    /* len = minimum of len1 and len2 */
    if (len1 < len2)
	len = len1;
    else
	len = len2;
    /* find first non-matching byte */
    while(len-- > 0){
	if (*name1 < *name2)
	    return -1;
	if (*name2++ < *name1++)
	    return 1;
    }
    /* bytes match up to length of shorter string */
    if (len1 < len2)
	return -1;  /* name1 shorter, so it is "less" */
    if (len2 < len1)
	return 1;
    return 0;	/* both strings are equal */
}


u_char *
var_system(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;   /* IN - pointer to variable entry that points here */
    register oid	*name;	    /* IN/OUT - input name requested, output name found */
    register int	*length;    /* IN/OUT - length of input and output oid's */
    int			exact;	    /* IN - TRUE if an exact match was requested. */
    int			*var_len;   /* OUT - length of variable or 0 if function returned. */
    int			*access_method;	/* OUT - 1 if function, 0 if char pointer. */
{
    if (exact && (compare(name, *length, vp->name, (int)vp->namelen) != 0))
	return NULL;
    bcopy((char *)vp->name, (char *)name, (int)vp->namelen * sizeof(oid));
    *length = vp->namelen;
    *access_method = 0;
    *var_len = sizeof(long);	/* default length */
    switch (vp->magic){
	case VERSION_DESCR:
	    *var_len = strlen(version_descr);
	    return (u_char *)version_descr;
	case VERSION_ID:
	    *var_len = sizeof(version_id);
	    return (u_char *)version_id;
	case CFG_NNETS:
	    return (u_char *)&cfg_nnets;
	case UPTIME:
	    return (u_char *)&uptime;
	default:
	    ERROR("");
    }
    return NULL;
}


u_char *
var_ifEntry(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;   /* IN - pointer to variable entry that points here */
    register oid	*name;	    /* IN/OUT - input name requested, output name found */
    register int	*length;    /* IN/OUT - length of input and output oid's */
    int			exact;	    /* IN - TRUE if an exact match was requested. */
    int			*var_len;   /* OUT - length of variable or 0 if function returned. */
    int			*access_method; /* OUT - 1 if function, 0 if char pointer. */
{
    oid			newname[MAX_NAME_LEN];
    register int	interface;
    register struct mib_ifEntry	*ifp;
    extern struct conf	conf;
    int			result;

    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));
    /* find "next" interface */
    for(interface = 1; interface <= MAX_INTERFACES; interface++){
	newname[10] = (oid)interface;
	result = compare(name, *length, newname, (int)vp->namelen);
	if ((exact && (result == 0)) || (!exact && (result < 0)))
	    break;
    }
    if (interface > MAX_INTERFACES)
	return NULL;
    interface--; /* translate into internal index of interfaces */
    bcopy((char *)newname, (char *)name, (int)vp->namelen * sizeof(oid));
    *length = vp->namelen;
    *access_method = 0;
    *var_len = sizeof(long);

    ifp = &mib_ifEntry[interface];
    switch (vp->magic){
        case IFDESCR:
	    *var_len = strlen(ifp->ifDescr);
	    return (u_char *)ifp->ifDescr;
	case IFPHYSADDRESS:
	    *var_len = ifp->PhysAddrLen;
	    if (interface == 0)
	    	return (u_char *)ifie.if_haddr;
	    else {
		/*
		 * As far as IP is concerned, the "physical" address includes the Appletalk
		 * network address as well as node number.
		 */
		return_buf[0] = ((u_char *)&conf.atneta)[0];
		return_buf[1] = ((u_char *)&conf.atneta)[1];
		return_buf[2] = ifab.if_dnode;
	    	return (u_char *)return_buf;
	    }
	case IFOUTQLEN:
#ifdef notdef
	    if (interface == 0)
		long_return = sendq->pq_len;
	    else
		long_return = 0;	/* There is no appletalk transmit queue */
#else
	    long_return = 0;
#endif
	    return (u_char *)&long_return;
	default:
	    return (u_char *)(((char *)ifp) + vp->magic);
    }
}

/* 
 * from arp.c:
 * There is no arp.h, so this must be recreated here.
 */
#define	ARPHLNMAX	6	/* largest arp_hln value needed */
#define	ARPPLN		4	/* length of protocol address (IP) */
struct	arptab {
	iaddr_t at_iaddr;		/* internet address */
	u_char	at_haddr[ARPHLNMAX];	/* hardware address */
	u_char	at_timer;		/* minutes since last reference */
	u_char	at_flags;		/* flags */
	struct	pbuf *at_hold;		/* last packet until resolved/timeout */
};
/* at_flags field values */
#define	ATF_INUSE	1		/* entry in use */
#define ATF_COM		2		/* completed entry (haddr valid) */

#define	ARPTAB_BSIZ	5		/* bucket size */
#define	ARPTAB_NB	11		/* number of buckets */
#define	ARPTAB_SIZE	(ARPTAB_BSIZ * ARPTAB_NB)


u_char *
var_atEntry(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;	/* IN - pointer to variable entry that points here */
    register oid	    *name;	/* IN/OUT - input name requested, output name found */
    register int	    *length;	/* IN/OUT - length of input and output oid's */
    int			    exact;	/* IN - TRUE if an exact match was requested. */
    int			    *var_len;	/* OUT - length of variable or 0 if function returned. */
    int			    *access_method; /* OUT - 1 if function, 0 if char pointer. */
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.3.1.1.1.interface.1.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 12.
     */
    oid			    lowest[16];
    oid			    current[16];
    register struct arptab  *arp;
    struct arptab	    *lowarp = 0;
    extern struct arptab    arptab[];
    register struct ipdad   *ipdp;
    struct ipdad	    *lowipdp = 0;
    extern struct ipdad	    ipdad[];
    long		    ipaddr;
    int			    addrlen;
    extern struct conf	    conf;
    register u_char	    *cp;
    register oid	    *op;
    register int	    count;

    /* fill in object part of name for current (less sizeof instance part) */
    bcopy((char *)vp->name, (char *)current, (int)(vp->namelen - 6) * sizeof(oid));
    for(arp = arptab; arp < arptab + ARPTAB_SIZE; arp++){
	if (!(arp->at_flags & ATF_COM))	/* if this entry isn't valid */
	    continue;
	/* create new object id */
	current[10] = 1;	/* ifIndex == 1 (ethernet) */
	current[11] = 1;
	cp = (u_char *)&(arp->at_iaddr);
	op = current + 12;
	for(count = 4; count > 0; count--)
	    *op++ = *cp++;
	if (exact){
	    if (compare(current, 16, name, *length) == 0){
		bcopy((char *)current, (char *)lowest, 16 * sizeof(oid));
		lowarp = arp;
		break;	/* no need to search further */
	    }
	} else {
	    if ((compare(current, 16, name, *length) > 0) && (!lowarp || (compare(current, 16, lowest, 16) < 0))){
		/*
		 * if new one is greater than input and closer to input than
		 * previous lowest, save this one as the "next" one.
		 */
		bcopy((char *)current, (char *)lowest, 16 * sizeof(oid));
		lowarp = arp;
	    }
	}
    }
    ipaddr = conf.ipaddr + conf.ipstatic + 1;
    for(ipdp = ipdad; ipdp < ipdad + NIPDAD; ipdp++, ipaddr++){
	if (ipdp->timer == 0)	/* if this entry is unused, continue */
	    continue;
	/* create new object id */
	current[10] = 2;	/* ifIndex == 2 (appletalk) */
	current[11] = 1;
	cp = (u_char *)&ipaddr;
	op = current + 12;
	for(count = 4; count > 0; count--)
	    *op++ = *cp++;
	if (exact){
	    if (compare(current, 16, name, *length) == 0){
		bcopy((char *)current, (char *)lowest, 16 * sizeof(oid));
		lowipdp = ipdp;
		lowarp = 0;
		break;	/* no need to search further */
	    }
	} else {
	    if ((compare(current, 16, name, *length) > 0) && ((!lowarp && !lowipdp) || (compare(current, 16, lowest, 16) < 0))){
		/*
		 * if new one is greater than input and closer to input than
		 * previous lowest, save this one as the "next" one.
		 */
		bcopy((char *)current, (char *)lowest, 16 * sizeof(oid));
		lowipdp = ipdp;
		/* ipdad entry is lower, so invalidate arp entry */
		lowarp = 0;
	    }
	}
    }
    if (lowarp != 0){	/* arp entry was lowest */
	addrlen = 6;
	bcopy((char *)lowarp->at_haddr, (char *)return_buf, 6);
    } else if (lowipdp != 0) {
	addrlen = 3;
	/*
	 * As far as IP is concerned, the "physical" address includes the Appletalk
	 * network address as well as node number.
	 */
	return_buf[0] = ((u_char *)&lowipdp->net)[0];
	return_buf[1] = ((u_char *)&lowipdp->net)[1];
	return_buf[2] = lowipdp->node;
    } else
	return NULL;	/* no match */
    bcopy((char *)lowest, (char *)name, 16 * sizeof(oid));
    *length = 16;
    *access_method = 0;
    switch(vp->magic){
	case ATIFINDEX:
	    *var_len = sizeof long_return;
	    long_return = lowest[10];
	    return (u_char *)&long_return;
	case ATPHYSADDRESS:
	    *var_len = addrlen;
	    return (u_char *)return_buf;
	case ATNETADDRESS:
	    *var_len = sizeof long_return;
	    cp = (u_char *)&long_return;
	    op = lowest + 12;
	    for(count = 4; count > 0; count--)
		*cp++ = *op++;
	    return (u_char *)&long_return;
	default:
	    ERROR("");
   }
   return NULL;
}

u_char *
var_ip(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;   /* IN - pointer to variable entry that points here */
    oid	    *name;	    /* IN/OUT - input name requested, output name found */
    int	    *length;	    /* IN/OUT - length of input and output oid's */
    int	    exact;	    /* IN - TRUE if an exact match was requested. */
    int	    *var_len;	    /* OUT - length of variable or 0 if function returned. */
    int	    *access_method; /* OUT - 1 if function, 0 if char pointer. */
{
    if (exact && (compare(name, *length, vp->name, (int)vp->namelen) != 0))
	return NULL;
    bcopy((char *)vp->name, (char *)name, (int)vp->namelen * sizeof(oid));
    *length = vp->namelen;
    *access_method = 0;
    *var_len = sizeof(long);
    return ((u_char *)&mib_ip) + vp->magic;
}

u_char *
var_ipRouteEntry(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;   /* IN - pointer to variable entry that points here */
    register oid    	*name;	    /* IN/OUT - input name requested, output name found */
    register int	*length;    /* IN/OUT - length of input and output strings */
    int			exact;	    /* IN - TRUE if an exact match was requested. */
    int			*var_len;   /* OUT - length of variable or 0 if function returned. */
    int			*access_method; /* OUT - 1 if function, 0 if char pointer. */
{
    oid			    newname[MAX_NAME_LEN];
    register int	    entry;
    register struct mib_ipRouteEntry	*routep;
    int			    result;
    register int	    count;
    register u_char	    *cp;
    register oid	    *op;
    extern struct conf	    conf;

    /* set up a routing table to search. All other values are set at startup. */
    routep = mib_ipRouteEntry;
    routep[ROUTE_DEFAULT].ipRouteDest = 0;
    routep[ROUTE_DEFAULT].ipRouteNextHop = conf.iproutedef;
    routep[ROUTE_LOCAL].ipRouteDest = ipnetpart(conf.ipaddr);
    routep[ROUTE_LOCAL].ipRouteNextHop = conf.ipaddr;

    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));
    /* find "next" route */
    for(entry = 0; entry < ROUTE_ENTRIES; entry++){
	cp = (u_char *)&routep->ipRouteDest;
	op = newname + 10;
	for(count = 4; count > 0; count--)
	    *op++ = *cp++;
	result = compare(name, *length, newname, (int)vp->namelen);
	if ((exact && (result == 0)) || (!exact && (result < 0)))
	    break;
	routep++;
    }
    if (entry >= ROUTE_ENTRIES)
	return NULL;
    bcopy((char *)newname, (char *)name, (int)vp->namelen * sizeof(oid));
    *length = vp->namelen;
    *access_method = 0;
    *var_len = sizeof(long);

    routep = &mib_ipRouteEntry[entry];
    switch (vp->magic){
	case IPROUTENEXTHOP:
	    if (entry == ROUTE_DEFAULT)
		return (u_char *)&conf.iproutedef;
	    else
		return (u_char *)&conf.ipaddr;
	default:
	    return (u_char *)(((u_char *)routep) + vp->magic);
    }
}

u_char *
var_ipAddrEntry(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;    /* IN - pointer to variable entry that points here */
    register oid	*name;	    /* IN/OUT - input name requested, output name found */
    register int	*length;    /* IN/OUT - length of input and output oid's */
    int			exact;	    /* IN - TRUE if an exact match was requested. */
    int			*var_len;   /* OUT - length of variable or 0 if function returned. */
    int			*access_method; /* OUT - 1 if function, 0 if char pointer. */
{
    oid		    newname[14];
    int		    result;
    extern struct conf	conf;
    register int    count;
    register u_char *cp;
    register oid    *op;

    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));
    /* now find "next" ipaddr */
    /*
     * foreach ipaddress entry, cobble up newname with its IP address,
     * by copying the ipaddress into the 10 - 13't subid's
     * then compare with name.  If greater than name and less than lowest,
     * save as new lowest.
     * Having said all that, I'm now going to cheat because I only have one
     * IP address (on both interfaces).
     */
    cp = (u_char *)&conf.ipaddr;
    op = newname + 10;
    for(count = sizeof(conf.ipaddr); count > 0; count--)
	*op++ = *cp++;
    result = compare(name, *length, newname, (int)vp->namelen);
    if ((exact && (result != 0)) || (!exact && (result >= 0)))
	return NULL;	/* no match */
    bcopy((char *)newname, (char *)name, (int)vp->namelen * sizeof(oid));
    *length = vp->namelen;
    *access_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic){
    	case IPADADDR:
	    return (u_char *)&conf.ipaddr;
        case IPADIFINDEX:
	    /*
	     * Always return ethernet interface. SNMP
	     * has no access method to access instances
	     * on different interfaces with same IP address.
	     */
	    long_return = 1;
	    return (u_char *)&long_return;
	case IPADNETMASK:
	    long_return = (IN_CLASSA(conf.ipaddr) ? IN_CLASSA_NET :
			(IN_CLASSB(conf.ipaddr) ? IN_CLASSB_NET : IN_CLASSC_NET));
	    return (u_char *)&long_return;
	case IPADBCASTADDR:
	    long_return = conf.ipbroad & 1;
	    return (u_char *)&long_return;
	default:
	    ERROR("");
    }
    return NULL;
}


u_char *
var_icmp(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;    /* IN - pointer to variable entry that points here */
    oid	    *name;	    /* IN/OUT - input name requested, output name found */
    int	    *length;	    /* IN/OUT - length of input and output oid's */
    int	    exact;	    /* IN - TRUE if an exact match was requested. */
    int	    *var_len;	    /* OUT - length of variable or 0 if function returned. */
    int	    *access_method; /* OUT - 1 if function, 0 if char pointer. */
{
    if (exact && (compare(name, *length, vp->name, (int)vp->namelen) != 0))
	return NULL;
    bcopy((char *)vp->name, (char *)name, (int)vp->namelen * sizeof(oid));
    *length = vp->namelen;
    *access_method = 0;
    *var_len = sizeof(long); /* all following variables are sizeof long */
    switch (vp->magic){
	case ICMPINMSGS:
	    return (u_char *)&mib_icmpInMsgs;
	case ICMPINERRORS:
	    return (u_char *)&mib_icmpInErrors;
	case ICMPINDESTUNREACHS:
	    return (u_char *)&mib_icmpInCount[3];
	case ICMPINTIMEEXCDS:
	    return (u_char *)&mib_icmpInCount[11];
	case ICMPINPARMPROBS:
	    return (u_char *)&mib_icmpInCount[12];
	case ICMPINSRCQUENCHS:
	    return (u_char *)&mib_icmpInCount[4];
	case ICMPINREDIRECTS:
	    return (u_char *)&mib_icmpInCount[5];
	case ICMPINECHOS:
	    return (u_char *)&mib_icmpInCount[8];
	case ICMPINECHOREPS:
	    return (u_char *)&mib_icmpInCount[0];
	case ICMPINTIMESTAMPS:
	    return (u_char *)&mib_icmpInCount[13];
	case ICMPINTIMESTAMPREPS:
	    return (u_char *)&mib_icmpInCount[14];
	case ICMPINADDRMASKS:
	    return (u_char *)&mib_icmpInCount[17];
	case ICMPINADDRMASKREPS:
	    return (u_char *)&mib_icmpInCount[18];
	case ICMPOUTMSGS:
	    return (u_char *)&mib_icmpOutMsgs;
	case ICMPOUTERRORS:
	    return (u_char *)&mib_icmpOutErrors;
	case ICMPOUTDESTUNREACHS:
	    return (u_char *)&mib_icmpOutCount[3];
	case ICMPOUTTIMEEXCDS:
	    return (u_char *)&mib_icmpOutCount[11];
	case ICMPOUTPARMPROBS:
	    return (u_char *)&mib_icmpOutCount[12];
	case ICMPOUTSRCQUENCHS:
	    return (u_char *)&mib_icmpOutCount[4];
	case ICMPOUTREDIRECTS:
	    return (u_char *)&mib_icmpOutCount[5];
	case ICMPOUTECHOS:
	    return (u_char *)&mib_icmpOutCount[8];
	case ICMPOUTECHOREPS:
	    return (u_char *)&mib_icmpOutCount[0];
	case ICMPOUTTIMESTAMPS:
	    return (u_char *)&mib_icmpOutCount[13];
	case ICMPOUTTIMESTAMPREPS:
	    return (u_char *)&mib_icmpOutCount[14];
	case ICMPOUTADDRMASKS:
	    return (u_char *)&mib_icmpOutCount[17];
	case ICMPOUTADDRMASKREPS:
	    return (u_char *)&mib_icmpOutCount[18];
	default:
	    ERROR("");
    }
    return NULL;
}


u_char *
var_udp(vp, name, length, exact, var_len, access_method)
    register struct variable *vp;    /* IN - pointer to variable entry that points here */
    oid	    *name;	    /* IN/OUT - input name requested, output name found */
    int	    *length;	    /* IN/OUT - length of input and output oid's */
    int	    exact;	    /* IN - TRUE if an exact match was requested. */
    int	    *var_len;	    /* OUT - length of variable or 0 if function returned. */
    int	    *access_method; /* OUT - 1 if function, 0 if char pointer. */
{
    if (exact && (compare(name, *length, vp->name, (int)vp->namelen) != 0))
	return NULL;
    bcopy((char *)vp->name, (char *)name, (int)vp->namelen * sizeof(oid));
    *length = vp->namelen;
    *access_method = 0;
    *var_len = sizeof(long);
    return ((u_char *)&mib_udp) + vp->magic;
}
