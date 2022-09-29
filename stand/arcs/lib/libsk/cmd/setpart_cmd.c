#ifdef SN0
#include <sys/types.h>
#include <sys/SN/arch.h>
#include <sys/SN/gda.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/ip27config.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <ksys/elsc.h>

#ifdef VBPRT
#undef VBPRT
#endif

#define	VBPRT	if (verbose) printf

#define	PART_DEBUG	0

typedef struct part_ip_s {
    partid_t	part_id;
    int		n_modules;
    moduleid_t	modules[MAX_MODULES];
} part_ip_t;

typedef struct part_s {
    part_ip_t	partitions[MAX_MODULES];
    int		n_partitions;
} part_t;

#define	PART_CMD_NONE	0
#define	PART_CMD_SETUP	1
#define	PART_CMD_CLEAR	2
#define	PART_CMD_HELP	3
#define	PART_CMD_LIST	4
#define SET_DOMAIN 	1
#define SET_CLUSTER	2

#define	MAX_DEPTH		8
#define	MAX_ROUTER_BRDS		128

static int check_valid(part_t *partip, int verbose);
static int do_partitions(part_t *partip, int verbose);

static void print_part(part_t *paritp, int verbose);
static int get_part_cmd_code(int argc, char **argv, int *verbose);

static int part_null(int verbose);
static int part_setup(int verbose);
static int part_clear(int verbose);
static int part_help(int verbose);
static int part_list(int verbose);
static int set_domain_cluster(int flag);
static int get_domain_cluster(int flag);

static int (*part_cmds[])(int) = {
		part_null,
		part_setup,
		part_clear, 
		part_help,
		part_list
};

#define	END_CHAR	'q'	

static nasid_t	last_nasid, trying_nasid;
extern nasid_t	master_nasid;

typedef struct rou_flag_nic_s {
    nic_t		nic;
    unsigned short	flags;
} rou_flag_nic_t;

#define	ROU_FLAG_PORT_FENCE(p)		(1 << p)

/*
 * Function:		setpart_cmd -> sets up partitions
 * Args:		argc, argv, envp
 * Returns:		0
 */

int setpart_cmd(int argc, char **argv, char **envp)
{
	int		cmd_code, verbose;

	if (CONFIG_12P4I ) {
		printf("Partitioning unsupported on 12P4IO config!\n");
		return 0;
	}

	if ((cmd_code = get_part_cmd_code(argc, argv, &verbose)) == 
			PART_CMD_NONE)
		cmd_code = PART_CMD_SETUP;

	(*part_cmds[cmd_code])(verbose);

	return 0;
}

int setdomain_cmd(int argc, char **argv, char **envp)
{
	int		cmd_code;
	if(argc == 2) {
		if (!strcmp(argv[1], "-h")) {
        		printf("\tsetdomain:\t\tSetup/List domain ID's.\n");
        		printf("\toptions:\n");
        		printf("\t\t-h:\t\tPrint out help about cmd\n");
        		printf("\t\t-s:\t\tSetup domain ID's\n");
        		printf("\t\t-l:\t\tList all domain ID's\n");
			return 0;
		}
		else if (!strcmp(argv[1], "-s")) {
			set_domain_cluster(SET_DOMAIN);
			return 0;
		}
		else if (!strcmp(argv[1], "-l")) {
			get_domain_cluster(SET_DOMAIN);
			return 0;
		}
	}
	return -1;
}

int setcluster_cmd(int argc, char **argv, char **envp)
{
	int		cmd_code;
	if(argc == 2) {
		if (!strcmp(argv[1], "-h")) {
        		printf("\tsetcluster:\t\tSetup/List cluster ID's.\n");
        		printf("\toptions:\n");
        		printf("\t\t-h:\t\tPrint out help about cmd\n");
        		printf("\t\t-s:\t\tSetup cluster ID's\n");
        		printf("\t\t-l:\t\tList all cluster ID's\n");
			return 0;
		}
		else if (!strcmp(argv[1], "-s")) {
			set_domain_cluster(SET_CLUSTER);
			return 0;
		}
		else if (!strcmp(argv[1], "-l")) {
			get_domain_cluster(SET_CLUSTER);
			return 0;
		}
	}
	return -1;
}

