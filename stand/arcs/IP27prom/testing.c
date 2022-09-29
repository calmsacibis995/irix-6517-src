#if defined(RTL) || defined(GATE)

#ident "$Revision: 1.32 $"
#define TRACE_FILE_NO		10

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/SN0/IP27.h>

#include "ip27prom.h"
#include "router.h"
#include "fprom.h"
#include "mdir.h"
#include "libc.h"
#include "hub.h"
#include "libasm.h"
#include "rtc.h"

/*
 * TESTADDR: a physical address having pre-initialized mem/dir on all nodes.
 */

#define TESTADDR		0x80000

#define VECTOR0HOP()		(0)
#define VECTOR1HOP(t1)		(t1)
#define VECTOR2HOP(t1, t2)	((t2) << 4 | (t1))
#define VECTOR3HOP(t1, t2, t3)	((t3) << 8 | (t2) << 4 | (t1))

#define PI_RT_COUNTER_FREQ	5000000

#define USEC(usec)		((ulong) (usec) * \
				 PI_RT_COUNTER_FREQ / 1000000)

#define TIME()			LD(LOCAL_HUB(PI_RT_COUNT))

static void check(char *what, int rc)
{
    if (rc < 0) {
	TRACE(rc);
#ifndef RTL
	printf("%s failed: %s\n", what, net_errmsg(rc));
#endif
    }
}

/*
 * OLD 4node_2rtr configuration:
 *
 *        +----+
 *        |h001|
 *        +----+\  __         __
 *               \/  \       /  \
 *               /5  6\     /2  3\
 *     +----+   |      |   |      |   +----+
 *     |h000|---|4 r0 1|---|1 r1 4|---|h002|
 *     +----+   |      |   |      |   +----+
 *               \3  2/     \6  5/
 *                \__/       \__/\
 *                                \+----+
 *                                 |h003|
 *                                 +----+
 *
 * NEW 4node_2rtr configuration:
 *
 *        +----+        +----+        +----+
 *        |h001|        |h002|        |h003|
 *        +----+\  __   +----+\  __  /+----+
 *               \/  \         \/  \/
 *               /5  6\        /2  3\
 *     +----+   |      |      |      |
 *     |h000|---|4 r0 1|------|1 r1 4|
 *     +----+   |      |      |      |
 *               \3  2/        \6  5/
 *                \__/          \__/
 *
 *
 * 2node_1rtr_cosim configuration:
 *
 *        +----+
 *        |h001|
 *        +----+\  __
 *               \/  \
 *               /5  6\
 *     +----+   |      |
 *     |h000|---|4 r0 1|
 *     +----+   |      |
 *               \3  2/
 *                \__/
 *
 *
 * TWOHUB configuration (2node_cosim)
 *
 *        +----+     +----+
 *        |h000|-----|h001|
 *        +----+     +----+
 */

static ulong routes_4node[4][4] = { /* [src_node][dst_node] */
    0,		      VECTOR1HOP(5),	VECTOR2HOP(1, 2), VECTOR2HOP(1, 3),
    VECTOR1HOP(4),    0,		VECTOR2HOP(1, 2), VECTOR2HOP(1, 3),
    VECTOR2HOP(1, 4), VECTOR2HOP(1, 5), 0,		  VECTOR1HOP(3),
    VECTOR2HOP(1, 4), VECTOR2HOP(1, 5), VECTOR1HOP(2),	  0
};

static void test_wr(ulong addr, ulong value)
{
    ulong	stime, etime, result;

#ifndef RTL
    printf("test_wr(0x%lx, 0x%lx)\n", addr, value);
#endif

    TRACE(addr);
    TRACE(value);

#define sync_instr()

    stime = TIME();
    sync_instr();
    SD(addr, value);
    sync_instr();
    result = LD(addr);
    sync_instr();
    etime = TIME();

    if (result != value) {
	TRACE(0xdeadbeef);
	TRACE(result);
#ifndef RTL
	printf("Oops 0x%lx\n", result);
#endif
    } else {
	TRACE(etime - stime);
#ifndef RTL
	printf("%ld\n", etime - stime);
#endif
    }
}

/*
 * Wait for SCRATCH_REG1 at the specified vector route to become non-zero.
 */

