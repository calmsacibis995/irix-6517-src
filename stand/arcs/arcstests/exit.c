/*  exit.c  -- tests ARCS exit vectors.  Takes one arg to determine which
 * call to use, then sets as much memory as it can to 0xff, and tests
 * the exit entry point.
 */
#ident "$Revision: 1.2 $"

#include <arcs/spb.h>
#include <arcs/io.h>
#include <arcs/restart.h>
#include <sys/sbd.h>

struct commands {
	char *arg;
	void (*func)();
} cmds[] = {
	"Halt", Halt,
	"PowerDown", PowerDown,
	"Restart", Restart,
	"Reboot", Reboot,
	"EnterInteractiveMode", EnterInteractiveMode,
	0,0
};

static void mem_destroy(void);
static void bset(char *p, char c, int i);
extern _end[];

main(int argc, char **argv, char **envp)
{
	struct commands *p;
	void (*f)() = 0;

	for (p = cmds; p->arg ; p++) {
		if (!strcmp(p->arg, argv[1])) {
			f = p->func;
			break;
		}
	}

	if (!f) {
		printf("usage: exit Halt|PowerDown|Restart|Reboot|EnterInteractiveMode\n");
		return;
	}

	printf("Calling %s()\n",argv[1]);

	/* Kill memory */
	mem_destroy();
	(*f)();
	/*NOTREACHED*/
}

static void
mem_destroy(void)
{
	MEMORYDESCRIPTOR *d = GetMemoryDescriptor(NULL);
	int idx = 0;
	int first = 0;

	if (!d) {
		printf ("No memory descriptors available.\n");
		return;
	}

	while (d) {
		if ((d->Type == FirmwareTemporary) && (d->PageCount > 2)) {
			int i;

			if (!first) {
				printf("trash FirmwareTemporary...0x%x\n",
				       PHYS_TO_K1(ptob(d->BasePage)));
				first++;
			}

			for (i = 0; i < d->PageCount; i++)
				bset((char *)PHYS_TO_K1((ptob(d->BasePage)+(i*NBPP))),
				     0xff, NBPP);
		}
		d = GetMemoryDescriptor(d);
	}
}

static void
bset(char *p, char c, int i)
{
        while (i--) *p++ = c;
}


