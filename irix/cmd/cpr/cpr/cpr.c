/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.54 $"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/sbd.h>
#include <ckpt.h>

#define	CKPT_RLIMIT_MAX	RLIM_INFINITY

#define	SUBCMD_CKPT	1
#define	SUBCMD_RESTART	2
#define	SUBCMD_REMOVE	3
#define	SUBCMD_INFO	4

static int ckpt_info(char *);
static void usage(void);
static int parse_id(ckpt_id_t *, u_long *);
static int do_subcmd(int, char *);
static int do_checkpoint(ckpt_id_t, u_long, char *);
static int do_restart_remove_stat(int, char *);


extern char ckpt_restart_is_client;
extern char *ckpt_array_name = NULL;

static char *idp;	/* pointer to the id string */
static int force = 0;

void
main(int argc, char *argv[])
{
	int subcmd = 0;
	int verbose = 0;
	char *pathname = 0;
	struct rlimit rl;
	int c, rc = 0;
	int i;

	while ((c = getopt(argc, argv, "UASVdfjukgvc:p:r:D:t:i:P:a:"))!= EOF){

		switch(c) {

		case 'p':
			if ((idp = malloc(strlen(optarg)+1)) == NULL) {
				cerror("Failed to allocate memory (%s)\n", STRERR);
				exit(1);
			}
			sprintf(idp, "%s", optarg);
			break;

		case 'c':
			pathname = optarg;
			subcmd = SUBCMD_CKPT;
			cpr_flags |= CKPT_CHECKPOINT_CPRCMD;
			break;

		case 'P':
			pipe_rc = atoi(optarg);
			break;

		case 'r':
			pathname = optarg;
			subcmd = SUBCMD_RESTART;
			cpr_flags |= CKPT_RESTART_CPRCMD;
			break;
		case 'D':
			pathname = optarg;
			subcmd = SUBCMD_REMOVE;
			break;

		case 'i':
			/* get information on the statefile */
			pathname = optarg;
			subcmd = SUBCMD_INFO;
			break;

		case 'f':
			force = 1;
			break;

		case 'u':
			cpr_flags |= CKPT_CHECKPOINT_UPGRADE;
			break;

		case 'k':
			cpr_flags |= CKPT_CHECKPOINT_KILL;
			break;

		case 'g':
			cpr_flags |= CKPT_CHECKPOINT_CONT;
			break;

		case 'd':
			cpr_flags |= CKPT_OPENFILE_DISTRIBUTE;
			break;

		case 'S':
			cpr_flags |= CKPT_CHECKPOINT_STEP;
			break;

		case 'j':
			cpr_flags |= CKPT_RESTART_INTERACTIVE;
			break;

		case 'U':
			cpr_flags |= CKPT_CHECKPOINT_UPGRADE;
			break;

		case 'A':
			/* Hack to start the restart clients */
			ckpt_restart_is_client = 1;
			break;

		case 'a':
			/* array name * */
			ckpt_array_name = optarg;
			break;

		case 'v':
			/* Verbose mode: info about cpr */
			verbose++;
			cpr_debug++;
			break;

		case 'V':
			/* more verbose mode: more info about cpr */
			verbose++;
			cpr_debug += 0x2;
			break;


		default:
			usage();
			exit(2);
		}

		/* allow multiple requests */
		if (subcmd == SUBCMD_RESTART || subcmd == SUBCMD_REMOVE ||
			subcmd == SUBCMD_INFO)
			break;
	}
	if (argc == 1 || pathname == 0 ||
	    (subcmd == SUBCMD_CKPT) && idp == 0) {
		usage();
		exit(2);
	}
	/*
	 * We want to unlimited access to a bunch of resources
	 */
	rl.rlim_cur = CKPT_RLIMIT_MAX;
	rl.rlim_max = CKPT_RLIMIT_MAX;
	for (i = 0; i < RLIM_NLIMITS; i++) {
		switch (i) {
		case RLIMIT_FSIZE:
		case RLIMIT_DATA:
		case RLIMIT_NOFILE:
		case RLIMIT_VMEM:
		case RLIMIT_RSS:
		case RLIMIT_STACK:
#if (RLIMIT_AS != RLIMIT_VMEM)
		case RLIMIT_AS:
#endif
			if (setrlimit(i, &rl)) {
				cnotice("Failed to increase limit on %d (%s)\n", i, STRERR);
				(void) getrlimit(i, &rl);
				cnotice("rlim_cur=%d, rlim_max=%d\n", rl.rlim_cur, rl.rlim_max);
			}
			break;
		default:
			break;
		}
	}
	if (do_subcmd(subcmd, pathname))
		rc = 1;

	/*
	 * take care of the multiple requests
	 */
	if (optind < argc && !strcmp(argv[optind], "-"))
		optind++;
	argc -= optind;
	argv = &argv[optind];

	while (subcmd != SUBCMD_CKPT && argc-- > 0) {
		if (do_subcmd(subcmd, *argv))
			rc = 1;
		argv++;
	}
	if(rc) {
		/* set the errno as the return code, so that the process
		 * that execed this cpr command can see this. Only 8 bits
		 * of the return code could be seen by the parent. For errno
		 * bigger than 255, though unlikly, set it to ECKPT.
		 */
		rc = (errno>255)? ECKPT:errno;
	}
	exit(rc);
}

