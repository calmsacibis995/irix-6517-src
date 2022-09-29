/*
 * MIB support module for RSVP.
 *
 * $Id: rsvp_mib.c,v 1.1 1998/11/25 08:43:14 eddiem Exp $
 */

#ifdef _MIB

#include <sys/types.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <strings.h>
#include <malloc.h>
#include <sys/sysctl.h>
#include <sys/capability.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <alloca.h>
#include <errno.h>
#include <net/tc_isps.h>
#include <net/rsvp.h>
#include "rsvp_daemon.h"

#include "rsvp_mib.h" 
#ifdef _PEER
#include "peer_mib.h"
#endif


/* 
 * if_vec a vector of all the interfaces rsvpd knows about.  
 * if_num is the number of physical interfaces.
 * vif_num+1 is how many elements are in the vector.  These can include virtual
 * interfaces in case of multicast tunnels(?)
 * if_vec[vif_num] is the API interface.
 */
extern if_rec *if_vec;
extern int if_num;
extern int vif_num;
extern int mib_entry_expire;		/* defined in rsvp_main */

static intSrvIfAttribEntry_t *intSrvIfAttribTable;

static int intSrv_socket;
static int max_ifIndex=0;

static struct timeval starttv;		/* when rsvp MIB module started */
#ifdef _PEER
static void *peer_mgmt_env;
#endif
fd_set peer_mgmt_mask;


/*
 * internal intSrvIfAttribTable functions
 */
int intSrvIfAttribTable_init(void);
int read_kernel_intSrvIfAttribEntry(intSrvIfAttribEntry_t *entryp);
int write_kernel_intSrvIfAttribMaxAllocatedBits(int ifIndex, int maxallocbits);
void zero_intSrvIfAttribEntry(intSrvIfAttribEntry_t *entryp);
intSrvIfAttribEntry_t *find_intSrvIfAttribEntry(int ifIndex);
void dump_intSrvIfAttribEntry(intSrvIfAttribEntry_t *entryp);

/*
 * internal intSrvFlowTable functons
 */
void intSrvFlowTable_init(void);
intSrvFlowEntry_t *find_intSrvFlowEntry(SessionNumber flownum);
intSrvFlowEntry_t *find_intSrvFlowEntry_kernhand(int kernel_handle);
void read_kernel_intSrvFlowEntry(int kernel_handle);
void free_intSrvFlowEntry(intSrvFlowEntry_t *p);
void dump_intSrvFlowEntry(intSrvFlowEntry_t *flowp);

/*
 * internal rsvpSessionTable functions.
 */
void rsvpSessionTable_init(void);
rsvpSessionEntry_t *find_rsvpSessionEntry(SessionNumber sessionnum);
void free_rsvpSessionEntry(rsvpSessionEntry_t *entryp);
void dump_rsvpSessionEntry(rsvpSessionEntry_t *entryp);

/*
 * internal rsvpSenderTable functions.
 */
void rsvpSenderTable_init(void);
rsvpSenderEntry_t * find_rsvpSenderEntry(rsvpSessionEntry_t *sessp,
					 SessionNumber sendernum);
void free_rsvpSenderEntry(rsvpSenderEntry_t *p);
void dump_rsvpSenderEntry(rsvpSenderEntry_t *p);

/*
 * internal rsvpSenderOutInterfaceTable functions.
 */
void rsvpSenderOutInterfaceTable_init(void);
rsvpSenderOutInterfaceEntry_t *find_rsvpSenderOutInterfaceEntry(rsvpSenderEntry_t *sendp, ifIndex ifindex);
int next_ifindex(ifIndex ifindex);
void dump_rsvpSenderOutInterfaceEntry(rsvpSenderEntry_t *sendp);


/*
 * internal rsvpResvTable functions.
 */
void rsvpResvTable_init(void);
rsvpResvEntry_t * find_rsvpResvEntry(rsvpSessionEntry_t *sessp,
					 SessionNumber resvnum);
void free_rsvpResvEntry(rsvpResvEntry_t *p);
void dump_rsvpResvEntry(rsvpResvEntry_t *p);

/*
 * internal rsvpResvFwdTable functions.
 */
void rsvpResvFwdTable_init(void);
rsvpResvFwdEntry_t *find_rsvpResvFwdEntry(rsvpSessionEntry_t *sessp,
					 SessionNumber resvnum);
void free_rsvpResvFwdEntry(rsvpResvFwdEntry_t *p);
void dump_rsvpResvFwdEntry(rsvpResvFwdEntry_t *p);

/*
 * internal rsvpIfTable functions.
 */
rsvpIfEntry_t *find_rsvpIfEntry_ifindex(ifIndex ifindex);
rsvpIfEntry_t *find_rsvpIfEntry_ifvec(int ifvec);
rsvpIfEntry_t *next_rsvpIfEntry(rsvpIfEntry_t *ifp);
void dump_rsvpIfEntry(rsvpIfEntry_t *ifp);
void rsvpIfTable_init(void);

/*
 * internal rsvpNbrTable functions.
 */
void rsvpNbrTable_init(void);
void insert_rsvpNbrEntry(rsvpIfEntry_t *ifp, rsvpNbrEntry_t *nbrp);
int ascend_addr(OCTETSTRING *osp1, OCTETSTRING *osp2);
rsvpNbrEntry_t *find_rsvpNbrEntry(rsvpIfEntry_t *ifp, OCTETSTRING *nbraddrp);
void dump_rsvpNbrEntry(rsvpNbrEntry_t *nbrp);

/*
 * internal rsvpBadPackets functions.
 */
rsvpBadPackets_init(void);

/*
 * General functions.
 */
int mib_ifindex_init(void);
static char *get_ifname(int ifIndex);
char *string_rowstatus(RowStatus s);
char *string_QosService(QosService q);
char *string_TruthValue(TruthValue t);
char *string_RsvpEncapsulation(RsvpEncapsulation e);



int
mib_init(void)
{
#ifdef _PEER
        int retries=3;
#endif
	gettimeofday(&starttv);

	if ((mib_ifindex_init()) < 0)
		return -1;

#ifdef _PEER
	/*
	 * Get the global peer mgmt handle.  We have to do this retry hack
	 * because rsvpd gets started in S30network while the PEER master
	 * daemon gets started in S34snmp.  
	 */
	while (retries) {
	     peer_mgmt_env = mgmt_init_env(NULL, NULL, "RSVP and IntSrv MIB", NULL, NULL);
	     if (peer_mgmt_env)
		  break;
	     retries--;
	     sleep(7);
	}
	if (peer_mgmt_env == NULL)
	     return -1;

	mgmt_select_mask(peer_mgmt_env, &peer_mgmt_mask);
#endif

	if ((intSrvIfAttribTable_init()) < 0)
	     return -1;

	intSrvFlowTable_init();
	rsvpSessionTable_init();
	rsvpSenderTable_init();
	rsvpSenderOutInterfaceTable_init();
	rsvpResvTable_init();
	rsvpResvFwdTable_init();
	rsvpIfTable_init();
	rsvpNbrTable_init();
        rsvpBadPackets_init();

	return 0;
}

void
mib_process_req(void)
{
#ifdef _PEER
     struct timeval timeout;
     int rc=1;

     timeout.tv_sec = 0;
     timeout.tv_usec = 1000;

     while (rc)
	  rc = mgmt_poll(peer_mgmt_env, &timeout);
#endif
}


/*
 * Associate an if_index number with each RSVP interface in the if_vec structure.
 * if_index is used as the table index for many tables.
 * Return 0 if the initialization was OK.  -1 if there was an error.
 *
 * Algorithm: for each entry in the if_vec table, compare the IP address
 * of that if_vec entry with the IP addresses from the kernel buffer.
 * When a match is found, then we know the if_index number of that interface.
 */
int
mib_ifindex_init(void)
{
	int i, found;
	int mib[6];
	size_t buf_size;
	char *buf, *buf_lim, *next, *cp;
	struct ifa_msghdr *ifam;
	struct sockaddr_in *addr;
	struct sockaddr *sa;

	/*
	 * First get the interface list with the if_index numbers from
	 * the kernel.
	 */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &buf_size, NULL, 0) < 0) {
		printf("sysctl 1 error\n");
		return -1;
	}
	buf = (char *)alloca(buf_size);
	if (sysctl(mib, 6, buf, &buf_size, NULL, 0) < 0) {
		printf("sysctl 2 error\n");
		return -1;
	}
	buf_lim = buf + buf_size;

	/* Now start the search */
	for (i=0; i < if_num; i++) {
		found = 0;

		for (next = buf; next < buf_lim; next += ifam->ifam_msglen) {
			ifam = (struct ifa_msghdr *)next;
			if (ifam->ifam_type == RTM_IFINFO)
				continue;

			if ((ifam->ifam_addrs & (RTA_IFA|RTA_NETMASK)) != 
			    (RTA_IFA|RTA_NETMASK))
				continue;

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
	: sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif
	                cp = (char *)(ifam + 1);
			sa = (struct sockaddr *)(ifam + 1);
			ADVANCE(cp, sa);
			addr = (struct sockaddr_in *)cp;

			if (if_vec[i].if_orig.s_addr == addr->sin_addr.s_addr) {
				if_vec[i].if_index = ifam->ifam_index;
				found = 1;
				break;
			}
		}

		if (found == 0) {
			/* we could not find the if_index number for this
			   interface. */
			return -1;
		}
#ifdef _MIB_VERBOSE
		else {
			printf("mib_ifindex_init: if_vec[%d] ", i);
			printf("if_index %d IP addr %s\n", ifam->ifam_index,
			       inet_ntoa(addr->sin_addr));
		}
#endif
	}

	return 0;
}


/*
 ***********************************************************************************
 *
 * Begin intSrvIfAttribTable functions.
 *
 * The intSrvIfAttribTable is a static array of intSrvIfAttribEntry_t's.
 * The entry's are ordered by the field sgi_ifIndex from lowest to highest.
 * Note that sgi_ifIndex may not be contiguous.
 *
 * By definition, two variable values can be modified by the SNMP manager:
 * - intSrvIfAttribMaxAllocatedBits: we support this.
 * - intSrvIfAttribPropagationDelay: must be 0, any attempt to modify will error.
 *
 ***********************************************************************************
 */

int
intSrvIfAttribTable_init(void)
{
     int i, j, lowest, found;

     intSrvIfAttribTable = malloc(sizeof(intSrvIfAttribEntry_t) * if_num);
     bzero(intSrvIfAttribTable, (sizeof(intSrvIfAttribEntry_t) * if_num));

     /*
      * Insert the ifIndex number in increasing order into the table.
      */
     lowest=-1;
     for (i=0; i < if_num; i++) {
	  found = 0;
	  while (found == 0) {
	       lowest++;
	       for (j=0; j < if_num; j++) {
		    if (if_vec[j].if_index == lowest) {
			 intSrvIfAttribTable[i].sgi_ifIndex = lowest;
			 found = 1;
			 break;
		    }
	       }
	  }
     }
     max_ifIndex = lowest;

     intSrv_socket = socket(AF_INET, SOCK_DGRAM, 0);
     if (intSrv_socket == -1) {
	  perror("intSrvIfAttribTable_init socket: ");
	  return -1;
     }

     read_kernel_intSrvIfAttribEntry(NULL);

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_intSrvIfAttribEntry,
		       (void *) intSrvIfAttribTable)) == NULL )
         log(LOG_ERR, 0, "mgmt_new_instance failed for IfAttribEntry\n");
#endif

     return 0;
}


#ifdef _PEER
int
peer_next_intSrvIfAttribEntry(void *ctxt , void **indices)
{
     intSrvIfAttribEntry_t *next_entryp;
     InterfaceIndex index;


     intSrvIfAttribEntry_t *table = (intSrvIfAttribEntry_t *)ctxt;

     if (*indices == NULL) {
	  /*
	   * if indices is NULL, then return the first row in the table, which
	   * is pointed to by table.
	   */
	  next_entryp = table;
     } else {
	  /* 
	   * The master agent has given me an ifIndex.  First find the
	   * row corresponding to the ifIndex, and then get the next one.
	   */
	  index = *( (InterfaceIndex *) indices[0]);
	  next_entryp = find_intSrvIfAttribEntry(index);
	  if (next_entryp) {
	       if (next_entryp->sgi_ifIndex == max_ifIndex)
		    next_entryp = NULL;
	       else
		    next_entryp = next_entryp + 1;
	  }
     }

     /*
      * At this point, next_entryp points at the next row, return the ifIndex
      */
     if (next_entryp) {
	  indices[0] = (void *) &(next_entryp->sgi_ifIndex);
	  return 0;
     }
     
     return (SNMP_ERR_NO_SUCH_NAME);
}

void *
peer_locate_intSrvIfAttribEntry(void *ctxt, void **indices, int op)
{
     InterfaceIndex index;
     intSrvIfAttribEntry_t *entryp;

     intSrvIfAttribEntry_t *table = (intSrvIfAttribEntry_t *)ctxt;

     index = *((InterfaceIndex *)indices[0]);
     entryp = find_intSrvIfAttribEntry(index);
     if (entryp == NULL)
	  return NULL;

     /*
      * Update the values for this row before returning it.
      */
     read_kernel_intSrvIfAttribEntry(entryp);
     return ((void *) entryp);
}


/*
 * Test to see if the proposed new value for this variable is OK.
 * If OK, return 0.  If not, return -1.
 */
int
peer_test_intSrvIfAttribMaxAllocatedBits(void *ctxt, void **indices, void *attr_ref)
{
     intSrvIfAttribEntry_t *entryp;
     int value, rc;

     entryp = (intSrvIfAttribEntry_t *) ctxt;
     value = *((int *) attr_ref);

     /*
      * make sure the requested max allocation does not exceed the interface
      * bandwidth.
      */
     if (value > entryp->sgi_intbandwidth)
	  return -1;

     /*
      * Since we are going to use the PEER generated set function, we have
      * to actually modify the kernel value in the test function.  It's OK,
      * since if we return OK from the test function, the set will definately
      * take place.
      */
     rc = write_kernel_intSrvIfAttribMaxAllocatedBits(entryp->sgi_ifIndex, value);
     return rc;
}


/*
 * Generic test function for variables which are defined as read-write
 * or read-create, but SGI does not support (row) creation or write on
 * those variables.  This just returns -1 if PEER ever asks permission
 * to write to them.
 */
peer_test_generic_fail(void *ctxt, void **indices, void *attr_ref)
{
     return -1;
}


#endif /* _PEER */

/*
 * Update a particular entry in the intSrvIfAttribTable.
 * This function is called by rsvpd when a new flow is installed or
 * a flow is torn down.
 *
 * Returns 0 if OK, -1 on error.
 */
int
rsvpd_update_intSrvIfAttribEntry(int ifIndex, int allocbufs, int numflows)
{
     intSrvIfAttribEntry_t *entryp;

     entryp = find_intSrvIfAttribEntry(ifIndex);
     if (entryp == NULL)
	  return -1;

     read_kernel_intSrvIfAttribEntry(entryp);

     /*
      * If there is no TC or TC has been disabled on this interface, do
      * not update the values.
      */
     if (entryp->intSrvIfAttribStatus != ROWSTATUS_ACTIVE)
	  return -1;

     entryp->intSrvIfAttribAllocatedBuffer = allocbufs;
     entryp->intSrvIfAttribFlows = numflows;

#ifdef _MIB_VERBOSE
     printf("UPDATE ");
     dump_intSrvIfAttribEntry(entryp);
#endif 

     return 0;
} 


/*
 * Call the kernel and update one or all rows in the intSrvIfAttribTable.
 * If entryp is NULL, update all rows.  If entryp is not null, update only that row.
 * Only the kernel knows what the MaxAllocatedBits is for an interface.
 *
 * Returns 0 on success, -1 on error.
 */
int
read_kernel_intSrvIfAttribEntry(intSrvIfAttribEntry_t *entryp)
{
     int i, maxif, do_table;
     char *ifname;
     cap_t		ocap;
     cap_value_t	cap_network_mgt = CAP_NETWORK_MGT;
     struct ispsreq		req, *req_p;
     psif_config_info_t	*configp;

     req_p = (struct ispsreq *) &req;

     if (entryp == NULL) {
	  maxif = max_ifIndex;
	  do_table = 1;
     } else {
	  maxif = 0;
	  do_table = 0;
     }

     for (i=0; i < maxif+1; i++) {
	  if (do_table) {
	       entryp = find_intSrvIfAttribEntry(i);
	       if (entryp == NULL)
		    continue;
	  }

	  bzero(req_p, sizeof(struct ispsreq));

	  ifname = get_ifname(entryp->sgi_ifIndex);
	  if (ifname == NULL) {
	       log(LOG_ERR, 0, "read_kernel_intSrvIfAttribEntry could not get ifname\n");
	       continue;
	  }
	  strcpy(req_p->iq_name, ifname);
	  req_p->iq_function = IF_GET_CONFIG;

	  ocap = cap_acquire(1, &cap_network_mgt);
	  if (ioctl(intSrv_socket, SIOCIFISPSCTL, req_p) < 0) {
	       cap_surrender(ocap);
	       entryp->sgi_flags |= IFATTRIB_NO_TC;
	       entryp->intSrvIfAttribStatus = ROWSTATUS_NOTINSERVICE;
	       zero_intSrvIfAttribEntry(entryp);
	       continue;
	  }
	  cap_surrender(ocap);

	  configp = (psif_config_info_t *) &req_p->iq_config;

	  entryp->sgi_flags &= ~IFATTRIB_NO_TC;

	  if (configp->flags & PSIF_ADMIN_PSOFF) {
	       entryp->sgi_flags |= IFATTRIB_ADMIN_OFF;
	       zero_intSrvIfAttribEntry(entryp);
	       /*
	        * The interface has been disabled by the administrator,
		* but is still ready, hence active
		*/
	       entryp->intSrvIfAttribStatus = ROWSTATUS_ACTIVE;
	       continue;
	  }

	  if (entryp->sgi_flags & IFATTRIB_ADMIN_OFF)
	       entryp->sgi_flags &= ~IFATTRIB_ADMIN_OFF;

	  /*
	   * Just update the info on this entry.
	   */
	  entryp->sgi_intbandwidth = configp->int_bandwidth * 8;
	  entryp->intSrvIfAttribAllocatedBits = configp->cur_resv_bandwidth * 8;
	  entryp->intSrvIfAttribMaxAllocatedBits = configp->max_resv_bandwidth * 8;
	  /*
	   * The next two entries, allocatedBuffer and Flows are updated
	   * by rsvpd when new flows are installed.
	   */
	  entryp->intSrvIfAttribPropagationDelay = 0; /* always 0 for SGI */
	  entryp->intSrvIfAttribStatus = ROWSTATUS_ACTIVE;

#ifdef _MIB_VERBOSE
	  printf("KERNEL READ ");
	  dump_intSrvIfAttribEntry(entryp);
#endif
     }

     return 0;
}


