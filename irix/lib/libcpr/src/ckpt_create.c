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
#ident "$Revision: 1.103 $"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/errno.h>
#include <sys/syssgi.h>
#include <sys/ckpt_sys.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h"

static int _ckpt_create(const char *, ckpt_id_t, u_long, struct ckpt_args **, size_t);
static int ckpt_main_tasks(ch_t *);
static int ckpt_proctree_list(pid_t, pid_t **, int *);
static int ckpt_build_proctrees(ch_t *);
static int ckpt_verify_sharegroups(ch_t *);
static void ckpt_create_abort(void);
static void ckpt_create_quit(int, siginfo_t *, void *);
static void ckpt_create_cleanup(ch_t *);
static int ckpt_cprobj_init(ch_t *, ckpt_id_t, u_long, const char *);
static void ckpt_ckptobj_destroy(ckpt_obj_t *);
static void ckpt_ckptobj_remove(ckpt_obj_t *, ckpt_obj_t *);
static int ckpt_update_threadlist(ckpt_obj_t *co);
static int ckpt_init_abort_handlers(int);

int cpr_debug;
int pipe_rc = 0;

#ifdef DEBUG
char cpr_file[64];
int cpr_line;
static void ckpt_dumppids(char *, pid_t, int, pid_t *);
#endif

ch_t cpr; 		/* main CPR handle */
int cpr_flags = 0;
int cpr_child_pid = 0;

/* ARGSUSED */
int
ckpt_setup(struct ckpt_args *args[], size_t nargs)
{
	return(0);
} 

static int
ckpt_checkpoint_inplace(u_long type)
{
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
	/* For effective super-user and NOEXEC applications, go ahead */
	return (geteuid() == 0 || (type & CKPT_TYPE_NOEXEC));
#else
	/* Can only do checkpoint directly in running on a 32-bit kernel */
	/* and effective super-user or NOEXEC applications */

	if (geteuid() == 0 || (type & CKPT_TYPE_NOEXEC)) {

		if (sysconf(_SC_KERN_POINTERS) == 32)
			return (1);

		else if (type & CKPT_TYPE_NOEXEC) {
			cerror("abi violation\n");
			return (-1);
		}
	}
	return (0);
#endif
}

/* ARGSUSED */
int
ckpt_create(const char *path, ckpt_id_t id, u_long type, 
						struct ckpt_args *args[],
	size_t nargs)
{
	int stat;
	int inplace, rc;

	/*
	 * Someone has to pay for the effort
	 */
	if (!ckpt_license_valid())
		return (-1);

	bzero(&cpr, sizeof(ch_t));
	/* 
	 * in case of checkpoint array, don't use array daemon
	 * if all the processes are in the same machine.
	 * set pci_flag = CKPT_PCI_LOCALASH to do this.
	 * if cpr -pid:ASH and no -A and only one machine involved in array.
	 */
	cpr.ch_type = type;
	if(CKPT_IS_ASH(type) && !(type&CKPT_TYPE_ARCLIENT) &&
					ckpt_is_ash_local()) {
		cpr.ch_flags |= CKPT_PCI_LOCALASH;
	}
	if (CKPT_IS_ARSERVER(cpr.ch_type, cpr.ch_flags)) {

                if (id == getash()) {
                        /*
                         * This won't work
                         */
                        cerror("Attempting to checkpoint ASH containing checkpointer\n");
                        setoserror(ECKPT);
                        return (-1);
                }
		if (ckpt_run_checkpoint_arserver(path, id, CKPT_REAL_TYPE(type),
		    args, nargs))
			return(-1);
		return(0);
	}
	if ((inplace = ckpt_checkpoint_inplace(type)) < 0)
		return ECKPT;

	if (inplace)
		return (_ckpt_create(path, id, type, args, nargs));

	/*
  	 * set up abort handlers
 	 */
 	if (ckpt_init_abort_handlers(1) < 0)
	 	return (-1);

	IFDEB1(cdebug("executing %s to checkpoint...\n", CPR));
	if ((cpr_child_pid = fork()) == 0) {
                char *argv[12];
                int i = 0;
		char s_id[PSARGSZ];
		
		sprintf(s_id, "%lld:%s", id, ckpt_type_str(type));

		IFDEB1(cdebug("creating child proc %d to ckpt process %s\n",
			getpid(), s_id));

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

		cerror("Cannot execl cpr (%s)\n", STRERR);
		exit(1);
	}
	if (cpr_child_pid < 0) {
		cerror("Failed to fork (%s)\n", STRERR);
		return (-1);
	}

	/* parent waits the child to be done */
again:
	if ((rc = waitpid(cpr_child_pid, &stat, 0)) < 0) {
		if (oserror() == EINTR) {
			setoserror(0);
			goto again;
		}
		/* waitpid could fail for other reasons. It may return
		 * ECHILD, if the process is chkpnt itself. Because this
		 * proc was chkpnt't at waitpid. When restart, it will
		 * restart at waitpid(), and it's child (the real ckpnt proc)
		 * has already gone away.
		 */
		ckpt_init_abort_handlers(0);
		if(oserror() == ECHILD) {
			setoserror(0);
			return 0;
		} else {
			cerror("waitpid failed: return %d, (%s)\n", rc, STRERR);
			return (-1);
		}
	}
	ckpt_init_abort_handlers(0);
	if (WEXITSTATUS(stat) || WIFSIGNALED(stat)) {
		/* The fork'ed child will exec cpr command, we assume cpr
		 * command will return errno code in case of error. 0
		 * otherwise. Set errno the same as cpr command's.
		 *
		 * Because WEXITSTATUS() gets only 8 bits of the return code.
		 * for errno >255, we set it to ECKPT.
		 */
		int err = (WEXITSTATUS(stat)>255)?ECKPT:WEXITSTATUS(stat);
		setoserror(err);
		cerror("ckpt_create child failed:errno %d (%s)\n",err, STRERR);
		return -1;
	}
	return (0);
}

