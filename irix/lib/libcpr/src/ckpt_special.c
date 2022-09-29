/*
 * ckpt_special.c
 *
 *      Routines for saving/restoring device state, especially for 'unusal'
 *	devices.  Inspired by the desire to support the ccsync device.
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

#ident "$Revision: 1.30 $"

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/ckpt_sys.h>
#include <sys/mkdev.h>
#include <sys/EVEREST/ccsync.h>
#include <termios.h>
#undef _KMEMUSER
#include <sys/usioctl.h>
#define _KMEMUSER
#include <sys/pmo.h>	/* fetchops.h needs pmo_handle_t defined */
#include <fetchop.h>
#include "ckpt.h"
#include "ckpt_internal.h"

/*
 * Routines for dealing with special handling of devices
 */

/*
 * deices supported with cpr
 */
static struct ckpt_supdev {
	char	*path;
	dev_t	rdev;
	int	minor;
	int	valid;
} ckpt_devlist[] = {	{ "/dev/tty",		0, 1, 0},
			{ "/dev/console",	0, 1, 0},
			{ "/dev/null",		0, 1, 0},
			{ "/dev/zero",		0, 1, 0},
			{ "/dev/mem",		0, 1, 0},
			{ "/dev/mmem",		0, 1, 0},
			{ "/dev/kmem",		0, 1, 0},
			{ "/dev/ccsync",	0, 0, 0},
			{ "/dev/usema",		0, 0, 0},
			{ "/dev/usemaclone",	0, 0, 0}};

static int ckpt_devlist_size = sizeof(ckpt_devlist)/sizeof(struct ckpt_supdev);
static int ckpt_devlist_init = 0;

void
ckpt_warn_unsupported(ckpt_obj_t *co, ckpt_f_t *fp, int fd, void *vaddr)
{
	char *path = NULL;
	int i;
	struct stat sb;

	if (!ckpt_devlist_init) {
		for (i = 0; i < ckpt_devlist_size; i++) {
			if (stat(ckpt_devlist[i].path, &sb) == 0) {
				ckpt_devlist[i].rdev = sb.st_rdev;
				ckpt_devlist[i].valid = 1;
			}
		}
		ckpt_devlist_init = 1;
	}
	for (i = 0; i < ckpt_devlist_size; i++) {
		/*
		 * skip non-existant devices
		 */
		if (ckpt_devlist[i].valid == 0)
			continue;
		if ((major(fp->cf_rdev) == major(ckpt_devlist[i].rdev))&&
		    ((!ckpt_devlist[i].minor)||
		     (minor(fp->cf_rdev) == minor(ckpt_devlist[i].rdev))))
			/*
			 * got a match
			 */
			return;
	}
	/*
	 * print a warning
	 */
	if (vaddr) {
		cnotice("pid %d:unsupported memory mapped device @%lX\n",
			co->co_pid, vaddr);
	} else if (ckpt_pathname(co, fd, &path) == 0) {
		cnotice("pid %d:fd %d:%s:unsupported device\n",
			co->co_pid,
			fp->cf_fd,
			path);
		free(path);
	} else {
		cnotice("pid %d:fd %d:unsupported device\n",
			co->co_pid,
			fp->cf_fd);
	}
}

/*
 * devices requiring special handling
 */
/*
 * CCSYNC defines and data structs
 */
#define	MAXCCSYNCDEV	16

static int validccsync_dev;
static dev_t ccsync_dev = 0;
/*
 * Following ref'd from ckpt_revision.c
 */
int validusema_dev;
dev_t usema_dev = 0;
dev_t usemaclone_dev = 0;

static struct ccsync_fd_s {
	dev_t	cc_dev;		/* file system dev */
	ino_t	cc_ino;		/* inode */
	dev_t	cc_rdev;	/* actual ccsync device with grp encoded */
	int	cc_fd;		/* fd that device is/will be opened on */
	pid_t	cc_pid;
} ccsync_inst[MAXCCSYNCDEV];
static int ccsync_count = 0;

static int dev_init = 0;