/*
 * Two things in this table are defined to be writeable:
 * MaxAllocatedBits and PropagationDelay.
 *
 * Writing a 0 to MaxAllocatedBits will cause packet scheduling to turn off
 * immediately.  All flows will be canceled.
 * Writing a very small value to MaxAllocatedBits, i.e. 1, will deny future
 * reservation requests, but still allow current flows to continue.
 *
 * Attempts to write to PropagationDelay will return an error since the kernel
 * cannot add propagation delay to packets.
 *
 * Return Values: 0 if OK, -1 on error.
 */
int
write_kernel_intSrvIfAttribMaxAllocatedBits(int ifIndex, int maxallocbits)
{
     cap_t		ocap;
     cap_value_t	cap_network_mgt = CAP_NETWORK_MGT;
     struct ispsreq		*req_p, req;
     psif_config_info_t	*config_p;
     char *ifname;
     intSrvIfAttribEntry_t *entryp;

     entryp = find_intSrvIfAttribEntry(ifIndex);
     if (entryp == NULL)
	  return -1;
     if (entryp->intSrvIfAttribStatus != ROWSTATUS_ACTIVE)
	  return -1;

     ifname = get_ifname(ifIndex);

     req_p = (struct ispsreq *) &req;
     bzero(req_p, sizeof(struct ispsreq));
     strcpy(req_p->iq_name, ifname);
     req_p->iq_function = IF_GET_CONFIG;

     ocap = cap_acquire(1, &cap_network_mgt);
     if (ioctl(intSrv_socket, SIOCIFISPSCTL, req_p) < 0) {
	  cap_surrender(ocap);
	  perror("write_kernel_intSrvIfAttribEntry IF_GET_CONFIG ioctl");
	  entryp->sgi_flags |= IFATTRIB_NO_TC;
	  entryp->intSrvIfAttribStatus = ROWSTATUS_NOTINSERVICE;
	  zero_intSrvIfAttribEntry(entryp);
	  return 0;
     }
     cap_surrender(ocap);

     req_p->iq_function = IF_SET_CONFIG;
     config_p = (psif_config_info_t *) &req_p->iq_config;
     config_p->max_resv_bandwidth = maxallocbits / 8;
     if (maxallocbits == 0)
	  config_p->flags |= PSIF_ADMIN_PSOFF;

     ocap = cap_acquire(1, &cap_network_mgt);
     if (ioctl(intSrv_socket, SIOCIFISPSCTL, req_p) < 0) {
	  cap_surrender(ocap);
	  perror("SIOCIFISPSCTL ioctl: ");
	  exit(1);
     }
     cap_surrender(ocap);

     return 0;
}


void
zero_intSrvIfAttribEntry(intSrvIfAttribEntry_t *entryp)
{
     entryp->intSrvIfAttribAllocatedBits = 0;
     entryp->intSrvIfAttribMaxAllocatedBits = 0;
     entryp->intSrvIfAttribAllocatedBuffer = 0;
     entryp->intSrvIfAttribFlows = 0;
     entryp->intSrvIfAttribPropagationDelay = 0;
}


intSrvIfAttribEntry_t *
find_intSrvIfAttribEntry(int ifIndex)
{
     int i;

     for (i=0; i < if_num; i++) {
	  if (intSrvIfAttribTable[i].sgi_ifIndex == ifIndex)
	       return (&(intSrvIfAttribTable[i]));
     }

     return NULL;
}


void
dump_intSrvIfAttribEntry(intSrvIfAttribEntry_t *entryp)
{
     printf("intSrvIfAttribEntry (0x%x) ifIndex %d ", entryp, entryp->sgi_ifIndex);
     if (entryp->sgi_flags & IFATTRIB_NO_TC)
	  printf("(NO TC) ");
     if (entryp->sgi_flags & IFATTRIB_ADMIN_OFF)
	  printf("(ADMIN OFF) ");
     printf("\n");
     printf("\tInterface bdwth  %9d (bits/sec)\n", entryp->sgi_intbandwidth);

     printf("\tAllocatedBits    %9d\n", entryp->intSrvIfAttribAllocatedBits);
     printf("\tMaxAllocatedBits %9d\n", entryp->intSrvIfAttribMaxAllocatedBits);
     printf("\tAllocatedBuffer  %d (Bytes)\n", entryp->intSrvIfAttribAllocatedBuffer);
     printf("\tFlows            %d\n", entryp->intSrvIfAttribFlows);
     printf("\tPropagationDelay %d\n", entryp->intSrvIfAttribPropagationDelay);
     printf("\tStatus           %s\n", string_rowstatus(entryp->intSrvIfAttribStatus));
     printf("\n");
}


static char *
get_ifname(int ifIndex)
{
     int i;

     for (i=0; i < if_num; i++) {
	  if (if_vec[i].if_index == ifIndex)
	       return if_vec[i].if_name;
     }

     log(LOG_ERR, 0, "get_ifname passed bad ifIndex %d\n", ifIndex);

     return NULL;
}



/*
 *********************************************************************************
 * 
 * Begin IntSrvFlowTable functions. (2)
 * The intSrvFlowTable is a doubly linked list of intSrvFlowEntry_t's.  The head
 * and tail of the list are not linked.
 *
 *********************************************************************************
 */

static intSrvFlowEntry_t *intSrvFlowTable;
static int intSrvFlowNumber;
static TimeStamp intSrvCheckTime;	/* when to check for expired entries */


void
intSrvFlowTable_init(void)
{
     TimeStamp tn;
     struct timeval tv;

     gettimeofday(&tv);
     srand(tv.tv_usec);
     intSrvFlowNumber = rand();

     tn = get_mib_timestamp();
     intSrvCheckTime = tn + 2 * mib_entry_expire * 100;

     intSrvFlowTable = NULL;

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_intSrvFlowEntry,
		       (void *) &intSrvFlowTable)) == NULL )
         log(LOG_ERR, 0, "mgmt_new_instance failed for FlowEntry\n");
#endif 
}


#ifdef _PEER
int
peer_next_intSrvFlowEntry(void *ctxt, void **indices)
{
     intSrvFlowEntry_t *next_entryp;
     SessionNumber flownum;

     /*
      * The standard way to start this procedure is :
      *
      *   intSrvFlowEntry_t *table = (intSrvFlowEntry_t *)ctxt;
      *
      * But since I know exactly which table we are talking about
      * here, I can refer to it directly.
      */
     if (*indices == NULL) {
	  /* 
	   * Return the first entry in the table.
	   */
	  next_entryp = intSrvFlowTable;
     } else {
	  /* 
	   * The master agent has given me a FlowNumber.
	   * Find the row corresponding to the flownum, and then get the next one.
	   */
	  flownum = *((SessionNumber *)indices[0]);
	  next_entryp = find_intSrvFlowEntry(flownum);
	  if (next_entryp)
	       next_entryp = next_entryp->sgi_next;

     }

     /*
      * At this point, next_entryp points at the next row, return the index.
      */
     if (next_entryp) {
	  indices[0] = (void *) &next_entryp->intSrvFlowNumber;
	  return 0;
     }
     
     return (SNMP_ERR_NO_SUCH_NAME);
}


void *
peer_locate_intSrvFlowEntry(void *ctxt, void **indices, int op)
{
     SessionNumber flownum;
     intSrvFlowEntry_t *entryp;

     /* see comments in peer_next_intSrvFlowEntry */
     /* intSrvFlowEntry_t *table = (intSrvFlowEntry_t *)ctxt; */

     flownum = *( (SessionNumber *) indices[0]);
     entryp = find_intSrvFlowEntry(flownum);
     if (entryp == NULL)
	  return NULL;

     /*
      * Update the values for this row before returning it.
      */
     read_kernel_intSrvFlowEntry(entryp->sgi_kernel_handle);
     return ((void *) entryp);
}

#endif /* _PEER */


/*
 * rsvpd has malloc'd a flowentry and filled it out.  Now it gives it to
 * the MIB module to insert into the flowentry table (ordered by intSrvFlowNumber).
 * This entry does not have the full information yet (still missing the address
 * and port numbers), so set the FlowStatus to notReady.
 * When the address and port numbers are available, rsvp_activate_intSrvFlowEntry
 * should be called.
 */
void
rsvpd_insert_intSrvFlowEntry(int kernel_handle, intSrvFlowEntry_t *flowentryp)
{
     intSrvFlowEntry_t *p;
     int unique=0;

     /*
      * Allocate a new flow number and make sure its unique
      */
     while (unique == 0) {
	  if (intSrvFlowNumber == 0x7fffffff)
	       intSrvFlowNumber = 0;
	  else
	       intSrvFlowNumber++;

	  unique = 1;
	  p = intSrvFlowTable;
	  while (p) {
	       if (intSrvFlowNumber < p->intSrvFlowNumber)
		    break;
	       if (p->intSrvFlowNumber == intSrvFlowNumber) {
		    unique = 0;
		    break;
	       }
	       p = p->sgi_next;
	  }
     }


     flowentryp->intSrvFlowNumber = intSrvFlowNumber;
     flowentryp->sgi_kernel_handle = kernel_handle;
     flowentryp->intSrvFlowStatus = ROWSTATUS_NOTREADY;

     /*
      * Insert the flow entry into the table.  The easy case is if the table
      * is empty.
      */
     if (intSrvFlowTable == NULL) {
	  intSrvFlowTable = flowentryp;
	  flowentryp->sgi_next = NULL;
	  flowentryp->sgi_prev = NULL;

     } else if (flowentryp->intSrvFlowNumber < intSrvFlowTable->intSrvFlowNumber) {
	  flowentryp->sgi_prev = NULL;
	  flowentryp->sgi_next = intSrvFlowTable;
	  intSrvFlowTable->sgi_prev = flowentryp;
	  intSrvFlowTable = flowentryp;

     } else {
	  p = intSrvFlowTable;
	  while ((p->sgi_next != NULL) &&
		 (flowentryp->intSrvFlowNumber > p->intSrvFlowNumber)) {
	       p = p->sgi_next;
	  }
	  if (flowentryp->intSrvFlowNumber > p->intSrvFlowNumber) {
	       /* end of the list */
	       flowentryp->sgi_next = NULL;
	       flowentryp->sgi_prev = p;
	       p->sgi_next = flowentryp;
	  } else {
	       /* insert in the middle of the list */
	       flowentryp->sgi_next = p;
	       flowentryp->sgi_prev = p->sgi_prev;
	       p->sgi_prev->sgi_next = flowentryp;
	       p->sgi_prev = flowentryp;
	  }
     }
}


/* 
 * Update the flow entry and set its status to active.
 * This is the second and final step in installing a new flow.
 */
int
rsvpd_activate_intSrvFlowEntry(int kernel_handle, int protocol,
			       struct in_addr *src_inap, int src_port,
			       struct in_addr *dest_inap, int dest_port)
{
     int addrlen, portlen;
     short short_port;
     int int_port;
     intSrvFlowEntry_t *p;

     p = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (p == NULL)
	  return -1;

     /*
      * The IntServ FlowEntry MIB definition does not support the notion
      * of shared explicit filters (multiple senders per flow) well.
      * So if another set of sender addrs are added, I can't store it in
      * this flow entry.  Just ref. count it.
      */
     if (p->intSrvFlowStatus == ROWSTATUS_ACTIVE) {
	  p->sgi_num_filters++;
	  return 0;
     }
     if (p->intSrvFlowStatus == ROWSTATUS_NOTINSERVICE) 
	  return -1;


     if ((p->intSrvFlowType == ctype_SESSION_ipv4) ||
	 (p->intSrvFlowType == ctype_SESSION_ipv4GPI)) {
	  addrlen = 4;
	  portlen = 2;
     } else {
	  addrlen = 16;
	  portlen = 4;
     }

     octetstring_fill(&(p->intSrvFlowSenderAddr), (void *) src_inap, addrlen);
     octetstring_fill(&(p->intSrvFlowDestAddr), (void *) dest_inap, addrlen);

     if (portlen == 2) {
	  short_port = src_port;
	  octetstring_fill(&(p->intSrvFlowPort), (void *) &short_port, 2);
	  short_port = dest_port;
	  octetstring_fill(&(p->intSrvFlowDestPort), (void *) &short_port, 2);
     } else {
	  int_port = src_port;
	  octetstring_fill(&(p->intSrvFlowDestPort), (void *) &int_port, 4);
	  int_port = dest_port;
	  octetstring_fill(&(p->intSrvFlowDestPort), (void *) &int_port, 4);
     }
	  
     p->intSrvFlowProtocol = protocol;
     p->intSrvFlowStatus = ROWSTATUS_ACTIVE;
     p->sgi_num_filters = 1;

#ifdef _MIB_VERBOSE
     dump_intSrvFlowEntry(p);
#endif

     return 0;
}


/*
 * Decrement the ref count on this flow by 1.  If the ref count becomes 0,
 * that means all filters have been removed from this flow.  So set
 * the FlowStatus to notInService.  (Don't free it because we are
 * sending a notification on it.  But when can we free it?)
 */
int
rsvpd_dec_intSrvFlowEntry(int kernel_handle)
{
     intSrvFlowEntry_t *p;

     p = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (p == NULL)
	  return -1;

     if (p->sgi_num_filters <= 1) {
	  p->sgi_num_filters = 0;
	  p->intSrvFlowStatus = ROWSTATUS_NOTINSERVICE;
     } else {
	  p->sgi_num_filters--;
     }

     return 0;
}


/*
 * Return the FlowBurst variable for the flow described by kernel_handle.
 * This just lets the caller look at the flow entry.  The flow entry is still in
 * the table and is not freed.
 *
 * Return -1 if the entry does not exist or is notInService.
 */
int
rsvpd_get_intSrvFlowEntry_bkt(int kernel_handle)
{
     intSrvFlowEntry_t *p;

     p = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (p == NULL) 
	  return -1;
     if (p->intSrvFlowStatus == ROWSTATUS_NOTINSERVICE)
	  return -1;

     return (p->intSrvFlowBurst);
}


/*
 * Set the rate and burst variables for the flow described by kernel_handle.
 *
 * Return -1 if the entry does not exist or is notInService.
 */
int
rsvpd_set_intSrvFlowEntry_rate_bkt(int kernel_handle, int rate, int bkt)
{
     intSrvFlowEntry_t *p;

     p = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (p == NULL) 
	  return -1;
     if (p->intSrvFlowStatus == ROWSTATUS_NOTINSERVICE)
	  return -1;

     p->intSrvFlowRate = rate;
     p->intSrvFlowBurst = bkt;
     return 0;
}


/*
 * Deactivate the flowentry by setting the rowstatus to NOTINSERVICE.
 * Keep the flowentry around for mib_entry_expire because we probably sent
 * a lost flow notification and the system administrator may want to look at it.
 * Eventually, we need to free the entry to keep our memory consumption down.
 */
void 
rsvpd_deactivate_intSrvFlowEntry(int kernel_handle)
{
     intSrvFlowEntry_t *p, *entryp;
     TimeStamp tn, expired;

     p = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (p == NULL)
	  return;

     p->intSrvFlowStatus = ROWSTATUS_NOTINSERVICE;
     p->sgi_timestamp = get_mib_timestamp();

#ifdef _MIB_VERBOSE
     printf("DEACTIVATED the following intSrvFlowEntry:\n");
     printf("num filters on this flow was %d - set to 0\n", p->sgi_num_filters);
     p->sgi_num_filters = 0;
     dump_intSrvFlowEntry(p);
#endif

     /*
      * See if we need to clean up the expired entries.
      * If so, update the time that we need to recheck, and then go 
      * through the table free'ing entries which are in the
      * NOTINSERVICE state and expired.
      */
     tn = get_mib_timestamp();
     if (tn < intSrvCheckTime)
	  return;

     intSrvCheckTime = tn + (mib_entry_expire * 100);
     expired = tn - (mib_entry_expire * 100);

     while ((intSrvFlowTable->intSrvFlowStatus == ROWSTATUS_NOTINSERVICE) &&
	    (intSrvFlowTable->sgi_timestamp < expired)) {
	  p = intSrvFlowTable;
	  intSrvFlowTable = p->sgi_next;
	  if (intSrvFlowTable)
	       intSrvFlowTable->sgi_prev = NULL;
	  free_intSrvFlowEntry(p);
     }

     if (intSrvFlowTable)
	  p = intSrvFlowTable->sgi_next;
     else
	  p = NULL;
     while (p != NULL) {
	  if ((p->intSrvFlowStatus == ROWSTATUS_NOTINSERVICE) &&
	      (p->sgi_timestamp < expired)) {
	       entryp = p;
	       p->sgi_prev->sgi_next = p->sgi_next;
	       if (p->sgi_next)
		    p->sgi_next->sgi_prev = p->sgi_prev;
	       p = p->sgi_next;
	       free_intSrvFlowEntry(entryp);
	  } else {
	       p = p->sgi_next;
	  }
     }
}


