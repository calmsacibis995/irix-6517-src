/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
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

#ifndef __SAT_H
#define __SAT_H
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.8 $"

#include <stdio.h>
#include <sys/sat.h>
#include <sys/capability.h>

/* Prototypes for libc/libmls interfaces */

extern int	satread( char *, unsigned );
extern int	satwrite( int, int, char *, unsigned );
extern int	satvwrite( int, int, char *, ... );
extern int	saton( int );
extern int	satoff( int );
extern int	satstate( int );
extern int	satsetid( int );
extern int	satgetid( void );
extern int	ia_audit(char *, char *, int, char *, ...);

extern char *	sat_eventtostr( int );
extern int	sat_strtoevent( const char * );
extern char *	sys_call( int, int );


/* defines for sat_read_file_info() */
#define	SFI_NONE	0x00
#define	SFI_TIMEZONE	0x01
#define	SFI_HOSTNAME	0x02
#define	SFI_DOMNAME	0x04
#define	SFI_USERS	0x08
#define	SFI_GROUPS	0x10
#define	SFI_HOSTS	0x20
#define	SFI_BUFFER	0x40
#define	SFI_ALL		0xff

/* sat_read_file_info() returns */
#define SFI_OKAY	 0
#define SFI_ERROR	-1
#define SFI_WARNING	-2

/* defines for sat_read_header_info() */
#define	SHI_NONE	0x00
#define SHI_GROUPS	0x01
#define SHI_PLABEL	0x02
#define SHI_CWD		0x04
#define SHI_ROOTDIR	0x08
#define SHI_PNAME	0x10
#define SHI_BUFFER	0x20
#define SHI_CAP		0x40
#define SHI_ALL		0xff

/* sat_read_header_info() returns */
#define SHI_OKAY	 0
#define SHI_ERROR	-1


/* non-kernel structs */

/*
 * Processed version of sat_filehdr.
 * See sat_read_file_info().
 */
struct sat_file_info {
	int	sat_major;		/* version of audit data */
	int	sat_minor;
	time_t	sat_start_time;		/* time header written */
	time_t	sat_stop_time;		/* time file closed */
	sat_host_t sat_host_id;		/* host id */
	int	sat_mac_enabled;	/* boolean: ignore mac fields or not */
	int	sat_cap_enabled;	/* boolean: ignore cap fields or not */
	int	sat_cipso_enabled;	/* boolean: ignore cipso fields or
					   not */
	char *	sat_timezone;		/* "TZ=(timezone)" */
	char *	sat_hostname;		/* hostname */
	char *	sat_domainname;		/* domainname */
	int	sat_fhdrsize;		/* no. of bytes in disk image */
	char *	sat_buffer;		/* buffer w/disk image of header */
	int	sat_user_entries;	/* number of sat_list_ent structs */
	int	sat_group_entries;	/*   in the user, group, and */
	int	sat_host_entries;	/*   the hostid <-> name list */
	struct sat_list_ent **sat_users;	/* user entries */
	struct sat_list_ent **sat_groups;	/* group entries */
	struct sat_list_ent **sat_hosts;	/* hostid entries */
};

/*
 * Processed version of sat_hdr.
 * See sat_read_header_info().
 */
struct sat_hdr_info {
	int	sat_magic;	/* sat header "magic number" */
	int	sat_rectype;	/* what type of record follows */
	int	sat_outcome;	/* failure/success, because of dac/mac check */
	cap_value_t sat_cap;	/* what capability affected the result */
	int	sat_sequence;	/* sequence number for this record (by type) */
	int	sat_errno;	/* system call error number */
	time_t	sat_time;	/* seconds since 1970 */
	int	sat_ticks;	/* sub-second clock ticks (0-99) */
	int	sat_syscall;	/* system call number */
	int	sat_subsyscall;	/* system call "command" number */
	sat_host_t sat_host_id;	/* host id (new for format 2.0) */
	uid_t	sat_id;		/* SAT user-id */
	dev_t	sat_tty;	/* controlling tty, if present */
	pid_t	sat_ppid;	/* parent process id */
	pid_t	sat_pid;	/* process id of record's generator */
	char *	sat_pname;	/* process name, from u.u_comm */
	mac_label *sat_plabel;	/* process label */
	cap_t	sat_pcap;	/* capability set */
	uid_t	sat_euid;	/* Effective user id */
	uid_t	sat_ruid;	/* Real user id */
	gid_t	sat_egid;	/* Effective group id */
	gid_t	sat_rgid;	/* Real group id */
	int	sat_ngroups;	/* number of multi-group entries */
	gid_t *	sat_groups;	/* group list */
	char *	sat_cwd;	/* current working directory */
	char *	sat_rootdir;	/* current root directory */
	int	sat_recsize;	/* bytes in the following record */
	int	sat_hdrsize;	/* no. of bytes in disk image of header */
	char *	sat_buffer;	/* buffer holding disk image of header */
};

/* audit file interfaces */

struct sat_hdr;
struct sat_pathname;
extern int	sat_read_file_info( FILE *, FILE *, struct sat_file_info *,
				    int );
extern int	sat_write_file_info( FILE *, struct sat_file_info * );
extern void	sat_free_file_info( struct sat_file_info * );

extern int	sat_write_filehdr( int );
extern int	sat_close_filehdr( int );
extern int	sat_read_header_info( FILE *, struct sat_hdr_info *, int,
				      int, int );
extern void	sat_free_header_info( struct sat_hdr_info *);
extern int	sat_intrp_pathname( char **, struct sat_pathname *,
			           char **, char **, mac_label **, int, int );

#ifdef __cplusplus
}
#endif
#endif /* !__SAT_H */