int
ckpt_is_special(dev_t rdev)
{
	if (dev_init == 0) {
		struct stat sb;

		dev_init++;

		if (stat(DEVCCSYNC, &sb) < 0) {
			if (oserror() != ENOENT) {
				cerror("cannot stat %s: %s\n", DEVCCSYNC, STRERR);
				return (-1);
			}
		} else {
			ccsync_dev = sb.st_rdev;
			validccsync_dev = 1;
		}
		/*
		 * Set validusema_dev only after getting both usema devs
		 */
		if (stat(DEVUSEMA, &sb) < 0) {
			if (oserror() != ENOENT) {
				cerror("cannot stat %s: %s\n", DEVCCSYNC, STRERR);
				return (-1);
			}
		} else
			usema_dev = sb.st_rdev;

		if (stat(DEVUSEMACLONE, &sb) < 0) {
			if (oserror() != ENOENT) {
				cerror("cannot stat %s: %s\n", DEVCCSYNC, STRERR);
				return (-1);
			}
		} else {
			usemaclone_dev = sb.st_rdev;
			validusema_dev = 1;
		}
	}
	if (validccsync_dev && (major(rdev) == major(ccsync_dev)))
		return (1);
	if (validusema_dev && (major(rdev) == major(usema_dev)))
		return (1);
	if (validusema_dev && (major(rdev) == major(usemaclone_dev)))
		return (1);

	return (0);
}

static int
ckpt_ccsync_lookup(dev_t dev, ino_t ino, dev_t rdev)
{
	int i;

	for (i = 0; i < ccsync_count; i++) {
		if (dev == ccsync_inst[i].cc_dev &&
		    ino == ccsync_inst[i].cc_ino &&
		    rdev == ccsync_inst[i].cc_rdev)
		return (i);
	}
	return (-1);
}

int
ckpt_fdattr_special(ckpt_obj_t *co, ckpt_f_t *fp, dev_t dev, ino_t ino, int fd)
{
	int rc;
	static struct stat console_sb;
	static int console_statdone = 0;
	static struct stat tty_sb;
	static int tty_statdone = 0;
#ifdef DEBUG_USEMA
	int once = 0;
#endif
	if (isatty(fd)) {
		struct termios *tip;
		/*
		 * This should probably be generalized, but console is
		 * only funny tty device so far
		 */
		if (!console_statdone) {
			if (stat(DEVCONSOLE, &console_sb) < 0) {
				cerror("stat");
				return (-1);
			}
			console_statdone++;
		}
		if (fp->cf_rdev == console_sb.st_rdev)
			return (0);
		/*
		 * replace dev
		 */
		if (!tty_statdone) {
			if (stat(DEVTTY, &tty_sb) < 0) {
				cerror("stat");
				return (-1);
			}
			tty_statdone++;
		}
		fp->cf_rdev = tty_sb.st_rdev;

		tip = (struct termios *)malloc(sizeof(struct termio));
		if (tip == NULL) {
			cerror("malloc (%s)\n", STRERR);
			return (-1);
		}
		if (tcgetattr(fd, tip) < 0) {
			cerror("termios (%s)\n", STRERR);
			free(tip);
			return (-1);
		}
		fp->cf_auxptr = (void *)tip;
		fp->cf_auxsize = sizeof(struct termios);

		return (0);
	}
	if ((rc = ckpt_is_special(fp->cf_rdev)) <= 0) {
		return (rc);
	}
	if (validccsync_dev && (major(fp->cf_rdev) == major(ccsync_dev))) {
		int inst;

		if (ckpt_ccsync_lookup(dev, ino, fp->cf_rdev) >= 0) {
			/*
			 * must have inherited this fd...ignore it
			 */
			return (0);
		}
		inst = ccsync_count++;
		ccsync_inst[inst].cc_dev = dev;
		ccsync_inst[inst].cc_ino = ino;
		ccsync_inst[inst].cc_rdev = fp->cf_rdev;
		ccsync_inst[inst].cc_fd = fp->cf_fd;
		ccsync_inst[inst].cc_pid = co->co_pid;
		/*
		 * Replace actual dev with generic...we can't open, nor do we
		 * need to, on restart..
		 */
		fp->cf_rdev = ccsync_dev;

	} else if (validusema_dev && (major(fp->cf_rdev) == major(usema_dev))) {
		ussemastate_t *ussema;
		int nproc = co->co_ch->ch_nproc;
#ifdef DEBUG_USEMA
		int i;
#endif
		if ((minor(fp->cf_rdev) == minor(usema_dev))||
		    (minor(fp->cf_rdev) == minor(usemaclone_dev))) {
			return (0);
		}
		ussema = (ussemastate_t *)malloc(sizeof(ussemastate_t) +
				(nproc * sizeof(struct ussematidstate_s)));
		if (ussema == NULL) {
			cerror("failed to allocate memory (%s)\n", STRERR);
			return (-1);
		}
		ussema->ntid = nproc;
		ussema->tidinfo = (struct ussematidstate_s *)
				((char *)ussema + sizeof(ussemastate_t));

		if (ioctl(fd, UIOCGETSEMASTATE, ussema) < 0) {
			cerror("failed to get ussema state (%s)\n", STRERR);
#ifdef DEBUG_USEMA
			printf("proc [%d] usema %d\n", co->co_pid, minor(fp->cf_rdev));
#endif
			free(ussema);
			return (-1);
		}
		if (ussema->nthread > nproc) {
			cerror("process %d detected procs not being checkpointed sharimg ussema on fd %d\n", co->co_pid, fp->cf_fd);
			free(ussema);
			return (-1);
		}
		assert(ussema->nfilled == ussema->nthread);

#ifdef DEBUG_USEMA
		for (i = 0; i < ussema->nfilled; i++) {
			if (once++ == 0) {
				printf("sema %d filled %d proc %d\n",
					minor(fp->cf_rdev),
					ussema->nfilled,
					ussema->nproc);
			}
			printf("pid %d count %d\n",
				ussema->pidinfo[i].pid,
				ussema->pidinfo[i].count);
		}
#endif
		/*
		 * Attach this info to file struct
		 */
		fp->cf_auxptr = ussema;
		fp->cf_auxsize = (int)(sizeof(ussemastate_t) +
	                            (nproc * sizeof(struct ussematidstate_s)));
	}
	return (0);
}

