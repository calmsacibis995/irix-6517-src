#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include "snmplib/snmp.h"
#include "snmplib/asn1.h"
#include "snmplib/snmp_impl.h"
#include "snmplib/snmp_api.h"
#include "snmplib/snmp_client.h"
#include "snmplib/view.h"
#include "snmplib/acl.h"

extern int  errno;
int addronly = 0;  /* If set, displays numeric ip addresses instead of names */
char  hostname[80]; /* Hostname of remote mrouted */
char *prefix   = NULL; /* Instance prefix */
int   table    = 0;    
int   snmp_dump_packet = 0; /* referenced by snmp library */
int   interrupted;

/* Enumerated types */
static char *ifStatus[] = { "up", "down", "disabled" };
static char *ifType[] = { "tunnel", "srcrt", "querier", "subnet" };
static char *mrouteProtocol[] = { "other", "local", "netmgmt",
 "dvmrp", "mospf", "pim", "cbt" };
static char *pimMode[] = { "dense", "sparse" };
static char *hopType[] = { "leaf", "branch" };
static char *trueFalse[] = { "true", "false" };

static struct ivar {
		char   *iv_object;
		char  **iv_values;
		int	    iv_nvalue;
} ivars[] = {
   ".1.3.6.1.3.62.1.1.3.1.2",       /* dvmrpVInterfaceType             */
      ifType, sizeof ifType / sizeof ifType[0],
   ".1.3.6.1.3.62.1.1.3.1.3",       /* dvmrpVInterfaceState            */
      ifStatus, sizeof ifStatus / sizeof ifStatus[0],
   ".1.3.6.1.3.62.1.1.6.1.4",       /* dvmrpRouteNextHopType */
      hopType, sizeof hopType / sizeof hopType[0],
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.11", /* ipMRouteProtocol             */
      mrouteProtocol, sizeof mrouteProtocol / sizeof mrouteProtocol[0],
   ".1.3.6.1.4.1.9.10.3.1.1.4.1.4",  /* pimGroupMode    */
      pimMode, sizeof pimMode / sizeof pimMode[0],
   ".1.3.6.1.4.1.9.10.3.1.1.2.1.4",  /* pimInterfaceMode          */
      pimMode, sizeof pimMode / sizeof pimMode[0],
   ".1.3.6.1.4.1.9.10.3.1.1.3.1.5",  /* pimNeighborMode       */
      pimMode, sizeof pimMode / sizeof pimMode[0],
   ".1.3.6.1.4.1.9.10.1.1.1.2.1.3",  /* igmpCacheSelf         */
      trueFalse, sizeof trueFalse / sizeof trueFalse[0],
		NULL
};

struct row {
   char *var_name;
   int   width;
};

/* List of variables for each type of report */
char *header_vars[] = {
   ".1.3.6.1.2.1.1.5",  /* sysName */
   0
};
char *igmp_cache_vars[] = { 
   ".1.3.6.1.4.1.9.10.1.1.1.2.1.3",  /* igmpCacheSelf         */
   ".1.3.6.1.4.1.9.10.1.1.1.2.1.4",  /* igmpCacheLastReporter */
   ".1.3.6.1.4.1.9.10.1.1.1.2.1.5",  /* igmpCacheUpTime       */
   ".1.3.6.1.4.1.9.10.1.1.1.2.1.6",  /* igmpCacheExpiryTime   */
   0
};
char *cache_vars[] = { 
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.5",  /* ipMRouteInIfIndex            */
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.6",  /* ipMRouteUpTime               */
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.7",  /* ipMRouteExpiryTime           */
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.8",  /* ipMRoutePkts                 */
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.10", /* ipMRouteOctets               */
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.9",  /* ipMRouteDifferentInIfIndexes */
   ".1.3.6.1.4.1.9.10.2.1.1.2.1.11", /* ipMRouteProtocol             */
   0
};
char *pim_if_vars[] = { 
   ".1.3.6.1.4.1.9.10.3.1.1.2.1.2",  /* pimInterfaceAddress       */
   ".1.3.6.1.4.1.9.10.3.1.1.2.1.3",  /* pimInterfaceNetMask       */
   ".1.3.6.1.4.1.9.10.3.1.1.2.1.4",  /* pimInterfaceMode          */
   ".1.3.6.1.4.1.9.10.3.1.1.2.1.5",  /* pimInterfaceDR            */
   ".1.3.6.1.4.1.9.10.2.1.1.4.1.2",  /* ipMRouteInterfaceTtl      */
   ".1.3.6.1.4.1.9.10.3.1.1.2.1.6",  /* pimInterfaceQueryInterval */
   0
};
char *pim_neighbor_vars[] = { 
   ".1.3.6.1.4.1.9.10.3.1.1.3.1.2",  /* pimNeighborIfIndex    */
   ".1.3.6.1.4.1.9.10.3.1.1.3.1.3",  /* pimNeighborUpTime     */
   ".1.3.6.1.4.1.9.10.3.1.1.3.1.4",  /* pimNeighborExpiryTime */
   ".1.3.6.1.4.1.9.10.3.1.1.3.1.5",  /* pimNeighborMode       */
   0
};