static void wait_for_node(net_vec_t dest)
{
    ulong	value;
    int		r;

#ifndef RTL
    printf("Wait on 0x%lx\n", dest);
#endif

    TRACE(dest);

    do {
	value = 0;		/* In case vector read fails */
	check("Vector read wait",
	      vector_read(dest, 0, NI_SCRATCH_REG1, &value));
    } while (value == 0);

#ifndef RTL
    printf("Cont\n");
#endif

    TRACE(value);
}

void net_vec_test(int node)
{
    ulong	status;
    ulong	result;
    int		r;

    switch (node) {
    case 0:
	TRACE(0);

	/******************************************/
	/* VECTOR OP TO UNCONNECTED ROUTER PORT   */

	check("Vector read U",
	      vector_read(VECTOR2HOP(2, 2), 0,
			   RR_STATUS_REV_ID, &result));

	TRACE(0);

	check("Vector write ok",
	      vector_write(VECTOR0HOP(), 0,
			    RR_LOCAL_TABLE(3), 0777772L));

	TRACE(0);

	check("Vector write U",
	      vector_write(VECTOR2HOP(2, 2), 0,
			    RR_SCRATCH_REG0, 0xcafecafecafecafe));

	TRACE(0);

	/******************************************/

	break;

    case 1:
	/*
	 * Check to see that attached device is a router by reading
	 * its STATUS_REV_ID and checking the device type.
	 */

	check("Vector read 1",
	      vector_read(VECTOR0HOP(), 0,
			   RR_STATUS_REV_ID, &status));

	TRACE(status);

	if ((status & RSRI_CHIPID_MASK) != CHIPID_ROUTER) {
	    TRACE("0xdeadbeef\n");
#ifndef RTL
	    printf("Not on a router??\n");
#endif
	}

	/*
	 * Try to lock and unlock the router (vector exchange)
	 */

	TRACE(0);

	r = router_lock(VECTOR0HOP(), 0, 1000000);

	TRACE(0);

	if (r < 0) {
	    TRACE(0);
	    TRACE("0xdeadbeef\n");
#ifndef RTL
	    printf("Lock failed: %d\n", r);
#endif
	}

	TRACE(0);

	r = router_unlock(VECTOR0HOP());

	if (r < 0) {
	    TRACE("0xdeadbeaf\n");
#ifndef RTL
	    printf("Unlock failed: %d\n", r);
#endif
	}

	break;

    case 2:
	/*
	 * Write something to scratch register 1 in router.
	 * Read it back and compare.
	 */

	check("Vector write RR scratch",
	      vector_write(VECTOR0HOP(), 0,
			    RR_SCRATCH_REG0, 0x0feed0beef0cafe0));

	TRACE(0);

	check("Vector read RR scratch",
	      vector_read(VECTOR0HOP(), 0,
			   RR_SCRATCH_REG0, &result));

	if (result != 0x0feed0beef0cafe0) {
	    TRACE(0xdeadbeef);
	    TRACE(result);
#ifndef RTL
	    printf("Oops, result=0x%lx\n", result);
#endif
	}

	break;

    case 3:
	/*
	 * Write something to scratch register 0 in node 0 (across both routers).
	 * Read it back and compare.
	 */

	check("Vector write scratch",
	      vector_write(VECTOR2HOP(1, 4), 0,
			    NI_SCRATCH_REG0, 0x00decade00facade));

	TRACE(0);

	check("Vector read scratch",
	      vector_read(VECTOR2HOP(1, 4), 0,
			   NI_SCRATCH_REG0, &result));

	if (result != 0x00decade00facade) {
	    TRACE(0xdeadbeef);
	    TRACE(result);
#ifndef RTL
	    printf("Oops, result=0x%lx\n", result);
#endif
	}

	break;
    }
}