/* ARGSUSED */
static int
_ckpt_create(const char *path, ckpt_id_t id, u_long type, struct ckpt_args *args[],
	size_t nargs)
{
	int rc = 0, stat =0;
	int  is_array_server = 0;

	IFDEB1(cdebug("checkpointing pid 0x%016llx to file %s args %x nargs %d\n",
		id, path, args, nargs));

	if (syssgi(SGI_CKPT_SYS, CKPT_TESTSTUB) < 0) {
		if (oserror() == ENOSYS) {
			cerror("Kernel CPR module (cpr.a) is not installed\n");
		}
		else {
			cerror("Failed to issue syscall CKPT_TESTSTUB (%s)\n",
				STRERR);
		}
		return (-1);
	}

	is_array_server = ((type&CKPT_TYPE_ARCLIENT) !=0);
	type = CKPT_REAL_TYPE(type);

	/*
  	 * set up abort handlers
 	 */
 	if (ckpt_init_abort_handlers(1) < 0)
	 	return (-1);

	if (is_array_server || (cpr_child_pid = fork()) == 0) {
		/* Main path: we ask the child do the real job. 
		 * This will separate cpr changes to the process attributes
		 * such as resource limits, signals, file fds, from the caller.
		 *
		 * For client part of array cpr, we don't fork child to do it. 
		 * Because the master side need to have signal control over 
		 * the pid. 
		 */

		IFDEB1(cdebug("is_array_server %d, child_pid %d\n", 
				is_array_server, cpr_child_pid));
 		if (ckpt_cprobj_init(&cpr, id, type, path) < 0) {
			rc = -1;
			goto child_exit;
		}

 		if ((rc = ckpt_create_proc_list(&cpr)) < 0)
			goto child_exit;

		/* bump the limits we need for checkpoint first
		 * we don't need to restore, becasue this is the child process,
		 * it won't change the caller limits.
		 */
		ckpt_bump_limits();

		/* main task */
		if (rc = ckpt_main_tasks(&cpr)) {
		/*
		 * Wait for the rest of the ASH members on different machines 
		 * to know that we cannot go on like this. And they'd better 
		 * die as well.
		 */
			if (CKPT_IS_GASH(cpr.ch_type, cpr.ch_flags))
				ckpt_reach_sync_point(CKPT_ABORT_SYNC_POINT, 1);

			ckpt_create_abort();

		}

		ckpt_create_cleanup(&cpr);
		ckpt_init_abort_handlers(0);

	child_exit:
		if(rc) {
			/* if there is an error, remember the errno */
			rc = errno;
		}
		/* For client part array cpr, we use the normal return to the 
		 * caller. Otherwise, the child is fork'ed by the parent to
		 * do the real work. Now the work is done, we simply exit.
		 */
		if(is_array_server)
			return rc;
		exit(rc);
	} 

	if (cpr_child_pid < 0) {
		cerror("_ckpt_create: Failed to fork (%s)\n", STRERR);
		return (-1);
	}

	if ((cpr_flags & CKPT_CHECKPOINT_STEP) && (cpr_child_pid >0)) {
		if (blockproc(getpid()) < 0) {
			cerror("Failed to stop at checkpoint step point\n");
			return -1;
		}
		/* we need to unblock our child */
		if(unblockproc(cpr_child_pid) < 0) {
			cerror("Failed to continue checkpoint step point\n");
			return -1;
		}
	}
	/* parent: we need to wait and get child's status */
	/* parent waits the child to be done */
again:
	if ((rc = waitpid(cpr_child_pid, &stat, 0)) < 0) {
		if (oserror() == EINTR) {
			setoserror(0);
			goto again;
		}
		/* waitpid could fail for other reasons. It may return
		 * ECHILD, if the process is chkpnt itself. Because this
		 * proc was chkpnt't at waitpid. When restart, it will
		 * restart at waitpid(), and it's child (the real ckpnt proc)
		 * has already gone away.
		 */
		ckpt_init_abort_handlers(0);
		if(oserror() == ECHILD) {
			setoserror(0);
			return 0;
		} else {
			cerror("waitpid failed: return %d, (%s)\n", rc, STRERR);
			return (-1);
		}
	}
	ckpt_init_abort_handlers(0);
	if (WEXITSTATUS(stat) || WIFSIGNALED(stat)) {
		/* let the caller have the same errno as the fork'ed child */
		/* because WEXITSTATUS() gets only 8 bits of the return code.
		 * for errno >255, we set it to ECKPT.
		 */
		int err = (WEXITSTATUS(stat)>255)?ECKPT:WEXITSTATUS(stat);
		setoserror(err);
		cerror("ckpt_create child failed: errno %d (%s)\n",err, STRERR);
		return -1;
	}
	IFDEB1(cdebug("ckpt_create wait return %d status %d\n", rc, stat));
	return 0;
}