static int
do_subcmd(int subcmd, char *pathname)
{
	struct direct *direct;
	DIR	*name;
	int rc, lastone = 1;
	char *procset, dir[CPATHLEN];
	int proc_count = 0;
	struct stat sb;

	if (subcmd == SUBCMD_CKPT) {
		u_long type = CKPT_PROCESS; /* default */
		ckpt_id_t id;

		/*
		 * prepare the space if force checkpoint
		 */
		if (force) {
			struct stat buf;

			if (stat(pathname, &buf) == 0) {
				if ((rc = ckpt_remove(pathname)) != 0) {
					printf("Remove statefile %s failed\n",
						pathname);
					return (rc);
				}
			}
		}

		/*
		 * Do regular checkpoint if only one
		 */
		if ((rc = parse_id(&id, &type)) <= 0) {
			if (rc == 0)
				rc = do_checkpoint(id, type, pathname);
			return (rc);
		}
		/*
		 * Checkpoint set. Create one more directory level
		 */
		if (rc = mkdir(pathname, S_IRWXU|S_IRGRP|S_IROTH|S_IXOTH)) {
			if (errno == EEXIST) {
				printf("Checkpoint-Set directory %s exists."
					" Use -f to overwrite\n", pathname);
			}
			else {
				cerror("Failed to mkdir for %s (%s)\n",
					pathname, STRERR);
			}
			return (rc);
		}
		if (chown(pathname, getuid(), getgid())) {
			cerror("Failed to change owner (%s)\n", STRERR);
			return (-1);
		}
		do {
			sprintf(dir, "%s/%lld", pathname, id);
			if (rc = do_checkpoint(id, type, dir))
				return (rc);

			/* We have done the last one. Get out */
			if (lastone == 0)
				return (rc);

		} while ((rc = lastone = parse_id(&id, &type)) >= 0);
		return (rc);
	}

	/*
	 * For restart, remove and stat
	 */
	/* make sure pathname is a dir first */
	if (stat(pathname, &sb) < 0) {
		cerror("Failed to stat pathname %s (%s)\n", pathname, STRERR);
		return -1;
	}
	if(!S_ISDIR(sb.st_mode)) {
		cerror("pathname %s is not a directory\n", pathname);
		return -1;
	}		
	if ((name = opendir(pathname)) == NULL) {
		/*
		 * It's ok when there is no statefile on a node
		 */
		if (ckpt_restart_is_client) {
			ckpt_reach_sync_point(CKPT_RESTART_SYNC_POINT, 0);
			return (0);
		}
		cerror("Failed to open directory %s (%s)\n", pathname, STRERR);
		return (-1);
	}
	/*
	 * figure out how many processes are involved here and close dir
	 * so that no additional open fds are passed down for restart.
	 */
	while ((direct = readdir(name)) != NULL) {
		struct stat buf;

		/* ignore "." and ".." */
		if (!strcmp(direct->d_name, ".") || !strcmp(direct->d_name, ".."))
			continue;

		sprintf(dir, "%s/%s", pathname, direct->d_name);
		if (stat(dir, &buf) == -1) {
			closedir(name);
			cerror("Read CPR statefile %s failed (%s)\n", dir, STRERR);
			return (-1);
		}
		/*
		 * Non-process set case
		 */
		if (!(buf.st_mode & S_IFDIR)) {
			closedir(name);
			return (do_restart_remove_stat(subcmd, pathname));
		}
		proc_count++;
	}
	/*
	 * Now handle the process set request
	 */
	if (proc_count == 0) {
		closedir(name);
		cerror("empty statefile directory %s\n", pathname);
		return (-1);
	}
	if ((procset = malloc(proc_count * CPATHLEN)) == NULL) {
		closedir(name);
		cerror("Failed to allocate mem (%s)\n", STRERR);
		return (-1);
	}
	rewinddir(name);
	while ((direct = readdir(name)) != NULL) {
		/* ignore "." and ".." */
		if (!strcmp(direct->d_name, ".") || !strcmp(direct->d_name, ".."))
			continue;

		sprintf(procset, "%s/%s", pathname, direct->d_name);
		procset += CPATHLEN;
	}
	procset -= proc_count * CPATHLEN;
	closedir(name);
	while (proc_count--) {
		if (do_restart_remove_stat(subcmd, procset))
			return (-1);
		procset += CPATHLEN;
	} 
	return (0);
}