void access_test(int node)
{
    switch (node) {
    case 0:
	test_wr(CALIAS_BASE	   + TESTADDR, 0x0001000100010001);
	test_wr(NODE_CAC_BASE(0)   + TESTADDR, 0x0002000200020002);
	test_wr(NODE_CAC_BASE(1)   + TESTADDR, 0x0003000300030003);
	test_wr(NODE_CAC_BASE(2)   + TESTADDR, 0x0004000400040004);
	test_wr(NODE_CAC_BASE(3)   + TESTADDR, 0x0005000500050005);
	test_wr(NODE_UNCAC_BASE(0) + TESTADDR, 0x0006000600060006);
	test_wr(NODE_UNCAC_BASE(1) + TESTADDR, 0x0007000700070007);
	test_wr(NODE_UNCAC_BASE(2) + TESTADDR, 0x0008000800080008);
	test_wr(NODE_UNCAC_BASE(3) + TESTADDR, 0x0009000900090009);
	break;
    case 1:
	test_wr(CALIAS_BASE	   + TESTADDR, 0x0010001000100010);
	test_wr(NODE_CAC_BASE(0)   + TESTADDR, 0x0012001200120012);
	test_wr(NODE_CAC_BASE(1)   + TESTADDR, 0x0013001300130013);
	test_wr(NODE_CAC_BASE(2)   + TESTADDR, 0x0014001400140014);
	test_wr(NODE_CAC_BASE(3)   + TESTADDR, 0x0015001500150015);
	test_wr(NODE_UNCAC_BASE(0) + TESTADDR, 0x0016001600160016);
	test_wr(NODE_UNCAC_BASE(1) + TESTADDR, 0x0017001700170017);
	test_wr(NODE_UNCAC_BASE(2) + TESTADDR, 0x0018001800180018);
	test_wr(NODE_UNCAC_BASE(3) + TESTADDR, 0x0019001900190019);
	break;
    case 2:
	test_wr(CALIAS_BASE	   + TESTADDR, 0x0020002000200020);
	test_wr(NODE_CAC_BASE(0)   + TESTADDR, 0x0022002200220022);
	test_wr(NODE_CAC_BASE(1)   + TESTADDR, 0x0023002300230023);
	test_wr(NODE_CAC_BASE(2)   + TESTADDR, 0x0024002400240024);
	test_wr(NODE_CAC_BASE(3)   + TESTADDR, 0x0025002500250025);
	test_wr(NODE_UNCAC_BASE(0) + TESTADDR, 0x0026002600260026);
	test_wr(NODE_UNCAC_BASE(1) + TESTADDR, 0x0027002700270027);
	test_wr(NODE_UNCAC_BASE(2) + TESTADDR, 0x0028002800280028);
	test_wr(NODE_UNCAC_BASE(3) + TESTADDR, 0x0029002900290029);
	break;
    case 3:
	test_wr(CALIAS_BASE	   + TESTADDR, 0x0030003000300030);
	test_wr(NODE_CAC_BASE(0)   + TESTADDR, 0x0032003200320032);
	test_wr(NODE_CAC_BASE(1)   + TESTADDR, 0x0033003300330033);
	test_wr(NODE_CAC_BASE(2)   + TESTADDR, 0x0034003400340034);
	test_wr(NODE_CAC_BASE(3)   + TESTADDR, 0x0035003500350035);
	test_wr(NODE_UNCAC_BASE(0) + TESTADDR, 0x0036003600360036);
	test_wr(NODE_UNCAC_BASE(1) + TESTADDR, 0x0037003700370037);
	test_wr(NODE_UNCAC_BASE(2) + TESTADDR, 0x0038003800380038);
	test_wr(NODE_UNCAC_BASE(3) + TESTADDR, 0x0039003900390039);
	break;
    }
}

void stress_test(int node)
{
    ulong	t0, t1, t2, t3;
    ulong	a, m;
    int		i, r;

    t0 = NODE_CAC_BASE(0) + TESTADDR + 64 + 8 * node;
    t1 = NODE_CAC_BASE(1) + TESTADDR + 64 + 8 * node;
    t2 = NODE_CAC_BASE(2) + TESTADDR + 64 + 8 * node;
    t3 = NODE_CAC_BASE(3) + TESTADDR + 64 + 8 * node;

    /*
     * Write and verify 4 meta table entries in each of the 4 other nodes.
     * Simultaneously does a lot of memory activity with cache lines
     * pinging around.
     */

    for (i = 0; i < 16; i++) {
	TRACE(i);
	if (node != (i & 3) &&
	    vector_write(routes_4node[node][i & 3], 0,
			  NI_META_TABLE(node * 4 + (i / 4)),
			  5 * (i & 3) + (i / 4)) < 0)
	    TRACE(0);
	a = LD(t0);
	if (a != (ulong) i)
	    TRACE(a);
	SD(t0, a + 1);
	a = LD(t1);
	if (a != (ulong) i)
	    TRACE(a);
	SD(t1, a + 1);
	a = LD(t2);
	if (a != (ulong) i)
	    TRACE(a);
	SD(t2, a + 1);
	a = LD(t3);
	if (a != (ulong) i)
	    TRACE(a);
	SD(t3, a + 1);
	if (node != (i & 3) &&
	    vector_read(routes_4node[node][i & 3], 0,
			 NI_META_TABLE(node * 4 + (i / 4)),
			 &m) < 0)
	    TRACE(0);
	if (m != 5 * (i & 3) + (i / 4))
	    TRACE(m);
	a = LD(t0);
	if (a != (ulong) i)
	    TRACE(a);
	a = LD(t1);
	if (a != (ulong) i)
	    TRACE(a);
	a = LD(t2);
	if (a != (ulong) i)
	    TRACE(a);
	a = LD(t3);
	if (a != (ulong) i)
	    TRACE(a);
	SD(t0, a + 1);
	SD(t1, a + 1);
	SD(t2, a + 1);
	SD(t3, a + 1);
    }

    TRACE(0);
}

