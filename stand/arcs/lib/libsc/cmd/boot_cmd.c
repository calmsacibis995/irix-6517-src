#ident	"lib/libsc/cmd/boot_cmd.c:  $Revision: 1.39 $"

/*
 * boot -- boot new image
 */

#include <sys/sbd.h>
#include <stringlist.h>
#include <arcs/errno.h>
#include <arcs/restart.h>
#include <arcs/hinv.h>
#include <arcs/io.h>
#include <arcs/pvector.h>
#include <libsc.h>
#include <libsc_internal.h>

extern char *	make_bootfile(int);
extern MEMORYDESCRIPTOR *mem_getblock(void);

static void booterr (long, struct string_list *, char *, int);

unsigned long boot_pc;		/* shared with go_cmd.c */
char *boot_file;		/* shared with go_cmd.c */

/*ARGSUSED*/
int
boot(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
	struct string_list newargv;
	int nogo = 0;
	int nobootfile = 0;
	int noreloc = 0;
	unsigned long lowaddr;
	MEMORYDESCRIPTOR *m;
	long err = 0;

	/*
	 * initialize defaults
	 */
	boot_file = (char *)make_bootfile(1);

	init_str(&newargv);
	if (new_str1("", &newargv))	/* leave space for boot_file as arg0 */
		return(0);

	while (--argc > 0) {
		argv++;
		if (!*argv)		/* allow empty argv slots */
			continue;

		if ((*argv)[0] == '-')
			switch ((*argv)[1]) {

			case 'a':	/* load without relocating */
				noreloc = 1;
				break;

			case 'f':	/* file to boot */
				if (--argc <= 0)
					return(1);
				boot_file = *++argv;
				nobootfile = 1;
				break;

			case 'n':	/* load but don't go */
				nogo = 1;
				break;

			case '-':	/* just pass arg to booted program */
					/* skip initial - */
				if (new_str1((*argv)+1, &newargv))
					return(0);
				break;

			default:
				printf("unknown boot option: %s\n"
				       "precede option with extra '-' to\n"
				       "pass to booted program\n",*argv);
				return(1);
			}
		else if (new_str1(*argv, &newargv))
			return(0);
	}

	if (boot_file == NULL) {
		printf("No valid boot information found in environment.\n");
		return(0);
	}

	if (set_str(boot_file, 0, &newargv))
		return(0);

	rbsetbs(BS_BSTARTED);		/* boot started */

	if (noreloc)
		if (nogo)
			err = load_abs (newargv.strptrs[0], &boot_pc);
		else
			err = exec_abs (newargv.strptrs[0],
					(LONG)newargv.strcnt,
					newargv.strptrs, environ);
	else
		if (nogo) {
			/* If the program is relocatable and is not to be
			 * executed, then load it at the default address,
			 * which is the top of the largest continuous memory
			 * block.
			 */
			unsigned long top;
			if (!(m = mem_getblock()))
				return (ENOMEM);
			top = PHYS_TO_K0(arcs_ptob(m->BasePage + m->PageCount));
			err = Load(newargv.strptrs[0], top, &boot_pc, &lowaddr);
		} else {
			setenv ("kernname", newargv.strptrs[0]);
			err = Execute (newargv.strptrs[0], (LONG)newargv.strcnt,
						newargv.strptrs, environ);
		}

	if (err != ESUCCESS)
		booterr (err, &newargv, boot_file, nobootfile);

	return(0);
}

static void 
booterr (long err, struct string_list *newargv, char *bootfile, int nobootfile)
{
	rbclrbs(BS_BSTARTED);		/* boot will never finish! */

	printf("Unable to load %s: ", newargv->strptrs[0]);
	switch(err) {
	case EIO:
		printf("no such device.\n");
		break;
	case ENXIO:
		printf("no recognizable file system on device.\n");
		break;
	case ENOCONNECT:
		printf("could not connect to remote server.\n");
		break;
	case EINVAL:
		if (nobootfile)
			printf("``%s'' is not a valid file to boot.\n",
				newargv->strptrs[0]);
		else
			printf("invalid bootfile %s.\n", bootfile);
		break;
	default:
		perror(err,NULL);
		break;
	}
}