static int
do_checkpoint(ckpt_id_t id, u_long type, char *pathname)
{
	int rc;

	/* we don't want to exec anyone */
	type |= CKPT_TYPE_NOEXEC;

	/* checkpoint array-client only */
	if (ckpt_restart_is_client)
		type |= CKPT_TYPE_ARCLIENT;
	
	if (CKPT_IS_ASH(type))
		printf("Checkpointing id 0x%016llx (type %s) to directory %s\n",
			id, ckpt_type_str(CKPT_REAL_TYPE(type)), pathname);
	else
		printf("Checkpointing id %lld (type %s) to directory %s\n",
			id, ckpt_type_str(CKPT_REAL_TYPE(type)), pathname);

	if ((rc = ckpt_create(pathname, id, type, 0, 0)) != 0) {
		printf("Failed to checkpoint process %lld\n", id);
		return (rc);
	}
	return (0);
}

static int
do_restart_remove_stat(int subcmd, char *path)
{
	int rc = 0;

	switch(subcmd) {
	case SUBCMD_RESTART:
		printf("Restarting processes from directory %s\n", path);
		if (ckpt_restart(path, 0, 0) < 0) {
			printf("Restart %s failed\n", path);
			return (-1);
		}
		break;
	
	case SUBCMD_REMOVE:
		if ((rc = ckpt_remove(path)) != 0) {
			printf("Remove checkpoint statefile %s failed\n", path);
			return (rc);
		}
		break;
	
	case SUBCMD_INFO:
		if (rc = ckpt_info(path))
			printf("Cannot get information on CPR file %s\n", path);
		break;

	default:
		printf("Unknown sub-command %d\n", subcmd);
		break;
	}
	return (rc);
}

struct imp_tbl {
	char *it_name;
	unsigned it_imp;
};

/*
 * This table must be updated as new cpu types are supported
 */
