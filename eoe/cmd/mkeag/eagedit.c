/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.1 $"
/*
 * Update Plan G data files directly.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mls.h>
#include <mntent.h>
#include <pwd.h>
#include <grp.h>

#ifdef	LATER_CBS
#include <sys/inf_label.h>
#include <sys/acl.h>
#endif	/* LATER_CBS */

/*
 * Verification modes
 */
#define	DO_NOT_VERIFY	0	/* Don't verify at all */
#define VERIFY_ALL	1	/* Produce a message for everything */
#define VERIFY_EXISTING	2	/* Produce a message iff it's there and wrong */

char *program;

void
arg_error(char *arg, char *value)
{
	fprintf(stderr, "%s: argument %s cannot use value \"%s\".\n",
	    program, arg, value);
	exit(1);
}

void
script_error(char *arg, char *value)
{
	fprintf(stderr, "%s: script line \"%s\" cannot use value \"%s\".\n",
	    program, arg, value);
}

struct mount_table {
	struct mount_table	*next;
	char			*mnt;
	dev_t			dev;
} *mount_table;

void
read_mount_table()
{
	struct mntent *mp;
	struct mount_table *mte;
	struct mount_table *new;
	struct stat statbuf;

	FILE *fp = setmntent("/etc/mtab", "r");

	while (mp = getmntent(fp)) {
		if (strcmp(mp->mnt_type, "efs"))
			continue;
		if (lstat(mp->mnt_dir, &statbuf) < 0) {
			fprintf(stderr, "%s: cannot read file system \"%s\".\n",
			    program, mp->mnt_dir);
			continue;
		}
		mte = malloc(sizeof(struct mount_table));
		mte->mnt = strdup(mp->mnt_dir);
		mte->dev = statbuf.st_dev;
		mte->next = mount_table;
		mount_table = mte;
	}
	endmntent(fp);
}

int
verify_dbfile(struct mount_table *mte, char *root, char *dbfile)
{
	char fullpath[MAXPATHLEN];
	struct stat filestatbuf;

	sprintf(fullpath, "%s/%s/%s", root, mte->mnt, dbfile);

	if (lstat(fullpath, &filestatbuf) < 0) {
		fprintf(stderr, "%s: Database file \"%s\" is inaccessable. ",
		    program, fullpath);
		perror("Reason");
		return (1);
	}
	return (0);
}

int
doopen_rw(struct mount_table *mte, char *root, char *dbfile)
{
	char fullpath[MAXPATHLEN];
	int fd;

	sprintf(fullpath, "%s/%s/%s", root, mte->mnt, dbfile);

	if ((fd = open(fullpath, O_RDWR | O_SYNC)) < 0) {
		fprintf(stderr, "%s: Database file \"%s\" is imutable. ",
		    program, fullpath);
		perror("Reason");
		exit(1);
	}
	return (fd);
}

