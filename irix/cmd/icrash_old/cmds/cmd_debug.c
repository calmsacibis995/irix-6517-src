#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_debug.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

typedef struct debug_class_s {
	char 		   *label;
	k_uint_t   		value;
} debug_class_t;

debug_class_t debug_class[] = {
	/*-------------------------------*/
	/* LABEL            VALUE        */
	/*-------------------------------*/
	{ "GLOBAL",		 	DC_GLOBAL     },
	{ "FUNCTRACE",	 	DC_FUNCTRACE  },
	{ "ALLOC",		 	DC_ALLOC      },
	{ "BUF",		 	DC_BUF        },
	{ "DWARF",		 	DC_DWARF      },
	{ "ELF",		 	DC_ELF        },
	{ "EVAL",		 	DC_EVAL       },
	{ "FILE",		 	DC_FILE       },
	{ "KTHREAD",	 	DC_KTHREAD    },
	{ "MEM",		 	DC_MEM        },
	{ "PAGE",		 	DC_PAGE       },
	{ "PDA",		 	DC_PDA        },
	{ "PROC",		 	DC_PROC       },
	{ "REGION",		 	DC_REGION     },
	{ "SEMA",		 	DC_SEMA       },
	{ "SNODE",		 	DC_SNODE      },
	{ "SOCKET",		 	DC_SOCKET     },
	{ "STREAM",		 	DC_STREAM     },
	{ "STRUCT",		 	DC_STRUCT     },
	{ "SYM",		 	DC_SYM        },
	{ "TRACE",		 	DC_TRACE      },
	{ "USER",		 	DC_USER       },
	{ "VNODE",		 	DC_VNODE      },
	{ "ZONE",		 	DC_ZONE       },
	{ (char *)0,	 	-1            }
};

debug_class_t klib_debug_class[] = {
	/*--------------------------------*/
	/* LABEL            VALUE         */
	/*--------------------------------*/
	{ "KL_GLOBAL",		KLDC_GLOBAL    },
	{ "KL_FUNCTRACE",	KLDC_FUNCTRACE },
	{ "KL_CMP",		 	KLDC_CMP       },
	{ "KL_ERROR",		KLDC_ERROR     },
	{ "KL_HWGRAPH",	 	KLDC_HWGRAPH   },
	{ "KL_INIT",		KLDC_INIT      },
	{ "KL_KTHREAD",	 	KLDC_KTHREAD   },
	{ "KL_MEM",		 	KLDC_MEM       },
	{ "KL_PAGE",	 	KLDC_PAGE      },
	{ "KL_PROC",		KLDC_PROC      },
	{ "KL_STRUCT",		KLDC_STRUCT    },
	{ "KL_UTIL",		KLDC_UTIL      },
	{ (char *)0,	    -1             }
};


/*
 * debug_banner()
 */
void
debug_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		fprintf(ofp, "LABEL             VALUE\n");         
		indent_it(flags, ofp);
	}

	if (flags & SMAJOR) {
		fprintf(ofp, "====================================\n");         
		indent_it(flags, ofp);
	}
}

/*
 * list_debug_classes()
 */
void
list_klib_debug_classes(FILE *ofp)
{
	int i = 0;

	while (klib_debug_class[i].label) {
		fprintf(ofp, "%-16s  0x%016llx\n", 
			klib_debug_class[i].label, klib_debug_class[i].value); 
		i++;
	}
}

/*
 * list_debug_classes()
 */
void
list_debug_classes(FILE *ofp)
{
	int i = 0;

	while (debug_class[i].label) {
		fprintf(ofp, "%-16s  0x%016llx\n", 
			debug_class[i].label, debug_class[i].value); 
		i++;
	}
}

/*
 * print_debug_value()
 */
void
print_debug_value(k_uint_t d, FILE *ofp)
{
	if (d) {
		fprintf(ofp, "\n");
		fprintf(ofp, "    LEVEL : %lld\n", (d & 0xf));
		fprintf(ofp, "  CLASSES : GLOBAL");

		if (d & 0xfffffffffffffff0) {
			int i;

			i = 1;
			while (klib_debug_class[i].label) {
				if (d & klib_debug_class[i].value) {
					fprintf(ofp, "|%s", klib_debug_class[i].label);
				}
				i++;
			}
			i = 1;
			while (debug_class[i].label) {
				if (d & debug_class[i].value) {
					fprintf(ofp, "|%s", debug_class[i].label);
				}
				i++;
			}
		}
		fprintf(ofp, "\n");
	}
}

/*
 * debug_cmd() -- Sets the debug level or reports the current level
 */
int
debug_cmd(command_t cmd)
{
	k_uint_t d;

	if (cmd.flags & C_LIST) {
		debug_banner(cmd.ofp, BANNER|SMAJOR);
		fprintf(cmd.ofp, "KLIB DEBUG CLASSES:\n\n");
		list_klib_debug_classes(cmd.ofp);	
		fprintf(cmd.ofp, "\nICRASH DEBUG CLASSES:\n\n");
		list_debug_classes(cmd.ofp);	
		debug_banner(cmd.ofp, SMAJOR);
		return(0);
	}

	if (cmd.nargs == 0) {
		fprintf(cmd.ofp, "Current debug value : 0x%llx\n", klib_debug);
		print_debug_value(klib_debug, cmd.ofp);
		return(0);
	}
	GET_VALUE(cmd.args[0], &d);
	if (KL_ERROR) {
		KL_ERROR |= KLE_BAD_DEBUG;
		kl_print_error(K);
		fprintf(cmd.ofp, "Current debug value : 0x%llx\n", klib_debug);
		print_debug_value(klib_debug, cmd.ofp);
		return(1);
	}

	/* If 'd' is negative, then unset the debug value.
	 */
	if ((long long)d < 0) {
		d = 0;
	}

	/* set the 'd' value 
	 */
	klib_debug = d;
	fprintf(cmd.ofp, "New debug value : 0x%llx\n", klib_debug);
	print_debug_value(klib_debug, cmd.ofp);
	return(0);
}

#define _DEBUG_USAGE "[-l] [-w outfile] [debug_value]"

/*
 * debug_usage() -- Print the usage string for the 'debug' command.
 */
void
debug_usage(command_t cmd)
{
	CMD_USAGE(cmd, _DEBUG_USAGE);
}

/*
 * debug_help() -- Print the help information for the 'debug' command.
 */
void
debug_help(command_t cmd)
{
	CMD_HELP(cmd, _DEBUG_USAGE,
		"Set the debug value, or print the current debug value if no "
		"debug value is specified.");
}

/*
 * debug_parse() -- Parse the command line arguments for the 'debug' command.
 */
int
debug_parse(command_t cmd)
{
	return (C_MAYBE|C_LIST);
}