int
ckpt_restore_fdattr_special(ckpt_f_t *fp, int fd)
{
	int rc;
#ifdef DEBUG_USEMA
	int once = 0;
#endif

	if ((rc = ckpt_xlate_fdattr_special(fp, fd)) != 0) {
		if (rc < 0)
			/*
			 * Error
			 */
			return (rc);
		if (rc > 0)
			/*
			 * Restored
			 */
			return (0);
	}
	if (isatty(fd)) {
		struct termios termios;
		struct termios *tiop = (struct termios *)fp->cf_auxptr;
		/*
		 * Get current settings
		 */
		if (tcgetattr(fd, &termios) < 0) {
			cerror("tcgetattr (%s)\n", STRERR);
			return (-1);
		}
		tiop->c_cflag &= ~(CBAUD|CIBAUD); 
		termios.c_cflag &= ~(CBAUD|CIBAUD); 
		/*
		 * If all the attrs are the same, don't bother messing
		 * with them
		 */
		if ((tiop->c_iflag == termios.c_iflag)&&
		    (tiop->c_oflag == termios.c_oflag)&&
		    (tiop->c_cflag == termios.c_cflag)&&
		    (tiop->c_lflag == termios.c_lflag)&&
		    (tiop->c_ispeed == termios.c_ispeed)&&
		    (tiop->c_ospeed == termios.c_ospeed))
			return (0);
		/*
		 * Only do tcsetattr if interesting bits are different...
		 * basically a heuristic to see if app called tcsetattr or
		 * equiv 
		 * Really should have kernel support to tell if his was
		 * done by app...
		 */
#define	TCIFLAGS (INLCR|ICRNL|IUCLC)
#define	TCOFLAGS (OLCUC|ONLCR|OCRNL|ONOCR|ONLRET|TAB1|TAB2|TAB3)
#define	TCLFLAGS (ICANON|ECHO|ECHOE|ECHOK|ECHONL)

		if (((tiop->c_iflag & TCIFLAGS) ==
				(termios.c_iflag & TCIFLAGS)) &&
		    ((tiop->c_oflag & TCOFLAGS) ==
				(termios.c_oflag & TCOFLAGS)) &&
		    ((tiop->c_lflag & TCLFLAGS) ==
				(termios.c_lflag & TCLFLAGS)))
			return (0);

		/*
		 * Don't allow restart if control or speed mis-match
		 */
		if((tiop->c_cflag != termios.c_cflag)||
		   (tiop->c_ospeed != termios.c_ospeed)||
		   (tiop->c_ispeed != termios.c_ispeed)) {
			cerror("fd %d, mis-match on tty hardware or speed settings\n", fp->cf_fd);
			return (-1);
		}
		/*
		 * If we're going to mess with the tty, make
		 * sure this is an interactive restart...
		 */
		if (!(cpr_flags & CKPT_RESTART_INTERACTIVE)) {
			cerror("fd %d requires interactive restart\n", fp->cf_fd);
#ifdef DEBUG
			cdebug("termios iflag cur %x statefile %x \n", termios.c_iflag, tiop->c_iflag);
			cdebug("termios oflag cur %x statefile %x \n", termios.c_oflag, tiop->c_oflag);
			cdebug("termios cflag cur %x statefile %x \n", termios.c_cflag, tiop->c_cflag);
			cdebug("termios lflag cur %x statefile %x \n", termios.c_lflag, tiop->c_lflag);
#endif
			return (-1);
		}
		if (tcsetattr(fd, TCSANOW, tiop) < 0) {
			cerror("tcsetattr (%s)\n", STRERR);
			return (-1);
		}
		return (0);
	}
	if ((rc = ckpt_is_special(fp->cf_rdev)) <= 0) {
		return (rc);
	}
	if (validusema_dev && (major(fp->cf_rdev) == major(usemaclone_dev))) {
		ussemastate_t *ussema = (ussemastate_t *)fp->cf_auxptr;

		if ((minor(fp->cf_rdev) == minor(usema_dev))||
		    (minor(fp->cf_rdev) == minor(usemaclone_dev))) {
			assert(ussema == NULL);
			return (0);
		}
		ussema->tidinfo = (struct ussematidstate_s *)
				(       (char *)ussema + sizeof(ussemastate_t));

		assert(ussema->nthread == ussema->nfilled);

		if (ussema->nthread || ussema->nprepost) {
			ussema->ntid = ussema->nthread;
	
			if (ioctl(fd, UIOCSETSEMASTATE, ussema) < 0) {
				cerror("set ussema state (%s)\n", STRERR);
				return (-1);
			}
		}
		return (0);
	}
	cerror("Unexpected device aux data (fd %d)\n", fp->cf_fd);
	return (-1);
}