/* DVMRP */
char *vif_vars1[] = { 
   ".1.3.6.1.3.62.1.1.3.1.4",       /* dvmrpVInterfaceLocalAddress     */
   ".1.3.6.1.3.62.1.1.3.1.5",       /* dvmrpVInterfaceRemoteAddress    */
   ".1.3.6.1.3.62.1.1.3.1.6",       /* dvmrpVInterfaceRemoteSubnetMask */
   ".1.3.6.1.3.62.1.1.3.1.7",       /* dvmrpVInterfaceMetric           */
   ".1.3.6.1.4.1.9.10.2.1.1.4.1.2", /* ipMRouteInterfaceTtl            */
   ".1.3.6.1.3.62.1.1.3.1.2",       /* dvmrpVInterfaceType             */
   ".1.3.6.1.3.62.1.1.3.1.3",       /* dvmrpVInterfaceState            */
   0
};
char *vif_vars2[] = { 
   ".1.3.6.1.3.62.1.1.3.1.5",       /* dvmrpVInterfaceRemoteAddress    */
   ".1.3.6.1.3.62.1.1.3.1.6",       /* dvmrpVInterfaceRemoteSubnetMask */
   ".1.3.6.1.3.62.1.1.3.1.9",       /* dvmrpVInterfaceInPkts           */
   ".1.3.6.1.3.62.1.1.3.1.10",      /* dvmrpVInterfaceOutPkts          */
   ".1.3.6.1.3.62.1.1.3.1.11",      /* dvmrpVInterfaceInOctets         */
   ".1.3.6.1.3.62.1.1.3.1.12",      /* dvmrpVInterfaceOutOctets        */
   ".1.3.6.1.3.62.1.1.3.1.8",       /* dvmrpVInterfaceRateLimit        */
   ".1.3.6.1.3.62.1.1.3.1.2",       /* dvmrpVInterfaceType             */
   ".1.3.6.1.3.62.1.1.3.1.3",       /* dvmrpVInterfaceState            */
   0
};
char *dvmrp_neighbor_vars[] = { 
   ".1.3.6.1.3.62.1.1.4.1.3",       /* dvmrpNeighborUpTime       */
   ".1.3.6.1.3.62.1.1.4.1.4",       /* dvmrpNeighborExpiryTime   */
   ".1.3.6.1.3.62.1.1.4.1.6",       /* dvmrpNeighborGenerationId */
   ".1.3.6.1.3.62.1.1.4.1.5",       /* dvmrpNeighborVersion      */
   0
};
char *dvmrp_route_vars[] = { 
   ".1.3.6.1.3.62.1.1.5.1.3",       /* dvmrpRouteUpstreamNeighbor */
   ".1.3.6.1.3.62.1.1.5.1.4",       /* dvmrpRouteInVifIndex       */
   ".1.3.6.1.3.62.1.1.5.1.5",       /* dvmrpRouteMetric           */
   ".1.3.6.1.3.62.1.1.5.1.6",       /* dvmrpRouteExpiryTime       */
   0
};
char *dvmrp_routenext_vars[] = { 
   ".1.3.6.1.3.62.1.1.6.1.4",       /* dvmrpRouteNextHopType */
   0
};
char *boundary_vars[] = { 
   ".1.3.6.1.3.62.1.1.7.1.1",       /* dvmrpBoundaryVifIndex */
   0
};
char *pim_group_vars[] = { 
   ".1.3.6.1.4.1.9.10.3.1.1.4.1.4",  /* pimGroupMode    */
   ".1.3.6.1.4.1.9.10.3.1.1.4.1.2",  /* pimGroupRPcount */
   ".1.3.6.1.4.1.9.10.3.1.1.4.1.3",  /* pimGroupRPreach */
   0
};

