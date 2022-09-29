
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <uif.h>

int b2bdiaga(long, long);
int b2bdiagas(long, long);
int b2bdiagb(long, long);
int b2bdiagc(long, long);
int b2bdiagd(long, long);
int b2bdiage(long, long);
int b2bdiagf(long, long);
int b2bdiagg(long, long);
int b2bdiagh(long, long);

#define MAXTESTS 9

static struct {
	int (*func)(long,long);
	char *desc;
} tests[MAXTESTS] = {
	b2bdiaga, "sw mem, sw gio, lw mc",
	b2bdiagas, "sw mem, sw gio, sync, lw mc",
	b2bdiagg, "sw mem, sw gio, lw prom",
	b2bdiagb, "sw mem, sw gio, lw mem",
	b2bdiagc, "cache wb, sw gio, lw mc",
	b2bdiagd, "sw gio, sw gio, lw mc",
	b2bdiage, "sw mem, sw mem, lw mc",
	b2bdiagf, "sw mem, sw gio, cache ld",
	/* XXX time above to be back to back */
	b2bdiagh, "sw mem, sw gio, sw mem",
	/* XXX cache wb, sw gio, cache ld */
};

/*ARGSUSED*/
int
b2b_test(int argc, char **argv)
{
	int tmp, rc = 0;
	int i,max;

	if (argc == 2) {
		i = atoi(argv[1]);
		max = i + 1;

		if (i >= MAXTESTS)
			return 1;
	}
	else {
		i = 0;
		max = MAXTESTS;
	}

	while (i < max) {
		msg_printf(VRB,"b2bdiag%d: %s\n",i,tests[i].desc);
		tmp = (*tests[i].func)(PHYS_TO_K1_RAM(0x0c00000),
			PHYS_TO_K1(0x1fbe0064));
		msg_printf(VRB,"b2bdiag%d: rc=%d %s\n", i, tmp,
			tmp ? "failed" : "passed");
		rc += tmp;

		i++;
	}

	return rc;
}