/*
 * Tests for 2newt1hub1rtr model
 */

static void node0x(void)
{
    ulong	status;
    int		i1, i2;

    TRACE(0);

    check("Vector write scratch 0 with 0",
	  vector_write(VECTOR0HOP(), 0, RR_SCRATCH_REG0, 0));

    TRACE(0);

    status = 0x5555555555555555;

    check("Vector exchange scratch 0 with 5's",
	  vector_exch(VECTOR0HOP(), 0, RR_SCRATCH_REG0, &status));

    TRACE(status);

    status = 0xaaaaaaaaaaaaaaaa;

    check("Vector exchange scratch 0 with a's",
	  vector_exch(VECTOR0HOP(), 0, RR_SCRATCH_REG0, &status));

    TRACE(status);

    check("Vector write scratch 1 with 0",
	  vector_write(VECTOR0HOP(), 0, RR_SCRATCH_REG1, 0));

    status = 0x1111111111111111;

    check("Vector exchange scratch 1 with 1's",
	  vector_exch(VECTOR0HOP(), 0, RR_SCRATCH_REG1, &status));

    TRACE(status);

    status = 0x7777777777777777;

    check("Vector exchange scratch 1 with 7's",
	  vector_exch(VECTOR0HOP(), 0, RR_SCRATCH_REG1, &status));

    TRACE(status);
}

/*
 * 2node_cosim tests
 */

static void two_hub_0(void)
{
    SD(LOCAL_HUB(NI_LOCAL_TABLE(1)), 0);

    /* NB: node ID is already 0 and we're not changing it, so this is okay */

    SD(LOCAL_HUB(NI_STATUS_REV_ID),
       MORE_MEMORY << NSRI_MORENODES_SHFT |
       REGIONSIZE_FINE << NSRI_REGIONSIZE_SHFT |
       0 << NSRI_NODEID_SHFT);

    check("vw1",
	  vector_write(VECTOR0HOP(), 0, NI_LOCAL_TABLE(0), 0));

    check("vw2",
	  vector_write(VECTOR0HOP(), 0,
			NI_STATUS_REV_ID,
			MORE_MEMORY << NSRI_MORENODES_SHFT |
			REGIONSIZE_FINE << NSRI_REGIONSIZE_SHFT |
			1 << NSRI_NODEID_SHFT));

    SD(LOCAL_HUB(NI_SCRATCH_REG1), 1);

    TRACE(0);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  0, 0x0051005100510051);
    TRACE(1);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  8, 0x0052005200520052);
    TRACE(2);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR + 16, 0x0053005300530053);

    TRACE(3);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  0, 0x0061006100610061);
    TRACE(4);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  8, 0x0062006200620062);
    TRACE(5);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR + 16, 0x0063006300630063);

    TRACE(6);
}

static void two_hub_1(void)
{
    wait_for_node(0);

    LD(LOCAL_HUB(NI_STATUS_REV_ID));
    LD(LOCAL_HUB(NI_LOCAL_TABLE(0)));

    TRACE(0);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  0, 0x0071007100710071);
    TRACE(1);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  8, 0x0072007200720072);
    TRACE(2);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR + 16, 0x0073007300730073);

    TRACE(3);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  0, 0x0081008100810081);
    TRACE(4);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  8, 0x0082008200820082);
    TRACE(5);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR + 16, 0x0083008300830083);

    TRACE(6);
}