/*
 * Function:		get_part_cmd_code -> get partition cmd code
 * Args:		argc, argv, verbose ptr
 * Returns:		cmd code and fills up verbose info.
 * Logic:		Checks if options were legal, etc.
 */

static int get_part_cmd_code(int argc, char **argv, int *verbose)
{
	int	cmd_code = PART_CMD_NONE;

	*verbose = 0;

	if (argc == 1)
		cmd_code = PART_CMD_SETUP;
	else {
		int	i;

		for (i = 1; i < argc; i++) {
			if (!strcmp(argv[i], "-v")) 
				*verbose = 1;
			else if (!strcmp(argv[i], "-s")) {
				if (cmd_code != PART_CMD_NONE)
					goto error;
				else    
					cmd_code = PART_CMD_SETUP;
			}
                        else if (!strcmp(argv[i], "-c")) {
				if (cmd_code != PART_CMD_NONE)
					goto error;
				else
					cmd_code = PART_CMD_CLEAR;
			}
                        else if (!strcmp(argv[i], "-h")) {
				if (cmd_code != PART_CMD_NONE)
					goto error;
				else
					cmd_code = PART_CMD_HELP;
			}
                        else if (!strcmp(argv[i], "-l")) {
				if (cmd_code != PART_CMD_NONE)
					goto error;
				else
					cmd_code = PART_CMD_LIST;
			}
			else
				goto error;
		}
	}

	return cmd_code;

error:
	printf("ERROR: Illegal option(s)\n");
	cmd_code = PART_CMD_HELP;
	return cmd_code;
}

static int part_null(int verbose)
{
	printf("ERROR! NULL executed.\n");
	return 0;
}

static int part_setup(int verbose)
{
	int		i, n;
	part_t		all_partitions;
	char		buf[64];

	printf("**********************************************************\n");
	printf("*            Entering partition command monitor          *\n");
	printf("* Setting up partition requires a list of partition ids  *\n");
	printf("*             and module ids in each partition           *\n");
	printf("*  If this is already a partitioned machine, you need to *\n");
	printf("*     tear down all partitions before setting them up    *\n");
	printf("*            Do a 'setpart -h' for all commands          *\n");
	printf("**********************************************************\n");
	printf("\nProceed [y] ? ");

	if (!gets(buf))
		return 0;

	if ((buf[0] == 'n') || (buf[0]== 'N'))
		return 0;

	printf("\n");

	printf("**********************************************************\n");
	printf("* Enter partition ids followed by the module ids in that *\n");
	printf("*  partition. Type in a 'q' at the module prompt if you  *\n");
	printf("*   are done with all modules in a partition. Type in a  *\n");
	printf("*   'q' at the partition prompt if you're done with all  *\n");
	printf("*                       partitions.                      *\n");
	printf("**********************************************************\n");

	all_partitions.n_partitions = 0;

	for (i = 0; i < MAX_MODULES; i++)
		all_partitions.partitions[i].n_modules = 0;

	n = 0;

	while (1) {
		int		m = 0;

		printf("\nEnter partition id: ");

		if (!gets(buf))
			break;
	
		if (buf[0] == END_CHAR)
			break;

		all_partitions.partitions[n].part_id = atoi(buf);

		while (1) {
			char	mbuf[64];

			printf("Enter module id: ");

			if (!gets(mbuf))
				break;
	
			if (mbuf[0] == END_CHAR)
				break;

			all_partitions.partitions[n].modules[m++] = atoi(mbuf);

			all_partitions.partitions[n].n_modules = m;
		}

		n++;
		all_partitions.n_partitions = n;
	}

	printf("\n");
	print_part(&all_partitions, verbose);
	printf("\n");

	printf("Do you wish to proceed [y] ? ");

	if (!gets(buf))
		return 0;

	if ((buf[0] == 'n') || (buf[0] == 'N'))
		return 0;

	printf("\n");

	if (check_valid(&all_partitions, verbose) < 0)
		return 0;

	if (do_partitions(&all_partitions, verbose) < 0)
		return 0;

	printf("Reset to partition system.\n");

	return 0;
}

