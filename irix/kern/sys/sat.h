/*
 *
 * Copyright 1988,1992, Silicon Graphics, Inc.
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


#ifndef	__SYS_SAT_H
#define	__SYS_SAT_H
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.72 $"

#include <sys/types.h>
#ifdef _KERNEL
#include <sys/systm.h>	/* for rval_t */
#include <sys/vnode.h>	/* for enum symfollow */
#endif /* _KERNEL */
#include <sys/acl.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/select.h>
struct socket;
struct soacl;

/* The following define must match user.h */
#ifndef PSCOMSIZ
#define PSCOMSIZ        32
#endif


	/*********************/
	/*	Typdefs      */
	/*********************/

typedef struct mbuf sat_rec_t;		/* for audit records */
typedef unsigned int sat_host_t;	/* host identifier */
typedef uint16_t	sat_token_id_t;
typedef uint16_t	sat_token_size_t;

struct sat_token_header {
	sat_token_id_t		sat_token_id;
	sat_token_size_t	sat_token_size;
};
typedef struct sat_token_header sat_token_header_t;

#ifdef	_KERNEL

struct sat_token;

#else	/* _KERNEL */
#define PSEUDO_SIZE 4
struct sat_token {
	sat_token_header_t	token_header;
	char			token_data[PSEUDO_SIZE];
};

#define SAT_TOKEN_DATA_SIZE(x) \
	((x)->token_header.sat_token_size - sizeof(sat_token_header_t))

#endif	/* _KERNEL */

typedef struct sat_token *sat_token_t;

	/*********************/
	/*	Defines      */
	/*********************/

/*
 * Major and minor version numbers.  These identify the
 * "version" of the records in a file.
 */
#define	SAT_VERSION_MAJOR	4
#define	SAT_VERSION_MINOR	0
#define	SAT_FILE_MAGIC		"SGIAUDIT"
#define	SAT_RECORD_MAGIC	0xee6540ee

/*
 * Resist the temptation to assume that everyone thinks these
 * are th ecorrect values for true and false.
 */
#define SAT_TRUE 1
#define SAT_FALSE 0

/* handy macro */

#define	sat_skip_hdr(m,t)	mtod((m)->m_act,t)

/* maximum values */

#define SAT_MAX_RECORD		65535	/* maximum kernel record size */
#define SAT_MAX_USER_REC	 4000	/* maximum buffer to sat_write */
					/* (user-level auditing) */
/* SAT outcome bits */

#define SAT_UNDEFINED		0xff	/* should never show up in audit file*/
#define SAT_FAILURE		0x00	/* handy, redundant, says '~SUCCESS' */
#define SAT_SUCCESS		0x01	/* 1 = success, 0 = failure          */
#define SAT_DAC			0x02	/* 1 = dac affected outcome, 0 = not */
#define SAT_MAC			0x04	/* 1 = mac affected outcome, 0 = not */
#define SAT_PRIVILEGE		0x08	/* 1 = failed/succeded due to priv.  */
#define SAT_SUSER		0x08	/* 1 = failed/succeeded due to suser */
#define SAT_CAPABILITY		0x10	/* 1 = failed/succeeded due to cap */
#define SAT_CHK			0x20	/* 1 = was suser/priv checked? */

/* sat_ctl() commands */

#define SATCTL_AUDIT_ON		1
#define SATCTL_AUDIT_OFF	2
#define SATCTL_AUDIT_QUERY	3
#define SATCTL_SET_SAT_ID	4
#define SATCTL_GET_SAT_ID	5
#define SATCTL_LOCALAUDIT_ON	6
#define SATCTL_LOCALAUDIT_OFF	7
#define SATCTL_LOCALAUDIT_QUERY	8
#define SATCTL_REGISTER_SATD	9

/* bitmask for audit events */

#define NSATBITS	(sizeof(unsigned int) * __NBBY)	/* bits per mask */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

#define	SAT_SET(n, p)	((p)->ev_bits[(n)/NSATBITS] |= (1 << ((n) % NSATBITS)))
#define	SAT_CLR(n, p)	((p)->ev_bits[(n)/NSATBITS] &= ~(1 << ((n) % NSATBITS)))
#define	SAT_ISSET(n, p) \
	(((p)->ev_bits[(n)/NSATBITS] & (1 << ((n) % NSATBITS))) != 0)
#define SAT_ZERO(p)	bzero((char *)(p), sizeof(*(p)))

#ifdef _KERNEL
	/************************************************/
	/*	Prototypes and function macros		*/
	/************************************************/

extern	int	sat_enabled;
extern void	sat_init( void );

/* opaque type definitions */
struct pathname;
struct acct_counts;
struct acct_timers;
struct ifnet;
struct ifreq;
struct ipc_perm;
struct pollfd;
struct ip;
struct uthread_s;
struct proc;
struct arsess;

/*
 * We mark all of the audit routines as infrequent in order to cause any
 * code necessary to call them to be compiled out of line.  This penalizes
 * sites that use SAT slightly and rewards those that don't.  Since the SAT
 * calls already involve an out-of-line function call this seems like a good
 * tradeoff.
 */

static __inline void sat_never(void) {}
#pragma mips_frequency_hint NEVER sat_never

/* syscalls */

extern int sat_read(char *, unsigned, rval_t *);
#define _SAT_READ(a,b,c) \
	(sat_enabled? (sat_never(), sat_read(a,b,c)): ENOSYS)

