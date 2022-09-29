#ident "irix/cmd/mkpart.c: $Revision: 1.22 $"

/*
 * mkpart.c
 *
 *	make partitions for Origin 2000/200 systems.
 *	command line handler.
 *
 */

#define SN0 1

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/conf.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/signal.h>

#include <sys/SN/router.h>
#include <sys/SN/SN0/sn0drv.h>

#include <capability.h>
#include <sys/capability.h>

#include "mkpart.h"
#include "mkpd.h"
#include "mkprou.h"

extern valid_config_t   valid_config[MAX_TOTAL_MODULES] ;
extern valid_config_t   *vc_hdr_p ;

extern int debug ;		/* declared in mkplib.c */
extern int force_option ;	/* declared also in mkplib.c */

static int insufficient_privilege(void) ;
extern partcfg_t *generate_pc_from_user(void) ;

/*
 * Error Messages.
 * NOTE: The err msg defines need to be repeated in mkpart.h
 */
char *msgs[] = {
#define MSG_HEADER 	0
	"Making Partitions ...\n",
#define MSG_CUR_PARTS 	1
	"Current Partition Information\n\n",
#define MSG_WARN_SU  	2
	"Warning: Must be privileged user\n",
#define MSG_ARG_INVALID 3
	"%s: Invalid option.\n",
#define MSG_SYNTAX_ERROR 4
        "Syntax error.\n",
NULL
} ;

char *force_option_msg = 
"WARNING: Use of Force Option overrides ALL sanity checks.\n" ;

/*
 * Usage msg.
 */
char *usgmsg = 
"Usage: mkpart [-F] [-l] [-init] [-f <cfgfile>]\n"
"              [-p <pno> -m <mno> ...]\n"
"              [-p <pno> -m <mno> ...] ...\n"
"              [-a] [-d <partid>] [-help]\n"
;

/*
 * Command line parsing support
 */

static void
pr_usage(void)
{
    	fprintf(stdout, "%s", usgmsg) ;
}

static void
pr_errmsg(char *errbuf)
{
    	if (*errbuf) {
		fprintf(stderr, "%s", errbuf) ;
	}
}

/* check if an arg follows this arg. Like <fname> after a -f */

int
check_max_indx(int max, int ind)
{
	if ((ind + 1) == max)
        	return -1 ;
    	return 1 ;
}

/* check if args are of the form -fxxx */

int
check_opt(char **argv, int indx)
{
	if (argv[indx] == NULL)
                return -1 ;

	if (argv[indx][2])
        	return -1 ;

    	return 1 ;
}

/*
 * get_arg_fp
 * 
 * 	get the fileh of the file arg
 * 	specified in the cmd line.
 *
 * Return:
 *
 *	file handle in fpp
 *	value of next argument index.
 * 	if this returns argc, it means there are no more args
 *	to parse. Then the calling loop will terminate.
 */

static int
get_arg_fp(int argc, char **argv, int indx, FILE **fpp)
{
	FILE 	*fp ;
	char	errbuf[MAX_ERRBUF_LEN] ;

	/* Return if the option looks bad or if there is no file name */

	if (check_opt(argv, indx) < 0) {
		fprintf(stdout, "Ignoring unknown arg %s\n", argv[indx]) ;
		return argc ;
	}

	if (check_max_indx(argc, indx) < 0) {
		fprintf(stderr, "Filename should be specified\n") ;
		return argc ;
	}

	fp = fopen(argv[indx+1], "r");
	if (fp == NULL) {
		sprintf(errbuf, "%s", argv[indx+1]);
		perror(errbuf);
		return argc ;
	}

	*fpp = fp ;

	return indx + 2 ;
}

/* 
 * get_arg_kill
 *
 * 	read the partid given as arg to -kill option.
 *	create a partcfg_t for it and return the ptr.
 *	This is done for uniformity purpose.
 */
static partcfg_t *
get_arg_kill(int argc, char **argv, int indx)
{
	partid_t 	p ;
	partcfg_t	*pc = NULL ;

        if ((check_opt(argv, indx) < 0) ||
                        (check_max_indx(argc, indx) < 0))
		return NULL ;
	p = atoi(argv[indx+1]) ;
	if (pc = partcfgCreate(MAX_MODS_PER_PART)) {
		partcfgId(pc) = p ;
	}
	return pc ;
}

/*
 * parse_cmd_line
 * 
 * 	parse the command line which has the form
 * 	-p <pno> -m <mod1> <mod2> ... -p <pno> -m OR
 *	-f <filename>
 *
 *	Returns	:	opcode for given command
 *			An error msg in errbuf in case of error.
 */

/* All accepted options and their opcodes. */