static void two_hub_0x(void)
{
    net_node_set(0);

    SD(LOCAL_HUB(NI_LOCAL_TABLE(1)), 0);

    /*
     * Synchronize with other node
     */

    SD(LOCAL_HUB(NI_SCRATCH_REG1), 1);

    while (LD(LOCAL_HUB(NI_SCRATCH_REG1)))
	;

    /*
     * Run tests
     */

    TRACE(0);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  0, 0x0051005100510051);
    TRACE(1);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  8, 0x0052005200520052);
    TRACE(2);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR + 16, 0x0053005300530053);

    TRACE(3);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  0, 0x0061006100610061);
    TRACE(4);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  8, 0x0062006200620062);
    TRACE(5);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR + 16, 0x0063006300630063);

    TRACE(6);
    test_wr(NODE_CAC_BASE(0) + TESTADDR +   0, 0x0071007100710071);
    TRACE(7);
    test_wr(NODE_CAC_BASE(0) + TESTADDR + 128, 0x0072007200720072);
    TRACE(8);
    test_wr(NODE_CAC_BASE(0) + TESTADDR + 256, 0x0073007300730073);

    TRACE(9);
    test_wr(NODE_CAC_BASE(1) + TESTADDR +   0, 0x0081008100810081);
    TRACE(10);
    test_wr(NODE_CAC_BASE(1) + TESTADDR + 128, 0x0082008200820082);
    TRACE(11);
    test_wr(NODE_CAC_BASE(1) + TESTADDR + 256, 0x0083008300830083);

    TRACE(12);
}

static void two_hub_1x(void)
{
    net_node_set(1);

    SD(LOCAL_HUB(NI_LOCAL_TABLE(0)), 0);

    /*
     * Synchronize with other node
     */

    wait_for_node(0);

    check("va",
	  vector_write(VECTOR0HOP(), 0, NI_SCRATCH_REG1, 0));

    /*
     * Run tests
     */

    TRACE(0);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  0, 0x0091009100910091);
    TRACE(1);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR +  8, 0x0092009200920092);
    TRACE(2);
    test_wr(NODE_UNCAC_BASE(1) + TESTADDR + 16, 0x0093009300930093);

    TRACE(3);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  0, 0x00a100a100a100a1);
    TRACE(4);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR +  8, 0x00a200a200a200a2);
    TRACE(5);
    test_wr(NODE_UNCAC_BASE(0) + TESTADDR + 16, 0x00a300a300a300a3);

    TRACE(6);
    test_wr(NODE_CAC_BASE(1) + TESTADDR +   0, 0x00b100b100b100b1);
    TRACE(7);
    test_wr(NODE_CAC_BASE(1) + TESTADDR + 128, 0x00b200b200b200b2);
    TRACE(8);
    test_wr(NODE_CAC_BASE(1) + TESTADDR + 256, 0x00b300b300b300b3);

    TRACE(9);
    test_wr(NODE_CAC_BASE(0) + TESTADDR +   0, 0x00c100c100c100c1);
    TRACE(10);
    test_wr(NODE_CAC_BASE(0) + TESTADDR + 128, 0x00c200c200c200c2);
    TRACE(11);
    test_wr(NODE_CAC_BASE(0) + TESTADDR + 256, 0x00c300c300c300c3);

    TRACE(12);
}

static void router_setup(int node)
{
    net_node_set(node);

    /*
     * Program the local tables
     */

    switch (node) {
    case 0:
	SD(LOCAL_HUB(NI_LOCAL_TABLE(1)), 1);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(2)), 3);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(3)), 3);
	break;
    case 1:
	SD(LOCAL_HUB(NI_LOCAL_TABLE(0)), 5);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(2)), 2);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(3)), 2);
	break;
    case 2:
	SD(LOCAL_HUB(NI_LOCAL_TABLE(0)), 5);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(1)), 5);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(3)), 1);
	break;
    case 3:
	SD(LOCAL_HUB(NI_LOCAL_TABLE(0)), 4);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(1)), 4);
	SD(LOCAL_HUB(NI_LOCAL_TABLE(2)), 5);
	break;
    }

    if (node == 0) {
	/*
	 * Program the router 0 tables
	 */

	TRACE(0);
	vector_write(VECTOR0HOP(), 0, RR_LOCAL_TABLE(2), 0777771L);
	TRACE(0);
	vector_write(VECTOR0HOP(), 0, RR_LOCAL_TABLE(3), 0777772L);

	/*
	 * Program the router 1 tables
	 */

	TRACE(0);
	vector_write(VECTOR1HOP(1), 0, RR_LOCAL_TABLE(0), 0777773L);
	TRACE(0);
	vector_write(VECTOR1HOP(1), 0, RR_LOCAL_TABLE(1), 0777774L);
    }

    /*
     * Synchronize
     */

    SD(LOCAL_HUB(NI_SCRATCH_REG1), 1);

    if (node != 0)
	wait_for_node(routes_4node[node][0]);
    if (node != 1)
	wait_for_node(routes_4node[node][1]);
    if (node != 2)
	wait_for_node(routes_4node[node][2]);
    if (node != 3)
	wait_for_node(routes_4node[node][3]);

    TRACE(0);
}