extern int sat_write(int, int, char *, unsigned);
#define _SAT_WRITE(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_write(a,b,c,d)): ENOSYS)

extern int sat_ctl(int, int, pid_t, rval_t *);
#define _SAT_CTL(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_ctl(a,b,c,d)): ENOSYS)

/* misc kernel routines */
extern void sat_pn_save (struct pathname *, struct uthread_s *);
#define _SAT_PN_SAVE(a,b) \
	(sat_enabled? (sat_never(), sat_pn_save(a,b)): (void)0)

extern void sat_pn_book(int, struct uthread_s *);
#define _SAT_PN_BOOK(a,b) \
	(sat_enabled? (sat_never(), sat_pn_book(a,b)): (void)0)

extern void sat_pn_start(struct uthread_s *);
#define _SAT_PN_START(a) \
	(sat_enabled? (sat_never(), sat_pn_start(a)): (void)0)

extern void sat_pn_finalize(struct vnode *, struct uthread_s *);
#define _SAT_PN_FINALIZE(a,b) \
	(sat_enabled? (sat_never(), sat_pn_finalize(a,b)): (void)0)

extern void sat_pn_append(char *, struct uthread_s *);
#define _SAT_PN_APPEND(a,b) \
	(sat_enabled? (sat_never(), sat_pn_append(a,b)): (void)0)

extern void sat_save_attr (sat_token_id_t, struct uthread_s *);
#define _SAT_SAVE_ATTR(a,b) \
	(sat_enabled? (sat_never(), sat_save_attr(a,b)): (void)0)

extern void sat_update_rwdir (int, struct uthread_s *);
#define _SAT_UPDATE_RWDIR(a,b) \
	(sat_enabled? (sat_never(), sat_update_rwdir(a,b)): (void)0)

extern int sat_lookup(char *, enum symfollow, struct uthread_s *);
#define _SAT_LOOKUP(a,b,c) \
	(sat_enabled? (sat_never(), sat_lookup(a,b,c)): 0)

extern void sat_confignote(void);
#define _SAT_CONFIGNOTE() \
	(sat_enabled? (sat_never(), sat_confignote()): (void)0)

/* "audit points" */

extern void sat_access(int, int);
#define	_SAT_ACCESS(a,b) \
	(sat_enabled? (sat_never(), sat_access(a,b)): (void)0)

extern void sat_access_pn(int, int);
#define	_SAT_ACCESS_PN(a,b) \
	(sat_enabled? (sat_never(), sat_access_pn(a,b)): (void)0)

extern void sat_access2(int, int);
#define	_SAT_ACCESS2(a,b) \
	(sat_enabled? (sat_never(), sat_access2(a,b)): (void)0)

extern void sat_acct(char *,  int);
#define	_SAT_ACCT(a,b) \
	(sat_enabled? (sat_never(), sat_acct(a,b)): (void)0)

extern void sat_bsdipc_addr(int, struct socket *, struct mbuf *, int);
#define	_SAT_BSDIPC_ADDR(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_bsdipc_addr(a,b,c,d)): (void)0)

extern void sat_bsdipc_create(short, struct socket *, short, short, int);
#define	_SAT_BSDIPC_CREATE(a,b,c,d,e) \
	(sat_enabled? (sat_never(), sat_bsdipc_create(a,b,c,d,e)): (void)0)

extern void sat_bsdipc_create_pair(short, struct socket *, short, short,
				   short, struct socket *, int);
#define	_SAT_BSDIPC_CREATE_PAIR(a,b,c,d,e,f,g) \
	(sat_enabled? (sat_never(), sat_bsdipc_create_pair(a,b,c,d,e,f,g)): (void)0)

extern void sat_bsdipc_if_config(int, struct socket *, int, struct ifreq *, int);
#define	_SAT_BSDIPC_IF_CONFIG(a,b,c,d,e) \
	(sat_enabled? (sat_never(), sat_bsdipc_if_config(a,b,c,d,e)): (void)0)

extern void sat_bsdipc_missing(struct ifnet *, struct ip *, int);
#define	_SAT_BSDIPC_MISSING(a,b,c) \
	(sat_enabled? (sat_never(), sat_bsdipc_missing(a,b,c)): (void)0)

extern void sat_bsdipc_range(struct ifnet *, struct ip *, uid_t,
			     mac_label *, int, int);
#define	_SAT_BSDIPC_RANGE(a,b,c,d,e,f) \
	(sat_enabled? (sat_never(), sat_bsdipc_range(a,b,c,d,e,f)): (void)0)

extern void sat_bsdipc_resvport(int, struct socket *, int, int);
#define	_SAT_BSDIPC_RESVPORT(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_bsdipc_resvport(a,b,c,d)): (void)0)

extern void sat_bsdipc_shutdown(short, struct socket *, short, int);
#define	_SAT_BSDIPC_SHUTDOWN(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_bsdipc_shutdown(a,b,c,d)): (void)0)

extern void sat_bsdipc_snoop(struct socket *, mac_label *, int, int);
#define	_SAT_BSDIPC_SNOOP(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_bsdipc_snoop(a,b,c,d)): (void)0)

extern void sat_check_priv(int, int);
#define	_SAT_CHECK_PRIV(a,b) \
	(sat_enabled? (sat_never(), sat_check_priv(a,b)): (void)0)

