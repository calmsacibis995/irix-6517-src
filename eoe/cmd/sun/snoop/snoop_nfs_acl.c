/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_nfs_acl.c,v 1.2 1996/07/06 17:51:25 nn Exp $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/tiuser.h>
#include <setjmp.h>
#include <pwd.h>
#include <grp.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <string.h>
#include "snoop.h"

#include <sys/stat.h>

extern char *get_sum_line();
extern void check_retransmit();
extern char *sum_nfsfh();
extern int sum_nfsstat();
extern int detail_nfsstat();
extern void detail_nfsfh();
extern void detail_fattr();
extern void skip_fattr();
extern char *sum_nfsfh3();
extern int sum_nfsstat3();
extern int detail_nfsstat3();
extern void detail_post_op_attr();
extern void detail_nfsfh3();

void interpret_nfs_acl2(int flags, int type, int xid, int vers, int proc, char *data, int len);
void interpret_nfs_acl3(int flags, int type, int xid, int vers, int proc, char *data, int len);


extern jmp_buf xdr_err;

static void aclcall2();
static void aclreply2();
static void aclcall3();
static void aclreply3();
static void detail_access2();
static char *sum_access2();
static void detail_mask();
static void detail_secattr();
static void detail_aclent();
static char *detail_uname();
static char *detail_gname();
static char *detail_perm(u_short);

#define	ACLPROC2_NULL		((unsigned long)(0))
#define	ACLPROC2_GETACL		((unsigned long)(1))
#define	ACLPROC2_SETACL		((unsigned long)(2))
#define	ACLPROC2_GETATTR	((unsigned long)(3))
#define	ACLPROC2_ACCESS		((unsigned long)(4))

#define	ACLPROC3_NULL		((unsigned long)(0))
#define	ACLPROC3_GETACL		((unsigned long)(1))
#define	ACLPROC3_SETACL		((unsigned long)(2))

#define	NA_USER_OBJ	0x1
#define	NA_USER		0x2
#define	NA_GROUP_OBJ	0x4
#define	NA_GROUP	0x8
#define	NA_CLASS_OBJ	0x10
#define	NA_OTHER_OBJ	0x20
#define	NA_ACL_DEFAULT	0x1000

#define	NA_DEF_USER_OBJ		(NA_USER_OBJ | NA_ACL_DEFAULT)
#define	NA_DEF_USER		(NA_USER | NA_ACL_DEFAULT)
#define	NA_DEF_GROUP_OBJ	(NA_GROUP_OBJ | NA_ACL_DEFAULT)
#define	NA_DEF_GROUP		(NA_GROUP | NA_ACL_DEFAULT)
#define	NA_DEF_CLASS_OBJ	(NA_CLASS_OBJ | NA_ACL_DEFAULT)
#define	NA_DEF_OTHER_OBJ	(NA_OTHER_OBJ | NA_ACL_DEFAULT)

#define	NA_ACL		0x1
#define	NA_ACLCNT	0x2
#define	NA_DFACL	0x4
#define	NA_DFACLCNT	0x8

#define	ACCESS2_READ	0x0001
#define	ACCESS2_LOOKUP	0x0002
#define	ACCESS2_MODIFY	0x0004
#define	ACCESS2_EXTEND	0x0008
#define	ACCESS2_DELETE	0x0010
#define	ACCESS2_EXECUTE	0x0020

static char *procnames_short_v2[] = {
	"NULL2",	/*  0 */
	"GETACL2",	/*  1 */
	"SETACL2",	/*  2 */
	"GETATTR2",	/*  3 */
	"ACCESS2",	/*  4 */
};
static char *procnames_short_v3[] = {
	"NULL3",	/*  0 */
	"GETACL3",	/*  1 */
	"SETACL3",	/*  2 */
};

static char *procnames_long_v2[] = {
	"Null procedure",		/*  0 */
	"Get file access control list",	/*  1 */
	"Set file access control list",	/*  2 */
	"Get file attributes",		/*  3 */
	"Check access permission",	/*  4 */
};
static char *procnames_long_v3[] = {
	"Null procedure",		/*  0 */
	"Get file access control list",	/*  1 */
	"Set file access control list",	/*  2 */
};

#define	MAXPROC_V2	4
#define	MAXPROC_V3	2