/*
 * Return the flow entry associated with the flownum.
 * Return NULL if there is no flow with that flownum.
 */
intSrvFlowEntry_t *
find_intSrvFlowEntry(SessionNumber flownum)
{
     intSrvFlowEntry_t *p;

     p = intSrvFlowTable;
     while (p != NULL) {
	  if (p->intSrvFlowNumber == flownum)
	       return p;
	  p = p->sgi_next;
     }

     return NULL;
}


/*
 * Return the flow entry associated with the kernel handle.
 * Return NULL if there is no flow with that kernel handle.
 */
intSrvFlowEntry_t *
find_intSrvFlowEntry_kernhand(int kernel_handle)
{
     intSrvFlowEntry_t *p;

     p = intSrvFlowTable;
     while (p != NULL) {
	  if (p->sgi_kernel_handle == kernel_handle)
	       return p;
	  p = p->sgi_next;
     }

     return NULL;
}


/*
 * Get kernel packet statistics for this flow.
 */
void
read_kernel_intSrvFlowEntry(int kernel_handle)
{
     cap_t		ocap;
     cap_value_t	cap_network_mgt = CAP_NETWORK_MGT;
     intSrvFlowEntry_t *flowp;
     psif_flow_stats_t *fstatsp;
     struct ispsreq	req, *req_p;
     char *ifname;

     flowp = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (flowp == NULL) {
	  log(LOG_ERR, 0, "read_kernel_intSrvFlowEntry: invalid kernel handle %d",
	      kernel_handle);
	  return;
     }

     /*
      * We keep old flows structures around for a while, so don't
      * ask the kernel about those.
      */
     if (flowp->intSrvFlowStatus != ROWSTATUS_ACTIVE)
	  return;

     req_p = (struct ispsreq *) &req;
     bzero(req_p, sizeof(struct ispsreq));

     ifname = get_ifname(flowp->intSrvFlowInterface);
     strcpy(req_p->iq_name, ifname);
     req_p->iq_function = IF_GET_FLOW_STATS;
     req_p->iq_handle = flowp->sgi_kernel_handle;

     ocap = cap_acquire(1, &cap_network_mgt);
     if (ioctl(intSrv_socket, SIOCIFISPSCTL, req_p) < 0) {
	  cap_surrender(ocap);
	  log(LOG_ERR, 0, "read_kernel_intSrvFlowEntry: ioctl failed for kernel handle %d",
	      kernel_handle);
	  return;
     }
     cap_surrender(ocap);

     fstatsp = (psif_flow_stats_t *) &req_p->iq_config;
     /*
      * for each flow, the kernel keeps fsp->cl_pkts and fsp->nc_pkts.
      * Non conforming packets are not immediately discarded, but they are treated
      * worse than best effort, so its hard to say which field I should set.
      */
     flowp->intSrvFlowBestEffort = fstatsp->nc_pkts;
     flowp->intSrvFlowPoliced = 0;
}


void
free_intSrvFlowEntry(intSrvFlowEntry_t *p)
{
#ifdef _MIB_VERBOSE
     printf("freeing the following intSrvFlowEntry:\n");
     dump_intSrvFlowEntry(p);
#endif
     if (p->intSrvFlowDestAddr.val)
	  free(p->intSrvFlowDestAddr.val);
     if (p->intSrvFlowSenderAddr.val)
	  free(p->intSrvFlowSenderAddr.val);
     if (p->intSrvFlowDestPort.val)
	  free(p->intSrvFlowDestPort.val);
     if (p->intSrvFlowPort.val)
	  free(p->intSrvFlowPort.val);
     if (p->intSrvFlowIfAddr.val)
	  free(p->intSrvFlowIfAddr.val);
     free(p);
}


void
dump_intSrvFlowEntry(intSrvFlowEntry_t *flowp)
{
     struct in_addr ia;
     int len;
     short short_port;
     int int_port;

     printf("Dump of intSrvFlowEntry (sgi_kernel_handle %d): \n",
	    flowp->sgi_kernel_handle);
     printf("\t(sgi_num_filters %d)\n", flowp->sgi_num_filters);
     printf("\tFlowNumber %d (0x%x)\n", flowp->intSrvFlowNumber,
	                                flowp->intSrvFlowNumber);
     printf("\tFlowType %d\n", flowp->intSrvFlowType);
     printf("\tFlowOwner %d\n", flowp->intSrvFlowOwner);

     len = flowp->intSrvFlowDestAddrLength;
     if (flowp->intSrvFlowDestAddr.val == NULL) {
	  printf("\tFlowDestAddr : undefined\n");
     } else {
	  bcopy((void *) flowp->intSrvFlowDestAddr.val, &ia, len);
	  printf("\tFlowDestAddr %s\n", inet_ntoa(ia));
     }

     if (flowp->intSrvFlowDestPort.val == NULL) {
	  printf("\tFlowDestPort : undefined\n");
     } else if (flowp->intSrvFlowDestPort.len == 2) {
	  bcopy((void *) (flowp->intSrvFlowDestPort.val), &short_port, 2);
	  printf("\tFlowDestPort %d\n", short_port);
     } else {
	  bcopy((void *) (flowp->intSrvFlowDestPort.val), &int_port, 4);
	  printf("\tFlowDestPort %d\n", int_port);
     }

     printf("\tFlowDestAddrLength %d\n", len);

     len = flowp->intSrvFlowSenderAddrLength;
     if (flowp->intSrvFlowSenderAddr.val == NULL) {
	  printf("\tFlowDestAddr : undefined\n");
     } else {
	  bcopy((void *) flowp->intSrvFlowSenderAddr.val, &ia, len);
	  printf("\tFlowSenderAddr %s\n", inet_ntoa(ia));
     }

     if (flowp->intSrvFlowDestPort.val == NULL) {
	  printf("\tFlowPort : undefined\n");
     } else if (flowp->intSrvFlowPort.len == 2) {
	  bcopy((void *) (flowp->intSrvFlowPort.val), &short_port, 2);
	  printf("\tFlowPort %d\n", short_port);
     } else {
	  bcopy((void *) (flowp->intSrvFlowPort.val), &int_port, 4);
	  printf("\tFlowPort %d\n", int_port);
     }

     printf("\tFlowSenderAddrLength %d\n", flowp->intSrvFlowSenderAddrLength);

     printf("\tFlowInterface %d\n", flowp->intSrvFlowInterface);
     if (flowp->intSrvFlowIfAddr.val == NULL) {
	  printf("\tFlowIfAddr : undefined\n");
     } else {
	  bcopy((void *) flowp->intSrvFlowIfAddr.val, &ia,
		flowp->intSrvFlowIfAddr.len);
	  printf("\tFlowIfAddr %s\n", inet_ntoa(ia));
     }
     printf("\tFlowRate %d (bits/sec)\n", flowp->intSrvFlowRate);
     printf("\tFlowBurst %d (bytes)\n", flowp->intSrvFlowBurst);
     printf("\tFlowWeight %d\n", flowp->intSrvFlowWeight);
     printf("\tFlowQueue %d\n", flowp->intSrvFlowQueue);
     printf("\tFlowMinTU %d\n", flowp->intSrvFlowMinTU);
     printf("\tFlowMaxTU %d\n", flowp->intSrvFlowMaxTU);
     printf("\tFlowBestEffort %d\n", flowp->intSrvFlowBestEffort);
     printf("\tFlowPoliced %d\n", flowp->intSrvFlowPoliced);
     printf("\tFlowDiscard : %s\n", string_TruthValue(flowp->intSrvFlowDiscard));
     printf("\tFlowService : %s\n", string_QosService(flowp->intSrvFlowService));
     printf("\tFlowOrder %d\n", flowp->intSrvFlowOrder);
     printf("\tFlowStatus : %s\n", string_rowstatus(flowp->intSrvFlowStatus));
     printf("\n");
}


/*
 ***********************************************************************************
 *
 * Begin rsvpSessionTable functions. (1)
 *
 ***********************************************************************************
 */

static rsvpSessionEntry_t *rsvpSessionTable;
static int rsvpSessionNumber;
static TimeStamp rsvpSessionCheckTime;

void
rsvpSessionTable_init(void)
{
     TimeStamp tn;
     struct timeval tv;

     gettimeofday(&tv);
     srand(tv.tv_usec);
     rsvpSessionNumber = rand();

     tn = get_mib_timestamp();
     rsvpSessionCheckTime = tn + 2 * mib_entry_expire * 100;

     rsvpSessionTable = NULL;

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpSessionEntry,
		       (void *) &rsvpSessionTable)) == NULL )
         log(LOG_ERR, 0, "PEER mgmt_new_instance failed for rsvpSessionTable\n");
#endif 
}


#ifdef _PEER
int
peer_next_rsvpSessionEntry(void *ctxt, void **indices)
{
     rsvpSessionEntry_t *next_entryp;
     SessionNumber sessionnum;

     /*
      * same comments as peer_next_intSrvFlowEntry apply here.
      * i.e. I don't need to use the ctxt pointer.  I already know which
      * table we are talking about.
      */
     if (*indices == NULL) {
	  /* 
	   * Return the first entry in the table.
	   */
	  next_entryp = rsvpSessionTable;
     } else {
	  /* 
	   * The master agent has given me a FlowNumber.
	   * Find the row corresponding to the flownum, and then get the next one.
	   */
	  sessionnum = *((SessionNumber *)indices[0]);
	  next_entryp = find_rsvpSessionEntry(sessionnum);
	  if (next_entryp)
	       next_entryp = next_entryp->sgi_next;
     }

     /*
      * At this point, next_entryp points at the next row, return the index.
      */
     if (next_entryp) {
	  indices[0] = (void *) &(next_entryp->rsvpSessionNumber);
	  return 0;
     }
     
     return (SNMP_ERR_NO_SUCH_NAME);
}


void *
peer_locate_rsvpSessionEntry(void *ctxt, void **indices, int op)
{
     SessionNumber sessionnum;
     rsvpSessionEntry_t *entryp;

     sessionnum = *( (SessionNumber *) indices[0]);
     entryp = find_rsvpSessionEntry(sessionnum);

     /*
      * The values in this entry are always up to date, so just return it.
      */
     return ((void *) entryp);
}

#endif /* _PEER */


/*
 * rsvpd has malloc'd a session entry and filled it out.  rsvpd now gives it
 * to this function.  This function is responsible for giving the entry
 * a new session number, and inserting this entry into the rsvpSessionTable
 * in order of session number.
 */
void
rsvpd_insert_rsvpSessionEntry(rsvpSessionEntry_t *entryp)
{
     rsvpSessionEntry_t *p;
     int unique=0;

     /*
      * Allocate a new session number and make sure its unique
      */
     while (unique == 0) {
	  if (rsvpSessionNumber == 0x7fffffff)
	       rsvpSessionNumber = 0;
	  else
	       rsvpSessionNumber++;

	  unique = 1;
	  p = rsvpSessionTable;
	  while (p) {
	       if (rsvpSessionNumber < p->rsvpSessionNumber)
		    break;
	       if (rsvpSessionNumber == p->rsvpSessionNumber) {
		    unique = 0;
		    break;
	       }
	       p = p->sgi_next;
	  }
     }

     entryp->rsvpSessionNumber = rsvpSessionNumber;
     entryp->sgi_status = ROWSTATUS_ACTIVE;

     /*
      * Insert the session entry into the table ordered by session number.
      * The easy case is if the table is empty.
      */
     if (rsvpSessionTable == NULL) {
	  /* easy case, table is empty */
	  rsvpSessionTable = entryp;
	  entryp->sgi_next = NULL;
	  entryp->sgi_prev = NULL;

     } else if (entryp->rsvpSessionNumber < rsvpSessionTable->rsvpSessionNumber) {
	  /* session entry goes to the head of the list */
	  entryp->sgi_prev = NULL;
	  entryp->sgi_next = rsvpSessionTable;
	  rsvpSessionTable->sgi_prev = entryp;
	  rsvpSessionTable = entryp;

     } else {
	  p = rsvpSessionTable;
	  while ((p->sgi_next != NULL) &&
		 (entryp->rsvpSessionNumber > p->rsvpSessionNumber)) {
	       p = p->sgi_next;
	  }
	  if (entryp->rsvpSessionNumber > p->rsvpSessionNumber) {
	       /* end of the list */
	       entryp->sgi_next = NULL;
	       entryp->sgi_prev = p;
	       p->sgi_next = entryp;
	  } else {
	       /* insert in the middle of the list */
	       entryp->sgi_next = p;
	       entryp->sgi_prev = p->sgi_prev;
	       p->sgi_prev->sgi_next = entryp;
	       p->sgi_prev = entryp;
	  }
     }

#ifdef _MIB_VERBOSE
     dump_rsvpSessionEntry(entryp);
#endif
}


/*
 * Deactivate the Session entry by setting the rowstatus to NOTINSERVICE.
 * Keep the entry around for mib_entry_expire because we probably sent
 * a lost flow notification and the system administrator may want to look at it.
 * Eventually, we need to free the entry to keep our memory consumption down.
 */
void 
rsvpd_deactivate_rsvpSessionEntry(void *sessionhandle)
{
     rsvpSessionEntry_t *p, *entryp;
     TimeStamp tn, expired;

     p = (rsvpSessionEntry_t *) sessionhandle;
     p->sgi_status = ROWSTATUS_NOTINSERVICE;
     p->sgi_timestamp = get_mib_timestamp();

#ifdef _MIB_VERBOSE
     printf("deactivated the following rsvpSessionEntry:\n");
     dump_rsvpSessionEntry(p);
#endif

     /*
      * See if we need to clean up the expired entries.
      * If so, update the time that we need to recheck, and then go 
      * through the table free'ing entries which are in the
      * NOTINSERVICE state and expired.
      */
     tn = get_mib_timestamp();
     if (tn < rsvpSessionCheckTime)
	  return;

     rsvpSessionCheckTime = tn + (mib_entry_expire * 100);
     expired = tn - (mib_entry_expire * 100);

     while ((rsvpSessionTable->sgi_status == ROWSTATUS_NOTINSERVICE) &&
	    (rsvpSessionTable->sgi_timestamp < expired)) {
	  p = rsvpSessionTable;
	  rsvpSessionTable = p->sgi_next;
	  if (rsvpSessionTable)
	       rsvpSessionTable->sgi_prev = NULL;
	  free_rsvpSessionEntry(p);
     }

     if (rsvpSessionTable)
	  p = rsvpSessionTable->sgi_next;
     else
	  p = NULL;
     while (p != NULL) {
	  if ((p->sgi_status == ROWSTATUS_NOTINSERVICE) &&
	      (p->sgi_timestamp < expired)) {
	       entryp = p;
	       p->sgi_prev->sgi_next = p->sgi_next;
	       if (p->sgi_next)
		    p->sgi_next->sgi_prev = p->sgi_prev;
	       p = p->sgi_next;
	       free_rsvpSessionEntry(entryp);
	  } else {
	       p = p->sgi_next;
	  }
     }
}


/*
 * Add the value of inc to the rsvpSessionSenders field of the session.
 * inc should be either a 1 or -1, although this is not checked.
 */
void
rsvpd_mod_rsvpSessionSenders(void *sessionhandle, int inc)
{
     rsvpSessionEntry_t *entryp = (rsvpSessionEntry_t *) sessionhandle;

     if ((entryp->rsvpSessionSenders == 0xffffffff) && (inc > 0))
	  return;
     if ((entryp->rsvpSessionSenders == 0) && (inc < 0))
	  return;

     entryp->rsvpSessionSenders += inc;

#ifdef _MIB_VERBOSE
     dump_rsvpSessionEntry(entryp);
#endif
}


/*
 * Add the value of inc to the rsvpSessionReceivers field of the session.
 * inc should be either a 1 or -1, although this is not checked.
 */
void
rsvpd_mod_rsvpSessionReceivers(void *sessionhandle, int inc)
{
     rsvpSessionEntry_t *entryp = (rsvpSessionEntry_t *) sessionhandle;

     if ((entryp->rsvpSessionReceivers == 0xffffffff) && (inc > 0))
	  return;
     if ((entryp->rsvpSessionReceivers == 0) && (inc < 0))
	  return;

     entryp->rsvpSessionReceivers += inc;

#ifdef _MIB_VERBOSE
     dump_rsvpSessionEntry(entryp);
#endif
}


/*
 * Add the value of inc to the rsvpSessionRequests field of the session
 * inc should be either a 1 or -1, although this is not checked.
 */
void
rsvpd_mod_rsvpSessionRequests(void *sessionhandle, int inc)
{
     rsvpSessionEntry_t *entryp = (rsvpSessionEntry_t *) sessionhandle;

     if ((entryp->rsvpSessionRequests == 0xffffffff) && (inc > 0))
	  return;
     if ((entryp->rsvpSessionRequests == 0) && (inc < 0))
	  return;

     entryp->rsvpSessionRequests += inc;

#ifdef _MIB_VERBOSE
     dump_rsvpSessionEntry(entryp);
#endif
}


/*
 * Return the rsvp session entry associated with the sessionnum.
 * Return NULL if there is no session with that sessionnum.
 */
rsvpSessionEntry_t *
find_rsvpSessionEntry(SessionNumber sessionnum)
{
     rsvpSessionEntry_t *p;

     p = rsvpSessionTable;
     while (p != NULL) {
	  if (p->rsvpSessionNumber == sessionnum)
	       return p;
	  p = p->sgi_next;
     }

     return NULL;
}