extern void sat_chmod(int, int);
#define	_SAT_CHMOD(a,b) \
	(sat_enabled? (sat_never(), sat_chmod(a,b)): (void)0)

extern void sat_chown(int, int, int);
#define	_SAT_CHOWN(a,b,c) \
	(sat_enabled? (sat_never(), sat_chown(a,b,c)): (void)0)

extern void sat_chrwdir( int);
#define	_SAT_CHRWDIR(a) \
	(sat_enabled? (sat_never(), sat_chrwdir(a)): (void)0)

extern void sat_clock(time_t, int);
#define	_SAT_CLOCK(a,b) \
	(sat_enabled? (sat_never(), sat_clock(a,b)): (void)0)

extern void sat_close(int, int);
#define	_SAT_CLOSE(a,b) \
	(sat_enabled? (sat_never(), sat_close(a,b)): (void)0)

extern void sat_domainname_set(char *, int);
#define	_SAT_DOMAINNAME_SET(a,b) \
	(sat_enabled? (sat_never(), sat_domainname_set(a,b)): (void)0)

extern void sat_dup(int, int, int);
#define	_SAT_DUP(a,b,c) \
	(sat_enabled? (sat_never(), sat_dup(a,b,c)): (void)0)

extern void sat_exec(int);
#define	_SAT_EXEC(a) \
	(sat_enabled? (sat_never(), sat_exec(a)): (void)0)

extern void sat_exit(int, int);
#define	_SAT_EXIT(a,b) \
	(sat_enabled? (sat_never(), sat_exit(a,b)): (void)0)

extern void sat_fchdir(int, int);
#define	_SAT_FCHDIR(a,b) \
	(sat_enabled? (sat_never(), sat_fchdir(a,b)): (void)0)

extern void sat_fchmod(int, mode_t, int);
#define	_SAT_FCHMOD(a,b,c) \
	(sat_enabled? (sat_never(), sat_fchmod(a,b,c)): (void)0)

extern void sat_fchown(int, uid_t, gid_t, int);
#define	_SAT_FCHOWN(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_fchown(a,b,c,d)): (void)0)

extern void sat_fd_read(int, int);
#define	_SAT_FD_READ(a,b) \
	(sat_enabled? (sat_never(), sat_fd_read(a,b)): (void)0)

extern void sat_fd_read2(fd_set *, int);
#define	_SAT_FD_READ2(a,b) \
	(sat_enabled? (sat_never(), sat_fd_read2(a,b)): (void)0)

extern void sat_pfd_read2(struct pollfd *, int, int);
#define	_SAT_PFD_READ2(a,b,c) \
	(sat_enabled? (sat_never(), sat_pfd_read2(a,b,c)): (void)0)

extern void sat_tty_setlabel( mac_label *, int);
#define	_SAT_TTY_SETLABEL(a,b) \
	(sat_enabled? (sat_never(), sat_tty_setlabel(a,b)): (void)0)

extern void sat_fd_rdwr(int, int, int);
#define	_SAT_FD_RDWR(a,b,c) \
	(sat_enabled? (sat_never(), sat_fd_rdwr(a,b,c)): (void)0)

extern void sat_fork(pid_t, int);
#define	_SAT_FORK(a,b) \
	(sat_enabled? (sat_never(), sat_fork(a,b)): (void)0)

extern void sat_hostid_set(long, int);
#define	_SAT_HOSTID_SET(a,b) \
	(sat_enabled? (sat_never(), sat_hostid_set(a,b)): (void)0)

extern void sat_hostname_set(char *, int);
#define	_SAT_HOSTNAME_SET(a,b) \
	(sat_enabled? (sat_never(), sat_hostname_set(a,b)): (void)0)

extern void sat_mount(dev_t, int);
#define	_SAT_MOUNT(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_mount(a,b,c,d)): (void)0)

extern void sat_open(int, int, int,  int);
#define	_SAT_OPEN(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_open(a,b,c,d)): (void)0)

extern void sat_pipe(int, int, int);
#define	_SAT_PIPE(a,b,c) \
	(sat_enabled? (sat_never(), sat_pipe(a,b,c)): (void)0)

extern void sat_kill(int, pid_t, uid_t, uid_t, mac_label *, int);
#define	_SAT_KILL(a,b,c,d,e,f) \
	(sat_enabled? (sat_never(), sat_kill(a,b,c,d,e,f)): (void)0)

extern void sat_proc_access(int, pid_t, struct cred *, int, int);
#define	_SAT_PROC_ACCESS(a,b,c,d,e) \
	(sat_enabled? (sat_never(), sat_proc_access(a,b,c,d,e)): (void)0)

extern void sat_ptrace(int, pid_t, struct cred *, int);
#define	_SAT_PTRACE(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_ptrace(a,b,c,d)): (void)0)

extern void sat_setgroups(int, gid_t *, int);
#define	_SAT_SETGROUPS(a,b,c) \
	(sat_enabled? (sat_never(), sat_setgroups(a,b,c)): (void)0)

extern void sat_setlabel(struct mac_label *, int);
#define	_SAT_SETLABEL(a,b) \
	(sat_enabled? (sat_never(), sat_setlabel(a,b)): (void)0)

extern void sat_setplabel(mac_label *, int);
#define	_SAT_SETPLABEL(a,b) \
	(sat_enabled? (sat_never(), sat_setplabel(a,b)): (void)0)

