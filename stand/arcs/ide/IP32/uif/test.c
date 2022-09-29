#ident	"IP20diags/uif/test.c:  $Revision: 1.1 $"

/*
 * test.c - diagnostic command parser
 */

#if IP20
/* 
 * This file provides a basic interface to any
 * diagnostics that will be in the prom, so they can
 * be run after booting the prom
 */

#include "sys/param.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsc.h"
#include "libsk.h"
#include "uif.h"

/* test wrappers which parse and implement common options */
static int	memtest(/* argc, argv */);
static int	delaytest(/* argc, argv */);

/* imported test functions */
extern bool_t memaddruniq();
extern bool_t memwalkingbit();
extern bool_t memparity();

static struct cmd_table toplevel[] = {
	{ "addr", memtest,
	       "address uniqueness:	addr [-wiuf] RANGE ..." },
	{ "data", memtest,
	       "walking bit:		data [-bhwiuf] RANGE ..." },
	{ "parity", memtest,
	       "parity checking:	parity [-bhwiuf] [RANGE] ..." },
	{ "delay", delaytest,
		"test delay function:	delay USEC ..." },
	{ "help", help,
	       "test help:		help [COMMAND ...]" },
	{ 0, }
};

int
test(argc, argv, envp, parentct)
	register int argc;
	register char **argv;
	register char **envp;
	register struct cmd_table *parentct;
{
	extern int *Reportlevel;
	register struct cmd_table *ct;

	*Reportlevel = DBG;
	if (argc == 1) {
		command_parser(toplevel, "test>> ", 0, parentct);
		return 0;
	}
	--argc, argv++;
	ct = lookup_cmd(toplevel, *argv);
	if (ct == 0) {
		return help(1, argv, envp, toplevel);
	}
	if ((*ct->ct_routine)(argc, argv, envp, toplevel) < 0)
		usage(ct);
	return 0;
}

/* XXX this is kind of coupled to PROM_STACK in IP20.h.  BSS and the
 *     stack for the prom are below 4Mb and any memory above 4Mb is
 *     not really saved for anything
 */
#define	MEMBASE_DEFAULT	0x400000

enum loopmode { ONCE, UNTIL_ERROR, FOREVER };

static int
delaytest(argc, argv)
	int argc;
	char **argv;
{
#ifdef	NOTDEF
	printf("cycles/instruction = %d\n", _cyclesperinst());
	printf("ticks/1024inst = %d\n", _ticksper1024inst());
#endif	/* NOTDEF */
	while (--argc > 0) {
		auto int usec;
		if (*atob(*++argv, &usec))
			return -1;
		printf("About to delay ~%d microseconds...", usec);
		DELAY(usec);
		printf(" done.\n");
	}
	return 0;
}

static int
memtest(argc, argv)
	register int argc;
	register char **argv;
{
	register enum loopmode mode;
	register enum bitsense sense;
	register int size;
	struct range range;
	register bool_t (*testit)();
	register char *tname;

	if (argc < 2 || argc > 3)
		return -1;
	tname = *argv;
	mode = ONCE;
	sense = BIT_TRUE;
	size = sizeof(int);
	while (--argc > 0 && (*++argv)[0] == '-') {
		register char *ap;

		for (ap = *argv + 1; *ap != '\0'; ap++)
			switch (*ap) {
			  case 'b':
				size = sizeof(char);
				break;
			  case 'h':
				size = sizeof(short);
				break;
			  case 'w':
				size = sizeof(int);
				break;
			  case 'i':
				sense = BIT_INVERT;
				break;
			  case 'u':
				mode = UNTIL_ERROR;
				break;
			  case 'f':
				mode = FOREVER;
				break;
			  default:
				return -1;
			}
	}
	if (argc == 0)
		return -1;
	switch (tname[0]) {
	  case 'a':	/* addr */
		if (size != sizeof(int))
			return -1;
		testit = memaddruniq;
		break;
	  case 'd':	/* data */
		testit = memwalkingbit;
		break;
	  case 'p':
		testit = memparity;
		break;
	}
	while (--argc >= 0) {
		if (!parse_range(*argv, size, &range)) {
			if (strcmp(atob(*argv, &range.ra_count), "meg") != 0)
				return -1;
			range.ra_base = PHYS_TO_K1(MEMBASE_DEFAULT);
			/* what a botch - sizeof is unsigned in MIPS-land */
			range.ra_count =
			    ((range.ra_count << 20) - MEMBASE_DEFAULT)
				/ (signed int) sizeof(int);
			range.ra_size = sizeof(int);
		}
		argv++;
		switch (mode) {
		  case ONCE:
			(void)(*testit)(&range, sense, RUN_UNTIL_DONE, memerr);
			break;
		  case UNTIL_ERROR:
			while ((*testit)(&range, sense, RUN_UNTIL_ERROR,
				memerr)) {
				/*
				 * The following allows interrupts from the
				 * keyboard to be serviced.
				 */
				printf(".");
			}
			break;
		  case FOREVER: {
			register void (*errfun)() = memerr;

			/*
			 * A bad cc -O2 bug will bite this code: because
			 * there is no exit from this block, the optimizer
			 * gets confused and saves regular and fp regs on
			 * the stack.  Not so good without an FPU...
			 */
			for (;;) {
				(void)(*testit)(&range, sense, RUN_UNTIL_DONE,
				    errfun);
				errfun = 0;
			}
		  }
		}
	}
	return 0;
}
#endif	/* IP20 */