static struct imp_tbl cpu_imp_tbl[] = {
	{ "Unknown CPU type.",  C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
	{ "MIPS R5000 Processor Chip",  C0_MAKE_REVID(C0_IMP_R5000,0,0) },
	{ "MIPS R4650 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4650,0,0) },
	{ "MIPS R4700 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4700,0,0) },
	{ "MIPS R4600 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4600,0,0) },
	{ "MIPS R8000 Processor Chip",  C0_MAKE_REVID(C0_IMP_R8000,0,0) },
	{ "MIPS R12000 Processor Chip", C0_MAKE_REVID(C0_IMP_R12000,0,0) },
	{ "MIPS R10000 Processor Chip", C0_MAKE_REVID(C0_IMP_R10000,0,0) },
	{ "MIPS R6000A Processor Chip", C0_MAKE_REVID(C0_IMP_R6000A,0,0) },
	{ "MIPS R4400 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4400,0,0) },
	{ "MIPS R4000 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4000,0,0) },
};

static char *
ckpt_cpuname(int cputype)
{
	int i;

	cputype &= C0_IMPMASK;	/* only interested in implementation, not rev */

	for (i = 0; i < sizeof(cpu_imp_tbl)/sizeof(struct imp_tbl); i++) {
		if (cputype == cpu_imp_tbl[i].it_imp)
			return (cpu_imp_tbl[i].it_name);
	}
	/*
	 * didn't find cputype
	 */
	return (cpu_imp_tbl[0].it_name);
}

static char *
ckpt_abiname(int abi)
{
	if (abi == -1)
		return ("Unknown");
	else if (ABI_IS_64BIT(abi))
		return("64");
	else if (ABI_IS_IRIX5_N32(abi))
		return("n32");
	else if (ABI_IS_IRIX5(abi))
		return("o32");

	return("Unknown");
}

static int
ckpt_info(char *path)
{
	ckpt_stat_t *sp, *sp_next;
	int rc;

	if ((rc = ckpt_stat(path, &sp)) != 0)
		return (rc);

	printf("\nInformation About Statefile %s (%s):\n",
		path, rev_to_str(sp->cs_revision));
	printf("\n    CPU: %s\n", ckpt_cpuname(sp->cs_cpu));
	printf("\n    Checkpointed with ID %lld and Type %s\n\n",
		sp->cs_id, ckpt_type_str(sp->cs_type));
	while (sp) {
		printf("    Process:\t\t%s\n", sp->cs_psargs);
		printf("    ABI:\t\t%s\n", ckpt_abiname(sp->cs_abi));
		printf("    PID,PPID:\t\t%d,%d\n", sp->cs_pid, sp->cs_ppid);
		printf("    PGRP,SID:\t\t%d,%d\n", sp->cs_pgrp, sp->cs_sid);
		printf("    Working at dir:\t%s\n", sp->cs_cdir);
		printf("    Num of Openfiles:\t%d\n", sp->cs_nfiles);
		printf("    Checkpoint Done at:\t%s\n", ctime(&sp->cs_stat.st_mtime));
		sp_next = sp->cs_next;
		free(sp);
		sp = sp_next;
	}
	return (0);
}

#define	ID_END(c)	((c) == ':' || (c) == ',' || (c) == NULL)
#define	TYPE_END(c)	((c) == ',' || (c) == NULL)

/*
 * Return 1 if we have more id/type pair coming; 0 if no more request; -1 if error
 */
static int
parse_id(ckpt_id_t *id, u_long *type)
{
	extern int atol_type(u_long *, char *);
	char s_id[64];
	u_long type_l;
	int i = 0;

	*type = CKPT_PROCESS;
	while (!ID_END(*idp))
		s_id[i++] = *idp++;
	s_id[i] = 0;

	*id = strtoll(s_id, NULL, 0);
	if (*idp++ == NULL)
		return (0);
	if (*(idp-1) == ',')
		return (1);

	/* prepare for the type */
	i = 0;
	while (!TYPE_END(*idp))
		s_id[i++] = *idp++;
	s_id[i] = 0;

	if (atol_type(&type_l, s_id) == -1) {
		printf("Failed to checkpoint\n");
		return (-1);
	}
	*type |= type_l;	/* bit-add id type */

	if (*idp++ == NULL)
		return (0);
	return (1);
}

static void
usage(void)
{
	/*
	 * internal: -A for checkpoint array-client only on one node
	 */
	printf("Usage: cpr    -c pathname -p id[:type],[id[:type]...] [-fgku]"
		"\t(checkpoint)\n");
	printf("\t      [-j] -r pathname [pathname...]\t\t\t(restart)\n");
	printf("\t      -D pathname [pathname...]\t\t\t\t(delete)\n");
	printf("\t      -i pathname [pathname...]\t\t\t\t(info)\n");
	printf("\n\t      type definitions:\n");
	printf("\t\t\tPID\tfor Unix process ID (PID) (default)\n");
	printf("\t\t\tGID\tfor Unix process group ID\n");
	printf("\t\t\tSID\tfor Unix process session ID\n");
	printf("\t\t\tASH\tfor Irix array session ID\n");
	printf("\t\t\tHID\tfor process hierarchy rooted at PID\n");
	printf("\t\t\tSGP\tfor Irix sproc shared group\n");
}