extern void sat_setacl(struct acl *, struct acl *, int);
#define _SAT_SETACL(a,b,c) \
	(sat_enabled? (sat_never(), sat_setacl(a,b,c)): (void)0)

extern void sat_setcap(cap_t, int);
#define	_SAT_SETCAP(a,b) \
	(sat_enabled? (sat_never(), sat_setcap(a,b)): (void)0)

extern void sat_setpcap(cap_t, int);
#define	_SAT_SETPCAP(a,b) \
	(sat_enabled? (sat_never(), sat_setpcap(a,b)): (void)0)

extern void sat_setregid(gid_t, gid_t, int);
#define	_SAT_SETREGID(a,b,c) \
	(sat_enabled? (sat_never(), sat_setregid(a,b,c)): (void)0)

extern void sat_setreuid(uid_t, uid_t, int);
#define	_SAT_SETREUID(a,b,c) \
	(sat_enabled? (sat_never(), sat_setreuid(a,b,c)): (void)0)

extern void sat_umask(mode_t, int);
#define _SAT_UMASK(a,b) \
	(sat_enabled? (sat_never(), sat_umask(a,b)): (void)0)

extern void sat_svipc_access(mac_label *, int, int, int);
#define	_SAT_SVIPC_ACCESS(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_svipc_access(a,b,c,d)): (void)0)

extern void sat_svipc_change(int, struct ipc_perm *, struct ipc_perm *, int);
#define	_SAT_SVIPC_CHANGE(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_svipc_change(a,b,c,d)): (void)0)

extern void sat_svipc_create(key_t, int, int, int);
#define	_SAT_SVIPC_CREATE(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_svipc_create(a,b,c,d)): (void)0)

extern void sat_svipc_ctl(int, int, struct ipc_perm *, struct ipc_perm *, int);
#define	_SAT_SVIPC_CTL(a,b,c,d,e) \
	(sat_enabled? (sat_never(), sat_svipc_ctl(a,b,c,d,e)): (void)0)

extern void sat_svipc_remove(int, int);
#define	_SAT_SVIPC_REMOVE(a,b) \
	(sat_enabled? (sat_never(), sat_svipc_remove(a,b)): (void)0)

extern struct ipc_perm *sat_svipc_save(struct ipc_perm *);
#define	_SAT_SVIPC_SAVE(a) \
	(sat_enabled? (sat_never(), sat_svipc_save(a)): NULL)

extern void sat_utime(time_t *, time_t, time_t, int);
#define	_SAT_UTIME(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_utime(a,b,c,d)): (void)0)

extern void sat_control(int, int, int, int);
#define	_SAT_CONTROL(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_control(a,b,c,d)): (void)0)

extern void sat_proc_acct(struct proc *, struct acct_timers *,
	  struct acct_counts *, int);
#define _SAT_PROC_ACCT(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_proc_acct(a,b,c,d)): (void)0)

extern void sat_session_acct(struct arsess *, struct uthread_s *, int);
#define _SAT_SESSION_ACCT(a,b,c) \
	(sat_enabled? (sat_never(), sat_session_acct(a,b,c)): (void)0)


extern void sat_svr4net_addr(int, void *, struct mbuf *, int);
#define	_SAT_SVR4NET_ADDR(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_svr4net_addr(a,b,c,d)): (void)0)

extern void sat_svr4net_create(int, void *, short, short, int);
#define	_SAT_SVR4NET_CREATE(a,b,c,d,e) \
	(sat_enabled? (sat_never(), sat_svr4net_create(a,b,c,d,e)): (void)0)

extern void sat_svr4net_shutdown(int, void *, short, int);
#define	_SAT_SVR4NET_SHUTDOWN(a,b,c,d) \
	(sat_enabled? (sat_never(), sat_svr4net_shutdown(a,b,c,d)): (void)0)

/* Functions for setting items the sat_proc area */

extern void sat_init_syscall (void);
#define _SAT_INIT_SYSCALL() \
	(sat_enabled? (sat_never(), sat_init_syscall()): (void)0)

extern void sat_set_subsysnum (u_short);
#define _SAT_SET_SUBSYSNUM(a) \
	(sat_enabled? (sat_never(), sat_set_subsysnum(a)): (void)0)

extern void sat_set_suflag (u_short);
#define _SAT_SET_SUFLAG(a) \
	(sat_enabled? (sat_never(), sat_set_suflag(a)): (void)0)

extern void sat_set_uid (uid_t);
#define _SAT_SET_UID(a) \
	(sat_enabled? (sat_never(), sat_set_uid(a)): (void)0)

extern void sat_set_soacl (struct proc * , struct soacl *);
#define _SAT_SET_SOACL(a,b) \
	(sat_enabled? (sat_never(), sat_set_soacl(a,b)): (void)0)

extern void sat_set_comm (char *);
#define _SAT_SET_COMM(a) \
	(sat_enabled? (sat_never(), sat_set_comm(a)): (void)0)

extern void sat_set_openfd (int);
#define _SAT_SET_OPENFD(a) \
	(sat_enabled? (sat_never(), sat_set_openfd(a)): (void)0)

extern void sat_set_cap (cap_value_t);
#define	_SAT_SET_CAP(a) \
	(sat_enabled? (sat_never(), sat_set_cap(a)): (void)0)

extern void sat_check_flags (int);
#define _SAT_CHECK_FLAGS(a) \
	(sat_enabled? (sat_never(), sat_check_flags(a)): (void)0)