/*
 * Free the rsvpSessionEntry.
 * Since the sender, resv, and resv fwd entries are linked off of the
 * session entry, when we free a session entry, free all the linked entries
 * as well.  They should be in the deactivated state also.
 */
void
free_rsvpSessionEntry(rsvpSessionEntry_t *p)
{
     rsvpSenderEntry_t *sendp;
     rsvpResvEntry_t *resvp;
     rsvpResvFwdEntry_t *fwdp;

#ifdef _MIB_VERBOSE
     printf("Freeing the following Session Entry\n");
     dump_rsvpSessionEntry(p);
#endif

     while (p->sgi_senderp) {
	  sendp = p->sgi_senderp;
	  p->sgi_senderp = p->sgi_senderp->sgi_next;
	  free_rsvpSenderEntry(sendp);
     }

     while (p->sgi_resvp) {
	  resvp = p->sgi_resvp;
	  p->sgi_resvp = p->sgi_resvp->sgi_next;
	  free_rsvpResvEntry(resvp);
     }

     while (p->sgi_resvfwdp) {
	  fwdp = p->sgi_resvfwdp;
	  p->sgi_resvfwdp = p->sgi_resvfwdp->sgi_next;
	  free_rsvpResvFwdEntry(fwdp);
     }

     if (p->rsvpSessionDestAddr.val)
	  free(p->rsvpSessionDestAddr.val);
     if (p->rsvpSessionPort.val)
	  free(p->rsvpSessionPort.val);
     free(p);
}


void
dump_rsvpSessionEntry(rsvpSessionEntry_t *entryp)
{
     struct in_addr ia;
     short v4_port;
     int v6_port;

     printf("Dump of rsvpSessionEntry %d (0x%x)\n", entryp->rsvpSessionNumber,
	    entryp->rsvpSessionNumber);
     printf("\trsvpSessionType : %d\n", entryp->rsvpSessionType);
     bcopy((void *) (entryp->rsvpSessionDestAddr.val), &ia,
	                                         entryp->rsvpSessionDestAddr.len);
     printf("\trsvpSessionDestAddr %s\n", inet_ntoa(ia));
     printf("\trsvpSessionDestAddrLength : %d\n", entryp->rsvpSessionDestAddrLength);
     if (entryp->rsvpSessionPort.len == 2) {
	  bcopy((void *) (entryp->rsvpSessionPort.val), &v4_port, 2);
	  printf("\trsvpSessionPort : %d\n", v4_port);
     } else {
	  bcopy((void *) (entryp->rsvpSessionPort.val), &v6_port, 4);
	  printf("\trsvpSessionPort : %d\n", v6_port);
     }
     printf("\trsvpSessionSenders : %d\n", entryp->rsvpSessionSenders);
     printf("\trsvpSessionReceivers : %d\n", entryp->rsvpSessionReceivers);
     printf("\trsvpSessionRequests : %d\n", entryp->rsvpSessionRequests);
     printf("\n");
     fflush(stdout);
}


/*
 ***********************************************************************************
 *
 * Begin rsvpSenderTable functions. (2)
 * 
 * Since each sender is associated and indexed by the session to which it
 * is sending, the sender entries are chained off of the rsvpSessionEntry
 * structure.  This means that there are multiple disconnected sender entries,
 * and rsvpSenderTable is not even used.  This is true for rsvpResvTable,
 * and rsvResvFwdTables also.
 * Logically, all the senders are still in a single table.
 *
 ***********************************************************************************
 */

static rsvpSenderEntry_t *rsvpSenderTable;
static int rsvpSenderNumber;
static TimeStamp rsvpSenderCheckTime;

void
rsvpSenderTable_init(void)
{
     TimeStamp tn;
     struct timeval tv;

     gettimeofday(&tv);
     srand(tv.tv_usec);
     rsvpSenderNumber = rand();

     tn = get_mib_timestamp();
     rsvpSenderCheckTime = tn + 2 * mib_entry_expire * 100;

     rsvpSenderTable = NULL;

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpSenderEntry,
		       (void *) (&rsvpSenderTable))) == NULL )
         log(LOG_ERR, 0, "PEER mgmt_new_instance failed for rsvpSenderTable\n");
#endif 
}


#ifdef _PEER
int
peer_next_rsvpSenderEntry(void *ctxt, void **indices)
{
     rsvpSessionEntry_t *sessp;
     rsvpSenderEntry_t *sendp=NULL;
     SessionNumber sessionnum, sendernum;

     if (indices[0] == NULL) {
	  /*
	   * Return the first sender, which really means return the first
	   * sender of the first session.  There should be at least one
	   * sender for every session, but just in case.... I'll will not
	   * assume that.
	   */
	  sessp = rsvpSessionTable;
	  while (sessp) {
	       if (sessp->sgi_senderp) {
		    sendp = sessp->sgi_senderp;
		    break;
	       }
	       sessp = sessp->sgi_next;
	  }
     } else {
	  /*
	   * The master agent has given me a {SessionNumber, SenderNumber} index.
	   * Find that sender entry, and return the next one (which could
	   * be the first sender on the next session).
	   */
	  sessionnum = *((SessionNumber *) indices[0]);
	  
	  sessp = find_rsvpSessionEntry(sessionnum);
	  if (sessp) {
	       if (indices[1] == NULL) {
		    sendp = sessp->sgi_senderp;
	       } else {
		    sendernum = *((SessionNumber *) indices[1]);
		    sendp = find_rsvpSenderEntry(sessp, sendernum);
		    if (sendp)
			 sendp = sendp->sgi_next;
	       }

	       /*
		* If sendp is not NULL, then we are done.
		* If it is NULL, we have to search through the following
		* sessions for the next sender.
		*/
	       if (sendp == NULL) {
		    sessp = sessp->sgi_next;
		    while (sessp) {
			 if (sessp->sgi_senderp) {
			      sendp = sessp->sgi_senderp;
			      break;
			 }
			 sessp = sessp->sgi_next;
		    }
	       }

	       /* At this point, sendp is pointing to the next sender entry. */
	  }
     }

     if (sendp) {
	  indices[0] = (void *) &(sessp->rsvpSessionNumber);
	  indices[1] = (void *) &(sendp->rsvpSenderNumber);
	  return 0;
     }

     return (SNMP_ERR_NO_SUCH_NAME);
}


void *
peer_locate_rsvpSenderEntry(void *ctxt, void **indices, int op)
{
     SessionNumber sessionnum, sendernum;
     rsvpSessionEntry_t *sessp;
     rsvpSenderEntry_t *sendp;

     sessionnum = *( (SessionNumber *) indices[0]);
     sendernum = *( (SessionNumber *) indices[1]);
     sessp = find_rsvpSessionEntry(sessionnum);
     if (sessp == NULL)
	  return NULL;
     sendp = find_rsvpSenderEntry(sessp, sendernum);

     /*
      * The values in this entry are always up to date, so just return it.
      */
     return ((void *) sendp);
}

#endif /* _PEER */


/*
 * rsvpd has malloc'd a sender entry and filled it out.  rsvpd now gives it
 * to this function.  This function is responsible for giving the entry
 * a new sender number, and inserting this entry into the sender list
 * of the correct session entry.
 */
void
rsvpd_insert_rsvpSenderEntry(void *sessionhandle, rsvpSenderEntry_t *entryp)
{
     rsvpSessionEntry_t *sessp;
     rsvpSenderEntry_t *p;
     int unique=0;

     sessp = (rsvpSessionEntry_t *) sessionhandle;

     /*
      * Allocate a new sender number.  It only has to be unique within the
      * list of the session.
      */
     while (unique == 0) {
	  if (rsvpSenderNumber == 0x7fffffff)
	       rsvpSenderNumber = 0;
	  else
	       rsvpSenderNumber++;

	  unique = 1;
	  p = sessp->sgi_senderp;
	  while (p) {
	       if (rsvpSenderNumber < p->rsvpSenderNumber)
		    break;
	       if (rsvpSenderNumber == p->rsvpSenderNumber) {
		    unique = 0;
		    break;
	       }
	       p = p->sgi_next;
	  }
     }

     entryp->sgi_sessp = sessp;
     entryp->rsvpSenderNumber = rsvpSenderNumber;
     entryp->rsvpSenderStatus = ROWSTATUS_ACTIVE;

     /*
      * Insert the sender entry into the session's sender list ordered by
      * sender number.
      * The easy case is if the list is empty.
      */
     if (sessp->sgi_senderp == NULL) {
	  sessp->sgi_senderp = entryp;
	  entryp->sgi_next = NULL;
	  entryp->sgi_prev = NULL;

     } else if (entryp->rsvpSenderNumber < sessp->sgi_senderp->rsvpSenderNumber) {
	  /* session entry goes to the head of the list */
	  entryp->sgi_prev = NULL;
	  entryp->sgi_next = sessp->sgi_senderp;
	  sessp->sgi_senderp->sgi_prev = entryp;
	  sessp->sgi_senderp = entryp;

     } else {
	  p = sessp->sgi_senderp;
	  while ((p->sgi_next != NULL) &&
		 (entryp->rsvpSenderNumber > p->rsvpSenderNumber)) {
	       p = p->sgi_next;
	  }
	  if (entryp->rsvpSenderNumber > p->rsvpSenderNumber) {
	       /* end of the list */
	       entryp->sgi_next = NULL;
	       entryp->sgi_prev = p;
	       p->sgi_next = entryp;
	  } else {
	       /* insert in the middle of the list */
	       entryp->sgi_next = p;
	       entryp->sgi_prev = p->sgi_prev;
	       p->sgi_prev->sgi_next = entryp;
	       p->sgi_prev = entryp;
	  }
     }

#ifdef _MIB_VERBOSE
     printf("INSERTED ");
     dump_rsvpSenderEntry(entryp);
#endif
}


/*
 * Deactivate the Sender entry by setting the rowstatus to NOTINSERVICE.
 * Keep the entry around for mib_entry_expire because we probably sent
 * a lost flow notification and the system administrator may want to look at it.
 * Eventually, we need to free the entry to keep our memory consumption down.
 * Even though the session entry frees all the sender, resv, and resvfwd
 * entries when the session is freed, the sender, resv, and resvfwd code
 * still need to do their own expired clean up in case there is a long lived
 * session, but senders, resv's, come in and out of existence.
 */
void
rsvpd_deactivate_rsvpSenderEntry(void *senderhandle)
{
     rsvpSessionEntry_t *sessp;
     rsvpSenderEntry_t *p, *entryp;
     TimeStamp tn, expired;

     p = (rsvpSenderEntry_t *) senderhandle;
     p->rsvpSenderStatus = ROWSTATUS_NOTINSERVICE;
     p->rsvpSenderLastChange = get_mib_timestamp();

     /*
      * Since the sender entry is no longer active, it is not being forwarded
      * out of any interfaces anymore.
      */
     rsvpd_update_rsvpSenderOutInterfaceEntry(p, 0);

#ifdef _MIB_VERBOSE
     printf("DEACTIVATED ");
     dump_rsvpSenderEntry(p);
#endif

     /*
      * See if we need to clean up the expired entries.
      * If so, update the time that we need to recheck, and then go 
      * through the table free'ing entries which are in the
      * NOTINSERVICE state and expired.
      */
     tn = get_mib_timestamp();
     if (tn < rsvpSenderCheckTime)
	  return;

     rsvpSenderCheckTime = tn + (mib_entry_expire * 100);
     expired = tn - (mib_entry_expire * 100);

     sessp = p->sgi_sessp;
     while ((sessp->sgi_senderp->rsvpSenderStatus == ROWSTATUS_NOTINSERVICE) &&
	    (sessp->sgi_senderp->rsvpSenderLastChange < expired)) {
	  p = sessp->sgi_senderp;
	  sessp->sgi_senderp = p->sgi_next;
	  if (sessp->sgi_senderp)
	       sessp->sgi_senderp->sgi_prev = NULL;
	  free_rsvpSenderEntry(p);
     }

     if (sessp->sgi_senderp)
	  p = sessp->sgi_senderp->sgi_next;
     else
	  p = NULL;
     while (p != NULL) {
	  if ((p->rsvpSenderStatus == ROWSTATUS_NOTINSERVICE) &&
	      (p->rsvpSenderLastChange < expired)) {
	       entryp = p;
	       p->sgi_prev->sgi_next = p->sgi_next;
	       if (p->sgi_next)
		    p->sgi_next->sgi_prev = p->sgi_prev;
	       p = p->sgi_next;
	       free_rsvpSenderEntry(entryp);
	  } else {
	       p = p->sgi_next;
	  }
     }
}


void
rsvpd_update_rsvpSenderPhop(void *senderhandle, RSVP_HOP *phop)
{
     rsvpSenderEntry_t *entryp;

     entryp = (rsvpSenderEntry_t *) senderhandle;

     octetstring_fill(&(entryp->rsvpSenderHopAddr),
		      (void *) &(phop->hop4_addr), 4);
     entryp->rsvpSenderHopLih = phop->hop4_LIH;
     entryp->rsvpSenderLastChange = get_mib_timestamp();
#ifdef _MIB_VERBOSE
     printf("UPDATED Phop on ");
     dump_rsvpSenderEntry(entryp);
#endif
}


void
rsvpd_update_rsvpSenderTspec(void *senderhandle, SENDER_TSPEC *Tspecp)
{
     rsvpSenderEntry_t *entryp;
     TB_Tsp_parms_t *parmsp;

     parmsp = &(Tspecp->stspec_body.tspec_u.gen_stspec.gen_Tspec_parms);
     entryp = (rsvpSenderEntry_t *) senderhandle;

     entryp->rsvpSenderTSpecRate = parmsp->TB_Tspec_r * 8;
     entryp->rsvpSenderTSpecPeakRate = parmsp->TB_Tspec_p * 8;
     entryp->rsvpSenderTSpecBurst = parmsp->TB_Tspec_b;
     entryp->rsvpSenderTSpecMinTU = parmsp->TB_Tspec_m;
     entryp->rsvpSenderTSpecMaxTU = parmsp->TB_Tspec_M;
     entryp->rsvpSenderLastChange = get_mib_timestamp();
#ifdef _MIB_VERBOSE
     printf("UPDATED Tspec on ");
     dump_rsvpSenderEntry(entryp);
#endif
}


rsvpSenderEntry_t *
find_rsvpSenderEntry(rsvpSessionEntry_t *sessp, SessionNumber sendernum)
{
     rsvpSenderEntry_t *p;

     p = sessp->sgi_senderp;
     while (p != NULL) {
	  if (p->rsvpSenderNumber == sendernum)
	       return p;
	  p = p->sgi_next;
     }

     return NULL;
}


void
free_rsvpSenderEntry(rsvpSenderEntry_t *p)
{
#ifdef _MIB_VERBOSE
     printf("FREE ");
     dump_rsvpSenderEntry(p);
#endif

     if (p->sgi_outifp)
	  free(p->sgi_outifp);
     if (p->rsvpSenderDestAddr.val)
	  free(p->rsvpSenderDestAddr.val);
     if (p->rsvpSenderAddr.val)
	  free(p->rsvpSenderAddr.val);
     if (p->rsvpSenderDestPort.val)
	  free(p->rsvpSenderDestPort.val);
     if (p->rsvpSenderPort.val)
	  free(p->rsvpSenderPort.val);
     if (p->rsvpSenderHopAddr.val)
	  free(p->rsvpSenderHopAddr.val);
     if (p->rsvpSenderPolicy.val)
	  free(p->rsvpSenderPolicy.val);

     free(p);
}