/* ARGSUSED */
void
interpret_nfs_acl(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{

	if (vers == 2) {
		interpret_nfs_acl2(flags, type, xid, vers, proc, data, len);
		return;
	}

	if (vers == 3) {
		interpret_nfs_acl3(flags, type, xid, vers, proc, data, len);
		return;
	}
}

void
interpret_nfs_acl2(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff[2048];
	int off, sz;
	char *fh;
	u_long mask;

	if (proc < 0 || proc > MAXPROC_V2)
		return;

	if (flags & F_SUM) {
		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line, "NFS_ACL C %s",
				procnames_short_v2[proc]);
			line += strlen(line);
			switch (proc) {
			case ACLPROC2_GETACL:
				fh = sum_nfsfh();
				mask = getxdr_u_long();
				(void) sprintf(line, "%s mask=%lu", fh, mask);
				break;
			case ACLPROC2_SETACL:
				(void) sprintf(line, sum_nfsfh());
				break;
			case ACLPROC2_GETATTR:
				(void) sprintf(line, sum_nfsfh());
				break;
			case ACLPROC2_ACCESS:
				fh = sum_nfsfh();
				(void) sprintf(line, "%s (%s)", fh,
						sum_access2());
				break;
			default:
				break;
			}

			check_retransmit(line, (u_long)xid);
		} else {
			(void) sprintf(line, "NFS_ACL R %s ",
				procnames_short_v2[proc]);
			line += strlen(line);
			switch (proc) {
			case ACLPROC2_GETACL:
				(void) sum_nfsstat(line);
				break;
			case ACLPROC2_SETACL:
				(void) sum_nfsstat(line);
				break;
			case ACLPROC2_GETATTR:
				(void) sum_nfsstat(line);
				break;
			case ACLPROC2_ACCESS:
				if (sum_nfsstat(line) == 0) {
					skip_fattr();
					line += strlen(line);
					(void) sprintf(line, " (%s)",
							sum_access2());
				}
				break;
			default:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NFS_ACL:  ", "Sun NFS_ACL", len);
		show_space();
		(void) sprintf(get_line(0, 0), "Proc = %d (%s)",
			proc, procnames_long_v2[proc]);
		if (type == CALL)
			aclcall2(proc);
		else
			aclreply2(proc);
		show_trailer();
	}
}

void
interpret_nfs_acl3(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff[2048];
	int off, sz;
	char *fh;
	u_long mask;

	if (proc < 0 || proc > MAXPROC_V3)
		return;

	if (flags & F_SUM) {
		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line, "NFS_ACL C %s",
				procnames_short_v3[proc]);
			line += strlen(line);
			switch (proc) {
			case ACLPROC3_GETACL:
				fh = sum_nfsfh3();
				mask = getxdr_u_long();
				(void) sprintf(line, "%s mask=%lu", fh, mask);
				break;
			case ACLPROC2_SETACL:
				(void) sprintf(line, sum_nfsfh3());
				break;
			default:
				break;
			}

			check_retransmit(line, (u_long)xid);
		} else {
			(void) sprintf(line, "NFS_ACL R %s ",
				procnames_short_v3[proc]);
			line += strlen(line);
			switch (proc) {
			case ACLPROC3_GETACL:
				(void) sum_nfsstat3(line);
				break;
			case ACLPROC3_SETACL:
				(void) sum_nfsstat3(line);
				break;
			default:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("NFS_ACL:  ", "Sun NFS_ACL", len);
		show_space();
		(void) sprintf(get_line(0, 0), "Proc = %d (%s)",
			proc, procnames_long_v3[proc]);
		if (type == CALL)
			aclcall3(proc);
		else
			aclreply3(proc);
		show_trailer();
	}
}

/*
 * Print out version 2 NFS_ACL call packets
 */
static void
aclcall2(proc)
	int proc;
{

	switch (proc) {
	case ACLPROC2_GETACL:
		detail_nfsfh();
		detail_mask();
		break;
	case ACLPROC2_SETACL:
		detail_nfsfh();
		detail_secattr();
		break;
	case ACLPROC2_GETATTR:
		detail_nfsfh();
		break;
	case ACLPROC2_ACCESS:
		detail_nfsfh();
		detail_access2();
		break;
	default:
		break;
	}
}

/*
 * Print out version 2 NFS_ACL reply packets
 */
static void
aclreply2(proc)
	int proc;
{

	switch (proc) {
	case ACLPROC2_GETACL:
		if (detail_nfsstat() == 0) {
			detail_fattr();
			detail_secattr();
		}
		break;
	case ACLPROC2_SETACL:
		if (detail_nfsstat() == 0)
			detail_fattr();
		break;
	case ACLPROC2_GETATTR:
		if (detail_nfsstat() == 0)
			detail_fattr();
		break;
	case ACLPROC2_ACCESS:
		if (detail_nfsstat() == 0) {
			detail_fattr();
			detail_access2();
		}
		break;
	default:
		break;
	}
}

/*
 * Print out version 3 NFS_ACL call packets
 */
static void
aclcall3(proc)
	int proc;
{

	switch (proc) {
	case ACLPROC3_GETACL:
		detail_nfsfh3();
		detail_mask();
		break;
	case ACLPROC3_SETACL:
		detail_nfsfh3();
		detail_secattr();
		break;
	default:
		break;
	}
}

/*
 * Print out version 3 NFS_ACL reply packets
 */
static void
aclreply3(proc)
	int proc;
{

	switch (proc) {
	case ACLPROC3_GETACL:
		if (detail_nfsstat3() == 0) {
			detail_post_op_attr("");
			detail_secattr();
		}
		break;
	case ACLPROC3_SETACL:
		if (detail_nfsstat3() == 0)
			detail_post_op_attr("");
		break;
	default:
		break;
	}
}

static void
detail_access2()
{
	u_int bits;

	bits = showxdr_u_long("Access bits = 0x%08x");
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(bits, ACCESS2_READ, "Read", "(no read)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(bits, ACCESS2_LOOKUP, "Lookup", "(no lookup)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(bits, ACCESS2_MODIFY, "Modify", "(no modify)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(bits, ACCESS2_EXTEND, "Extend", "(no extend)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(bits, ACCESS2_DELETE, "Delete", "(no delete)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(bits, ACCESS2_EXECUTE, "Execute", "(no execute)"));
}

static char *
sum_access2()
{
	int bits;
	static char buff[22];

	bits = getxdr_u_long();
	buff[0] = '\0';

	if (bits & ACCESS2_READ)
		(void) strcat(buff, "read,");
	if (bits & ACCESS2_LOOKUP)
		(void) strcat(buff, "lookup,");
	if (bits & ACCESS2_MODIFY)
		(void) strcat(buff, "modify,");
	if (bits & ACCESS2_EXTEND)
		(void) strcat(buff, "extend,");
	if (bits & ACCESS2_DELETE)
		(void) strcat(buff, "delete,");
	if (bits & ACCESS2_EXECUTE)
		(void) strcat(buff, "execute,");
	if (buff[0] != '\0')
		buff[strlen(buff) - 1] = '\0';

	return (buff);
}

static void
detail_mask()
{
	u_long mask;

	mask = showxdr_u_long("Mask = 0x%lx");
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(mask, NA_ACL, "aclent", "(no aclent)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(mask, NA_ACLCNT, "aclcnt", "(no aclcnt)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(mask, NA_DFACL, "dfaclent", "(no dfaclent)"));
	(void) sprintf(get_line(0, 0), "	%s",
		getflag(mask, NA_DFACLCNT, "dfaclcnt", "(no dfaclcnt)"));
}

static void
detail_secattr()
{

	detail_mask();
	showxdr_long("Aclcnt = %d");
	detail_aclent();
	showxdr_long("Dfaclcnt = %d");
	detail_aclent();
}

static void
detail_aclent()
{
	int count;
	int type;
	int id;
	u_short perm;

	count = getxdr_long();
	while (count-- > 0) {
		type = getxdr_long();
		id = getxdr_long();
		perm = getxdr_u_short();
		switch (type) {
		case NA_USER:
			(void) sprintf(get_line(0, 0), "\tuser:%s:%s",
					detail_uname(id), detail_perm(perm));
			break;
		case NA_USER_OBJ:
			(void) sprintf(get_line(0, 0), "\tuser::%s",
					detail_perm(perm));
			break;
		case NA_GROUP:
			(void) sprintf(get_line(0, 0), "\tgroup:%s:%s",
					detail_gname(id), detail_perm(perm));
			break;
		case NA_GROUP_OBJ:
			(void) sprintf(get_line(0, 0), "\tgroup::%s",
					detail_perm(perm));
			break;
		case NA_CLASS_OBJ:
			(void) sprintf(get_line(0, 0), "\tmask:%s",
					detail_perm(perm));
			break;
		case NA_OTHER_OBJ:
			(void) sprintf(get_line(0, 0), "\tother:%s",
					detail_perm(perm));
			break;
		case NA_DEF_USER:
			(void) sprintf(get_line(0, 0), "\tdefault:user:%s:%s",
					detail_uname(id), detail_perm(perm));
			break;
		case NA_DEF_USER_OBJ:
			(void) sprintf(get_line(0, 0), "\tdefault:user::%s",
					detail_perm(perm));
			break;
		case NA_DEF_GROUP:
			(void) sprintf(get_line(0, 0), "\tdefault:group:%s:%s",
					detail_gname(id), detail_perm(perm));
			break;
		case NA_DEF_GROUP_OBJ:
			(void) sprintf(get_line(0, 0), "\tdefault:group::%s",
					detail_perm(perm));
			break;
		case NA_DEF_CLASS_OBJ:
			(void) sprintf(get_line(0, 0), "\tdefault:mask:%s",
					detail_perm(perm));
			break;
		case NA_DEF_OTHER_OBJ:
			(void) sprintf(get_line(0, 0), "\tdefault:other:%s",
					detail_perm(perm));
			break;
		default:
			(void) sprintf(get_line(0, 0), "\tunrecognized entry");
			break;
		}
	}
}

static char *
detail_uname(uid_t uid)
{
	struct passwd *pwd;
	static char uidp[10];

	pwd = getpwuid(uid);
	if (pwd == NULL) {
		sprintf(uidp, "%d", uid);
		return (uidp);
	}
	return (pwd->pw_name);
}

static char *
detail_gname(gid_t gid)
{
	struct group *grp;
	static char gidp[10];

	grp = getgrgid(gid);
	if (grp == NULL) {
		sprintf(gidp, "%d", gid);
		return (gidp);
	}
	return (grp->gr_name);
}

static char *perms[] = {
	"---",
	"--x",
	"-w-",
	"-wx",
	"r--",
	"r-x",
	"rw-",
	"rwx"
};
static char *
detail_perm(u_short perm)
{

	if (perm > sizeof (perms) / sizeof (perms[0]))
		return ("?");
	return (perms[perm]);
}