extern void sat_proc_init (struct uthread_s * ut, struct uthread_s * parent);
#define _SAT_PROC_INIT(a,b) \
	(sat_enabled? (sat_never(), sat_proc_init(a,b)): (void)0)

extern void sat_proc_exit (struct uthread_s * ut);
#define _SAT_PROC_EXIT(a) \
	(sat_enabled? (sat_never(), sat_proc_exit(a)): (void)0)

extern void sat_sys_note(char *, int);
#define	_SAT_SYS_NOTE(a) \
	(sat_enabled? (sat_never(), sat_sys_note(a)): (void)0)

#endif /* _KERNEL */

/*
 * The audit file header
 */
struct sat_filehdr {
	char	sat_magic[8];		/* == "SGIAUDIT" */
	u_char	sat_major;		/* version of audit data */
	u_char	sat_minor;
	u_char	sat_pad1[2];		/* alignment filler */
	time_t	sat_start_time;		/* time header written */
	time_t	sat_stop_time;		/* time file closed (added later) */
	sat_host_t sat_host_id;		/* host id */
	u_int	sat_mac_enabled: 1;	/* boolean: ignore mac fields or not */
	u_int	sat_cap_enabled: 1;	/* boolean: ignore cap fields or not */
	u_int	sat_cipso_enabled: 1;	/* boolean: ignore cipso fields or
					   not */
	u_int	sat_total_bytes: 29;	/* number of bytes to skip past hdr */
	u_short	sat_user_entries;	/* number of sat_list_ent structs */
	u_short	sat_group_entries;	/*   in the user and group lists */
	u_short	sat_host_entries;	/*   and the hostid <-> name list */
	u_char	sat_timezone_len;	/* bytes of timezone string */
	u_char	sat_hostname_len;	/* bytes of hostname */
	u_char	sat_domainname_len;	/* bytes of domainname */
	u_char	sat_pad2[3];		/* alignment filler */
	/* TZ (timezone) (including trailing null) */
	/* hostname (including trailing null) */
	/* domainname (including trailing null) */
	/* user entries, each word aligned */
	/* group entries, each word aligned */
	/* hostid entries, each word aligned */
};

	/**************************************/
	/*	Audit record definitions      */
	/**************************************/

#define SATIFNAMSIZ 16

struct sat_proc_acct {
	char	sat_version;		/* Accounting data version */
	char	sat_flag;		/* Miscellaneous flags */
	char	sat_nice;		/* Nice value */
	unchar	sat_sched;		/* Scheduling discipline */
					/* (see sys/schedctl.h) */
	int	sat_spare1;		/*   reserved */
	ash_t	sat_ash;		/* Array session handle */
	prid_t	sat_prid;		/* Project ID */
	time_t	sat_btime;		/* Begin time (in secs since 1970)*/
	time_t	sat_etime;		/* Elapsed time (in HZ) */
	int	sat_spare2[2];		/*   reserved */
};
/* Generally followed by SAT_ACCT_TIMERS_TOKEN and SAT_ACCT_COUNTS_TOKEN */

/* sat_proc_acct.sat_flag */
#define	SPASF_FORK	0x80		/* has executed fork, but no exec */
#define	SPASF_SU	0x40		/* used privilege */
#define SPASF_SESSEND   0x20		/* Last process in session */
#define SPASF_CKPT	0x10		/* process has been checkpointed */
#define SPASF_SECONDARY	0x08		/* 2nd+ record for this process */


struct sat_session_acct {
	char	sat_version;		/* Accounting data version */
	char	sat_flag;		/* Miscellaneous flags */
	char	sat_nice;		/* Initial nice of session leader */
	char	sat_spare1;		/*   reserved */
	int	sat_spare2;		/*   reserved */
	ash_t	sat_ash;		/* Array session handle */
	prid_t	sat_prid;		/* Project ID */
	time_t	sat_btime;		/* Begin time (in secs since 1970) */
	time_t	sat_etime;		/* Elapsed time (in HZ) */
	short	sat_spilen;		/* Length of "sat_spi" (ver 2 only) */
	short	sat_spare3;		/*   reserved */
	int     sat_spare4;		/*   reserved */
};
/* struct sat_session_acct is valid for both version 1 and version 2 */
/* records - the only difference is that the "sat_spilen" field does */
/* not contain valid data in version 1. This appears in the audit    */
/* stream as a SAT_SESSION_ACCT_TOKEN. If this is flushed accounting */
/* data, then it will be followed by a SAT_UGID_TOKEN containing the */
/* real user and group of an arbitrary member of the array session   */
/* (since the ruid/rgid in the record header will belong to the user */
/* the caused the flush operation). Next will be one of the SPI	     */
/* tokens (SAT_ACCT_SPI_TOKEN or SAT_ACCT_SPI2_TOKEN) and then both  */
/* SAT_ACCT_TIMERS_TOKEN and SAT_ACCT_COUNTS_TOKEN.		     */

/* sat_session_acct.sat_flag */
#define SSASF_CKPT	0x80		/* process has been checkpointed */
#define SSASF_SECONDARY	0x40		/* 2nd+ record for this session */
#define SSASF_FLUSHED   0x20		/* flushed, session still active */

	/*********************************/
	/*	System call numbers      */
	/*********************************/

