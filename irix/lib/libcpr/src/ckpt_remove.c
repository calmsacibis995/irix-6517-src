/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.29 $"

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/wait.h>
#include "ckpt.h"
#include "ckpt_internal.h"

static int ckpt_remove_one_statefile(pid_t pid);
static int ckpt_remove_proc_index(const char *);
static int ckpt_undir(const char *, int);

static int
ckpt_remove_inplace(void)
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
        }
        /*
         * If we get here, must fork/exec cpr
         */
        return (0);
#endif
}

int
ckpt_remove(const char *path)
{
	int rc;
	pid_t child;
	int stat;

	if (ckpt_remove_inplace()) {
		if ((rc = ckpt_check_directory(path)))
			return (rc);
	
		if (rc = ckpt_remove_proc_index(path)) {
			if (rc == EACCES)
				return (-1);
	
			/* force to remove */
			if ((cpr_flags & CKPT_NQE) == 0)
				printf("Trying to remove corrupted statefile %s\n", path);
		}
		if (rc = ckpt_undir(path, 0))
			return (rc);
	
		if ((rc = rmdir(path)) == 0) {
			if ((cpr_flags & CKPT_NQE) == 0)
				printf("Removed statefile %s successfully.\n", path);
			return (0);
		}
		cerror("directory %s not removed (%s)\n", path, STRERR);
		return (rc);
	} else {
		if ((child = fork()) == 0) {
			execl(CPR, CPR, "-D", path, NULL);
			cerror("Failed to exec cpr (%s)\n", STRERR);
			exit(1);
		}
		if (child < 0) {
	                cerror("Failed to fork (%s)\n", STRERR);
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
	}
	return (0);
}
/*
 * Remove call from checkpoint create if it has successfully craeted the
 * directory
 */
int
ckpt_create_remove(const char *path)
{
	int rc;

	if ((rc = ckpt_check_directory(path)))
		return (rc);

	if (rc = ckpt_undir(path, 1))
		return (rc);

	if (rc = rmdir(path))
		return (rc);

	return (0);
}

static int
ckpt_undir(const char *path, int force)
{
	char    *newpath;
	DIR     *name;
	struct direct *direct;
	struct stat buf;
	
	if ((name = opendir(path)) == NULL) {
		perror("ckpt_undir: opendir");
		return (-1);
	}
	while ((direct = readdir(name)) != NULL) {

		/* ignore "." and ".." */
		if (!strcmp(direct->d_name, ".") || !strcmp(direct->d_name, ".."))
			continue;
		/*
		 * MUST IGNORE NON-CPR-LIKE FILES TO PRESERVE SECURITY
		 */
		if (strncmp(direct->d_name, STATEF_SHARE,
				strlen(STATEF_SHARE)) &&
		    strncmp(direct->d_name, STATEF_PIPE,
				strlen(STATEF_PIPE)) &&
		    strncmp(direct->d_name, STATEF_SHM,
				strlen(STATEF_SHM)) &&
		    strncmp(direct->d_name, STATEF_FILE,
				strlen(STATEF_FILE))) {
			int i = 1;
			while (direct->d_name[i-1] != '.'&&direct->d_name[i] != NULL)
				i++;
		    	if (strncmp(&direct->d_name[i], STATEF_INDEX, 
				strlen(STATEF_INDEX)) &&
		    	    strncmp(&direct->d_name[i], STATEF_STATE, 
				strlen(STATEF_STATE)) &&
		    	    strncmp(&direct->d_name[i], STATEF_IMAGE, 
				strlen(STATEF_IMAGE))) {
				cerror("%s is not CPR related files: Abort!\n",
					direct->d_name); 
				closedir(name);
				return (EACCES);
			}
		}
		if ((newpath = (char *)malloc(PATH_MAX)) == NULL) {
			cerror("Failed to alloc mem (%s)\n", STRERR);
			closedir(name);
			return (-1);
		}
		sprintf(newpath, "%s/%s", path, direct->d_name);
		if (stat(newpath, &buf) == -1) {
			cerror("Read statefile %s failed (%s)\n", newpath, STRERR);
			return (-1);
		}
		/*
		 * Allow the real owner to clean up. We have been careful here
		 * since ckpt_remove can be very dangerous. If a statefile is
		 * copied and corrupted, ckpt_remove won't be allowed to remove it.
		 */
		if (force == 0 &&
		    getuid() != 0 &&
		    (buf.st_uid != getuid() || buf.st_gid != getgid())) {
			cerror("Non-owner removing statefile %s: permission denied\n", 
				cr.cr_path);
			return (EACCES);
		}
		if (unlink(newpath)) {
			cerror("failed to remove file %s (%s)\n", newpath, STRERR);
			closedir(name);
			free(newpath);
			return (-1);
		}
		free(newpath);
	}
	closedir(name);
	return (0);
}

static int
ckpt_remove_proc_index(const char *dname)
{
	char file[PATH_MAX];
	int rc, i;

	if (rc = ckpt_load_shareindex(dname))
		goto cleanup;
	/*
	 * Read share statefile for the children picture
	 */
	if (rc = ckpt_read_share_property(&cr, NULL, CKPT_MAGIC_CHILDINFO, NULL))
		goto cleanup;
	/*
	 * Remove each of the tree roots
	 */
	for (i = 0; i < cr.cr_pci.pci_ntrees; i++) {

		IFDEB1(cdebug("removing statefile pid: %d (ntree=%d)\n",
			*(cr.cr_roots+i), cr.cr_pci.pci_ntrees));
		if (rc = ckpt_remove_one_statefile(*(cr.cr_roots+i)))
			goto cleanup;
	}
	IFDEB1(cdebug("removing shared files from %s\n", dname));
	sprintf(file, "%s/%s.%s", dname, STATEF_SHARE, STATEF_INDEX);
	if (unlink(file) == -1) {
		cerror("file %s not removed (%s)\n", file, STRERR);
	}
	sprintf(file, "%s/%s.%s", dname, STATEF_SHARE, STATEF_STATE);
	if (unlink(file) == -1) {
		cerror("file %s not removed (%s)\n", file, STRERR);
	}
cleanup:
	free(cr.cr_roots);
	close(cr.cr_ifd);
	close(cr.cr_sfd);
	close(cr.cr_rsfd);
	close(cr.cr_ssfd);
	return (rc);
}

static int
ckpt_remove_one_statefile(pid_t pid)
{
	char file[CPATHLEN];
	ckpt_ta_t ta;
	int i, rc;

	if (rc = ckpt_load_target_info(&ta, pid))
		goto cleanup;

	if (rc = ckpt_restart_perm_check2(&(ta.pi), ta.sfd)) {
		cerror("cannot remove %s: permission denied\n", cr.cr_path);
		goto cleanup;
	}
	/*
	 * Remove various openfiles
	 */
	if ((rc = ckpt_remove_proc_property(&ta, CKPT_MAGIC_OPENSPECS)) < 0)
		goto cleanup;

	if ((rc = ckpt_remove_proc_property(&ta, CKPT_MAGIC_OPENFILES)) < 0)
		goto cleanup;

	if ((rc = ckpt_remove_proc_property(&ta, CKPT_MAGIC_MAPFILE)) < 0)
		goto cleanup;

	/*
	 * remove any children we have
	 */
	for (i = 0; i < ta.nchild; i++) {
		IFDEB1(cdebug("removing statefile for pid: %d\n", ta.cpid[i]));
		if (rc = ckpt_remove_one_statefile(ta.cpid[i]))
			goto cleanup;
	}
	sprintf(file, "%s/%d.%s", cr.cr_path, pid, STATEF_INDEX);
	if (unlink(file) == -1) {
		cerror("file %s not removed (%s)\n", file, STRERR);
	}
	sprintf(file, "%s/%d.%s", cr.cr_path, pid, STATEF_STATE);
	if (unlink(file) == -1) {
		cerror("file %s not removed (%s)\n", file, STRERR);
	}
	sprintf(file, "%s/%d.%s", cr.cr_path, pid, STATEF_IMAGE);
	if (unlink(file) == -1) {
		cerror("file %s not removed (%s)\n", file, STRERR);
	}

cleanup:
	ckpt_close_ta_fds(&ta);
	return (rc);
}

int
ckpt_remove_files(ckpt_ta_t *tp, ckpt_magic_t magic)
{
	ckpt_f_t file;

	if (read(tp->sfd, &file, sizeof (ckpt_f_t)) <= 0) {
		cerror("failed to read the per-file header (%s)\n", STRERR);
		return (-1);
	}
	IFDEB1(cdebug("magic=%x\n", file.cf_magic));

	if (file.cf_magic != magic) {
		cerror("mismatched magic %s vs. %s\n", &(file.cf_magic), &magic);
		return (-1);
	}
	/*
	 * Seek past any aux data
	 */
	if (file.cf_auxsize)
		lseek(tp->sfd, file.cf_auxsize, SEEK_CUR);

	/*
	 * Only remove files that we've copied
	 */
	switch (file.cf_mode) {
	case CKPT_MODE_REPLACE:
	case CKPT_MODE_SUBSTITUTE:
	case CKPT_MODE_RELOCATE:
		/*
		 * remove these
		 */
		break;
	case CKPT_MODE_MERGE:
	case CKPT_MODE_IGNORE:
	case CKPT_MODE_APPEND:
		/*
		 * don't remove these
		 */
		return (0);
	default:
		/*
		 * Huh?
		 */
		cerror("unrecognized file save mode %d (%s)\n", file.cf_mode,
			file.cf_newpath);
		return (-1);
	}

	IFDEB1(cdebug("removing openfile=%s\n", file.cf_newpath));
	if (file.cf_newpath[0] != 0 && (unlink(file.cf_newpath) == -1)) {
		cerror("file %s not removed (%s)\n", file.cf_newpath, STRERR);
	}
	return (0);
}
