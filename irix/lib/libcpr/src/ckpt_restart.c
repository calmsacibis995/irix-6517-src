/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.186 $"

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <mutex.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <strings.h>
#include <sched.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/syssgi.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/ckpt_sys.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sysmp.h>
#include <sys/schedctl.h>
#include <sys/prctl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "ckpt.h"
#include "ckpt_internal.h"
#include "ckpt_revision.h"
#ifdef LIBPW_WAR
#include <rpcsvc/ypclnt.h>
#endif
#include <malloc.h>

static int ckpt_restart_one_proc(pid_t, void *, struct ckpt_args *[], size_t);
static int ckpt_restart_proc_index(const char *, struct ckpt_args *[], size_t);
static int ckpt_restore_identity(int, ckpt_pi_t *, pid_t, struct passwd *);
static int ckpt_restore_misc(ckpt_pi_t *, pid_t);
static int ckpt_restore_gid_sid(ckpt_pi_t *);
static int ckpt_init_sighandler(int);
static int ckpt_wait_procs(void);
static int ckpt_init_proclist(int);
static int ckpt_init_shmlist(void);
static void ckpt_rmid_shmlist(void);
static int ckpt_load_librestart(void);
static int ckpt_restore_cred(ckpt_pi_t *, pid_t, struct passwd *);
static ckpt_ta_t *ckpt_refill_properties(void);
static int ckpt_refill_one_proc(ckpt_ta_t **, pid_t);
static int _ckpt_restart(const char *, struct ckpt_args *[], size_t);
static void ckpt_restore_schedmode(ckpt_ta_t *tp);
static int ckpt_wstop_target(ckpt_ta_t *);
static int ckpt_restore_target(ckpt_ta_t *);
static int ckpt_scan_proclist(uint_t state);
static void ckpt_update_proclist_opid(unsigned long, pid_t);
static void ckpt_restore_root(unsigned long, ckpt_pi_t *);
static int ckpt_restore_cwd(ckpt_pi_t *, pid_t);
static ckpt_liblock_t *ckpt_init_liblock(int);
static void ckpt_cleanup_liblock(ckpt_liblock_t *);
static int ckpt_setrun_zombies(void);
static int ckpt_setrun_procs(void);
static void *ckpt_alloc_sharedmem(void *, int, size_t);
static int ckpt_open_proclist(void);
static void ckpt_kill_proclist(int);
static void ckpt_restart_cleanup(void);
static void ckpt_restart_interactive_wait(void);
static int ckpt_open_ta_fds(ckpt_ta_t *tp, pid_t pid);
#ifdef LIBPW_WAR
static int ckpt_libpw_war(void);
#endif
#ifdef DEBUG_RESTART_HANG
static void ckpt_dump_proclist(void);
#endif
static int ckpt_load_defaults(void);

static unsigned long *creates_started = NULL;
static unsigned long *creates_finished = NULL;
static unsigned long *procs_syncd = NULL;
long *interactive_restart = NULL;
ckpt_pl_t *proclist = NULL;
static int cpr_errno;

static jmp_buf jmpbuf;

/*
 * dlsym symbols
 */
#if _MIPS_SIM == _ABI64
#define LIBRESTART      "/usr/lib64/librestart.so"
#else
#define LIBRESTART      "/usr/lib32/librestart.so"
#endif