/*
 * Selected system call numbers.  This is not intended to be
 * a complete list.  Add new entries on an as-needed basis.
 * (these must stay up to date with uts/mips/os/sysent.c)
 *
 * SAT_SYSCALL_KERNEL is for kernel-direct messages that are not
 * the result of some system call (timetrim, for example).
 */
#define SAT_SYSCALL_KERNEL	255

#define SAT_SYSCALL_ACCESS	 33
#define SAT_SYSCALL_CHDIR	 12
#define SAT_SYSCALL_CHMOD	 15
#define SAT_SYSCALL_CHOWN	 16
#define SAT_SYSCALL_CHROOT	 61
#define SAT_SYSCALL_CLOSE	  6
#define SAT_SYSCALL_CREAT	  8
#define SAT_SYSCALL_DUP		 41
#define SAT_SYSCALL_EXEC	 11
#define SAT_SYSCALL_EXECE	 59
#define SAT_SYSCALL_EXIT	  1
#define SAT_SYSCALL_FORK	  2
#define SAT_SYSCALL_FCHDIR	147
#define SAT_SYSCALL_FCHMOD	153
#define SAT_SYSCALL_FCHOWN	152
#define SAT_SYSCALL_KILL	 37
#define SAT_SYSCALL_LINK	  9
#define SAT_SYSCALL_MKDIR	 80
#define SAT_SYSCALL_MKNOD	 14
#define SAT_SYSCALL_MOUNT	 21
#define SAT_SYSCALL_OPEN	  5
#define SAT_SYSCALL_PIPE	 42
#define SAT_SYSCALL_PROCBLK	131
#define SAT_SYSCALL_RENAME	114
#define SAT_SYSCALL_RMDIR	 79
#define SAT_SYSCALL_SETGID	 46
#define SAT_SYSCALL_SETREGID	123
#define SAT_SYSCALL_SETREUID	124
#define SAT_SYSCALL_SETUID	 23
#define SAT_SYSCALL_SYSSGI	 40
#define SAT_SYSCALL_STAT	 18
#define SAT_SYSCALL_TRUNCATE	112
#define SAT_SYSCALL_UMASK	 60
#define SAT_SYSCALL_UMOUNT	 22
#define SAT_SYSCALL_UNLINK	 10
#define SAT_SYSCALL_UTIME	 30

	/******************************/
	/*	SAT record types      */
	/******************************/
/*
 * If you add or remove a record type, update sat_init in sat.c as well
 * as the sat_eventtostr library function (event-to-string mapping).
 */

/* Path name record types */
#define SAT_FILE_HEADER		0	/* special type for SAT file headers */
#define SAT_ACCESS_DENIED	1	/* file access denied */
#define SAT_ACCESS_FAILED	2	/* file access failed (e.g. no file) */
#define SAT_CHDIR		3	/* change working directory */
#define SAT_CHROOT		4	/* change root directory */
#define SAT_OPEN		5	/* file open */
#define SAT_OPEN_RO		6	/* file open, read only */
#define SAT_READ_SYMLINK	7	/* read symbolic link */
#define SAT_FILE_CRT_DEL	8	/* file creation/deletion */
#define SAT_FILE_CRT_DEL2	9	/* as above with two pathnames */
#define SAT_FILE_WRITE		10	/* file data write */
#define SAT_MOUNT		11	/* mount/unmount */
#define SAT_FILE_ATTR_READ	12	/* file attribute read */
#define SAT_FILE_ATTR_WRITE	13	/* file attribute write */
#define SAT_EXEC		14	/* exec */
#define SAT_SYSACCT		15	/* system accounting */
/* File descriptor record types */
#define SAT_FCHDIR		20	/* change working directory via fd */
#define SAT_FD_READ		21	/* read file data or attrs via fd */
#define SAT_FD_READ2		22	/* as above with a set of fd's */
#define SAT_TTY_SETLABEL	23	/* tty reclassify (ioctl) */
#define SAT_FD_WRITE		24	/* write file data via fd */
#define SAT_FD_ATTR_WRITE	25	/* write file attributes via fd */
#define SAT_PIPE		26	/* create a pipe */
#define SAT_DUP			27	/* duplicate a descriptor */
#define SAT_CLOSE		28	/* close a descriptor */
/* Process record types */
#define SAT_FORK		40	/* create a new process */
#define SAT_EXIT		41	/* destroy a (this) process */
#define SAT_PROC_READ		42	/* read a process's addr space */
#define SAT_PROC_WRITE		43	/* write a process's addr space */
#define SAT_PROC_ATTR_READ	44	/* read a process's attributes */
#define SAT_PROC_ATTR_WRITE	45	/* change a process's attributes */
#define SAT_PROC_OWN_ATTR_WRITE	46	/* change this process's attributes */
#define SAT_PROC_ACCT		47	/* process end accounting data */
#define SAT_SESSION_ACCT	48	/* session end accounting data */
/* System V IPC record types */
#define SAT_SVIPC_ACCESS	50	/* System V IPC access */
#define SAT_SVIPC_CREATE	51	/* System V IPC create */
#define SAT_SVIPC_REMOVE	52	/* System V IPC remove */
#define SAT_SVIPC_CHANGE	53	/* System V IPC change */
/* BSD IPC record types */
#define	SAT_BSDIPC_CREATE	60	/* socket, accept */
#define	SAT_BSDIPC_CREATE_PAIR	61	/* socketpair */
#define	SAT_BSDIPC_SHUTDOWN	62	/* shutdown */
#define	SAT_BSDIPC_MAC_CHANGE	63	/* setsockopt */
#define	SAT_BSDIPC_ADDRESS	64	/* bind, connect, accept syscalls */
#define SAT_BSDIPC_RESVPORT	65	/* bind to reserved port */
#define SAT_BSDIPC_DELIVER	66	/* rx pkt delivered to socket	*/
#define SAT_BSDIPC_CANTFIND	67	/* rx pkt no match on port/label */
#define SAT_BSDIPC_SNOOP_OK	68	/* raw socket delivery permitted */
#define SAT_BSDIPC_SNOOP_FAIL	69	/* raw socket delivery denied	*/