static int part_help(int verbose)
{
	printf("\tsetpart:\t\tSetup/teardown partitions.\n");
	printf("\toptions:\n");
	printf("\t\t-v:\t\tVerbose\n");
	printf("\t\t-h:\t\tPrint out help about cmds\n");
	printf("\t\t-s:\t\tSetup partitions\n");
	printf("\t\t-c:\t\tClear all partitions\n");
	printf("\t\t-l:\t\tList all partitions\n");

	return 0;
}

static int
in_my_partition(nasid_t nasid)
{
	gda_t		*gdap = (gda_t *) GDA_ADDR(master_nasid);
	cnodeid_t	cnode;

	for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode++)
		if (gdap->g_nasidtable[cnode] == INVALID_NASID)
			return 0;
		else if (gdap->g_nasidtable[cnode] == nasid)
			return 1;

	return 0;
}

/*
 * Function:		part_clear -> tear down all partitions. Very unelegant
 * Args:		verbose
 * Returns:		rc
 * Logic:		If any nasid doesn't take a exception, it has to be 
 *			cleared
 */

static int part_clear(int verbose)
{
	jmp_buf		new_buf;
	void		*old_buf;
	char		buf[8];
	int		i;
	char		nasid_to_reset[MAX_NASIDS];

	printf("**********************************************************\n");
	printf("*    You need to reset all partitions to coalesce the    *\n");
	printf("*            machine back into a single system.          *\n");
	printf("**********************************************************\n");

	printf("\nProceed [y] ? ");

	if (!gets(buf))
		return 0;

	if ((buf[0] == 'n') || (buf[0]== 'N'))
		return 0;

	printf("\n");

	bzero(nasid_to_reset, MAX_NASIDS);

	last_nasid = 0;

	VBPRT("Tearing down partitions...\n");

	if (setfault(new_buf, &old_buf)) {
		last_nasid = trying_nasid + 1;
	}

	for (trying_nasid = last_nasid; trying_nasid < MAX_NASIDS; 
					trying_nasid++) {
		elsc_t			remote_elsc;
		volatile __uint64_t	sr0;

		VBPRT("Tearing down partiion on nasid %d\n", trying_nasid);

		/* Dummy read */

		sr0 = REMOTE_HUB_L(trying_nasid, NI_SCRATCH_REG0);
		REMOTE_HUB_S(trying_nasid, NI_SCRATCH_REG0, sr0);

		if (SN00)
			ip27log_unsetenv(trying_nasid, IP27LOG_PARTITION, 0);
		else {
			elsc_init(&remote_elsc, trying_nasid);
			if (elsc_partition_set(&remote_elsc, 0) < 0)
				printf("*** ERROR: Can't set pttnid on nasid"
					" %d\n", trying_nasid);
		}

		if (!in_my_partition(trying_nasid))
			nasid_to_reset[trying_nasid] = 1;
	}

	restorefault(old_buf);

	printf("\nTore down all partitions. Reset entire system [y] ? ");

	if (!gets(buf))
		return 0;

	if ((buf[0] == 'n') || (buf[0]== 'N'))
		return 0;

	printf("\n");

	/*
	 * We could die trying to reset a nasid, whose partition has
	 * already been reset. Catch exceptions here!
	 */

	last_nasid = 0;

	if (setfault(new_buf, &old_buf)) {
		last_nasid = trying_nasid + 1;
	}

	for (trying_nasid = last_nasid; trying_nasid < MAX_NASIDS; 
					trying_nasid++)
		if (nasid_to_reset[trying_nasid]) {
			VBPRT("Resetting nasid %d\n", trying_nasid);
			REMOTE_HUB_S(trying_nasid, NI_PORT_RESET,
				NPR_PORTRESET | NPR_LOCALRESET);
		}

	LOCAL_HUB_S(NI_PORT_RESET, NPR_PORTRESET | NPR_LOCALRESET);

	/* Never gets here */
	return 0;
}

