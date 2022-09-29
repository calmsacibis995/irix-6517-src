 /*
 * ckpt_file.c
 *
 * Checkpoint/restart file support.
 *
 *      Routines for saving a processes memory state
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.84 $"

#include <fcntl.h>
#include <pwd.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/syssgi.h>
#include <sys/ckpt_procfs.h>
#include <sys/file.h>
#include <sys/stropts.h>
#include <pipefs/pipeioctl.h>
#include <sys/mman.h>
#include <sys/psema_cntl.h>
#include <sys/sysmacros.h>
#include <sys/ckpt_sys.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h"

extern ch_t cpr;
extern cr_t cr;
extern char *ckpt_tty_name;

#define TIMER_EQ(t, v)	((t)->tv_sec == (v)->tv_sec && (t)->tv_nsec == (v)->tv_nsec)
#define TIMER_NEQ(t, v)	((t)->tv_sec != (v)->tv_sec || (t)->tv_nsec != (v)->tv_nsec)
#define TIMER_GEQ(t, v) ((t)->tv_sec > (v)->tv_sec || \
			 ((t)->tv_sec == (v)->tv_sec && \
			  (t)->tv_nsec > (v)->tv_nsec))

static int ckpt_restore_namedpipe(ckpt_ta_t *tp, ckpt_f_t *);
static int
ckpt_fstat(ckpt_ta_t *ta, char *pathname, struct stat *buf);
static int ckpt_truncate(ckpt_ta_t *ta, char *pathname, off_t len);
static int ckpt_restore_tty(ckpt_ta_t *tp, ckpt_f_t *fp, int *dev_null);

#ifdef XXX_DEBUG_FDS
static void ckpt_dump_fds(int, ckpt_po_t *);
#endif

static int
ckpt_copy_a_file(int rfd, int wfd)
{
	char *buf;
	ssize_t cnt;
	int miniosz;
	int bufsz;
	int align;

	miniosz = getpagesize();
	bufsz = CKPT_IOBSIZE;
	align = miniosz;

	while (bufsz >= miniosz) {
		buf = memalign(align, bufsz);
		if (buf)
			break;
		bufsz = bufsz >> 1;
	}
	if (buf == NULL) {
		cerror("ckpt_copy_a_file: memalign failed (%s)\n", STRERR);
		return (-1);
	}
	IFDEB1(cdebug("bufsz: %x\n", bufsz));

	while (1) {
		if ((cnt = read(rfd, buf, bufsz)) < 0) {
			cerror("ckpt_copy_a_file: read failed (%s)\n", STRERR);
			free(buf);
			return (-1);
		}
		if (cnt == 0)
			break;

		if (write(wfd, buf, cnt) < 0) {
			cerror("ckpt_copy_a_file: write failed (%s)\n", STRERR);
			free(buf);
			return (-1);
		}
	}
	free(buf);

	return (0);
}

/* 
 * restore fifo data from a file. 
 */