/* Public object record types */
#define SAT_CLOCK_SET		70	/* set the system clock */
#define SAT_HOSTNAME_SET	71	/* set the host name */
#define SAT_DOMAINNAME_SET	72	/* set the domain name */
#define SAT_HOSTID_SET		73	/* set the host id */
/* other record types */
#define SAT_CHECK_PRIV		80	/* make-or-break privilege checks */
#define SAT_CONTROL		81	/* audit controls */
#define SAT_SYS_NOTE		82	/* debug string */
/* more BSD IPC types */
#define SAT_BSDIPC_DAC_CHANGE	87	/* change socket uid or acl	*/
#define SAT_BSDIPC_DAC_DENIED	88	/* rx pkt not delivered	due to DAC  */
#define SAT_BSDIPC_IF_SETUID	89	/* ioctl SIOCSIFUID succeed/fail */
#define SAT_BSDIPC_RX_OK	90	/* rx pkt label in range	*/
#define SAT_BSDIPC_RX_RANGE	91	/* rx pkt label out of range	*/
#define SAT_BSDIPC_RX_MISSING	92	/* rx pkt label missing/malformed */
#define SAT_BSDIPC_TX_OK	93	/* tx pkt label in range	*/
#define SAT_BSDIPC_TX_RANGE	94	/* tx pkt label out of range	*/
#define SAT_BSDIPC_TX_TOOBIG	95	/* tx pkt label doesn't fit	*/
#define SAT_BSDIPC_IF_CONFIG	96	/* configure interface address	*/
#define	SAT_BSDIPC_IF_INVALID	97	/* ioctl SIOCSIFLABEL disallowed */
#define SAT_BSDIPC_IF_SETLABEL	98	/* ioctl SIOCSIFLABEL succeeded */

/* record types for user-level records generated with satwrite(2) */
#define SAT_USER_RECORDS	100	/* beginning of non-kernel auditing */
#define SAT_AE_AUDIT		100	/* audit subsys reporting on itself */
#define SAT_AE_IDENTITY		101	/* identification & authentication */
#define SAT_AE_DBEDIT		102	/* admin database editor */
#define SAT_AE_MOUNT		103	/* mount / unmount */
#define SAT_AE_CUSTOM		104	/* user-defined */
#define SAT_AE_LP		105	/* lp subsystem */
#define SAT_AE_X_ALLOWED	106	/* X11 Server permitted accesses */
#define SAT_AE_X_DENIED		107	/* X11 Server prohibited accesses */

/* record types for svr4 networking */
#define	SAT_SVR4NET_CREATE	120	/* socket, accept */
#define	SAT_SVR4NET_ADDRESS	121	/* bind, connect, accept syscalls */
#define	SAT_SVR4NET_SHUTDOWN	122	/* shutdown */

/* extended process attributes */
#define SAT_PROC_OWN_EXT_ATTR_WRITE	123	/* capability, mac */

#define SAT_NTYPES		130	/* max record type + 1 (or greater) */

typedef	struct sat_ev_mask {
	unsigned int	ev_bits[howmany(SAT_NTYPES, NSATBITS)];
} sat_event_mask;

/*
 *  Generic information required for every sat audit record.
 *  Currently this infomation duplicates information in the
 *  user area.  In the future this dupication can be eliminated,
 *  but for the Trusted Irix 5.3 release, we don't want to
 *  alter the shape of the user area.
 */

typedef struct sat_proc {
	u_short         sat_subsysnum;	/* cmd arg from sysgi and ilk */
	u_short         sat_suflag;	/* superuser checks, etc. */
	u_short         sat_sequence;	/* event sequence number */
	uid_t           sat_uid;	/* SAT user-id (only set by login) */
	sat_token_t	sat_cwd;	/* current directory */
	sat_token_t	sat_root;	/* current root */
	sat_token_t	sat_pn;		/* Pathname assembled by lookup */
	sat_token_t	sat_tokens;	/* Full tokens for current activity */
	char            sat_comm[PSCOMSIZ];
	int		sat_openfd;
	int		sat_event;	/* event type number */
	cap_value_t	sat_cap;	/* capability used */
	char*		sat_abs;	/* Absolute pathname base */
} sat_proc_t;

/*
 * sat_suflag values
 */
#define SAT_SUSERCHK	0x0001	/* auditing: superuser was checked */
#define SAT_SUSERPOSS	0x0002	/* auditing: uid == 0 when checked */
#define SAT_CAPPOSS	0x0004	/* auditing: capability ON when checked */
#define SAT_PATHLESS	0x0008	/* auditing: don't gather pathnames */
#define SAT_IGNORE	0x0010	/* auditing: this event is not interesting */