int check_add_module(part_t *allpart, partid_t partition, moduleid_t module)
{
	int	i, found_module = 0, found_partition = 0;

	for (i = 0; i < allpart->n_partitions; i++) {
		int	j;

		if (allpart->partitions[i].part_id != partition)
			continue;

		found_partition = 1;

		for (j = 0; j < allpart->partitions[i].n_modules; j++)
			if (allpart->partitions[i].modules[j] == 
				module) {
				found_module = 1;
				break;
			}

		break;
	}

	if (found_partition) {
		if (!found_module) {
			int	n;

			n = allpart->partitions[i].n_modules;

			if (n == MAX_MODULES)
				return -1;

			allpart->partitions[i].modules[n] = module;
			allpart->partitions[i].n_modules++;
		}
	}
	else {
		int	n;

		n = allpart->n_partitions;

		if (n == MAX_MODULES)
			return -1;

		allpart->partitions[n].part_id = partition;
		allpart->partitions[n].modules[0] = module;
		allpart->partitions[n].n_modules = 1;
		allpart->n_partitions++;
	}

	return 0;
}

/*
 * Function:		part_list -> lists all partitions
 * Args:		verbose
 * Returns:		0
 */

static int part_list(int verbose)
{
	part_t		all_partitions;
	int		i;
	jmp_buf		new_buf;
	void		*old_buf;

	all_partitions.n_partitions = 0;

	for (i = 0; i < MAX_MODULES; i++) {
		int	j;

		all_partitions.partitions[i].n_modules = 0;
		all_partitions.partitions[i].part_id = 0;

		for (j = 0; j < MAX_MODULES; j++)
			all_partitions.partitions[i].modules[j] = -1;
	}

	last_nasid = 0;

	if (setfault(new_buf, &old_buf))
		last_nasid = trying_nasid + 1;

	for (trying_nasid = last_nasid; trying_nasid < MAX_NASIDS; 
			trying_nasid++) {
		elsc_t			remote_elsc;
		int			partition, module;
		volatile __uint64_t	sr0;

		/* Dummy read */

		sr0 = REMOTE_HUB_L(trying_nasid, NI_SCRATCH_REG0);
		REMOTE_HUB_S(trying_nasid, NI_SCRATCH_REG0, sr0);

		if (SN00) {
			char		buf[8];

			ip27log_getenv(trying_nasid, IP27LOG_PARTITION, buf,
				"0", 0);
			partition = atoi(buf);

			if (ip27log_getenv(trying_nasid, IP27LOG_MODULE_KEY, 
				buf, 0, 0) < 0) {
				module = -1;
				printf("WARNING: %s unset\n", 
					IP27LOG_MODULE_KEY);
			}
			else
				module = atoi(buf);
		}
		else {
			elsc_init(&remote_elsc, trying_nasid);

			if ((partition = elsc_partition_get(&remote_elsc)) < 0)
				printf("*** ERROR %d: Can't get pttnid from "
				"nasid %d\n", partition, trying_nasid);


			if ((module = elsc_module_get(&remote_elsc)) < 0)
				printf("*** ERROR %d: Can't get modid from "
				"nasid %d\n", module, trying_nasid);
		}

		if ((partition >= 0) && (module >= 0) && (check_add_module(
				&all_partitions, partition, module) < 0))
			VBPRT("WARNING: Cannot account for module %d in "
				"partition %d\n", module, partition);
	}

	restorefault(old_buf);

	printf("************ List of partitions ************\n");

	print_part(&all_partitions, verbose);

	printf("************ End list of partitions ************\n");

	return 0;
}

static int
in_src_partition(lboard_t *src, net_vec_t vec)
{
	int		i;
	lboard_t	*next = src;
	net_vec_t	port_list = vector_reverse(vec);

	for (i = 0; i < vector_length(vec); i++) {
		int	port = vector_get(port_list, i);
		klrou_t	*info;

		if (next->brd_partition != src->brd_partition)
			return 0;

		if (!(info = (klrou_t *) 
			find_first_component(next, KLSTRUCT_ROU)))
			return 0;

		if (info->rou_port[port].port_nasid == INVALID_NASID)
			return 0;

		next = (lboard_t *) NODE_OFFSET_TO_K1(
			info->rou_port[port].port_nasid,
			info->rou_port[port].port_offset);
	}

	return 1;
}

