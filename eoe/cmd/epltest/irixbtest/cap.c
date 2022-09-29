/*
 *  "capability.c" - routines for doing things that need capabilities
 */

/*
 * Copyright 1996, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.5 $"

#include "cap.h"

#define AR_SIZE(array)	(sizeof (array) / sizeof(array[0]))


static const cap_value_t _do_caps[] = {CAP_FOWNER, CAP_DAC_READ_SEARCH,
				       CAP_DAC_WRITE, CAP_DAC_EXECUTE,
				       CAP_MAC_READ, CAP_MAC_WRITE,
				       CAP_MAC_MLD, CAP_FSETID	};

static int
_do(const char *file, mode_t mode, int (*f)(const char *, mode_t),
    const char *fname)
{
	int	rval;
	cap_t   ocap;

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_do_caps), _do_caps);
	    if (ocap == (cap_t)0)
		Dbug(("_do_create: cap_acquire() failed; %s\n",
		      strerror(errno)));
	}

	rval = (*f)(file, mode);
	if (rval < 0 && errno != ENOENT)
	    Dbug(("_do_create: %s(\"%s\") failed; %s\n", fname, file,
		  strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}


/* unlink args used and system call */
static int
wunlink(const char *file, mode_t f_mode)
{
	return(unlink(file));
}
int
cap_unlink(const char *file)
{
	return(_do(file, 0, wunlink, "unlink"));
}


/* chmod args used and system call */
static int
wchmod(const char *file, mode_t f_mode)
{
	return(chmod(file, f_mode));
}
int
cap_chmod(const char *file, mode_t f_mode)
{
	return(_do(file, f_mode, wchmod, "chmod"));
}


/* fchmod args used and system call */
static int
wfchmod(const char *file, mode_t f_mode)
{
	return(fchmod(file, f_mode));
}
int
cap_fchmod(const char *file, mode_t f_mode)
{
	return(_do(file, f_mode, wfchmod, "fchmod"));
}


/* chdir args used and system call */
static int
wchdir(const char *dir, mode_t d_mode)
{
	return(chdir(dir));
}
int
cap_chdir(const char *dir)
{
	return(_do(dir, 0, wchdir, "chdir"));
}


/* rmdir args used and system call */
static int
wrmdir(const char *dir, mode_t d_mode)
{
	return(rmdir(dir));
}
int
cap_rmdir(const char *dir)
{
	return(_do(dir, 0, wrmdir, "rmdir"));
}


/* mkdir args used and system call */
static int
wmkdir(const char *dir, mode_t d_mode)
{
	return(mkdir(dir, d_mode));
}
int
cap_mkdir(const char *dir, mode_t d_mode)
{
	return(_do(dir, d_mode, wmkdir, "mkdir"));
}

