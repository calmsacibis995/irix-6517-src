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
#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include "ckpt.h" /* for now */

int cpr_debug;

#ifdef DEBUG
char cpr_file[64];
int cpr_line;
#endif

int cpr_flags = 0;

struct strmap {
        int             id;
        char            *s_id;
};

static struct strmap rec_type[] = {
        CKPT_PROCESS,   "PID",          /* control cpr by proc id */
        CKPT_PGROUP,    "GID",          /* by proc group id */
        CKPT_SESSION,   "SID",          /* by session id */
        CKPT_ARSESSION, "ASH",          /* by array session id */
        CKPT_HIERARCHY, "HID",          /* process hierarchy rooted at pid */
        CKPT_SGROUP,    "SGP"           /* share group */
};

char *
ckpt_type_str(ckpt_type_t type)
{
        int i;

        for (i = 0; i < sizeof (rec_type)/sizeof (struct strmap); i++) {
                if (rec_type[i].id == type)
                        return (rec_type[i].s_id);
        }
        setoserror(EINVAL);
        return (NULL);
}


/* ARGSUSED */
int
ckpt_setup(struct ckpt_args *args[], size_t nargs)
{
	return(0);
} 

/* ARGSUSED */
int
ckpt_create(const char *path, ckpt_id_t id, u_long type, struct ckpt_args *args[],
	size_t nargs)
{
	pid_t child;
	int stat;

	if ((child = fork()) == 0) {
                char *argv[12];
                int i = 0;
		char s_id[PSARGSZ];
		
		sprintf(s_id, "%lld:%s", id, ckpt_type_str(type));

                argv[i++] = CPR;
                argv[i++] = "-c";
                argv[i++] = (char *)path;
                argv[i++] = "-p";
                argv[i++] = s_id;
  
                if (cpr_flags & CKPT_CHECKPOINT_UPGRADE)
                        argv[i++] = "-u";
  
                if (cpr_flags & CKPT_CHECKPOINT_KILL)
                        argv[i++] = "-k";
                else if (cpr_flags & CKPT_CHECKPOINT_CONT)
                        argv[i++] = "-g";
  
                argv[i++] = NULL;
  
                execv(CPR, argv);

		exit(1);
	}
	if (child < 0)
		return (-1);

	/* parent waits the child to be done */
again:
	if (waitpid(child, &stat, 0) < 0) {
		if (oserror() == EINTR) {
			setoserror(0);
			goto again;
		}
		return (-1);
	}
	if (WIFEXITED(stat) == 0 || WEXITSTATUS(stat)) {
		setoserror(ECKPT);
		return (-1);
	}
	return (0);
}

/* ARGSUSED */
ckpt_id_t
ckpt_restart(const char *path, struct ckpt_args *args[], size_t nargs)
{
	pid_t child;
	ckpt_id_t id = -1;
	int stat;
	int pfd[2];

	if (nargs != 0) {
		/*
		 * No args supported when forking/execing cpr
		 */
		setoserror(ENOTSUP);
		return (-1);
	}
	/* 
	 * To comply POSIX return (ckpt_id_t) when the caller is not root,
	 * create a pipe to pass up the root process ID from "cpr -r path"
	 */
	pipe(pfd);

	if ((child = fork()) == 0) {
		char pipe_str[16];

		sprintf(pipe_str, "%d", pfd[1]);
		if (cpr_flags & CKPT_RESTART_INTERACTIVE)
			execl(CPR, CPR, "-j", "-P", pipe_str, "-r", path, 0);
		else
			execl(CPR, CPR, "-P", pipe_str, "-r", path, 0);
		exit(1);
	}
	if (child < 0)
		return (-1);

	/* parent waits the child to be done */
again:
	if (waitpid(child, &stat, 0) < 0) {
		if (oserror() == EINTR) {
			setoserror(0);
			goto again;
		}
		return (-1);
	}
	/* error return */
	if (WIFEXITED(stat) != 0 && WEXITSTATUS(stat) != 0) {
		int error = 0;

		read(pfd[0], &error, sizeof(error));
		close(pfd[0]);
		close(pfd[1]);
		setoserror(error);
		return (-1);
	}
	read(pfd[0], &id, sizeof(id));
	close(pfd[0]);
	close(pfd[1]);
	return (id);
}

int
ckpt_remove(const char *path)
{
	int rc;
	pid_t child;
	int stat;

	if ((child = fork()) == 0) {
		execl(CPR, CPR, "-D", path, NULL);
		exit(1);
	}
	if (child < 0) {
                return (-1);
       	}
again:
        if (waitpid(child, &stat, 0) < 0) {
                if (oserror() == EINTR) {
                        setoserror(0);
                        goto again;
                }
                return (-1);
        }
        /* error return */
        if (WIFEXITED(stat) != 0 && WEXITSTATUS(stat) != 0) {
                setoserror(ECKPT);
                return (-1);
        }
	return (0);
}

/* ARGSUSED */
int
ckpt_stat(const char *path, struct ckpt_stat **sp)
{
	setoserror(ENOSYS);
	return (-1);
}