void
dump_rsvpSenderEntry(rsvpSenderEntry_t *p)
{
     struct in_addr ia;
     short v4_port;
     int v6_port;
     time_t ct;

     printf("rsvpSenderEntry %d (0x%x)\n", p->rsvpSenderNumber,
	    p->rsvpSenderNumber);
     printf("\trsvpSenderType : %d\n", p->rsvpSenderType);
     bcopy((void *) (p->rsvpSenderDestAddr.val), &ia, p->rsvpSenderDestAddr.len);
     printf("\trsvpSenderDestAddr : %s\n", inet_ntoa(ia));
     bcopy((void *) (p->rsvpSenderAddr.val), &ia, p->rsvpSenderAddr.len);
     printf("\trsvpSenderAddr : %s\n", inet_ntoa(ia));
     printf("\trsvpSenderDestAddrLength : %d\n", p->rsvpSenderDestAddrLength);
     printf("\trsvpSenderAddrLength : %d\n", p->rsvpSenderAddrLength);
     printf("\trsvpSenderProtocol : %d\n", p->rsvpSenderProtocol);
     if (p->rsvpSenderDestPort.len == 2) {
	  bcopy((void *) (p->rsvpSenderDestPort.val), &v4_port, 2);
	  printf("\trsvpSenderDestPort : %d\n", v4_port);
     } else {
	  bcopy((void *) (p->rsvpSenderDestPort.val), &v6_port, 4);
	  printf("\trsvpSenderDestPort : %d\n", v6_port);
     }
     if (p->rsvpSenderPort.len == 2) {
	  bcopy((void *) (p->rsvpSenderPort.val), &v4_port, 2);
	  printf("\trsvpSenderPort : %d\n", v4_port);
     } else {
	  bcopy((void *) (p->rsvpSenderPort.val), &v6_port, 4);
	  printf("\trsvpSenderPort : %d\n", v6_port);
     }
     printf("\trsvpSenderFlowId : %d\n", p->rsvpSenderFlowId);
     bcopy((void *) (p->rsvpSenderHopAddr.val), &ia, p->rsvpSenderHopAddr.len);
     printf("\trsvpSenderHopAddr : %s\n", inet_ntoa(ia));
     printf("\trsvpSenderHopLih : %d\n", p->rsvpSenderHopLih);
     printf("\trsvpSenderInterface : %d\n", p->rsvpSenderInterface);
     printf("\trsvpSenderTSpecRate : %d (bits/sec)\n", p->rsvpSenderTSpecRate);
     printf("\trsvpSenderTSpecPeakRate : %d (bits/sec)\n", p->rsvpSenderTSpecPeakRate);
     printf("\trsvpSenderTSpecBurst : %d (bytes)\n", p->rsvpSenderTSpecBurst);
     printf("\trsvpSenderTSpecMinTU : %d\n", p->rsvpSenderTSpecMinTU);
     printf("\trsvpSenderTSpecMaxTU : %d\n", p->rsvpSenderTSpecMaxTU);
     printf("\trsvpSender(refresh)Interval : %d (ms)\n", p->rsvpSenderInterval);
     printf("\trsvpSenderRSVPHop : %s\n", string_TruthValue(p->rsvpSenderRSVPHop));
     ct = (time_t) p->rsvpSenderLastChange/100 + starttv.tv_sec;
     printf("\trsvpSenderLastChange : %d ticks (%s", p->rsvpSenderLastChange,
	    ctime(&ct));
     if (p->rsvpSenderPolicy.val)
	  printf("\trsvpSenderPolicy : %s\n", p->rsvpSenderPolicy.val);
     printf("\trsvpSenderAdspecBreak : %s\n",
	    string_TruthValue(p->rsvpSenderAdspecBreak));
     printf("\trsvpSenderAdspecHopCount : %d\n", p->rsvpSenderAdspecHopCount);
     printf("\trsvpSenderAdspecPathBw : %d\n", p->rsvpSenderAdspecPathBw);
     printf("\trsvpSenderAdspecMinLatency : %d\n", p->rsvpSenderAdspecMinLatency);
     printf("\trsvpSenderAdspecMtu : %d\n", p->rsvpSenderAdspecMtu);
     printf("\trsvpSenderAdspecGuaranteedSvc : %s\n",
	    string_TruthValue(p->rsvpSenderAdspecGuaranteedSvc));
     printf("\trsvpSenderAdspecGuaranteedBreak : %s\n",
	    string_TruthValue(p->rsvpSenderAdspecGuaranteedBreak));
     printf("\trsvpSenderAdspecGuaranteedCtot : %d\n",
	    p->rsvpSenderAdspecGuaranteedCtot);
     printf("\trsvpSenderAdspecGuaranteedDtot : %d\n",
	    p->rsvpSenderAdspecGuaranteedDtot);
     printf("\trsvpSenderAdspecGuaranteedCsum : %d\n",
	    p->rsvpSenderAdspecGuaranteedCsum);
     printf("\trsvpSenderAdspecGuaranteedDsum : %d\n",
	    p->rsvpSenderAdspecGuaranteedDsum);
     printf("\trsvpSenderAdspecGuaranteedHopCount : %d\n",
	    p->rsvpSenderAdspecGuaranteedHopCount);
     printf("\trsvpSenderAdspecGuaranteedPathBw : %d\n",
	    p->rsvpSenderAdspecGuaranteedPathBw);
     printf("\trsvpSenderAdspecGuaranteedMinLatency : %d\n",
	    p->rsvpSenderAdspecGuaranteedMinLatency);
     printf("\trsvpSenderAdspecGuaranteedMtu : %d\n",
	    p->rsvpSenderAdspecGuaranteedMtu);
     printf("\trsvpSenderAdspecCtrlLoadSvc : %s\n",
	    string_TruthValue(p->rsvpSenderAdspecCtrlLoadSvc));
     printf("\trsvpSenderAdspecCtrlLoadBreak : %s\n",
	    string_TruthValue(p->rsvpSenderAdspecCtrlLoadBreak));
     printf("\trsvpSenderAdspecCtrlLoadHopCount : %d\n",
	    p->rsvpSenderAdspecCtrlLoadHopCount);
     printf("\trsvpSenderAdspecCtrlLoadPathBw : %d\n",
	    p->rsvpSenderAdspecCtrlLoadPathBw);
     printf("\trsvpSenderAdspecCtrlLoadMinLatency : %d\n",
	    p->rsvpSenderAdspecCtrlLoadMinLatency);
     printf("\trsvpSenderAdspecCtrlLoadMtu : %d\n",
	    p->rsvpSenderAdspecCtrlLoadMtu);
     printf("\trsvpSenderStatus : %s\n", string_rowstatus(p->rsvpSenderStatus));
     printf("\trsvpSenderTTL : %d\n", p->rsvpSenderTTL);
     printf("\n");
}

 
/*
 ***********************************************************************************
 *
 * Begin rsvpSenderOutInterfaceTable functions. (3)
 *
 * Each rsvpSenderEntry will point to an array of rsvpSenderOutInterfaceEntry_t's
 * which correspond to the interfaces which PATH messages are going out on.
 *
 ***********************************************************************************
 */

static rsvpSenderOutInterfaceEntry_t *rsvpSenderOutInterfaceTable;

void
rsvpSenderOutInterfaceTable_init(void)
{
     rsvpSenderOutInterfaceTable = NULL;

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpSenderOutInterfaceEntry,
		       (void *) (&rsvpSenderOutInterfaceTable))) == NULL )
         log(LOG_ERR, 0,
	     "PEER mgmt_new_instance failed for rsvpSenderOutInterfaceTable\n");
#endif 
}


#ifdef _PEER
int
peer_next_rsvpSenderOutInterfaceEntry(void *ctxt, void **indices)
{
     int if_index, ifvec_index;
     SessionNumber sessionnum, sendernum;
     if_rec *ifvecp=NULL;
     rsvpSessionEntry_t *sessp=NULL;
     rsvpSenderEntry_t *sendp=NULL;

     if (indices[0] == NULL) {
	  /*
	   * return the first SenderOutInterfaceEntry.
	   */
	  sessp = rsvpSessionTable;
	  if (sessp) {
	       sendp = sessp->sgi_senderp;
	       ifvec_index = next_ifindex(-1);
	       if (ifvec_index != -1)
		    ifvecp = &(if_vec[ifvec_index]);
	  }
     } else {
	  sessionnum = *((SessionNumber *) indices[0]);
	  sessp = find_rsvpSessionEntry(sessionnum);
	  if (sessp == NULL)
	       return (SNMP_ERR_NO_SUCH_NAME);

	  if (indices[1] == NULL) {
	       sendp = sessp->sgi_senderp;
	  } else {
	       sendernum = *((SessionNumber *) indices[1]);
	       sendp = find_rsvpSenderEntry(sessp, sendernum);
	       if (sendp == NULL)
		    return (SNMP_ERR_NO_SUCH_NAME);
	  }

	  if (indices[2] == NULL) {
	       ifvec_index = next_ifindex(-1);
	       if (ifvec_index != -1)
		    ifvecp = &(if_vec[ifvec_index]);
	  } else {
	       if_index = *((ifIndex *) indices[2]);
	       ifvec_index = next_ifindex(if_index);
	       if (ifvec_index != -1) {
		    /*
		     * We've found the next entry!  Done.
		     */
		    ifvecp = &(if_vec[ifvec_index]);
	       } else {
		    /*
		     * We've hit the last entry for this sender, so we
		     * have to go to the next sender.  But we might be at the
		     * last sender, so we have to go to the next session.
		     */
		    sendp = sendp->sgi_next;
		    if (sendp == NULL) {
			 sessp = sessp->sgi_next;
			 if (sessp == NULL) 
			      return (SNMP_ERR_NO_SUCH_NAME);
			 sendp = sessp->sgi_senderp;
		    }
		    ifvec_index = next_ifindex(-1);
		    if (ifvec_index != -1)
			 ifvecp = &(if_vec[ifvec_index]);
	       }
	  }
     }

     /*
      * At this point, ifvecp is pointing to the correct next entry.
      * Return its indices.
      */
     if (sessp && sendp && ifvecp) {
	  indices[0] = (void *) &(sessp->rsvpSessionNumber);
	  indices[1] = (void *) &(sendp->rsvpSenderNumber);
	  indices[2] = (void *) &(ifvecp->if_index);
	  return 0;
     }

     return (SNMP_ERR_NO_SUCH_NAME);
}


void *
peer_locate_rsvpSenderOutInterfaceEntry(void *ctxt, void **indices, int op)
{
     SessionNumber sessionnum, sendernum;
     ifIndex ifindex;
     rsvpSessionEntry_t *sessp;
     rsvpSenderEntry_t *sendp;
     rsvpSenderOutInterfaceEntry_t *outifp;

     sessionnum = *( (SessionNumber *) indices[0]);
     sendernum = *( (SessionNumber *) indices[1]);
     ifindex = *( (ifIndex *) indices[2]);

     sessp = find_rsvpSessionEntry(sessionnum);
     if (sessp == NULL)
	  return NULL;
     sendp = find_rsvpSenderEntry(sessp, sendernum);
     if (sendp == NULL)
	  return NULL;
     outifp = find_rsvpSenderOutInterfaceEntry(sendp, ifindex);

     /*
      * The values in this entry are always up to date, so just return it.
      */
     return ((void *) outifp);
}

#endif  /* _PEER */


/*
 * rsvpd noticed that the PATH messages for this session is going out
 * of this interface.  Mark that interface to ACTIVE status.
 *
 * A static table is allocated to hold the interfaces status.  This table is
 * indexed by the if_vec index, just like the if_rec array.  Notice though
 * that we still have to return the entries in order of ifindex.
 */
void
rsvpd_update_rsvpSenderOutInterfaceEntry(void *senderhandle, bitmap bm)
{
     int i;
     rsvpSenderEntry_t *sendp;
     rsvpSenderOutInterfaceEntry_t *ifarray;

     if (senderhandle == NULL)
	  return;
     sendp = (rsvpSenderEntry_t *) senderhandle;

     if (sendp->sgi_outifp == NULL) {
	  sendp->sgi_outifp = (rsvpSenderOutInterfaceEntry_t *) calloc(vif_num,
					sizeof(rsvpSenderOutInterfaceEntry_t));
     }

     ifarray = sendp->sgi_outifp;

     for (i=0; i < vif_num; i++) {
	  if ((BIT_TST(bm, i)) &&
	      (!(if_vec[i].if_flags & IF_FLAG_Disable)))
	       ifarray[i].rsvpSenderOutInterfaceStatus = ROWSTATUS_ACTIVE;
          else
	       ifarray[i].rsvpSenderOutInterfaceStatus = ROWSTATUS_NOTINSERVICE;
     }

#ifdef _MIB_VERBOSE
     printf("INSERTED ");
     dump_rsvpSenderOutInterfaceEntry(sendp);
#endif
}


rsvpSenderOutInterfaceEntry_t *
find_rsvpSenderOutInterfaceEntry(rsvpSenderEntry_t *sendp, ifIndex ifindex)
{
     int i;

     for (i=0; i < vif_num; i++)
	  if (if_vec[i].if_index == ifindex)
	       return (&(sendp->sgi_outifp[i]));

     return NULL;
}


/*
 * Return the if_vec index (in the if_vec array) which has the next highest ifindex.
 * Return -1 if there is no higher ifindex.
 */
int
next_ifindex(ifIndex ifindex)
{
     int i;

     while (ifindex < max_ifIndex) {
	  ifindex++;
	  for (i=0; i < vif_num; i++) {
	       if ((if_vec[i].if_index == ifindex) &&
		   !(if_vec[i].if_flags & IF_FLAG_Disable))
		    return i;
	  }
     }
     return -1;
}


void
dump_rsvpSenderOutInterfaceEntry(rsvpSenderEntry_t *sendp)
{
     int i;
     rsvpSenderOutInterfaceEntry_t *ifarray;

     printf("rsvpSenderOutInterfaceEntry for sender %d\n",
	    sendp->rsvpSenderNumber);

     ifarray = sendp->sgi_outifp;

     for (i=0; i < vif_num; i++) {
	  if (ifarray[i].rsvpSenderOutInterfaceStatus == ROWSTATUS_ACTIVE)
	       printf("\tif_vec %d if_index %d (%s) is active\n", i, 
		      if_vec[i].if_index, if_vec[i].if_name);
     }
     printf("\n");
}


/*
 ***********************************************************************************
 *
 * Begin rsvpResvTable functions. (4)
 *
 *
 ***********************************************************************************
 */

static rsvpResvEntry_t *rsvpResvTable;
static int rsvpResvNumber;
static TimeStamp rsvpResvCheckTime;

void
rsvpResvTable_init(void)
{
     TimeStamp tn;
     struct timeval tv;

     gettimeofday(&tv);
     srand(tv.tv_usec);
     rsvpResvNumber = rand();

     tn = get_mib_timestamp();
     rsvpResvCheckTime = tn + 2 * mib_entry_expire * 100;

     rsvpResvTable = NULL;

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpResvEntry,
		       (void *) (&rsvpResvTable))) == NULL )
         log(LOG_ERR, 0, "PEER mgmt_new_instance failed for rsvpResvTable\n");
#endif 
}


#ifdef _PEER
int
peer_next_rsvpResvEntry(void *ctxt, void **indices)
{
     rsvpSessionEntry_t *sessp;
     rsvpResvEntry_t *resvp=NULL;
     SessionNumber sessionnum, resvnum;

     if (indices[0] == NULL) {
	  /*
	   * Return the first resv, which really means return the first
	   * resv of the first session.  There should be at least one
	   * resv for every session, but just in case.... I'll will not
	   * assume that.
	   */
	  sessp = rsvpSessionTable;
	  while (sessp) {
	       if (sessp->sgi_resvp) {
		    resvp = sessp->sgi_resvp;
		    break;
	       }
	       sessp = sessp->sgi_next;
	  }
     } else {
	  /*
	   * The master agent has given me a {SessionNumber, ResvNumber} index.
	   * Find that resv entry, and return the next one (which could
	   * be the first resv on the next session).
	   */
	  sessionnum = *((SessionNumber *) indices[0]);
	  
	  sessp = find_rsvpSessionEntry(sessionnum);
	  if (sessp) {
	       if (indices[1] == NULL) {
		    resvp = sessp->sgi_resvp;
	       } else {
		    resvnum = *((SessionNumber *) indices[1]);
		    resvp = find_rsvpResvEntry(sessp, resvnum);
		    if (resvp)
			 resvp = resvp->sgi_next;
	       }

	       /*
		* If resvp is not NULL, then we are done.
		* If it is NULL, we have to search through all the
		* sessions for the first resv.
		*/
	       if (resvp == NULL) {
		    sessp = sessp->sgi_next;
		    while (sessp) {
			 if (sessp->sgi_resvp) {
			      resvp = sessp->sgi_resvp;
			      break;
			 }
			 sessp = sessp->sgi_next;
		    }
	       }

	       /* At this point, resvp is pointing to the next resv entry. */
	  }
     }

     if (resvp) {
	  indices[0] = (void *) &(sessp->rsvpSessionNumber);
	  indices[1] = (void *) &(resvp->rsvpResvNumber);
	  return 0;
     }

     return (SNMP_ERR_NO_SUCH_NAME);
}


void *
peer_locate_rsvpResvEntry(void *ctxt, void **indices, int op)
{
     SessionNumber sessionnum, resvnum;
     rsvpSessionEntry_t *sessp;
     rsvpResvEntry_t *resvp;

     sessionnum = *( (SessionNumber *) indices[0]);
     resvnum = *( (SessionNumber *) indices[1]);
     sessp = find_rsvpSessionEntry(sessionnum);
     if (sessp == NULL)
	  return NULL;
     resvp = find_rsvpResvEntry(sessp, resvnum);

     /*
      * The values in this entry are always up to date, so just return it.
      */
     return ((void *) resvp);
}

#endif /* _PEER */


/*
 * rsvpd has malloc'd a resv entry and filled it out.  rsvpd now gives it
 * to this function.  This function is responsible for giving the entry
 * a new resv number, and inserting this entry into the resv list
 * of the correct session entry.
 */
void
rsvpd_insert_rsvpResvEntry(void *sessionhandle, rsvpResvEntry_t *entryp)
{
     rsvpSessionEntry_t *sessp;
     rsvpResvEntry_t *p;
     int unique=0;

     sessp = (rsvpSessionEntry_t *) sessionhandle;

     /*
      * Allocate a new resv number.  It only has to be unique within the
      * list of the session.
      */
     while (unique == 0) {
	  if (rsvpResvNumber == 0x7fffffff)
	       rsvpResvNumber = 0;
	  else
	       rsvpResvNumber++;

	  unique = 1;
	  p = sessp->sgi_resvp;
	  while (p) {
	       if (rsvpResvNumber < p->rsvpResvNumber)
		    break;
	       if (rsvpResvNumber == p->rsvpResvNumber) {
		    unique = 0;
		    break;
	       }
	       p = p->sgi_next;
	  }
     }

     entryp->sgi_sessp = sessp;
     entryp->rsvpResvNumber = rsvpResvNumber;
     entryp->rsvpResvStatus = ROWSTATUS_ACTIVE;

     /*
      * Insert the resv entry into the session's resv list ordered by
      * resv number.
      * The easy case is if the list is empty.
      */
     if (sessp->sgi_resvp == NULL) {
	  sessp->sgi_resvp = entryp;
	  entryp->sgi_next = NULL;
	  entryp->sgi_prev = NULL;

     } else if (entryp->rsvpResvNumber < sessp->sgi_resvp->rsvpResvNumber) {
	  /* session entry goes to the head of the list */
	  entryp->sgi_prev = NULL;
	  entryp->sgi_next = sessp->sgi_resvp;
	  sessp->sgi_resvp->sgi_prev = entryp;
	  sessp->sgi_resvp = entryp;

     } else {
	  p = sessp->sgi_resvp;
	  while ((p->sgi_next != NULL) &&
		 (entryp->rsvpResvNumber > p->rsvpResvNumber)) {
	       p = p->sgi_next;
	  }
	  if (entryp->rsvpResvNumber > p->rsvpResvNumber) {
	       /* end of the list */
	       entryp->sgi_next = NULL;
	       entryp->sgi_prev = p;
	       p->sgi_next = entryp;
	  } else {
	       /* insert in the middle of the list */
	       entryp->sgi_next = p;
	       entryp->sgi_prev = p->sgi_prev;
	       p->sgi_prev->sgi_next = entryp;
	       p->sgi_prev = entryp;
	  }
     }

#ifdef _MIB_VERBOSE
     printf("INSERTED ");
     dump_rsvpResvEntry(entryp);
#endif
}