int
ckpt_mapattr_special(	ckpt_obj_t *co,
			ch_t *ch,
			ckpt_memmap_t *memmap,
			struct stat *sbp)
{
	char path[CPATHLEN];
	dev_t rdev;
	int devfd;
	int i;
	ckpt_ccsync_t *ccsync;

	if (!ckpt_is_special(memmap->cm_rdev))
		return (0);

	rdev = memmap->cm_rdev;
	/*
	 * get a handle to the dev
	 */
	sprintf(path, "%s/ckpt_spec_%d_%x", ch->ch_path, co->co_pid, rdev);

	if (mknod(path, S_IFCHR|S_IRUSR, rdev) < 0) {
		cerror("cannot mknod %s (dev=0x%x): %s\n", path, rdev, STRERR);
		return (-1);
	}
	if ((devfd = open(path, O_RDONLY)) < 0) {
		cerror("cannot open %s (dev=0x%x): %s\n", path, rdev, STRERR);
		(void)unlink(path);
		return (-1);
	}
	(void)unlink(path);
	/*
	 * retrieve contents
	 */
	if (validccsync_dev && (major(rdev) == major(ccsync_dev))) {
		memmap->cm_auxptr = ccsync = malloc(sizeof(ckpt_ccsync_t));
		memmap->cm_auxsize = sizeof(ckpt_ccsync_t);
		if (memmap->cm_auxptr == NULL) {
			cerror("cannot allocate memory: %s\n", STRERR);
			close(devfd);
			return (-1);
		}
		if (ioctl(devfd, CCSYNCIO_INFO, &ccsync->cc_info) < 0) {
			cerror("cannot retrieve info (dev=0x%x): %s\n", rdev, STRERR);
			free(memmap->cm_auxptr);
			close(devfd);
			return (-1);
		}
		close(devfd);

		/* we don't care which grp at restart */
		memmap->cm_rdev = ccsync_dev;
		memmap->cm_pid = ccsync->cc_info.ccsync_master;

		if (memmap->cm_pid == co->co_pid) {
			/*
			 * If fd still open for device, record it and use it at
			 * restart to map.  Otherwise, we'll open a tmp fd at
			 * restart.
			 * (We don't want any unneccessay opens of ccsync...
			 * there are only 16 in the system.)
			 */
			i = ckpt_ccsync_lookup(	sbp->st_dev,
						sbp->st_ino,
						sbp->st_rdev);
			if (i >= 0) {
				/*
				 * should check that pidsw match or at
				 * least are in same sproc
				 * TBD
				 */
				ccsync->cc_fd = ccsync_inst[i].cc_fd;
			} else
				ccsync->cc_fd = -1;
		} else {
			/*
			 * Check that it's okay for us to have this
			 */
			/* need to check that pid is a share member and sharing
			 * addr with this proc (TBD)
			 */
			free(memmap->cm_auxptr);
			memmap->cm_auxptr = NULL;
			memmap->cm_auxsize = 0;
			return (0);
		}
	} else {
		assert(0);
		return (-1);
	}
	return (0);
}