static int
ckpt_main_tasks(ch_t *cp)
{
	int terminate = 0;

	IFDEB1(cdebug("doing main work by pid %d\n", getpid()));

	if (ckpt_stop_procs(cp))
		return (-1);

	if (cpr_flags & CKPT_CHECKPOINT_STEP) {
		ckpt_obj_t *co;
		int nofiles = 0;
		char buf[CPATHLEN];

		FOR_EACH_PROC(cp->ch_first, co) {
			if (ckpt_count_openfiles(co))
				continue;
			nofiles += co->co_nofiles;
			nofiles += ckpt_display_mapfiles(co, 1);
		}
		/*
		 * send out # of openfiles
		 */
		assert(pipe_rc);
		sprintf(buf, "%d", nofiles);
		if (write(pipe_rc, buf, sizeof(buf)) < 0) {
			cerror("Failed to pass up openfile info (%s)\n", STRERR);
			return (-1);
		}
		FOR_EACH_PROC(cp->ch_first, co) {
			ckpt_display_openfiles(co);
			ckpt_display_mapfiles(co, 0);
		}
		/*
		 * sync up with the GUI
		 */
		sprintf(buf, "%s", CKPT_GUI_SYNC_POINT);
		if (write(pipe_rc, buf, sizeof(buf)) < 0) {
			cerror("Failed to pass up openfile info (%s)\n", STRERR);
			return (-1);
		}
		close(pipe_rc);
		printf("Checkpoint preparation is done. Ready to continue\n");
		if (blockproc(getpid()) < 0) {
			cerror("Failed to stop at checkpoint step point\n");
		}
		printf("Continue to checkpoint. Please wait...\n");
	}
#ifdef DEBUG
	{
	ckpt_obj_t *co;
	FOR_EACH_PROC(cp->ch_first, co)
		assert(co->co_prfd >= 0);
	}
#endif
	if (CKPT_IS_GASH(cp->ch_type,cp->ch_flags))
		ckpt_reach_sync_point(CKPT_STOP_SYNC_POINT, 1);

	if (ckpt_verify_sharegroups(cp))
		return (-1);

	if (ckpt_build_proctrees(cp))
		return (-1);

	if (ckpt_create_statefile(cp))
		return (-1);

	if (CKPT_IS_GASH(cp->ch_type,cp->ch_flags))
		ckpt_reach_sync_point(CKPT_DUMP_SYNC_POINT, 1);

	/*
	 * exit: the default action to the original process
	 * The cpr_flags override the .cpr policy
	 */
	if (ckpt_attr_use_default(cp, CKPT_WILL, CKPT_EXIT))
		terminate = 1;
	if (cpr_flags & CKPT_CHECKPOINT_CONT)
		terminate = 0;
	else if (cpr_flags & CKPT_CHECKPOINT_KILL)
		terminate = 1;

	ckpt_continue_procs(cp->ch_first, terminate);

	if (!(cpr_flags & CKPT_NQE))
		printf("Checkpoint done\n");
	return (0);
}

static int
ckpt_find_all_pids(ckpt_id_t id, ckpt_type_t type, pid_t **pidp, int *npids)
{
	pid_t *tpidp;

	switch (type) {
	case CKPT_PROCESS:
		*npids = 1;
		*pidp = (pid_t *)malloc(*npids * sizeof(pid_t));
		**pidp = (pid_t)id;
		break;
			
	case CKPT_PGROUP:
		*npids = (int)syssgi(SGI_GETGRPPID, id, NULL);
		*pidp = (pid_t *)malloc(*npids * sizeof(pid_t));

		if (syssgi(SGI_GETGRPPID, id, *pidp, *npids*sizeof(pid_t)) == -1L) {
			cerror("Failed to get process ids for group %d\n", id);
			return (-1);
		}
		IFDEB1(cdebug("SGI_GETGRPPID: npids=%d\n", *npids));
		break;

	case CKPT_SESSION:
		*npids = (int)syssgi(SGI_GETSESPID, id, NULL);
		*pidp = (pid_t *)malloc(*npids * sizeof(pid_t));

		if (syssgi(SGI_GETSESPID, id, *pidp, *npids*sizeof(pid_t)) == -1L) {
			cerror("Failed to get process ids for session %d\n", id);
			return (-1);
		}
		IFDEB1(cdebug("SGI_GETSESPID: npids=%d\n", *npids));
		break;

	case CKPT_ARSESSION:
		if (ckpt_get_localpids(id, pidp, npids))
			return (-1);
		break;

	case CKPT_HIERARCHY:

		if (ckpt_proctree_list((pid_t)id, pidp, npids) < 0)
			return (-1);

		if ((tpidp = (pid_t *)malloc(((*npids)+1)*sizeof(pid_t))) == NULL) {
			cerror("Failed to alloc memory (%s)\n", STRERR);
			return (-1);
		}
		*tpidp = (pid_t)id;
		if (*npids > 0) {
			bcopy(*pidp, &tpidp[1], (*npids)*sizeof(pid_t));
			free(*pidp);
		}
		*pidp = tpidp;
		(*npids) += 1;

		IFDEB2(ckpt_dumppids("final final", (pid_t)id, *npids, *pidp));

		break;

	case CKPT_SGROUP:
		*npids = (int)syssgi(SGI_CKPT_SYS, CKPT_GETSGPIDS, id, NULL);
		*pidp = (pid_t *)malloc(*npids * sizeof(pid_t));

		if (syssgi(	SGI_CKPT_SYS,
				CKPT_GETSGPIDS,
				id,
				*pidp,
				*npids*sizeof(pid_t)) == -1L) {
			cerror("Cannot get process ids for sproc group %d\n", id);
			return (-1);
		}
		IFDEB1(cdebug("CKPT_GETSGPID: npids=%d\n", *npids));
		break;
	}
	if (*npids == 0) {
		cerror("No process associated with id %lld type %s\n",
			id, ckpt_type_str(type));
		return (-1);
	}
	return (0);
}