static int
check_ctus_route(lboard_t *brd, void *ap, void *rc)
{
	int		found, nth_alternate;
	lboard_t	*src = (lboard_t *) ap;
	net_vec_t	vec;

	if (*(int *) rc)
		return 1;

	if ((KLCLASS(brd->brd_type) != KLCLASS_ROUTER) ||
		(brd->brd_partition != src->brd_partition))
		return 0;

	found = nth_alternate = 0;

	while ((vec = klcfg_discover_route(src, brd, nth_alternate++)) !=
		NET_VEC_BAD)
		if (in_src_partition(src, vec)) {
			found = 1;
			break;
		}

	if (!found) {
		printf("ERROR: No contiguous route found from src rtr NIC 0x%lx"
			" nasid %d to dst rtr NIC 0x%lx nasid %d\n",
			src->brd_nic, src->brd_nasid, brd->brd_nic,
			brd->brd_nasid);
		*(int *) rc = 1;
		return 1;
	}

	return 0;
}

static int
check_router(lboard_t *brd, void *ap, void *rc)
{
	if ((KLCLASS(brd->brd_type) != KLCLASS_ROUTER) ||
			(brd->brd_type == KLTYPE_META_ROUTER) || *(int *) rc)
		return 1;

	visit_lboard(check_ctus_route, (void *) brd, rc);

	return (*(int *) rc ? 1 : 0);
}

static int
power_of_2(int n)
{
	int         i, one_seen = 0;

	for (i = 0; i < 8 * sizeof(int); i++)
		if (n & 0x1 << i) {
			if (one_seen)
				return 0;
			else
				one_seen = 1;
		}

	return 1;
}

/*
 * Function:		check_valid -> check if input is valid
 * Args:		part_ip_t * -> ip ptr
 * Logic:		Checks basic stuff like
 *			1. If there's atleast one partition
 *			2. If there's atleast one module in each partition
 *			3. If any module appears in > 1 partition
 *			4. Any illegal module & if all modules are covered
 *			5. Golden rules of partitioning
 *				5a) Power of 2 # of module
 *				5b) Partitionid musn't conflict with a existing 
 *					partition
 *				5c) Partitions must be fully contiguous
 *				5d) Partitions must be fully interconnected ?
 *			6. Any more needed ?
 */