static int
ckpt_copy_fifo_file(int rfd, int wfd)
{
	long cnt;
	char buf[PIPE_BUF];	

	while(1) {
		/* we write to the fifo at most PIPE_BUF-1 bytes, so we can do 
		 * one more write the second time.
		 */
		if ((cnt = read(rfd, buf, PIPE_BUF-1)) < 0) {
			cerror("ckpt_copy_a_file: read failed (%s)\n", STRERR);
			return (-1);
		}
		if (cnt == 0)
			break;
		if (write(wfd, buf, cnt) < 0) {
			cerror("ckpt_copy_a_file: write failed (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

void
ckpt_setpaths(ckpt_obj_t *co, ckpt_f_t *fp, char *path, dev_t dev, ino_t ino,
		mode_t mode, pid_t pid, int unlinked)
{
	char tmp[MAXPATHLEN];
	char relocation[MAXPATHLEN];
	ch_t *ch = co->co_ch;

	if (fp->cf_mode == CKPT_MODE_RELOCATE) {
		/*
		 * Modify path for relocation
		 */
		if (unlinked)
			path = ch->ch_relocdir;
		else {
			strcpy(relocation, ch->ch_relocdir);
			strcpy(tmp, path);
			strcat(relocation, "/");
			strcat(relocation, ckpt_path_to_basename(path));

			path = relocation;
		}
	}
	if (S_ISCHR(mode)||S_ISBLK(mode)) {
		sprintf(fp->cf_path, "/dev/ckpt_spec_%d_%x", pid, dev);
		fp->cf_newpath[0] = 0;

	} else if (ch->ch_flags & CKPT_PCI_DISTRIBUTE) {
		if (unlinked) {
			sprintf(fp->cf_path, "%s/%s.%lld.%d.u",
						path, STATEF_FILE, ino, pid);
			sprintf(fp->cf_newpath, "%s/%s.%lld.%d",
						path, STATEF_FILE, ino, pid);
		} else {
			strcpy(fp->cf_path, path);
			if (fp->cf_mode == CKPT_MODE_REPLACE ||
			    fp->cf_mode == CKPT_MODE_SUBSTITUTE ||
			    fp->cf_mode == CKPT_MODE_RELOCATE)
				sprintf(fp->cf_newpath, "%s.%s.%d",
						 path, STATEF_FILE, pid);
			else
				fp->cf_newpath[0] = 0;
		}
	} else {
		/* centralized */
		if (unlinked) {
			sprintf(fp->cf_path, "%s/%s.%lld.%d.u",
					path, STATEF_FILE, ino, pid);
			sprintf(fp->cf_newpath, "%s/%s.%d.%lld",
					ch->ch_path, STATEF_FILE, dev, ino);
		} else {
			strcpy(fp->cf_path, path);
			if (fp->cf_mode == CKPT_MODE_REPLACE ||
			    fp->cf_mode == CKPT_MODE_SUBSTITUTE ||
			    fp->cf_mode == CKPT_MODE_RELOCATE)
				sprintf(fp->cf_newpath, "%s/%s.%d.%lld",
					ch->ch_path, STATEF_FILE, dev, ino);
			else
				fp->cf_newpath[0] = 0;
		}
	}
}

void
ckpt_setmempath(char *path, ckpt_f_t *fp)
{
	if (fp->cf_mode == CKPT_MODE_SUBSTITUTE)
		strcpy(path, fp->cf_newpath);
	else
		strcpy(path, fp->cf_path);
}

void
ckpt_setpipepath(ckpt_p_t *pp, const char *path, int instance)
{
	sprintf(pp->cp_path, "%s/%s.%x.%lld.%d", path, STATEF_PIPE, pp->cp_dev,
		pp->cp_ino, instance);
}

void
ckpt_set_shmpath(ckpt_obj_t *co, char *path, int shmid)
{
	sprintf(path, "%s/%s.%d", co->co_ch->ch_path, STATEF_SHM, shmid);
}

/*
 * reset file times...
 *
 * This can fail if user doesn't own the file.  If that happens,
 * we, for now, silently ignore	XXX
 */
static int
ckpt_reset_ftimes(int fd, ckpt_f_t *fp, struct stat *sbp)
{
	timespec_t tim[2];
	long err;

	if (sbp != NULL &&
	    TIMER_EQ(&sbp->st_atim, &fp->cf_atime) && 
	    TIMER_EQ(&sbp->st_mtim, &fp->cf_mtime))
		return (0);

	IFDEB1(cdebug("Resetting times on %s\n", fp->cf_path));

	tim[0] = fp->cf_atime;
	tim[1] = fp->cf_mtime;

	if (fd < 0)
		err = syssgi(SGI_CKPT_SYS, CKPT_SETTIMES, fp->cf_path, tim);
	else
		err = syssgi(SGI_CKPT_SYS, CKPT_FSETTIMES, fd, tim);

	if (err && oserror() != EPERM) {
		cerror("syscall CKPT_SETTIMES failed on file %s fd %d(%s)\n",
			fp->cf_path, fd, STRERR);
		return (-1);
	}
	return (0);
}

int
ckpt_dump_one_ofile(ckpt_f_t *fp, int rfd)
{
	struct stat savesb;
	struct stat targsb;

	IFDEB1(cdebug("file disposition mode %d\n", fp->cf_mode));
	switch(fp->cf_mode) {
	case CKPT_MODE_REPLACE:
	case CKPT_MODE_SUBSTITUTE:
	case CKPT_MODE_RELOCATE:
	{
		int wfd;

		/* save the open file for replacement or merge at restart time */
		assert(fp->cf_newpath[0]);
		IFDEB1(cdebug("copying files:\n"));
		IFDEB1(cdebug("\tfrom:\t %s\n", fp->cf_path));
		IFDEB1(cdebug("\tto:\t %s\n", fp->cf_newpath));
		/*
		 * get file times
		 */
		if (fstat(rfd, &targsb) < 0) {
			cerror("fstat failed on fd %d (%s)\n", rfd, STRERR);
			return (-1);
		}
		/*
		 * If save file exists and is up to date, nothing to do
		 */
		if ((stat(fp->cf_newpath, &savesb) >= 0) &&
		    TIMER_GEQ(&savesb.st_mtim, &targsb.st_mtim)) {

			IFDEB1(cdebug("current save file for %s\n", fp->cf_path));
			break;
		}
		if (S_ISFIFO(fp->cf_fmode))
			return (ckpt_save_svr4fifo_data(fp->cf_newpath, rfd));

		if ((wfd = open(fp->cf_newpath, O_CREAT|O_RDWR|O_TRUNC, 
			fp->cf_fmode)) == -1) {
			cerror("failed to open %s (%s)\n", fp->cf_newpath, STRERR);
			return (-1);
		}
		/*
		 * Save the file with the same owner and group
		 */
		IFDEB1(cdebug("change file %s to uid %d gid %d\n",
			fp->cf_newpath, targsb.st_uid, targsb.st_gid));
		if (fchown(wfd, targsb.st_uid, targsb.st_gid) < 0) {
			cerror("cannot change owner on file %s (%s)\n",
				fp->cf_newpath, STRERR);
			return (-1);
		}
		if (ckpt_copy_a_file(rfd, wfd) < 0) {
			(void)close(wfd);
			/*
			 * Clean up this saved file out if we are out of space
			 */
			if (oserror() == ENOSPC) {
				unlink(fp->cf_newpath);
			}
			return -1;
		}
		if (ckpt_reset_ftimes(rfd, fp, NULL) < 0) {
			(void)close(wfd);
			return (-1);
		}
		(void)fsync(wfd);
		(void)close(wfd);
		break;
	}
	case CKPT_MODE_APPEND:
	case CKPT_MODE_IGNORE:
	case CKPT_MODE_MERGE:
	default:
		break;
	}
	return 0;
}
/*
 * Move fd used by restart out of the way of targets fds
 */
int
ckpt_move_fd(int *fd, int minfd)
{
	int tfd;

	if ((tfd = fcntl(*fd, F_DUPFD, minfd)) < 0) {
		cerror("ckpt_move_fd: fcntl (%s)\n", STRERR);
		return -1;
	}
	IFDEB1(cdebug("old fd: %d, New fd: %d\n", *fd, tfd));
	(void)close(*fd);
	*fd = tfd;

	return (0);
}
/*
 * Used to get private copies of originally shared fds
 */
int
ckpt_get_private_fds(ckpt_ta_t *tp, cr_t *crp)
{
	int minfd = tp->pi.cp_maxfd + tp->pi.cp_npfds + 1;
	int rc;
	/*
	 * shared index & state files, stderr, stdout, stdin
	 */
	if (IS_SPROC(&tp->pi) && IS_SFDS(&tp->pi)) {
		/*
		 * Let each share member get a copy of the fd, sync up,
		 * then close original fd
		 */
		if ((tp->errfd = fcntl(fileno(stderr), F_DUPFD, minfd)) < 0) {
			cerror("dup failed (%s)\n", STRERR);
			return (-1);
		}
		/*
		 * Wait here until all have a copy.  Then only one guy should
		 * close.  Close before unblocking.
		 */
		rc = ckpt_sproc_block(	getpid(),
					sfdlist->sfd_pid,
					sfdlist->sfd_count,
					&sfdlist->sfd_sync);
		if (rc < 0)
			return (-1);
		if (rc > 0) {
			/*
			 * close originals (also close stdin and stdout)
			 */
			if (close(crp->cr_ifd) < 0) {
				cerror("close failed (%s)\n", STRERR);
				return (-1);
			}
			if (close(crp->cr_sfd) < 0) {
				cerror("close failed (%s)\n", STRERR);
				return (-1);
			}
			if (close(crp->cr_rsfd) < 0) {
				cerror("close failed (%s)\n", STRERR);
				return (-1);
			}
			if (close(crp->cr_ssfd) < 0) {
				cerror("close failed (%s)\n", STRERR);
				return (-1);
			}
			(void)close(fileno(stdin));
			(void)close(fileno(stdout));
			(void)close(fileno(stderr));

			rc = ckpt_sproc_unblock(getpid(),
					sfdlist->sfd_count, sfdlist->sfd_pid);
			if (rc < 0)
				return (rc);
		}
		/*
		 * If we're not sharing address space, update
		 * stderr(fileno) with our own errfd
		 */
		if (!IS_SADDR(&tp->pi)) {
			fileno(stderr) = tp->errfd;
#ifdef DEBUG
			/* booby trap cr_ifd, cr_sfd */
			crp->cr_ifd = -1;
			crp->cr_sfd = -1;
#endif
		}
	} else {
		/*
		 * NOT sharing fds but might be sharing addr space
		 */
		tp->errfd = fileno(stderr);

		if (tp->errfd < minfd && ckpt_move_fd(&tp->errfd, minfd))
			return (-1);

		/*
		 * Also close stdin and stdout
		 */
		if (close(crp->cr_ifd) < 0) {
			cerror("close failed (%s)\n", STRERR);
			return (-1);
		}
		if (close(crp->cr_sfd) < 0) {
			cerror("close failed (%s)\n", STRERR);
			return (-1);
		}
		if (close(crp->cr_rsfd) < 0) {
			cerror("close failed (%s)\n", STRERR);
			return (-1);
		}
		if (close(crp->cr_ssfd) < 0) {
			cerror("close failed (%s)\n", STRERR);
			return (-1);
		}
		(void)close(fileno(stdin));
		(void)close(fileno(stdout));

		if (!IS_SPROC(&tp->pi) || !IS_SADDR(&tp->pi)) {
			/*
			 * sharing neither fds or addr space
			 */
			fileno(stderr) = tp->errfd;
#ifdef DEBUG
			crp->cr_ifd = -1;
			crp->cr_sfd = -1;
#endif
		}
	}
	/*
	 * stderr hack
	 */
	if (saddrlist && saddrlist->saddr_liblock)
		*((int *)(PRDA->usr_prda.fill)) = tp->errfd;
	/*
	 * Make shared addr sprocs sync up due to stderr hack
	 */
	if (IS_SPROC(&tp->pi) && IS_SADDR(&tp->pi)) {
		extern int ckpt_stderr_initialized;

		rc = ckpt_sproc_block(	getpid(),
					saddrlist->saddr_pid,
					saddrlist->saddr_count,
					&saddrlist->saddr_sync);
		if (rc < 0)
			return (-1);

		ckpt_stderr_initialized = 1;
		if (rc > 0) {
			rc = ckpt_sproc_unblock(getpid(),
						saddrlist->saddr_count,
						saddrlist->saddr_pid);
			return (rc);
		}
	}
	return (0);
}

/*
 * Close any statefile fds that we inherited but shouldn't have
 * all this is made necessary by possible combinations of sproc(),
 * fork() and sharing and not sharing fds and by sharing of kernel file
 * structs due to fd inheritence.
 *
 * all thast should be open are the statefiles in the ckpt_ta_t struct and the 
 * input /proc fd
 */
int
ckpt_close_inherited_fds(ckpt_ta_t *tp, cr_t *crp, pid_t mypid, int forked)
{
	int prfd;
	char procpath[32];
	ckpt_statbuf_t sbuf;

	if (!forked && IS_SFDS(&tp->pi))
		return (0);

	sprintf(procpath, "/proc/%d", mypid);
	if ((prfd = open(procpath, O_RDONLY)) < 0) {
		cerror("proc open failed (%s) %s:%d\n", STRERR, procpath, getpid());
		return (-1);
	}
	sbuf.ckpt_fd = 0;
	while (1) {
		if (ioctl(prfd, PIOCCKPTFSTAT, &sbuf) != 0) {
			if (oserror() == ENOENT) {
				/*
				 * our work is done here
				 */
				close(prfd);
				return (0);
			}
			cerror("fd scan failure (%s)\n", STRERR);
			return (-1);
		}
		if (strcmp(sbuf.ckpt_fstype, "proc") == 0) {
			if (sbuf.ckpt_fd == prfd) {
				/* /proc fd */
				sbuf.ckpt_fd++;
				continue;
			}
		}
		/*
		 * Yuch!
		 */
		if (sbuf.ckpt_fd != tp->sifd &&
		    sbuf.ckpt_fd != tp->ssfd &&
		    sbuf.ckpt_fd != tp->ifd &&
		    sbuf.ckpt_fd != tp->sfd &&
		    sbuf.ckpt_fd != tp->mfd &&
		    sbuf.ckpt_fd != tp->cprfd &&
		    sbuf.ckpt_fd != crp->cr_ifd &&
		    sbuf.ckpt_fd != crp->cr_sfd &&
		    sbuf.ckpt_fd != crp->cr_rsfd &&
		    sbuf.ckpt_fd != crp->cr_ssfd &&
		    sbuf.ckpt_fd != fileno(stdin) &&
		    sbuf.ckpt_fd != fileno(stdout) &&
	 	    sbuf.ckpt_fd != fileno(stderr)) {
			/*
			 * Not one of ours
			 */
			if (close(sbuf.ckpt_fd) < 0) {
				cerror("statefile close failed (%s)\n", STRERR);
				return (-1);
			}
		}
		sbuf.ckpt_fd++;
	}
	/* NOT REACHED */
}

static int
ckpt_restore_one_specfile(ckpt_ta_t *tp, ckpt_f_t *fp, int create)
{
	int fd, tempfd, dev_null=0;

	if (create) {
		/* create a unique name */
		if (mknod(fp->cf_path, fp->cf_fmode, fp->cf_rdev) < 0) {
			cerror("failed to mknod for %s (%s)\n", fp->cf_path, STRERR);
			return (-1);
		}
		if (chmod(fp->cf_path, fp->cf_fmode) < 0) {
			cerror("failed to chmod for %s (%s)\n", fp->cf_path, STRERR);
			return (-1);
		}
	}
	if (fp->cf_flags & CKPT_FLAGS_MAPFILE) {
		assert(fp->cf_fd == -1);
		return (0);
	}
	if ((tempfd = open(fp->cf_path, fp->cf_fflags)) < 0) {
               	struct stat sb;
                int error = oserror();
		/*
		 * if the dev is /dev/tty, or /dev/ttyqX, then use the tty of
		 * the restart proc.
		 */
		if (((stat(DEVTTY, &sb) == 0)&&(sb.st_rdev == fp->cf_rdev)) ||
		    ((ckpt_tty_name!=NULL)&&(stat(ckpt_tty_name, &sb) == 0)&&
		     (major(sb.st_rdev) == major(fp->cf_rdev)))) {
			if((tempfd = ckpt_restore_tty(tp, fp, &dev_null)) < 0)
				return -1;
		} else {
			setoserror(error);
			cerror("Cannot open %s (dev=0x%x) create %d (%s) %d\n", 
				fp->cf_path, fp->cf_rdev, create, STRERR);
			return (-1);
		}
	}
	if (fp->cf_fdflags && !dev_null)
		fcntl(tempfd, F_SETFD, fp->cf_fdflags);

	IFDEB1(cdebug("specfs %s: tempfd=%d fd=%d rdev=0x%x\n",
		fp->cf_path, tempfd, fp->cf_fd, fp->cf_rdev));

	if (fp->cf_auxsize) {
		if ((fp->cf_auxptr = (void *)malloc(fp->cf_auxsize)) == NULL) {
			cerror("malloc (%s)\n", STRERR);
			(void)close(tempfd);
			return (-1);
		}
		if (read(tp->sfd, fp->cf_auxptr, fp->cf_auxsize) < 0) {
			cerror("Failed to read device aux data (%s)\n", STRERR);
			(void)close(tempfd);
			free(fp->cf_auxptr);
			return (-1);
		}
		if (ckpt_restore_fdattr_special(fp, tempfd) < 0 && !dev_null) {
			(void)close(tempfd);
			free(fp->cf_auxptr);
			return (-1);
		}
		free(fp->cf_auxptr);
	}
	if (tempfd != fp->cf_fd) {
		/*
		 * There should *not* be anything open on fp->cf_fd.
		 */
#ifdef NS_WAR
		(void)close(fp->cf_fd);
#endif
#ifdef DEBUG
		if (fp->cf_fd <= 2)
			(void)close(fp->cf_fd);
#endif
		if ((fd = fcntl(tempfd, F_DUPFD, fp->cf_fd)) < 0) {
			cerror("ckpt_restore_one_specfile:fcntl\n(%s)\n", STRERR);
			return -1;
		}

		assert(fd == fp->cf_fd);
		(void)close(tempfd);
	}
	if (ckpt_set_stropts(fp->cf_fd, &fp->cf_stropts) < 0 && !dev_null)
		return (-1);

	/*
	 * for devs like /dev/mem & /dev/kmem
	 */
	if (fp->cf_offset)
		(void)lseek(fd, fp->cf_offset, SEEK_SET);

	return (0);
}

static int
ckpt_open_fd(ckpt_ta_t *ta, ckpt_f_t *fp)
{
	int fd;
	struct stat sb;
	char *path;
	/* REFERENCED */
	int tfd;

	switch (fp->cf_mode) {
	case CKPT_MODE_REPLACE:
	case CKPT_MODE_MERGE:
	case CKPT_MODE_IGNORE:
	case CKPT_MODE_APPEND:
	case CKPT_MODE_RELOCATE:
		path = fp->cf_path;
		break;
	case CKPT_MODE_SUBSTITUTE:
		path = fp->cf_newpath;
		break;
	}
	fd = ckpt_open(ta, path, fp->cf_fflags, 0);
	/*
	 * If open fails, try chmod'ing the file
	 */
	if ((fd < 0) && (oserror() == EACCES) &&
	    (stat(fp->cf_path, &sb) >= 0) &&
	    (chmod(fp->cf_path, S_IRWXU) >= 0)) {
		fd = ckpt_open(ta, path, fp->cf_fflags, 0);
		(void)chmod(fp->cf_path, sb.st_mode);
	}
	if (fd < 0) {
		cerror("failed to reopen file %s (%s)\n", path, STRERR);
		return -1;
	}
	switch (fp->cf_mode) {
	case CKPT_MODE_MERGE:
	case CKPT_MODE_REPLACE:
	case CKPT_MODE_SUBSTITUTE:
	case CKPT_MODE_RELOCATE:
		lseek(fd, fp->cf_offset, SEEK_SET);
		break;
	case CKPT_MODE_APPEND:
		lseek(fd, fp->cf_offset, SEEK_END);
		break;
	case CKPT_MODE_IGNORE:
		break;
	}
	if (fp->cf_fdflags)
		fcntl(fd, F_SETFD, fp->cf_fdflags);
	
	/*
	 * Make sure that we've opened on the original fd
	 */
	IFDEB1(cdebug("fd %d, cf_fd: %d\n", fd, fp->cf_fd));

	if (fd != fp->cf_fd) {
		/*
		 * There should *not* be anything open on
		 * fp->cf_fd.
		 */
#ifdef NS_WAR
		(void)close(fp->cf_fd);
#endif
#ifdef DEBUG
		if (fp->cf_fd <= 2)
			(void)close(fp->cf_fd);

#endif
		if ((tfd = fcntl(fd, F_DUPFD, fp->cf_fd)) < 0) {
			cerror("ckpt_open_fd: fcntl\n(%s)\n", STRERR);
			return -1;
		}
		assert(tfd == fp->cf_fd);
		(void)close(fd);
	}
	if (ckpt_set_stropts(fp->cf_fd, &fp->cf_stropts) < 0)
		return (-1);

	return 0;
}

static int
ckpt_recreate_file(ckpt_ta_t *ta, ckpt_f_t *fp)
{
	int fd;
	int tries = 0;

	if (fp->cf_flags & CKPT_FLAGS_PSEMA) {
		/*
		 * unlink any existing file to force state restore
		 */
		if (unlink(fp->cf_path) < 0) {
			if (oserror() != ENOENT) {
				cerror("psema unlink (%s)\n", STRERR);
				return(-1);
			}
		}
		fd = psema_cntl(PSEMA_OPEN, fp->cf_path, O_CREAT|O_EXCL|O_RDWR,
					fp->cf_fmode, fp->cf_psemavalue);
		if (fd < 0) {
			cerror("failed to open psema %s (%s)\n", fp->cf_path,
								STRERR);
			return (-1);
		}
		return (fd);
	}
retry_create:
	if ((fd = ckpt_open(ta, fp->cf_path, O_CREAT|O_TRUNC|O_RDWR, (int)fp->cf_fmode)) < 0) {
		if ((oserror() == EACCES) && (tries++ == 0)) {
			IFDEB1(cdebug("%s denied, unlinking\n",
				fp->cf_path));
			unlink(fp->cf_path);
			goto retry_create;
		}
		cerror("failed to open %s (%s)\n", fp->cf_path, STRERR);
		return -1;
	}
	return (fd);
}

int
ckpt_restore_one_ofile(ckpt_ta_t *tp, ckpt_f_t *fp)
{
	int rfd, wfd;
	int pipefd = -1;
	int err = 0;
	struct stat sb;
	int statdone = 0;

	switch (fp->cf_mode) {
	case CKPT_MODE_REPLACE:
	case CKPT_MODE_RELOCATE:
		assert(fp->cf_newpath[0]);

		IFDEB1(cdebug("restoring files:\n"));
		IFDEB1(cdebug("\tfrom:\t %s\n", fp->cf_newpath));
		IFDEB1(cdebug("\tto:\t %s\n", fp->cf_path));

		/*
		 * If the original file hasn't been modified since checkpoint,
		 * don't bother restoring.  Note..original file may not exist.
		 * May need to reset access time though...
		 */
		if ((S_ISFIFO(fp->cf_fmode) == 0) &&
		    ((fp->cf_flags & CKPT_FLAGS_PSEMA) == 0) &&
		    (stat(fp->cf_path, &sb) == 0)) {
			statdone++;

			if (TIMER_EQ(&sb.st_mtim, &fp->cf_mtime)) {
				IFDEB1(cdebug("Orig file unchanged..."
					"skipping replace\n"));
				break;
			}
		}
		/*
		 * Original file is newer or non-existant...must replace
		 */
		if ((rfd = ckpt_open(tp, fp->cf_newpath, O_RDONLY, 0)) < 0) {
			cerror("failed to open %s (%s)\n", fp->cf_newpath, STRERR);
			return -1;
		}
		/*
		 * Should we unlink saved file?
		 */
		if (S_ISFIFO(fp->cf_fmode)) {
			/*
			 * Need to hold pipe open until we get the fd in
			 * place below.  Data is flushed from pipe on last
			 * close.
			 */
			if ((pipefd = ckpt_restore_namedpipe(tp, fp)) < 0) {
				(void)close(rfd);
				return (-1);
			}
			(void)close(rfd);
			break;
		}
		if ((wfd = ckpt_recreate_file(tp, fp)) < 0) {
			(void)close(rfd);
			return (-1);
		}
		if (ckpt_copy_a_file(rfd, wfd) < 0) {
			(void)close(rfd);
			(void)close(wfd);
			return -1;
		}
		(void)close(rfd);
		(void)close(wfd);
		break;

	case CKPT_MODE_SUBSTITUTE:
		break;

	case CKPT_MODE_MERGE:

		if (!(fp->cf_fflags & O_APPEND))
			break;

		if (ckpt_fstat(tp, fp->cf_path, &sb) < 0) {
			cerror("stat failed on file %s (%s)\n", fp->cf_path, STRERR);
			return -1;
		}
		if (sb.st_size != fp->cf_length) {
			/*
			 * reset the file to it's oiginal length
			 */
			err = ckpt_truncate(tp, fp->cf_path, fp->cf_length);
			/*
			 * If trunc fails, try chmod'ing
			 */
			if ((err < 0)&&
			    (oserror() == EACCES)&&
			    (chmod(fp->cf_path, S_IRWXU) >= 0)) {
				err = truncate(fp->cf_path, fp->cf_length);
				(void)chmod(fp->cf_path, sb.st_mode);
			}
			if (err < 0) {
				cerror("ckpt_restore_one_ofile: truncate\n(%s)\n",
					STRERR);
				return -1;
			}
		}
		break;
	case CKPT_MODE_IGNORE:
	case CKPT_MODE_APPEND:
		break;
	}
	/*
	 * skip fd processing for mapped files
	 */
	if ((fp->cf_flags & CKPT_FLAGS_MAPFILE) == 0) {
		assert(fp->cf_fd >= 0);
		/*
		 * Now that file is re-established, need to open with
		 * appropriate fd and modes
		 */
		err = ckpt_open_fd(tp, fp);

	}
	/*
	 * Now that we have fd with proper flags or are going to return
	 * error, close pipe if any
	 */
	if (pipefd >= 0)
		close(pipefd);

	if (err == 0 && fp->cf_mode == CKPT_MODE_REPLACE) {
		err = ckpt_reset_ftimes(fp->cf_fd, fp, (statdone)? &sb:NULL);
	}
	return (err);
}

ckpt_flist_t *
ckpt_file_lookup(dev_t dev, ino_t ino, int *found)
{
	ckpt_flist_t *fp = cpr.ch_filelist;

	while (fp) {
		if ((fp->cl_ino == ino) && (fp->cl_dev == dev)) {
			*found = 1;
			return (fp);
		}
		fp = fp->cl_next;
	}
	/* 
	 * no other process has handled this file yet. Let's mark it since we'll
	 * handle it.
	 */
	if ((fp = (ckpt_flist_t *)malloc(sizeof(ckpt_flist_t))) == NULL) {
		cerror("ckpt_file_lookup: malloc(%s)\n", STRERR);
		return (NULL);
	}
	*found = 0;

	fp->cl_pid = -1;
	fp->cl_id = (caddr_t)(-1L);
	fp->cl_fd = -1;;
	fp->cl_ino = ino;
	fp->cl_dev = dev;

	fp->cl_next = cpr.ch_filelist;
	cpr.ch_filelist = fp;

	return (fp);
}

pid_t
ckpt_file_is_handled(ckpt_obj_t *co, dev_t dev, ino_t ino, int fd, caddr_t id)
{
	int found;
	ckpt_flist_t *fp;

	if ((fp = ckpt_file_lookup(dev, ino, &found)) == NULL)
		return (-1);

	if (found)
		return (fp->cl_pid);

	fp->cl_pid = co->co_pid;
	fp->cl_fd = fd;
	fp->cl_id = id;

	return (0);
}

/*
 * Check to see if this is an fd sharing sproc, and if so, have we encountered
 * a share member already
 */
int
ckpt_exam_fdsharing_sproc(ckpt_obj_t *co)
{
	ckpt_psi_t *psi = &co->co_pinfo->cp_psi;
	ckpt_sp_t *sproc;

	if (!IS_SPROC(co->co_pinfo) || !IS_SFDS(co->co_pinfo))
		/*
		 * Not an sproc, or not sharing fds
		 */
		return (0);

	for (sproc = cpr.ch_sproclist; sproc; sproc = sproc->sp_next) {
		if (sproc->sp_sproc == psi->ps_shaddr) {
			co->co_flags |= CKPT_CO_SPROCFD_HANDLED;
			return (0);
		}
	}
	/*
	 * First instance of shared fd sproc member
	 */
	if ((sproc = (ckpt_sp_t *)malloc(sizeof(ckpt_sp_t))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (-1);
	}
	sproc->sp_sproc = psi->ps_shaddr;
	sproc->sp_next = cpr.ch_sproclist;
	cpr.ch_sproclist = sproc;

	return (0);
}

#define	CKPT_FLOCK_WAIT	2

/*
 * mark it if this process owns the flock of the file
 */
void
ckpt_get_flocks(ckpt_f_t *fp, int fd, pid_t mypid)
{
	ckpt_fl_t **curpp = &fp->cf_flp;
	flock_t fl;

	fl.l_start = 0;

	while (1) {

		fl.l_len = 0;
		fl.l_whence = SEEK_SET;
		fl.l_type = F_WRLCK; /* need it even it's a RDLCK */

		if (fcntl(fd, F_GETLK, &fl) == -1) {
			if (oserror() != ENOLCK) {
				cnotice("ckpt_get_flocks:fcntl: F_GETLK(%s)\n", STRERR);
			}
			break;
		}
		IFDEB1(cdebug("dupfd %d lock: type=%d whnce=%d strt=%d len=%d "
			"sid=%d pid=%d\n", fd, fl.l_type, fl.l_whence, 
			fl.l_start, fl.l_len, fl.l_sysid, fl.l_pid));

		if (fl.l_type == F_UNLCK)
			break;

		/* XXX need to handle non-zero sysid for other systems */
		if (fl.l_sysid == 0 && fl.l_pid == mypid) {
			ckpt_fl_t *newp;

			/* we own at least one lock */
			fp->cf_flags |= CKPT_FLAGS_LOCKED;
			if ((newp = (ckpt_fl_t *)malloc(sizeof(ckpt_fl_t))) == NULL) {
				cerror("cannot malloc (%s)\n", STRERR);
				cnotice("file locks are not saved\n");
				return;
			}
			newp->fl_lock = fl;
			newp->fl_next = NULL;

			while (*curpp)
				curpp = &(*curpp)->fl_next;
			*curpp = newp;
		}
		if (fl.l_len == 0)
			break;

		fl.l_start = fl.l_start + fl.l_len;
	}
}

int
ckpt_set_flocks(ckpt_f_t *fp, int sfd)
{
	int count = 0;
	ckpt_fl_t cfl, *next;

	next = fp->cf_flp;
	while (next) {
		if (read(sfd, &cfl, sizeof(ckpt_fl_t)) == -1) {
			cerror("Failed to read per file lock (%s)\n", STRERR);
			return (-1);
		}
		next = cfl.fl_next;

		IFDEB1(cdebug("SETLK: fd %d type=%d whnce=%d strt=%d len=%d"
			"sid=%d pid=%d\n",
			fp->cf_fd, cfl.fl_lock.l_type, cfl.fl_lock.l_whence,
			cfl.fl_lock.l_start, cfl.fl_lock.l_len, 
			cfl.fl_lock.l_sysid, cfl.fl_lock.l_pid));
		/* 
		 * wait up a bit if the lock is held by others
		 */
		for (;;) {
			flock_t fl;

			if (count++ > 3) {
				setoserror(ECKPT);
				cerror("Failed to set a lock on file %s."
					"Abort...\n", fp->cf_path);
				return (-1);
			}
				
			if (fcntl(fp->cf_fd, F_SETLK, &cfl.fl_lock) != -1)
				break;
	
			if (fcntl(fp->cf_fd, F_GETLK, &fl) != -1 && 
							fl.l_sysid != -1) {
				cnotice("file %s is locked by process %d."
					"Wait...\n", fp->cf_path, fl.l_pid);
			} else {
				cnotice("cannot get a file lock for file %s."
					"Wait...\n", fp->cf_path);
			}

			IFDEB1(cdebug("FAILED:count=%d fd %d type=%d whnce=%d" 
				"strt=%d len=%d sid=%d pid=%d\n",
				count, fp->cf_fd, cfl.fl_lock.l_type, 
				cfl.fl_lock.l_whence, cfl.fl_lock.l_start, 
			  	cfl.fl_lock.l_len, cfl.fl_lock.l_sysid,
				cfl.fl_lock.l_pid));
	
			sleep(CKPT_FLOCK_WAIT*(count+1));
		}
	}
	return (0);
}
/*
 * Common file restore routine for all versions
 */
static int
_ckpt_restore_files(ckpt_ta_t *tp, ckpt_f_t *fp, ckpt_magic_t magic)
{
	int rv;

	IFDEB1(cdebug("openfile=%s\n", fp->cf_path));

	if (fp->cf_magic != magic) {
		setoserror(EINVAL);
		cerror("incorrect per openfile magic %8.8s (v.s. %8.8s)\n",
			&fp->cf_magic, &magic);
		return (-1);
	}
	/*
	 * restore speical files
	 */
	if (S_ISCHR(fp->cf_fmode) || S_ISBLK(fp->cf_fmode)) {
		if (ckpt_restore_one_specfile(tp, fp, 1))
			return (-1);
		return (0);
	}
	/*
	 * restore regular open files
	 */
	rv = ckpt_restore_one_ofile(tp, fp);

        if (rv)
		return (-1);

	if (fp->cf_flags & CKPT_FLAGS_LOCKED) {
		if (ckpt_set_flocks(fp, tp->sfd))
			return (-1);
	}
	return (0);
}

int
ckpt_restore_files(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_f_t file;

	if (read(tp->sfd, &file, sizeof (ckpt_f_t)) == -1) {
		cerror("Failed to read the per-file header (%s)\n", STRERR);
		return (-1);
	}
	return(_ckpt_restore_files(tp, &file, magic));
}

void
ckpt_unlink_files(ckpt_ul_t *ulp)
{
	while(ulp) {
		(void)unlink(ulp->ul_path);
		ulp = ulp->ul_next;
	}
}
/*
 * Common fd restore for all versions
 */
static int
_ckpt_restore_fds(ckpt_ta_t *tp, ckpt_f_t *fp, ckpt_magic_t magic)
{
	int fd, tfd, minfd = tp->pi.cp_maxfd + tp->pi.cp_npfds + 1;
	int prfd;
	char procname[20];
	uid_t uid;

	IFDEB1(cdebug("openfile=%s duppid=%d dupfd=%d\n", fp->cf_path,
		fp->cf_duppid, fp->cf_dupfd));

	if (fp->cf_magic != magic) {
		setoserror(EINVAL);
		cerror("mismatched magic %8.8s (vs. %8.8s)\n", &fp->cf_magic, &magic);
			return (-1);
	}
#ifdef NS_WAR
	(void)close(fp->cf_fd);
#endif
#ifdef DEBUG
	if (fp->cf_fd <= 2)
		close(fp->cf_fd);
#endif
	if (fp->cf_flags & CKPT_FLAGS_DUPFD) {
		/*
		 * dup of another fd already open by this file
		 */
		if ((fd = fcntl(fp->cf_dupfd, F_DUPFD, fp->cf_fd)) < 0) {
			cerror("fcntl failed (%s)\n", STRERR);
			return (-1);
		}
		assert(fd == fp->cf_fd);

	} else if (fp->cf_flags & CKPT_FLAGS_OPENFD) {
		if (S_ISCHR(fp->cf_fmode)||S_ISBLK(fp->cf_fmode)) {
			if (ckpt_restore_one_specfile(tp, fp, 0) < 0)
				return (-1);
		} else {
			if (ckpt_open_fd(tp, fp) < 0)
				return (-1);
		}
	} else if (fp->cf_flags & CKPT_FLAGS_INHERIT) {
		/*
		 * open the proc who handled the file, move /proc
		 * fd out of range, then dup fd from proc and
		 * move it, if necessary
		 */
                assert(getuid() == 0);
		uid = 0;
		if (ckpt_seteuid(tp, &uid, 1) < 0) {
                      cerror("seteuid (%s)\n", STRERR);
                      return (-1);
              	}
		sprintf(procname, "/proc/%5.5d", vtoppid(fp->cf_duppid));
		prfd = open(procname, O_RDONLY);

		if (ckpt_seteuid(tp, &uid, 0) < 0) {
                      cerror("seteuid (%s)\n", STRERR);
                      return (-1);
              	}
		if (prfd < 0) {
			cerror("Failed to open /proc %s (%s)\n", procname, STRERR);
			return (-1);
		}
		if (prfd < minfd && ckpt_move_fd(&prfd, minfd) < 0)
			return(-1);

		fd = ioctl(prfd, PIOCCKPTDUP, &fp->cf_dupfd);
		if (fd < 0) {
			cerror("ioctl PIOCCKPTDUP failed on fd %d: %s\n",
				fp->cf_dupfd, STRERR);
			return (-1);
		}
		if (fd != fp->cf_fd) {
			if ((tfd = fcntl(fd, F_DUPFD, fp->cf_fd)) < 0) {
				cerror("fcntl failed (%s)\n", STRERR);
				return (-1);
			}
			close(fd);
			fd = tfd;
		}
		assert(fd == fp->cf_fd);

		close(prfd);
	} else {
		setoserror(ECKPT);
		cerror("Failed to restore file %s due to unexpected file"
			" flags %x\n", fp->cf_path, fp->cf_flags);
		return (-1);
	}
	if (fp->cf_flags & CKPT_FLAGS_LOCKED) {
		if (ckpt_set_flocks(fp, tp->sfd))
			return (-1);
	}
	return (0);
}

int
ckpt_restore_fds(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_f_t file;

	if (read(tp->sfd, &file, sizeof (ckpt_f_t)) == -1) {
		cerror("Failed to read the per-file header (%s)\n", STRERR);
		return (-1);
	}
	return (_ckpt_restore_fds(tp, &file, magic));
}

ckpt_po_t *
ckpt_pipe_lookup(dev_t dev, ino_t ino)
{
	ckpt_po_t *list = cpr.ch_pipelist;

	while (list != NULL) {
		if ((list->cpo_dev == dev) && (list->cpo_ino == ino))
			return (list);
		list = list->cpo_link;
	}
	return (NULL);
}

ckpt_po_t *
ckpt_pipe_add(ckpt_obj_t *co, ckpt_p_t *pp, int *protofd)
{
	ckpt_po_t *new;

	if ((new = (ckpt_po_t *)malloc(sizeof(ckpt_po_t))) == NULL) {
		cerror("ckpt_pipe_add:malloc(%s)\n", STRERR);
		return (NULL);
	} 

	new->cpo_pid = co->co_pid;
	new->cpo_inst[0].fd = (*protofd)++;
	new->cpo_inst[0].flags = 0;
	new->cpo_inst[0].id = 0;
	new->cpo_inst[1].fd = (*protofd)++;
	new->cpo_inst[1].flags = 0;
	new->cpo_inst[1].id = 0;
	new->cpo_dev = pp->cp_dev;
	new->cpo_ino = pp->cp_ino;
	new->cpo_link = cpr.ch_pipelist;
	cpr.ch_pipelist = new;

	return (new);
}

/*
 * close prototype pipe object file descriptors
 */
void
ckpt_pipe_cleanup(ckpt_ta_t *tp)
{
	ckpt_p_t *pp;

	for (pp = tp->pp; pp; pp = pp->cp_next) {
		if (pp->cp_flags & CKPT_PIPE_HANDLED) {
			close(pp->cp_protofd[0]);
			close(pp->cp_protofd[1]);
		}
	}
}
		
int
ckpt_save_svr3fifo_data(char *path, int pipefd)
{
	int savefd;
	struct stat targsb;
	long cnt;
	char pipebuf[PIPE_BUF];
	pipepeek_t peek;

	if ((savefd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) < 0) {
		cerror("ckpt_save_pipe_data:open(%s)\n", STRERR);
		return -1;
	}
	/* Save the file with the same owner and group */
	if (fstat(pipefd, &targsb) < 0) {
		cerror("fstat failed on fd %d (%s)\n", pipefd, STRERR);
		return (-1);
	}
	IFDEB1(cdebug("change file %s to uid %d gid %d\n",
		path, targsb.st_uid, targsb.st_gid));
	
	if (fchown(savefd, targsb.st_uid, targsb.st_gid) < 0) {
		cerror("cannot change owner on file %s (%s)\n", path, STRERR);
                return (-1);
        }
	peek.pp_size = sizeof(pipebuf);
	peek.pp_buf = pipebuf;

	if ((cnt = ioctl(pipefd, I_PPEEK, &peek)) < 0) {
		cerror("ckpt_save_svr3fifo_data:pipe peek(%s)\n", STRERR);
		return -1;
	}
	if (cnt > 0) {

		if (write(savefd, pipebuf, cnt) < 0) {
			cerror("ckpt_save_svr3fifo_data:write(%s)\n", STRERR);
			return -1;
		}
	}
	(void)close(savefd);

	return (0);
}

int
ckpt_restore_pipe_data(char *path, int instance, int pipefd, int enoent_ok)
{
	int savefd;
	long cnt;
	char pipebuf[PIPE_BUF];
	int i;

	/*
	 * A quick hack to reconstruct the data file path based on the instace
	 */
	for (i = 0; *(path+i) != 0; i++);
	if (i == 0)
		return 0;

	*(path+i-1) = instance ? '1' : '0';

	IFDEB1(cdebug("restore pipe from file %s\n", path));

	if ((savefd = open(path, O_RDONLY)) < 0) {
		if (enoent_ok && oserror() == ENOENT)
			return (0);
		cerror("ckpt_restore_pipe_data:open(%s)\n", STRERR);
		return -1;
	}
	while (1) {
		if ((cnt = read(savefd, pipebuf, sizeof(pipebuf))) < 0) {
			cerror("ckpt_restore_pipe_data:read(%s)\n", STRERR);
			return -1;
		}
		if (cnt == 0)
			break;
		if (write(pipefd, pipebuf, cnt) < 0) {
			cerror("ckpt_restore_pipe_data:write(%s)\n", STRERR);
			return -1;
		}
	}
	(void)close(savefd);

	return 0;
}

static int
ckpt_restore_namedpipe(ckpt_ta_t *tp, ckpt_f_t *fp)
{
	int pipefd;
	int rfd;

	IFDEB1(cdebug("restore named pipe %s\n", fp->cf_path));

	/*
	 * We only want to write the fifo, but open for read/write to avoid
	 * sleeping in kernel waiting for a reader...
	 */
	if ((pipefd = ckpt_open(tp, fp->cf_path, O_RDWR, 0)) < 0) {
		if (oserror() == ENOENT) {
			/*
			 * fifo doesn't exist - create it and try to
			 * open it
			 */
			IFDEB1(cdebug("creating named pipe\n"));

			if (mkfifo(fp->cf_path, fp->cf_fmode) < 0) {
				cerror("ckpt_restore_namedpipe:mkfifo(%s)\n", STRERR);
				return (-1);
			}
			pipefd = open(fp->cf_path, O_RDWR);
		}
	}
	if (pipefd < 0) {
		cerror("ckpt_restore_namedpipe:open fifo(%s)\n", STRERR);
		return (-1);
	}
	if ((rfd = ckpt_open(tp, fp->cf_newpath, O_RDONLY, 0)) < 0) {
		cerror("ckpt_restore_namedpipe:open file(%s)\n", STRERR);
		close(pipefd);
		return (-1);
	}
	if (ckpt_copy_fifo_file(rfd, pipefd) < 0) {
		close(pipefd);
		close(rfd);
		return (-1);
	}
	close(rfd);

	return (pipefd);
}

/*
 * Save fifo data.  Use extended PEEK functionality so we don't
 * actually empty the pipe.
 */
int
ckpt_save_svr4fifo_data(char *path, int pipefd)
{
	int savefd;
	struct stat targsb;
	struct strreadn nreadn;
	struct strpeekn peekbuf;
	struct strbuf *databufp = &peekbuf.databuf;
	struct strbuf *ctlbufp = &peekbuf.databuf;
	char *databuf = NULL;
	int datasize = 0;
	char junk[2];

	IFDEB1(cdebug("named pipe:saving fd %d, to %s\n", pipefd, path));

	if ((savefd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR)) < 0) {
		cerror("ckpt_save_pipe_data:open(%s)\n", STRERR);
		return -1;
	}
	/* Save the file with the same owner and group */
	if (fstat(pipefd, &targsb) < 0) {
		cerror("fstat failed on fd %d (%s)\n", pipefd, STRERR);
		return (-1);
	}
	IFDEB1(cdebug("change file %s to uid %d gid %d\n",
		path, targsb.st_uid, targsb.st_gid));
	if (fchown(savefd, targsb.st_uid, targsb.st_gid) < 0) {
		cerror("cannot change owner on file %s (%s)\n", path, STRERR);
                return (-1);
        }
	ctlbufp->maxlen = 2;
	ctlbufp->buf = junk;

	for (nreadn.msgnum = 0; ; nreadn.msgnum++) {
		if (ioctl(pipefd, I_NREADN, &nreadn) < 0) {
			if (oserror() == ENODATA)
				break;
			cerror("ioctl:I_READN(%s)\n", STRERR);
			return (-1);
		}
		if (nreadn.datasz == 0)
			continue;
		if ((databuf == NULL)||(datasize < nreadn.datasz)) {
			if (databuf)
				free(databuf);
			databuf = malloc(nreadn.datasz);
			if (databuf == NULL) {
				cerror("malloc(%s)\n", STRERR);
				return (-1);
			}
			datasize = nreadn.datasz;

			databufp->maxlen = datasize;
			databufp->buf = databuf;
		}
		peekbuf.msgnum = nreadn.msgnum;

		if (ioctl(pipefd, I_PEEKN, &peekbuf) < 0) {
			cerror("ioctl:I_PEEKN(%s)\n", STRERR);
			return (-1);
		}
		assert(databufp->len ==  datasize);

		if (write(savefd, databuf, datasize) < 0) {
			cerror("ckpt_save_svr4pipe_data:write(%s)\n", STRERR);
			return -1;
		}
	}
	(void)close(savefd);

	return 0;
}

int
ckpt_restore_pipefds(ckpt_ta_t *tp)
{
	char procname[20];
	ckpt_p_t *pp;
	int minfd = tp->pi.cp_maxfd+1;
	int fd, prfd, tfd;

	for (pp = tp->pp; pp; pp = pp->cp_next) {
#ifdef NS_WAR
		(void)close(pp->cp_fd);
#endif
#ifdef DEBUG
		if (pp->cp_fd <= 2)
			(void)close(pp->cp_fd);
#endif
		if (get_mypid() == vtoppid(pp->cp_duppid)) {
			/*
			 * This proc created the pipe, so just dup
			 */
			if ((fd = fcntl(pp->cp_dupfd, F_DUPFD, pp->cp_fd)) < 0) {
				cerror("ckpt_restore_pipefds:fcntl(%s)\n", STRERR);
				return (-1);
			}
		} else {
			/*
			 * Some other proc created the pipe.
			 */
			sprintf(procname, "/proc/%5.5d", vtoppid(pp->cp_duppid));
			prfd = open(procname, O_RDONLY);
			if (prfd < 0) {
				cerror("/proc open(%s)\n", STRERR);
				return (-1);
			}
			if (prfd < minfd  && ckpt_move_fd(&prfd, minfd) < 0)
				return(-1);
			fd = ioctl(prfd, PIOCCKPTDUP, &pp->cp_dupfd);
			if (fd < 0) {
				cerror("ioctl PIOCCKPTDUP failed on fd %d: %s\n",
					pp->cp_fd, STRERR);
				return (-1);
			}
			if (fd != pp->cp_fd) {
				if ((tfd = fcntl(fd, F_DUPFD, pp->cp_fd)) < 0) {
					cerror("fcntl failed (%s)\n", STRERR);
					return (-1);
				}
				close(fd);
				fd = tfd;
			}
			close(prfd);
		}
		assert(fd == pp->cp_fd);

	}
	return (0);
}

int
ckpt_add_unlinked(ckpt_obj_t *co, char *path)
{
	ch_t *ch = co->co_ch;
	ckpt_ul_t *ulp;

	ulp = (ckpt_ul_t *)malloc(sizeof(ckpt_ul_t));
	if (ulp == NULL) {
		cerror("Failed to alloc memory (%s)\n", STRERR);
		return (-1);
	}
	ulp->ul_magic = CKPT_MAGIC_UNLINKED;
	strcpy(ulp->ul_path, path);
	ulp->ul_next = ch->ch_ulp;
	ch->ch_ulp = ulp;

	return (0);
}

int
ckpt_write_unlinked(ch_t *ch, ckpt_prop_t *pp)
{
	ckpt_ul_t *ulp;
	long rc;

	for (ulp = ch->ch_ulp; ulp; ulp = ulp->ul_next) {
		CWRITE(ch->ch_sfd, ulp, sizeof(ckpt_ul_t), 1, pp, rc);
		if (rc < 0) {
			cerror("statefile write failure (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

int
ckpt_restore_unlinked(cr_t *cr, ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_ul_t unlinked;
	int sfd = (tp)? tp->ssfd : cr->cr_sfd;

	if (read(sfd, &unlinked, sizeof(unlinked)) < 0) {
		cerror("statefile read (%s)\n", STRERR);
		return (-1);
	}
	if (unlinked.ul_magic != magic) {
		setoserror(ECKPT);
		cerror("mismatched magic %8.8s (v.s. %8.8s)\n",
			&unlinked.ul_magic, &magic);
		return(-1);
	}
	if ((unlink(unlinked.ul_path) < 0)&&
	    ((cr->cr_pci.pci_flags & CKPT_PCI_ABORT) == 0)) {
		/* warn only */
		cnotice("restart failed to unlink %s (%s)\n", unlinked.ul_path,
									STRERR);
	}
	return (0);
}

int
ckpt_pfd_init(ckpt_obj_t *co, int nofiles)
{
	if (co->co_pfd != NULL)
		return (0);

	co->co_pfd = (ckpt_pfd_t *)malloc(nofiles * sizeof(ckpt_pfd_t));
	if (co->co_pfd == NULL) {
		cerror("failed to allocate fd table (%s)\n", STRERR);
		return (-1);
	}
	co->co_pfdidx = 0;
	return (0);
}

void
ckpt_pfd_add(ckpt_obj_t *co, ckpt_f_t *fp, dev_t dev, ino_t ino)
{
	int i = co->co_pfdidx++;

	assert(co->co_pfd);

	co->co_pfd[i].pfd_fd = fp->cf_fd;
	co->co_pfd[i].pfd_fflags = fp->cf_fflags & (O_RDONLY|O_WRONLY|O_RDWR);
	co->co_pfd[i].pfd_dev = dev;
	co->co_pfd[i].pfd_ino = ino;
}

int
ckpt_pfd_lookup(ckpt_obj_t *co, dev_t dev, ino_t ino, int maxprots)
{
	int i;

	for (i = 0; i < co->co_pfdidx; i++) {
		if ((co->co_pfd[i].pfd_dev == dev)&&
		    (co->co_pfd[i].pfd_ino == ino)) {
			/*
			 * got a vnode match...check access modes
			 */
			switch (maxprots & (PROT_READ|PROT_WRITE)) {
			case PROT_READ:
				if ((co->co_pfd[i].pfd_fflags == O_RDONLY)||
				    (co->co_pfd[i].pfd_fflags == O_RDWR))
					return (co->co_pfd[i].pfd_fd);
				break;
			case PROT_WRITE:	/* requires read access too! */
			case PROT_READ|PROT_WRITE:
				if (co->co_pfd[i].pfd_fflags == O_RDWR)
					return (co->co_pfd[i].pfd_fd);
				break;
			}
		}
	}
	return (-1);
}

void
ckpt_get_stropts(int fd, ckpt_str_t *str, int proconly)
{
	if (proconly) {
		/*
		 * get only options unique to this proc
		 */
		str->str_sigevents = 0;
		str->str_rdopts = 0;
		str->str_wropts = 0;
	} else {
		str->str_sigevents = 0;
		if (ioctl(fd, I_GRDOPT, &str->str_rdopts) < 0)
			str->str_rdopts = 0;
		if (ioctl(fd, I_GWROPT, &str->str_wropts) < 0)
			str->str_wropts = 0;
	}
#ifdef DEBUG_STROPTS
	if (str->str_sigevents)
		printf("fd %d sigevents %x\n", fd, str->str_sigevents);
	if (str->str_rdopts)
		printf("fd %d rdopts %x\n", fd, str->str_rdopts);
	if (str->str_wropts)
		printf("fd %d wropts %x\n", fd, str->str_wropts);
#endif
}

int
ckpt_set_stropts(int fd, ckpt_str_t *str)
{
#ifdef DEBUG_STROPTS
	if (str->str_sigevents)
		cdebug("fd %d sigevents %x\n", fd, str->str_sigevents);
	if (str->str_rdopts)
		cdebug("fd %d rdopts %x\n", fd, str->str_rdopts);
	if (str->str_wropts)
		cdebug("fd %d wropts %x\n", fd, str->str_wropts);
#endif
	/*
	 * gross hack to avoid restart proc from jobstopping attempting 
	 * to ioctl controlling tty.  restart really should catch SIGCLD
	 * and SIGCONT procs...but that's hard since it might be a childs
	 * child who stops...
	 */
	if (isatty(fd) && (!(cpr_flags & CKPT_RESTART_INTERACTIVE))) {
#ifdef DEBUG_STROPTS
		cdebug("skipping stropts on fd %d\n", fd);
#endif
		return (0);
	}
	if (str->str_sigevents) {
		if (ioctl(fd, I_SETSIG, str->str_sigevents) < 0) {
			cerror("I_SETSIG (%s)\n", STRERR);
			return (-1);
		}
	}
	if (str->str_rdopts) {
		if (ioctl(fd, I_SRDOPT, str->str_rdopts) < 0) {
			cerror("I_SRDOPT (%s)\n", STRERR);
			return (-1);
		}
	}
	if (str->str_wropts) {
		if (ioctl(fd, I_SWROPT, str->str_wropts) < 0) {
			cerror("I_SWROPT (%s)\n", STRERR);
			return (-1);
		}
	}
	return (0);
}

void
ckpt_get_psemavalue(int fd, ckpt_f_t *fp)
{
	if (psema_cntl(PSEMA_GETVALUE, fd, &fp->cf_psemavalue) == 0) {
		/*
		 * This fd really is a psema
		 */
		fp->cf_flags |= CKPT_FLAGS_PSEMA;
	}
}
/*
 * Check for an unsupported fd type
 */
char *
ckpt_unsupported_fdtype(ckpt_statbuf_t *sbuf)
{
	if (S_ISSOCK(sbuf->ckpt_mode))
		return ("socket");
	if (S_ISDIR(sbuf->ckpt_mode))
		return ("directory");
	if (strcmp(sbuf->ckpt_fstype, "proc") == 0)
		return ("/proc");
	return (NULL);
}
/*
 * Open a file.  First try as user.  If this works, or user is
 * creating, return result.  (Note, that for NFS mounted, this is more
 * powerful then trying as root, since root gets mapped to nobody).
 * If it fails, then try as root, then check permission on underlying object.
 */
int 
ckpt_open(ckpt_ta_t *ta, char *pathname, int oflag, int mode)
{
	uid_t was = 0;
	int fd, eflag = 0;

        fd = open(pathname, oflag, mode);
        if ((fd >= 0) || (oflag & O_CREAT))
                return (fd);

	if (ckpt_seteuid(ta, &was, 1) < 0)
		return (-1);

	fd = open(pathname, oflag, mode);

	if (ckpt_seteuid(ta, &was, 0) < 0) {
		close(fd);
		return(-1);
	}
	if (fd < 0)
		return (-1);

	if((oflag&0x3) == O_RDONLY) eflag =  R_OK;
	if((oflag&0x3) == O_WRONLY) eflag =  W_OK;
	if((oflag&0x3) == O_RDWR  ) eflag =  R_OK|W_OK;

	/* we want to check if the user has requested permission to access */
	if (syssgi(SGI_CKPT_SYS, CKPT_FDACCESS, fd, eflag|EFF_ONLY_OK) < 0) {
		close(fd);
		return (-1);
	}
	return (fd);
}

/* 
 * like ckpt_open(), we also need ckpt_fstat for the same reason.
 */
static int
ckpt_fstat(ckpt_ta_t *ta, char *pathname, struct stat *buf)
{
	int fd, rv = 0;
	if((fd = ckpt_open(ta, pathname, O_RDONLY, 0)) < 0) {
		cerror("ckpt_fstat: open %s (%s)\n", pathname, STRERR);
		return -1;
	}
	if(fstat(fd, buf))  {
		cerror("fstat: %s fd= %d (%s)\n", pathname, fd, STRERR);
		rv = -1;
	}
	close(fd);
	return rv;
}

static int ckpt_truncate(ckpt_ta_t *ta, char *pathname, off_t len)
{
	int fd, rv = 0;
	if((fd = ckpt_open(ta, pathname, O_RDWR, 0)) < 0) {
		cerror("ckpt_truncate: open %s (%s)\n", pathname, STRERR);
		return -1;
	}
	rv = ftruncate(fd, len);
	close(fd);
	return rv;
}

#ifdef DIO
int
ckpt_enable_dio(char *file, int fd, struct dioattr *dioinfop)
{
	if (fcntl(fd, F_SETFL, FDIRECT) < 0) {
		cnotice("failed to set dio on %s (%s)\n", file, STRERR);
		return (-1);
	}
	if (fcntl(fd, F_DIOINFO, dioinfop) < 0) {
		cnotice("failed to get dio info on %s (%s)\n", file, STRERR);
		ckpt_disable_dio(file, fd, 1);
		return (-1);
	}
	return (0);
}

void
ckpt_disable_dio(char *file, int fd, int warn)
{
	if (warn)
		cnotice("disabling direct io to %s\n", file);

	(void)fcntl(fd, F_SETFL, 0);
}
#endif
#ifdef XXX_DEBUG_FDS
static void
ckpt_dump_fds(int sfd, ckpt_po_t *plist)
{
	cdebug("sfd %d\n", sfd);

	while(plist) {
		cdebug("pipe(%x,%d), fds(%d,%d), ids(%lx,%lx)\n",
			plist->cpo_dev,
			plist->cpo_ino,
			plist->cpo_inst[0].fd,
			plist->cpo_inst[1].fd,
			plist->cpo_inst[0].id,
			plist->cpo_inst[1].id);
		plist = plist->cpo_link;
	}
}
#endif

static int ckpt_restore_tty(ckpt_ta_t *tp, ckpt_f_t *fp, int *dev_null)
{
	int tempfd;
	extern char *ckpt_tty_name;
		
	if(tp->pi.cp_ttydev == PRNODEV) {
		/* no control terminal for this proc,
		 * just open /dev/null. This is the case where
		 * a process does a setsid() and become the 
		 * session leader, thus loses the old tty, 
		 * and never open new ttys later. The fd 0,
		 * 1,2, however, were not closed.
		 */
		if(ckpt_tty_name != NULL) 
			tempfd = open(ckpt_tty_name, fp->cf_fflags);
		else 
			tempfd = open("/dev/null", fp->cf_fflags);

		IFDEB1(cdebug("No controlling terminal: "
			"process %d fd %d tempfd %d, tty %s\n", 
			getpid(), fp->cf_fd, tempfd, ckpt_tty_name));
		*dev_null = 1;
	} else { 
		/* a session, such as a login was 
		 * ckeckpointed. Open tty through /dev/tty.
		 */
		*dev_null = 0;
		if(ckpt_tty_name != NULL) 
			tempfd = open(ckpt_tty_name, fp->cf_fflags);
		else {
			/* we really need to open a new tty, but not sure */
			tempfd = open("/dev/null", fp->cf_fflags);
			*dev_null = 1;
		}
		IFDEB1(cdebug("proc %d open %s fd %d tempfd %d\n", 
				getpid(), ckpt_tty_name, fp->cf_fd, tempfd));
	}
	if(tempfd <0) {
		/* Failed attmpt on /dev/tty */
		cerror("Process %d unable open /dev/tty on "
			"fd %d,  has no controlling tty, callers tty_name %s\n",
			getpid(), fp->cf_fd, ckpt_tty_name);
		setoserror(ECKPT);
		return (-1);
	}

	return  tempfd;
}