/* Specification of each type of table */
struct table {
   char   opt;
   char **vars;
   char *header;
   char *body;
} tables[] = {

   'C', cache_vars, "\
IP Multicast Route Table for $0\n\
Mcast-group     Origin-Subnet     InIf  UpTime Tmr   Pkts     Bytes RpF Proto\n\
", "$A(-12):-15 $S(-8,-4):-18 $0:2 $1:8 $2:3 $3:6 $4:9 $5:3 $6",

   'G', pim_group_vars, "\
PIM Group Table for $0\n\
Group-Address      Mode   RPs Tmr\n\
", "$A(-4):-18 $0:-6 $1:3 $2:3",

   'I', pim_if_vars, "\
PIM Interface Table for $0\n\
If Address            Mode   DRouter         Ttl Int\n\
", "$I(-1):2 $S(0,1):-18 $2:-6 $3:-15 $4:3 $5:3",

   'P', pim_neighbor_vars, "\
PIM Neighbor Table for $0\n\
If   UpTime ExpTm Mode   Neighbor\n\
", "$0:2 $1:8 $2:5 $3:-6 $A(-4)",

   'b', boundary_vars, "\
Boundary Table for $0\n\
Vif Boundary Address\n\
", "$0:3 $S(-8,-4):-18", 

   'd', dvmrp_neighbor_vars, "\
DVMRP Neighbor Table for $0\n\
Vif UpTime   ExpTm     GenId Version Neighbor\n\
", "$I(-5):2  $0:8 $1:5 $2:9 $3:-7 $A(-4)",

   'g', igmp_cache_vars, "\
IGMP Group Table for $0\n\
Group-Address   Vif Self    UpTime Tmr Member\n\
", "$A(-5):-15 $I(-1):2  $0:-5 $2:8 $3:3 $A(1)",

   'i', vif_vars1, "\
DVMRP Virtual Interface Table for $0\n\
Vif Local-Address   Met Thr Type    State Remote-Address\n\
", "$I(-1):2  $0:-15 $3:3 $4:3 $5:-7 $6:-5 $S(1,2)",

   'r', dvmrp_route_vars, "\
DVMRP Route Table for $0\n\
Origin-Subnet      IVif Met Tmr  UpstreamNeighbor\n\
", "$S(-8,-4):-18 $1:4 $2:3 $3:3  $A(0)",

   't', dvmrp_routenext_vars, "\
DVMRP Route Next Hop Table for $0\n\
Origin-Subnet      OVif State\n\
", "$S(-9,-5):-18 $I(-1):4 $0",

   'v', vif_vars2, "\
DVMRP Virtual Interface Statistics Table for $0\n\
Vif  InPkts OutPkts    InBytes   OutBytes Rate Type    State Remote-Address\n\
", "$I(-1):2  $2:7 $3:7 $4:10 $5:10 $6:4 $7:-7 $8:-5 $S(0,1):-18",
 
   NULL
};

void
get_ipaddr(buff, var)
   char *buff;
   struct variable_list *var;
{
   u_char *ip = var->val.string;
   sprintf(buff, "%d.%d.%d.%d",ip[0], ip[1], ip[2], ip[3]);
}

void
get_timesecs(buff, var)
   char *buff;
   struct variable_list *var;
{
   sprintf(buff, "%lu", *var->val.integer / 100);
}

void
get_counter(buff, var)
   char *buff;
   struct variable_list *var;
{
   sprintf(buff, "%lu", *var->val.integer);
}

void
get_integer(buff, var)
   char *buff;
   struct variable_list *var;
{
   char objid[256];
   struct ivar *iv;
   int i = *var->val.integer;

   sprint_objid(objid, var->name, var->name_length);
   for (iv = ivars; iv->iv_object && strncmp(objid, iv->iv_object, strlen(iv->iv_object)); iv++);
   if (iv->iv_object) {
      if (i <= 0 || i > iv->iv_nvalue)
         sprintf(buff, "%d", i);
      else
         sprintf(buff, "%s",iv->iv_values[i-1]);
   } else
      sprintf(buff, "%d", i);
}

