/*
 * FILE: eoe/cmd/miser/cmd/cpuset/cpuset.c
 *
 * DESCRIPTION:
 *	cpuset - define and manage a set of CPUs. 	
 *
 * 	The cpuset command is used to create and destroy cpusets, 
 *	to retrieve information about existing cpusets, and to attach a 
 *	process and all of its children to a cpuset.
 */   

/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <stdio.h>
#include "libcpuset.h"


int
usage(const char *pname)
/*
 * Print an usage message with a brief description of each possible
 * option and quit.
 */
{
        fprintf(stderr,
                "\nUsage: %s [-q cpuset_name [-A cmd]|[-c -f fname]|[-d]|"
			"[-l][-m]|[-Q]]\n\t\t| -C | -Q\n\n"
                "Valid Arguments:\n"
                "\t-q cpuset_name\n"
		"\t\t-A cmd\tRuns the command on the cpuset.\n"
		"\t\t-c -f fname\n"
			"\t\t\tCreates a cpuset according to config file.\n"
		"\t\t-d\tDestroys the cpuset (no process attached).\n"
		"\t\t-l\tLists all the processes in the cpuset.\n"
		"\t\t-m\tMoves all the attached processes out of the cpuset.\n"
		"\t\t-Q\tPrints a list of the cpus belong to the cpuset.\n"
		"\t-C\tQuery cpuset, to which process is currently attached.\n"
		"\t-Q\tLists names of all the cpusets currently defined.\n"
		"\t-h\tPrint the command's usage message.\n\n",
			pname);
        return 1;	/* error */

} /* usage */


int
main(int argc, char** argv)
/*
 * Parse command line input and make cpuset library call to perform
 * requested cpuset operation.
 */
{
	int	c	= 0;	/* Hold command line option char */
	int	command	= 0;	/* Command+1 index in argv */
	int	A_flag	= 0;	/* '-A' flag */
	int	C_flag	= 0;	/* '-C' flag */
	int	c_flag	= 0;	/* '-c' flag */
	int	d_flag	= 0;	/* '-d' flag */
	int	f_flag	= 0;	/* '-f' flag */
	int	l_flag	= 0;	/* '-l' flag */
	int	m_flag	= 0;	/* '-m' flag */
	int	Q_flag	= 0;	/* '-Q' flag */
	int	q_flag	= 0;	/* '-q' flag */

	char*	fname;		/* Cpuset config filename */
	char*	qname;		/* Cpuset queue name */

	while ((c = getopt(argc, argv, ":A:Ccdf:lmQq:h")) != -1) {

		switch(c) {

                case 'q':
			q_flag++;
                        qname = optarg;
                        break;

		/*
		 * -q qname  -A command
		 *	Runs the command on the cpuset identified by 
		 *	the -q parameter. If the user does not have 
		 *	access permissions or the cpuset does not
		 *	exist, an error is returned.
		 */		
		case 'A':
			A_flag++;
			command = optind;
			break;

		/*
		 * -C
		 *	Prints the name of the cpuset to which the 
		 *	process is currently attached.
		 */
                case 'C':
                        C_flag++;
                        break;
                        
		/*
		 * -q cpuset_name -c -f  filename
		 *	Creates a cpuset with the configuration file 
		 * 	specified by the -f parameter and the name 
		 *	specified by the -q parameter. If the cpuset
		 *	name already exists, a CPU specified in the 
		 *	cpuset configuration file is already a member 
		 *	of a cpuset, or the user does not have the
		 *	requisite permissions, the operation fails.
		 */
                case 'c':
                       c_flag++;
                        break;

                case 'f':
			f_flag++;
                        fname = optarg;
                        break;	

		/*
		 * -q cpuset_name -d
		 *	Destroys the specified cpuset. A cpuset can 
		 *	only be destroyed if there are no processes 
		 *	currently attached to it.
		 */
                case 'd':
                        d_flag++;
                        break;

		/*
		 * -q cpuset_name -l
		 *	Lists all the processes in the cpuset.
		 */
		case 'l':
			l_flag++;
			break;

		/*
		 * -q cpuset_name -m
		 *	Moves all the attached processes out of the cpuset.
		 */
		case 'm':
			m_flag++;
			break;

		/*
		 * -q cpuset_name -Q
		 *	Prints a list of the cpus that belong to the cpuset.
		 */
                case 'Q':
                        Q_flag++;
                        break;

		/* Print usage message */
                case 'h':
			return usage(argv[0]);

		/* Getopt reports required argument missing from -j option. */
		case ':':
			/* -A option requires an argument. */
			if (strcmp(argv[optind -1], "-A") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-A' option "
					"requires an argument\n", argv[0]);
				return usage(argv[0]);
			}

			/* -f option requires an argument. */
			if (strcmp(argv[optind -1], "-f") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-f' option "
					"requires an argument\n", argv[0]);
				return usage(argv[0]);
			}

			/* -q option requires an argument. */
			if (strcmp(argv[optind -1], "-q") == 0) {
				fprintf(stderr, "\n%s: ERROR - '-q' option "
					"requires an argument\n", argv[0]);
				return usage(argv[0]);
			}

			break;

		/* getopt reports invalid option in command line. */
		case '?':
			fprintf(stderr, "\n%s: ERROR - invalid command "
				"line option '%s'\n",
				argv[0], argv[optind -1]);
			return usage(argv[0]);

		} /* switch */

		if (A_flag) {
			break;
		}

	} /* while */

	/* Following arguments cannot co-exist */
	if (A_flag + Q_flag + c_flag + d_flag + C_flag + l_flag + m_flag != 1) {
		return usage(argv[0]);
	}

	/* Create request must have a cpuset queue name */
	if (!q_flag && c_flag && f_flag) {
		return usage(argv[0]);
	}

	/* Call specified cpuset queue related functions */
        if (q_flag) {

		/* Attach a command to the cpuset queue */
	        if (A_flag && command) {
			return cpuset_attach(qname, argv[command-1], 
				&argv[command-1]); 

		/* Destroy the cpuset queue specified - no process attached */
		} else if (d_flag) {
			return cpuset_destroy(qname);

		/* List processes attached to the specified cpuset queue */
		} else if(l_flag) {
			return cpuset_list_procs(qname);
 
		/* Move all processes from the cpuset queue */
		} else if(m_flag) {
			return cpuset_move_procs(qname);

		/* List cpus belong to the specified cpuset queue */
		} else if (Q_flag) {
			return cpuset_query_cpus(qname);

		/* Create the cpuset queue according to the config file */
		} else if (c_flag && f_flag) {
			return cpuset_create(qname, fname);

		} else {
			return usage(argv[0]);
		}

	/* Get all the cpuset names */
	} else if (Q_flag) {
		return cpuset_query_names();

	/* Name of the cpuset, current process is attached to */
        } else if (C_flag) {
                return cpuset_query_current();

	} else {
		return usage(argv[0]);
	}

} /* main */
