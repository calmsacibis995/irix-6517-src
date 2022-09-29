/*
 *	Library for mtr/cmd
 */

#undef	ONLY_6_3			/* code only for 6.3, IP32, release */

typedef struct {
        short   junk;
} SCB;

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <net/raw.h>
#include <sys/stream.h>
#include <sys/tr.h>
#include <sys/llc.h>
#include <sys/tr_user.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/if_arp.h>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <mtr.h>

int	GotSignal = 0;			/* sig_handler() reports the received signal */
int	DebugMode = 0;			/* debugging mode or not */
int	DumpMode = 0;			/* control dump, DUMP_...  in mtr.h */
char	*MyName = NULL;			/* program name */
char	*Interface = NULL;		/* interface name */
int	DumpSize = DEFAULT_DUMP_SIZE;	/* default hex dump size */

/*
 *	Log the 'msg' with the given 'log_leve'.
 *
 *	MyName must be defined.
 */
void
Log(int log_level, char	*msg)
{
	char	*log_level_msg;

	switch( log_level ){
	case LOG_ERR:
		log_level_msg = "ERR ";
		break;
	case LOG_WARNING:
		log_level_msg = "Warn";
		break;
	case LOG_INFO:
		log_level_msg = "info";
		break;
	case LOG_DEBUG:
		if( !DebugMode )goto done;
		log_level_msg = "debug";
		break;
	default:
		goto done;
	}

	fprintf(stderr, "%s: %s: %s\n", MyName, log_level_msg, msg);

done:
	return;	
} /* Log() */

/*
 *	Common statements for all the programs.
 */
int
Begin(int argc, char **argv, int root_required)
{
	int		error = NO_ERROR;
	char		*ptr;
	char		msg[MAX_MSG_SIZE];
	struct utsname	utsname;

	/*	No buffering output, kind of real time.	*/
	setbuf(stderr, NULL);
	setbuf(stdout, NULL);

	/*	Only get the program name.	*/
	MyName=argv[0];
	for(ptr = MyName; (ptr = strstr(ptr, "/")) != NULL; MyName = ++ptr ){};

	/*	If real root  is required, only real root is allowed.	*/
	if( root_required == REAL_ROOT_REQUIRED){
		        if( (error = getuid()) ) {
                		/*      Sorry, _REAL_ uid must be root. */
                		sprintf(msg, "permssion denied for %d.", error);
				Log(LOG_ERR, msg);
                		error = 1;
                	goto done;
        	}
	} /* if root_required */

#ifdef	ONLY_6_3
	/*	Must be executed in IP32 with 6.3 release.	*/
        if( uname(&utsname) < 0 ){
                Perror("uname()");
                error = 1;
                goto done;
        }
        if( strcmp(utsname.machine, "IP32") ||
            strncmp(utsname.release, "6.3", 3) ){
                /*      Not the right platform for ...   */
		Log(LOG_ERR, "must be IP32 with 6.3.");
                error = 1;
                goto done;
        }
#endif	/* ONLY_6_3 */

done:
	return( error );
} /* Begin() */

/*
 *	Default signal handler: just record the received signal in 'GotSignal'.
 *
 *	The program which uses this handler is responsible for the actions
 *	taken after receiving the signal.
 */
void
sig_handler(int sig)
{
	char	msg[MAX_MSG_SIZE];

        GotSignal = sig;
        sprintf(msg, "got signal %d.", sig);
	Log(LOG_INFO, msg);

        return;
} /* sig_handler() */

/*
 *	A better version of perror(), with name, error string, and error no.
 */
void
Perror(char *string)
{
	char	msg[MAX_MSG_SIZE];

	
	sprintf(msg, "%s %s[%d].", string, strerror(errno), errno);
	Log(LOG_ERR, msg);

	return;
} /* Perror() */

/*
 *	Convert the MAC address from numeric format to ascii format.
 *
 *	'n': numeric format.
 *	'a': acsii format.
 */
void
tr_ntoa(u_char *n, char *a)
{
	sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x",
		n[0], n[1], n[2], n[3], n[4], n[5]);
	return;
} /* tr_ntoa() */

/*
 *	Convert the MAC address from ascii format to numberic format. 
 *
 *	'a':	ascii format.
 *	'n':	numeric format.
 */