static int
_blockproc(pid_t pid, int (*func)(pid_t), const char *funcname)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t _cap_blockproc[] = {
					CAP_DAC_READ_SEARCH, CAP_DAC_WRITE,
					CAP_DAC_EXECUTE, CAP_PROC_MGT,
					CAP_MAC_READ, CAP_MAC_WRITE, };

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_blockproc), _cap_blockproc);
	    if (ocap == (cap_t)0)
		Dbug(("_cap_blockproc: cap_acquire() failed; %s\n",strerror(errno)));
	}

	rval = (*func)(pid);

	if (rval < 0)
	    Dbug(("_blockproc: %s() failed; %s\n", funcname, strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

int
cap_blockproc(pid_t block_pid)
{
	int	rval;
	cap_t   ocap;
	return(_blockproc(block_pid, blockproc, "blockproc"));
}

int
cap_unblockproc(pid_t block_pid)
{
	int	rval;
	cap_t	ocap;
	return(_blockproc(block_pid, unblockproc, "unblockproc"));
}

ptrdiff_t
cap_prctl(unsigned option, pid_t pid)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t _cap_prctl[] = {
					CAP_DAC_READ_SEARCH, CAP_DAC_WRITE,
					CAP_MAC_READ, CAP_MAC_WRITE,
					CAP_PROC_MGT, };

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_prctl), _cap_prctl);
	    if (ocap == (cap_t)0)
		Dbug(("_cap_prctl: cap_acquire() failed; %s\n",strerror(errno)));
	}

	rval = prctl(option, pid);
	if (rval < 0)
	    Dbug(("cap_prctl: prctl(%d, %d) failed; %s\n", 
	    	option, pid, strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

int
cap_BSDsetpgrp(pid_t pid1, pid_t pid2)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t _cap_BSDsetpgrp[] = {
					CAP_PROC_MGT, CAP_SETGID,
					CAP_SETUID, CAP_DAC_READ_SEARCH,
					CAP_DAC_WRITE, CAP_DAC_EXECUTE,
					CAP_MAC_DOWNGRADE, CAP_MAC_RELABEL_SUBJ,
					CAP_MAC_UPGRADE, CAP_MAC_MLD,
					CAP_MAC_READ, CAP_MAC_WRITE, };

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_BSDsetpgrp), _cap_BSDsetpgrp);
	    if (ocap == (cap_t)0)
		Dbug(("_cap_BSDsetpgrp: cap_acquire() failed; %s\n",strerror(errno)));
	}

	rval = BSDsetpgrp(pid1, pid2);
	if (rval < 0)
	    Dbug(("cap_BSDsetpgrp: BSDsetpgrp( %d, %d) failed; %s\n", 
	    	pid1, pid2, strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

mac_t
cap_mac_get_file(const char *path)
{
	mac_t	rval;
	cap_t   ocap;
	static const cap_value_t _cap_mac_get_file[] = {
					CAP_DAC_READ_SEARCH, CAP_DAC_WRITE,
					CAP_DAC_EXECUTE, CAP_FOWNER,
					CAP_MAC_READ, CAP_MAC_WRITE, };

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_mac_get_file), _cap_mac_get_file);
	    if (ocap == (cap_t)0)
		Dbug(("_cap_mac_get_file: cap_acquire() failed; %s\n",
			strerror(errno)));
	}

	rval = mac_get_file(path);
	if (rval < 0)
	    Dbug(("cap_mac_get_file: mac_get_file(%s) failed; %s\n", 
	    	path, strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

int
cap_msgctl(int id, int cmd, struct msqid_ds *buf)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t _cap_mac_msgctl[] = {
					CAP_FOWNER, CAP_MAC_READ,
					CAP_MAC_WRITE, };

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_mac_msgctl), _cap_mac_msgctl);
	    if (ocap == (cap_t)0)
		Dbug(("_cap_mac_msgctl: cap_acquire() failed; %s\n",
			strerror(errno)));
	}

	rval = msgctl(id, cmd, buf);
	if (rval < 0)
	    Dbug(("cap_mac_msgctl: msgctl(%d, %d, %o) failed; %s\n", 
	    	id, cmd, buf, strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

int
cap_open(const char *file, int oflags, mode_t mode)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t _cap_open[] = {
					CAP_DAC_READ_SEARCH, CAP_DAC_WRITE,
					CAP_DAC_EXECUTE, CAP_FOWNER,
					CAP_MAC_READ, CAP_MAC_WRITE, };

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_open), _cap_open);
	    if (ocap == (cap_t)0)
		Dbug(("_cap_open: cap_acquire() failed; %s\n", strerror(errno)));
	}

	rval = open(file, oflags, mode);
	if (rval < 0)
	    Dbug(("cap_open: open(%s, %.4o, %.4o) failed; %s\n", 
	    	file, oflags, mode, strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

static int
_setgroups(int ngroups, void *gids, int (*func)(int, void *),
	   const char *funcname)
{
	int	rval;
	cap_t   ocap;

	if (cap_enabled) {
	    const cap_value_t capv = CAP_SETGID;
	    ocap = cap_acquire(1, &capv);
	    if (ocap == (cap_t)0)
		Dbug(("_setgroups: cap_acquire() failed; %s\n",
		strerror(errno)));
	}

	rval = (*func)(ngroups, gids);
	if (rval < 0)
	    Dbug(("_setgroups: %s() failed; %s\n", funcname, strerror(errno)));

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

static int
wsetgroups(int ngroups, void *gidset)
{
	return(setgroups(ngroups, (gid_t *) gidset));
}

int
cap_setgroups(int ngroups, gid_t *gidset)
{
	return(_setgroups(ngroups, gidset, wsetgroups, "setgroups"));
}

static int
wBSDsetgroups(int ngroups, void *gidset)
{
	return(BSDsetgroups(ngroups, (int *) gidset));
}

int
cap_BSDsetgroups(int ngroups, int *gidset)
{
	return(_setgroups(ngroups, gidset, wBSDsetgroups, "BSDsetgroups"));
}

static const cap_value_t id_caps[] = {CAP_SETUID, CAP_SETGID};

static int
_setid(uid_t id, int (*f)(uid_t), const char *fname)
{
    int rval;
    cap_t ocap;

    if (cap_enabled) {
	ocap = cap_acquire(AR_SIZE(id_caps), id_caps);
	if (ocap == (cap_t)0)
	    Dbug(("_setid: cap_acquire() failed; %s\n", strerror(errno)));
    }

    rval = (*f)(id);
    if (rval < 0)
	Dbug(("_setid: %s failed; %s\n", fname, strerror(errno)));

    if (cap_enabled)
	cap_surrender(ocap);

    return(rval);
}

static int
_setreid(uid_t rid, uid_t eid, int (*f)(uid_t, uid_t), const char *fname)
{
    int rval;
    cap_t ocap;

    if (cap_enabled) {
	ocap = cap_acquire(AR_SIZE(id_caps), id_caps);
	if (ocap == (cap_t)0)
	    Dbug(("_setreid: cap_acquire() failed; %s\n", strerror(errno)));
    }

    rval = (*f)(rid, eid);
    if (rval < 0)
	Dbug(("_setreid: %s failed; %s\n", fname, strerror(errno)));

    if (cap_enabled)
	cap_surrender(ocap);

    return(rval);
}

int
cap_setuid(uid_t uid)
{
	return(_setid(uid, setuid, "setuid"));
}

int
cap_setgid(gid_t gid)
{
	return(_setid(gid, setgid, "setgid"));
}

int
cap_setreuid(uid_t ruid, uid_t euid)
{
	return(_setreid(ruid, euid, setreuid, "setreuid"));
}

int
cap_setregid(gid_t rgid, gid_t egid)
{
	return(_setreid(rgid, egid, setregid, "setregid"));
}

int
cap_setlabel(const char *path, mac_t maclp)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t lbl_caps[] = {CAP_MAC_RELABEL_OPEN,
					       CAP_MAC_UPGRADE,
					       CAP_MAC_DOWNGRADE, CAP_MAC_MLD,
					       CAP_FOWNER, CAP_MAC_READ,
					       CAP_MAC_WRITE};

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(lbl_caps), lbl_caps);
	    if (ocap == (cap_t)0)
		Dbug(("cap_setlabel: cap_acquire() failed; %s\n",
		strerror(errno)));
	}
	
	rval = mac_set_file(path, maclp);

	if (cap_enabled)
	    cap_surrender(ocap);
	
	return(rval);
}