static void fprom_check(int rc)
{
    if (rc < 0)
	printf("fprom: %s\n", fprom_errmsg(rc));
}

/*
 * The 1 MB CPU prom resides at LBOOT_BASE.  Aliased copies of it fill up
 * the entire 256 MB LBOOT space.  However, in Sable the second MB only
 * (0x100000 to 0x1fffff) is occupied by the 1 MB simulated IO6 prom.
 */

static void fprom_test(void)
{
    char	buf[256];
    int		c;
    int		manu_code, dev_code;
    fprom_t	f;

    f.base   = (void *) LBOOT_BASE;
    f.dev    = FPROM_DEV_HUB;
    f.afn    = 0;
    f.aparm1 = 0;
    f.aparm2 = 0;

    /*
     * This executable code resides at the beginning of the CPU PROM,
     * so tests will take care not to trash the first half of that PROM.
     */

    printf("FPROM TEST\n");

    printf("Init CPU PROM\n");
    fprom_check(fprom_probe(&f, &manu_code, &dev_code));

#if 0
    printf("Init IO PROM\n");
    fprom_check(fprom_probe(&f, &manu_code, &dev_code));
#endif

    strcpy(buf, "OK*data");

    printf("Write string '%s', offset 0x080008\n");
    fprom_check(fprom_write(&f, 0x080008, buf, 8));

    strcpy(buf, "ok+data");

#if 0
    printf("Write string 'ok+DATA' offset 0x880000\n");
    fprom_check(fprom_write(&f, 0x880000, buf, 8));
#endif

    printf("Read offset 0 (code at beginning of PROM):\n");
    fprom_check(fprom_read(&f,	0, buf, 8));
    printf("%y ", *(ulong *) buf);
    fprom_check(fprom_read(&f,	8, buf, 8));
    printf("%y ", *(ulong *) buf);
    fprom_check(fprom_read(&f, 16, buf, 8));
    printf("%y ", *(ulong *) buf);
    fprom_check(fprom_read(&f, 24, buf, 8));
    printf("%y\n", *(ulong *) buf);

    memset(buf, ' ', 8);

    printf("Read string from offset 0x080008 (a): ");
    fprom_check(fprom_read(&f, 0x080008, buf, 8));
    printf("%s\n", buf);

#if 0
    printf("Read string from offset 0x880008 (a): ");
    fprom_check(fprom_read(&f, 0x880008, buf, 8));
    printf("%s\n", buf);	/* Wrap-around test */

    printf("Read string from offset 0x180008 (a): ");
    fprom_check(fprom_read(&f, 0x180008, buf, 8));
    printf("%s\n", buf);	/* Wrap-around test */

    printf("Read string from offset 0x980008 (a): ");
    fprom_check(fprom_read(&f, 0x980008, buf, 8));
    printf("%s\n", buf);	/* Wrap-around test */
#endif

    printf("Flash CPU PROM upper half\n");
    fprom_check(fprom_flash_sectors(&f, 0xff00));

    printf("Read offset 0x080000 (a): ");
    fprom_check(fprom_read(&f, 0x080000, buf, 8));
    printf("%y\n", *(ulong *) buf);

#if 0
    printf("Flash IO PROM upper half\n");
    fprom_check(fprom_flash_sectors(base,  0, 0,0xff00));

    printf("Read offset 0x880000 (a): ");
    fprom_check(fprom_read(&f, 0x880000, buf, 8));
    printf("%y\n", *(ulong *) buf);
#endif

    printf("END FPROM TEST\n");
}

static void mdir_test(void)
{
    mdir_init(0);
    mdir_config(0);

#if !defined(GATE) && !defined(RTL)
    printf("Memory config register: 0x%lx\n",
	   LD(LOCAL_HUB(MD_MEMORY_CONFIG)));
#endif
}