int
doopen_ro(struct mount_table *mte, char *root, char *dbfile)
{
	char fullpath[MAXPATHLEN];
	int fd;

	sprintf(fullpath, "%s/%s/%s", root, mte->mnt, dbfile);

	if ((fd = open(fullpath, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: Database file \"%s\" is unreadable. ",
		    program, fullpath);
		perror("Reason");
		exit(1);
	}
	return (fd);
}

int
readfrom(int fd, int offset, int size, void *buffer)
{
	if (lseek(fd, offset, SEEK_SET) < 0)
		return (-1);

	if (read(fd, buffer, size) != size)
		return (-1);

	return (0);
}

int
writeto(int fd, int offset, int size, void *buffer)
{
	if (offset >= 0) {
		if (lseek(fd, offset, SEEK_SET) < 0)
			return (-1);
	}
	else if (lseek(fd, 0, SEEK_END) < 0)
		return (-1);

	if (write(fd, buffer, size) != size)
		return (-1);

	return (0);
}

char *
verify_cap(struct mount_table *mte, char *root, capability_t *allowed,
	   capability_t *forced, int seteff, int inode)
{
	static int cap_index_fd = -1;
	static int cap_set_fd = -1;
	static struct mount_table *last_mte;
	static char result[512];
	cap_eag_index_t capindex;
	cap_eag_t capset;
	char *cp;
	int error = 0;

	if (last_mte != mte) {
		if (cap_index_fd >= 0)
			close(cap_index_fd);
		if (cap_set_fd >= 0)
			close(cap_set_fd);
		cap_index_fd = doopen_ro(mte, root, CAP_EAG_INDEX);
		cap_set_fd = doopen_ro(mte, root, CAP_EAG_SET);
		last_mte = mte;
	}

	if (readfrom(cap_index_fd, inode * sizeof(cap_eag_index_t),
	    sizeof(cap_eag_index_t), &capindex)) {
		fprintf(stderr, "%s: Capability verify read failure.\n",
		    program);
		exit(1);
	}

	if (capindex == -1) {
		bzero(&capset, sizeof(cap_eag_t));
	}
	else if (readfrom(cap_set_fd, capindex, sizeof(cap_eag_t), &capset)) {
		fprintf(stderr, "%s: Capability verify read failure.\n",
		    program);
		exit(1);
	}

	result[0] = '\0';

	if (allowed) {
		if (bcmp(allowed, &capset.cap_allowed, sizeof(capability_t))) {
			cp = sgi_cap_captostr(&capset.cap_allowed);
			strcat(result, ":cap-allowed=");
			strcat(result, cp);
			free(cp);
			cp = sgi_cap_captostr(allowed);
			strcat(result, "(");
			strcat(result, cp);
			strcat(result, ")");
			free(cp);
		}
	}
	if (forced) {
		if (bcmp(forced, &capset.cap_forced, sizeof(capability_t))) {
			cp = sgi_cap_captostr(&capset.cap_forced);
			strcat(result, ":cap-forced=");
			strcat(result, cp);
			free(cp);
			cp = sgi_cap_captostr(forced);
			strcat(result, "(");
			strcat(result, cp);
			strcat(result, ")");
			free(cp);
		}
	}
	if (seteff >= 0) {
		if (capset.cap_flags != seteff) {
			strcat(result, ":cap-set-effective=");
			strcat(result, capset.cap_flags ? "on" : "off");
			strcat(result, "(");
			strcat(result, seteff ? "on" : "off");
			strcat(result, ")");
		}
	}
	return ((result[0] == '\0') ? NULL : result);
}

char *
verify_mac(struct mount_table *mte, char *root, mac_label *lp, int inode)
{
	static int mac_index_fd = -1;
	static struct mount_table *last_mte;
	static char result[512];
	mac_eag_index_t index_entry;
	char *cp;
	char *op;

	if (last_mte != mte) {
		if (mac_index_fd >= 0)
			close(mac_index_fd);
		mac_index_fd = doopen_ro(mte, root, MAC_EAG_INDEX);
		last_mte = mte;
	}

	if (readfrom(mac_index_fd, inode * sizeof(mac_eag_index_t),
	    sizeof(mac_eag_index_t), &index_entry)) {
		fprintf(stderr, "%s: MAC verify read failure.\n", program);
		exit(1);
	}

	if (!(index_entry.mei_flags & MAC_EAG_DIRECT)) {
		sprintf(result, ":mac=INDIRECT");
		return (result);
	}
	if (bcmp(lp, &index_entry, 8)) {
		cp = mac_labeltostr((mac_label *)&index_entry, NAME_OR_COMP);
		op = mac_labeltostr(lp, NAME_OR_COMP);
		sprintf(result, ":mac=%s(%s)", cp, op);
		free(cp);
		free(op);
		return (result);
	}
	return (NULL);
}

void
update_cap(struct mount_table *mte, char *root, capability_t *allowed,
	capability_t *forced, int seteff, int inode)
{
	static int cap_index_fd = -1;
	static int cap_set_fd = -1;
	static struct mount_table *last_mte;
	cap_eag_index_t capindex;
	cap_eag_t capset;

	if (last_mte != mte) {
		if (cap_index_fd >= 0)
			close(cap_index_fd);
		if (cap_set_fd >= 0)
			close(cap_set_fd);
		cap_index_fd = doopen_rw(mte, root, CAP_EAG_INDEX);
		cap_set_fd = doopen_rw(mte, root, CAP_EAG_SET);
		last_mte = mte;
	}

	if (readfrom(cap_index_fd, inode * sizeof(cap_eag_index_t),
	    sizeof(cap_eag_index_t), &capindex)) {
		fprintf(stderr, "%s: Capability Update read failure.\n",
		    program);
		exit(1);
	}

	if (capindex == -1)
		bzero(&capset, sizeof(cap_eag_t));
	else if (readfrom(cap_set_fd, capindex, sizeof(cap_eag_t), &capset)) {
		fprintf(stderr, "%s: Capability Update read failure.\n",
		    program);
		exit(1);
	}

	if (allowed)
		capset.cap_allowed = *allowed;
	if (forced)
		capset.cap_forced = *forced;
	if (seteff >= 0)
		capset.cap_flags = seteff;

	if (capindex == -1) {
		capindex = lseek(cap_set_fd, 0, SEEK_END);
		if (writeto(cap_index_fd, inode * sizeof(cap_eag_index_t),
		    sizeof(cap_eag_index_t), &capindex)) {
			fprintf(stderr,"%s: Capability Update write failure.\n",
			    program);
			exit(1);
		}
	}
	if (writeto(cap_set_fd, capindex, sizeof(cap_eag_t), &capset)) {
		fprintf(stderr,"%s: Capability Update write failure.\n",
		    program);
		exit(1);
	}
}

void
update_mac(struct mount_table *mte, char *root, mac_label *lp, int inode)
{
	static int mac_index_fd = -1;
	static struct mount_table *last_mte;
	mac_eag_index_t index_entry;

	if (last_mte != mte) {
		if (mac_index_fd >= 0)
			close(mac_index_fd);
		mac_index_fd = doopen_rw(mte, root, MAC_EAG_INDEX);
		last_mte = mte;
	}

	bzero(&index_entry, sizeof(index_entry));
	index_entry.mei_flags = MAC_EAG_DIRECT;
	bcopy(lp, &index_entry, mac_size(lp));

	if (writeto(mac_index_fd, inode * sizeof(mac_eag_index_t),
	    sizeof(mac_eag_index_t), &index_entry)) {
		fprintf(stderr, "%s: MAC Update write failure.\n", program);
		exit(1);
	}
}

#ifdef	LATER_CBS
void
update_acl(struct mount_table *mte, char *root, acl_eag_t *acl,
	acl_eag_t *acl_dir, int inode)
{
	static int acl_index_fd = -1;
	static int acl_acl_fd = -1;

	if (acl_index_fd < 0) {
		acl_index_fd = doopen_rw(mte, root, ACL_EAG_INDEX);
		acl_acl_fd = doopen_rw(mte, root, ACL_EAG_ACL);
	}
}

void
update_inf(struct mount_table *mte, char *root, inf_label *lp, int inode)
{
	static int inf_index_fd = -1;
	static int inf_label_fd = -1;

	if (inf_index_fd < 0) {
		inf_index_fd = doopen_rw(mte, root, INF_EAG_INDEX);
		inf_label_fd = doopen_rw(mte, root, INF_EAG_LABEL);
	}
}
#endif	/* LATER_CBS */

int
doone(char *path, char *root, int verify, int edit, cap_eag_t *cep, 
	int cap_seteff, capability_t *cap_forced, capability_t *cap_allowed,
	char *filetypename, int filetype,
	uid_t userid, gid_t groupid, mode_t mode,
	mac_label *mac)
{
	int error = 0;
	struct passwd *pwd;
	struct group *grp;
	struct stat filestatbuf;
	struct mount_table *mte;
	char *cp;

	if (lstat(path, &filestatbuf) < 0) {
		if (verify == VERIFY_EXISTING)
			return (1);
		fprintf(stderr, "%s: \"%s\" is inaccessable. ",
		    program, path);
		perror("Reason");
		return (1);
	}
	for (mte = mount_table; mte; mte = mte->next)
		if (mte->dev == filestatbuf.st_dev)
			break;
	if (!mte) {
		fprintf(stderr,
		    "%s: Cannot find file system for \"%s\".\n",
		    program, path);
		return (1);
	}

	/*
	 * Verify the presence of the requested PlanG Databases.
	 */
	if (cep || cap_forced || cap_allowed || cap_seteff != -1) {
		error += verify_dbfile(mte, root, CAP_EAG_INDEX);
		error += verify_dbfile(mte, root, CAP_EAG_SET);
	}
	if (mac) {
		error += verify_dbfile(mte, root, MAC_EAG_INDEX);
		error += verify_dbfile(mte, root, MAC_EAG_LABEL);
	}
#ifdef	LATER_CBS
	if (inf) {
		error += verify_dbfile(mte, root, INF_EAG_INDEX);
		error += verify_dbfile(mte, root, INF_EAG_LABEL);
	}
	if (acl || acl_dir) {
		error += verify_dbfile(mte, root, ACL_EAG_INDEX);
		error += verify_dbfile(mte, root, ACL_EAG_ACL);
	}
#endif	/* LATER_CBS */
	if (error)
		return (error);

	switch (verify) {
	case DO_NOT_VERIFY:
		break;
	case VERIFY_ALL:
		printf("%s", path);
		if (userid >= 0 && userid != filestatbuf.st_uid) {
			if (pwd = getpwuid(filestatbuf.st_uid))
				printf(":uid=%s", pwd->pw_name);
			else
				printf(":uid=%d", filestatbuf.st_uid);
			if (pwd = getpwuid(userid))
				printf("(%s)", pwd->pw_name);
			else
				printf("(%d)", userid);
			error++;
		}
		if (groupid >= 0 && groupid != filestatbuf.st_gid) {
			if (grp = getgrgid(filestatbuf.st_gid))
				printf(":gid=%s", grp->gr_name);
			else
				printf(":gid=%d", filestatbuf.st_gid);
			if (grp = getgrgid(groupid))
				printf("(%s)", grp->gr_name);
			else
				printf("(%d)", userid);
			error++;
		}
		if (mode != -1 && mode != (filestatbuf.st_mode &06777)){
			printf(":mode=%04o(%04o)",
			    (filestatbuf.st_mode & 06777), (mode & 06777));
			error++;
		}
		if (cep) {
			if (cp = verify_cap(mte, root, &cep->cap_allowed,
			    &cep->cap_forced, cep->cap_flags,
			    filestatbuf.st_ino)) {
				printf("%s", cp);
				error++;
			}
		}
		if (cap_forced || cap_allowed || cap_seteff != -1) {
			if (cp = verify_cap(mte, root, cap_allowed,
			    cap_forced, cap_seteff, filestatbuf.st_ino)) {
				printf("%s", cp);
				error++;
			}
		}
		if (mac) {
			if (cp = verify_mac(mte, root, mac, filestatbuf.st_ino)) {
				printf("%s", cp);
				error++;
			}
		}
		if (filetype &&
		    filetype != (S_IFMT & filestatbuf.st_mode)) {
			if (S_ISREG(filestatbuf.st_mode))
				printf(":type=file");
			else if (S_ISDIR(filestatbuf.st_mode))
				printf(":type=directory");
			else if (S_ISLNK(filestatbuf.st_mode))
				printf(":type=symlink");
			else if (S_ISFIFO(filestatbuf.st_mode))
				printf(":type=FIFO");
			else if (S_ISSOCK(filestatbuf.st_mode))
				printf(":type=socket");
			else if (S_ISCHR(filestatbuf.st_mode))
				printf(":type=cdev");
			else if (S_ISBLK(filestatbuf.st_mode))
				printf(":type=bdev");
			else
				printf(":type=UNKNOWN");
			printf("(%s)", filetypename);
			error++;
		}
		printf("\n");
		break;
	case VERIFY_EXISTING:
		if (userid >= 0 && userid != filestatbuf.st_uid) {
			printf("%s", path);
			if (pwd = getpwuid(filestatbuf.st_uid))
				printf(":uid=%s", pwd->pw_name);
			else
				printf(":uid=%d", filestatbuf.st_uid);
			if (pwd = getpwuid(userid))
				printf("(%s)", pwd->pw_name);
			else
				printf("(%d)", userid);
			error++;
		}
		if (groupid >= 0 && groupid != filestatbuf.st_gid) {
			if (!error++)
				printf("%s", path);
			if (grp = getgrgid(filestatbuf.st_gid))
				printf(":gid=%s", grp->gr_name);
			else
				printf(":gid=%d", filestatbuf.st_gid);
			if (grp = getgrgid(groupid))
				printf("(%s)", grp->gr_name);
			else
				printf("(%d)", userid);
		}
		if (mode != -1 && mode != (filestatbuf.st_mode &06777)) {
			if (!error++)
				printf("%s", path);
			printf(":mode=%04o(%04o)",
			    (filestatbuf.st_mode & 06777), (mode & 06777));
		}
		if (cep) {
			if (cp = verify_cap(mte, root, &cep->cap_allowed,
			    &cep->cap_forced, cep->cap_flags,
			    filestatbuf.st_ino)) {
				if (!error++)
					printf("%s", path);
				printf("%s", cp);
			}
		}
		if (cap_forced || cap_allowed || cap_seteff != -1) {
			if (cp = verify_cap(mte, root, cap_allowed,
			    cap_forced, cap_seteff, filestatbuf.st_ino)) {
				if (!error++)
					printf("%s", path);
				printf("%s", cp);
			}
		}
		if (mac) {
			if (cp = verify_mac(mte, root,mac,filestatbuf.st_ino)) {
				if (!error++)
					printf("%s", path);
				printf("%s", cp);
			}
		}
		if (filetype && filetype != (S_IFMT & filestatbuf.st_mode)) {
			if (!error++)
				printf("%s", path);
			if (S_ISREG(filestatbuf.st_mode))
				printf(":type=file");
			else if (S_ISDIR(filestatbuf.st_mode))
				printf(":type=directory");
			else if (S_ISLNK(filestatbuf.st_mode))
				printf(":type=symlink");
			else if (S_ISFIFO(filestatbuf.st_mode))
				printf(":type=FIFO");
			else if (S_ISSOCK(filestatbuf.st_mode))
				printf(":type=socket");
			else if (S_ISCHR(filestatbuf.st_mode))
				printf(":type=cdev");
			else if (S_ISBLK(filestatbuf.st_mode))
				printf(":type=bdev");
			else
				printf(":type=UNKNOWN");
			printf("(%s)", filetypename);
		}
		if (error)
			printf("\n");
		break;
	}
	if (edit) {
		if (userid >= 0 || groupid >= 0) {
			if (chown(path, userid, groupid) < 0) {
				fprintf(stderr,
				    "Changing owner/group of ");
				perror(path);
			}
		}
		if (mode != -1) {
			if (chmod(path, mode) < 0) {
				fprintf(stderr, "Changing mode of ");
				perror(path);
			}
		}
		if (cep)
			update_cap(mte, root, &cep->cap_allowed, &cep->cap_forced,
			    cep->cap_flags, filestatbuf.st_ino);
		if (cap_forced || cap_allowed || cap_seteff != -1)
			update_cap(mte, root, cap_allowed, cap_forced,
			    cap_seteff, filestatbuf.st_ino);
		if (mac)
			update_mac(mte, root, mac, filestatbuf.st_ino);
#ifdef	LATER_CBS
		if (inf)
			update_inf(mte, root, inf, filestatbuf.st_ino);
		if (acl || acl_dir)
			update_acl(mte, root,acl,acl_dir,filestatbuf.st_ino);
#endif	/* LATER_CBS */
	}
	return (error);
}

int
doscript(char *script, char *root, int verify, int edit,
	cap_eag_t *m_cep, int m_cap_seteff,
	capability_t *m_cap_forced, capability_t *m_cap_allowed,
	char *filetypename, int filetype,
	uid_t m_userid, gid_t m_groupid, mode_t m_mode,
	mac_label *m_mac)
{
	cap_eag_t *cep;
	int cap_seteff;
	capability_t *cap_forced;
	capability_t *cap_allowed;
	uid_t userid;
	gid_t groupid;
	mode_t mode;
	mac_label *mac;
	int error = 0;
	char *value;
	char *cp;
	char buffer[512];
	FILE *fp;

	if ((fp = fopen(script, "r")) == NULL)
		arg_error("script", script);
	
	while (fgets(buffer, 510, fp)) {
		cep = m_cep;
		cap_seteff = m_cap_seteff;
		cap_forced = m_cap_forced;
		cap_allowed = m_cap_allowed;
		userid = m_userid;
		groupid = m_groupid;
		mode = m_mode;
		mac = m_mac;
		if (cp = strchr(buffer, '\n'))
			*cp = '\0';

		if (!cep && (value = strstr(buffer, "-cap="))) {
			value += strlen("-cap=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if ((cep = sgi_cap_strtocap_eag(value)) == NULL) {
				if (cp)
					*cp = ' ';
				script_error(buffer, value);
				continue;
			}
			if (cp)
				*cp = ' ';
		}
		if (!cap_forced && (value = strstr(buffer, "-cap-forced="))) {
			value += strlen("-cap-forced=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if ((cap_forced = sgi_cap_strtocap(value)) == NULL) {
				if (cp)
					*cp = ' ';
				script_error(buffer, value);
				continue;
			}
			if (cp)
				*cp = ' ';
		}
		if (!cap_allowed && (value = strstr(buffer, "-cap-allowed="))) {
			value += strlen("-cap-allowed=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if ((cap_allowed = sgi_cap_strtocap(value)) == NULL) {
				if (cp)
					*cp = ' ';
				script_error(buffer, value);
				continue;
			}
			if (cp)
				*cp = ' ';
		}
		if (cap_seteff == -1 &&
		    (value = strstr(buffer, "-cap-set-effective="))) {
			value += strlen("-cap-set-effective=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if (strcmp(value, "on") == 0)
				cap_seteff = CAP_SET_EFFECTIVE;
			else
				cap_seteff = 0;
			if (cp)
				*cp = ' ';
		}
		if (!mac && (value = strstr(buffer, "-mac="))) {
			value += strlen("-mac=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if ((mac = mac_strtolabel(value)) == NULL) {
				if (cp)
					*cp = ' ';
				script_error(buffer, value);
				continue;
			}
			if (cp)
				*cp = ' ';
			if (mac_size(mac) > 8) {
				script_error(buffer, value);
				continue;
			}
		}
		if (userid == -1 && (value = strstr(buffer, "-user="))) {
			struct passwd *pwd;
			value += strlen("-user=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if (isdigit(*value)) {
				if (sscanf(value, "%d", &userid) != 1) {
					if (cp)
						*cp = ' ';
					script_error(buffer, value);
					continue;
				}
			}
			else {
				if (!(pwd = getpwnam(value))) {
					if (cp)
						*cp = ' ';
					script_error(buffer, value);
					continue;
				}
				userid = pwd->pw_uid;
			}
			if (cp)
				*cp = ' ';
		}
		if (groupid == -1 && (value = strstr(buffer, "-group="))) {
			struct group *grp;
			value += strlen("-group=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if (isdigit(*value)) {
				if (sscanf(value, "%d", &groupid) != 1) {
					if (cp)
						*cp = ' ';
					script_error(buffer, value);
					continue;
				}
			}
			else {
				if (!(grp = getgrnam(value))) {
					if (cp)
						*cp = ' ';
					script_error(buffer, value);
					continue;
				}
				groupid = grp->gr_gid;
			}
			if (cp)
				*cp = ' ';
		}
		if (mode == -1 && (value = strstr(buffer, "-mode="))) {
			value += strlen("-mode=");
			if (cp = strchr(value, ' '))
				*cp = '\0';
			if (sscanf(value, "%o", &mode) != 1) {
				if (cp)
					*cp = ' ';
				script_error(buffer, value);
				continue;
			}
			if (cp)
				*cp = ' ';
		}
		for (cp = buffer; cp; cp = strchr(cp + 1, ' '))
			value = cp + 1;
		error += doone(value, root, verify, edit, cep, cap_seteff,
		    cap_forced, cap_allowed, filetypename, filetype, userid,
		    groupid, mode, mac);
	}
}

main(int argc, char *argv[])
{
	int i;
	int error = 0;
	int cap_seteff = -1;
	int filetype = 0;
	int verify = DO_NOT_VERIFY;
	int edit = 1;
	uid_t userid = -1;
	gid_t groupid = -1;
	mode_t mode = -1;
	char *filetypename = NULL;
	char *value;
	char *script = NULL;
	char *root = "/";
	mac_label *mac = NULL;
	capability_t *cap_forced = NULL;
	capability_t *cap_allowed = NULL;
	cap_eag_t *cep = NULL;
#ifdef	LATER_CBS
	acl_eag_t *acl_dir = NULL;
	acl_eag_t *acl = NULL;
	inf_label *inf = NULL;
#endif	/* LATER_CBS */

	program = argv[0];

	read_mount_table();

	for (i = 1; i < argc; i++) {
		/*
		 * No '-' means the option list is complete.
		 */
		if (argv[i][0] != '-')
			break;
		if ((value = strchr(argv[i], '=')) == NULL) {
			fprintf(stderr, "%s: no value given for \"%s\".\n",
			    program, argv[i]);
			exit(1);
		}
		*value++ = '\0';

		if (!strcmp(argv[i], "-root")) {
			root = value;
			continue;
		}
		if (!strcmp(argv[i], "-script")) {
			script = value;
			continue;
		}
		if (!strcmp(argv[i],"-verify")) {
			if (!strcmp(value, "all")) {
				verify = VERIFY_ALL;
				edit = 0;
			}
			else if (!strcmp(value, "installed")) {
				verify = VERIFY_EXISTING;
				edit = 0;
			}
			else if (!strcmp(value, "also")) {
				verify = VERIFY_ALL;
				edit = 1;
			}
			else
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i],"-filetype") || !strcmp(argv[i],"-type")) {
			filetypename = value;

			if (!strcmp(value, "d") || !strcmp(value, "directory"))
				filetype = S_IFDIR;

			else if (!strcmp(value, "f") || !strcmp(value, "file"))
				filetype = S_IFREG;

			else if (!strcmp(value, "l") ||
			    !strcmp(value, "link") || !strcmp(value, "symlink"))
				filetype = S_IFLNK;

			else if (!strcmp(value, "p") ||
			    !strcmp(value, "pipe") || !strcmp(value, "fifo"))
				filetype = S_IFIFO;

			else if (!strcmp(value, "c") ||
			    !strcmp(value, "char") || !strcmp(value, "chardev"))
				filetype = S_IFCHR;

			else if (!strcmp(value, "b") ||
			    !strcmp(value, "blk") || !strcmp(value, "block") ||
			    !strcmp(value, "blockdev"))
				filetype = S_IFBLK;

			else if (!strcmp(value, "s") ||
			    !strcmp(value, "socket"))
				filetype = S_IFSOCK;

			else
				arg_error(argv[i], value);
			continue;
		}
		/*
		 * Database types.
		 */
		if (!strcmp(argv[i], "-cap")) {
			if (cap_forced || cap_allowed || (cap_seteff != -1)) {
				fprintf(stderr,
				    "%s: Bad capability specification.\n",
				    program);
				exit(1);
			}
			if ((cep = sgi_cap_strtocap_eag(value)) == NULL) 
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-cap-forced")) {
			if (cep) {
				fprintf(stderr,
				    "%s: Bad capability specification.\n",
				    program);
				exit(1);
			}
			if ((cap_forced = sgi_cap_strtocap(value)) == NULL) 
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-cap-allowed")) {
			if (cep) {
				fprintf(stderr,
				    "%s: Bad capability specification.\n",
				    program);
				exit(1);
			}
			if ((cap_allowed = sgi_cap_strtocap(value)) == NULL) 
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-cap-set-effective")) {
			if (cep) {
				fprintf(stderr,
				    "%s: Bad capability specification.\n",
				    program);
				exit(1);
			}
			if (strcmp(value, "on") == 0)
				cap_seteff = CAP_SET_EFFECTIVE;
			else
				cap_seteff = 0;
			continue;
		}
		if (!strcmp(argv[i], "-mac")) {
			if ((mac = mac_strtolabel(value)) == NULL) 
				arg_error(argv[i], value);
			if (mac_size(mac) > 8) {
				fprintf(stderr, "%s: Label %s too big.\n",
					program, argv[i]);
				exit(1);
			}
			continue;
		}
		if (!strcmp(argv[i], "-mode")) {
			if (sscanf(value, "%o", &mode) != 1)
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-user")) {
			struct passwd *pwd;
			if (isdigit(*value)) {
				if (sscanf(value, "%d", &userid) != 1)
					arg_error(argv[i], value);
			}
			else {
				if (!(pwd = getpwnam(value)))
					arg_error(argv[i], value);
				userid = pwd->pw_uid;
			}
			continue;
		}
		if (!strcmp(argv[i], "-group")) {
			struct group *grp;
			if (isdigit(*value)) {
				if (sscanf(value, "%d", &groupid) != 1)
					arg_error(argv[i], value);
			}
			else {
				if (!(grp = getgrnam(value)))
					arg_error(argv[i], value);
				userid = grp->gr_gid;
			}
			continue;
		}
#ifdef	LATER_CBS
		if (!strcmp(argv[i], "-acl")) {
			if ((acl = sgi_acl_strtoacl(value)) == NULL) 
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-acl-default")) {
			if ((acl_dir = sgi_acl_strtoacl(value)) == NULL) 
				arg_error(argv[i], value);
			continue;
		}
		if (!strcmp(argv[i], "-inf")) {
			if ((inf = inf_strtolabel(value)) == NULL) 
				arg_error(argv[i], value);
			continue;
		}
#endif	/* LATER_CBS */
		/*
		 * Not something we know about.
		 */
		fprintf(stderr, "%s: unknown argument \"%s\".\n",
		    program, argv[i]);
		exit(1);
	}

	if (!cep && !cap_forced && !cap_allowed && cap_seteff == -1 &&
	    userid == -1 && groupid == -1 && mode == -1 &&
#ifdef	LATER_CBS
	    !inf && !acl && !acl_dir &&
#endif	/* LATER_CBS */
	    !mac && !script) {
		fprintf(stderr, "%s: No attributes specified.\n", program);
		exit(1);
	}

	/*
	 * Now loop through the desired paths.
	 */
	for (; i < argc; i++)
		error += doone(argv[i], root, verify, edit,
		    cep, cap_seteff, cap_forced, cap_allowed,
		    filetypename, filetype, userid, groupid, mode, mac);

	/*
	 * Do a script.
	 */
	if (script)
		error += doscript(script, root, verify, edit,
		    cep, cap_seteff, cap_forced, cap_allowed,
		    filetypename, filetype, userid, groupid, mode, mac);

	exit(error);
}