int
tr_aton(char *a, char *n)
{
	int		cnt, idx;
	u_int		tmp[TR_MAC_ADDR_SIZE];

	cnt = sscanf(a, " %x:%x:%x:%x:%x:%x",
		&tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
	if( cnt != TR_MAC_ADDR_SIZE ){
		cnt = sscanf(a, " %x.%x.%x.%x.%x.%x",
			 &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);
	}
	if( cnt == TR_MAC_ADDR_SIZE ) {
		for(idx=0; idx<TR_MAC_ADDR_SIZE; idx++)
			n[idx] = (char)(tmp[idx] & 0xff);
	} else {
		memset(n, 0x0, TR_MAC_ADDR_SIZE );
	}
	return(cnt);
} /* tr_aton() */

/*
 *	MACRO to seach the tables defined by symtable_t which
 *	has 'type' and 'string' two elements. The search is
 *	done by the given 'type'.
 *
 *	'_lim':		the number of elements in the tables.
 *	'_ptr':		the pointer to the table.
 *	'_type':	the "seacrhed" value for 'type' in symtable_t.
 *	'_string':	the result of the seach.
 */
#define GET_NAME_FROM_SYMTABLE(_lim, _ptr, _type, _string )	 \
	{							 \
		int     idx;					 \
		string = "???";					 \
		for( idx = 0; idx < _lim; idx++, (_ptr)++ ){	 \
			if( (_ptr)->type == (_type) ){		 \
				(_string) = (_ptr)->string;	 \
				break;				 \
			} /* if */				 \
		} /* for */					 \
	}

/*
 *	Symtable_t for FC value in TR frame.
 */
static symtable_t 
mac_fcs[] = {
	TR_FC_PCF_EXP,  "exp buf, remove ring station, dup addr test",
	TR_FC_PCF_BCN,  "beacon",
	TR_FC_PCF_CLM,  "claim",
	TR_FC_PCF_PURGE,"ring purge",
	TR_FC_PCF_AMP,  "AMP",
	TR_FC_PCF_SMP,  "SMP",
	0, 		" "
};

/*
 *	Symtable_t for major vector in TR MAC frame.
 */
static symtable_t 
mac_mv_classes[] = {
	TR_CLASS_STA,	   "ring station",
	TR_CLASS_MGR,	   "LLC manager",
	TR_CLASS_CRS,	   "Network Manager(CRS)",
	TR_CLASS_RPS,	   "Ring Parameter Server(RPS)",
	TR_CLASS_REM,	   "Ring Error Monitor(REM)"
};

/*
 *	Symtable_t for command in TR MAC frame.
 */
static symtable_t 
mac_mv_cmds[] = {
	TR_MVC_RESPONSE,	"response",
	TR_MVC_BEACON,	  	"BEACON",
	TR_MVC_CLAIM,	   	"CLAIM TOKEN",
	TR_MVC_PURGE,	   	"RING PURGE",
	TR_MVC_AMP,	   	"AMP",
	TR_MVC_SMP,	   	"SMP",
	TR_MVC_DAT,	   	"dup addr test",
	TR_MVC_LOBE,		"Lobe test",
	TR_MVC_FORWARD,	 	"transmit forward",
	TR_MVC_REMOVE,	  	"REMOVE RING STATION",
	TR_MVC_CHANGE,	  	"change parameters",
	TR_MVC_INIT,	    	"init ring station",
	TR_MVC_REQADDR,	 	"req ring station addr",
	TR_MVC_REQSTATE,	"req ring station state",
	TR_MVC_REQATACH,	"req ring station attach",
	TR_MVC_REQINIT,	 	"req initialization",
	TR_MVC_RESADDR,	 	"report ring station addr",
	TR_MVC_RESSTATE,	"report ring station state",
	TR_MVC_RESATACH,	"report ring station attach",
	TR_MVC_REPNAM,	  	"REPORT NEW AM",
	TR_MVC_REPUNA,	  	"REPORT NAUN CHANGE",
	TR_MVC_REPNNI,	  	"REPORT POLL ERROR",
	TR_MVC_REPAME,	  	"REPORT AM ERROR",
	TR_MVC_REPSE,	   	"REPORT SOFT ERROR",
	TR_MVC_REPFRWD,	 	"report transmit forward"
};

/*
 *	Symtable_t for sub-vector in TR MAC frame.
 */
static symtable_t 
mac_sv_types[] = {
	TR_SVC_BCNTYPE,	 	"beacon type",
	TR_SVC_UNA,	     	"UNA",
	TR_SVC_RINGNUM,	 	"local ring number",
	TR_SVC_ASIGNLOC,	"assign physical drop number",
	TR_SVC_SETIMO,	  	"soft error report time out",
	TR_SVC_EFC,	     	"enable function classes",
	TR_SVC_AAP,	     	"allowed access priority",
	TR_SVC_COREL,	   	"corelator?",
	TR_SVC_LASTUNA,	 	"address of last ring poll",
	TR_SVC_PHYLOC,	  	"physical location",
	TR_SVC_RESPONSE,	"response code",
	TR_SVC_RESERVE,	 	"reserved",
	TR_SVC_PID,	     	"product instance ID",
	TR_SVC_VERS,	    	"adapter s/w level",
	TR_SVC_WRAPDATA,	"wrap data",
	TR_SVC_FRMFWD,	  	"frame forward",
	TR_SVC_SID,	     	"station ID",
	TR_SVC_STATUS,	  	"adapter status word",
	TR_SVC_XMITSTAT,	"xmitstat?",
	TR_SVC_GRPADDR,	 	"group addr",
	TR_SVC_FCNADDR,	 	"func addr",
	TR_SVC_IERRCNT,	 	"isolating err cnt",
	TR_SVC_ERRCNT,	  	"non-isolating err cnt",
	TR_SVC_ERRCODE,	 	"err code"
};

/*
 *	Get the string for the given FC value.
 */
static char *
mac_fc_type( u_char fc_type )
{
	symtable_t	*ptr = mac_fcs;
	char		*string;

	GET_NAME_FROM_SYMTABLE( (sizeof(mac_fcs) / sizeof(*ptr)), 
		ptr, fc_type, string);
	return(string);
} /* mac_fc_type() */

/*
 *	Get the string for the given major vector value.
 */
static char *
mac_mv_class( u_char class )
{
	symtable_t	*ptr = mac_mv_classes;
	char		*string;

	class &= 0x0f;
	GET_NAME_FROM_SYMTABLE((sizeof(mac_mv_classes) / sizeof(*ptr)), 
		ptr, class, string);	
	return(string);
} /* mac_mv_class() */

/*
 *	Get the string for the given command value.
 */
static char *
mac_mv_cmd( u_char cmd )
{
	symtable_t	*ptr = mac_mv_cmds;
	char		*string;

	GET_NAME_FROM_SYMTABLE((sizeof(mac_mv_cmds) / sizeof(*ptr)), 
		ptr, cmd, string);
	return(string);
} /* mac_mv_cmd() */

/*
 *	Get the string for the given sub-vector value.
 */
static char *
mac_sv_type(u_char type )
{
	symtable_t	*ptr = mac_sv_types;
	char		*string;

	GET_NAME_FROM_SYMTABLE((sizeof(mac_sv_types) / sizeof(*ptr)), 
		ptr, type, string);
	return(string);
} /* mac_sv_type() */


/*
 *	dump 'cnt' bytes of data pointed by 'cp'.
 *	the dumped data starts from 'offset' from the original buffer.
 *	When offset < 0, no banner. 
 */
void
dump_hex(caddr_t cp, int cnt, int offset)
{
	int	idx;

	if( cnt < 0 )goto done;
	if( cnt > DumpSize )cnt = DumpSize;
	for( idx = 0; idx < cnt ; idx++ , cp++){
		if( offset >= 0 && !(idx & 0x0f) ) {
			if( idx )fprintf(stdout, "\n");
			fprintf(stdout,"%s%05d:  ", IDENT, (offset+idx));
		}
		fprintf(stdout, "%02x ", *cp);
	}
	if( idx )fprintf(stdout,"\n");

done:
	return;
} /* dump_hex() */

/*
 *	Dump MAC header part of the given frame, pointed by 'pkt_p'.
 *
 *	That includes AC, FC, src MAC addr, dst MAC addr and RI (source routing
 *	information), if any.
 */
void
dump_mac_hdr( TR_MAC *pkt_p )
{
	int		idx, len;
	u_short 	*ptr;

	if( pkt_p == NULL )goto done;
	fprintf(stdout, "%sAC: 0x%x, FC: 0x%x, "
		"dst MAC: 0x%02x:%02x:%02x:%02x:%02x:%02x, "
		"src MAC: 0x%02x:%02x:%02x:%02x:%02x:%02x.\n",
		IDENT,
		pkt_p->mac_ac,
		pkt_p->mac_fc,
		pkt_p->mac_da[0], pkt_p->mac_da[1],
		pkt_p->mac_da[2], pkt_p->mac_da[3],
		pkt_p->mac_da[4], pkt_p->mac_da[5],
		pkt_p->mac_sa[0], pkt_p->mac_sa[1],
		pkt_p->mac_sa[2], pkt_p->mac_sa[3],
		pkt_p->mac_sa[4], pkt_p->mac_sa[5]);

	if( pkt_p->mac_sa[0] & TR_MACADR_RII ){
		tr_packet_t	*pkt2_p = (tr_packet_t *)pkt_p;
		
		/*	Source routing! */
		len = pkt2_p->rii.rii & TR_RIF_LENGTH >> 8;
		fprintf(stdout, "%sRII: 0x%04x",
			IDENT, (u_short) pkt2_p->rii.rii);
	
		ptr = (u_short *)&(pkt2_p->rii.sgt_nos[0]);
		for(idx = 2; idx < len; idx += sizeof(*ptr), ptr++){
			fprintf(stdout, ".%04x", *ptr);
		} /* for */
		fprintf(stdout, "\n");
	}

done:
	return;
} /* dump_mac_hdr() */

/*
 *	Dump the given MAC frame, pointed by pkt_p.
 *
 *	pkt_p:		pointer to MAC header part, including RI, of the MAC frame.
 *	tr_mac_mv_p:	pointer to MAC info part.
 *	pkt_size:	size of the frame, including MAC header.
 *	hdr_size:	size of the header, including other junk to skip.
 *	interface:	ascii string of the interface where the frame is received.
 *	pkt_cnt:	calling routing's frame counter for the received frame.
 *	
 */
void
dump_mac_pkt( TR_MAC *pkt_p, 
	TR_MACMV *tr_mac_mv_p,
	int 	pkt_size, 
	int	hdr_size,
	char 	*interface, 
	int 	pkt_cnt )
{
	int		offset;
	char		msg[MAX_MSG_SIZE];

	if( DebugMode ) dump_hex((caddr_t) pkt_p, pkt_size, 0);

	if( !(DumpMode & DUMP_TR_ALL_EVENTS) ){

		/*	
		 *	Not all the MAC frame so do not report SMP and AMP.
		 */
		if( pkt_p->mac_da[0] == 0xc0 && 
		    pkt_p->mac_da[1] == 0x00 &&
		    pkt_p->mac_da[2] == 0xff && 
		    pkt_p->mac_da[3] == 0xff ){

			/*	No AMP and SMP */
			switch( pkt_p->mac_fc & TR_FC_PCF ){
			case TR_FC_PCF_AMP:
			case TR_FC_PCF_SMP:
				goto done;
	
			default:
				break;
			} /* switch */
		} /* if */
	}

	/*
	 *	Dump MAC header if we really have it.
	 */
	if( pkt_p == NULL || pkt_size < sizeof(TR_MAC) || tr_mac_mv_p == NULL)goto done;
	dump_mac_hdr( pkt_p );	
	offset = hdr_size;

	/*
 	 *	We should never need to skip any data starting from 
	 *	tr_mac_mv_p. Just in case we have bug in other places.
	 */
	while( tr_mac_mv_p->len == 0 && offset < pkt_size ){
		sprintf(msg, "dump_mac_pkt(): tr_mac_mv_p: %d %d %d!",
			tr_mac_mv_p->len, offset, pkt_size);
		Log(LOG_WARNING, msg);
		tr_mac_mv_p = (TR_MACMV *)((caddr_t)tr_mac_mv_p +
			sizeof(tr_mac_mv_p->len));
		offset += sizeof(tr_mac_mv_p->len);
	}
	if( pkt_size == offset ){
		goto done;
	} else if( pkt_size < offset + sizeof(tr_mac_mv_p) ){
		fprintf(stdout, "%sillega size: %d %d %d.\n",
			IDENT, pkt_size, offset, sizeof(tr_mac_mv_p));
		goto done;
	}

	/*
	 *	Report the major vector in the MAC frame.
	 */
	fprintf(stdout,
		"%sMV.length: %d, class: 0x%x[%s.%s], cmd: 0x%x[%s]",
		IDENT,
		tr_mac_mv_p->len,
		tr_mac_mv_p->vclass,
		mac_mv_class( (tr_mac_mv_p->vclass >> 4) & 0x0f),
		mac_mv_class( tr_mac_mv_p->vclass & 0x0f),
		tr_mac_mv_p->cmd,
		mac_mv_cmd( tr_mac_mv_p->cmd ));

	/*
	 *	Skip the major vector and process each subvector, if any.
	 */
	offset += sizeof(TR_MACMV);
	if( pkt_size <= offset ){
		fprintf(stdout, ".\n");
		goto done;
	} else {
		caddr_t		ptr;
		TR_MACSV1	*tr_mac_sv_p;
		u_char		sv_len, sv_type;

		fprintf(stdout, ",\n");
		ptr = (caddr_t)(pkt_p) + offset;

		/*
		 *	for each subvector.
		 */
		for ( ; offset < pkt_size; ptr += sv_len, offset += sv_len ){

			tr_mac_sv_p = (TR_MACSV1 *)ptr;
			sv_len = tr_mac_sv_p->len;
			if( sv_len == 0 ){
				sv_len = 1;
			} else if( sv_len <= sizeof(TR_MACSV1) ){
				fprintf(stdout, "%sSV.length: %d \n",
					IDENT, sv_len);
			} else {
				sv_type= tr_mac_sv_p->svid;
				fprintf(stdout,
					"%sSV.length: %02d, type: 0x%02x[%s] ",
					IDENT,
					sv_len,
					sv_type,
					mac_sv_type(sv_type));

				dump_hex( ptr + sizeof(TR_MACSV1),
					(sv_len - 2), -1);
			} /* if_sv_len */

		} /* for */
	} /* if */

	fprintf(stdout, "\n");
done:
	return;
} /* dump_mac_pkt() */

/*
 *	Dump the snoop information, if any.
 */
void
dump_snoop_hdr( int pkt_cnt, int pkt_size,
	char *who, char *interface, 
	struct snoopheader *sh_p)
{
        struct tm       *tm;

	if( sh_p == NULL )goto done;
        tm = localtime( &sh_p->snoop_timestamp.tv_sec );
        fprintf(stdout, "%s%s: %s %d.%d-th pkt: flags: 0x%x, "
                "len: %d.%d: time: %02d:%02d:%02d.%06d.\n",
		IDENT, who, interface,
                pkt_cnt,
                sh_p->snoop_seq,
                sh_p->snoop_flags,
                sh_p->snoop_packetlen,
		pkt_size,
                tm->tm_hour, tm->tm_min, tm->tm_sec,
                sh_p->snoop_timestamp.tv_usec);
done:
        return;
} /* dump_snoop_hdr() */

/*
 *	Symtable_t for upper layer type in the IP header.
 */
static symtable_t 
ip_uppers[] = {
#ifdef	IPPROTO_ST
	IPPROTO_ST,	"st",
#endif	/* IPPROTO_ST */
#ifdef	IPPROTO_IPIP
	IPPROTO_IPIP,	"multi tunnel",
#endif	/* IPPROTO_IPIP */
#ifdef	IPPROTO_OSPF
	IPPROTO_OSPF,	"ospf",
#endif	/* IPPROTO_OSPF */
	IPPROTO_ICMP,	"icmp",
	IPPROTO_IGMP,	"igmp",
	IPPROTO_GGP,	"ggp",
	IPPROTO_TCP,	"tcp",
	IPPROTO_EGP,	"egp",
	IPPROTO_PUP,	"pup",
	IPPROTO_UDP,	"udp",
	IPPROTO_IDP,	"idp",
	IPPROTO_TP,	"tp",
	IPPROTO_XTP,	"xtp",
#ifdef	IPPROTO_RSVP
	IPPROTO_RSVP,	"rsvp",
#endif	/* IPPROTO_RSVP */
	IPPROTO_HELLO,	"hello",
	IPPROTO_RAW,	"raw"
};

/*
 *	Symtable_t for ICMP type.
 */
static symtable_t
icmps[] = {
	ICMP_ECHOREPLY,		"echo reply",
	ICMP_UNREACH,		"unreach",
	ICMP_SOURCEQUENCH,	"source quench",
	ICMP_REDIRECT,		"redirect",
	ICMP_ECHO,		"echo",
	ICMP_ROUTERADVERT,	"router adv",
	ICMP_ROUTERSOLICIT,	"router solicit",
	ICMP_TIMXCEED,		"time exceed",
	ICMP_PARAMPROB,		"bad ip hdr",
	ICMP_TSTAMP,		"timestamp req",
	ICMP_TSTAMPREPLY,	"timestamp reply",
	ICMP_MASKREQ,		"mask req",
	ICMP_MASKREPLY,		"mask reply"
};

static char *
icmp_type( u_char icmp_type )
{
	symtable_t	*ptr = icmps;
	char		*string;

	GET_NAME_FROM_SYMTABLE( (sizeof(icmps) / sizeof(*ptr)), ptr, icmp_type, string);
	return(string);
} /* icmp_type() */

static char *
ip_up_type( u_char ip_p )
{
	symtable_t		*ptr = ip_uppers;
	char			*string = " ";
	struct protoent		*pp;
	static char		s_buf[80];

	if( (pp = getprotobynumber( (int) ip_p )) != NULL ) {
		strcpy(s_buf, pp->p_name);
	} else {
		GET_NAME_FROM_SYMTABLE( (sizeof(ip_uppers) / sizeof(*ptr)), ptr, 
			ip_p, string);
	}

	return(s_buf);
} /* ip_up_type() */

/*
 *	Dump the standard IPV4 header part.
 */
static void
dump_std_ip_hdr( struct ip *ip_p )
{
	struct hostent		*hp;
	char			tmp[IP_ADDR_STRING_SIZE], tmp2[IP_ADDR_STRING_SIZE];
	char			src_host[80], dst_host[80];

	hp = gethostbyaddr( (char *)&ip_p->ip_src.s_addr, 
		sizeof(ip_p->ip_src.s_addr), AF_INET);
	if( hp != NULL )
		strncpy(src_host, hp->h_name, sizeof(src_host));
	else
		strcpy(src_host, " ");
	hp = gethostbyaddr( (char *)&ip_p->ip_dst.s_addr, 
		sizeof(ip_p->ip_dst.s_addr), AF_INET);
	if( hp != NULL )
		strncpy(dst_host, hp->h_name, sizeof(dst_host));
	else
		strcpy(dst_host, " ");
	fprintf(stdout, "%sIP: 0x%x, hlen: %d, tos: %d, "
		"len: %d, id: %d,\n"
		"%soff: %d, ttl: %d, prot: %d[%s],\n",
		IDENT,
		ip_p->ip_v, ip_p->ip_hl, 	
		ip_p->ip_tos, ip_p->ip_len,
		ip_p->ip_id, 
		IDENT,
		ip_p->ip_off,
		ip_p->ip_ttl,
		ip_p->ip_p,
		ip_up_type(ip_p->ip_p));
	strncpy(tmp, inet_ntoa(ip_p->ip_src), sizeof(tmp));
	strncpy(tmp2, inet_ntoa(ip_p->ip_dst), sizeof(tmp2));
	fprintf(stdout, "%ssrc IP: %s[%s], dst IP: %s[%s].\n",
		IDENT, tmp, src_host, tmp2, dst_host);
	return;
} /* dump_std_ip_hdr() */

/*
 *	Dump the given ICMP packet.
 *
 *	hex_ptr points the begining of the LLC1 frame, no MAC, LLC and SNAP header,
 *	hex_off gives the offset to access the ICMP header.
 *
 */
void
dump_icmp_pkt( caddr_t hex_ptr, int hex_off)
{
	int		hex_cnt;
	char		gw_host[80];
	struct hostent	*hp;
	struct icmp	*icmp_p;

	icmp_p = (struct icmp *)(hex_ptr + hex_off);
	fprintf(stdout, "%sICMP: type: %d[%s], code: %d, ",
		IDENT,
		icmp_p->icmp_type,
		icmp_type(icmp_p->icmp_type),
		icmp_p->icmp_code);

	switch( icmp_p->icmp_type ){
	case ICMP_ECHOREPLY:
	case ICMP_ECHO:
		fprintf(stdout, "id: %04d, seq: %d.\n",
			icmp_p->icmp_id,
			icmp_p->icmp_seq);
		break;

	case ICMP_REDIRECT:
		hp = gethostbyaddr( (char *)&icmp_p->icmp_gwaddr.s_addr, 
			sizeof(icmp_p->icmp_gwaddr.s_addr), AF_INET);
		if( hp != NULL )
			strncpy(gw_host, hp->h_name, sizeof(gw_host));
		else
			strcpy(gw_host, " ");
		fprintf(stdout,"gateway: 0x%x[%s], \n",
			inet_ntoa(icmp_p->icmp_gwaddr), gw_host);
		dump_std_ip_hdr( &icmp_p->icmp_ip );
		hex_off += (4 + sizeof(icmp_p->icmp_ip) + 
			sizeof(icmp_p->icmp_gwaddr));
		hex_ptr += hex_off;	
		hex_cnt =  64 - (4 + sizeof(icmp_p->icmp_ip) +
			sizeof(icmp_p->icmp_gwaddr)); 
		dump_hex( hex_ptr, hex_cnt, hex_off);
		break;

	case ICMP_UNREACH:
	case ICMP_SOURCEQUENCH:
	case ICMP_TIMXCEED:
	case ICMP_PARAMPROB:
		hex_off += 4; 		/* skip icmp hdr part */
		hex_ptr += hex_off;	
		hex_cnt =  64 + 4;
		fprintf(stdout,"\n");
		dump_hex( hex_ptr, hex_cnt, hex_off);
		break;

	case ICMP_MASKREQ:
	case ICMP_MASKREPLY:
		fprintf(stdout, "id: %04d, seq: %d, mask: 0x%x.\n",
			icmp_p->icmp_id,
			icmp_p->icmp_seq,
			icmp_p->icmp_mask);
		break;

	case ICMP_TSTAMP:
	case ICMP_TSTAMPREPLY:
		fprintf(stdout, "id: %04d, seq: %d, \n"
			"%sotime: 0x%x, rtime: 0x%x, ttime: 0x%x.\n",
			icmp_p->icmp_id,
			icmp_p->icmp_seq,
			IDENT,
			icmp_p->icmp_otime,
			icmp_p->icmp_rtime,
			icmp_p->icmp_ttime);
		break;

	case ICMP_ROUTERADVERT:
	case ICMP_ROUTERSOLICIT:
	default:
		fprintf(stdout, ".\n");
		break;
	}

	return;
} /* dump_icmp_pkt() */

/*
 *	Dump the given ARP packet, pointed by arp_p.
 */
void
dump_arp_pkt( struct arphdr *arp_p )
{
	struct hostent	*hp;
	caddr_t		ptr;
	uchar_t		src_mac[TR_MAC_ADDR_SIZE];
	uchar_t		dst_mac[TR_MAC_ADDR_SIZE];
	struct in_addr	src_ip;
	struct in_addr	dst_ip;
	char		tmp[IP_ADDR_STRING_SIZE], tmp2[IP_ADDR_STRING_SIZE];

	fprintf(stdout, 
		"%sARP: hrd: 0x%x[%s], pro: 0x%x[%s], hlen: %d, "
		"plen: %d, op: %d[%s].\n",
		IDENT,
		arp_p->ar_hrd,
		(arp_p->ar_hrd == ARPHRD_802 ? "802" : 
		 (arp_p->ar_hrd == ARPHRD_ETHER ? "ether" : "???")),
		arp_p->ar_pro,
		(arp_p->ar_pro == ETHERTYPE_IP ? "ip" : "???"),
		arp_p->ar_hln,
		arp_p->ar_pln,
		arp_p->ar_op,
		(arp_p->ar_op == ARPOP_REQUEST ? "arp-req" :
		 (arp_p->ar_op == ARPOP_REPLY  ? "arp-reply" : "???"))
		);

	switch( arp_p->ar_pro ){
	case ETHERTYPE_IP:
		ptr = (caddr_t)(arp_p);
		ptr += 8;
		memcpy(src_mac, ptr, sizeof(src_mac));
		ptr	+= sizeof(src_mac);
		memcpy(&src_ip.s_addr,  ptr, sizeof(src_ip));
		ptr	+= sizeof(src_ip);
		memcpy(dst_mac, ptr, sizeof(dst_mac));
		ptr	+= sizeof(dst_mac);
		memcpy(&dst_ip.s_addr,  ptr, sizeof(dst_ip));
		strncpy(tmp, inet_ntoa(src_ip), sizeof(tmp));
		strncpy(tmp2, inet_ntoa(dst_ip), sizeof(tmp2));
		fprintf(stdout,
			"%ssrc ha: 0x%02x:%02x:%02x:%02x:%02x:%02x, "
			"src ip: 0x%08x,\n" 
			"%sdst ha: 0x%02x:%02x:%02x:%02x:%02x:%02x, "
			"dst ip: 0x%08x.\n",
			IDENT,
			src_mac[0], src_mac[1], src_mac[2],
			src_mac[3], src_mac[4], src_mac[5],
			tmp,
			IDENT,
			dst_mac[0], dst_mac[1], dst_mac[2],
			dst_mac[3], dst_mac[4], dst_mac[5],
			tmp2);
		break;

	default:
		break;
	}

	return;
} /* dump_arp_pkt() */

/*
 *	Dump the given IP packet.
 *
 *	hex_ptr points the begining of the LLC1 frame, no MAC, LLC and SNAP header,
 *	hex_off gives the offset to access the IP header.
 */
void
dump_ip_pkt( caddr_t hex_ptr, int hex_off)
{
	struct ip		*ip_p;
	struct udphdr		*udp_p;
	struct tcphdr		*tcp_p;
	struct servent		*sp;
	char			src_port[80], dst_port[80];

	u_int			ip_hlen, tcp_hlen;

	ip_p = (struct ip *)(hex_ptr + hex_off);
	if( DumpMode & DUMP_IP_HDR ){

		dump_std_ip_hdr( ip_p );	

		ip_hlen 	=   (u_int)ip_p->ip_hl;
		ip_hlen		<<= 2;
		if( ip_hlen > DEAULT_IP_HDR_SIZE){
			dump_hex( (hex_ptr + hex_off + 
				DEAULT_IP_HDR_SIZE), 
				(ip_hlen - DEAULT_IP_HDR_SIZE), 
				hex_off + DEAULT_IP_HDR_SIZE);
		}
		hex_off 	+=  ip_hlen;
	} else {
		ip_hlen 	=   (u_int)ip_p->ip_hl;
		ip_hlen		<<= 2;
		hex_off 	+=  ip_hlen;
	}

	switch( ip_p->ip_p ){
	case IPPROTO_UDP:

		udp_p = (struct udphdr *) ((caddr_t)ip_p + ip_hlen);
		if( DumpMode & DUMP_UDP_HDR ) {

			sp = getservbyport( udp_p->uh_sport, "udp");
			if( sp != NULL )
				strcpy(src_port, sp->s_name);
			else 
				strcpy( src_port, " ");
			sp = getservbyport( udp_p->uh_dport, "udp");
			if( sp != NULL )
				strcpy(dst_port, sp->s_name);
			else 
				strcpy( dst_port, " ");
			fprintf(stdout,
				"%sUDP: src UDP port: %d[%s], dst UDP port: %d[%s], "
				"len: %d.\n",
				IDENT,
				udp_p->uh_sport, src_port,
				udp_p->uh_dport, dst_port,
				udp_p->uh_ulen);
		}
		hex_off +=  sizeof(*udp_p);
		break;

	case IPPROTO_TCP:

		tcp_p = (struct tcphdr *) ((caddr_t)ip_p + ip_hlen);
		if( DumpMode & DUMP_TCP_HDR) {

			sp = getservbyport( tcp_p->th_sport, src_port);
			if( sp != NULL )
				strcpy(src_port, sp->s_name);
			else 
				strcpy( src_port, " ");
			sp = getservbyport( tcp_p->th_dport, dst_port);
			if( sp != NULL )
				strcpy(dst_port, sp->s_name);
			else 
				strcpy( dst_port, " ");
			fprintf(stdout,
				"%sTCP: src TCP port: %d[%s], dst TCP port: %d[%s], "
				"seq: 0x%x, ack: 0x%x"
				"flags: 0x%x, win: 0x%x, sum: 0x%x.\n",
				IDENT,
				tcp_p->th_sport, src_port,
				tcp_p->th_dport, dst_port,
				tcp_p->th_seq,	
				tcp_p->th_ack,	
				tcp_p->th_flags,	
				tcp_p->th_win,	
				tcp_p->th_sum);
		}
		tcp_hlen 	=   (u_int)tcp_p->th_off;
		tcp_hlen	<<= 2;
		hex_off 	+=  tcp_hlen;
		break;

	case IPPROTO_ICMP:

		if( DumpMode & DUMP_ICMP ){
			dump_icmp_pkt( hex_ptr, hex_off );
		}
		break;

	default:
		break;
	} /* switch */

	return;
} /* dump_ip_pkt() */