int
cap_setplabel(mac_t maclp)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t plbl_caps[] = {CAP_MAC_RELABEL_SUBJ,
						CAP_MAC_UPGRADE,
						CAP_MAC_DOWNGRADE,
						CAP_MAC_MLD};

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(plbl_caps), plbl_caps);
	    if (ocap == (cap_t)0)
		Dbug(("cap_setplabel: cap_acquire() failed; %s\n",
		strerror(errno)));
	}
	
	rval = mac_set_proc(maclp);

	if (cap_enabled)
	    cap_surrender(ocap);
	
	return(rval);
}

int
cap_kill(pid_t pid, int sig_no)
{
	int	rval;
	cap_t   ocap;
	static const cap_value_t kill_caps[] = {CAP_KILL, CAP_MAC_WRITE};

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(kill_caps), kill_caps);
	    if (ocap == (cap_t)0)
		Dbug(("cap_kill: cap_acquire() failed; %s\n", strerror(errno)));
	}
	
	rval = kill(pid, sig_no);

	if (cap_enabled)
	    cap_surrender(ocap);
	
	return(rval);
}

static const cap_value_t own_caps[] = { CAP_DAC_READ_SEARCH, CAP_DAC_WRITE,
					CAP_DAC_EXECUTE, CAP_FOWNER,
					CAP_MAC_READ, CAP_MAC_WRITE, };

int
cap_chown(const char *path, uid_t owner, gid_t group)
{
	int	rval;
	cap_t   ocap;

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(own_caps), own_caps);
	    if (ocap == (cap_t)0)
		Dbug(("cap_chown: cap_acquire() failed; %s\n",
		strerror(errno)));
	}
	
	rval = chown(path, owner, group);

	if (cap_enabled)
	    cap_surrender(ocap);
	
	return(rval);
}

int
cap_fchown(int fd, uid_t owner, gid_t group)
{
	int	rval;
	cap_t   ocap;

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(own_caps), own_caps);
	    if (ocap == (cap_t)0)
		Dbug(("cap_fchown: cap_acquire() failed; %s\n",
		strerror(errno)));
	}
	
	rval = fchown(fd, owner, group);

	if (cap_enabled)
	    cap_surrender(ocap);
	
	return(rval);
}