static int check_valid(part_t *partip, int verbose)
{
	int		i, rc;
	gda_t		*gdap;
	__psunsigned_t	result;

	/*
	 * 1. If there's atleast one partition
	 */

	if (!partip->n_partitions) {
		printf("ERROR: Need atleast one partition!\n");
		return -1;
	}

	/*
	 * 2. If there's atleast one module in each partition
	 * 3. If any module appears in > 1 partition
	 */

	for (i = 0; i < partip->n_partitions; i++) {
		int	j;

		if (!partip->partitions[i].n_modules) {
			printf("ERROR: Partition id %d needs atleast one"
				" module\n", partip->partitions[i].part_id);
			return -1;
		}

		if ((partip->partitions[i].part_id < 0) || 
			(partip->partitions[i].part_id >= MAX_PARTITIONS)) {
			printf("ERROR: Partition id should be >= 0 and "
				"< %d\n", MAX_PARTITIONS);
			return -1;
		}

		for (j = 0; j < partip->partitions[i].n_modules; j++) {
			int	k;

			for (k = 0; k < partip->n_partitions; k++) {
				int	l;

				if (partip->partitions[i].part_id ==
						partip->partitions[k].part_id)
					break;

				for (l = 0; l < 
					partip->partitions[k].n_modules; l++)
					if (partip->partitions[i].modules[j]
					== partip->partitions[k].modules[l])
						printf("ERROR: module %d is "
						"present in both partition %d"
						" and %d\n",
						partip->partitions[i].modules[j], 
						partip->partitions[i].part_id,
						partip->partitions[k].part_id);
			}
		}
	}

	gdap = (gda_t *) GDA_ADDR(get_nasid());

	/*
	 * 4. Are all moduleids legal ?
	 */

	for (i = 0; i < partip->n_partitions; i++) {
		int	j;

		for (j = 0; j < partip->partitions[i].n_modules; j++) {
			int	k, found = 0;

			for (k = 0; k < MAX_COMPACT_NODES; k++)
				if (gdap->g_nasidtable[k] != INVALID_NASID) {
					lboard_t	*l;

					if (!(l = find_lboard((lboard_t *)
					KL_CONFIG_INFO(gdap->g_nasidtable[k]),
					KLTYPE_IP27))) {
						VBPRT("WARNING: nasid %d does "
						"not have a IP27 brd\n",
						gdap->g_nasidtable[k]);
						continue;
					}

					if (partip->partitions[i].modules[j]
						== l->brd_module) {
						found = 1;
						break;
					}
				}

			if (!found) {
				printf("ERROR: partition %d module %d is a "
				"invalid module id\n", 
				partip->partitions[i].part_id, 
				partip->partitions[i].modules[j]);
				return -1;
			}
		}
	}

	/*
	 * 4. If all modules are covered
	 */

	for (i = 0; i < MAX_COMPACT_NODES; i++)
		if (gdap->g_nasidtable[i] != INVALID_NASID) {
			lboard_t	*l;
			int		j, found = 0;

			if (!(l = find_lboard((lboard_t *) KL_CONFIG_INFO(
				gdap->g_nasidtable[i]), KLTYPE_IP27))) {
				VBPRT("WARNING: nasid %d doesn't have a IP27"
					"\n", gdap->g_nasidtable[i]);
				continue;
			}

			for (j = 0; j < partip->n_partitions; j++) {
				int	k;

				
				for (k = 0; k < 
					partip->partitions[j].n_modules; k++)
					if (partip->partitions[j].modules[k]
						== l->brd_module) {
						found = 1;
						break;
					}

				if (found)
					break;
			}

			if (!found) {
				printf("ERROR: module %d not included in any "
					"partition\n", l->brd_module);
				return -1;
			}
		}

	/*
	 * 5. Golden rules of partitioning
	 *	5a) Power of 2 # of module
	 *	XXX: Turned off for now
	 */

#if 0
	for (i = 0; i < partip->n_partitions; i++)
		if (!power_of_2(partip->partitions[i].n_modules)) {
			printf("ERROR: Partition %d has %d modules. Each "
				"partition needs to have a power of 2 # of "
				"modules.\n", partip->partitions[i].part_id,
				partip->partitions[i].n_modules);
			return -1;
		}
#endif

	/*
	 * 5b) Partitionid musn't conflict with a existing partition
	 * We'll check for running partitions later on.
	 * XXX: Need some elsc locking to do that
	 */


	/*
	 * 5c) Partitions must be fully contiguous
	 * 5d) Partitions must be fully interconnected
         * Visit each partition. There must a route (same as the shortest route
	 * depth) between any two routers in the same partition consisting 
	 * entirely of routers in the same partition.
	 */

	rc = 0;
	visit_lboard(check_router, 0, (void *) &rc);
	if (rc)
		return -1;

	return 0;
}

static nasid_t
module_nasid(moduleid_t module, nasid_t *skip_nasid)
{
	cnodeid_t	cnode;
	gda_t		*gdap = (gda_t *) GDA_ADDR(get_nasid());

	for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode++) {
		lboard_t	*brd;

		if (gdap->g_nasidtable[cnode] == INVALID_NASID)
			continue;

		if (gdap->g_nasidtable[cnode] == *skip_nasid)
			continue;

		if (!(brd = find_lboard((lboard_t *) KL_CONFIG_INFO(
			gdap->g_nasidtable[cnode]), KLTYPE_IP27)))
			continue;

		if (brd->brd_module == module)
			return gdap->g_nasidtable[cnode];
	}

	return INVALID_NASID;
}

/*
 * Function:		do_partitions -> sets up partitions
 * Args:		partip -> all partitions
 * Returns:		0
 */

