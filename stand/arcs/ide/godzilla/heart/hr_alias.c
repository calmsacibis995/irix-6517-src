
#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>

#ifdef HEART1_ALIAS_WINDOW_WAR
#error do not know how to handle HEART1_ALIAS_WINDOW_WAR!
#endif

static struct tornes {
	__psunsigned_t addr;
	__psunsigned_t end;
	int heart_rev;
	int fault;			/* 1 == fault */
	int stride;
#define SCL	128
#define SPG	16384
} tornes[] = {
	{ PHYS_TO_K0(0),		PHYS_TO_K0(0x0000004000),   2, 0, SCL},
	{ PHYS_TO_K0(0x0020000000),	PHYS_TO_K0(0x0020004000),   2, 1, SCL},
	{ PHYS_TO_K0(0x0000004000),	PHYS_TO_K0(0x000ff00000),   1, 1, SCL},
	{ PHYS_TO_K0(0x000ff00000),	PHYS_TO_K0(0x001fc00000),   1, 1, SPG},
	{ 0x980000001fc00000,		0x9800000020000000,   	    1, 0, SPG},
	{ PHYS_TO_K0(0x0020004000),	0x0020000000,		    1, 0, SPG},
	{ 0x0020000000,			PHYS_TO_K0(0x0400000000),   1, 1, SPG},
	{ PHYS_TO_K0(0x0400000000),	0x0400000000, 		  8|2, 0, SPG},
	{ 0x0400000000,			PHYS_TO_K0(0x0420000000), 8|2, 1, SPG},
	{ PHYS_TO_K0(0x0420000000),	0x0420000000,		  8|2, 0, SPG},
	{ 0x0420000000,			PHYS_TO_K0(0x0800000000), 8|2, 1, SPG},
	{ PHYS_TO_K0(0x0400000000),	PHYS_TO_K0(0x0800000000),   3, 1, SPG},
	{ PHYS_TO_K0(0x0800000000),	PHYS_TO_K0(0x10000000000),  1, 1, SPG},
	{ 0,0,0,0,0}
};

int
heart_mem(int argc, char **argv)
{
	volatile unsigned char *addr;
	volatile unsigned char *end;
	volatile unsigned char *nextmsg;
	int rev = heart_rev();
	int rc = 0;
	int i;

	msg_printf(VRB, "Heart memory map test.\n");

	for (i = 0; tornes[i].addr || tornes[i].end; i++) {
		if (rev < tornes[i].heart_rev) {
			if (!((tornes[i].heart_rev & 8) &&
			    (rev == (tornes[i].heart_rev&7)))) {
				msg_printf(VRB,
					"*** Skipping 0x%x to 0x%x, expect %s\n"
					"*** with rev %c heart "
					"[requires rev %c].\n",
					tornes[i].addr,
					tornes[i].end,
					tornes[i].fault ? "a fault":"no error",
					'A' + rev - 1,
					'A' + (tornes[i].heart_rev&7) - 1);
				continue;
			}
		}
		flush_cache();
		addr = (volatile unsigned char *)tornes[i].addr;
		end = (volatile unsigned char *)tornes[i].end;
		if (IS_KUSEG(addr))
			addr = (volatile unsigned char *)
				(K0BASE + addr + cpu_get_memsize());
		if (IS_KUSEG(end))
			end = (volatile unsigned char *)
				(K0BASE + end + cpu_get_memsize());
		nextmsg = addr;
		msg_printf(VRB,"Testing 0x%x to 0x%x, expect %s.\n",
				addr,end,
				tornes[i].fault ? "a fault":"no error");
		while (addr < end) {
			if (addr >= nextmsg) {
				busy(1);
				nextmsg = addr + 0x1000000;
			}
			if (badaddr(addr,1) != tornes[i].fault) {
				msg_printf(ERR,"Impropper response @ 0x%x.  "
					"Expected %s.\n",
					addr,
					tornes[i].fault ? "a fault":"no error");
				rc++;
				addr += 0x100000;	/* skip a MB */
				continue;
			}
			addr += tornes[i].stride;
		}
		busy(0);
	}

	msg_printf(VRB,"Map test complete, %d errors\n",rc);

	return rc;
}