static int
ckpt_proc_open(ckpt_obj_t *co, int *error)
{
	char proc[32];
	int procfd;
	prpsinfo_t psinfo;

	assert(co->co_prfd == -1);

	sprintf(proc, "/proc/%d", co->co_pid);
	if ((procfd = open(proc, O_RDWR|O_EXCL)) == -1) {
		*error = oserror();
		if (*error != ENOENT) {
			cerror("Failed to open %s (%s)\n", proc, STRERR);
			return (-1);	
		}
	} else
		return (procfd);
	/*
	 * Got ENOENT, try /proc/pinfo open.  This will succeed where
	 * above fails for zombie procs
	 */
	sprintf(proc, "/proc/pinfo/%d", co->co_pid);
	if ((procfd = open(proc, O_RDONLY)) == -1) {
		/* proc really must not exist */
		*error = oserror();
		return (-1);	
	}
        if (ioctl(procfd, PIOCPSINFO, &psinfo) < 0) {
		*error = oserror();
		if (*error != ENOENT) {
			cerror("failed to get process info on %d (%s)\n", co->co_pid, STRERR);
		}
		close(procfd);
		return (-1);
	}
	if (psinfo.pr_state != SZOMB) {
		*error = ENOENT;
		close(procfd);
		return (-1);
	}
	co->co_flags |= CKPT_CO_ZOPEN;
	return (procfd);
}

static int
ckpt_ckptobj_init(ch_t *cp, pid_t pid, ckpt_obj_t **copp)
{
	ckpt_obj_t *co;
	int rv = 0;
	int error;
	/*
	 * Allocate
	 */
	*copp = co = (ckpt_obj_t *)malloc(sizeof(ckpt_obj_t));
	if (co == NULL) {
		cerror("Failed to malloc (%s)\n", STRERR);
		return (-1);
	}
	bzero(co, sizeof(ckpt_obj_t));
	co->co_psinfo = (prpsinfo_t *)malloc(sizeof(prpsinfo_t));
	if (co->co_psinfo == NULL) {
		cerror("Failed to malloc (%s)\n", STRERR);
		rv = -1;
		goto out;
	}
	co->co_pinfo = (ckpt_pi_t *)malloc(sizeof(ckpt_pi_t));
	if (co->co_pinfo == NULL) {
		cerror("Failed to alloc mem (%s)\n", STRERR);
		rv = -1;
		goto out;
	}
	bzero(co->co_pinfo, sizeof(ckpt_pi_t));
	/*
	 * Init
	 */
	co->co_pid = pid;
	co->co_prfd = -1;
	co->co_next = NULL;
	co->co_parent = NULL;
	co->co_children = NULL;
	co->co_sibling = NULL;
	co->co_nchild = 0;
	co->co_flags = 0;
	co->co_nprops = 0;
	co->co_sap = NULL;

	if ((co->co_prfd = ckpt_proc_open(co, &error)) < 0) {
		rv = (error == ENOENT)? 0 : -1;
		goto out;
	}
	/*
	 * Even though we've got proc open, it can exit out from under us
	 */
	if (ioctl(co->co_prfd, PIOCPSINFO, co->co_psinfo) < 0) {
		if (oserror() == ENOENT) {
			rv = 0;
			goto out;
		}
		cerror("PIOCPSINFO (%s)\n", STRERR);
		rv = -1;
		goto out;
	}
	/*
	 * If we had to do a 'zombie' open, make sure proc is a zombie
	 */
	if ((co->co_flags & CKPT_CO_ZOPEN)&&(co->co_psinfo->pr_state != SZOMB)) {
		cerror("zombie open of non-zombie pid %d\n", pid);
		setoserror(ECKPT);
		rv = -1;
		goto out;
	}
	if (ioctl(co->co_prfd, PIOCCKPTPSINFO, &co->co_pinfo->cp_psi) == -1) {
		if ((oserror() == ENOENT)||(oserror() == ESRCH)) {
			rv = 0;
			goto out;
		}
		cerror("PIOCCKPTPSINFO (%s)\n", STRERR);
		rv = -1;
		goto out;
	}
	co->co_ch = cp;

	co->co_next = cp->ch_first;
	cp->ch_first = co;

	return (0);
out:
	*copp = NULL;
	ckpt_ckptobj_destroy(co);
	return (rv);
}
/*
 * destroy a checkpoint object. *must not* be on the chain!
 */
static void
ckpt_ckptobj_destroy(ckpt_obj_t *co)
{
	assert(co->co_next == NULL);

	if (co->co_prfd != -1)
		close(co->co_prfd);
	if (co->co_pinfo)
		free(co->co_pinfo);
	if (co->co_psinfo)
		free(co->co_psinfo);
	if (co->co_pfd)
		free(co->co_pfd);
	free(co);
}

/*
 * Remove a ckpt object from the chain and destroy it
 */
static void
ckpt_ckptobj_remove(ckpt_obj_t *co, ckpt_obj_t *prev)
{
	ch_t *cp = co->co_ch;

	assert(cp);

	if (prev) {
		prev->co_next = co->co_next;
	} else {
		assert(cp->ch_first == co);

		cp->ch_first = co->co_next;
	}
	co->co_next = NULL;
	ckpt_ckptobj_destroy(co);
}

int
ckpt_create_proc_list(ch_t *cp)
{
	ckpt_obj_t *co;
	pid_t *pidp;
	int i, npids, rc;
	int exited = 0;
	int skipped = 0;

	if (rc = ckpt_find_all_pids(cp->ch_id, cp->ch_type, &pidp, &npids))
		return (rc);

	IFDEB1(cdebug("id %d with type 0x%x includes %d of real pids\n",
		cp->ch_id, cp->ch_type, npids));

	for (i = 0; i < npids; i++) {
		/* don't stop myself; this may happen when type is session */
		if (pidp[i] == get_mypid()) {
			skipped++;
			continue;
		}
		if (ckpt_ckptobj_init(cp, pidp[i], &co) < 0)
			return (-1);

		if (!co) {
			/*
			 * return value of 0 with NULL object pointer
			 * means proc exited.
			 */
			if (pidp[i] == cp->ch_id) {
				
				cerror("Process associated with id %lld type %s does not exist\n",
					cp->ch_id, ckpt_type_str(cp->ch_type));
				setoserror(ESRCH);
				return (-1);
			}
			exited++;
		}
	}
	free(pidp);

	npids -= (skipped + exited);

	cp->ch_nproc = 0;
	FOR_EACH_PROC(cp->ch_first, co) {
		cp->ch_nproc++;

		if ((rc = ckpt_update_threadlist(co)) < 0)
			return (-1);
		if (rc == 1)
			exited++;
	}
#ifdef DEBUG
	assert(cp->ch_nproc == npids);
#endif
	return (exited);
}