static int do_partitions(part_t *partip, int verbose)
{
	int	i;

	for (i = 0; i < partip->n_partitions; i++) {
		int	j;

		for (j = 0; j < partip->partitions[i].n_modules; j++) {
			int		rc = -1;
			nasid_t		nasid = INVALID_NASID;

			while (rc < 0) {
				elsc_t	remote_elsc;

				nasid = module_nasid(
					partip->partitions[i].modules[j],
					&nasid);

				if (nasid == INVALID_NASID) {
					printf("ERROR: Can't write partitionid"
					" to MSC!\n");
					return -1;
				}

				if (SN00) {
					char	buf[8];

					sprintf(buf, "%d", 
						partip->partitions[i].part_id);
					rc = ip27log_setenv(nasid, 
						IP27LOG_PARTITION, buf, 0);
				}
				else {
					elsc_init(&remote_elsc, nasid);

					if ((rc = elsc_partition_set(
						&remote_elsc, (int)
						partip->partitions[i].part_id))
						< 0)
						printf("*** ERROR %d: Can't set"
							" pttnid on nasid %d\n",
							rc, nasid);
				}
			}
		}
	}

	return 0;
}

static void print_part(part_t *paritp, int verbose)
{
	int	i;

	for (i = 0; i < paritp->n_partitions; i++) {
		int	j;

		printf("Partition: %d\n", paritp->partitions[i].part_id);

		for (j = 0; j < paritp->partitions[i].n_modules; j++)
			printf("\t\tModule\t%d\n", paritp->partitions[i].modules[j]);
	}
}


static int set_domain_cluster(int flag)
{
	uint	mod;
	uint	id;
	char 	buf[256];
	printf("Starting to set up domain id's\n");
	while (1) {
		printf("Enter module name <'q' to quit>\t\t");
		if(!gets(buf))
			return 0;
		if((buf[0] == 'q') || (buf[0] == 'Q'))
			break;
		mod = atoi(buf);
		if(flag == SET_DOMAIN)
			printf("Enter domain id\t\t");
		else if(flag == SET_CLUSTER)
			printf("Enter cluster id\t\t");
		if(!gets(buf))
			return 0;
		id = atoi(buf);
		do_dom_clus(mod, id, flag);
	}
		
	printf("Reset system to take effect\n");
	return 0;
		
}

int do_dom_clus(uint mod, uint value, int flag)
{
	nasid_t nasid;
	elsc_t  remote_elsc;
	int rc = -1;
	
	nasid = module_nasid(mod, &nasid);
	if (nasid == INVALID_NASID) {
		printf("ERROR: Can't access MSC of module %d\n", mod);
		return -1;
	}
	if(!SN00) {
		elsc_init(&remote_elsc, nasid);
		if(flag == SET_DOMAIN) 
			rc = elsc_domain_set(&remote_elsc, value);
		else if(flag == SET_CLUSTER) 
			rc = elsc_cluster_set(&remote_elsc, value);
		return rc;
	}
	else {
		printf("This command does not work with Origin200\n");
		return -1;
	}
}

static int get_domain_cluster(int flag)
{
	int 	i, module, value;
	nasid_t	nasid;
	elsc_t	remote_elsc;
	uint64_t bmap = 0;
	cnodeid_t	cnode;
	gda_t		*gdap = (gda_t *) GDA_ADDR(get_nasid());

	if(SN00)
		return 0;
	for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode++) {
		lboard_t	*brd;
		

		if ((nasid = gdap->g_nasidtable[cnode]) == INVALID_NASID)
			continue;

		if (!(brd = find_lboard((lboard_t *) KL_CONFIG_INFO(
			nasid), KLTYPE_IP27)))
			continue;

		module = brd->brd_module;
		if(!((bmap >> module) & 0x1)) {
			printf("Module %d\t\t", module);
			elsc_init(&remote_elsc, nasid);
			if(flag == SET_DOMAIN) {
				value = elsc_domain_get(&remote_elsc);
				printf("Domain-id %d\n", value);
			}
			else if(flag == SET_CLUSTER) {
				value = elsc_cluster_get(&remote_elsc);
				printf("Cluster-id %d\n", value);
			}
			bmap |= ((uint64_t)0x1 << module);
		}
	}

}

		
#endif