char *arg_opt[] = {
#define ARG_F		0
	"f",
#define ARG_D		1
	"debug",
#define ARG_L		2
	"l",
#define ARG_P		3
	"p",
#define ARG_M		4
	"m",
#define ARG_RESET	5
	"init",
#define ARG_ACTIVATE	6
	"a",
#define ARG_KILL 	7
	"d",
#define ARG_FORCE 	8
	"F",
#define ARG_HELP 	9
	"help",
NULL
} ;
#define ARG_INVALID	-1

static int
parse_cmd_line(	int 		argc	, 
		char 		**argv	, 
		partcfg_t 	**pcp	, 
		int 		*cmd	,  	/* opcode for given command */
		char 		*errbuf)
{
	int		nxtarg 	= 1 ;
	FILE		*fp 	= NULL ;
	partcfg_t	*pc 	= NULL ;
	char		*str ;
	int		rv 	= -1 ;
	int		opt_ind = -1 ;

	*errbuf = 0 ;
	*cmd    = -1 ;

	while (nxtarg < argc) {
		if (argv[nxtarg][0] != '-') {
			fprintf(stdout, "Ignoring unknown arg %s.\n", 
					argv[nxtarg]) ;
	    		nxtarg++ ;	/* skip args without a leading '-' */
			continue ;
		}

		/* Get the index of the option from arg_opt table */

		opt_ind = get_string_index(arg_opt, &argv[nxtarg][1]) ;

            	switch (opt_ind) {
			case ARG_F:
				/* get file pointer */
    		    		nxtarg = get_arg_fp(argc, argv, nxtarg, &fp) ;
		    		if (fp) {
	    	       	 		pc = parse_file(fp, errbuf) ;
					fclose(fp) ;
				}
		    		*cmd = ARG_F ;
		    		goto parse_done;

			case ARG_D:
		    		debug = 1 ;
	    	    		nxtarg++ ;
				break ;

			case ARG_FORCE:
		    		force_option = 1 ;
				fprintf(stderr, "%s", force_option_msg) ;
				if (!yes_or_no(
					"Do you want to continue? [y/n]: ")) {
					fprintf(stderr, 
						"Exiting the command now.\n") ;
					exit(0) ;
				}
	    	    		nxtarg++ ;
				break ;

			case ARG_HELP:
				pr_usage() ;
	    	    		nxtarg++ ;
				break ;

			case ARG_L:
			case ARG_RESET:
		    		rv 	= 1 ;
		    		*cmd 	= opt_ind ;
		    		goto all_done ;

			case ARG_ACTIVATE:
		    		rv 	= 1 ;
		    		*cmd = opt_ind ;
                    		goto all_done ;

			case ARG_KILL:
				pc = get_arg_kill(argc, argv, nxtarg) ;
		    		*cmd = ARG_KILL ;
				if (!pc)
					strcpy(errbuf, msgs[MSG_SYNTAX_ERROR]) ;
                    		goto parse_done ;

			case ARG_P:
				/* 
				 * Transform cmd line args to intermediate
				 * form string. File parsing routine looks at
				 * this string. This means a single parsing
				 * routine for both -f and -p option.
				 */ 
	   	    		str = cmd_to_file_string(argc, argv, nxtarg) ;
		    		if (str) {
					pc = parse_multi_string(str, errbuf) ;
					free(str) ;
		    		}
		    		*cmd   	= ARG_P ;
	   	    		nxtarg 	= argc ; /* end the loop */
				break ;

			case ARG_M:
				/* This should have been consumed by ARG_P
				 * It must have appeared without the -p
				 */
				sprintf(errbuf,
				"-m must be specified after a -p\n") ;
                    		goto all_done ;

			case ARG_INVALID:
		    		sprintf(errbuf,	
					msgs[MSG_ARG_INVALID], argv[nxtarg]) ;
                    		goto all_done ;

			default:	/* this will not be reached */
		    		goto all_done ;
	    	}	/* switch */
    	} 	/* While */
parse_done:
    	*pcp = pc ;
    	if (pc)
		rv = 1 ;
all_done:
    	return rv ;
}