/*
 * update the chain of procs.  return > 0 if any change in the list.
 */
int
ckpt_update_proc_list(ch_t *cp)
{
	ckpt_obj_t *co, *prev;
	pid_t *pidp;
	int i, npids, rc;
	int rescan = 0;
	int skipped = 0;
	int exited = 0;
	/*
	 * First remove all objects for procs that exited between open
	 * and stop
	 */
	co = cp->ch_first;
	prev = NULL;

	while (co) {
		if (co->co_flags & CKPT_CO_EXITED) {
#ifdef DEBUG_WAITSYS
			printf("proc %d exited\n", co->co_pid);
#endif
			rescan++;

			ckpt_ckptobj_remove(co, prev);
			if (prev)
				co = prev->co_next;
			else
				co = cp->ch_first;
		} else {
			prev = co;
			co = co->co_next;
		}
	}

	if(cp->ch_first == NULL) {
		/* no target process any more */
		cnotice("ckpt_update_proc_list: no target process\n");
		errno = ESRCH;
		return -1;
	}

	/*
	 * Find active pids
	 */
	if (rc = ckpt_find_all_pids(cp->ch_id, cp->ch_type, &pidp, &npids))
		return (rc);

	IFDEB1(cdebug("id %d with type 0x%x includes %d of real pids\n",
		cp->ch_id, cp->ch_type, npids));

	/*
	 * Create ckpt objects for 'new' procs
	 */
	for (i = 0; i < npids; i++) {

		/* don't stop myself; this may happen when type is session */
		if (pidp[i] == get_mypid()) {
			skipped++;
			continue;
		}
		/* find if we have an existing record */

		for (co = cp->ch_first; co; co = co->co_next) {
			if (co->co_pid == pidp[i]) {
				break;
			}
		}
		if (!co) {


			/* find no record */
			if ((rc = ckpt_ckptobj_init(cp, pidp[i], &co)) < 0)
				return (-1);

			if (rc == 0) {

				rescan++;

				if (!co) {
					/*
					 * return value of 0 with NULL object
					 * pointer means proc exited.
					 */
					if (pidp[i] == cp->ch_id) {
						
						setoserror(ESRCH);
						cerror("Process associated with id %lld type %s does not exist\n",
							cp->ch_id,
							ckpt_type_str(cp->ch_type));
						return (-1);
					}
					exited++;
				}
			} else {
				/*
				 * This was a match on cpr....
				 */
				assert ((rc == 1) && !co);
				skipped++;
			}
		}

	}
	/*
	 * Remove ckpt objects for no-longer active pids
	 */
	co = cp->ch_first;
	prev = NULL;

	while (co) {

		for (i = 0; i < npids; i++) {
			if (co->co_pid == pidp[i])
				break;
		}
		if (i == npids) {
			/* Found an obsolete proc */

			rescan++;

			(void)ckpt_continue_proc(co, 0);

			ckpt_ckptobj_remove(co, prev);

			if (prev)
				co = prev->co_next;
			else
				co = cp->ch_first;
		} else {
			prev = co;
			co = co->co_next;
		}
	}
	free(pidp);

	npids -= (exited + skipped);

	cp->ch_nproc = 0;
	FOR_EACH_PROC(cp->ch_first, co) {
		cp->ch_nproc++;

		if ((rc = ckpt_update_threadlist(co)) < 0)
			return (-1);
		if (rc == 1)
			rescan++;
	}
#ifdef DEBUG
	assert(cp->ch_nproc == npids);
#endif
	return (rescan);
}

#ifdef DEBUG
static void
ckpt_dumppids(char *str, pid_t root, int count, pid_t *pids)
{
	cdebug("<%d>:%s:%d", root, str, count);
	while (--count >= 0)
		cdebug(":%d", *pids++);
	cdebug("\n");
}
#endif

static int
ckpt_proctree_list(pid_t rootpid, pid_t **pidp, int *npids)
{
	int maxchild, nchild;	/* number of children */
	int ncchild;		/* number of childrens children */
	int nichild;		/* number of immediate children */
	pid_t *cpids;		/* children pids */
	pid_t *ccpids;		/* childrens children pids */
	int i;

	maxchild = (int)syssgi(SGI_CKPT_SYS, CKPT_GETCPIDS, rootpid, 0, NULL);
	if (maxchild < 0) {
		if (oserror() == ENOENT) {
			*npids = 0;
			return (0);
		}
		cerror("Failed to find the number of children processes for root %d\n",
			rootpid);
		return (-1);
	}
	if (maxchild == 0) {
		*npids = 0;
		return (0);
	}
	cpids = (pid_t *)malloc(maxchild*sizeof(pid_t));
	if (cpids == NULL) {
		cerror("Failed to alloc memory\n");
		return (-1);
	}
	/*
	 * New children may have forked off, existing children may have
	 * died.
	 *
	 * If current # children is <= previous, then all pids are in our
	 * buffer.
	 * If current # > pre1vious, we didn't get em all.  We'll catch new
	 * forks after stopping the ones we catch this time around
	 */
	if ((nchild = (int)syssgi(SGI_CKPT_SYS, CKPT_GETCPIDS, rootpid, maxchild,
		cpids)) < 0) {
		if (oserror() == ENOENT) {
			*npids = 0;
			free(cpids);
			return (0);
		}
		cerror("Failed to find # of children processes for root %d\n",
			rootpid);
		return (-1);
	}
	if (nchild == 0) {
		*npids = 0;
		free(cpids);
		return (0);
	}
	if (nchild > maxchild)
		nchild = maxchild;

	IFDEB2(ckpt_dumppids("nchild", rootpid, nchild, cpids));

	nichild = nchild;

	for (i = 0; i < nichild; i++) {
		if (ckpt_proctree_list(cpids[i], &ccpids, &ncchild) < 0)
			return (-1);

		if (ncchild != 0) {

			IFDEB2(ckpt_dumppids("ncchild", rootpid, ncchild, ccpids));

			*npids += ncchild;
			cpids = (pid_t *)realloc(cpids,
						(nchild+ncchild)*sizeof(pid_t));
			if (cpids == NULL) {
				cerror("Failed to realloc mem (%s)\n", STRERR);
				return (-1);
			}
			bcopy(ccpids, &cpids[nchild], ncchild*sizeof(pid_t));
			free(ccpids);
			nchild += ncchild;

			IFDEB2(ckpt_dumppids("updated nchild",
				rootpid, nchild, cpids));
		}
	}
	IFDEB2(ckpt_dumppids("final nchild", rootpid, nchild, cpids));

	*npids = nchild;
	*pidp = cpids;

	return (0);
}


