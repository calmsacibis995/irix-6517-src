/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 *	npri - do various funky things with the kernel priorities for
 *	       process scheduling.
 *
 *	Usage:
 *	   npri {[-w]|[-t slice]|[-n nice]|[-r pri [-s policy]]} 
 *						[command [arg1 [arg2 ...]
 *	   npri {[-w]|[-t slice]|[-n nice]|[-r pri [-s policy]]} -p proc
 *
 */

#include	<sys/types.h>
#include	<sys/param.h>
#include	<sys/schedctl.h>
#include	<sys/sched.h>
#include 	<stdio.h>
#include	<string.h>
#include	<sys/resource.h>

#define	SHELL		"/bin/sh"

char		*pname;
int		slice = -1;
int		niceval = -1;
int		ndpri = -1;
int 		weightless = 0;
int		pidpoke = 0;
char            *policy = NULL;
struct sched_param prio;
extern int	errno;
char		*getenv();

int
main(int argc, char **argv, char **envp)
{
        extern int	optind;
        extern char	*optarg;
        int		c;
        int		err = 0;
        char		*sh;

        prio.sched_priority = -1;
	pname = argv[0];
	while ((c = getopt(argc, argv, "wt:n:h:p:r:s:")) != EOF) {
		switch (c) {
		case 'w':
			weightless = 1;
			break;
		case 't':
			slice = strtoul(optarg, (char **) 0, 0);
			break;
		case 'n':
			niceval = strtol(optarg, (char **) 0, 0);
			break;
		case 'h':
			ndpri = strtol(optarg, (char **) 0, 0);
			break;
		case 'p':
			pidpoke = strtoul(optarg, (char **) 0, 0);
			break;
                case 'r':
                        prio.sched_priority = strtol(optarg, (char**) 0,0);
                        break;
                case 's':
                        policy = optarg;
                        break;
                default:
			err++;
			break;
		}
	}
	if ((weightless && (prio.sched_priority >= 0 || ndpri >= 0)) ||
		err || (prio.sched_priority >= 0 && ndpri >= 0) ||
		(policy && prio.sched_priority == -1)) {
		fprintf(stderr,
"usage:\n%s [-w]|[-t slice][-n nice]\\\n\t[-r pri[-s policy]]](-p pid | cmd args ...)\n",
			pname);
		exit(1);
	}

	if (pidpoke != 0) {
		if (optind < argc) {
			fprintf(stderr,
				"%s: -p option not valid with command\n",
				pname);
			exit(1);
		}
		setsched(pidpoke);
	}
	else {
		setsched(0);
		if (optind == argc) {
			if ((sh = getenv("SHELL")) == 0)
				sh = SHELL;
			execl(sh, sh, 0);
			perror(pname);
			exit(1);
		}
		else {
			execvp(argv[optind], &argv[optind]);
			perror(pname);
			exit(1);
		}

	}
        return 0;
}

int
setsched(int pid) 
{
        int policy_num;
	if (weightless) {
		if (schedctl(NDPRI, pid, NDPLOMAX) < 0) {
			perror(pname);
			exit(1);
		}
	}
	if (slice > 0) {
		if (schedctl(SLICE, pid, slice) < 0) {
			perror(pname);
			exit(1);
		}
	}
	if (niceval >= 0) {
		if (schedctl(RENICE, pid, niceval) < 0) {
			perror(pname);
			exit(1);
		}
	}
	if (ndpri >= 0) {
		if (schedctl(NDPRI, pid, ndpri) < 0) {
			perror(pname);
			exit(1);
		}
	}
        if (prio.sched_priority != -1)  {
                if (policy) {
                        if (strcmp(policy, "FIFO") == 0)
                                policy_num = SCHED_FIFO;
                        else if (strcmp(policy, "TS") == 0)
                                policy_num = SCHED_TS;
                        else if (strcmp(policy, "RR") == 0)
                                policy_num = SCHED_RR;
                        else {
                                fprintf(stderr,"npri: Invalid policy.\n");
                                exit(1);
                        }
                        if (sched_setscheduler(pid, policy_num, &prio) < 0) {
                                perror(pname);
                                exit(1);
                        }
                        return 1;
                }

                if (sched_setparam(pid, &prio) < 0) {
                         perror(pname);
                         exit(1);
                }

        } 
        return 1;
}