/*
 * Audit record token types - sat_token_id_t's
 */
#define SAT_TOKEN_BASE			0
#define SAT_RECORD_HEADER_TOKEN		(SAT_TOKEN_BASE + 0x01)
#define SAT_IFREQ_TOKEN			(SAT_TOKEN_BASE + 0x02)
#define SAT_PROTOCOL_TOKEN		(SAT_TOKEN_BASE + 0x03)
#define SAT_TIME_TOKEN			(SAT_TOKEN_BASE + 0x04)
#define SAT_SYSCALL_TOKEN		(SAT_TOKEN_BASE + 0x05)
#define SAT_UGID_TOKEN			(SAT_TOKEN_BASE + 0x06)
#define SAT_FILE_TOKEN			(SAT_TOKEN_BASE + 0x07)
#define SAT_SOCKADDER_TOKEN		(SAT_TOKEN_BASE + 0x08)
#define SAT_IP_HEADER_TOKEN		(SAT_TOKEN_BASE + 0x09)
#define SAT_GID_LIST_TOKEN		(SAT_TOKEN_BASE + 0x0a)
#define SAT_UID_LIST_TOKEN		(SAT_TOKEN_BASE + 0x0b)
#define SAT_SYSARG_LIST_TOKEN		(SAT_TOKEN_BASE + 0x0c)
#define SAT_DESCRIPTOR_LIST_TOKEN	(SAT_TOKEN_BASE + 0x0d)
#define SAT_IFNAME_TOKEN		(SAT_TOKEN_BASE + 0x0e)
#define SAT_SOCKET_TOKEN		(SAT_TOKEN_BASE + 0x0f)
#define SAT_MAC_LABEL_TOKEN		(SAT_TOKEN_BASE + 0x10)
#define SAT_ACL_TOKEN			(SAT_TOKEN_BASE + 0x11)
#define SAT_CAP_VALUE_TOKEN		(SAT_TOKEN_BASE + 0x12)
#define SAT_CAP_SET_TOKEN		(SAT_TOKEN_BASE + 0x13)
#define SAT_TEXT_TOKEN			(SAT_TOKEN_BASE + 0x14)
#define SAT_SVIPC_KEY_TOKEN		(SAT_TOKEN_BASE + 0x15)
#define SAT_SVIPC_ID_TOKEN		(SAT_TOKEN_BASE + 0x16)
#define SAT_MODE_TOKEN			(SAT_TOKEN_BASE + 0x17)
#define SAT_PORT_TOKEN			(SAT_TOKEN_BASE + 0x18)
#define SAT_HOSTID_TOKEN		(SAT_TOKEN_BASE + 0x19)
#define SAT_BINARY_TOKEN		(SAT_TOKEN_BASE + 0x1a)
#define SAT_PID_TOKEN			(SAT_TOKEN_BASE + 0x1b)
#define SAT_PRIVILEGE_TOKEN		(SAT_TOKEN_BASE + 0x1c)
#define SAT_ERRNO_TOKEN			(SAT_TOKEN_BASE + 0x1d)
#define SAT_SATID_TOKEN			(SAT_TOKEN_BASE + 0x1e)
#define SAT_DEVICE_TOKEN		(SAT_TOKEN_BASE + 0x1f)
#define SAT_TITLED_TEXT_TOKEN		(SAT_TOKEN_BASE + 0x20)
#define SAT_PATHNAME_TOKEN		(SAT_TOKEN_BASE + 0x21)
#define SAT_OPENMODE_TOKEN		(SAT_TOKEN_BASE + 0x22)
#define SAT_SIGNAL_TOKEN		(SAT_TOKEN_BASE + 0x23)
#define SAT_STATUS_TOKEN		(SAT_TOKEN_BASE + 0x24)
#define SAT_OPAQUE_TOKEN		(SAT_TOKEN_BASE + 0x25)
#define SAT_LOOKUP_TOKEN		(SAT_TOKEN_BASE + 0x26)
#define SAT_CWD_TOKEN			(SAT_TOKEN_BASE + 0x27)
#define SAT_ROOT_TOKEN			(SAT_TOKEN_BASE + 0x28)
#define SAT_PARENT_PID_TOKEN		(SAT_TOKEN_BASE + 0x29)
#define SAT_COMMAND_TOKEN		(SAT_TOKEN_BASE + 0x2a)
#define SAT_ACCT_COUNTS_TOKEN		(SAT_TOKEN_BASE + 0x2b)
#define SAT_ACCT_TIMERS_TOKEN		(SAT_TOKEN_BASE + 0x2c)
#define SAT_ACCT_PROC_TOKEN		(SAT_TOKEN_BASE + 0x2d)
#define SAT_ACCT_SESSION_TOKEN		(SAT_TOKEN_BASE + 0x2e)
#define SAT_ACCT_SPI_TOKEN		(SAT_TOKEN_BASE + 0x2f)
#define SAT_ACCT_SPI2_TOKEN		(SAT_TOKEN_BASE + 0x30)

typedef __uint64_t	sat_socket_id_t;
typedef __uint32_t	sat_descriptor_t;
typedef __uint64_t	sat_sysarg_t;

#define SAT_IFNAME_SIZE	16
typedef char *	sat_ifname_t;

#define SAT_TITLE_SIZE	8


#ifdef __cplusplus
}
#endif
#endif	/* !__SYS_SAT_H */