#define IS_PARENT(P, C)	(((P)->co_pid)==((C)->co_psinfo->pr_ppid))
#define PARENT_LINK(P,C)	\
			(C)->co_parent = (P);				\
			if ((P)->co_children == NULL) {			\
				(P)->co_children = (C);			\
				(C)->co_sibling = NULL;			\
			} else {					\
				(C)->co_sibling = (P)->co_children;	\
				(P)->co_children = (C);			\
			}

int
ckpt_build_proctrees(ch_t *cp)
{
	ckpt_obj_t *co1, *co2, **treep;
	int ntrees = cp->ch_nproc;	/* max # of trees == # procs */

	for (co1 = cp->ch_first; co1; co1 = co1->co_next) {
		for (co2 = co1->co_next; co2; co2 = co2->co_next) {
			/*
			 * If we find a proc with a parent, decrement the
			 * number of trees.  By def, can't be a tree root if
			 * you have a parent.
			 */
			if (IS_PARENT(co1, co2)) {

				ntrees--;

				PARENT_LINK(co1, co2);

				co1->co_nchild++;

			} else if (IS_PARENT(co2,co1)) {

				ntrees--;

				PARENT_LINK(co2, co1);

				co2->co_nchild++;

			}
		}
	}
	cp->ch_ntrees = ntrees;
	/*
	 * Make a 2nd pass to find tree roots...parent-less procs
	 */
	if ((treep = (ckpt_obj_t **)malloc(ntrees*sizeof(ckpt_obj_t *)))== NULL) {
		cerror("Failed to alloc memory (%s)\n", STRERR);
		return (-1);
	}
	cp->ch_ptrees = treep;

	FOR_EACH_PROC(cp->ch_first, co1) {
		if (co1->co_parent == NULL) {
			*treep++ = co1;
			co1->co_pinfo->cp_flags |= CKPT_PI_ROOT;
		}
	}
	assert(treep == &cp->ch_ptrees[ntrees]);

	return (0);
}
/*
 * Routines for tracking save of share groups
 */
static ckpt_sp_t *sproch = NULL;

/*
 * Increment the number of procs in share group and other share group
 * bookkeepping
 */