static void ckpt_gettty_name(void);
static int (*ckpt_librestart_entry_low)(ckpt_ta_t *);
static int (*ckpt_read_thread_low)(ckpt_ta_t *, ckpt_magic_t);
static int (*ckpt_restore_memobj_low)(ckpt_ta_t *, ckpt_magic_t);
static int (*ckpt_restore_prattach_low)(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
static int (*ckpt_restore_pmi_low)(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
static int (*ckpt_restore_depend_low)(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
static int (*ckpt_restore_pusema_low)(cr_t *, ckpt_ta_t *, ckpt_magic_t, int *);
static int (*ckpt_sproc_block_low)(pid_t, pid_t *, unsigned long, unsigned long *);
static int (*ckpt_sproc_unblock_low)(int, unsigned long, pid_t *);
static int (*ckpt_sid_barrier_low)(ckpt_pi_t *, pid_t);
static int (*ckpt_seteuid_low)(ckpt_ta_t *ta, uid_t *, int);
static int (*ckpt_foster_care)(void);
static ckpt_saddr_t **saddrlist_low = NULL;
static ckpt_sid_t **sidlist_low = NULL;

ckpt_saddr_t *saddrlist = NULL;

/*
 * Shared fd/id/dir structs.  Pointer inherited or shared, depending on kind
 * of sproc.  Contents mapped into a shared mem area.
 */
#ifndef DEBUG_FDS
ckpt_sfd_t *sfdlist = NULL;
#else
ckpt_sfd_t **sfdlist_low = NULL;
ckpt_sfd_t *sfdlist = NULL;
#endif
static ckpt_sid_t *sidlist = NULL;
static ckpt_sdir_t *sdirlist = NULL;

struct ckpt_restart cr;
static int arserver = 0;
extern int pipe_rc;

#ifdef RESTGID
static pid_t ckpt_group_leader = 0;
#endif

char *ckpt_tty_name;            /* ttyname of the calling process */

/*
 * Hack for restart to distinguish server vs. client during ARSESSION restart.
 * Only used by /usr/bin/cpr.
 */
int ckpt_restart_is_client = 0;
char *ckpt_array_name = NULL;

static int
ckpt_restart_inplace(void)
{
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
	/* For effective super-user applications, go ahead */
	return (geteuid() == 0);
#else
	/*
	 * 32-bit programs can only restart directly on 32-bit kernels
	 */
	if (geteuid() == 0) {

		if (sysconf(_SC_KERN_POINTERS) == 32)
			return (1);

		else if (cpr_flags & CKPT_RESTART_CPRCMD) {
			/*
			 * This guy *really* wanted to do it dcirecty!
			 */
			cerror("abi violation\n");
			return (-1);
		}
	}
	/*
	 * If we get here, must fork/exec cpr
	 */
	return (0);
#endif
}

static int
ckpt_validate_args(struct ckpt_args *args[], size_t nargs, int inplace)
{
	int i;
	ckpt_arg_callback_t *callback;

	if (nargs == 0)
		return (0);

	if (!inplace) {
		/*
		 * No args supported when fork/exec'ing cpr
		 */
		setoserror(ENOTSUP);
		return (-1);
	}
	for (i = 0; i < nargs; i++) {
		switch (args[i]->ckpt_type) {
		case CKPT_TYPE_CALLBACK:

			callback = (ckpt_arg_callback_t *)args[i]->ckpt_data;

			switch (callback->callback_type) {
			case CKPT_CALLBACK_HIERARCHY:
				break;
			default:
				setoserror(EINVAL);
				return (-1);
			}
			break;
		default:
			setoserror(EINVAL);
			return (-1);
		}
	}
	return (0);
}

static int
ckpt_process_args(struct ckpt_args *args[], size_t nargs)
{
	int i, rv;
	ckpt_arg_callback_t *callback;

	for (i = 0; i < nargs; i++) {

		switch (args[i]->ckpt_type) {
		case CKPT_TYPE_CALLBACK:
			callback = (ckpt_arg_callback_t *) args[i]->ckpt_data;

			rv = (*callback->callback_func)(callback->callback_arg);
			if (rv < 0)
				return (rv);
			break;
		default:
			break;
		}
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
	int inplace;

	if (setjmp(jmpbuf)) {
		(void)ckpt_init_sighandler(-1);
		setoserror(ECKPT);
		return (-1);
	}
	/*
	 * Check please!
	 */
	if (!ckpt_license_valid())
		return (-1);

	if ((inplace = ckpt_restart_inplace()) < 0)
		return (-1);

	if (ckpt_validate_args(args, nargs, inplace))
		return (-1);

	if (inplace) {
		int rc;

		rc = _ckpt_restart(path, args, nargs);
		if (rc&&CKPT_IS_GASH(cr.cr_pci.pci_type,cr.cr_pci.pci_flags)){
			*interactive_restart = -1;
			if(ckpt_restart_is_client) {
				/* let the server know, we can't go on */
				ckpt_reach_sync_point(CKPT_ABORT_SYNC_POINT, 0);
			}
			/* server can only come back from :
			 * ckpt_run_restart_arserver , at this time, we are
			 * sure, all the cpr commands running on the cluster
			 * are either not started or killed.
			 */
		}
		if (rc < 0) {
			if (cpr_errno == 0) {
				assert(0);	/* track these guys down */
				cpr_errno = ECKPT;
			}
			setoserror(cpr_errno);
			/*
			 * Return errno
			 */
			if (pipe_rc != 0) {
		    		write(pipe_rc, &cpr_errno, sizeof(cpr_errno));
				close(pipe_rc);
			}
			return (rc);
		}
		if (pipe_rc != 0 &&
		    write(pipe_rc, &cr.cr_pci.pci_id, sizeof(cr.cr_pci.pci_id)) < 0) {
			cnotice("Failed to pass up the root process ID (%s)\n", STRERR);
		}
		if (pipe_rc)
			close(pipe_rc);
		return (cr.cr_pci.pci_id);
	}
	IFDEB1(cdebug("executing %s to restart...\n", CPR));
	assert(!(cpr_flags & CKPT_RESTART_CPRCMD));
	/* 
	 * To comply POSIX return (ckpt_id_t) when the caller is not root,
	 * create a pipe to pass up the root process ID from "cpr -r path"
	 */
	if (pipe(pfd) < 0) {
		cnotice("Failed to create a pipe for passing root process ID (%s)\n", 
			STRERR);
	}
	if ((child = fork()) == 0) {
		char pipe_str[16];
		IFDEB1(cdebug("creating child proc %d to restart process from %s\n",
			getpid(), path));
		sprintf(pipe_str, "%d", pfd[1]);
		if (cpr_flags & CKPT_RESTART_INTERACTIVE)
			execl(CPR, CPR, "-j", "-P", pipe_str, "-r", path, 0);
		else
			execl(CPR, CPR, "-P", pipe_str, "-r", path, 0);
		cerror("Failed to execl (%s)\n", STRERR);
	}
	if (child < 0) {
		cerror("Failed to fork (%s)\n", STRERR);
		return (-1);
	}
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

static int
_ckpt_restart(const char *path, struct ckpt_args *args[], size_t nargs)
{
	int rc;

	/*
	 * Let's get them out before we fork processes which inherit them and
	 * may flush them several times under the child processes to the
	 * array server.
	 */
	fflush(stdout);
	fflush(stderr);

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
	ckpt_bump_limits();
	ckpt_gettty_name();
	/*
	 * check if we've been submitted to the batch scheduler and, if so,
	 * update cpr_flags to say so
	 */
	if ((rc = ckpt_batch_scheduled()) < 0)
		goto cleanup;

	if (rc == 1)
		cpr_flags |= CKPT_BATCH;

	if (rc = ckpt_load_librestart())
		goto cleanup;

	if ((rc = ckpt_check_directory(path)))
		goto cleanup;

	if (rc = ckpt_init_sighandler(1))
		goto cleanup;

	if ((rc = ckpt_restart_proc_index(path, args, nargs)) < 0)
		goto cleanup;

	if ((!rc)&&((cpr_flags & CKPT_NQE) == 0))
		printf("Process restarted successfully.\n");

	/*
	 * cpr does foster-care if the restart request is interactive
	 */
	if ((cr.cr_pci.pci_flags & CKPT_PCI_INTERACTIVE)&&
	    (cpr_flags & CKPT_RESTART_CPRCMD)) {
		int stat;

		IFDEB1(cdebug("Going to foster care\n"));
		if (arserver)
			while (waitpid(arserver, &stat, 0) != arserver);

		ckpt_init_sighandler(0);
		/*
		 * If caller is cpr, we can go underground
		 */
		if (pipe_rc == 0) {
			rc = ckpt_foster_care();
			/*
			 * No Return
			 */
		}
		if (wait(&stat) < 0)
			rc = -1;
	}
cleanup:
	ckpt_restore_limits();
	ckpt_restart_cleanup();
	return (rc);
}

static int
ckpt_load_librestart(void)
{
	void *handle;
	char *librestart = LIBRESTART;

	if((librestart = getenv("LIBRESTART")) == NULL) {
		librestart = LIBRESTART;
	}
	IFDEB1(cdebug("librestart %s\n", librestart));

	if ((handle = dlopen(librestart, RTLD_NOW)) == NULL) {
		cerror("Failed to open library %s (%s)\n", LIBRESTART,dlerror());
		return (-1);
	}
	ckpt_librestart_entry_low = (int (*)())dlsym(handle, "ckpt_librestart_entry");
	ckpt_read_thread_low = (int (*)())dlsym(handle, "ckpt_read_thread");
	ckpt_restore_memobj_low = (int (*)())dlsym(handle, "ckpt_restore_memobj");
	ckpt_restore_prattach_low = (int (*)())dlsym(handle, "ckpt_restore_prattach");
	ckpt_restore_depend_low = (int (*)())dlsym(handle, "ckpt_restore_depend");
	ckpt_restore_pmi_low = (int (*)())dlsym(handle, "ckpt_restore_pmi");
	ckpt_restore_pusema_low = (int (*)())dlsym(handle, "ckpt_restore_pusema");
	ckpt_sproc_block_low = (int (*)())dlsym(handle, "ckpt_sproc_block");
	ckpt_sproc_unblock_low = (int (*)())dlsym(handle, "ckpt_sproc_unblock");
	ckpt_sid_barrier_low = (int (*)())dlsym(handle, "ckpt_sid_barrier");
	ckpt_seteuid_low = (int (*)())dlsym(handle, "ckpt_seteuid");
	ckpt_foster_care = (int (*)())dlsym(handle, "ckpt_foster_care");
	saddrlist_low = (ckpt_saddr_t **)dlsym(handle, "saddrlist");
	sidlist_low = (ckpt_sid_t **)dlsym(handle, "sidlist");
#ifdef DEBUG_FDS
	sfdlist_low = (ckpt_sfd_t **)dlsym(handle, "sfdlist");
#endif
	assert(ckpt_librestart_entry_low);
	assert(ckpt_read_thread_low);
	assert(ckpt_restore_prattach_low);
	assert(ckpt_restore_memobj_low);
	assert(ckpt_restore_depend_low);
	assert(ckpt_restore_pmi_low);
	assert(ckpt_restore_pusema_low);
	assert(ckpt_sproc_block_low);
	assert(ckpt_sproc_unblock_low);
	assert(ckpt_sid_barrier_low);
	assert(ckpt_seteuid_low);
	assert(ckpt_foster_care);
	assert(saddrlist_low);
	assert(sidlist_low);
#ifdef DEBUG_FDS
	assert(sfdlist_low);
#endif
	ckpt_proc_prop_table_fixup(CKPT_MAGIC_MEMOBJ, ckpt_restore_memobj_low);
	ckpt_proc_prop_table_fixup(CKPT_MAGIC_THREAD, ckpt_read_thread_low);
	ckpt_share_prop_table_fixup(CKPT_MAGIC_DEPEND, ckpt_restore_depend_low);
	ckpt_share_prop_table_fixup(CKPT_MAGIC_PMI, ckpt_restore_pmi_low);
	ckpt_share_prop_table_fixup(CKPT_MAGIC_PRATTACH, ckpt_restore_prattach_low);
	ckpt_share_prop_table_fixup(CKPT_MAGIC_PUSEMA, ckpt_restore_pusema_low);
	return (0);
}

int
ckpt_librestart_entry(ckpt_ta_t *tp)
{
	return (ckpt_librestart_entry_low(tp));
}

int
ckpt_restore_memobj(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	return (ckpt_restore_memobj_low(tp, magic));
}

int
ckpt_restore_prattach(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	return (ckpt_restore_prattach_low(cp, tp, magic, misc));
}

int
ckpt_restore_depend(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	return (ckpt_restore_depend_low(cp, tp, magic, misc));
}

int
ckpt_restore_pmi(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	return (ckpt_restore_pmi_low(cp, tp, magic, misc));
}

int
ckpt_restore_pusema(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	return (ckpt_restore_pusema_low(cp, tp, magic, misc));
}

ckpt_read_thread(ckpt_ta_t *tp, ckpt_magic_t magic)
{
      return (ckpt_read_thread_low(tp, magic));
}


int
ckpt_sproc_block(pid_t client, pid_t *list, unsigned long count, unsigned long *sync)
{
	return (ckpt_sproc_block_low(client, list, count, sync));
}

int
ckpt_sproc_unblock(pid_t ignore, unsigned long count, pid_t *list)
{
	return (ckpt_sproc_unblock_low(ignore, count, list));
}

int 
ckpt_sid_barrier(ckpt_pi_t *pip, pid_t pid)
{
	return(ckpt_sid_barrier_low(pip, pid));
}

int
ckpt_seteuid(ckpt_ta_t *ta, uid_t *uid, int entry)
{
	return (ckpt_seteuid_low(ta, uid, entry));
}

/*
 * Check if the user given statefile is valid for restart
 */
/* ARGSUSED */
int
ckpt_check_directory(const char *path)
{
	struct stat buf;

	assert(geteuid() == 0);

	if (stat(path, &buf) == -1) {
		cerror("Read CPR statefile %s failed (%s)\n", path, STRERR);
		return (-1);
	}
	if (buf.st_uid != 0) {
		cerror("Cannot access statefile %s: permission denied\n", path);
		return (-1);
	}
	if (!(buf.st_mode & S_IFDIR)) {
		setoserror(ENOTDIR);
		printf("File %s is not a CPR statefile directory.\n", path);
		return (-1);
	}
	return (0);
}

/*
 * Using uid and gid to check if a restart/remove is requested by the same 
 * person who created the statefile
 */
int
ckpt_restart_perm_check1(uid_t statef_ruid, gid_t statef_rgid)
{
	uid_t ruid = getuid();

	/*
	 * Only allow the owner and root to restart the process
	 */
	if (statef_ruid == ruid && statef_rgid == getgid() || ruid == 0)
		return (0);

	IFDEB1(cdebug("statefile's ruid=%d vs restarter's ruid=%d\n",
		statef_ruid, getuid()));
	cerror("Non-owner operating on statefile %s: permission denied\n", cr.cr_path);
	setoserror(EPERM);
	return (-1);
}

/*
 * To be further secure, using the credential info to check if a restart/remove 
 * has the right permission 
 */
int
ckpt_restart_perm_check2(ckpt_pi_t *pip, int sfd)
{
	prcred_t *cred = &pip->cp_cred;
	struct stat buf;
	uid_t restart_ruid = getuid();
	gid_t restart_rgid = getgid();

	if (fstat(sfd, &buf) == -1) {
		cerror("Failed to fstat (%s)\n", STRERR);
		return (EACCES);
	}
	/*
	 * Only allow the owner and root to restart the process
	 */
	if (restart_ruid == cred->pr_ruid && restart_rgid == cred->pr_rgid ||
	    restart_ruid == 0)
		return (0);

	IFDEB1(cdebug("restart ruid %d rgid %d cred ruid %d rgid %d\n",
		restart_ruid, restart_rgid, cred->pr_ruid, cred->pr_rgid));

	setoserror(EACCES);
	return (EACCES);
}

int
ckpt_read_treeinfo(cr_t *cp, ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_ci_t ci;
	int sfd = (tp)? tp->ssfd : cp->cr_sfd;

	if (read(sfd, &ci, sizeof(ckpt_ci_t)) < 0) {
		cerror("Failed to alloc memory (%s)\n", STRERR);
		return (-1);
	}
	if (ci.ci_magic != magic) {
		setoserror(EINVAL);
		cerror("Mismatched magic %8.8s (v.s. %8.8s)\n", &(ci.ci_magic), &magic);
		return (-1);
	}
	if ((cp->cr_roots = (pid_t *)malloc(ci.ci_nchild * sizeof(pid_t)))
		== NULL) {
		cerror("Failed to alloc memory (%s)\n", STRERR);
		return (-1);
	}
	if (read(sfd, cp->cr_roots, ci.ci_nchild*sizeof(pid_t)) < 0) {
		cerror("Failed to read treeinfo (%s)\n", STRERR);
		free(cp->cr_roots);
		cp->cr_roots = NULL;
		return (-1);
	}
	return (0);
}

static int
ckpt_restart_proc_index(
	const char *dname,
	struct ckpt_args *args[],
	size_t nargs)
{
	ckpt_ta_t *tarray, *tp;
	int i;
#define	FOR_EACH_TARGET(tp)	\
	for (i = 0, tp = tarray; i < cr.cr_pci.pci_nproc; i++, tp = (tarray+i))

	if (ckpt_load_shareindex(dname))
		return (-1);

	if (ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_HINV, NULL)) {
		free(cr.cr_roots);
		cr.cr_roots = NULL;
		return (-1);
	}
	/*
	 * Check for basc global defaults in attribute file
	 */
	if (ckpt_load_defaults())
		return (-1);
	/*
	 * Read share statefile for the children picture
	 */
	if (ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_CHILDINFO, NULL)) {
		free(cr.cr_roots);
		cr.cr_roots = NULL;
		return (-1);
	}
	/*
	 * Pre-allocate a shared mem proclist to communite with all procs
	 */
	if (ckpt_init_proclist(cr.cr_pci.pci_nproc) < 0) {
		free(cr.cr_roots);
		cr.cr_roots = NULL;
		return (-1);
	}

	/*
	 * Array Restart
	 */
	if (CKPT_IS_GASH(cr.cr_pci.pci_type,cr.cr_pci.pci_flags)) {

		if (!ckpt_restart_is_client) {

			if ((cr.cr_pci.pci_flags & CKPT_PCI_INTERACTIVE) == NULL) {
				/*
				 * regular server
				 */
				IFDEB1(cdebug("RUNNING RESTART SERVER\n"));
				/* The server will not read the statefile any more */
				close(cr.cr_ifd);

				if (ckpt_run_restart_arserver(dname, cr.cr_pci.pci_id))
					return (-1);

				return(0);
			} 
			/*
			 * If the local job was an interactive one, ask
			 * the CPR server to do the restart so that we can get
			 * a controlling tty back.
			 *
			 */
			if ((arserver = fork()) == 0) {
				if (ckpt_run_restart_arserver(cr.cr_path, 
					cr.cr_pci.pci_id))
					exit(1);
				exit(0);
			}
			if (arserver < 0) {
				cerror("Failed to fork (%s)\n", STRERR);
				return (-1);
			}
		} else {
			/*
			 * Check if we are the client (cpr -A) and interactive.
			 * We reject the client request from arrayd for the
			 * local statefile if it was an interactive job.
			 */
			if (cr.cr_pci.pci_flags & CKPT_PCI_INTERACTIVE) {
				IFDEB1(cdebug("return on client interactive job\n"));
				exit(0);
			}
		}
	}

	/*
 	 * set ash for this proc, if was cpr'ed by -ASH, and this proc is not
	 * array cpr server process.
	 */
	if (CKPT_IS_ASH(cr.cr_pci.pci_type) && 
		!CKPT_IS_ARSERVER(cr.cr_pci.pci_type,cr.cr_pci.pci_flags)) {
		/*
		 * Client
		 */
		IFDEB1(cdebug("setash to ASH 0x%016llx\n", cr.cr_pci.pci_id));
		if (newarraysess() != 0) {
			cerror("creating a new array session (%s)\n", STRERR);
			return -1;
		}
		if (setash(cr.cr_pci.pci_id)) {
			cerror("Failed to restore the previous ASH "
				"0x%016llx: %s\n", cr.cr_pci.pci_id, STRERR);
			return -1;
		}
	}

	if (ckpt_init_shmlist() < 0) {
		free(cr.cr_roots);
		cr.cr_roots = NULL;
		return (-1);
	}
	/*
	 * Restart the tree roots and the roots will start their children
	 */
	for (i = 0; i < cr.cr_pci.pci_ntrees; i++) {
		IFDEB1(cdebug("ckpt_restart_proc_index: root %d (ntree=%d nproc=%d)\n",
			*(cr.cr_roots+i), cr.cr_pci.pci_ntrees, cr.cr_pci.pci_nproc));
		if (ckpt_restart_one_proc(*(cr.cr_roots+i), NULL, args, nargs) < 0) {
			free(cr.cr_roots);
			cr.cr_roots = NULL;
			return (-1);
		}
	}
	/*
	 * Working parallel with the restarting processes to get ready for
	 * the final portion of the restart
	 */
	if ((tarray = ckpt_refill_properties()) == NULL) {
		setoserror(ECKPT);
		cerror("Fatal errors: please clean up the partially restarted "
			"processes\n");
		return (-1);
	}
	/*
	 * Wait for remapping to be done
	 */
	if (ckpt_wait_procs())
		goto cleanup;
	/*
	 * Open /proc for all the ready processes
	 */
	if (ckpt_open_proclist())
		goto cleanup;
	/*
	 * Requeue signals and restore context and cwd etc from waitor
	 * for the restarting processes
	 */ 
	FOR_EACH_TARGET(tp) {
		/*
		 * Find out the right pid when supporting non pidfork
		 */
		if ((tp->pindex = ckpt_get_proclist_pindex(tp->pi.cp_pid)) == -1)
			return (-1);
		/* make sure we are talking about the same proc */
		IFDEB2(cdebug("tp=%x pindex=%d opid=%d pid=%d\n", tp, tp->pindex, 
			proclist[tp->pindex].pl_opid, tp->pi.cp_pid));
		assert(proclist[tp->pindex].pl_opid == tp->pi.cp_pid);

		if (ckpt_open_ta_fds(tp, proclist[tp->pindex].pl_opid) < 0)
			goto cleanup;

		if (ckpt_wstop_target(tp)) {
			(void)ckpt_close_ta_fds(tp);
			goto cleanup;
		}
	}
	FOR_EACH_TARGET(tp) {
		if (ckpt_restore_target(tp)) {
			(void)ckpt_close_ta_fds(tp);
			goto cleanup;
		}
	}
	/*
	 * Misc stuff that needs to be done last
	 */
	FOR_EACH_TARGET(tp) {

		ckpt_restore_schedmode(tp);
		/*
		 * Do this here last since it needs to be done after chdir
		 * with root permission.
		 */
		ckpt_restore_root(tp->pindex, &tp->pi);
	}
	/*
	 * unlink files that should be unlinked
	 */
	if (ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_UNLINKED, NULL)) {
		goto cleanup;

	}
	/*
	 * Everyone is done
	 */
	if (!CKPT_IS_ARSERVER(cr.cr_pci.pci_type,cr.cr_pci.pci_flags))
		ckpt_rmid_shmlist();

	if (ckpt_scan_proclist(CKPT_PL_ERR)) {
		ckpt_kill_proclist(0);
		return (-1);
	}
	/*
	 * For ASH report to the server that we are ready to run.
	 *
	 * If restoring local array interactive job, we need to inform
	 * the restart server that we are ready. 
	 */
	if (CKPT_IS_GASH(cr.cr_pci.pci_type,cr.cr_pci.pci_flags)) {
		if (ckpt_restart_is_client)
			ckpt_reach_sync_point(CKPT_RESTART_SYNC_POINT, 1);
		else if (cpr_flags & CKPT_RESTART_INTERACTIVE)
			ckpt_restart_interactive_wait();
	}
	/*
	 * Restart zombies, cancel all generated SIGCLDs, repost
	 * signals, restart all non-zombies
	 */
	if (ckpt_setrun_zombies())
		goto cleanup;

	FOR_EACH_TARGET(tp) {
		if (ckpt_requeue_signals(tp) < 0) {
			goto cleanup;
		}
		(void)ckpt_close_ta_fds(tp);
	}
	/*
	 * Restart all of the stopped processes at the same time
	 */
	if (ckpt_setrun_procs())
		goto cleanup;

	/* in case of restarting processes by -ASH, the caller's ash has been
	 * changed by calling newarraysess(). We need to assign a different
	 * ash to the caller.
	 */
	if (CKPT_IS_ASH(cr.cr_pci.pci_type) &&
		!CKPT_IS_ARSERVER(cr.cr_pci.pci_type,cr.cr_pci.pci_flags)) {
		if (newarraysess() != 0) {
			cnotice("creating a new array session\n");
		}
	}
	free(cr.cr_roots);
	cr.cr_roots = NULL;
	free(tarray);
	close(cr.cr_ifd);
	close(cr.cr_sfd);
	close(cr.cr_rsfd);
	close(cr.cr_ssfd);
	return (0);

cleanup:
	/* XXX may want to close all prfds */
	cerror("Cleaning up the failed restart\n");
	cr.cr_pci.pci_flags |= CKPT_PCI_ABORT;
	ckpt_kill_proclist(0);
	(void)ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_UNLINKED, NULL);
	free(cr.cr_roots);
	cr.cr_roots = NULL;
	free(tarray);
	close(cr.cr_ifd);
	close(cr.cr_sfd);
	close(cr.cr_rsfd);
	close(cr.cr_ssfd);
	return (-1);
}

int
ckpt_load_shareindex(const char *dname)
{
	char file[CPATHLEN];
	ckpt_magic_t magic = CKPT_MAGIC_INDEX;
	size_t size;
	int pfd[2];

	bzero(&cr, sizeof(cr_t));
	/*
	 * Open share index and state files
	 */
	sprintf(file, "%s/%s.%s", dname, STATEF_SHARE, STATEF_INDEX);
	if ((cr.cr_ifd = open(file, O_RDONLY)) < 0) {
		cerror("Failed to open index %s (%s)\n", file, STRERR);
		return (-1);
	}
	sprintf(file, "%s/%s.%s", dname, STATEF_SHARE, STATEF_STATE);
	if ((cr.cr_sfd = open(file, O_RDONLY)) < 0) {
		cerror("Failed to open index %s (%s)\n", file, STRERR);
		close(cr.cr_ifd);
		return (-1);
	}
	if (pipe(pfd) < 0) {
		cerror("Failed to open status pipe (%s)\n", STRERR);
		close(cr.cr_ifd);
		close(cr.cr_sfd);
		return (-1);
	}
	cr.cr_rsfd = pfd[0];
	cr.cr_ssfd = pfd[1];

	if (read(cr.cr_ifd, &cr.cr_pci, sizeof(ckpt_pci_t)) < 0) {
		cerror("Failed to read per-cpr info (%s)\n", STRERR);
		close(cr.cr_ifd);
		close(cr.cr_sfd);
		close(cr.cr_rsfd);
		close(cr.cr_ssfd);
		return (-1);
	}
	if (cr.cr_pci.pci_magic != magic) {
		setoserror(EINVAL);
	        cerror("Incorrect index magic %8.8s vs. %8.8s (%s)\n",
	                &cr.cr_pci.pci_magic, &magic, STRERR);
		close(cr.cr_ifd);
		close(cr.cr_sfd);
		close(cr.cr_rsfd);
		close(cr.cr_ssfd);
		return (-1);
	}
	if (ckpt_revision_check(cr.cr_pci.pci_revision) < 0) {
		setoserror(EINVAL);
		cerror("Unsupported statefile revision %s\n",
			rev_to_str(cr.cr_pci.pci_revision));
		close(cr.cr_ifd);
		close(cr.cr_sfd);
		close(cr.cr_rsfd);
		close(cr.cr_ssfd);
		return (-1);
	}
	strcpy(cr.cr_path, dname);
	cr.cr_waitor = getpid();
	cr.cr_ruid = getuid();

	if (ckpt_restart_perm_check1(cr.cr_pci.pci_ruid, cr.cr_pci.pci_rgid)) {
		close(cr.cr_ifd);
		close(cr.cr_sfd);
		close(cr.cr_rsfd);
		close(cr.cr_ssfd);
		return (-1);
	}
	/* Corrupted statefile */
	if (cr.cr_pci.pci_prop_count == 0) {
		off_t cur, count;

		cnotice("Processing a corrupted statefile\n");
		cur = lseek(cr.cr_ifd, 0, SEEK_CUR);
		count = lseek(cr.cr_ifd, 0, SEEK_END);
		count -= sizeof(ckpt_pci_t);
		count /= sizeof(ckpt_prop_t);
		cr.cr_pci.pci_prop_count = (int)count;
		lseek(cr.cr_ifd, cur, SEEK_SET);
	}
	size = sizeof(ckpt_prop_t) * cr.cr_pci.pci_prop_count;
	if ((cr.cr_index = (ckpt_prop_t *)malloc( size)) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		close(cr.cr_ifd);
		close(cr.cr_sfd);
		close(cr.cr_rsfd);
		close(cr.cr_ssfd);
		return (-1);
	}
	if (read(cr.cr_ifd, cr.cr_index, size) < 0) {
		cerror("Read error (%s)\n", STRERR);
		free(cr.cr_index);
		cr.cr_index = NULL;
		close(cr.cr_ifd);
		close(cr.cr_sfd);
		close(cr.cr_rsfd);
		close(cr.cr_ssfd);
		return (-1);
	}
	return (0);
}
static int
ckpt_load_defaults(void)
{
	ch_t ch;
	/* determine cacheability */
	if (ckpt_allow_uncached())
		cr.cr_pci.pci_flags |= CKPT_PCI_UNCACHEABLE;

	/*
	 * Evaluate four possibilities of interactive restart request
	 */
	if (cpr_flags & CKPT_RESTART_INTERACTIVE) {
		if (!(cr.cr_pci.pci_flags & CKPT_PCI_INTERACTIVE)) {
			cnotice("Cannot restart interactively because the original"
				"process was not interactive\n");
		}
	} else {
		if (cr.cr_pci.pci_flags & CKPT_PCI_INTERACTIVE)
			cr.cr_pci.pci_flags &= ~CKPT_PCI_INTERACTIVE;
	}
	/*
	 * Check if user wants to do any-fork instead of pidfork;
	 * fake a ch_t to restore cwd if requested
	 */
	ch.ch_id = cr.cr_pci.pci_id;
	ch.ch_type = cr.cr_pci.pci_type;
	if (!ckpt_attr_use_default(&ch, CKPT_FORK, CKPT_FORKORIG)) {
		IFDEB1(cdebug("User selected any pid to restart process\n"));
		cr.cr_pci.pci_flags |= CKPT_PCI_FORKANY;
	}
	/*
	 * Get other restart attributes
	 */
	if (ckpt_attr_use_default(&ch, CKPT_CDIR, CKPT_REPLACE)) {
		IFDEB1(cdebug("User selected default cwd\n"));
		cr.cr_pci.pci_flags |= CKPT_PCI_DEFAULTCWD;
	}
	if (ckpt_attr_use_default(&ch, CKPT_RDIR, CKPT_REPLACE)) {
		IFDEB1(cdebug("User selected default root\n"));
		cr.cr_pci.pci_flags |= CKPT_PCI_DEFAULTROOT;
	}
	return (0);
}

static int
ckpt_refill_one_proc(ckpt_ta_t **tp, pid_t pid)
{
	ckpt_ta_t *curp = (*tp)++;
	int i;

	/*
	 * Fill in proc info about me and my children
	 */
	if (ckpt_load_target_info(curp, pid)) {
		ckpt_close_ta_fds(curp);
		return (-1);
	}
	ckpt_close_ta_fds(curp);

	for (i = 0; i < curp->nchild; i++) {
		if (ckpt_refill_one_proc(tp, curp->cpid[i]))
			return (-1);
	}
	return (0);
}

static ckpt_ta_t *
ckpt_refill_properties(void)
{
	ckpt_ta_t *tarray;
	ckpt_ta_t *tp;
	int np = cr.cr_pci.pci_nproc;
	int i;

	/*
	 * Alloc spaces for all targargs; the space will be inherited by the
	 * children process and deallocated in their space when cpr remap
	 * happens.
	 */
	IFDEB1(cdebug("Waitor recollects its properties\n"));
	if ((tp = tarray = (ckpt_ta_t *)malloc(sizeof(ckpt_ta_t)*np)) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (NULL);
	}
	bzero(tarray, sizeof(ckpt_ta_t)*np);
	for (i = 0; i < cr.cr_pci.pci_ntrees; i++) {
		if (ckpt_refill_one_proc(&tp, *(cr.cr_roots+i)))
			return (NULL);
	}
	return (tarray);
}

static int
ckpt_init_proclist(int nprocs)
{
	size_t size;
	/*
	 * Set up process list in a shared memory object in the low mem.
	 *
	 * make it big enough for 3 counters and the proc list
	 */
	size = (4*sizeof(unsigned long *))+(nprocs * sizeof(ckpt_pl_t));
	assert(size < getpagesize());
	creates_started = (unsigned long *)ckpt_alloc_sharedmem(CKPT_PLRESERVE,
						         MAP_FIXED|MAP_SHARED,
						 	 getpagesize());
	if (creates_started == NULL) {
		cerror("Failed to map memory (%s)\n", STRERR);
		return (-1);
	}
	bzero(creates_started, size);
	creates_finished = &creates_started[1];
	procs_syncd = &creates_started[2];
	interactive_restart =  (long *)&creates_started[3];
	*interactive_restart = 0;
	proclist = (ckpt_pl_t *)&creates_started[4];
	return (0);
}


static int
ckpt_init_shmlist(void)
{
	ckpt_shmlist_t *shmlist;

	shmlist = (ckpt_shmlist_t *)ckpt_alloc_sharedmem(CKPT_SHMRESERVE,
						         MAP_FIXED|MAP_SHARED,
						 	 getpagesize());
	if (shmlist == NULL)
		return (-1);

	shmlist->cshm_count = 0;
	shmlist->cshm_map =
	    (struct ckpt_shmmap_s *)((char *)shmlist + sizeof(ckpt_shmlist_t));
	return (0);
}
/*
 * Remove any sysV shm marked for removal
 */
static void
ckpt_rmid_shmlist(void)
{
	ckpt_shmlist_t *shmlist= (ckpt_shmlist_t *)CKPT_SHMRESERVE;
	int i;

	for (i = 0; i < shmlist->cshm_count; i++) {
		if (shmlist->cshm_map[i].rmid)
			(void)shmctl(shmlist->cshm_map[i].shmid, IPC_RMID);
	}
}

void
ckpt_update_proclist_pid(unsigned long pindex, pid_t pid)
{
	assert(pindex <= *creates_started);
	assert(proclist[pindex].pl_pid == 0);

	proclist[pindex].pl_pid = pid;
}

static void
ckpt_update_proclist_opid(unsigned long pindex, pid_t pid)
{
	assert(pindex <= *creates_started);
	assert(proclist[pindex].pl_opid == 0);

	proclist[pindex].pl_opid = pid;
}

void
ckpt_update_proclist_states(unsigned long pindex, uint_t update)
{
	assert(pindex <= *creates_started);

	proclist[pindex].pl_states |= update;
	if (update == CKPT_PL_ERR)
		proclist[pindex].pl_errno = oserror();
}

uint_t
ckpt_fetch_proclist_states(unsigned long pindex)
{
	assert(pindex <= *creates_started);

	return (proclist[pindex].pl_states);
}


static int
ckpt_proclist_sync(unsigned long pindex)
{
	unsigned long idx;

	idx = add_then_test(procs_syncd, 1);
	if (idx != cr.cr_pci.pci_nproc) {
		if (blockproc(get_mypid()) < 0) {
			cerror("blockproc (%s)\n", STRERR);
			return (-1);
		}
		return (0);
	}
	*procs_syncd = 0;

	for (idx = 0; idx < cr.cr_pci.pci_nproc; idx++) {
		if (idx == pindex )
			continue;

		if (unblockproc(proclist[idx].pl_pid) < 0) {
			cerror("unblockproc (%s)\n", STRERR);
			return (-1);
		}
	}
	return (1);
}

int
ckpt_get_proclist_pindex(pid_t pid)
{
	ckpt_pl_t *pl = proclist;
	int i;

	for (i = 0; i < *creates_started; i++) {
		IFDEB1(cdebug("i = %d looking for pid %d (found %d)\n",
			i, pid, pl->pl_opid));
		if (pl->pl_opid == pid)
			return (i);
		pl++;
	}
	setoserror(ECKPT);
	cerror("No pindex in the proclist. Abort.\n");
	return (-1);
}

ckpt_pl_t *
ckpt_get_proclist_entry(pid_t pid)
{
	ckpt_pl_t *pl = proclist;
	int i;

	for (i = 0; i < *creates_started; i++) {
		/*
		IFDEB1(cdebug("i = %d looking for pid %d (found %d)\n",
			i, pid, pl->pl_opid));
		*/
		if (pl->pl_opid == pid)
			return (pl);
		pl++;
	}
	setoserror(ECKPT);
	cerror("No pindex in the proclist. Abort.\n");
	return (NULL);
}

static int
ckpt_proclist_all_in_state(uint_t state)
{
	ckpt_pl_t *pl = proclist;
	int i;

	for (i = 0; i < *creates_started; i++) {
		if ((pl->pl_states & state) == NULL)
			return (0);
		pl++;
	}
	return (1);
}

static int
ckpt_scan_proclist(uint_t state)
{
	ckpt_pl_t *pl = proclist;
	int i;

	for (i = 0; i < *creates_started; i++) {
		if (pl->pl_states & state) {
			if (state == CKPT_PL_ERR)
				return ((pl->pl_errno)? pl->pl_errno : ECKPT);
			return (1);
		}
		pl++;
	}
	return (0);
}

static int
ckpt_open_proclist(void)
{
	char procpath[32];
	ckpt_pl_t *pl = proclist;
	int i;

	for (i = 0; i < *creates_started; i++) {
		assert(pl->pl_states & CKPT_PL_DONE);

		sprintf(procpath, "/proc/%d", pl->pl_pid);
		if ((pl->pl_prfd = open(procpath, O_RDWR)) < 0) {
			cerror("Failed to open /proc (%s)\n", STRERR);
			return (-1);
		}
		pl++;
	}
	return (0);
}

static int
ckpt_start_zombies(void)
{
	ckpt_pl_t *pl;
	int i;
	ckpt_psi_t zinfo;
	int zcount = 0;			/* zombie count */
	int xcount = 0;			/* exit count */
	/*
	 * 1. start all zombies
	 * 2. use /proc to decide when all zombies have hit exit
	 */
	for (pl = proclist, i = 0; i < *creates_started; i++, pl++) {
		if (pl->pl_states & CKPT_PL_ZOMBIE) {

			if (unblockproc(pl->pl_pid) < 0) {
				cerror("Failed to rerun zombie %d (%s)\n",
					pl->pl_pid, STRERR);
				return (-1);
			}
			zcount++;
		}
	}
	while (xcount < zcount) {

		for (pl = proclist, i = 0; i < *creates_started; i++, pl++) {
			if ((pl->pl_states & CKPT_PL_ZOMBIE)&&(pl->pl_prfd >= 0)) {
				/*
				 * Wait for child to terminate...
				 */
				if (ioctl(pl->pl_prfd, PIOCCKPTPSINFO, &zinfo) == 0) {
					if ((zinfo.ps_state == SZOMB)&&
					    (zinfo.ps_ckptflag&CKPT_PROC_EXIT)) {
						close(pl->pl_prfd);
						pl->pl_prfd = -1;
						xcount++;
					}
				} else if (oserror() == ENOENT) {
#ifdef DEBUG_WAITSYS
					printf("zombie early exit %d\n", pl->pl_pid);
#endif
					assert(0);
					close(pl->pl_prfd);
					pl->pl_prfd = -1;
					xcount++;
				} else {
					cerror("ioctl PIOCSTATUS failed (%s)\n", STRERR);
					return (-1);
				}
			}
		}
		if (xcount < zcount)
			sginap(0);
	}
	return (0);
}

static int
ckpt_start_proclist(void)
{
	ckpt_pl_t *pl;
	int i;
	ulong_t prflags;

	prflags = PR_RLC|PR_CKF;

	for (pl = proclist, i = 0; i < *creates_started; i++, pl++) {
		if (!(pl->pl_states & CKPT_PL_ZOMBIE)) {
			/*
			 * This can fail if someone hits target with SIGKILL
			 * That failure mode results in EBUSY or ENOENT
			 */
			if (ioctl(pl->pl_prfd, PIOCSET, &prflags)  < 0) {
				if (oserror() != ENOENT && oserror() != EBUSY) {
					cerror("Failed to rerun process %d (%s)\n",
						pl->pl_pid, STRERR);
					close(pl->pl_prfd);
					return (-1);
				}
			}
			close(pl->pl_prfd);
		}
	}
	return (0);
}
/*
 * Terminate any processes that we created.  Post SIGKILL, then set block count
 * to 0 in case proc is blocked.  Post signal first so proc dies before can
 * return to user mode
 */
static void
ckpt_kill_proclist(int force)
{
	ckpt_pl_t *pl = proclist;
	int i;

	for (i = 0; i < *creates_started; i++) {
		if ((pl->pl_states & (CKPT_PL_CREATE|CKPT_PL_DONE))||
		    (force && (pl->pl_pid != 0))) {
			kill(pl->pl_pid, SIGKILL);
			setblockproccnt(pl->pl_pid, 0);
		}
		pl++;
	}
}
#ifdef DEBUG_RESTART_HANG
static void
ckpt_dump_proclist(void)
{
	ckpt_pl_t *pl = proclist;
	int i;

	fprintf(stderr, "Dump proclist:satarted %d:finished %d\n",
		*creates_started, *creates_finished);

	for (i = 0; i < *creates_started; i++) {
		fprintf(stderr, "Entry %d:pid %d:opid %d:state %x:",
		    		i, pl->pl_pid, pl->pl_opid, pl->pl_states);
		if (pl->pl_states & CKPT_PL_CREATE)
			fprintf(stderr, "create ");
		if (pl->pl_states & CKPT_PL_DONE)
			fprintf(stderr, "done ");
		if (pl->pl_states & CKPT_PL_ERR)
			fprintf(stderr, "err ");
		if (pl->pl_states & CKPT_PL_FIRSTSP)
			fprintf(stderr, "firstsp ");
		if (pl->pl_states & CKPT_PL_ZOMBIE)
			fprintf(stderr, "zombie ");
		fprintf(stderr, "\n");
		pl++;
	}
}
#endif
static void *
ckpt_alloc_sharedmem(void *vaddr, int flags, size_t len)
{
	int zfd;
	void *ptr;

	zfd = open("/dev/zero", O_RDWR);
	if (zfd < 0) {
		cerror("Failed to open /dev/zero (%s)\n", STRERR);
		return (NULL);
	}
        ptr = mmap(vaddr, len, PROT_READ|PROT_WRITE, flags, zfd, 0);
	if (ptr == MAP_FAILED) {
		cerror("Failed to mmap (%s)\n", STRERR);
		CKPT_CLOSE_AND_RETURN(zfd, NULL);
	}
	close(zfd);
	return (ptr);
}
/*
 * Restore schedmode only after all targets have been restored
 */
static void
ckpt_restore_schedmode(ckpt_ta_t *tp)
{
	int prfd = proclist[tp->pindex].pl_prfd;

	if ((IS_SPROC(&tp->pi)) &&
	    (tp->pi.cp_schedmode)  &&
	    (tp->pi.cp_psi.ps_shflags & CKPT_PS_MASTER)) {
		if (ioctl(prfd, PIOCCKPTSCHED, &tp->pi.cp_schedmode) < 0) {
			cnotice("Failed to restore schedmode for %d (%s)\n",
				proclist[tp->pindex].pl_pid, STRERR);
		}
	}
}

static int
ckpt_wstop_target(ckpt_ta_t *tp)
{
	pid_t pid = proclist[tp->pindex].pl_pid;
	int prfd = proclist[tp->pindex].pl_prfd;

	IFDEB1(cdebug("waitor restoring proc %d (%s)\n", pid, tp->procpath));

	if (tp->pi.cp_stat == SZOMB) {
		/*
		 * the zombie will block itself just prior to
		 * the restart return call.  leave it there until all procs
		 * check in.  don't want it generating SIGCLD until it's
		 * parent is restored.
		 */
		return (0);
	}
	/*
	 * Target will have requested a stop on itself in restartreturn
	 * restartreturn is a full restore syscall, so once targets
	 * in the kernel, we can diddle at will with it's reg set
	 */
	if (ioctl(prfd, PIOCWSTOP, NULL) < 0) {
		cerror("stop process %d (%s)\n", pid, STRERR);
		return (-1);
	}
	IFDEB1(cdebug("The resumed process is stopped\n"));

	return (0);
}

static int
ckpt_restore_target(ckpt_ta_t *tp)
{
	int prfd = proclist[tp->pindex].pl_prfd;

	if (tp->pi.cp_stat == SZOMB) {
		/*
		 * the zombie will block itself just prior to
		 * the restart return call.  leave it there until all procs
		 * check in.  don't want it generating SIGCLD until it's
		 * parent is restored.
		 */
		return (0);
	}
	/*
	 * After signals are reposted, target may die!
	 * So use ioctl interface (restart_ioctl) that filters ENOENT errors
	 */
	/*
	 * Restore hwperf
	 */
	if (tp->pi.cp_psi.ps_flags & CKPT_HWPERF) {
		hwperf_profevctrarg_t	arg;
		hwperf_profevctrargex_t argex;

		arg.hwp_evctrargs = tp->pi.cp_hwpctl;
		arg.hwp_ovflw_sig = tp->pi.cp_hwpctlaux.hwp_aux_sig;
		bcopy(	tp->pi.cp_hwpctlaux.hwp_aux_freq,
			arg.hwp_ovflw_freq,
			sizeof(arg.hwp_ovflw_freq));
		argex.hwp_args = &arg;
		argex.hwp_aux = &tp->pi.cp_hwpaux;

		if (restart_ioctl(prfd, PIOCENEVCTRSEX, &argex) < 0) {
			cerror("restore counters (%s)\n", STRERR);
			return (-1);
		}
	}
	if (ckpt_read_proc_property(tp, CKPT_MAGIC_CTXINFO))
		return (-1);

	return (0);
}

static void
ckpt_become_target(void *arg, int forked)
{
	ckpt_ta_t *tp = (ckpt_ta_t *)arg;
	pid_t mypid;
	int i;
	int prfd;
	int minfd = tp->pi.cp_maxfd + tp->pi.cp_npfds + 1;
	char status = 0;
	struct passwd *pwent;
	uid_t ruid = tp->pi.cp_cred.pr_ruid;

	/*
	 * getpw* calls cache open sockets. We have to call this function
	 * before we close all the fds in ckpt_close_inherited_fds().
 	 * getpwuid also may open new file fds, and cach them.
	 */
	if (saddrlist && saddrlist->saddr_liblock)
		ckpt_acquire_mutex(saddrlist->saddr_liblock, get_mypid());
	if ((pwent = getpwuid(ruid)) == NULL) {
		cerror("Cannot locate passwd entry for uid %d errno %d\n",
								ruid, errno);
		if (saddrlist && saddrlist->saddr_liblock)
			ckpt_release_mutex(saddrlist->saddr_liblock, 
								get_mypid());
		goto err_exit;
	}
	endpwent();
	if (saddrlist && saddrlist->saddr_liblock)
		ckpt_release_mutex(saddrlist->saddr_liblock, get_mypid());

#ifdef DEBUG_RESTART_HANG_XXX
	(void)syssgi(SGI_CKPT_SYS, CKPT_TARGET);
#endif
	ckpt_bump_limits();
	/*
	 * We have inherited our parents statefile fds.  We may not
	 * want these and might need to close them.
	 *
	 * If we're a share member with our parent and we're sharing fds,
	 * we don't close, because doing so would close them for our parent
	 * too and they will get close when our parent is done wth them.
	 * Otherwise, we have our own 'copy' of the fds, so close them.
	 *
	 * We can safely reference parents targ args:
	 * -if we have a different address space (fork on non-saddr sproc)
	 *  then we have our own copy in our address space, thus no cotention.
	 * -if we're a shared addr sproc, the parents args will be around
	 *  until we release vm in librestart.o, and that can't happen untilr
	 *  this process gets into librestrat.so AND
	 *
	 *  parent will not be monkeying with the value of the fields we are
	 *  going to look at.
	 */
	mypid = get_mypid();

	ckpt_update_proclist_pid(tp->pindex, mypid);
	ckpt_update_proclist_opid(tp->pindex, tp->pi.cp_pid);
	ckpt_update_proclist_states(tp->pindex, CKPT_PL_CREATE);

	if (ckpt_close_inherited_fds(tp, &cr, mypid, forked) < 0) {
		(void)test_then_add(creates_finished, 1);
		goto err_exit;
	}
	if (ckpt_restore_gid_sid(&tp->pi)) {
		cerror("failed to restore identity for process %d\n", mypid);
		(void)test_then_add(creates_finished, 1);
		goto err_exit;
	}

	if(tp->pi.cp_psi.ps_flags &CKPT_PMO) {
		IFDEB1(cdebug("setup PMOs \n"));
		if (ckpt_numa_init(tp) < 0)
			goto err_exit;
		if (ckpt_read_proc_property(tp, CKPT_MAGIC_MLD))
			goto err_exit;
		if (ckpt_read_proc_property(tp, CKPT_MAGIC_MLDSET))
			goto err_exit;
		if (ckpt_read_proc_property(tp, CKPT_MAGIC_PMODULE))
			goto err_exit;
		if (ckpt_read_proc_property(tp, CKPT_MAGIC_PMDEFAULT))
			goto err_exit;

	}
	/*
	 * Handle any children we're supposed to have
	 */
	for (i = 0; i < tp->nchild; i++) {
		char file[CPATHLEN];

		sprintf(file, "%s/%d.%s", cr.cr_path, tp->cpid[i],
			STATEF_STATE);

		if (ckpt_restart_one_proc(tp->cpid[i],
					  tp->pi.cp_psi.ps_shaddr,
					  NULL,
					  0)) {
			(void)test_then_add(creates_finished, 1);
			goto err_exit;
		}
	}
	/*
	 * Increment completed forks *after* any new ones are counted
	 */
	(void)test_then_add(creates_finished, 1);
	if (write(tp->cprfd, &status, 1) < 0) {
		cerror("write status failure (%s)\n", STRERR);
		goto err_exit;
	}
	/*
	 * Now that everyone else is happy, let's take care of 
	 * ourself
	 */
	sprintf(tp->procpath, "/proc/%d", mypid);
	if ((prfd = open(tp->procpath, O_RDWR)) < 0) {
		cerror("failed to open file %s (%s)\n", tp->procpath, STRERR);
		goto err_exit;
	}
	if (prfd < minfd && ckpt_move_fd(&prfd, minfd))
		goto err_exit;

	if (ckpt_get_private_fds(tp, &cr) < 0)
		goto err_exit;
	/*
	 * barrier for shared fd sprocs...make sure all statefile and /proc fds
	 * have been opened and moved before we start restoring fds
	 */
	if (ckpt_sfd_sync(&tp->pi) < 0)
		goto err_exit;
	/*
	 * Restore files and fds, a 5 step process:
	 * 1st - restore files that were dumped on behalf of target process
	 * 2nd - sync up with all other restores to insure all files in place
	 * 3rd - restore fds that are 2nd opens, dups or inherits of files
	 *       from 1st step
	 * 4th - sync up one more time to insure all fds in place
	 * Memory mapped files handled a bit differently:
	 * 1st - restore files that were dumped on behalf of target process
	 * 2nd - sync up with all other restores to insure all files in place
	 * 3rd - remap address space.  Since this will remove our memory:
	 * 4th - waiter process will sync up, then
	 * 5th - waiter proc removes 'unlinked' files
	 */
	/*
	 * restore specfiles and pipes before identity is changed
	 */
	/*
	 * This barrier is needed *only* in case we've got /dev/usema
	 * activity...I'd like to have checkpoint set a flag on
	 * when we should do the barrier TBD KSB
	 */
	ckpt_proclist_sync(tp->pindex);
	if (ckpt_read_proc_property(tp, CKPT_MAGIC_OPENSPECS))
		goto err_exit;

	if (ckpt_read_proc_property(tp, CKPT_MAGIC_OPENPIPES) < 0)
		goto err_exit;

	if (ckpt_proclist_sync(tp->pindex) < 0)
		goto err_exit;

	if (ckpt_restore_pipefds(tp) < 0)
		goto err_exit;

	/* change to the original user identity to do the restore */

	if (ckpt_restore_identity(prfd, &tp->pi, mypid, pwent)) {
		cerror("failed to restore identity for process %d\n", mypid);
		goto err_exit;
	}
#ifdef LIBPW_WAR
	/*
	 * Need to close dangling socket fd(s)
	 */
	if (ckpt_libpw_war() < 0) {
		setoserror(ECKPT);
		goto err_exit;
	}
#endif
	close(prfd);

	if (ckpt_read_proc_property(tp, CKPT_MAGIC_OPENFILES))
		goto err_exit;

	if (ckpt_proclist_sync(tp->pindex) < 0)
		goto err_exit;

	if (ckpt_read_proc_property(tp, CKPT_MAGIC_OTHER_OFDS) < 0)
		goto err_exit;

	if (ckpt_proclist_sync(tp->pindex) < 0)
		goto err_exit;

	ckpt_pipe_cleanup(tp);
	/*
	 * Restore mmap files...normally would need a sync between shared
	 * fd sprocs to insure that no collision between restoring fds
	 * and fds temporarily opened for mmapping.  However, the 
	 * proclist sync effectively handles that.
	 */
	if (ckpt_read_proc_property(tp, CKPT_MAGIC_MAPFILE))
		goto err_exit;

	/*
	 * Restore sigstate.  No signals from here until restoration
	 * complete
	 */
	if (ckpt_restore_sigstate(&tp->sighdr) < 0)
		goto err_exit;
	/*
	 * Sync up again to insure all files required for mapping have been
	 * restored
	 *
	 * Note that shared fd sprocs need a sync here to insure that all
	 * fds have been restored before mmapping begins.  The file sync is
	 * a superset of that, so the sproc sync is effectively handled
	 * also
	 */
	if (ckpt_proclist_sync(tp->pindex) < 0)
		goto err_exit;

	if (ckpt_restore_misc(&tp->pi, mypid) < 0)
		goto err_exit;
	
	if (ckpt_read_share_property(&cr, tp, CKPT_MAGIC_MLDLINK, (int *)&mypid))
		goto err_exit;
	/*
	 * Restore unattached sys V shared mem segments
	 */
	if (ckpt_read_proc_property(tp, CKPT_MAGIC_SHM))
		goto err_exit;
	/*
	 * The following jumps us ino the special low mem segment
	 * to do remaining restoration and won't come back unless there's
	 * an error.
	 */
	IFDEB1(cdebug("Enter restart ...\n"));
	(void)ckpt_librestart_entry(tp);
err_exit:
	ckpt_update_proclist_states(tp->pindex, CKPT_PL_ERR);
	(void)write(tp->cprfd, &status, 1);
	kill(mypid, SIGKILL);
	/* no return */
	exit (1);
}

/*
 * Clear any inherited sproc data structs
 */
static void
ckpt_cleanup_sharegroup(void)
{
	ckpt_saddr_t *psaddr;

	if (saddrlist) {
		psaddr = saddrlist;
		saddrlist = NULL;
		*saddrlist_low = NULL;

		ckpt_cleanup_liblock(psaddr->saddr_liblock);
		(void)munmap(psaddr, psaddr->saddr_size);
	}
	if (sfdlist) {
		(void)munmap(sfdlist, sfdlist->sfd_size);
		sfdlist = NULL;
#ifdef DEBUG_FDS
		*sfdlist_low = NULL;
#endif
	}
	if (sidlist) {
		(void)munmap(sidlist, sidlist->sid_size);
		sidlist = NULL;
		*sidlist_low = NULL;
	}
	if (sdirlist) {
		(void)munmap(sdirlist, sdirlist->sdir_size);
		sdirlist = NULL;
	}
}

static int
ckpt_init_sharegroup(ckpt_ta_t *tp)
{
	size_t len;
	/*
	 * Init sproc counts, lists, etc.
	 */
	ckpt_cleanup_sharegroup();	/* clear any inherited sproc info */
	/*
	 * shared address space mgmt
	 *
	 * Data structures in private memory (only seen by PR_SADDR members)
	 * in the reserved address range right above the stack (it
	 * needs to remain after releasing other vm.)
	 *
	 * The number of procs is bounded by the share group refcnt.
	 * We can stash 4k pids in a 16k page.  If we ever exhaust that
	 * (unlikely) will need a more clever scheme.
	 */
	len = sizeof(ckpt_saddr_t) + tp->pi.cp_psi.ps_shrefcnt * sizeof(pid_t);

	if (len > getpagesize()) {
		setoserror(ECKPT);
		cerror("Share group too large\n");
		return (-1);
	}
	saddrlist = (ckpt_saddr_t *)ckpt_alloc_sharedmem(CKPT_SADDRRSRV,
						        MAP_FIXED|MAP_PRIVATE,
						 	len);
	if (saddrlist == NULL)
		return (-1);

	saddrlist->saddr_liblock = NULL;
	saddrlist->saddr_count = 0;
	saddrlist->saddr_sync = 0;
	saddrlist->saddr_size = len;
	saddrlist->saddr_pid = (pid_t *)((char *)saddrlist + sizeof(ckpt_saddr_t));

	if (IS_SADDR(&tp->pi)) {
		ckpt_update_proclist_states(tp->pindex, CKPT_PL_FIRSTSP);
		saddrlist->saddr_count++;
		saddrlist->saddr_pid[0] = get_mypid();
	}
	*saddrlist_low = saddrlist;
	/*
	 * shared fd mgmt
	 *
	 * Put this in shared mem (may not be sharing addr space) and does not
	 * need to persist after releasing vm.
	 */
	len = sizeof(ckpt_sfd_t) + tp->pi.cp_psi.ps_shrefcnt * sizeof(pid_t);

#ifdef DEBUG_FDS
	sfdlist = (ckpt_sfd_t *)ckpt_alloc_sharedmem(	CKPT_SFDRESERVE,
							MAP_FIXED|MAP_SHARED,
							len);
#else
	sfdlist = (ckpt_sfd_t *)ckpt_alloc_sharedmem(NULL, MAP_SHARED, len);
#endif
	if (sfdlist == NULL)
		return (-1);

	sfdlist->sfd_count = 0;
	sfdlist->sfd_sync = 0;
	sfdlist->sfd_size = len;
	sfdlist->sfd_pid = (pid_t *)((char *)sfdlist + sizeof(ckpt_sfd_t));

	if (IS_SFDS(&tp->pi)) {
		sfdlist->sfd_count++;
		sfdlist->sfd_pid[0] = get_mypid();
	}
#ifdef DEBUG_FDS
	*sfdlist_low = sfdlist;
#endif
	/*
	 * shared id mgmt
	 *
	 * Put this in shared mem (may not be sharing addr space) and does not
	 * need to persist after releasing vm.
	 */
	len = sizeof(ckpt_sid_t) + tp->pi.cp_psi.ps_shrefcnt * sizeof(pid_t);

	sidlist = (ckpt_sid_t *)ckpt_alloc_sharedmem(	CKPT_SIDRESERVE,
							MAP_FIXED|MAP_SHARED,
							len);
	if (sidlist == NULL)
		return (-1);

	CKPT_MUTEX_INIT(&sidlist->sid_uidlock);
	sidlist->sid_count = 0;
	sidlist->sid_sync = 0;
	sidlist->sid_size = len;
	sidlist->sid_assigned = 0;
	sidlist->sid_pid = (pid_t *)((char *)sidlist + sizeof(ckpt_sid_t));

	if (IS_SID(&tp->pi)) {
		sidlist->sid_count++;
		sidlist->sid_pid[0] = get_mypid();
	}
	*sidlist_low = sidlist;
	/*
	 * shared dir mgmt
	 *
	 * Put this in shared mem (may not be sharing addr space) and does not
	 * need to persist after releasing vm.
	 *
	 * Will be accessed by share members to sync on cwd, by waiter procs
	 * to sync on root dir.
	 */
	len = sizeof(ckpt_sdir_t) + tp->pi.cp_psi.ps_shrefcnt * sizeof(pid_t);

	sdirlist = (ckpt_sdir_t *)ckpt_alloc_sharedmem(NULL, MAP_SHARED, len);
	if (sdirlist == NULL)
		return (-1);

	sdirlist->sdir_count = 0;
	sdirlist->sdir_csync = 0;
	sdirlist->sdir_size = len;
	sdirlist->sdir_pid = (pid_t *)((char *)sdirlist + sizeof(ckpt_sdir_t));

	if (IS_SDIR(&tp->pi)) {
		sdirlist->sdir_count++;
		sdirlist->sdir_pid[0] = get_mypid();
	}
	return (0);
}

/* ARGSUSED */
static void
ckpt_become_sproc_target(void *arg, size_t stklen)
{
	/*
	 * Wait for shared list updates
	 */
	(void)blockproc(getpid());
	ckpt_become_target(arg, 0);
}

/* ARGSUSED */
static void
ckpt_just_exit(void *arg, size_t stklen)
{
	(void)prctl(PR_SETEXITSIG, 0);
	exit(0);
}
/* ARGSUSED */
static void
ckpt_become_sharegroup(void *arg, size_t stklen)
{
	ckpt_ta_t *tp = (ckpt_ta_t *)arg;

	if (ckpt_init_sharegroup(tp) < 0)
		exit(1);

	ckpt_become_target(tp, 1);
	/*
	 * No return
	 */
	assert(0);

	cerror("unexpected return from ckpt_become_target\n");
	exit(1);
}

/*
 * Create a new share group
 */
static pid_t
ckpt_create_sharegroup(ckpt_ta_t *tp, pid_t requested)
{
	pid_t child;
	struct rlimit stacklim;
	size_t stksz;
	/*
	 * We may not be creating the 'original' sproc.  If it's share
	 * mask doesn't contain PR_SALL, then this must've been sproc'd
	 * off
	 */
	if ((tp->pi.cp_psi.ps_shmask & PR_SALL)!=PR_SALL) {

		if (getrlimit(RLIMIT_STACK, &stacklim) < 0) {
			cerror("getrlimit (%s)\n", STRERR);
			return (-1);
		}
		stksz = min(16*getpagesize(), stacklim.rlim_cur);

		child = fork();

		if (child < 0)
			return (-1);
		/*
		 * wait for forked child to prevent zombie
		 */
		if (child > 0) {
			if (waitpid(child, NULL, 0) < 0) {
				cerror("waitpid (%s)\n", STRERR);
				return (-1);
			}
			return (0);
		}
		child = pidsprocsp(	ckpt_become_sharegroup,
				    	tp->pi.cp_psi.ps_shmask,
					tp,
					NULL,
					stksz,
					requested);
		if (child < 0 && (cr.cr_pci.pci_flags & CKPT_PCI_FORKANY))
			child = sprocsp(ckpt_become_sharegroup,
					tp->pi.cp_psi.ps_shmask,
					tp,
					NULL,
					stksz);
		if (child < 0) {
			cerror("Failed to sproc (%s)\n", STRERR);
			exit(1);
		}
		exit(0);
	} 
	if (tp->pi.cp_psi.ps_shrefcnt == 1) {
		/*
		 * Corner case..may be only remaining proc in share group.
		 * Which means we're not going to call sproc when creating
		 * children.
		 * Forking will not give us the shared address struct in the
		 * kernel.  For that case, we need to sproc something and 
		 * let that terminate.
		 */
		if ((tp->pi.cp_psi.ps_shmask & PR_SALL)==PR_SALL) {

			child = (pid_t)syssgi(SGI_CKPT_SYS, CKPT_PIDFORK, requested);
			/*
			 * If failed to pidfork but user doesn't care the orig pid
			 */
			if (child < 0 && (cr.cr_pci.pci_flags & CKPT_PCI_FORKANY))
				child = fork();
			/*
			 * If either parent or error, return now
			 */
			if (child != 0)
				return (child);

			tp->pi.cp_psi.ps_shmask |= PR_NOLIBC;
			tp->pi.cp_psi.ps_shmask &= ~PR_BLOCK;

			if ((child = sprocsp(	ckpt_just_exit,
					    	tp->pi.cp_psi.ps_shmask,
						NULL,
						NULL,
						4*getpagesize())) < 0) {
				cerror("sproc (%s)\n", STRERR);
				return (-1);
			}
			if (waitpid(child, NULL, 0) < 0) {
				cerror("waitpid (%s)\n", STRERR);
				return (-1);
			}
		}
	} else {
		child = (pid_t)syssgi(SGI_CKPT_SYS, CKPT_PIDFORK, requested);

		if (child < 0 && (cr.cr_pci.pci_flags & CKPT_PCI_FORKANY))
			child = fork();
		/*
		 * If either parent or error, return now
		 */
		if (child != 0)
			return (child);
	}
	ckpt_become_sharegroup(tp, 0);
	/*
	 * no return
	 */
	assert(0);

	cerror("unexpected return from ckpt_become_sharegp\n");
	return (-1);
}

static int
ckpt_create_target(
	ckpt_ta_t *tp,
	void *shaddr,
	struct ckpt_args *args[],
	size_t nargs)
{
	pid_t child;
	pid_t requested = 0;
	struct rlimit stacklim;
	size_t stksz;
	unsigned long saddridx;
	unsigned long sfdidx;
	unsigned long sididx;
	unsigned long sdiridx;
	int close_statefiles = 1;
	char status = 0;
	/*
	 * Create the process by either fork or sproc.  In either case,
	 * parent returns and child executes ckpt_become_target
	 */
	requested = tp->pi.cp_pid;

	tp->pindex = test_then_add(creates_started, 1);
	/*
	 * If it's not an sproc, fork it.
	 */
	if (!IS_SPROC(&tp->pi)) {

		child = (pid_t)syssgi(SGI_CKPT_SYS, CKPT_PIDFORK, requested);
		/*
		 * If we failed to pidfork and user doesn't care the orig. pid
		 */
		if (child < 0 && (cr.cr_pci.pci_flags & CKPT_PCI_FORKANY))
			child = fork();
		if (child == 0) {
			if (nargs) {
				if (ckpt_process_args(args, nargs)  < 0)
					goto err_return;
			}
			ckpt_cleanup_sharegroup();
			ckpt_become_target(tp, 1);
			/*
			 * no return
			 */
		}

	} else if (tp->pi.cp_psi.ps_shaddr != shaddr) {
		/*
		 * It's an sproc, but not in parents share group, which means
		 * it was initially forked, then it called sproc.
		 */
		child = ckpt_create_sharegroup(tp, requested);

	} else {
		if (getrlimit(RLIMIT_STACK, &stacklim) < 0) {
			cerror("getrlimit (%s)\n", STRERR);
			goto err_return;
		}
		stksz = min(16*getpagesize(), stacklim.rlim_cur);
		/*
		 * update shared counts now *before* creating
		 * target, so they lead the sync values
		 */
		/*
		 * If sharing address space, need to provide mutual exclusion
		 * for the C library.  We do this here rather than when
		 * creating share group because this is only needed if at least
		 * 2 procs are sharing.
		 */
		if (IS_SADDR(&tp->pi)) {

			if (saddrlist->saddr_liblock == NULL) {

				saddrlist->saddr_liblock =
					ckpt_init_liblock(tp->pi.cp_psi.ps_shrefcnt);
				if (saddrlist->saddr_liblock == NULL)
					goto err_return;
			}
			if ((saddridx = test_then_add(&saddrlist->saddr_count, 1)) == 0)
				ckpt_update_proclist_states(tp->pindex,
					CKPT_PL_FIRSTSP);
		}
		if (IS_SFDS(&tp->pi)) {
			sfdidx = test_then_add(&sfdlist->sfd_count, 1);
			close_statefiles = 0;
		}
		if (IS_SID(&tp->pi)) {
			sididx = test_then_add(&sidlist->sid_count, 1);
		}
		if (IS_SDIR(&tp->pi)) {
			sdiridx = test_then_add(&sdirlist->sdir_count, 1);
		}
		/*
		 * Share everything original sproc was sharing.  PR_NOLIBC
		 * so we don't end up with arena to clean up...this means,
		 * though, that we have to do our own libc mutual exclusion
		 */
		tp->pi.cp_psi.ps_shmask |= PR_NOLIBC;
		tp->pi.cp_psi.ps_shmask &= ~PR_BLOCK;

		child = pidsprocsp(	ckpt_become_sproc_target,
					tp->pi.cp_psi.ps_shmask,
					tp,
					NULL,
					stksz,
					requested);

		if (child < 0 && (cr.cr_pci.pci_flags & CKPT_PCI_FORKANY))
			child = sprocsp(ckpt_become_sproc_target,
					tp->pi.cp_psi.ps_shmask,
					tp,
					NULL,
					stksz);
		if (IS_SADDR(&tp->pi))
			saddrlist->saddr_pid[saddridx] = child;
		if (IS_SFDS(&tp->pi))
			sfdlist->sfd_pid[sfdidx] = child;
		if (IS_SID(&tp->pi))
			sidlist->sid_pid[sididx] = child;
		if (IS_SDIR(&tp->pi))
			sdirlist->sdir_pid[sdiridx] = child;
		/*
		 * Signal that shaeed lists are ready
		 */
		(void)unblockproc(child);
	}
	if (close_statefiles) {
		/*
		 * We need to close our reference to the targets statefiles
		 * unless we're sprocs sharing fds.
		 */
		if (ckpt_close_ta_fds(tp) < 0) 
			return (-1);
	}
	if (child > 0)
		return (0);
	
	/*
	 * Error
	 */
	cerror("Cannot create a process with pid %d (%s)\n", requested, STRERR);
err_return:
	ckpt_update_proclist_states(tp->pindex, CKPT_PL_ERR);
	(void)test_then_add(creates_finished, 1);
	(void)write(tp->cprfd, &status, 1);
	return (-1);
}
static int
ckpt_restart_one_proc(
	pid_t pid,
	void *shaddr,
	struct ckpt_args *args[],
	size_t nargs)
{
	ckpt_ta_t *tp;
	struct stat sb;

	IFDEB1(cdebug("restarting for pid %d\n", pid));
	/*
	 * malloc a buffer, rather than alloc'ing on stack, for the target
	 * process arguments, since sproc case will still ref this buffer
	 * after this procedure returns
	 */
	if ((tp = (ckpt_ta_t *)malloc(sizeof(ckpt_ta_t))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	if (ckpt_load_target_info(tp, pid))
		return (-1);
	/*
	 * Statefile spoof check
	 */
	if (cr.cr_pci.pci_revision >= CKPT_REVISION_12) {
		if (fstat(tp->sfd, &sb) < 0) {
			cerror("stat failed (%s)\n", STRERR);
			return (-1);
		}
		if (((sb.st_mode & S_ISUID) == 0)||(sb.st_uid != 0)) {
			cerror("Incorrect modes on statefile\n");
			setoserror(ECKPT);
			return (-1);
		}
	}
	/*
	 * check for old pthreads
	 */
	if ((cr.cr_pci.pci_revision < CKPT_REVISION_12)&&
	    (tp->pi.cp_psi.ps_shmask & PR_PTHREAD)) {
		cerror("Cannot restart pthreads from previous release\n");
		setoserror(ECKPT);
		return (-1);
	}
	IFDEB1(cdebug("about to issue ckpt_create_target\n"));
	if (ckpt_create_target(tp, shaddr, args, nargs) < 0 ) {
		return (-1);
	}
	return (0);
}

int
ckpt_read_childinfo(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_ci_t cinfo;

	if (read(tp->sfd, &cinfo, sizeof(ckpt_ci_t)) < 0) {
		cerror("read (%s)\n", STRERR);
		return (-1);
	}
	if (cinfo.ci_magic != magic) {
		setoserror(EINVAL);
	        cerror("incorrect cinfo magic %8.8s (vs. %8.8s)\n",
	                &cinfo.ci_magic, &magic);
	        return (-1);
	}
	if ((tp->nchild = cinfo.ci_nchild) == 0)
		return (0);

	if ((tp->cpid = (pid_t *)malloc(cinfo.ci_nchild*sizeof(pid_t)))
		== NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->sfd, tp->cpid, cinfo.ci_nchild*sizeof(pid_t)) < 0) {
		cerror("Failed to read pids (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}

int
ckpt_read_procinfo(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	int minfd;
	/*
	 * read CPR main header to prepare for changing identity
	 */
	if (ckpt_rxlate_procinfo(cr.cr_pci.pci_revision, tp, magic, &tp->pi) < 0)
	        return (-1);

	/*
	 * move the statefile and indexfile fds out of the way of coming proc
	 * move at least 100 fds apart. This is to avoid the getpwuid fd
	 * conflict.
	 */
	minfd = tp->pi.cp_maxfd + tp->pi.cp_npfds + 100;

	if (minfd > 10000) {
		/*
		 * Bogus
		 */
		cerror("Preposterous number of open files (%d) or pipes (%d)\n",
			tp->pi.cp_maxfd, tp->pi.cp_npfds);
		setoserror(ECKPT);
		return (-1);
	}
	if (ckpt_move_fd(&tp->sifd, minfd) ||
	    ckpt_move_fd(&tp->ssfd, minfd) ||
	    ckpt_move_fd(&tp->ifd, minfd) ||
	    ckpt_move_fd(&tp->sfd, minfd) ||
	    ckpt_move_fd(&tp->mfd, minfd) ||
	    ckpt_move_fd(&tp->cprfd, minfd))
		return (-1);

	if (ckpt_restart_perm_check2(&tp->pi, tp->sfd)) {
		cerror("restart %s permission denied\n", cr.cr_path);
		return (-1);
	}
	return (0);
}

/* ARGSUSED */
int
ckpt_read_context(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_context_t context;
	int prfd = proclist[tp->pindex].pl_prfd;
#ifdef DEBUG_HARDTEST
        greg_t *gregp = (greg_t *)&context.cc_ctxt.cc_ctxt.uc_mcontext.gregs;
#endif

	if (ckpt_rxlate_context(cr.cr_pci.pci_revision, tp, magic, &context) < 0)
		return (-1);

#ifdef DEBUG_HARDTEST
	cdebug("pid %d epc=%lx sr=%x v0=%ld, a0=%ld, a1=%lx,"
                      "a2=%ld a3=%ld ra=%lx\n",
			proclist[tp->pindex].pl_pid,
                        gregp[CXT_EPC],
                        gregp[CXT_SR],
                        gregp[CXT_V0],
                        gregp[CXT_A0],
                        gregp[CXT_A1],
                        gregp[CXT_A2],
                        gregp[CXT_A3],
                        gregp[CXT_RA]);
#endif

	if (restart_thread_ioctl(prfd,
				 context.cc_tid,
				 PIOCCKPTSETCTX,
				 (caddr_t)&context.cc_ctxt) < 0) {
		cerror("restore context failed (%s)", STRERR);
		return (-1);
	}
	/*
	 * If it's reissuing a system call.  Let it go as far as
	 * syscall entry.  Important in case expecting a SIGRESTART to break it
	 * out of a slow syscall
	 */
	if (context.cc_flags & CKPT_CTXT_REISSUE) {
		/*
		 * Set stop on syscall entry mask, advance the thread,
		 * then wait for it to stop again
		 */
		sysset_t entryset;

                prfillset(&entryset);
                if (restart_ioctl(prfd, PIOCSENTRY, &entryset) < 0) {
			cerror("restore reissue context failed (%s)", STRERR);
                        return (-1);
		}
		if (restart_thread_ioctl(prfd,
					 context.cc_tid,
					 PIOCRUN, 
					 NULL) < 0) {
			cerror("restore reissue context failed (%s)", STRERR);
                        return (-1);
		}
		if (restart_thread_ioctl(prfd,
					 context.cc_tid,
					 PIOCWSTOP, 
					 NULL) < 0) {
			cerror("restore reissue context failed (%s)", STRERR);
                        return (-1);
		}
                premptyset(&entryset);
                if (restart_ioctl(prfd, PIOCSENTRY, &entryset) < 0) {
			cerror("restore reissue context failed (%s)", STRERR);
                        return (-1);
		}
	}
	return (0);
}

static int
ckpt_read_svr3pipe_header(ckpt_p_t *pp)
{
	int pipefd[2];

	if (!(pp->cp_flags & CKPT_PIPE_HANDLED))
		return (0);

	if (syssgi(SGI_CKPT_SYS, CKPT_SVR3PIPE, pipefd) < 0) {
		cerror("ckpt_read_svr3pipe_header:pipe (%s)\n", STRERR);
		return(-1);
	}
	if (pipefd[1] != pp->cp_protofd[1]) {
		if (ckpt_move_fd(&pipefd[1], pp->cp_protofd[1]) < 0)
			return (-1);
	}
	if (pipefd[0] != pp->cp_protofd[0]) {
		if (ckpt_move_fd(&pipefd[0], pp->cp_protofd[0] ) < 0)
			return (-1);
	}
	assert(pipefd[0] == pp->cp_protofd[0]);
	assert(pipefd[1] == pp->cp_protofd[1]);

	if (pp->cp_size) {
		if (ckpt_restore_pipe_data(pp->cp_path, 0, pipefd[PIPEWRITE], 0)
			< 0 )
			return (-1);
	}
	return (0);
}

static int
ckpt_read_svr4pipe_header(ckpt_p_t *pp)
{
	int pipefd[2];

	if (!(pp->cp_flags & CKPT_PIPE_HANDLED))
		return (0);

        if (syssgi(SGI_SPIPE, 1) < 0) {
                cerror("ckpt_read_svr4pipe_header:syssgi (%s)\n", STRERR);
                return (-1);
	}
	if (pipe(pipefd) < 0) {
		cerror("pipe (%s)\n", STRERR);
		return(-1);
	}
        if (syssgi(SGI_SPIPE, 0) < 0) {
                cerror("ckpt_read_svr4pipe_header:syssgi (%s)\n", STRERR);
                return (-1);
	}
	if (pipefd[1] != pp->cp_protofd[1]) {
		if (ckpt_move_fd(&pipefd[1], pp->cp_protofd[1] ) < 0)
			return (-1);
	}
	if (pipefd[0] != pp->cp_protofd[0]) {
		if (ckpt_move_fd(&pipefd[0], pp->cp_protofd[0] ) < 0)
			return (-1);
	}
	assert(pipefd[0] == pp->cp_protofd[0]);
	assert(pipefd[1] == pp->cp_protofd[1]);
	/*
	 * Blindly attempt to restore data in both pipe instances.  ENOENT
	 * is the only error allowed.  We can check for correct pipe size
	 * after setting all pipe fds.
	 */
	if (ckpt_restore_pipe_data(pp->cp_path, 0, pipefd[SVR4PIPEMATE(0)], 1) < 0)
		return (-1);
	if (ckpt_restore_pipe_data(pp->cp_path, 1, pipefd[SVR4PIPEMATE(1)], 1) < 0)
		return (-1);
	return (0);
}
/* ARGSUSED */
int
ckpt_restore_pipes(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_p_t *pp;

	IFDEB1(cdebug("restore pipes (magic %8.8s)\n", &magic));

	/*
	 * Read pipe info from the statefile
	 */
	if ((pp = (ckpt_p_t *)malloc(sizeof (ckpt_p_t))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	if (read(tp->sfd, pp, sizeof(ckpt_p_t)) < 0) {
		cerror("Failed to read pipe property (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Create and reload pipes
	 */
	if (pp->cp_magic == CKPT_MAGIC_SVR3PIPE) {

		if (ckpt_read_svr3pipe_header(pp) < 0)
			return (-1);

	} else {
		assert(pp->cp_magic == CKPT_MAGIC_SVR4PIPE);

		if (ckpt_read_svr4pipe_header(pp) < 0)
			return (-1);
	}
	if (pp->cp_fdflags)
		fcntl(pp->cp_fd, F_SETFD, pp->cp_fdflags);

	pp->cp_next = tp->pp;
	tp->pp = pp;
	return (0);
}

static int
ckpt_restore_gid_sid(ckpt_pi_t *pip)
{
	/*
	 * Do not restore process group when cpr has been submitted to
	 * batch scheduler!
	 */
	if (cpr_flags & CKPT_BATCH)
		return (0);
	/*
	 * If we were a session leader, restore the session leadership
	 */
	if (pip->cp_sid == pip->cp_pid) {

		assert(pip->cp_pgrp == pip->cp_pid);
		if (setsid() < 0) {
			cerror("PID %d: Failed to become a session leader (%s)\n",
				getpid(), STRERR);
			return (-1);
		}
		return (0);
	}
	/*
	 * Otherwise,
	 * If we were a group leader, resume the leadership based on the
	 * following conditions:
	 * if ppid == 1 or non-interactive restart
	 *	restore group leader
	 */
	if (pip->cp_pgrp == pip->cp_pid &&
	   (pip->cp_ppid == 1 || !(cr.cr_pci.pci_flags & CKPT_PCI_INTERACTIVE))) {
#ifdef RESTGID
		if (pip->cp_flags & CKPT_PI_ROOT) {
			cdebug("jobcontrol group leader pid=%d\n", pip->cp_pid);
			ckpt_group_leader = pip->cp_pid;
		}
#endif
		if (setpgid(0, 0) < 0) {
			cerror("PID %d: Failed to become a group leader (%s)\n",
				getpid(), STRERR);
			return (-1);
		}
	}
	return (0);
}

/*
 * Change everything about me
 */
static int
ckpt_restore_identity(int prfd, ckpt_pi_t *pip, pid_t client_pid,
						struct passwd *pwent)
{
	sigset_t sigmask;
	ckpt_spi_t spi;
	int i;
	/*
	 * block all signals now, in case setting pinfo causes signal 
	 * to be sent (e.g. alarm timeout).  Restoring proc context
	 * will restore correct signal mask.
	 */
	sigfillset(&sigmask);
#ifdef DEBUG
	sigdelset(&sigmask, SIGBUS);
	sigdelset(&sigmask, SIGSEGV);
#endif
	if (sigprocmask(SIG_BLOCK, &sigmask, NULL) < 0) {
		cerror("Failed to block signals (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * change color now for u and proc!
	 */
	bcopy(pip->cp_comm, spi.spi_comm, sizeof(spi.spi_comm));
	bcopy(pip->cp_psargs, spi.spi_psargs, sizeof(spi.spi_psargs));
	spi.spi_gbread = pip->cp_gbread;
	spi.spi_bread = pip->cp_bread;
	spi.spi_gbwrit = pip->cp_gbwrit;
	spi.spi_bwrit = pip->cp_bwrit;
	spi.spi_syscr = pip->cp_syscr;
	spi.spi_syscw = pip->cp_syscw;
	spi.spi_time = pip->cp_time;
	spi.spi_birth_time = pip->cp_birth_time;
	spi.spi_mem = pip->cp_psi.ps_mem;
	spi.spi_ioch = pip->cp_psi.ps_ioch;
	spi.spi_exitsig = pip->cp_psi.ps_exitsig;
	spi.spi_shflags = pip->cp_psi.ps_shflags;
	bcopy(	(void *)&pip->cp_psi.ps_ru,
		(void *)&spi.spi_ru,
		sizeof(struct rusage));
	bcopy(	(void *)&pip->cp_psi.ps_cru,
		(void *)&spi.spi_cru,
		sizeof(struct rusage));
	spi.spi_cmask = pip->cp_psi.ps_cmask;
	spi.spi_acflag = pip->cp_psi.ps_acflag;
	spi.spi_xstat = pip->cp_psi.ps_xstat;
	bcopy(&pip->cp_psi.ps_timers, &spi.spi_timers, sizeof(ktimerpkg_t));

	if (ioctl(prfd, PIOCCKPTSETPI, &spi) == -1) {
		cerror("ioctl PIOCCKPTSETPI failed (%s)\n", STRERR);
		return (-1);
	}
	/* restore its cpu binding info if any */
	if (pip->cp_mustrun != -1) {
		IFDEB1(cdebug("binding process %d to cpu %d\n", 
			client_pid, pip->cp_mustrun));
		if (sysmp(MP_MUSTRUN_PID, pip->cp_mustrun, client_pid) == -1) {
			cerror("cannot bind process %d to processor %d (%s)\n",
				client_pid, pip->cp_mustrun, STRERR);
		}
	}
	/* restore tslice */
	if (((cpr_flags & CKPT_BATCH) == 0) &&
	    (pip->cp_tslice != 0) &&
	    (schedctl(SLICE, client_pid, pip->cp_tslice) == -1)) {
		cnotice("cannot restore the time slice for process %d (%s)\n",
			client_pid, STRERR);
	}
	if (pip->cp_flags & CKPT_PI_NOCAFFINE) {
		if (schedctl(AFFINITY_ON, client_pid) < 0) {
			cnotice("cannot disable cache affinity for process %d (%s)\n",
				client_pid, STRERR);
		}
	}
	/*
	 * Restore nice value
	 *
	 * TS and old statefiles (which have a bug, so we map them to TS
	 * arbitrarily) have nice value restored.
	 *
	 */
	if ((cpr_flags & CKPT_BATCH) == 0) {
		if ((strcmp(pip->cp_clname, "TS") == 0)||
		    (pip->cp_clname[0] == 0)) {
			int nnice;
			/*
			 * Bring saved value down to nice(2) range, then adjust
			 * for any current value
			 */
			nnice = pip->cp_nice - 20;
			nnice -= nice(0);	/* subtract any current delta */
			(void)nice(nnice);

		} else if (strcmp(pip->cp_clname, "WL") == 0) {

			(void)schedctl(NDPRI, client_pid, NDPLOMAX);

		/*
		 * 6.5 MR statefiles have a 0 for sched policy
		 * and no valid priority.  Can't do much about it
		 * so silently ignore these.
		 */
		} else if ((strcmp(pip->cp_clname, "RT") == 0) &&
			   (pip->cp_schedpolicy != 0)) {
			struct sched_param param;

			param.sched_priority = pip->cp_schedpriority;

			(void)sched_setscheduler(client_pid,
						 pip->cp_schedpolicy, 
						 &param);
		}
	}
	/* restore array related info */
	if (CKPT_IS_ASH(cr.cr_pci.pci_type) && (pip->cp_spilen > 0)) {
		if (arsop(ARSOP_SETSPILEN, -1, &pip->cp_spilen,
			  sizeof(pip->cp_spilen)) < 0)
		{
			cerror("unable to set length of service provider info\n");
			return (-1);
		}
		if (arsop(ARSOP_SETSPI, -1, pip->cp_spi, pip->cp_spilen) < 0) {
			cerror("Ignored errors of restoring array service provider info\n");
		}

		if (setprid(pip->cp_prid) < 0) {
			cerror("Ignored errors of restoring the array project ID\n");
		}
	}
	if (pip->cp_psi.ps_flags & CKPT_TERMCHILD) {

		if (prctl(PR_TERMCHILD) < 0) {
			cerror("cannot restore TERMCHILD for process %d (%s)\n",
				client_pid, STRERR);
			return (-1);
		}
	}
	if (pip->cp_psi.ps_flags & CKPT_COREPID) {
		if (prctl(PR_COREPID, 0, 1) < 0) {
			cerror("ignored error restoring COREPID for process %d (%s)\n",
				client_pid, STRERR);
		}
	}
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
			/*
			 * restore these after restoring files and 
			 * memory image
			 */
			break;

		default:
			if (setrlimit(i, &pip->cp_psi.ps_rlimit[i]) < 0) {
				cerror("Failed to restore rlimit (%s)\n",
					STRERR);
				return (-1);
			}
			break;
		}
	}
	/*
	 * CHANGE BACK TO ORIG. USER CREDENTIAL TO RESTORE THE REST!
	 */
	if (ckpt_restore_cred(pip, client_pid, pwent))
		return (-1);

	if (ckpt_restore_cwd(pip, client_pid) < 0)
		return (-1);

	return (0);
}
/*
 * Convert virtual pid to physical pid
 */
pid_t
vtoppid(pid_t vpid)
{
	ckpt_pl_t *pl;

	if ((pl = ckpt_get_proclist_entry(vpid)) == NULL) {
		assert(pl);
		return (-1);
	}
	return (pl->pl_pid);
}

/*
 * Restore misc state that requires proc hierarchy to be established
 */
/*
 * Want to spruce his up a bit...like /dev/usema, set a flag during
 * ckpt if this is needed, then merge this with restore identity
 * have it execute a barrier TBD KSB
 */
static int
ckpt_restore_misc(ckpt_pi_t *pip, pid_t client_pid)
{
	if (pip->cp_psi.ps_unblkonexecpid) {
		pid_t unblkpid; 
		if ((unblkpid = vtoppid(pip->cp_psi.ps_unblkonexecpid)) < 0) {
			cerror("PR_UNBLKONEXEC error for process %d (no process %d)\n",
				client_pid, pip->cp_psi.ps_unblkonexecpid);
			return (-1);
		}
		if (prctl(PR_UNBLKONEXEC, unblkpid) < 0) {
			cerror("PR_UNBLKONEXEC error for process %d (%s)\n",
				client_pid, STRERR);
			return (-1);
		}
	}
	return (0);
}

static int
ckpt_restore_cwd(ckpt_pi_t *pip, pid_t mypid)
{
	int rc;

	if (pip->cp_stat == SZOMB)
		return (0);

	if (IS_SPROC(pip) && IS_SDIR(pip)) {

		rc = ckpt_sproc_block(	mypid,
					sdirlist->sdir_pid,
					sdirlist->sdir_count,
					&sdirlist->sdir_csync);

		if (rc < 0) {
			cerror("ckpt_sproc_block:%s", STRERR);
			return (rc);
		}
		if (rc == 0)
			return (rc);
	}
	if (pip->cp_cdir && (cr.cr_pci.pci_flags & CKPT_PCI_DEFAULTCWD)) {
		if (chdir(pip->cp_cdir) < 0) {
			cerror("Failed to cwd to the original directory %s (%s)\n",
				pip->cp_cdir, STRERR);
			return (-1);
		}
	}
	if (IS_SPROC(pip) && IS_SDIR(pip)) {

		rc = ckpt_sproc_unblock(mypid,
					sdirlist->sdir_count,
					sdirlist->sdir_pid);
		if (rc < 0) {
			cerror("ckpt_sproc_unblock:%s", STRERR);
			return (rc);
		}
		return (rc);
	}
	return (0);
}

static void
ckpt_restore_root(unsigned long pindex, ckpt_pi_t *pip)
{
	int prfd = proclist[pindex].pl_prfd;

	if (!pip->cp_rdir[0] || !(cr.cr_pci.pci_flags & CKPT_PCI_DEFAULTROOT))
		return;
	IFDEB1(cdebug("chroot to the original directory %s\n", pip->cp_rdir));

	if (restart_ioctl(prfd, PIOCCKPTCHROOT, pip->cp_rdir) == -1) {
		cnotice("Failed to chroot to the original directory %s (%s)\n",
			pip->cp_rdir, STRERR);
		return;
	}
}

static int
ckpt_restore_cred(ckpt_pi_t *pip, pid_t client, struct passwd *pwent)
{
        prcred_t *cred = &pip->cp_cred;
	int rc;

	IFDEB1(cdebug("pid=%d\n", client));

	/*
	 * If it's an sproc sharing uid/gid, need to sync with others
	 * so that all ids change at once.  Need to select a member who
	 * really has share group creds (some may only have partial)
	 */
	if (IS_SPROC(pip) && IS_SID(pip)) {

		rc = ckpt_sid_barrier(pip, client);
		if (rc < 0) {
			cerror("shared id barrier error (%s)\n", STRERR);
			return (-1);
		}
		if (rc == 0)
			/*
			 * not my yob!
			 */
			return (0);
	}
	/*
	 * restore credential
	 */
	IFDEB1(cdebug("orig ruid=%d euid=%d rgid=%d egid=%d\n",
		getuid(), geteuid(), getgid(), getegid()));
	IFDEB1(cdebug("restoring ruid=%d euid=%d rgid=%d egid=%d\n",
		cred->pr_ruid, cred->pr_euid, cred->pr_rgid, cred->pr_egid));
	/*
	 * Restore gid and multiple group membership
	 */
	if (setegid(cred->pr_egid) < 0) {
		cerror("setegid (%s)\n", STRERR);
		return (-1);
	}
	if (setrgid(cred->pr_rgid) < 0) {
		cerror("setegid (%s)\n", STRERR);
		return (-1);
	}
	if (saddrlist && saddrlist->saddr_liblock)
		ckpt_acquire_mutex(saddrlist->saddr_liblock, get_mypid());

	if (initgroups(pwent->pw_name, pwent->pw_gid)) {
		cerror("Cannot initialize multiple group membership (%s)\n", 
			STRERR);
		return (-1);
	}

	if (saddrlist && saddrlist->saddr_liblock)
		ckpt_release_mutex(saddrlist->saddr_liblock, get_mypid());

	/*
	 * Temporary uid settings
	 *
	 * Set real uid to root, effective uid to target
	 * This lets us toggle back and forth depending on
	 * the operation being performed.
	 */
	if (setruid(0) == -1) {
		cerror("setruid (%s)\n", STRERR);
		return (-1);
	}
	if (seteuid(cred->pr_euid) == -1) {
		cerror("seteuid (%s)\n", STRERR);
		return (-1);
	}
	/*
	 * Release waiting share group members
	 */
	if (IS_SPROC(pip) && IS_SID(pip)) {


		rc = ckpt_sproc_unblock(client,
					sidlist->sid_count,
					sidlist->sid_pid);
		if (rc < 0) {
			cerror("ckpt_sproc_unblock:%s", STRERR);
			return (rc);
		}
		return (rc);
	}
	return (0);
}

int
ckpt_load_target_info(ckpt_ta_t *tp, pid_t pid)
{
	int rv = 0;
	struct rlimit cur, new;
	
	bzero(tp, sizeof(ckpt_ta_t));

	if (getrlimit(RLIMIT_NOFILE, &cur) < 0) {
		cerror("getrlimit (%s)\n", STRERR);
		return (-1);
	}
	new.rlim_cur = RLIM_INFINITY;
	new.rlim_max = RLIM_INFINITY;

	(void)setrlimit(RLIMIT_NOFILE, &new);

	if ((rv = ckpt_open_ta_fds(tp, pid)) < 0)
		goto err_exit;
	/*
	 * Read and exam the first part of statefile properties
	 */
	if (rv = ckpt_read_proc_property(tp, CKPT_MAGIC_CHILDINFO))
		goto err_exit;
	if (rv = ckpt_read_proc_property(tp, CKPT_MAGIC_PROCINFO))
		goto err_exit;
	if (rv = ckpt_read_proc_property(tp, CKPT_MAGIC_SIGINFO))
		goto err_exit;
err_exit:
	
	if (setrlimit(RLIMIT_NOFILE, &cur) < 0) {
		cerror("setrlimit (%s)\n", STRERR);
	}
	return (rv);
}

static int
ckpt_open_ta_fds(ckpt_ta_t *tp, pid_t pid)
{
	char file[CPATHLEN];

	sprintf(file, "%s/%s.%s", cr.cr_path, STATEF_SHARE, STATEF_INDEX);
	if ((tp->sifd = open(file, O_RDONLY)) < 0) {
		cerror("Failed to open index %s (%s)\n", file, STRERR);
		return (-1);
	}
	sprintf(file, "%s/%s.%s", cr.cr_path, STATEF_SHARE, STATEF_STATE);
	if ((tp->ssfd = open(file, O_RDONLY)) < 0) {
		cerror("Failed to open index %s (%s)\n", file, STRERR);
		close(tp->sifd);
		return (-1);
	}
	sprintf(file, "%s/%d.%s", cr.cr_path, pid, STATEF_INDEX);
	if ((tp->ifd = open(file, O_RDONLY)) == -1) {
		cerror("Cannot open indexfile %s (%s): %lx\n", file, STRERR);
		close(tp->sifd);
		close(tp->ssfd);
		return (-1);
	}
	sprintf(file, "%s/%d.%s", cr.cr_path, pid, STATEF_STATE);
	if ((tp->sfd = open(file, O_RDONLY)) == -1) {
		cerror("Cannot open statefile %s (%s)\n", file, STRERR);
		close(tp->sifd);
		close(tp->ssfd);
		close(tp->ifd);
		return (-1);
	}
	sprintf(file, "%s/%d.%s", cr.cr_path, pid, STATEF_IMAGE);
	if ((tp->mfd = open(file, O_RDONLY)) == -1) {
		cerror("Cannot open imagefile %s (%s)\n", file, STRERR);
		close(tp->sifd);
		close(tp->ssfd);
		close(tp->ifd);
		close(tp->sfd);
		return (-1);
	}
	if (cr.cr_ssfd >= 0) {
		if ((tp->cprfd = dup(cr.cr_ssfd)) < 0) {
			cerror("Cannot dup status fd %s (%s)\n", file, STRERR);
			close(tp->sifd);
			close(tp->ssfd);
			close(tp->ifd);
			close(tp->sfd);
			close(tp->mfd);
			return (-1);
		}
	} else
		tp->cprfd = -1;

	return (0);
}

int
ckpt_close_ta_fds(ckpt_ta_t *tp)
{
	if (	close(tp->sifd) < 0 ||
		close(tp->ssfd) < 0 ||
		close(tp->ifd) < 0 ||
		close(tp->sfd) < 0 ||
		close(tp->mfd) < 0 ||
		((tp->cprfd >= 0)&&close(tp->cprfd)) < 0) {
		cerror("Failed to close target file descriptors (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}
/*
 * Signal handdling.
 *
 * Save any existing handlers, call them on signal invocation
 */
static sigaction_t saveact[NSIG];
static void (*sa_action[NSIG])();

/* ARGSUSED */
static void
ckpt_cpr_sighandler(int signo, siginfo_t *sip, void *ptr)
{
	extern int ckpt_abort_arserver;

	IFDEB1(cdebug("ckpt_cpr_sighandler: signo %d func 0x%0x\n",
						signo, sa_action[signo-1]));
	if (sa_action[signo-1] == SIG_IGN)
		return;

	if (getpid() != cr.cr_waitor)
		return;

	if (CKPT_IS_GASH(cr.cr_pci.pci_type,cr.cr_pci.pci_flags) && 
							(signo == SIGTERM))
		return;

	IFDEB1(cdebug("cpr(%d): received signal %d, cleaning up\n", getpid(), signo));

#ifdef DEBUG_RESTART_HANG
	ckpt_dump_proclist();
#endif
       if (creates_started) {
		cr.cr_pci.pci_flags |= CKPT_PCI_ABORT;
		ckpt_kill_proclist(1);
		(void)ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_UNLINKED, NULL);
       }
       /*
	* Will not stop to wait for the sync point because we are in
	* signal handler already (SIGABRT will get us here again)
	*/
	if (CKPT_IS_GASH(cr.cr_pci.pci_type,cr.cr_pci.pci_flags)) {
		if (ckpt_restart_is_client)
			ckpt_reach_sync_point(CKPT_ABORT_SYNC_POINT, 0);
		else
			*interactive_restart = -1;
	}
	ckpt_restore_limits();
	if (sa_action[signo-1]) {
		(*sa_action[signo-1])(signo, sip, ptr);
		longjmp(jmpbuf, 1);
	}
	exit(1);
}

static int
ckpt_init_sighandler(int set)
{
	sigaction_t act, *actp, *savep;
	int signo;

	for (signo = 1; signo < NSIG; signo++) {

		if (signo == SIGKILL  ||
		    signo == SIGSTOP  ||
		    signo == SIGTSTP  ||
		    signo == SIGTTIN  ||
		    signo == SIGTTOU  ||
		    signo == SIGCONT  ||
		    signo == SIGCLD   ||
		    signo == SIGWINCH ||
		    signo == SIGURG   ||
		    signo == SIGPWR   ||
		    signo == SIGUSR1  ||
		    signo == SIGUSR2  ||
#ifdef DEBUG
		    signo == SIGBUS   ||
		    signo == SIGSEGV  ||
#endif
#ifdef SIGSWAP
		    signo == SIGSWAP ||
#endif
		    signo == SIGCKPT  ||
		    signo == SIGRESTART)
			continue;

		switch (set) {
		case 1:
			/*
			 * Get current handling
			 */
			savep = &saveact[signo-1];
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
				act.sa_sigaction = ckpt_cpr_sighandler;
			} else {
				sa_action[signo-1] = savep->sa_handler;
				act.sa_sigaction = NULL;
				act.sa_handler = ckpt_cpr_sighandler;
			}
			actp = &act;
			break;
		case 0:
			act.sa_flags = 0;
			act.sa_handler = SIG_DFL;
			sigemptyset(&act.sa_mask);
			act.sa_sigaction = NULL;
			actp = &act;
			break;
		case -1:
			actp = &saveact[signo-1];
			break;
		}
		if (sigaction(signo, actp, NULL) < 0) {
			cerror("sigaction failed (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

/*
 * Wait for all children to be ready
 */
static int
ckpt_wait_procs(void)
{
	char status;
	/*
	 * close write end of status pipe so we can detect target
	 * exit
	 */
	close(cr.cr_ssfd);
	cr.cr_ssfd = -1;

	IFDEB1(cdebug("wait procs\n"));

	do {
		IFDEB1(cdebug("started %d, finished %d\n",
			*creates_started, *creates_finished));
		
		if (read(cr.cr_rsfd, &status, sizeof(status)) != sizeof(status)) {
			cerror("Unexpected status EOF\n");
			setoserror(ECKPT);
			return (-1);
		}
	} while(*creates_started != *creates_finished);

	while (!ckpt_proclist_all_in_state(CKPT_PL_DONE)) {
		/*
		 * Still waiting on someone, check for any error
		 */
		if (ckpt_scan_proclist(CKPT_PL_ERR))
			break;

		if (read(cr.cr_rsfd, &status, sizeof(status)) != sizeof(status)) {
			cerror("Unexpected status EOF\n");
			setoserror(ECKPT);
			return (-1);
		}
	}
	/*
	 * Double check nobody is in error
	 */
	if (cpr_errno = ckpt_scan_proclist(CKPT_PL_ERR)) {

		IFDEB1(cdebug("proc in error\n"));

		if (CKPT_IS_GASH(cr.cr_pci.pci_type,cr.cr_pci.pci_flags)) {
			if (ckpt_restart_is_client)
				ckpt_reach_sync_point(CKPT_ABORT_SYNC_POINT, 0);
			else
				*interactive_restart = -1;
		}
		return (-1);
	}
	IFDEB1(cdebug("All restarting process done with remapping\n"));
	return (0);
}

static void
ckpt_restart_interactive_wait(void)
{
	*interactive_restart = 1;
	if (blockproc(getpid()) < 0) {
		cerror("Failed to block myself %d (%s)\n", getpid(), STRERR);
	}
}

static int
ckpt_setrun_zombies(void)
{
	IFDEB1(cdebug("ready to setrun all zombies\n"));
	if (ckpt_start_zombies()) {
		ckpt_kill_proclist(0);
		return (-1);
	}
	return (0);
}

static int
ckpt_setrun_procs(void)
{
	IFDEB1(cdebug("ready to setrun all procs\n"));
	if (ckpt_start_proclist()) {
		ckpt_kill_proclist(0);
		return (-1);
	}
	return (0);
}

static void
ckpt_restart_cleanup(void)
{
	(void)ckpt_init_sighandler(-1);
	if (cr.cr_roots)
		free(cr.cr_roots);
	if (cr.cr_index)
		free(cr.cr_index);	
}

int
ckpt_sfd_sync(ckpt_pi_t *pi)
{
	int rc;

	if (!IS_SPROC(pi) || !IS_SFDS(pi))
		return (0);

	rc = ckpt_sproc_block(	getpid(),
				sfdlist->sfd_pid,
				sfdlist->sfd_count,
				&sfdlist->sfd_sync);
	if (rc < 0) {
		cerror("ckpt_sproc_block:%s", STRERR);
		return (rc);
	}
	if (rc == 0)
		return (rc);

	rc = ckpt_sproc_unblock(getpid(),
				sfdlist->sfd_count,
				sfdlist->sfd_pid);
	if (rc < 0) {
		cerror("ckpt_sproc_unblock:%s", STRERR);
		return (rc);
	}
	return (0);
}

/*
 * Once signals begin posting to targets, the target may die from the signal.
 * So, need to check for and allow ENOENT errors
 */
int
restart_ioctl(int prfd, int request, void *arg)
{
	int rc;

	if ((rc = ioctl(prfd, request, arg)) < 0) {
		if (oserror() == ENOENT)
			return (0);
	}
	return (rc);
}

int
restart_thread_ioctl(int prfd, tid_t tid, int cmd, void *arg)
{
        prthreadctl_t prt;
	int rc;

        prt.pt_tid = tid;
        prt.pt_cmd = cmd;
        prt.pt_flags = PTFS_ALL | PTFD_EQL;
        prt.pt_data = arg;

        if ((rc = ioctl(prfd, PIOCTHREAD, &prt)) < 0) {
                if (oserror() == ENOENT)
			return (0);
        }
        return (rc);
}

static ckpt_liblock_t *
ckpt_init_liblock(int maxprocs)
{
	ckpt_liblock_t *liblock;

	if ((liblock = (ckpt_liblock_t *)malloc(sizeof(ckpt_liblock_t)))== NULL) {
		cerror("ckpt_init_liblock:malloc liblock (%s)\n", STRERR);
		return (NULL);
	}
	if ((liblock->ll_waiters = (pid_t *)malloc(maxprocs * sizeof(pid_t)))==NULL) {
		cerror("ckpt_init_liblock:malloc list (%s)\n", STRERR);
		return (NULL);
	}
	if (init_lock(&liblock->ll_spinlock)) {
		cerror("ckpt_init_liblock:init_lock (%s)\n", STRERR);
		return (NULL);
	}
	liblock->ll_owner = 0;
	liblock->ll_count = 0;
	liblock->ll_nwaiters = 0;
	liblock->ll_next = 0;
	liblock->ll_ffree = 0;
	liblock->ll_nentry = maxprocs;

	return (liblock);
}

static void
ckpt_cleanup_liblock(ckpt_liblock_t *liblock)
{
	if (liblock) {
		free(liblock->ll_waiters);
		free(liblock);
	}
}

void
ckpt_acquire_mutex(ckpt_liblock_t *llp, pid_t requester)
{
	spin_lock(&llp->ll_spinlock);

	if (llp->ll_owner == 0) {
		assert(llp->ll_count == 0);
		llp->ll_owner = requester;
		(void)release_lock(&llp->ll_spinlock);
		llp->ll_count = 1;
		return;

	} else if (llp->ll_owner == requester) {
		assert(llp->ll_count > 0);
		llp->ll_owner = requester;
		(void)release_lock(&llp->ll_spinlock);
		llp->ll_count++;
		return;
	}
	llp->ll_nwaiters++;
	llp->ll_waiters[llp->ll_ffree++] = requester;
	if (llp->ll_ffree >= llp->ll_nentry)
		llp->ll_ffree = 0;

	(void)release_lock(&llp->ll_spinlock);

	blockproc(requester);

	assert(llp->ll_owner == requester);
	assert(llp->ll_count == 0);

	llp->ll_count = 1;
}

/* ARGSUSED */
void
ckpt_release_mutex(ckpt_liblock_t *llp, pid_t owner)
{
	pid_t newowner;

	assert(llp->ll_owner == owner);

	/* only the owner updates ll_count, so no lock required */
	if (--llp->ll_count != 0)
		return;

	spin_lock(&llp->ll_spinlock);

	if (llp->ll_nwaiters == 0) {
		llp->ll_owner = 0;
		(void)release_lock(&llp->ll_spinlock);
		return;
	}
	llp->ll_nwaiters--;
	newowner = llp->ll_owner = llp->ll_waiters[llp->ll_next++];
	if (llp->ll_next >= llp->ll_nentry)
		llp->ll_next = 0;

	(void)release_lock(&llp->ll_spinlock);

	unblockproc(newowner);
}
/*
 * Preempt C library interfaces in order to impose mutual exclusion 
 * when required
 */
extern void *_malloc(size_t);
extern void *_memalign(size_t, size_t);
extern void _free(void *);

void *
malloc(size_t size)
{
#ifdef DEBUG_MALLOC
	void *ptr;
	static long baseline = 0;

	if (saddrlist && saddrlist->saddr_liblock)
		ckpt_acquire_mutex(saddrlist->saddr_liblock, get_mypid());

	if (baseline == 0) {
		baseline = (long)sbrk(0);
		printf("%d:baseline %lx\n", get_mypid(), baseline);
	}

	ptr = _malloc(size);

	printf("%d:%lx:malloc %d:%lx\n", get_mypid(), __return_address, size, ptr);

	if (saddrlist && saddrlist->saddr_liblock)
		ckpt_release_mutex(saddrlist->saddr_liblock, get_mypid());

	return (ptr);
#else /* !DEBUG_MALLOC */
	if (saddrlist && saddrlist->saddr_liblock) {
		void *ptr;

		ckpt_acquire_mutex(saddrlist->saddr_liblock, get_mypid());
		ptr = _malloc(size);
		ckpt_release_mutex(saddrlist->saddr_liblock, get_mypid());
		return (ptr);
	}
	return (_malloc(size));
#endif
}

void *
memalign(size_t alignment, size_t size)
{
	if (saddrlist && saddrlist->saddr_liblock) {
		void *ptr;

		ckpt_acquire_mutex(saddrlist->saddr_liblock, get_mypid());
		ptr = _memalign(alignment, size);
		ckpt_release_mutex(saddrlist->saddr_liblock, get_mypid());
		return (ptr);
	}
	return (_memalign(alignment, size));
}

#ifdef DEBUG_MEMALLOC
#undef free
#endif
void
free(void *ptr)
{
	if (saddrlist && saddrlist->saddr_liblock) {
		ckpt_acquire_mutex(saddrlist->saddr_liblock, get_mypid());
		_free(ptr);
		ckpt_release_mutex(saddrlist->saddr_liblock, get_mypid());
		return;
	}
	_free(ptr);
}
#ifdef LIBPW_WAR
/*
 * close any dangling sockets left by libpw
 *
 * do this by unbinding current domain
 */
static int
ckpt_libpw_war(void)
{
	char domain[64];	/* see man page for getdomainname */

	if (getdomainname(domain, sizeof(domain)) != 0) {
		cerror("getdomainname (%s)\n", STRERR);
		return (-1);
	}
	yp_unbind(domain);
	return (0);
}
#endif
#ifdef DEBUG_XXX
struct {
	pid_t pid;
	int fd;
	void *vaddr;
} logbuf[1000];
unsigned long log_index = 0;

int
close(int fd)
{
	extern int __close(int);
	if (fd == 2) {
	int i;

		i = (int)test_then_add(&log_index, 1);

		logbuf[i].pid = getpid();
		logbuf[i].fd = fd;
		logbuf[i].vaddr = (void *)__return_address;
	}
	return(__close(fd));
}

void
dumplog(void)
{
	int i;

	for (i = 0; i < log_index; i++){ 
		cdebug("log %d pid %d fd %d vaddr %lx\n",
			i, logbuf[i].pid, logbuf[i].fd, logbuf[i].vaddr);
	}
}
#endif

/*
 * return the tty name for the calling process.
 */
static void ckpt_gettty_name(void)
{
	int i;
	ckpt_tty_name = NULL;
	for(i=0; i<getdtablehi(); i++) {
		if(isatty(i)) {
			if((ckpt_tty_name = ttyname(i)) != NULL) {
				IFDEB1(cdebug("tty_name %s\n", ckpt_tty_name));
				return;
			}
		}
	}
}