/*
 * Deactivate the Resv entry by setting the rowstatus to NOTINSERVICE.
 * Keep the entry around for mib_entry_expire because we probably sent
 * a lost flow notification and the system administrator may want to look at it.
 * Eventually, we need to free the entry to keep our memory consumption down.
 */
void
rsvpd_deactivate_rsvpResvEntry(void *resvhandle)
{
     rsvpSessionEntry_t *sessp;
     rsvpResvEntry_t *p, *entryp;
     TimeStamp tn, expired;

     p = (rsvpResvEntry_t *) resvhandle;
     p->rsvpResvStatus = ROWSTATUS_NOTINSERVICE;
     p->rsvpResvLastChange = get_mib_timestamp();

#ifdef _MIB_VERBOSE
     printf("DEACTIVATED ");
     dump_rsvpResvEntry(p);
#endif

     /*
      * See if we need to clean up the expired entries.
      * If so, update the time that we need to recheck, and then go 
      * through the table free'ing entries which are in the
      * NOTINSERVICE state and expired.
      */
     tn = get_mib_timestamp();
     if (tn < rsvpResvCheckTime)
	  return;

     rsvpResvCheckTime = tn + (mib_entry_expire * 100);
     expired = tn - (mib_entry_expire * 100);

     sessp = p->sgi_sessp;
     while ((sessp->sgi_resvp->rsvpResvStatus == ROWSTATUS_NOTINSERVICE) &&
	    (sessp->sgi_resvp->rsvpResvLastChange < expired)) {
	  p = sessp->sgi_resvp;
	  sessp->sgi_resvp = p->sgi_next;
	  if (sessp->sgi_resvp)
	       sessp->sgi_resvp->sgi_prev = NULL;
	  free_rsvpResvEntry(p);
     }

     if (sessp->sgi_resvp)
	  p = sessp->sgi_resvp->sgi_next;
     else
	  p = NULL;
     while (p != NULL) {
	  if ((p->rsvpResvStatus == ROWSTATUS_NOTINSERVICE) &&
	      (p->rsvpResvLastChange < expired)) {
	       entryp = p;
	       p->sgi_prev->sgi_next = p->sgi_next;
	       if (p->sgi_next)
		    p->sgi_next->sgi_prev = p->sgi_prev;
	       p = p->sgi_next;
	       free_rsvpResvEntry(entryp);
	  } else {
	       p = p->sgi_next;
	  }
     }
}


void
rsvpd_update_rsvpResvTspec(void *resvhandle, FLOWSPEC *flowspecp)
{
     rsvpResvEntry_t *entryp;
     TB_Tsp_parms_t *parmsp;

     parmsp = &(flowspecp->flow_body.spec_u.CL_spec.CL_spec_parms);
     entryp = (rsvpResvEntry_t *) resvhandle;

     entryp->rsvpResvTSpecRate = parmsp->TB_Tspec_r * 8;
     entryp->rsvpResvTSpecPeakRate = parmsp->TB_Tspec_p * 8;
     entryp->rsvpResvTSpecBurst = parmsp->TB_Tspec_b;
     entryp->rsvpResvTSpecMinTU = parmsp->TB_Tspec_m;
     entryp->rsvpResvTSpecMaxTU = parmsp->TB_Tspec_M;
     entryp->rsvpResvLastChange = get_mib_timestamp();
#ifdef _MIB_VERBOSE
     printf("UPDATED flowspec on ");
     dump_rsvpResvEntry(entryp);
#endif
}


rsvpResvEntry_t *
find_rsvpResvEntry(rsvpSessionEntry_t *sessp, SessionNumber resvnum)
{
     rsvpResvEntry_t *p;

     p = sessp->sgi_resvp;
     while (p != NULL) {
	  if (p->rsvpResvNumber == resvnum)
	       return p;
	  p = p->sgi_next;
     }

     return NULL;
}


void
free_rsvpResvEntry(rsvpResvEntry_t *p)
{
#ifdef _MIB_VERBOSE
     printf("FREE ");
     dump_rsvpResvEntry(p);
#endif

     if (p->rsvpResvDestAddr.val)
	  free(p->rsvpResvDestAddr.val);
     if (p->rsvpResvSenderAddr.val)
	  free(p->rsvpResvSenderAddr.val);
     if (p->rsvpResvDestPort.val)
	  free(p->rsvpResvDestPort.val);
     if (p->rsvpResvPort.val)
	  free(p->rsvpResvPort.val);
     if (p->rsvpResvHopAddr.val)
	  free(p->rsvpResvHopAddr.val);
     if (p->rsvpResvScope.val)
	  free(p->rsvpResvScope.val);
     if (p->rsvpResvPolicy.val)
	  free(p->rsvpResvPolicy.val);
     free(p);
}


void
dump_rsvpResvEntry(rsvpResvEntry_t *p)
{
     struct in_addr ia;
     short v4_port;
     int v6_port;
     time_t ct;

     printf("rsvpResvEntry %d (0x%x)\n", p->rsvpResvNumber,
	    p->rsvpResvNumber);
     printf("\trsvpResvType : %d\n", p->rsvpResvType);
     bcopy((void *) (p->rsvpResvDestAddr.val), &ia, p->rsvpResvDestAddr.len);
     printf("\trsvpResvDestAddr : %s\n", inet_ntoa(ia));
     bcopy((void *) (p->rsvpResvSenderAddr.val), &ia, p->rsvpResvSenderAddr.len);
     printf("\trsvpResvSenderAddr : %s\n", inet_ntoa(ia));
     printf("\trsvpResvDestAddrLength : %d\n", p->rsvpResvDestAddrLength);
     printf("\trsvpResvSenderAddrLength : %d\n", p->rsvpResvSenderAddrLength);
     printf("\trsvpResvProtocol : %d\n", p->rsvpResvProtocol);
     if (p->rsvpResvDestPort.len == 2) {
	  bcopy((void *) (p->rsvpResvDestPort.val), &v4_port, 2);
	  printf("\trsvpResvDestPort : %d\n", v4_port);
     } else {
	  bcopy((void *) (p->rsvpResvDestPort.val), &v6_port, 4);
	  printf("\trsvpResvDestPort : %d\n", v6_port);
     }
     if (p->rsvpResvPort.len == 2) {
	  bcopy((void *) (p->rsvpResvPort.val), &v4_port, 2);
	  printf("\trsvpResvPort : %d\n", v4_port);
     } else {
	  bcopy((void *) (p->rsvpResvPort.val), &v6_port, 4);
	  printf("\trsvpResvPort : %d\n", v6_port);
     }
     bcopy((void *) (p->rsvpResvHopAddr.val), &ia, p->rsvpResvHopAddr.len);
     printf("\trsvpResvHopAddr : %s\n", inet_ntoa(ia));
     printf("\trsvpResvHopLih : %d\n", p->rsvpResvHopLih);
     printf("\trsvpResvInterface : %d\n", p->rsvpResvInterface);
     printf("\trsvpResvService : %s\n", string_QosService(p->rsvpResvService));
     printf("\trsvpResvTSpecRate : %d (bits/sec)\n", p->rsvpResvTSpecRate);
     printf("\trsvpResvTSpecPeakRate : %d (bits/sec)\n", p->rsvpResvTSpecPeakRate);
     printf("\trsvpResvTSpecBurst : %d (bytes)\n", p->rsvpResvTSpecBurst);
     printf("\trsvpResvTSpecMinTU : %d\n", p->rsvpResvTSpecMinTU);
     printf("\trsvpResvTSpecMaxTU : %d\n", p->rsvpResvTSpecMaxTU);
     printf("\trsvpResvRSpecRate : %d\n", p->rsvpResvRSpecRate);
     printf("\trsvpResvRSpecSlack : %d\n", p->rsvpResvRSpecSlack);
     printf("\trsvpResv(refresh)Interval : %d (ms)\n", p->rsvpResvInterval);
     if (p->rsvpResvScope.val)
	  printf("\trsvpResvScope : %s\n", p->rsvpResvScope.val);
     printf("\trsvpResvShared : %s\n", string_TruthValue(p->rsvpResvShared));
     printf("\trsvpResvExplicit : %s\n", string_TruthValue(p->rsvpResvExplicit));
     printf("\trsvpResvRSVPHop : %s\n", string_TruthValue(p->rsvpResvRSVPHop));
     ct = (time_t) p->rsvpResvLastChange/100 + starttv.tv_sec;
     printf("\trsvpResvLastChange : %d ticks (%s",
	    p->rsvpResvLastChange, ctime(&ct));
     if (p->rsvpResvPolicy.val)
	  printf("\trsvpResvPolicy : %s\n", p->rsvpResvPolicy.val);
     printf("\trsvpResvStatus : %s\n", string_rowstatus(p->rsvpResvStatus));
     printf("\trsvpResvTTL : %d\n", p->rsvpResvTTL);
     printf("\trsvpResvFlowId : %d\n", p->rsvpResvFlowId);
     printf("\n");
}


/*
 ***********************************************************************************
 *
 * Begin rsvpResvFwdTable functions. (5)
 *
 *
 ***********************************************************************************
 */

static rsvpResvFwdEntry_t *rsvpResvFwdTable;
static int rsvpResvFwdNumber;
static TimeStamp rsvpResvFwdCheckTime;


void
rsvpResvFwdTable_init(void)
{
     TimeStamp tn;
     struct timeval tv;

     gettimeofday(&tv);
     srand(tv.tv_usec);
     rsvpResvFwdNumber = rand();

     tn = get_mib_timestamp();
     rsvpResvFwdCheckTime = tn + 2 * mib_entry_expire * 100;

     rsvpResvFwdTable = NULL;

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpResvFwdEntry,
		       (void *) (&rsvpResvFwdTable))) == NULL )
         log(LOG_ERR, 0, "PEER mgmt_new_instance failed for rsvpResvFwdTable\n");
#endif 
}

#ifdef _PEER
peer_next_rsvpResvFwdEntry(void *ctxt, void **indices)
{
     rsvpSessionEntry_t *sessp;
     rsvpResvFwdEntry_t *fwdp=NULL;
     SessionNumber sessionnum, fwdnum;

     if (indices[0] == NULL) {
	  /*
	   * Return the first resvfwd, which really means return the first
	   * resvfwd of the first session.
	   */
	  sessp = rsvpSessionTable;
	  while (sessp) {
	       if (sessp->sgi_resvfwdp) {
		    fwdp = sessp->sgi_resvfwdp;
		    break;
	       }
	       sessp = sessp->sgi_next;
	  }
     } else {
	  /*
	   * The master agent has given me a {SessionNumber, ResvFwdNumber} index.
	   * Find that resvfwd entry, and return the next one (which could
	   * be the first resvfwd on the next session).
	   */
	  sessionnum = *((SessionNumber *) indices[0]);
	  
	  sessp = find_rsvpSessionEntry(sessionnum);
	  if (sessp) {
	       if (indices[1] == NULL) {
		    fwdp = sessp->sgi_resvfwdp;
	       } else {
		    fwdnum = *((SessionNumber *) indices[1]);
		    fwdp = find_rsvpResvFwdEntry(sessp, fwdnum);
		    if (fwdp)
			 fwdp = fwdp->sgi_next;
	       }

	       /*
		* If fwdp is not NULL, then we are done.
		* If it is NULL, we have to search through all the
		* sessions for the first resvfwd.
		*/
	       if (fwdp == NULL) {
		    sessp = sessp->sgi_next;
		    while (sessp) {
			 if (sessp->sgi_resvfwdp) {
			      fwdp = sessp->sgi_resvfwdp;
			      break;
			 }
			 sessp = sessp->sgi_next;
		    }
	       }

	       /* At this point, fwdp is pointing to the next resvfwd entry. */
	  }
     }

     if (fwdp) {
	  indices[0] = (void *) &(sessp->rsvpSessionNumber);
	  indices[1] = (void *) &(fwdp->rsvpResvFwdNumber);
	  return 0;
     }

     return (SNMP_ERR_NO_SUCH_NAME);
}


void *
peer_locate_rsvpResvFwdEntry(void *ctxt, void **indices, int op)
{
     SessionNumber sessionnum, fwdnum;
     rsvpSessionEntry_t *sessp;
     rsvpResvFwdEntry_t *fwdp;

     sessionnum = *( (SessionNumber *) indices[0]);
     fwdnum = *( (SessionNumber *) indices[1]);
     sessp = find_rsvpSessionEntry(sessionnum);
     if (sessp == NULL)
	  return NULL;
     fwdp = find_rsvpResvFwdEntry(sessp, fwdnum);

     /*
      * The values in this entry are always up to date, so just return it.
      */
     return ((void *) fwdp);
}

#endif /* _PEER */


/*
 * rsvpd has malloc'd a resvfwd entry and filled it out.  rsvpd now gives it
 * to this function.  This function is responsible for giving the entry
 * a new resvfwd number, and inserting this entry into the resvfwd list
 * of the correct session entry.
 */
void
rsvpd_insert_rsvpResvFwdEntry(void *sessionhandle, rsvpResvFwdEntry_t *entryp)
{
     rsvpSessionEntry_t *sessp;
     rsvpResvFwdEntry_t *p;
     int unique=0;

     sessp = (rsvpSessionEntry_t *) sessionhandle;

     /*
      * Allocate a new resvfwd number.  It only has to be unique within the
      * list of the session.
      */
     while (unique == 0) {
	  if (rsvpResvFwdNumber == 0x7fffffff)
	       rsvpResvFwdNumber = 0;
	  else
	       rsvpResvFwdNumber++;

	  unique = 1;
	  p = sessp->sgi_resvfwdp;
	  while (p) {
	       if (rsvpResvFwdNumber < p->rsvpResvFwdNumber)
		    break;
	       if (rsvpResvFwdNumber == p->rsvpResvFwdNumber) {
		    unique = 0;
		    break;
	       }
	       p = p->sgi_next;
	  }
     }

     entryp->sgi_sessp = sessp;
     entryp->rsvpResvFwdNumber = rsvpResvFwdNumber;
     entryp->rsvpResvFwdStatus = ROWSTATUS_ACTIVE;

     /*
      * Insert the resvfwd entry into the session's resvfwd list ordered by
      * resvfwd number.
      * The easy case is if the list is empty.
      */
     if (sessp->sgi_resvfwdp == NULL) {
	  sessp->sgi_resvfwdp = entryp;
	  entryp->sgi_next = NULL;
	  entryp->sgi_prev = NULL;

     } else if (entryp->rsvpResvFwdNumber < sessp->sgi_resvfwdp->rsvpResvFwdNumber) {
	  /* session entry goes to the head of the list */
	  entryp->sgi_prev = NULL;
	  entryp->sgi_next = sessp->sgi_resvfwdp;
	  sessp->sgi_resvfwdp->sgi_prev = entryp;
	  sessp->sgi_resvfwdp = entryp;

     } else {
	  p = sessp->sgi_resvfwdp;
	  while ((p->sgi_next != NULL) &&
		 (entryp->rsvpResvFwdNumber > p->rsvpResvFwdNumber)) {
	       p = p->sgi_next;
	  }
	  if (entryp->rsvpResvFwdNumber > p->rsvpResvFwdNumber) {
	       /* end of the list */
	       entryp->sgi_next = NULL;
	       entryp->sgi_prev = p;
	       p->sgi_next = entryp;
	  } else {
	       /* insert in the middle of the list */
	       entryp->sgi_next = p;
	       entryp->sgi_prev = p->sgi_prev;
	       p->sgi_prev->sgi_next = entryp;
	       p->sgi_prev = entryp;
	  }
     }

#ifdef _MIB_VERBOSE
     printf("INSERTED ");
     dump_rsvpResvFwdEntry(entryp);
#endif
}


/*
 * Deactivate the ResvFwd entry by setting the rowstatus to NOTINSERVICE.
 * Keep the entry around for mib_entry_expire because we probably sent
 * a lost flow notification and the system administrator may want to look at it.
 * Eventually, we need to free the entry to keep our memory consumption down.
 */
