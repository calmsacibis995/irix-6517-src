#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/SN/agent.h>

#include "net.h"
#include "router.h"
#include "hub.h"
#include "discover.h"

__uint64_t rtc_time(void)
{
    static __uint64_t tm = 0;
    tm += 10;
    return tm;
}

/*ARGSUSED*/
void rtc_sleep(__uint64_t usec)
{
}

/*ARGSUSED*/
char *net_errmsg(int r)
{
    return "net_errmsg\n";
}

static void my_gets(char *buf)
{
    fflush(stdout);

    do {
	if (fgets(buf, 256, stdin) == 0)
	    exit(1);
	if (buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = 0;
    } while (buf[0] == 0 || buf[0] == '#');

    if (! isatty(0))
	puts(buf);
}

int
net_link_up(void)
{
    char	buf[256];

    printf("Link status: ");
    my_gets(buf);

    return (int) strtoull(buf, 0, 0);
}

/*ARGSUSED*/
int
vector_write(net_vec_t dest,
	      int write_id, int address,
	      ulong value)
{
    printf("Vector write %x %x: %x\n", dest, address, value);
    return 0;
}

/*ARGSUSED*/
int
vector_read(net_vec_t dest,
	     int write_id, int address,
	     ulong *value)
{
    char	buf[256];

    printf("Vector read %x %x: ", dest, address);
    my_gets(buf);

    *value = strtoull(buf, 0, 16);

    return 0;
}

int vector_length(net_vec_t vec)
{
    int		len;

    for (len = 0; vec & 0xfULL << 4 * len; len++)
	;

    return len;
}

net_vec_t vector_get(net_vec_t vec, int n)
{
    return vec >> n * 4 & 0xfULL;
}

net_vec_t vector_prefix(net_vec_t vec, int n)
{
    return n ? vec & ~0ULL >> 64 - 4 * n : 0;
}

net_vec_t vector_modify(net_vec_t entry,
			 int n, int route)
{
    return entry & ~0xfULL << 4 * n | (net_vec_t) route << 4 * n;
}

net_vec_t vector_reverse(net_vec_t vector)
{
    net_vec_t	result;

    for (result = 0; vector; vector >>= 4)
	result = result << 4 | vector & 0xfULL;

    return result;
}

net_vec_t
vector_concat(net_vec_t vec1, net_vec_t vec2)
{
    return vec1 | vec2 << 4 * vector_length(vec1);
}

/*ARGSUSED*/
int
router_lock(net_vec_t dest,
	    int poll_usec,	/* Lock poll period */
	    int timeout_usec)	/* 0 for single try */
{
    printf("Router %x locked\n", dest);
    return 0;
}

int
router_unlock(net_vec_t dest)
{
    printf("Router %x unlocked\n", dest);
    return 0;
}

int
router_nic_get(net_vec_t dest, nic_t *nic)
{
    char	buf[256];

    printf("Router nic get %x: ", dest);
    my_gets(buf);
    *nic = strtoull(buf, 0, 16);

    return 0;
}

nic_t
get_hub_nic(void)
{
    char	buf[256];

    printf("Get hub nic: ");
    my_gets(buf);
    return strtoull(buf, 0, 16);
}

#define PSIZE		1000000

void main(int argc, char **argv)
{
    pcfg_t     *pc;
    nic_t	my_nic;

    pc		= (pcfg_t *) malloc(PSIZE);

    pc->magic	= PCFG_MAGIC;
    pc->alloc	= ((PSIZE - sizeof (pcfg_t)) / sizeof (pcfg_ent_t)) + 1;
    pc->gmaster	= 0;

    my_nic = get_hub_nic();

    discover(pc, my_nic);

    printf("%d\n", pc->count);

    discover_dump_promcfg(pc);

    if (argc == 3 ){
	int		src;
	net_vec_t	vec;

	src = (int) strtoull(argv[1], 0, 0);
	vec = strtoull(argv[2], 0, 0);

	printf("%d\n", discover_follow(pc, src, vec));
    }

    if (argc == 4) {
	int		src, dst, alt;
	net_vec_t	vec;

	src = (int) strtoull(argv[1], 0, 0);
	dst = (int) strtoull(argv[2], 0, 0);
	alt = (int) strtoull(argv[3], 0, 0);

	vec = discover_route(pc, src, dst, alt);

	printf("Vector: 0x%016llx\n", vec);

	vec = discover_return(pc, src, vec);

	printf("Retvec: 0x%016llx\n");
    }

    if (argc == 5) {
	int		src_hub, src_hub2, dst_hub;
	net_vec_t	vec, vec2, retvec;
	nasid_t		src_nasid, dst_nasid;

	src_hub	  = (int) strtoull(argv[1], 0, 0);
	vec	  = strtoull(argv[2], 0, 0);
	src_nasid = (nasid_t) strtoull(argv[3], 0, 0);
	dst_nasid = (nasid_t) strtoull(argv[4], 0, 0);

	printf("*** Programming forward path\n");

	discover_program(pc, src_hub, dst_nasid, vec, &retvec, &dst_hub);

	printf("*** Programming reverse path\n");

	discover_program(pc, dst_hub, src_nasid, retvec, &vec2, &src_hub2);

	printf("*** Dumping router tables\n");

	discover_dump_routing(pc);
    }

    exit(0);
}