void testing(void)
{
    int		node;
    ulong	result;

    /*
     * The node ID is pre-initialized by RTL so we have some way to
     * break symmetry.  We'll read it here.  net_init will clear
     * it back to zero before the tests actually begin.
     */

    node	= net_node_get();

    TRACE(node);

#if defined(RTL) || defined(GATE)
    TRACE(0);
    LD(0xffffffffbfc00000);	/* Test double-word PROM reads */
    LD(0xffffffffbfc00060);

    rtc_init();

    LD(LOCAL_HUB(PI_RT_LOCAL_CTRL)); /* GCLK_COUNTER counting? */
    LD(LOCAL_HUB(PI_RT_COUNT));
    delay(50);
    LD(LOCAL_HUB(PI_RT_LOCAL_CTRL));
    LD(LOCAL_HUB(PI_RT_COUNT));
    delay(50);
    LD(LOCAL_HUB(PI_RT_LOCAL_CTRL));
    LD(LOCAL_HUB(PI_RT_COUNT));
    delay(50);
    LD(LOCAL_HUB(PI_RT_LOCAL_CTRL));
    LD(LOCAL_HUB(PI_RT_COUNT));
    delay(50);
    LD(LOCAL_HUB(PI_RT_LOCAL_CTRL));
    LD(LOCAL_HUB(PI_RT_COUNT));
#endif /* RTL */

#if 0				/*defined(RTL) || defined(GATE)*/
    /*
     * Show how some of the memory/directory entries are initialized
     * by the RTL or gate-level simulators.
     */

    TRACE(LD(BDPRT_ENTRY(0xa800000000080000, 0))); /* 0000000000000006 */
    TRACE(LD(BDDIR_ENTRY_LO(0xa800000000080000))); /* 000000000000e0b9 */
    TRACE(LD(BDDIR_ENTRY_HI(0xa800000000080000))); /* 0000000000000000 */
    TRACE(LD(BDECC_ENTRY(0xa800000000080000)));	/* 0000000000000000 */
    TRACE(LD(BDPRT_ENTRY(0xa800000000081240, 0))); /* 0000000000000006 */
    TRACE(LD(BDDIR_ENTRY_LO(0xa800000000081240))); /* 000000000000e0b9 */
    TRACE(LD(BDDIR_ENTRY_HI(0xa800000000081240))); /* 0000000000000000 */
    TRACE(LD(BDECC_ENTRY(0xa800000000081240)));	/* 0000000000000000 */
#endif

#if !defined(RTL) && !defined(GATE)
    TRACE(node);
    TRACE(hub_local_master());

    /*
     * Display node number and freeze second CPU on each hub.
     */

    hub_lock(HUB_LOCK_PRINT);

    printf("CPU %C: Node %d", node);

    if (! hub_local_master())
	printf(" (freezing)\n");

    printf("\n");

    TRACE(0);
    hub_unlock(HUB_LOCK_PRINT);
    TRACE(0);

    if (! hub_local_master())
	goto exit_point;

    TRACE(0);
#endif

    /*
     * Perform selected test(s)
     */

#ifdef SABLE
    fprom_test();
#endif

#ifdef GATE
    if (node == 0)
	mdir_test();
    else
	hub_int_diag();

    TRACE(0);

    hub_barrier();
#endif

#if 1
    if (node == 0)
	mdir_test();
#endif

    /*
     * If a router is connected, assume 4node_2rtr_cosim model,
     * otherwise assume 2node_cosim model (see pictures above).
     */

    while (! net_link_up())
	;

    net_init();

    vector_read(VECTOR0HOP(), 0, RR_STATUS_REV_ID, &result);

    if (GET_FIELD(result, RSRI_CHIPID) == CHIPID_HUB) {
	/* Does not return */
	TRACE(0x12345678);

	if (node)
	    two_hub_1x();
	else
	    two_hub_0x();
    }

#if 0
    net_vec_test(node);		/* Test some vector ops */
#endif

    router_setup(node);		/* Set up router tables for 4node_2rtr */

    access_test(node);		/* Write/verify each node space */
    stress_test(node);		/* Stress test */

#if 1
    net_vec_test(node);		/* Test some vector ops */
#endif

    TRACE(0x7777777777777777);

#if !defined(RTL) && !defined(GATE)
    printf("\nExit\n");
#endif

 exit_point:

    TRACE(0);

    hub_led_set(0xf0);

    halt();
}

#endif /* RTL || GATE */
