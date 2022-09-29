#include "mediaclient.h"		/* See?  No other headers needed! */

#include <arpa/inet.h>
#include <assert.h>
#include <bstring.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

static char barndoor[] = "Closure barn door!";

static mc_port_t the_port;
int done;
int forced_unmounts = 0;

static void run_async(int verbose, const struct in_addr *hostaddr);
static void run_sync(int verbose, const struct in_addr *hostaddr);

static void print_system_state(const mc_system_t *);
static void print_event(const mc_event_t *);
static void receive_event(mc_port_t, mc_closure_t, const mc_event_t *);

static const char *event_type_name(mc_event_type_t);
static const char *what_name(mc_what_t);
static const char *failure_code_name(mc_failure_code_t);

static void
usage(void)
{
    fprintf(stderr, "use: testclient [-a] [-f] [-v] [hostname]\n");
    exit(1);
}

main(int argc, char *argv[])
{
    int c;
    int be_async = 0;
    int be_verbose = 0;
    struct in_addr hostaddr = { INADDR_LOOPBACK };

    while ((c = getopt(argc, argv, "afv")) != -1)
	switch (c)
	{
	case 'a':
	    be_async = 1;
	    break;

	case 'f':
	    forced_unmounts = 1;
	    break;

	case 'v':
	    be_verbose = 1;
	    break;

	default:
	    usage();
	}

    if (optind < argc - 1)
	usage();
    if (optind < argc)
    {
	struct hostent *hp = gethostbyname(argv[optind]);
	if (hp == NULL)
	{   fprintf(stderr, "Unknown host \"%s\"\n", argv[optind]);
	    exit(1);
	}
	hostaddr = * (struct in_addr *) hp->h_addr;
	if (be_verbose)
	    printf("hostaddr = %s\n", inet_ntoa(hostaddr));
    }

    if (be_async)
	run_async(be_verbose, &hostaddr);
    else
	run_sync(be_verbose, &hostaddr);
    return 0;
}

/* Synchronous connection mode */

static void
run_sync(int be_verbose, const struct in_addr *hostaddr)
{
    int i, fd;
    fd_set readfds;
    const mc_what_next_t *whatnext;

    the_port = mc_create_port(hostaddr->s_addr, receive_event, barndoor);
    fd = mc_port_connect(the_port);
    if (fd < 0)
    {   fprintf(stderr, "testclient: could not connect to mediad.\n");
	exit(1);
    }

    if (forced_unmounts)
	mc_forced_unmounts(the_port);

    while (!done)
    {   FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) != 1)
	{   perror("select");
	    exit(1);
	}
	whatnext = mc_execute(the_port);
	if (whatnext->mcwn_what != MC_INPUT)
	    done = 1;
    }
    mc_destroy_port(the_port);
}
    
/* Asynchronous connection mode */

static void
run_async(int be_verbose, const struct in_addr *hostaddr)
{
    const mc_what_next_t *whatnext;
    fd_set readfds, writefds;
    int rc, i;
    struct timeval sleep_interval, *tvp;

    the_port = mc_create_port(hostaddr->s_addr, receive_event, barndoor);

    if (forced_unmounts)
	mc_forced_unmounts(the_port);

    /* Asynchronous connection mode */

    while (!done)
    {
	/* Call mc_execute. */

	whatnext = mc_execute(the_port);
	assert(whatnext != NULL);
	if (be_verbose)
	    printf("mc_execute returned %s, failure code %s\n",
		   what_name(whatnext->mcwn_what),
		   failure_code_name(whatnext->mcwn_failcode));

	/* Decide what to select on. */

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	tvp = NULL;
	if (whatnext->mcwn_what == MC_TIMEOUT)
	{   tvp = &sleep_interval;
	    sleep_interval.tv_sec = whatnext->mcwn_seconds;
	    sleep_interval.tv_usec = 0;
	}
	else if (whatnext->mcwn_what == MC_INPUT)
	    FD_SET(whatnext->mcwn_fd, &readfds);
	else if (whatnext->mcwn_what == MC_OUTPUT)
	    FD_SET(whatnext->mcwn_fd, &writefds);
	else
	    assert(0);			/* Mustn't happen. */
	
	/* Call select */

	rc = select(FD_SETSIZE, &readfds, &writefds, NULL, tvp);
	assert(rc == (whatnext->mcwn_what != MC_TIMEOUT));
    }

    mc_destroy_port(the_port);
}

static void
receive_event(mc_port_t port, mc_closure_t closure, const mc_event_t *event)
{
    assert(port == the_port);
    assert(closure == barndoor);
    print_event(event);
    print_system_state(mc_system_state(port));
    /* done = 1; */
}

static const char *s(int n)
{
    return n == 1 ? "" : "s";
}

static void
print_event(const mc_event_t *event)
{
    printf("EVENT\n");
    printf("\ttype = %s\n", event_type_name(event->mce_type));
    printf("\tdevice index = %d\n", event->mce_device);
    printf("\tvolume index = %d\n\n", event->mce_volume);
}

static int device_index(const mc_system_t *system, const mc_device_t *dev)
{
    if (dev == NULL)
	return -1;
    else
    {   int index = (int) (dev - system->mcs_devices);
	assert(index >= 0 && index < system->mcs_n_devices);
	assert(dev == &system->mcs_devices[index]);
	return index;
    }
}

static int part_index(const mc_system_t *system, const mc_partition_t *part)
{
    if (part == NULL)
	return -1;
    else
    {   int index = (int) (part - system->mcs_partitions);
	assert(index >= 0 && index < system->mcs_n_parts);
	assert(part == &system->mcs_partitions[index]);
	return index;
    }
}