/*
 * For DisplayString objects, return the string to display
 */
void
get_string(buf, var)
   char *buf;
   struct variable_list *var;
{
   int  x;
   u_char *cp = var->val.string;

   for (x=0; x<var->val_len; x++)
      *buf++ = *cp++;
   *buf = '\0';
}

void
get_value(buf, var)
   char *buf;
   struct variable_list *var;
{
   switch (var->type){
   case ASN_INTEGER:   get_integer(buf, var);          break;
   case ASN_OCTET_STR: get_string(buf, var);           break;
   case IPADDRESS:     get_ipaddr(buf, var);           break;
   case COUNTER:       get_counter(buf, var);          break;
   case TIMETICKS:     get_timesecs(buf, var);         break;
   default:            sprintf(buf, "<unknown type>"); break;
   }
}

struct snmp_pdu *
snmpget(ss, type, names, num_names)
   struct snmp_session *ss;
   int    type;
   char **names;
   int    num_names;
{
   struct snmp_pdu *pdu, *response = NULL;
   int count;
   int name_length;
   oid name[MAX_NAME_LEN];
   int status;
   struct variable_list *vars;

   pdu = snmp_pdu_create(type);

   for(count = 0; count < num_names; count++){
      name_length = MAX_NAME_LEN;
      if (!get_objid(names[count], name, &name_length))
         printf("Invalid object identifier: %s\n", names[count]);
      snmp_add_null_var(pdu, name, name_length);
   }

   for (;;) {
      status = snmp_synch_response(ss, pdu, &response);
      if (status == STAT_SUCCESS) {
         if (response->errstat == SNMP_ERR_NOERROR){
            return response;
         }
      } else if (status == STAT_TIMEOUT) {
         printf("No Response from host\n");
      } else  {    /* status == STAT_ERROR */
         printf("An error occurred, Quitting\n");
      }
      break;
   }

   if (response)
      snmp_free_pdu(response);
   return NULL;
}

void
usage()
{
   int i;
   char buff[256], *p;

   fprintf(stderr, "Usage: mstat [-");
   for (i=0; tables[i].opt; i++)
      putc(tables[i].opt, stderr);
   fprintf(stderr, "n] [-c community] [-p port] [-x prefix] [ host ]\n");
   for (i=0; tables[i].opt; i++) {
      strcpy(buff, tables[i].header);
      if (p = strstr(buff, " for "))
         *p = '\0';
      fprintf(stderr, " -%c            Dump %s", tables[i].opt, buff);
      if (table==i)
         fprintf(stderr, " (default)\n");
      else
         fprintf(stderr, "\n");
   }
   fprintf(stderr, " -n            Display IP addresses in numeric format\n");
   fprintf(stderr, " -c community  Use alternate community string\n");
   fprintf(stderr, " -p port       Use alternate SNMP port number\n");
   fprintf(stderr, " -x prefix     Use instance prefix to limit table\n");
   fprintf(stderr, " host          Host to query.  If none given, use localhost.\n");
   exit(1);
}

char   *
inet_name(addr)
   u_long  addr;
{
   struct hostent *e;
   struct sockaddr_in tmp;
static char buff[80];
static u_long prevaddr = 0;

   if (addr==prevaddr)
      return buff;
   prevaddr = addr;

   if (!addronly) {
      e = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
      if (e) {
         strcpy(buff, e->h_name);
         return buff;
      }
   }

   tmp.sin_addr.s_addr = addr;
   strcpy(buff, inet_ntoa(tmp.sin_addr));
   return buff;
}

/* Get an IP address from an OID starting at element n */
int
get_address(name, length, addr, n)
   oid    *name;
   int     length;
   u_long *addr;
   int     n;
{
   int i;
   int ok = 1;

   (*addr) = 0;

   if (length < n+4)
      return 0;

   for (i=n; i<n+4; i++) {
      (*addr) <<= 8;
      if (i >= length)
          ok = 0;
      else
         (*addr) |= name[i];
   }
   (*addr) = ntohl(*addr);
   return ok;
}

/*
 * Convert an IP subnet number in u_long (network) format into a printable
 * string including the netmask as a number of bits.
 */