static void *
_acl_get(const char *file, int fd, acl_type_t type)
{
	void	*rval;
	cap_t   ocap;

	if (file == NULL && fd < 0)
		return(PASS);

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_do_caps), _do_caps);
	    if (ocap == (cap_t)0)
		Dbug(("_acl_get: cap_acquire() failed; %s\n", strerror(errno)));
	}

	if (fd < 0)
	    rval = (void *) acl_get_file(file, type);
	else
	    rval = (void *) acl_get_fd(fd);

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

void *
cap_acl_get_file(const char *arg, acl_type_t type)
{
	if (!acl_enabled)
		return(PASS);

	return(_acl_get(arg, -1, type));
}

void *
cap_acl_get_fd(int fd)
{
	if (!acl_enabled)
		return(PASS);

	return(_acl_get(NULL, fd, 0));
}

int
cap_acl_set_file(const char *name, int type, acl_t acl)
{
	int	rval;
	cap_t   ocap;

	if (!acl_enabled)
		return(PASS);

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_do_caps), _do_caps);
	    if (ocap == (cap_t)0)
		Dbug(("cap_acl_set_file: cap_acquire() failed; %s\n",
		strerror(errno)));
	}

	rval = acl_set_file(name, type, acl);

	if (cap_enabled)
	    cap_surrender(ocap);

	return(rval);
}

static const cap_value_t _cap_put_file[] = {CAP_SETFCAP};

int
cap_put_file(const char *path, cap_t ncap)
{
	int	rval;
	cap_t   ocap;
        cap_value_t capv;

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_put_file), _cap_put_file);
	    if (ocap == (cap_t)0)
		Dbug(("cap_put_file: cap_acquire() failed; %s\n",
		strerror(errno)));

	    ncap->cap_effective = ncap->cap_effective + CAP_SETFCAP;
	    rval = cap_set_file(path, ncap);

	    cap_free(ocap);

	    return(rval);
	}
	return(PASS);
}

int
cap_perm_all_file(const char *path)
{
	int	rval;
	cap_t   ocap;
        cap_value_t capv;

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_put_file), _cap_put_file);
	    if (ocap == (cap_t)0)
		Dbug(("cap_perm_all_file: cap_acquire() failed; %s\n",
		strerror(errno)));

	    ocap->cap_effective = (CAP_ALL_ON ^ CAP_INVALID);
	    ocap->cap_permitted = CAP_ALL_OFF;
	    ocap->cap_inheritable = CAP_ALL_OFF;
	    rval = cap_set_file(path, ocap);

	    cap_free(ocap);
	    return(rval);
	}

	return(PASS);
}

static const cap_value_t _cap_put_proc[] = {CAP_SETPCAP};

int
cap_put_proc(cap_t ncap)
{
	int	rval;
	cap_t   ocap, ccap;
        cap_value_t capv;

	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_put_proc), _cap_put_proc);
	    if (ocap == (cap_t)0)
		Dbug(("cap_put_proc: cap_acquire() failed; %s\n",
		strerror(errno)));

	    ncap->cap_effective = ncap->cap_effective + CAP_SETPCAP;
	    rval = cap_set_proc(ncap);

	    cap_free(ocap);
	    return(rval);
	}

	return(PASS);
}

int
cap_perm_all_proc(void)
{
	int	rval;
	cap_t   ocap, ccap;
        cap_value_t capv;
	
	if (cap_enabled) {
	    ocap = cap_acquire(AR_SIZE(_cap_put_proc), _cap_put_proc);
	    if (ocap == (cap_t)0)
		Dbug(("cap_perm_all_proc: cap_acquire() failed; %s\n",
		strerror(errno)));

	    ocap->cap_effective = (CAP_ALL_ON ^ CAP_INVALID);
	    ocap->cap_permitted = CAP_ALL_OFF;
	    ocap->cap_inheritable =  CAP_ALL_OFF;
	    rval = cap_set_proc(ocap);

	    cap_free(ocap);
	    return(rval);
	}

	return(PASS);
}

void
case_name(int casenum, int success, char *casename_p)
{
	int	poscase;
	int	negcase;


	if ( !casenum ) {
		poscase = 0;
		negcase = 0;
	}
	
	sprintf(casename_p, "%s%.2d", 
		(success == PASS) ? "pos" : "neg",
		(success == PASS) ? poscase++ : negcase++);

	return;
}