/* ARGSUSED */
int
ckpt_handle_mapspecial(	ckpt_obj_t *co,
			ch_t *ch,
		    	ckpt_memmap_t *memmap)
{
	if (!ckpt_is_special(memmap->cm_rdev))
		return (0);

	if (validccsync_dev && (major(memmap->cm_rdev) == major(ccsync_dev)))
		memmap->cm_xflags |= CKPT_SPECIAL;

	return (1);
}

int
ckpt_remap_disp_special(ckpt_memmap_t *memmap)
{
	if (validccsync_dev && (major(memmap->cm_rdev) == major(ccsync_dev))) {
		memmap->cm_cflags |= CKPT_SAVESPEC;
		return (0);
	}
	assert(0);
	return (0);
}

/*
 * Routines for dumping memory of special mapped memory
 */
/*
 * SN0 fetchop memory
 */
#define MAX_FETCHOP_CNT CKPT_IOBSIZE/FETCHOP_VAR_SIZE

static int
ckpt_save_fetchop(ckpt_obj_t *co, ckpt_seg_t *mapseg)
{
	int prfd = co->co_prfd;
	int mfd = co->co_mfd;
	char *membuf;
	fetchop_var_t *fetchopp;
	off_t fetchop_cnt;
	off_t off;

	membuf = malloc(CKPT_IOBSIZE);
	if (membuf == NULL) {
		cerror("malloc (%s)\n", STRERR);
		return (-1);
	}
	fetchopp = (fetchop_var_t *)membuf;
	fetchop_cnt = 0;
	/*
	 * Only read the fetchop variable, but fully populate the file,
	 * to make it easier to read in and restore
	 */
	for (off = 0; off < mapseg->cs_len; off += FETCHOP_VAR_SIZE) {

		if (lseek(prfd, (off_t)mapseg->cs_vaddr + off, SEEK_SET) < 0) {
			cerror("fetchop lseek (%s)\n", STRERR);
			free(membuf);
			return (-1);
		}
		if (read(prfd, fetchopp, sizeof(fetchop_var_t)) < 0) {
			cerror("fetchop read (%s)\n", STRERR);
			free(membuf);
			return (-1);
		}
		if (++fetchop_cnt >= MAX_FETCHOP_CNT) {

			IFDEB1(cdebug("fetchop write buf %d vars\n", fetchop_cnt));

			if (write(mfd, membuf, fetchop_cnt*FETCHOP_VAR_SIZE) < 0) {
				cerror("fetchop write (%s)\n", STRERR);
				free(membuf);
				return (-1);
			}
			fetchop_cnt = 0;
			fetchopp = (fetchop_var_t *)membuf;
		} else
			fetchopp = (fetchop_var_t*)(((char *)fetchopp) + FETCHOP_VAR_SIZE);
	}
	if (fetchop_cnt) {

		IFDEB1(cdebug("fetchop flush buf %d vars\n", fetchop_cnt));

		if (write(mfd, membuf, fetchop_cnt*FETCHOP_VAR_SIZE) < 0) {
			cerror("fetchop write (%s)\n", STRERR);
			free(membuf);
			return (-1);
		}
	}
	free(membuf);
	return (0);
}

int
ckpt_save_special(ckpt_obj_t *co, ckpt_memmap_t *memmap, ckpt_seg_t *mapseg)
{

	if (validccsync_dev && (major(memmap->cm_rdev) == major(ccsync_dev)))
		return (0);

	else if (memmap->cm_xflags & CKPT_FETCHOP) {
		int i;
		for (i = 0; i < memmap->cm_nsegs; i++) {
			int error;
			/* only save fetchoped segments, others are saved by
			 * ckpt_save_modifed().
			 */
			if (mapseg[i].cs_notcached != 0) {
				error = ckpt_save_fetchop(co, &mapseg[i]);
				if (error)
					return (error);
			}
		}
		return (0);
	}

	cerror("ckpt_save_special: unsupported mapped device (major %d)\n",
			major(memmap->cm_rdev));
	setoserror(ECKPT);
	return (-1);
}