int
ckpt_share_inc(ckpt_obj_t *co)
{
	ckpt_sp_t *sptr;

	for (sptr = sproch; sptr; sptr = sptr->sp_next) {
		if (sptr->sp_sproc == co->co_pinfo->cp_psi.ps_shaddr) {
			sptr->sp_proccnt++;
			return (0);
		}
	}
	/*
	 * Haven't seen shaddr yet, add it to list
	 */
	if ((sptr = (ckpt_sp_t *)malloc(sizeof(ckpt_sp_t))) == NULL) {
		cerror("Failed to alloc memory (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Update our idea of how many share members now that all procs are stopped
	 */
	if (ioctl(co->co_prfd, PIOCCKPTPSINFO, &co->co_pinfo->cp_psi) == -1) {
		cerror("PIOCCKPTPSINFO (%s)\n", STRERR);
		free(sptr);
		return (-1);
	}
	sptr->sp_sproc = co->co_pinfo->cp_psi.ps_shaddr;
	sptr->sp_flags = 0;
	sptr->sp_pid = co->co_pid;
	sptr->sp_refcnt = co->co_pinfo->cp_psi.ps_shrefcnt;
	sptr->sp_proccnt = 1;

	sptr->sp_next = sproch;
	sproch = sptr;

	return (0);
}
/*
 * Verify that all share goups have all members checkpointing
 */
int
ckpt_share_check(void)
{
	ckpt_sp_t *sptr;

	for (sptr = sproch; sptr; sptr = sptr->sp_next) {
		if (sptr->sp_refcnt != sptr->sp_proccnt) {
			cerror("not all members of share group"
				"containing pid %d being checkpointed\n",
				sptr->sp_pid);
			setoserror(ECKPT);
			return (-1);
		}
	}
	return (0);
}

static int
ckpt_verify_sharegroups(ch_t *cp)
{
	ckpt_obj_t *co;

	sproch = NULL;
	if (ckpt_resolve_prattach(cp) < 0)
		return (-1);

	FOR_EACH_PROC(cp->ch_first, co) {
		if (co->co_flags & CKPT_CO_ZOPEN)
			continue;

		if (IS_SPROC(co->co_pinfo)) {
			if (ckpt_share_inc(co) < 0)
				return (-1);
		}
		if (IS_SPROC(co->co_pinfo) && IS_SADDR(co->co_pinfo)) {
			if (ckpt_saddr_handled(co) == 0)
				co->co_flags |= CKPT_CO_SADDR;
		}
	}
	return (ckpt_share_check());
}

static int
ckpt_examine_shareflag(ckpt_obj_t *co, int flag)
{
	ckpt_sp_t *sptr;

	for (sptr = sproch; sptr; sptr = sptr->sp_next) {
		if (sptr->sp_sproc == co->co_pinfo->cp_psi.ps_shaddr) {
			if (sptr->sp_flags & flag)
				return (1);
			sptr->sp_flags |= flag;
			return (0);
		}
	}
	/*
	 * Haven't seen it yet...should *not* happen
	 */
	cerror("Unexpected sproc:pid %d\n", co->co_pid);
	setoserror(ECKPT);
	return (-1);
}

int
ckpt_saddr_handled(ckpt_obj_t *co)
{
	return (ckpt_examine_shareflag(co, CKPT_SPROC_SADDR));
}


int
ckpt_pmdefaults_handled(ckpt_obj_t *co)
{
	return (ckpt_examine_shareflag(co, CKPT_SPROC_PMDEFAULTS));
}

int
ckpt_pmodules_handled(ckpt_obj_t *co)
{
	return (ckpt_examine_shareflag(co, CKPT_SPROC_PMODULES));
}

int
ckpt_mlds_handled(ckpt_obj_t *co)
{
	return (ckpt_examine_shareflag(co, CKPT_SPROC_MLDS));
}

int
ckpt_mldset_handled(ckpt_obj_t *co)
{
	return (ckpt_examine_shareflag(co, CKPT_SPROC_MLDSET));
}

static void
ckpt_create_abort(void)
{
	/*
	 * restore all of the running processes before we abort
	 */
	if ((cpr.ch_flags & (CKPT_PCI_ABORT|CKPT_PCI_CREATE)) ==
				(CKPT_PCI_ABORT|CKPT_PCI_CREATE)) {
		if (ckpt_create_remove(cpr.ch_path) < 0)
			printf("Remove checkpoint statefile %s failed\n",
				cpr.ch_path);
	}
	ckpt_continue_procs(cpr.ch_first, 0);
	printf("Checkpoint failed. Restarted all processes back to their "
		"running states\n");
}

static int
ckpt_cprobj_init(ch_t *cp, ckpt_id_t id, u_long type, const char *path)
{
	char *pp;

	cp->ch_id = id;
	cp->ch_type = type;

	if (*path == '/')
		strcpy(cp->ch_path, path);
	else {
		pp = getcwd(NULL, CPATHLEN);
		if (pp == NULL)
			return (-1);
		sprintf(cp->ch_path, "%s/%s", pp, path);
		free(pp);
	}
	if (cpr_flags & CKPT_OPENFILE_DISTRIBUTE)
		cp->ch_flags |= CKPT_PCI_DISTRIBUTE;


	return (0);
}

static sigaction_t saveact[NSIG];
static int interesting_signos[] = { SIGABRT, SIGQUIT, SIGINT};
static void (*sa_action[NSIG])();

static void
ckpt_create_quit(int signo, siginfo_t *sip, void *ptr)
{
	
	/*
	 * restore all of the running processes before we abort
	 */

	/* ckpt_create checks this flag periodicaly, and will
	 * stop ckpt if it is set
	 */
	cpr.ch_flags |= CKPT_PCI_ABORT;
	cnotice("User request to abort checkpoint\n");

	if(cpr_child_pid) {
		/* this is the parent proc, and we got signal. We need
		 * to send this to the child, so that cpr can stop.
		 */
		kill(cpr_child_pid, signo);	
		IFDEB1(cdebug("send signal %d to pid %d\n", 
						signo, cpr_child_pid));
	}
	if (sa_action[signo-1])
		(*sa_action[signo-1])(signo, sip, ptr);
}

static int
ckpt_init_abort_handlers(int set)
{
	int i;
	int signo;
	sigaction_t act, *actp, *savep;

	for (i = 0; i < sizeof(interesting_signos)/sizeof(interesting_signos[0]); i++) {
		signo = interesting_signos[i];

		if (set) {
			savep =  &saveact[signo-1];
			if (sigaction(signo, NULL, savep) < 0) {
				cerror("sigaction failed (%s)\n", STRERR);
				return (-1);
			}
			act = *savep;
			/*
			 * The sa_sigaction & sa_handler fields are
			 * a union overlay...
			 */
			if (savep->sa_flags & SA_SIGINFO) {
				sa_action[signo-1] = savep->sa_sigaction;
				act.sa_handler = NULL;
				act.sa_sigaction = ckpt_create_quit;
			} else {
				sa_action[signo-1] = savep->sa_handler;
				act.sa_sigaction = NULL;
				act.sa_handler = ckpt_create_quit;
			}
			actp = &act;
		} else {
			actp = &saveact[signo-1];
		}
		if (sigaction(signo, actp, NULL) < 0) {
			cerror("sigaction failed (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

static void
ckpt_free_thread(ckpt_obj_t *co)
{
	ckpt_thread_t *thread;

	while (thread = co->co_thread) {

		if (thread->ct_memmap)
                        free(thread->ct_memmap);
		if (thread->ct_mapseg)
			free(thread->ct_mapseg);

		co->co_thread = thread->ct_next;

		free(thread);
	}
}

static void
ckpt_create_cleanup(ch_t *cp)
{
	ckpt_obj_t 	*co = cp->ch_first, *co_next;
	ckpt_flist_t	*fcp = cp->ch_filelist, *fnext;
	ckpt_po_t	*pcp = cp->ch_pipelist, *pnext;
	ckpt_sp_t	*scp = cp->ch_sproclist, *snext;
	ckpt_mldlist_t	*mld = cp->ch_mldlist, *mnext;
	ckpt_pmolist_t	*pmo = cp->ch_pmolist, *onext;
	ckpt_dpl_t	*dpl, *dnext;
	ckpt_prattach_t	*prp = cp->ch_prattach, *prnext;
	ckpt_ul_t	*ulp = cp->ch_ulp, *unext;
	ckpt_pul_t	*pup = cp->ch_pusema, *punext;
	/*
	 * remove all process objects
	 */
	while (co) {
		close(co->co_prfd);
		free(co->co_pinfo);
		free(co->co_psinfo);
		ckpt_free_thread(co);
		ckpt_mldlog_free(co);
		co_next = co->co_next;
		free(co);
		co = co_next;
	}
	/*
	 * Remove the CPR handle
	 */
	while (fcp) {
		fnext = fcp->cl_next;	
		free(fcp);
		fcp = fnext;
	}
	while (pcp) {
		pnext = pcp->cpo_link;
		free(pcp);
		pcp = pnext;
	}
	while (scp) {
		snext = scp->sp_next;
		free(scp);
		scp = snext;
	}
	while (mld) {
		mnext = mld->mld_next;
		free(mld);
		mld = mnext;
	}
	while (pmo) {
		onext = pmo->pmo_next;
		free(pmo);
		pmo = onext;
	}
	dpl = cp->ch_depend.dpl_next;
	while (dpl) {
		dnext = dpl->dpl_next;
		free(dpl);
		dpl = dnext;
	}
	while (prp) {
		prnext = prp->pr_next;
		free(prp);
		prp = prnext;
	}
	while (ulp) {
		unext = ulp->ul_next;
		free(ulp);
		ulp = unext;
	}
	while (pup) {
		punext = pup->pul_next;
		free(pup);
		pup = punext;
	}
	if (cp->ch_ptrees)
		free(cp->ch_ptrees);

	bzero(&cpr, sizeof(ch_t));
	if (pipe_rc)
		close(pipe_rc);
	return;
}

static int
ckpt_thread_insert(ckpt_obj_t *co, tid_t tid, ckpt_thread_t **insert, int *new)
{
	ckpt_thread_t *thread;
	/*
	 * lookup the id
	 */
	for (thread = co->co_thread; thread; thread = thread->ct_next) {
		if (thread->ct_id == tid) {
			/*
			 * already on the list
			 */
			thread->ct_flags |= CKPT_THREAD_VALID;
			return (0);
		}
	}
	*new = 1;

	thread = (ckpt_thread_t *)malloc(sizeof(ckpt_thread_t));
	if (thread == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	thread->ct_id = tid;
	thread->ct_flags = CKPT_THREAD_VALID;
	thread->ct_nmemmap = 0;
	thread->ct_memmap = NULL;
	thread->ct_mapseg = NULL;
	thread->ct_next = *insert;
	*insert = thread;
	insert = &thread->ct_next;
	/* mark the process as not stopped. New thread may popup 
	 * even after the ckpt_stop_one_proc() call.
	 */
	co->co_flags &= ~CKPT_CO_STOPPED;

	return (0);
}

/*
 * build/update list of threads, return:
 *	0 if no new threads
 *	1 if any new
 *	-1 if an error
 */
int
ckpt_update_threadlist(ckpt_obj_t *co)
{
	int anychange = 0;
	struct prthreadctl pt;
	struct prstatus ps;
	ckpt_thread_t **insert;
	ckpt_thread_t *thread;
	ckpt_thread_t *prev;

	if (co->co_flags & (CKPT_CO_ZOMBIE|CKPT_CO_ZOPEN)) {
		if (co->co_thread) {
			ckpt_free_thread(co);
			return (1);
		}
		assert(!co->co_thread);
		return (0);
	}
	/*
	 * Clear all thread valid flags
	 */
	for (thread = co->co_thread; thread; thread = thread->ct_next)
		thread->ct_flags &= ~CKPT_THREAD_VALID;

	insert = &co->co_thread;

	pt.pt_tid = 0;
	pt.pt_cmd = PIOCSTATUS;
	pt.pt_flags = PTFS_ALL | PTFD_GEQ;
	pt.pt_data = (caddr_t) &ps;

	if (ioctl(co->co_prfd, PIOCTHREAD, &pt) < 0) {
		if (oserror() == ENOENT) {
			if (co->co_thread)
				ckpt_free_thread(co);
			return (1);
		}
		cerror("PIOCTHREAD (%s)\n", STRERR);
		return (-1);
	}
	if (ckpt_thread_insert(co, ps.pr_who, insert, &anychange) < 0)
		return (-1);

	while (1) {
		pt.pt_tid = ps.pr_who;
		pt.pt_flags = PTFS_ALL | PTFD_GTR;

		if (ioctl(co->co_prfd, PIOCTHREAD, &pt) < 0) {
			if (oserror() != ENOENT) {
				cerror("PIOCTHREAD (%s)\n", STRERR);
				return (-1);
			}
			/*
			 * end of threads!
			 */
			break;
		}
		if (ckpt_thread_insert(co, ps.pr_who, insert, &anychange) < 0)
			return (-1);
	}
	/*
	 * Remove any threads that no longer exist
	 */
	prev = NULL;
	thread = co->co_thread;

	while (thread) {
		if ((thread->ct_flags & CKPT_THREAD_VALID) == 0) {
			anychange = 1;

			if (prev == NULL) {
				co->co_thread = thread->ct_next;
				free(thread);
				thread = co->co_thread;
			} else {
				prev->ct_next = thread->ct_next;
				free(thread);
				thread = prev->ct_next;
			}
		} else
			thread = thread->ct_next;
	}
	return (anychange);
}