char *
inet_fmts(addr, mask, s)
    unsigned long addr, mask;
    char *s;
{
    register u_char *a, *m;
    int bits;

    if ((addr == 0) && (mask == 0)) {
	sprintf(s, "default");
	return (s);
    }
    a = (u_char *)&addr;
    m = (u_char *)&mask;
    bits = 33 - ffs(ntohl(mask));

    if      (m[3] != 0) sprintf(s, "%u.%u.%u.%u/%d", a[0], a[1], a[2], a[3],
						bits);
    else if (m[2] != 0) sprintf(s, "%u.%u.%u/%d",    a[0], a[1], a[2], bits);
    else if (m[1] != 0) sprintf(s, "%u.%u/%d",       a[0], a[1], bits);
    else                sprintf(s, "%u/%d",          a[0], bits);

    return (s);
}

/* 
 * Given a body specification for a table, expand all the macros in it
 * and display the result
 */
void
expand(body, vparr, objid, objidlen)
   char *body;
   struct variable_list *vparr[10];
   oid *objid;
   int  objidlen;
{
   char *p = body;
   int width, n, var, var2, addr, mask;
   char str[80];
   struct sockaddr_in tmp;
   char buff[255];

   while (p && *p) {
      if (*p == '$') {
         p++;
         if (isdigit(*p)) {
            var = *p - '0';
            get_value(str, vparr[var]); 
            p++;
         } else if (*p=='S') {
            var = atoi(p+2);
            p = strchr(p, ',')+1;
            var2 = atoi(p);
            p = strchr(p, ')')+1;

            if (var>=0) {
               get_ipaddr(str, vparr[var]);
               addr = inet_addr(str);
            } else
               get_address(objid, objidlen, &addr,  objidlen+var);

            if (var2>=0) {
               get_ipaddr(str, vparr[var2]);
               mask = inet_addr(str);
            } else
               get_address(objid, objidlen, &mask,  objidlen+var2);

            if (mask)
               inet_fmts(addr, mask, str);
            else 
               strcpy(str, inet_name(addr));
         } else if (*p=='I') { /* retrieve int from oid location (n) */
            n = atoi(p+2);
            p = strchr(p, ')')+1;
            sprintf(str, "%d", objid[ objidlen+n ]);
         } else if (*p=='A') { /* retrieve ipaddr from oid loc (n) */
            var = atoi(p+2);
            p = strchr(p, ')')+1;

            if (var>=0) {
               u_char *ip = vparr[var]->val.string;
               sprintf(str, "%d.%d.%d.%d",ip[0], ip[1], ip[2], ip[3]);
               addr = inet_addr(str);
            } else
               get_address(objid, objidlen, &addr,  objidlen+var);

            strcpy(str, inet_name(addr));
         } else if (*p=='H') { /* destination host */
            strcpy(str, hostname);
            p++;
         }

         width = 0;
         if (*p == ':') {
            p++;
            width = atoi(p);
            p += strspn(p, "-0123456789");
         }
         sprintf(buff, "%*s", width, str);
         if (width<0) /* only truncate for left-justified things */
            buff[-width]='\0';
         printf("%s", buff);
      } else 
         putchar(*p++);
   }
}