static void
print_system_state(const mc_system_t *system)
{
    int i, j;

    if (system)
    {
	printf("SYSTEM\n");
	printf("\t%d volume%s\n",
	       system->mcs_n_volumes, s(system->mcs_n_volumes));
	printf("\t%d device%s\n",
	       system->mcs_n_devices, s(system->mcs_n_devices));
	printf("\t%d partition%s\n",
	       system->mcs_n_parts, s(system->mcs_n_parts));
	printf("\n");

	/* Iterate through the volumes, print all info. */

	for (i = 0; i < system->mcs_n_volumes; i++)
	{   mc_volume_t *vol = &system->mcs_volumes[i];
	    printf("    VOLUME %d\n", i);
	    printf("\tlabel         = \"%s\"\n", vol->mcv_label);
	    printf("\tfsname        = \"%s\"\n", vol->mcv_fsname);
	    printf("\tdir           = \"%s\"\n", vol->mcv_dir);
	    printf("\ttype          = \"%s\"\n", vol->mcv_type);
	    printf("\tsubformat     = \"%s\"\n", vol->mcv_subformat);
	    printf("\tmntopts       = \"%s\"\n", vol->mcv_mntopts);
	    printf("\tmounted       = %d\n", vol->mcv_mounted);
	    printf("\texported      = %d\n", vol->mcv_exported);
	    printf("\tpartition%s    = { ", s(vol->mcv_nparts));
	    for (j = 0; j < vol->mcv_nparts; j++)
		printf("%d ", part_index(system, vol->mcv_parts[j]));
	    printf("}\n");
	    printf("\n");
	}

	/* Iterate through the devices, print all info. */

	for (i = 0; i < system->mcs_n_devices; i++)
	{   mc_device_t *dev = &system->mcs_devices[i];
	    printf("    DEVICE %d\n", i);
	    printf("\tname          = \"%s\"\n", dev->mcd_name);
	    printf("\tfull name     = \"%s\"\n", dev->mcd_fullname);
	    printf("\tFTR name      = \"%s\"\n", dev->mcd_ftrname);
	    printf("\tdev name      = \"%s\"\n", dev->mcd_devname);
	    printf("\tinvent        = { ");
	    printf("class %d, ", dev->mcd_invent.inv_class);
	    printf("type %d, ", dev->mcd_invent.inv_type);
	    printf("controller %d, ",dev->mcd_invent.inv_controller);
	    printf("unit %d, ", dev->mcd_invent.inv_unit);
	    printf("state %d ", dev->mcd_invent.inv_state);
	    printf("}\n");
	    printf("\tmonitored     = %d\n", dev->mcd_monitored);
	    printf("\tmedia present = %d\n", dev->mcd_media_present);
	    printf("\twrite protect = %d\n", dev->mcd_write_protected);
	    printf("\tshared        = %d\n", dev->mcd_shared);
	    printf("\tpartition%s    = { ", s(dev->mcd_nparts));
	    for (j = 0; j < dev->mcd_nparts; j++)
		printf("%d ", part_index(system, dev->mcd_parts[j]));
	    printf("}\n");
	    printf("\n");
	}

	/* Iterate through the partitions, print all info. */

	for (i = 0; i < system->mcs_n_parts; i++)
	{   mc_partition_t *part = &system->mcs_partitions[i];
	    printf("    PARTITION %d\n", i);
	    printf("\tdevice        = %d\n",
		   device_index(system, part->mcp_device));
	    printf("\tformat        = %s\n", part->mcp_format);
	    if (part->mcp_index == MC_IX_WHOLE)
		printf("\tindex         = WHOLE DISK\n");
	    else
		printf("\tindex         = %d\n", part->mcp_index);
	    printf("\tsector size   = %u\n", part->mcp_sectorsize);
	    printf("\t1st sector    = %llu\n", part->mcp_sector0);
	    printf("\tnum sectors   = %llu\n", part->mcp_nsectors);
	    printf("\n");
	}
    }
    else
	printf("mediad is unreachable.\n");
}

static const char *
event_type_name(mc_event_type_t type)
{
    switch (type)
    {
    case MCE_NO_EVENT:
	return "MCE_NO_EVENT";
	
    case MCE_CONFIG:
	return "MCE_CONFIG";

    case MCE_INSERTION:
	return "MCE_INSERTION";
	
    case MCE_EJECTION:
	return "MCE_EJECTION";
	
    case MCE_SUSPEND:
	return "MCE_SUSPEND";
	
    case MCE_RESUME:
	return "MCE_RESUME";
	
    case MCE_MOUNT:
	return "MCE_MOUNT";
	
    case MCE_DISMOUNT:
	return "MCE_DISMOUNT";

    case MCE_FORCEUNMOUNT:
	return "MCE_FORCEUNMOUNT";

    case MCE_EXPORT:
	return "MCE_EXPORT";

    case MCE_UNEXPORT:
	return "MCE_UNEXPORT";
    }
    assert(0);
    return NULL;
}

static const char *
what_name(mc_what_t what)
{
    switch (what)
    {
    case MC_IDLE:
	return "MC_IDLE";

    case MC_INPUT:
	return "MC_INPUT";

    case MC_OUTPUT:
	return "MC_OUTPUT";

    case MC_TIMEOUT:
	return "MC_TIMEOUT";
    }
    assert(0);
    return NULL;
}

static const char *
failure_code_name(mc_failure_code_t code)
{
    switch (code)
    {
    case MCF_SUCCESS:
	return "MCF_SUCCESS";

    case MCF_SYS_ERROR:
	return "MCF_SYS_ERROR";

    case MCF_NO_HOST:
	return "MCF_NO_HOST";

    case MCF_NO_MEDIAD:
	return "MCF_NO_MEDIAD";

    case MCF_INPUT_ERROR:
	return "MCF_INPUT_ERROR";
    }
    assert(0);
    return NULL;
}