void
rsvpd_deactivate_rsvpResvFwdEntry(void *resvfwdhandle)
{
     rsvpSessionEntry_t *sessp;
     rsvpResvFwdEntry_t *p, *entryp;
     TimeStamp tn, expired;

     /* The resv is not forwarded when the resv if for the local host */
     if (resvfwdhandle == NULL)
	  return;

     p = (rsvpResvFwdEntry_t *) resvfwdhandle;
     p->rsvpResvFwdStatus = ROWSTATUS_NOTINSERVICE;
     p->rsvpResvFwdLastChange = get_mib_timestamp();

#ifdef _MIB_VERBOSE
     printf("DEACTIVATED ");
     dump_rsvpResvFwdEntry(p);
#endif

     /*
      * See if we need to clean up the expired entries.
      * If so, update the time that we need to recheck, and then go 
      * through the table free'ing entries which are in the
      * NOTINSERVICE state and expired.
      */
     tn = get_mib_timestamp();
     if (tn < rsvpResvFwdCheckTime)
	  return;

     rsvpResvFwdCheckTime = tn + (mib_entry_expire * 100);
     expired = tn - (mib_entry_expire * 100);

     sessp = p->sgi_sessp;
     while ((sessp->sgi_resvfwdp->rsvpResvFwdStatus == ROWSTATUS_NOTINSERVICE) &&
	    (sessp->sgi_resvfwdp->rsvpResvFwdLastChange < expired)) {
	  p = sessp->sgi_resvfwdp;
	  sessp->sgi_resvfwdp = p->sgi_next;
	  if (sessp->sgi_resvfwdp)
	       sessp->sgi_resvfwdp->sgi_prev = NULL;
	  free_rsvpResvFwdEntry(p);
     }

     if (sessp->sgi_resvfwdp)
	  p = sessp->sgi_resvfwdp->sgi_next;
     else
	  p = NULL;
     while (p != NULL) {
	  if ((p->rsvpResvFwdStatus == ROWSTATUS_NOTINSERVICE) &&
	      (p->rsvpResvFwdLastChange < expired)) {
	       entryp = p;
	       p->sgi_prev->sgi_next = p->sgi_next;
	       if (p->sgi_next)
		    p->sgi_next->sgi_prev = p->sgi_prev;
	       p = p->sgi_next;
	       free_rsvpResvFwdEntry(entryp);
	  } else {
	       p = p->sgi_next;
	  }
     }
}


void
rsvpd_update_rsvpResvFwdTspec(void *resvfwdhandle, FLOWSPEC *flowspecp)
{
     rsvpResvFwdEntry_t *entryp;
     TB_Tsp_parms_t *parmsp;

     parmsp = &(flowspecp->flow_body.spec_u.CL_spec.CL_spec_parms);
     entryp = (rsvpResvFwdEntry_t *) resvfwdhandle;

     entryp->rsvpResvFwdTSpecRate = parmsp->TB_Tspec_r * 8;
     entryp->rsvpResvFwdTSpecPeakRate = parmsp->TB_Tspec_p * 8;
     entryp->rsvpResvFwdTSpecBurst = parmsp->TB_Tspec_b;
     entryp->rsvpResvFwdTSpecMinTU = parmsp->TB_Tspec_m;
     entryp->rsvpResvFwdTSpecMaxTU = parmsp->TB_Tspec_M;
     entryp->rsvpResvFwdLastChange = get_mib_timestamp();
#ifdef _MIB_VERBOSE
     printf("UPDATED flowspec on ");
     dump_rsvpResvFwdEntry(entryp);
#endif
}


rsvpResvFwdEntry_t *
find_rsvpResvFwdEntry(rsvpSessionEntry_t *sessp, SessionNumber fwdnum)
{
     rsvpResvFwdEntry_t *p;

     p = sessp->sgi_resvfwdp;
     while (p != NULL) {
	  if (p->rsvpResvFwdNumber == fwdnum)
	       return p;
	  p = p->sgi_next;
     }

     return NULL;
}


void free_rsvpResvFwdEntry(rsvpResvFwdEntry_t *p)
{
#ifdef _MIB_VERBOSE
     printf("FREE ");
     dump_rsvpResvFwdEntry(p);
#endif

     if (p->rsvpResvFwdDestAddr.val)
	  free(p->rsvpResvFwdDestAddr.val);
     if (p->rsvpResvFwdSenderAddr.val)
	  free(p->rsvpResvFwdSenderAddr.val);
     if (p->rsvpResvFwdDestPort.val)
	  free(p->rsvpResvFwdDestPort.val);
     if (p->rsvpResvFwdPort.val)
	  free(p->rsvpResvFwdPort.val);
     if (p->rsvpResvFwdHopAddr.val)
	  free(p->rsvpResvFwdHopAddr.val);
     if (p->rsvpResvFwdScope.val)
	  free(p->rsvpResvFwdScope.val);
     if (p->rsvpResvFwdPolicy.val)
	  free(p->rsvpResvFwdPolicy.val);
     free(p);
}


void
dump_rsvpResvFwdEntry(rsvpResvFwdEntry_t *p)
{
     struct in_addr ia;
     short v4_port;
     int v6_port;

     printf("rsvpResvFwdEntry %d (0x%x)\n", p->rsvpResvFwdNumber,
	    p->rsvpResvFwdNumber);

     printf("\trsvpResvFwdType : %d\n", p->rsvpResvFwdType);
     bcopy((void *) (p->rsvpResvFwdDestAddr.val), &ia, p->rsvpResvFwdDestAddr.len);
     printf("\trsvpResvFwdDestAddr : %s\n", inet_ntoa(ia));
     bcopy((void *) (p->rsvpResvFwdSenderAddr.val), &ia, p->rsvpResvFwdSenderAddr.len);
     printf("\trsvpResvFwdSenderAddr : %s\n", inet_ntoa(ia));
     printf("\trsvpResvFwdDestAddrLength : %d\n", p->rsvpResvFwdDestAddrLength);
     printf("\trsvpResvFwdSenderAddrLength : %d\n", p->rsvpResvFwdSenderAddrLength);
     printf("\trsvpResvFwdProtocol : %d\n", p->rsvpResvFwdProtocol);
     if (p->rsvpResvFwdDestPort.len == 2) {
	  bcopy((void *) (p->rsvpResvFwdDestPort.val), &v4_port, 2);
	  printf("\trsvpResvFwdDestPort : %d\n", v4_port);
     } else {
	  bcopy((void *) (p->rsvpResvFwdDestPort.val), &v6_port, 4);
	  printf("\trsvpResvFwdDestPort : %d\n", v6_port);
     }
     if (p->rsvpResvFwdPort.len == 2) {
	  bcopy((void *) (p->rsvpResvFwdPort.val), &v4_port, 2);
	  printf("\trsvpResvFwdPort : %d\n", v4_port);
     } else {
	  bcopy((void *) (p->rsvpResvFwdPort.val), &v6_port, 4);
	  printf("\trsvpResvFwdPort : %d\n", v6_port);
     }
     bcopy((void *) (p->rsvpResvFwdHopAddr.val), &ia, p->rsvpResvFwdHopAddr.len);
     printf("\trsvpResvFwdHopAddr : %s\n", inet_ntoa(ia));
     printf("\trsvpResvFwdHopLih : %d\n", p->rsvpResvFwdHopLih);
     printf("\trsvpResvFwdInterface : %d\n", p->rsvpResvFwdInterface);
     printf("\trsvpResvFwdService : %s\n", string_QosService(p->rsvpResvFwdService));
     printf("\trsvpResvFwdTSpecRate : %d (bits/sec)\n", p->rsvpResvFwdTSpecRate);
     printf("\trsvpResvFwdTSpecPeakRate : %d (bits/sec)\n", p->rsvpResvFwdTSpecPeakRate);
     printf("\trsvpResvFwdTSpecBurst : %d (bytes)\n", p->rsvpResvFwdTSpecBurst);
     printf("\trsvpResvFwdTSpecMinTU : %d\n", p->rsvpResvFwdTSpecMinTU);
     printf("\trsvpResvFwdTSpecMaxTU : %d\n", p->rsvpResvFwdTSpecMaxTU);
     printf("\trsvpResvFwdRSpecRate : %d\n", p->rsvpResvFwdRSpecRate);
     printf("\trsvpResvFwdRSpecSlack : %d\n", p->rsvpResvFwdRSpecSlack);
     printf("\trsvpResvFwd(refresh)Interval : %d (ms)\n", p->rsvpResvFwdInterval);
     if (p->rsvpResvFwdScope.val)
	  printf("\trsvpResvFwdScope : %s\n", p->rsvpResvFwdScope.val);
     printf("\trsvpResvFwdShared : %s\n", string_TruthValue(p->rsvpResvFwdShared));
     printf("\trsvpResvFwdExplicit : %s\n", string_TruthValue(p->rsvpResvFwdExplicit));
     printf("\trsvpResvFwdRSVPHop : %s\n", string_TruthValue(p->rsvpResvFwdRSVPHop));
     printf("\trsvpResvFwdLastChange : %d (ticks)\n", p->rsvpResvFwdLastChange);
     if (p->rsvpResvFwdPolicy.val)
	  printf("\trsvpResvFwdPolicy : %s\n", p->rsvpResvFwdPolicy.val);
     printf("\trsvpResvFwdStatus : %s\n", string_rowstatus(p->rsvpResvFwdStatus));
     printf("\trsvpResvFwdTTL : %d\n", p->rsvpResvFwdTTL);
     printf("\trsvpResvFwdFlowId : %d\n", p->rsvpResvFwdFlowId);
     printf("\n");
}


/*
 ***********************************************************************************
 *
 * Begin rsvpIfTable functions. (6)
 * rsvpIfTable is indexed by ifIndex.  Not all slots are active, since 
 * not all ifIndices are present on a system.
 *
 ***********************************************************************************
 */

static rsvpIfEntry_t *rsvpIfTable;

void
rsvpIfTable_init(void)
{
     int i, j;

     rsvpIfTable = (rsvpIfEntry_t *) calloc(max_ifIndex+1, sizeof(rsvpIfEntry_t));

     /*
      * Initialize the entries in the rsvpIfTable.  Some of the entries
      * may not be active.
      */
     for (i=0; i < max_ifIndex+1; i++) {
	  for (j=0; j < if_num; j++)
	       if (if_vec[j].if_index == i)
		    break;
	  /*
	   * This ifindex does not exist.  Zero out this entry and set
	   * the status to notinservice.
	   */
	  if (j == if_num) {
	       bzero(&(rsvpIfTable[i]), sizeof(rsvpIfEntry_t));
	       rsvpIfTable[i].rsvpIfStatus = ROWSTATUS_NOTINSERVICE;
	       continue;
	  }

	  /*
	   * Initialize the entry with some values.
	   */
	  rsvpIfTable[i].sgi_ifindex = i;
	  rsvpIfTable[i].rsvpIfUdpRequired = TRUTHVALUE_FALSE;
	  rsvpIfTable[i].rsvpIfRefreshBlockadeMultiple = 4;
	  rsvpIfTable[i].rsvpIfRefreshMultiple = 3;
	  rsvpIfTable[i].rsvpIfTTL = 0;
	  rsvpIfTable[i].rsvpIfRefreshInterval = REFRESH_DEFAULT;
	  rsvpIfTable[i].rsvpIfRouteDelay = 200;
	  rsvpIfTable[i].rsvpIfStatus = ROWSTATUS_ACTIVE;

	  rsvpIfTable[i].rsvpIfEnabled = TRUTHVALUE_FALSE;
	  for (j=0; j < if_num; j++) {
	       if ((if_vec[j].if_index == i) &&
		   (!(if_vec[j].if_flags & IF_FLAG_Disable)))
		    rsvpIfTable[i].rsvpIfEnabled = TRUTHVALUE_TRUE;
	  }
#ifdef _MIB_VERBOSE
	  printf("ADDED ");
	  dump_rsvpIfEntry(&(rsvpIfTable[i]));
#endif
     }

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpIfEntry,
		       (void *) (&rsvpIfTable))) == NULL )
         log(LOG_ERR, 0, "PEER mgmt_new_instance failed for rsvpIfTable\n");
#endif 
}


#ifdef _PEER
int
peer_next_rsvpIfEntry(void *ctxt, void **indices)
{
     rsvpIfEntry_t *ifp;
     ifIndex ifindex;

     if (*indices == NULL) {
	  ifp = next_rsvpIfEntry(NULL);
     } else {
	  ifindex = *( (ifIndex *) indices[0]);
	  ifp = find_rsvpIfEntry_ifindex(ifindex);
	  if (ifp == NULL)
	       return (SNMP_ERR_NO_SUCH_NAME);
	  ifp = next_rsvpIfEntry(ifp);
     }

     /*
      * At this point, ifp is pointing to the next ifentry.
      */
     if (ifp) {
	  indices[0] = (void *) &(ifp->sgi_ifindex);
	  return 0;
     } 
     return (SNMP_ERR_NO_SUCH_NAME);
}


void *
peer_locate_rsvpIfEntry(void *ctxt, void **indices, int op)
{
     ifIndex ifindex;
     rsvpIfEntry_t *ifp;

     ifindex = *( (ifIndex *) indices[0]);
     ifp = find_rsvpIfEntry_ifindex(ifindex);

     return ((void *) ifp);
}
#endif   /* _PEER */


/*
 * Find the rsvpIfEntry by the (kernel) if_index.
 * The if_index is the official index into the rsvpIfTable.
 */
rsvpIfEntry_t *
find_rsvpIfEntry_ifindex(ifIndex ifindex)
{
     if ((ifindex <= 0) || (ifindex > max_ifIndex))
	  return NULL;
     
     return (&(rsvpIfTable[ifindex]));
}


/*
 * Find the rsvpIfEntry by the if_vec index.
 * The if_vec index is the index used by rsvpd for the if_vec array.
 */
rsvpIfEntry_t *
find_rsvpIfEntry_ifvec(int ifvec)
{
     if ((ifvec < 0) || (ifvec >= if_num))
	  return NULL;

     return (&(rsvpIfTable[if_vec[ifvec].if_index]));
}


rsvpIfEntry_t *
next_rsvpIfEntry(rsvpIfEntry_t *ifp)
{
     if (ifp == NULL)
	  ifp = &rsvpIfTable[0];
     else
	  ifp++;

     while ((ifp->sgi_ifindex < max_ifIndex) &&
	    (ifp->rsvpIfStatus != ROWSTATUS_ACTIVE))
	  ifp++;

     if ((ifp->sgi_ifindex <= max_ifIndex) &&
	 (ifp->rsvpIfStatus == ROWSTATUS_ACTIVE))
	  return ifp;
     else
	  return NULL;
}


void
dump_rsvpIfEntry(rsvpIfEntry_t *ifp)
{
     printf("rsvpIfEntry (0x%x), ifindex %d\n", ifp, ifp->sgi_ifindex);

     printf("\trsvpIfUdpNbrs %d\n", ifp->rsvpIfUdpNbrs);
     printf("\trsvpIfIpNbrs %d\n", ifp->rsvpIfIpNbrs);
     printf("\trsvpIfNbrs %d\n", ifp->rsvpIfNbrs);
     printf("\trsvpIfEnabled %s\n", string_TruthValue(ifp->rsvpIfEnabled));
     printf("\trsvpIfUdpRequired %s\n", string_TruthValue(ifp->rsvpIfUdpRequired));
     printf("\trsvpIfRefreshBlockadeMultiple %d\n", ifp->rsvpIfRefreshBlockadeMultiple);
     printf("\trsvpIfRefreshMultiple %d\n", ifp->rsvpIfRefreshMultiple);
     printf("\trsvpIfTTL %d\n", ifp->rsvpIfTTL);
     printf("\trsvpIfRefreshInterval %d (ms)\n", ifp->rsvpIfRefreshInterval);
     printf("\trsvpIfRouteDelay %d (ticks) \n", ifp->rsvpIfRouteDelay);
     printf("\trsvpIfStatus %s\n", string_rowstatus(ifp->rsvpIfStatus));
     printf("\n");
}


/*
 ***********************************************************************************
 *
 * Begin rsvpNbrTable functions. (7)
 *
 * NbrEntry's are chained off of each rsvpIfEntry's.
 *
 ***********************************************************************************
 */

static rsvpNbrEntry_t *rsvpNbrTable;

void
rsvpNbrTable_init(void)
{
     rsvpNbrTable = NULL;

#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpNbrEntry,
		       (void *) (&rsvpNbrTable))) == NULL )
         log(LOG_ERR, 0, "PEER mgmt_new_instance failed for rsvpNbrTable\n");
#endif 
}


#ifdef _PEER
int
peer_next_rsvpNbrEntry(void *ctxt, void **indices)
{
     rsvpIfEntry_t *ifp;
     ifIndex ifindex;
     OCTETSTRING nbraddr, *indexp;
     char addrbuf[16];
     rsvpNbrEntry_t *nbrp;

     if (*indices == NULL) {
	  /*
	   * Return the first ifindex of the first ifentry.
	   */
	  ifp = &(rsvpIfTable[1]);
	  nbrp = ifp->sgi_nbrp;
     } else {
	  ifindex = *( (ifIndex *) indices[0]);
	  ifp = find_rsvpIfEntry_ifindex(ifindex);
	  if (ifp == NULL)
	       return (SNMP_ERR_NO_SUCH_NAME);
	  if (indices[1] == NULL) {
	       nbrp = ifp->sgi_nbrp;
	  } else {
	       /*
		* Copy the OCTETSTRING structure pointed to
		* by the peer index into a local buffer.
		*/
	       indexp = ( (OCTETSTRING *) indices[1]);
	       bzero(addrbuf, 16);
	       bcopy(indexp->val, addrbuf, indexp->len);
	       nbraddr.len = indexp->len;
	       nbraddr.val = (u_char *) addrbuf;
	       nbrp = find_rsvpNbrEntry(ifp, &nbraddr);
	       if (nbrp)
		    nbrp = nbrp->sgi_next;
	  }
     }

     if (nbrp == NULL) {
	  /* 
	   * have to find the next nbr entry, which may be on the next
	   * rsvpIfEntry.
	   */
	  while ((ifp = next_rsvpIfEntry(ifp))) {
	       if ((nbrp = ifp->sgi_nbrp))
		    break;
	  }
     }

     if (nbrp) {
	  indices[0] = (void *) &(ifp->sgi_ifindex);
	  indices[1] = (void *) &(nbrp->rsvpNbrAddress);
	  return 0;
     }

     return (SNMP_ERR_NO_SUCH_NAME);
}