main(argc, argv)
   int    argc;
   char **argv;
{
   int    out=0;
   int	  i;
   char   initvar[10][256], 
          var[10][256], *vars[10],
          tmp[256];
   struct variable_list *vp, *vpc, *vparr[10];
   struct snmp_pdu *response;
   struct snmp_session session, *ss;
   char *community = "public";
   int port_flag = 0;
   int dest_port = 0;
   int maskout = 0;
   int	arg;
   char *ap;
   int timeout_flag = 0, timeout, retransmission_flag = 0, retransmission;

   hostname[0] = '\0';

   for (i=0; i<10; i++)
      vars[i] = var[i];

   for(arg = 1; arg < argc; arg++){
      ap = argv[arg];
      if (*ap == '-') {
         while (*++ap) {
            for (i=0; tables[i].opt && tables[i].opt!= *ap; i++);
            if (tables[i].opt) {
               table = i;
               continue;
            }
            switch(*ap) {
            case 'c':
               community = argv[++arg];
               break;
            case 'm':
               maskout = atoi(argv[++arg]);
               break;
            case 'n':
               addronly++;
               break;
            case 'p':
               port_flag++;
               dest_port = atoi(argv[++arg]);
               break;
/*
            case 'r':
               retransmission_flag++;
               retransmission = atoi(argv[++arg]);
               break;
            case 't':
               timeout_flag++;
               timeout = atoi(argv[++arg]) * 1000000L;
               break;
*/
            case 'x':
               prefix = argv[++arg];
               if (prefix == NULL || *prefix == '-')
                  usage();
/*
               if (inet_addr(prefix) == -1) {
                  fprintf(stderr, "%s: invalid address", prefix);
                  exit(-1);
               }
*/
               break;
            default:
               usage();
            }
            continue;
         }
         continue;
      }
      if (!hostname[0])
         strcpy(hostname, argv[arg]);
   }

   if (!hostname[0])
      gethostname(hostname,80);

   bzero((char *)&session, sizeof(struct snmp_session));
   session.peername = hostname;
   if (port_flag)
      session.remote_port = dest_port;
   session.version = SNMP_VERSION_1;
   session.community = (u_char *)community;
   session.community_len = strlen((char *)community);
   if (retransmission_flag)
      session.retries = retransmission;
   else
      session.retries = SNMP_DEFAULT_RETRIES;
   if (timeout_flag)
      session.timeout = timeout;
   else
      session.timeout = SNMP_DEFAULT_TIMEOUT;
   session.authenticator = NULL;
   snmp_synch_setup(&session);
   ss = snmp_open(&session);
   if (ss == NULL){
      printf("Couldn't open snmp\n");
      exit(-1);
   }

   /* Request variables that can go in header */
   for (i=0; header_vars[i]; i++);
   if (response = snmpget(ss, GETNEXT_REQ_MSG, header_vars, i)) {
      vp = response->variables;
      for (i=0, vpc=vp; vpc; i++, vpc=vpc->next_variable) { 
         vparr[i] = vpc;
         sprint_objid(var[i], vpc->name, vpc->name_length);
      }

      /* Display table header */
      expand(tables[table].header, vparr, vp->name, vp->name_length);

      snmp_free_pdu(response);
   }

   /* Initialize column variables */
   for(i=0; tables[table].vars[i]; i++)
      strcpy(initvar[i], tables[table].vars[i]);
   if (prefix) {
      for (i=0; tables[table].vars[i]; i++) {
         strcat(initvar[i], ".");
         strcat(initvar[i], prefix);
      }
   }
   for (i=0; tables[table].vars[i]; i++)
      strcpy(var[i], initvar[i]);

    /* Get and dump the table */
   for (interrupted = 0; !interrupted; ) {
      for (i=0; tables[table].vars[i]; i++);
      response = snmpget(ss, GETNEXT_REQ_MSG, vars, i);
      if (!response) break;
      vp = response->variables;
      if (!vp || interrupted) break;

      sprint_objid(tmp, vp->name, vp->name_length);
      if (strncmp(tmp, initvar[0], strlen(initvar[0])))
          break;

      /* Print the row */
      for (i=0, vpc=vp; vpc; i++, vpc=vpc->next_variable)
         vparr[i] = vpc;
      expand(tables[table].body, vparr, vp->name, vp->name_length);
      printf("\n");

      /* Modify OID if necessary */
      if (maskout) {
         for (vpc=vp; vpc; vpc=vpc->next_variable) {
            for (i= vpc->name_length-maskout; i<vpc->name_length; i++)
               vpc->name[i] = 255;
         }
      }

      /* Save new OID in var[i] */
      for (i=0, vpc=vp; vpc; i++, vpc=vpc->next_variable)
         sprint_objid(var[i], vpc->name, vpc->name_length);

      snmp_free_pdu(response);
      out++;
   }
   if (!out) { /* GETNEXT's failed, try GET's for a 1 row table of scalars */
      for (i=0; tables[table].vars[i]; i++);
      if (response = snmpget(ss, GET_REQ_MSG, tables[table].vars, i)) {
         vp = response->variables;
         if (vp && !interrupted) {
            sprint_objid(tmp, vp->name, vp->name_length);
            if (!strncmp(tmp, initvar[0], strlen(initvar[0]))) {
               for (i=0, vpc=vp; vpc; i++, vpc=vpc->next_variable)
                  vparr[i] = vpc;
               expand(tables[table].body, vparr, vp->name, vp->name_length);
               printf("\n");
            }
         }
         snmp_free_pdu(response);
      }
   }
   snmp_close(ss);
}
