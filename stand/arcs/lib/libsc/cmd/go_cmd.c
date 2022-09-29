#ident	"lib/libsc/cmd/go_cmd.c:  $Revision: 1.18 $"

/*
 * go -- transfer control to client code
 *	default pc provided by most recent load with boot -n <filename>
 *
 * go [<pc>] [<arg1>] [<arg2>] ... [<argn>]
 *
 */

#include <stringlist.h>
#include <arcs/restart.h>
#include <arcs/io.h>
#include <libsc.h>

extern unsigned long boot_pc;		/* from boot_cmd.c */
extern char *boot_file;			/* from boot_cmd.c */

/*ARGSUSED*/
int
go_cmd(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
	struct string_list newargv;
	__psunsigned_t go_pc;
	int sarg;
	long rc;

	if (argc < 1) 
		return (1);

	/* If no args, use default pc from boot -n command */
	if (argc == 1) {
		go_pc = boot_pc;
		sarg = 1;
	/* 
	 * If 2nd arg is not pc, then it must be an arg for program.
	 * Use default pc from boot -n command.
	 */
	} else if (*atobu_ptr(argv[1], &go_pc)) {
		go_pc = boot_pc;
		sarg = 1;
	} else
		sarg = 2;

	init_str(&newargv);
	if (boot_file && new_str1(boot_file, &newargv))	
		return(0);

	/* Copy args to pass to new program */
	for (; sarg<argc; sarg++) {
		if (new_str1(argv[sarg], &newargv))	
			return(0);
	}

	init_rb();

	printf("jumping to Invoke(%x)\n", go_pc);
	rc = Invoke(go_pc,0,(LONG)newargv.strcnt,newargv.strptrs,environ);
	printf("oops, back from Invoke(x), rc=%d\n", rc);
	return (int)rc;			/* ok to drop upper 32 bits of errno */
}