void
main(int argc, char **argv)
{
	partcfg_t	*pc = NULL ;
	int 		rv ;
	char		errbuf[MAX_ERRBUF_LEN] ;
	int		cmd ;
	int		part_make_rv = 0 ;

        vc_hdr_p = &valid_config[0] ;

	/* Check for su access */
        if (insufficient_privilege()) {
        	fprintf(stderr, msgs[MSG_WARN_SU]) ;
        	exit(1);
    	}

	/* Choose interactive or command line mode */

	if (argc == 1) {

		/* XXX TBD. For now, just print own partition id. */

		pc = mkp_interactive() ;
		partcfgFreeList(pc) ;
		exit (0) ;
    	} else {

		/* pc gets the partcfg list and cmd gets option */

		rv = parse_cmd_line(argc, argv, &pc, &cmd, errbuf) ;
		if (rv < 0) {
	    		pr_errmsg(errbuf) ;
	    		exit(0) ;
		}
	}

        /* Read in the valid configs permitted */

        read_valid_config(vc_hdr_p) ;

	if (!debug) {
		ignore_signal(SIGINT, SA_RESTART) ;
		ignore_signal(SIGTERM, SA_RESTART) ;
		ignore_signal(SIGHUP, SA_RESTART) ;
	}

    	if (debug) {
		printf("Required partition config is ...\n") ;
    		partcfgDumpList(pc) ;
		printf("\n") ;
    	}

	/* Act on the opcode returned. */

    	switch(cmd) {
		case ARG_F:
		case ARG_P:
			if (force_option || (check_sanity(pc, errbuf) >= 0)) {
	    			part_make_rv = part_make(pc, errbuf) ;
	    			if (part_make_rv == PART_MAKE_UNPARTITIONED)
					shutdown_self_query(errbuf) ;
				else if (part_make_rv == PART_MAKE_NORMAL)
					inquire_and_reboot(pc, errbuf) ;
			}
			break ;

		case ARG_L:
	    		part_list(errbuf) ;
			break ;

		case ARG_RESET:
	    		part_reinit(errbuf) ;
			if (!(*errbuf)) {
				fprintf(stdout, "All modules assigned to partition 0.\n") ;
				/* "Please reboot all partitions.\n") ; */
				inquire_and_reboot_init(errbuf) ;
			}
			break ;

		case ARG_ACTIVATE:
	    		part_activate(errbuf) ;
			break ;

		case ARG_KILL:
	    		part_kill(pc, errbuf) ;
			break ;

		default:
			break ;
    	}

        pr_errmsg(errbuf) ;

    	partcfgFreeList(pc) ;
}


/*
 * mkp_interactive
 *
 * Interactive partition maker.
 * TBD
 */

partcfg_t *
mkp_interactive(void)
{
	partcfg_t 	*pc = NULL ;
	partid_t	p ;

#ifdef DEBUG_VALID_CONFIG
        /* To debug the valid config file support.
         * Read in both the actual config and required
         * config from stdin, and check it against the
         * input from the config file.
         */
        read_valid_config(vc_hdr_p) ;
        check_valid_config(     generate_pc_from_user(),
                        generate_pc_from_user(), NULL, vc_hdr_p) ;
#endif

	p = get_my_partid() ;

	if (IS_PARTID_VALID(p)) {
		fprintf(stdout, "Partition id = %d\n", p) ;
	} else {
		fprintf(stdout, "This system is not partitioned.\n") ;
		return NULL ;
	}

	return pc ;
}

/* -------------- Capability checks ------------------------ */

static int      cap_enabled ;

/*
 * cap_permitted
 *
 *	Read the user's capability set value. If he has the required
 *	effective permissions, return TRUE, else return FALSE.
 */

static int
cap_permitted(cap_value_t cap)
{
        cap_t 		cap_state;
	int		result = 0 ;

        if (!cap_enabled)
                return (1);

        if ((cap_state = cap_get_proc()) == NULL) {
                return (0);
        }

        result = CAP_ID_ISSET(cap, cap_state->cap_effective) ;

	return result ;
}

/*
 * Procedure:   insufficient_privilege
 *
 * Job:         Checks user privilege. If Capability Mgmt
 *              is enabled (Trusted Irix, CMW, POSIX Capabilities)
 *              check if we have
 *
 *                      - CAP_SHUTDOWN
 *                      - CAP_SYSINFO_MGT
 *			- CAP_DEVICE_MGT
 *
 *              The current functionality of mkpart seems to need
 *              only these 2 now. More can be added if needed later.
 *
 * Returns:	TRUE if user DOES NOT have sufficient privilege.
 *		FALSE if user DOES have sufficient privilege.
 *
 * Note:        Most of this code is derived from a similar
 *              routine in su.c
 *
 */

static int
insufficient_privilege (void)
{

        /*
         * Find out a few things about the system's configuration
         */
        if ((cap_enabled = sysconf(_SC_CAP)) < 0)
                cap_enabled = 0;

        /*
         * If it's a strict SuperUser environment the effective uid tells all.
         */
        if (cap_enabled == CAP_SYS_DISABLED)
                 return (geteuid());

        /*
         * If it's an augmented SuperUser environment an effective uid
         * of root (0) is sufficient.
         */
        if ((cap_enabled == CAP_SYS_SUPERUSER) && (geteuid() == 0))
                 return (0);

        /*
         * If it's a No SuperUser environment, or the euid is not root,
         * check the capabilities.
         */

        if (!cap_permitted(CAP_SHUTDOWN | CAP_SYSINFO_MGT | CAP_DEVICE_MGT))
                return (1);

        return (0);
}