void *
peer_locate_rsvpNbrEntry(void *ctxt, void **indices, int op)
{
     ifIndex ifindex;
     OCTETSTRING nbraddr;
     rsvpNbrEntry_t *nbrp;
     rsvpIfEntry_t *ifp;

     ifindex = *( (ifIndex *) indices[0]);
     nbraddr = *( (OCTETSTRING *) indices[1]);
     
     ifp = find_rsvpIfEntry_ifindex(ifindex);
     if (ifp == NULL)
	  return NULL;
     nbrp = find_rsvpNbrEntry(ifp, &nbraddr);

     return ((void *) nbrp);
}

#endif /* _PEER */


/*
 * When rsvpd receives a packet from a neighbor, it calls this function
 * so that the neighbor information can be recorded.
 * This function also has the side effect of updating the raw and encaps
 * counts in the ifEntry.
 */
void
rsvpd_heard_from(int input_if, struct sockaddr_in *sin_addrp, int udp_encaps)
{
     static u_char addrbuf[16];
     rsvpIfEntry_t *ifp;
     rsvpNbrEntry_t *nbrp;
     OCTETSTRING nbraddr;
     
     if (sin_addrp == NULL) {
	  /* heard from API, ignore */
	  return;
     }

     /*
      * If mrouted is not running, and a multicast RSVP protocol packets
      * comes in, we cannot tell which interface it came from.  (input_if
      * is -1.)  In that case, just chain the NbrEntry on the first ifindex.
      * Sometimes, if mrouted is running, then rsvpd is able to get the
      * the input interface from the routing socket interface (RSRR).
      * (The above is just my guess of what happens.  mwang)
      * In that case, chain the nbr on the correct if.
      */
     ifp = find_rsvpIfEntry_ifvec(input_if);
     if (ifp == NULL)
	  ifp = find_rsvpIfEntry_ifindex(1);
     if (ifp == NULL)
	  return;

     bzero((void *) addrbuf, 16);
     strcpy((char *) addrbuf, (char *) &(sin_addrp->sin_addr));
     nbraddr.val = addrbuf;
     nbraddr.len = 4;
     nbrp = find_rsvpNbrEntry(ifp, &nbraddr);
     if (nbrp) {
	  /*
	   * We've heard from this address before.  Check if this is a different
	   * enacapsulation.
	   */
	  if (nbrp->rsvpNbrProtocol != RSVPENCAP_BOTH) {
	       if ((nbrp->rsvpNbrProtocol == RSVPENCAP_IP) && udp_encaps) {
		    nbrp->rsvpNbrProtocol = RSVPENCAP_BOTH;
		    ifp->rsvpIfUdpNbrs++;
	       } else if ((nbrp->rsvpNbrProtocol == RSVPENCAP_UDP && !udp_encaps)) {
		    nbrp->rsvpNbrProtocol = RSVPENCAP_BOTH;
		    ifp->rsvpIfIpNbrs++;
	       }
	  }
     } else {
	  /* 
	   * This is a new neighbor.  Allocate a NbrEntry and insert it into
	   * the chain.
	   */
	  nbrp = calloc(1, sizeof(rsvpNbrEntry_t));
	  octetstring_fill(&(nbrp->rsvpNbrAddress), addrbuf, 4);
	  if (udp_encaps) {
	       nbrp->rsvpNbrProtocol = RSVPENCAP_UDP;
	       ifp->rsvpIfUdpNbrs++;
	  } else {
	       nbrp->rsvpNbrProtocol = RSVPENCAP_IP;	       
	       ifp->rsvpIfIpNbrs++;
	  }
	  nbrp->rsvpNbrStatus = ROWSTATUS_ACTIVE;
	  ifp->rsvpIfNbrs++;
	  insert_rsvpNbrEntry(ifp, nbrp);
     }
}

void
insert_rsvpNbrEntry(rsvpIfEntry_t *ifp, rsvpNbrEntry_t *nbrp)
{
     rsvpNbrEntry_t *p;

     nbrp->sgi_ifp = ifp;

#ifdef _MIB_VERBOSE
     printf("INSERTING ");
     dump_rsvpNbrEntry(nbrp);
#endif

     /*
      * Easy case, there are no neighbors yet on this interface.
      */
     if (ifp->sgi_nbrp == NULL) {
	  ifp->sgi_nbrp = nbrp;
	  return;
     }

     /*
      * The new neighbor should go first on the list.
      */
     p = ifp->sgi_nbrp;
     if(ascend_addr(&(nbrp->rsvpNbrAddress), &(p->rsvpNbrAddress))) {
	  nbrp->sgi_next = p;
	  p->sgi_prev = nbrp;
	  ifp->sgi_nbrp = nbrp;
	  return;
     }

     while ((p->sgi_next != NULL) &&
	    ascend_addr(&(nbrp->rsvpNbrAddress), &(p->rsvpNbrAddress)))
	  p = p->sgi_next;

     if (p->sgi_next == NULL) {
	  p->sgi_next = nbrp;
	  nbrp->sgi_prev = p;
     } else {
	  nbrp->sgi_next = p;
	  nbrp->sgi_prev = p->sgi_prev;
	  p->sgi_prev->sgi_next = nbrp;
	  p->sgi_prev = nbrp;
     }
}


/*
 * Return 1 if osp1 and osp2 are in ascending order (osp1 preceeds osp2).
 * Return 0 otherwise.
 * ipv4 addresses will preceed ipv6 addresses.
 */
int
ascend_addr(OCTETSTRING *osp1, OCTETSTRING *osp2)
{
     int len, i;

     if (osp1->len != osp2->len) {
	  if (osp1->len < osp2->len)
	       return 1;
	  else
	       return 0;
     }

     len = osp1->len;
     for (i=0; i < len; i++) {
	  if (osp1->val[i] < osp2->val[i])
	       return 1;
	  else if (osp1->val[i] > osp2->val[i])
	       return 0;
     }
     return 0;
}


rsvpNbrEntry_t *
find_rsvpNbrEntry(rsvpIfEntry_t *ifp, OCTETSTRING *nbraddrp)
{
     rsvpNbrEntry_t *nbrp;

     nbrp = ifp->sgi_nbrp;
     while (nbrp) {
	  if ((nbrp->rsvpNbrAddress.len == nbraddrp->len) &&
	      !(strncmp((char *) nbrp->rsvpNbrAddress.val,
			(char *) nbraddrp->val, nbraddrp->len)))
	       return nbrp;
	  nbrp = nbrp->sgi_next;
     }

     return NULL;
}


void
dump_rsvpNbrEntry(rsvpNbrEntry_t *nbrp)
{
     struct in_addr ia;

     printf("rsvpNbrEntry for ifp 0x%x ifIndex %d:\n",
	    nbrp->sgi_ifp, nbrp->sgi_ifp->sgi_ifindex);
     bcopy((void *) (nbrp->rsvpNbrAddress.val), &ia, nbrp->rsvpNbrAddress.len);
     printf("\trsvpNbrAddress %s\n", inet_ntoa(ia));
     printf("\trsvpNbrProtocol %s\n",
	    string_RsvpEncapsulation(nbrp->rsvpNbrProtocol));
     printf("\trsvpNbrStatus %s\n", string_rowstatus(nbrp->rsvpNbrStatus));
     printf("\n");
}


/*
 ***********************************************************************************
 *
 * Code to deal with the rsvpBadPackets variable {rsvpGenObjects 1}
 * type is Gauge32 (max value of 2^32-1).
 *
 ***********************************************************************************
 */
u_int rsvpBadPackets = 0;

int
rsvpBadPackets_init(void)
{
#ifdef _PEER
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_rsvpGenObjects,
		       (void *) &(rsvpBadPackets))) == NULL )
         log(LOG_ERR, 0, "mgmt_new_instance failed for rsvpBadPackets\n");
#endif 
     return 0;
}

#ifdef _PEER
int
GET_rsvpBadPackets(void *ctxt, void **indices, void *attr_ref)
{
	Gauge32	*attr = (Gauge32 *) attr_ref;

	*attr = rsvpBadPackets;
	return(SNMP_ERR_NO_ERROR);
}
#endif /* _PEER */

/*
 * rsvpd can only increment this variable.
 */
void
rsvpd_inc_rsvpBadPackets(void)
{
     if (rsvpBadPackets < 0xffffffff)
	  rsvpBadPackets++;
}



/*
 ***********************************************************************************
 *
 * Code to deal with notifications.
 *
 ***********************************************************************************
 */
typedef struct notctx {
     intSrvFlowEntry_t *flowp;
     rsvpSessionEntry_t *sessp;
     rsvpSenderEntry_t *sendp;
     rsvpResvEntry_t *resvp;
     rsvpResvFwdEntry_t *fwdp;
} flownotctx_t;

flownotctx_t flownotctx;

int
rsvpNotifications_init(void)
{
     flownotctx.flowp = NULL;

#ifdef _PEER
/*
     if ( (mgmt_new_instance(peer_mgmt_env, &SMI_GROUP_trap,
			     (void *) &flownotctx)) == NULL )
         log(LOG_ERR, 0, "mgmt_new_instance failed for trap\n");
*/
#endif

     return 0;
}


void
rsvpd_newFlow(int kernel_handle, void *sessp, void *resvp, void *fwdp)
{
#ifdef _PEER
     int status;
#endif
     int fwdNumber, fwdStatus;

     flownotctx.flowp = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (flownotctx.flowp == NULL)
	  return;

     /*
      * Only send a notification for the first filter installed on the flow.
      * Be silent about for the second, third, etc. filters on the same flow.
      */
     if (flownotctx.flowp->sgi_num_filters != 1)
	  return;

     /* 
      * record which session/sender/flow, etc had the new flow installed.
      */
     flownotctx.sessp = (rsvpSessionEntry_t *) sessp;
     flownotctx.sendp = (rsvpSenderEntry_t *) flownotctx.sessp->sgi_senderp;
     flownotctx.resvp = (rsvpResvEntry_t *) resvp;
     flownotctx.fwdp = (rsvpResvFwdEntry_t *) fwdp;

     if (flownotctx.fwdp== NULL) {
	   fwdNumber= fwdStatus= 0;
     } else {
	   fwdNumber= flownotctx.fwdp->rsvpResvFwdNumber;
	   fwdStatus= flownotctx.fwdp->rsvpResvFwdStatus;
     }

#ifdef _PEER
     status = send_newFlow_trap(peer_mgmt_env, flownotctx.flowp->intSrvFlowNumber, flownotctx.flowp->intSrvFlowStatus,
	          flownotctx.sessp->rsvpSessionNumber, flownotctx.sessp->rsvpSessionDestAddr,
	          flownotctx.sessp->rsvpSessionNumber, fwdNumber, fwdStatus,
                  flownotctx.sessp->rsvpSessionNumber, flownotctx.resvp->rsvpResvNumber, flownotctx.resvp->rsvpResvStatus,
                  flownotctx.sessp->rsvpSessionNumber, flownotctx.sendp->rsvpSenderNumber, flownotctx.sendp->rsvpSenderStatus);

#endif

#ifdef _MIB_VERBOSE
     printf("*** rsvpd_newflow: installed flow %d (0x%x)\n",
	    flownotctx.flowp->intSrvFlowNumber,
	    flownotctx.flowp->intSrvFlowNumber);
     printf("\tsession 0x%x\n", flownotctx.sessp->rsvpSessionNumber);
     printf("\tsender 0x%x\n", flownotctx.sendp->rsvpSenderNumber);
     printf("\tresv 0x%x\n", flownotctx.resvp->rsvpResvNumber);
     printf("\tresvfwd 0x%x\n", fwdNumber);
     printf("\tresvfwdStatus 0x%x\n", fwdStatus);
#endif
}


void
rsvpd_lostFlow(int kernel_handle, void *sessp, void *resvp, void *fwdp)
{
#ifdef _PEER
     int status;
#endif
     int fwdNumber, fwdStatus;

     flownotctx.flowp = find_intSrvFlowEntry_kernhand(kernel_handle);
     if (flownotctx.flowp == NULL) {
	  /*
	   * the kernel flow is already gone at this point, so I guess
	   * I can't pass up a handle to the user.
	   */
           /*  printf("lostFlow: could not find flow, kernel_handle %d\n",
		 kernel_handle); */
	  return;
     }

     /*
      * Only send a notification after the last filter has been removed.
      * Be silent about for the second, third, etc. filters on the same flow.
      */
     if (flownotctx.flowp->sgi_num_filters != 0) {
	  return;
     }

     /* 
      * record which session/sender/flow, etc had the new flow deleted.
      */
     flownotctx.sessp = (rsvpSessionEntry_t *) sessp;
     flownotctx.sendp = (rsvpSenderEntry_t *) flownotctx.sessp->sgi_senderp;
     flownotctx.resvp = (rsvpResvEntry_t *) resvp;
     flownotctx.fwdp = (rsvpResvFwdEntry_t *) fwdp;

     if (flownotctx.fwdp== NULL) {
	fwdNumber= fwdStatus= 0;
     } else {
	fwdNumber= flownotctx.fwdp->rsvpResvFwdNumber;
	fwdStatus= flownotctx.fwdp->rsvpResvFwdStatus;
     }


#ifdef _PEER
     status = send_lostFlow_trap(peer_mgmt_env, flownotctx.flowp->intSrvFlowNumber, flownotctx.flowp->intSrvFlowStatus,
	          flownotctx.sessp->rsvpSessionNumber, flownotctx.sessp->rsvpSessionDestAddr,
	          flownotctx.sessp->rsvpSessionNumber, fwdNumber, fwdStatus,
                  flownotctx.sessp->rsvpSessionNumber, flownotctx.resvp->rsvpResvNumber, flownotctx.resvp->rsvpResvStatus,
                  flownotctx.sessp->rsvpSessionNumber, flownotctx.sendp->rsvpSenderNumber, flownotctx.sendp->rsvpSenderStatus);

#endif

#ifdef _MIB_VERBOSE
     printf("*** rsvpd_lostflow: lost flow %d (0x%x)\n",
	    flownotctx.flowp->intSrvFlowNumber,
	    flownotctx.flowp->intSrvFlowNumber);
     printf("\tsession 0x%x\n", flownotctx.sessp->rsvpSessionNumber);
     printf("\tsender 0x%x\n", flownotctx.sendp->rsvpSenderNumber);
     printf("\tresv 0x%x\n", flownotctx.resvp->rsvpResvNumber);
     printf("\tresvfwd 0x%x\n", fwdNumber);
#endif
}

/*
 ***********************************************************************************
 *
 * Generally useful functions.
 *
 ***********************************************************************************
 */

char *
string_rowstatus(RowStatus s)
{
     char *st;

     switch(s) {
     case ROWSTATUS_ACTIVE:
	  st = "active";
	  break;

     case ROWSTATUS_NOTINSERVICE:
	  st = "notInService";
	  break;

     case ROWSTATUS_NOTREADY:
	  st = "notReady";
	  break;

     case ROWSTATUS_CREATEANDGO:
	  st = "createAndGo";
	  break;

     case ROWSTATUS_CREATEANDWAIT:
	  st = "createAndWait";
	  break;

     case ROWSTATUS_DESTROY:
	  st = "destroy";
	  break;

     default:
	  log(LOG_ERR, 0, "invalid row status value %d\n", s);
	  st = "invalid rowstatus";
	  break;
     }

     return st;
}

char *
string_QosService(QosService q)
{
     char *st;

     switch (q) {
     case QOS_BESTEFFORT:
	  st = "Best Effort";
	  break;

     case QOS_GUARENTEEDDELAY:
	  st = "Guarenteed Delay";
	  break;

     case QOS_CONTROLLEDLOAD:
	  st = "Controlled Load";
	  break;

     default:
	  st = "invalid QosService";
	  break;
     }

     return st;
}
	  

char *
string_TruthValue(TruthValue t)
{
     static char tvsbuf[40];

     if (t == TRUTHVALUE_TRUE) {
	  sprintf(tvsbuf, "true");
     } else if (t == TRUTHVALUE_FALSE) {
	  sprintf(tvsbuf, "false");
     } else {
	  sprintf(tvsbuf, "invalid truthvalue (%d)", t);
     }

     return tvsbuf;
}


char *
string_RsvpEncapsulation(RsvpEncapsulation e)
{
     if (e == RSVPENCAP_IP)
	  return "ENCAP_IP";
     if (e == RSVPENCAP_UDP)
	  return "ENCAP_UDP";
     if (e == RSVPENCAP_BOTH)
	  return "ENCAP_BOTH_IP_UDP";

     return "invalid encaps value";
}


/*
 * Malloc the space requred for the OCTETSTRING, length len, and bcopy
 * the value from val.
 */
void
octetstring_fill(OCTETSTRING *osp, void *val, int len)
{
     osp->val = calloc(1, len);
     if (osp->val == NULL) {
	  log(LOG_ERR, 0, "calloc failed for MIB OCTETSTRING\n");
	  osp->len = 0;
	  return;
     }

     bcopy(val, (void *) osp->val, len);
     osp->len = len;
}

/*
 * type timestamp, defined by rfc 1903, which refers to 
 * sysUpTime, defined by rfc 1907 as the number of 10ms ticks since
 * the network management portion of the system was last re-initialized.
 * This is kept in a MIB variable called sysUpTime {system 3}, but since
 * I'm not sure how to access this, I'll just return the rsvpd.
 */
TimeStamp
get_mib_timestamp(void)
{
     TimeStamp ts;
     struct timeval tv;

     gettimeofday(&tv);
     if (tv.tv_usec < starttv.tv_usec) {
	  tv.tv_usec += 1000000;
	  tv.tv_sec -= 1;
     }
     
     ts = (tv.tv_sec - starttv.tv_sec) * 100 +
	  (tv.tv_usec - starttv.tv_usec) / 10000;

     return ts;
}
#endif /* _MIB */
