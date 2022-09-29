#ident "$Revision: 1.5 $"

#include <sys/sbd.h>
#include <sys/cpu.h>
#include "libsc.h"
#include "uif.h"
#include "./cache.h"

extern int test_dcache_tags(void), test_dcache_ram(void);

/*ARGSUSED*/
int
dcache(int argc, char *argv[])
{
	int tag, store;

	msg_printf(VRB, "Dcache test (dcache1.0)\n");

	flush_cache();

	tag = test_dcache_tags();
	if (tag)
		msg_printf(VRB, "DCache TAG test FAILED (code=0x%d)\n",tag);
	else
		msg_printf(VRB, "DCache TAG test passed\n");

	store = test_dcache_ram();
	if (store)
		msg_printf(VRB, "DCache RAM test FAILED (K1=%d, K0=%d)\n",
				(store&0xfff000)>>12,store&0xfff);
	else
		msg_printf(VRB, "DCache RAM test passed\n");

	return (tag+store);
}
