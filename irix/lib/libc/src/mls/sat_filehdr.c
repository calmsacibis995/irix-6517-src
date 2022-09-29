/*
 * Copyright 1990, 1991, 1992 Silicon Graphics, Inc. 
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

#ifdef __STDC__
	#pragma weak sat_write_filehdr = _sat_write_filehdr
	#pragma weak sat_close_filehdr = _sat_close_filehdr
	#pragma weak sat_read_file_info = _sat_read_file_info
	#pragma weak sat_free_file_info = _sat_free_file_info
	#pragma weak sat_write_file_info = _sat_write_file_info
#endif

#include "synonyms.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sat.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <bstring.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <malloc.h>
#include <sys/sat_compat.h>

static int		errorflag;
static ssize_t		offset;
static int		file_major, file_minor;
static const char	align[4] = { 0, 3, 2, 1 };

#define		my_fflush(fd)	my_fwrite(NULL, 0, 0, fd)
#define		my_ftell(fd)	(lseek(fd,0L,SEEK_CUR) + offset)
static void	my_fwrite(const void *, int, int, int);
static int	process_list(char *, int, struct sat_list_ent ***, int);
static void	sort_list(int, struct sat_list_ent ***, int *);


/*
 * Write a file header.  This predates all the file_info routines, and
 * should probably be rewritten using the malloc/realloc technique used
 * by sat_write_file_info().  I don't remember why I used my own fwrite,
 * but it was a good reason at the time.
 */
int
sat_write_filehdr( int out )
{
	struct sat_filehdr fh;
	struct sat_list_ent le;
#ifdef	BEFORE_VERSION_4
	struct passwd *pw;
	struct group *gr;
#endif	/* BEFORE_VERSION_4 */
	off_t orig_file_pos;
	off_t file_pos;
	char buf[8192];
	int pipe = SAT_FALSE;
	int old_out;
	char *tz;

	if (out < 0)
		return -1;

	errorflag = SAT_FALSE;

	orig_file_pos = lseek(out, 0L, SEEK_CUR);
	if (orig_file_pos < 0) {
		char *tmpfile = strdup("/tmp/satdXXXXXX");

		pipe = SAT_TRUE;
		old_out = out;
		out = mkstemp(tmpfile);
		(void) unlink(tmpfile);
		orig_file_pos = 0L;

		free(tmpfile);
	}

	strncpy( fh.sat_magic, SAT_FILE_MAGIC, sizeof(fh.sat_magic) );
	fh.sat_major = SAT_VERSION_MAJOR;
	fh.sat_minor = SAT_VERSION_MINOR;
	fh.sat_start_time = time(0);
	fh.sat_stop_time = 0L;
	fh.sat_host_id = (sat_host_t)gethostid();
	fh.sat_user_entries = 0;
	fh.sat_group_entries = 0;

	if (sysconf (_SC_MAC) > 0)
		fh.sat_mac_enabled = 1;
	else
		fh.sat_mac_enabled = 0;

	if (sysconf (_SC_CAP) > 0)
		fh.sat_cap_enabled = 1;
	else
		fh.sat_cap_enabled = 0;

	if (sysconf (_SC_IP_SECOPTS) > 0)
		fh.sat_cipso_enabled = 1;
	else
		fh.sat_cipso_enabled = 0;

	my_fwrite( &fh, sizeof(struct sat_filehdr), 1, out);

	tz = getenv("TZ");
	if (tz != NULL) {
		strncpy(buf, tz, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
	} else
		buf[0] = '\0';
	fh.sat_timezone_len = (int)strlen(buf) + 1;
	my_fwrite(buf, fh.sat_timezone_len, 1, out);

	if (gethostname(buf, sizeof(buf)) == -1)
		buf[0] = '\0';
	fh.sat_hostname_len = (int)strlen(buf) + 1;
	my_fwrite(buf, fh.sat_hostname_len, 1, out);

	if (getdomainname(buf, sizeof(buf)) == -1)
		buf[0] = '\0';
	fh.sat_domainname_len = (int)strlen(buf) + 1;
	my_fwrite(buf, fh.sat_domainname_len, 1, out);

	/*
	 * align the following structs on long boundaries
	 * (add the adjustment to domainname_len for convenience)
	 */
	file_pos = my_ftell(out);
	fh.sat_domainname_len += align[file_pos % 4];
	my_fwrite(buf, align[file_pos % 4], 1, out);

#ifdef	BEFORE_VERSION_4
	setpwent();
	while ((pw=getpwent()) != NULL) {
		le.sat_id = pw->pw_uid;
		le.sat_name.len = (int)strlen(pw->pw_name) + 1;
		le.sat_name.len += align[le.sat_name.len % 4];
		my_fwrite(&le, sizeof(le), 1, out);
		my_fwrite(pw->pw_name, le.sat_name.len, 1, out);
		fh.sat_user_entries++;
	}
	endpwent();
#else	/* BEFORE_VERSION_4 */
	le.sat_id = 0;
	le.sat_name.len = (int)strlen("root") + 1;
	le.sat_name.len += align[le.sat_name.len % 4];
	my_fwrite(&le, sizeof(le), 1, out);
	my_fwrite("root", le.sat_name.len, 1, out);
	fh.sat_user_entries++;
#endif	/* BEFORE_VERSION_4 */

#ifdef	BEFORE_VERSION_4
	setgrent();
	while ((gr=getgrent()) != NULL) {
		le.sat_id = gr->gr_gid;
		le.sat_name.len = (int)strlen(gr->gr_name) + 1;
		le.sat_name.len += align[le.sat_name.len % 4];
		my_fwrite(&le, sizeof(le), 1, out);
		my_fwrite(gr->gr_name, le.sat_name.len, 1, out);
		fh.sat_group_entries++;
	}
	endgrent();
#else	/* BEFORE_VERSION_4 */
	le.sat_id = 0;
	le.sat_name.len = (int)strlen("sys") + 1;
	le.sat_name.len += align[le.sat_name.len % 4];
	my_fwrite(&le, sizeof(le), 1, out);
	my_fwrite("sys", le.sat_name.len, 1, out);
	fh.sat_group_entries++;
#endif	/* BEFORE_VERSION_4 */

	/*
	 * put one host on the list: ours
	 */
	if (gethostname(buf, sizeof(buf)) == -1)
		buf[0] = '\0';
	if (strchr(buf, '.') == NULL) {
		char *p = buf + strlen(buf);
		*p = '.';
		if (getdomainname(p+1, sizeof(buf)) == -1 || *(p+1) == '\0') {
			*p = '\0';
		}
	}
	le.sat_id = (uid_t)gethostid();
	le.sat_name.len = (int)strlen(buf) + 1;
	le.sat_name.len += align[le.sat_name.len % 4];
	my_fwrite(&le, sizeof(le), 1, out);
	my_fwrite(buf, le.sat_name.len, 1, out);
	fh.sat_host_entries = 1;

	/*
	 * rewind to the file header and write the updated fields
	 */

	my_fflush(out);

	file_pos = lseek(out, 0L, SEEK_CUR);
	lseek(out, orig_file_pos, SEEK_SET);

	fh.sat_total_bytes = (int)(file_pos - orig_file_pos);
	if (write(out, &fh, sizeof(struct sat_filehdr))
	    != sizeof(struct sat_filehdr))
		errorflag = SAT_TRUE;

	lseek(out, file_pos, SEEK_SET);

	if (errorflag) {
		lseek(out, orig_file_pos, SEEK_SET);
		if (pipe)
			close(out);
		return -1;
	}

	if (pipe) {
		/*
		 * copy temp file header to pipe
		 */
		lseek(out, 0L, SEEK_SET);
		for (;;) {
			ssize_t n;
			n = read(out, buf, sizeof(buf));
			write(old_out, buf, n);
			if (n < sizeof(buf))
				break;
		}
		close(out);
	}

	return 0;
}


/*
 * Close the file, and write out the close time in the file header.
 */
int
sat_close_filehdr( int fd )
{
	struct sat_filehdr fh;
	off_t file_pos;

	if (fd < 0)
		return -1;

	file_pos = lseek(fd, 0L, SEEK_CUR);
	if (lseek(fd, 0L, SEEK_SET) < 0)
		return -1;

	if (read(fd, &fh, sizeof(struct sat_filehdr))
	    != sizeof(struct sat_filehdr))
		return -1;

	if (strncmp(fh.sat_magic, SAT_FILE_MAGIC, sizeof(fh.sat_magic)) != 0)
		return -1;

	fh.sat_stop_time = time(0);

	lseek(fd, 0L, SEEK_SET);
	if (write(fd, &fh, sizeof(struct sat_filehdr))
	    != sizeof(struct sat_filehdr))
		return -1;

	lseek(fd, file_pos, SEEK_SET);

	return 0;
}


#define MIN(a,b) (((a)<(b))?(a):(b))

/* move out of function scope so we get a global symbol for use with data cording */
static char *iobuf;

/* ARGSUSED3 */
static void
my_fwrite( const void *buf, int buflen, int notused, int fd )
{
	int minlen;

	if (iobuf == NULL) {
		iobuf = malloc(0x2000);
		if (iobuf == NULL)
			return;
	}

	if (buf == NULL) {	/* flush buffer */
		if (write(fd, iobuf, offset) != offset)
			errorflag = SAT_TRUE;
		offset = 0;
		return;
	}

	while (buflen > 0) {
		minlen = MIN( buflen, (int)sizeof(iobuf) - (int)offset );
		bcopy((char *)buf, (char *)(iobuf + offset), minlen);
		buf = (char *)buf + minlen;
		buflen -= minlen;
		offset += minlen;
		if (buflen > 0) {
			if (write(fd, iobuf, offset) != offset)
				errorflag = SAT_TRUE;
			offset = 0;
		}
	}
}


/*
 * Read a file header.  Something this complex belongs where it is: in a
 * library.
 */
int
sat_read_file_info(FILE *in, FILE *out, struct sat_file_info *finfo, int mask)
{
	int c;
	long filepos;			/* starting file pos */
	int struct_size;		/* size of this vers of file_hdr */
	int firstread;			/* amount read to get major/minor */
	char *origbuf = NULL, *buf;	/* free origbuf when done */
	int tz_len, host_len, dom_len;	/* info from file header */
	int total_bytes;		/* total size of file header data */

	filepos = ftell(in);

	/* initialize defaults */
	finfo->sat_major = SAT_VERSION_MAJOR;
	finfo->sat_minor = SAT_VERSION_MINOR;
	finfo->sat_timezone = NULL;
	finfo->sat_hostname = NULL;
	finfo->sat_domainname = NULL;
	finfo->sat_buffer = NULL;
	finfo->sat_users = NULL;
	finfo->sat_groups = NULL;
	finfo->sat_hosts = NULL;

	c = getc(in);		/* sample the first character */
	ungetc(c, in);

	if (c == SAT_FILE_MAGIC[0]) {
		origbuf = malloc(1024);
		if (! origbuf)
			return SFI_ERROR;
		firstread = sizeof(struct sat_filehdr);
		if (sizeof(struct sat_filehdr_1_0) > firstread)
			firstread = sizeof(struct sat_filehdr_1_0);
		fread(origbuf, firstread, 1, in);

		if (ferror(in)) {
			perror("Error reading file header");
			fseek(in, filepos, SEEK_SET);
			free(origbuf);
			return SFI_ERROR;
		}
	} else {
		fprintf(stderr, "Warning: missing or bad file header!\n");
		return (feof(in)? SFI_ERROR: SFI_WARNING);
	}

	if (strncmp(origbuf, SAT_FILE_MAGIC, sizeof(SAT_FILE_MAGIC)-1)) {
		fprintf(stderr, "Warning: missing or bad file header!\n");
		fseek(in, filepos, SEEK_SET);
		free(origbuf);
		return (feof(in)? SFI_ERROR: SFI_WARNING);
	}

	/* 
	 * Pretend origbuf is a sat_filehdr long enough to get the
	 * version numbers, which will allow us to decode it.
	 */
	file_major = ((struct sat_filehdr *)origbuf)->sat_major;
	file_minor = ((struct sat_filehdr *)origbuf)->sat_minor;

	/*
	 * copy the appropriate fields into a sat_file_info struct
	 */
        if (file_major == 1 && file_minor == 0) {
		struct sat_filehdr_1_0 *fh_1_0;
		fh_1_0 = (struct sat_filehdr_1_0 *)origbuf;
		struct_size = sizeof(struct sat_filehdr_1_0);

		finfo->sat_major = fh_1_0->sat_major;
		finfo->sat_minor = fh_1_0->sat_minor;
		finfo->sat_start_time = fh_1_0->sat_start_time;
		finfo->sat_stop_time = fh_1_0->sat_stop_time;
		finfo->sat_host_id = fh_1_0->sat_host_id;
		finfo->sat_mac_enabled = SAT_TRUE;
		finfo->sat_cap_enabled = SAT_FALSE;
		finfo->sat_cipso_enabled = SAT_FALSE;
		finfo->sat_user_entries = fh_1_0->sat_user_entries;
		finfo->sat_group_entries = fh_1_0->sat_group_entries;
		finfo->sat_host_entries = 0;
		tz_len = fh_1_0->sat_timezone_len;
		host_len = fh_1_0->sat_hostname_len;
		dom_len = fh_1_0->sat_domainname_len;
		total_bytes = fh_1_0->sat_total_bytes;
        } else if ((file_major >= 2 && file_major <= 4) ||
                   (file_major == 1 && file_minor > 0 && file_minor <= 2)) {
		struct sat_filehdr *fh;
		fh = (struct sat_filehdr *)origbuf;
		struct_size = sizeof(struct sat_filehdr);

		finfo->sat_major = fh->sat_major;
		finfo->sat_minor = fh->sat_minor;
		finfo->sat_start_time = fh->sat_start_time;
		finfo->sat_stop_time = fh->sat_stop_time;
		finfo->sat_host_id = fh->sat_host_id;
		finfo->sat_mac_enabled = fh->sat_mac_enabled;
		finfo->sat_cap_enabled = fh->sat_cap_enabled;
		finfo->sat_cipso_enabled = fh->sat_cipso_enabled;
		finfo->sat_user_entries = fh->sat_user_entries;
		finfo->sat_group_entries = fh->sat_group_entries;
		finfo->sat_host_entries = fh->sat_host_entries;
		tz_len = fh->sat_timezone_len;
		host_len = fh->sat_hostname_len;
		dom_len = fh->sat_domainname_len;
		total_bytes = fh->sat_total_bytes;
        } else {
                fprintf(stderr,
                    "Error: Don't know how to decode version %d.%d!\n",
                    file_major, file_minor);
		free(origbuf);
                return SFI_ERROR;
	}

	/* read in the rest of the header in one operation */

	origbuf = realloc(origbuf, total_bytes);
	if (origbuf == NULL)
		return SFI_ERROR;
	fread(origbuf+firstread, total_bytes - firstread, 1, in);
	if (ferror(in)) {
		perror("Error reading file header");
		fseek(in, filepos, SEEK_SET);
		free(origbuf);
		return SFI_ERROR;
	}
	if (mask & SFI_BUFFER) {
		finfo->sat_buffer = origbuf;
		finfo->sat_fhdrsize = total_bytes;
	}

	/* point past the header struct to the var length stuff*/

	buf = origbuf + struct_size;

	/*
	 * write out the file header, if requested.
	 */
	if (out) {
		fwrite(origbuf, total_bytes, 1, out);
	}

	/* read timezone */

	if (mask & SFI_TIMEZONE) {
		finfo->sat_timezone = malloc(tz_len + 3);
		if (finfo->sat_timezone) {
			strcpy(finfo->sat_timezone, "TZ=");
			strcat(finfo->sat_timezone, buf);
		}
	}
	buf += tz_len;

	if (mask & SFI_HOSTNAME)
		finfo->sat_hostname = strdup(buf);
	buf += host_len;

	if (mask & SFI_DOMNAME)
		finfo->sat_domainname = strdup(buf);
	buf += dom_len;

	/* get a list of users from buf */

	buf += process_list(buf, (mask & SFI_USERS), &(finfo->sat_users),
		finfo->sat_user_entries);
	buf += process_list(buf, (mask & SFI_GROUPS), &(finfo->sat_groups),
		finfo->sat_group_entries);
	buf += process_list(buf, (mask & SFI_HOSTS), &(finfo->sat_hosts),
		finfo->sat_host_entries);

	/* sort lists, remove dups, and adjust header counts */

	sort_list((mask & SFI_USERS), &(finfo->sat_users),
		&(finfo->sat_user_entries));
	sort_list((mask & SFI_GROUPS), &(finfo->sat_groups),
		&(finfo->sat_group_entries));
	sort_list((mask & SFI_HOSTS), &(finfo->sat_hosts),
		&(finfo->sat_host_entries));

	if (! (mask & SFI_BUFFER))
		free(origbuf);
	return SFI_OKAY;
}


/*
 * Free a file header.
 */
void
sat_free_file_info(struct sat_file_info *finfo)
{
	if (finfo->sat_timezone != NULL) {
		free(finfo->sat_timezone);
	}
	if (finfo->sat_hostname != NULL) {
		free(finfo->sat_hostname);
	}
	if (finfo->sat_domainname != NULL) {
		free(finfo->sat_domainname);
	}
	if (finfo->sat_buffer != NULL) {
		free(finfo->sat_buffer);
	}
	if (finfo->sat_users != NULL) {
		struct sat_list_ent **le = finfo->sat_users;
		while (*le != NULL) {
			free(*le);
			le++;
		}
		free(finfo->sat_users);
	}
	if (finfo->sat_groups != NULL) {
		struct sat_list_ent **le = finfo->sat_groups;
		while (*le != NULL) {
			free(*le);
			le++;
		}
		free(finfo->sat_groups);
	}
	if (finfo->sat_hosts != NULL) {
		struct sat_list_ent **le = finfo->sat_hosts;
		while (*le != NULL) {
			free(*le);
			le++;
		}
		free(finfo->sat_hosts);
	}
	bzero(finfo, sizeof(struct sat_file_info));
}


/*
 * Read a list of user (group) uid (gid) / user (group) names from
 * the file header.  The memory format is a null-terminated list of
 * sat_list_ent structs.
 *
 * Return the number of bytes to skip to next list.
 */
static int
process_list(char *buf, int want_it, struct sat_list_ent **listp[],
	     int n_entries)
{
	int i;
	int id;
	int namelen;
	char *origbuf = buf;

	/* allocate and null-terminate the list */
	if (want_it) {
		(*listp) = (void *) malloc(sizeof(void *) * (n_entries + 1));
		if (*listp)
			(*listp)[n_entries] = NULL;
		else
			want_it = 0;
	}

	for (i=0; i < n_entries; i++) {
		if (file_major == 1 && file_minor == 0) {
			struct sat_user_ent_1_0 *ue_1_0;
			ue_1_0 = (struct sat_user_ent_1_0 *)buf;
			buf += sizeof(struct sat_user_ent_1_0);
			id = ue_1_0->sat_uid;
			namelen = ue_1_0->sat_namelen;
	        } else if ((file_major == 3) ||
	        	   (file_major == 2) ||
			(file_major == 1 && file_minor > 0 && file_minor <= 2)){
			struct sat_list_ent *tmpue;
			tmpue = (struct sat_list_ent *)buf;
			buf += sizeof(struct sat_list_ent);
			id = tmpue->sat_id;
			namelen = tmpue->sat_name.len;
		} else {
			want_it = 0;
		}
		if (want_it) {
			struct sat_list_ent *ue_p;
			ue_p = (struct sat_list_ent *)
				malloc(sizeof(struct sat_list_ent) + namelen);
			if (!ue_p)
				want_it = 0;
			else {
				ue_p->sat_id = id;
				bcopy(buf, ue_p->sat_name.data, namelen);
			}
			(*listp)[i] = ue_p;
		}
		buf += namelen;
	}
	return (int)(buf - origbuf);
}


static int ue_compare(const void *va, const void *vb)
{
	struct sat_list_ent **a = (struct sat_list_ent **)va;
	struct sat_list_ent **b = (struct sat_list_ent **)vb;

	if ((*a) == NULL)
		return 1;
	if ((*b) == NULL)
		return -1;
	return ((*a)->sat_id - (*b)->sat_id);
}


/*
 * Sort the list we read in read_list, remove duplicates, and re-count
 * the number of entries.
 */
static void
sort_list(int want_it, struct sat_list_ent **listp[], int *n_entp)
{
	int i, j, n;
	int n_entries = (*n_entp);
	struct sat_list_ent **orig_list = (*listp);
	struct sat_list_ent **list;
	struct sat_list_ent **final_list;

	/*
	 * If they don't want the list, we say there are zero entries.
	 */
	if (! want_it) {
		(*n_entp) = 0;
		return;
	}

	/* special case */
	if (n_entries == 1) {
		return;
	}

	list = (struct sat_list_ent **)
		 malloc(sizeof(void *) * (n_entries+1));

	final_list = (struct sat_list_ent **)
		 malloc(sizeof(void *) * (n_entries+1));

	if (! list || ! final_list || ! orig_list) {
		(*n_entp) = 0;
		return;
	}

	/* make a null-terminated copy of the list */
	for (i=0; orig_list[i]; i++)
		list[i] = orig_list[i];
	list[i] = NULL;

	/* sort the copy */
	qsort(list, n_entries, sizeof(void *), ue_compare);

	/*
	 * Find duplicate entries.  Move the entry that was first in the
	 * original order to the final list.  Move all non-duplicates too.
	 */
	for (i=0, n=0; list[i]; i++) {
		if (list[i+1] &&
		    list[i]->sat_id == list[i+1]->sat_id) {
			for (j=0; orig_list[j]; j++)
				if (orig_list[j]->sat_id == list[i]->sat_id)
					break;
			while (list[i] &&
			       list[i]->sat_id == orig_list[j]->sat_id) {
				if (strcmp(orig_list[j]->sat_name.data,
					   list[i]->sat_name.data) == 0) {
					final_list[n++] = list[i];
					list[i] = NULL;
				}
				i++;
			}
			i--;
		} else {
			final_list[n++] = list[i];
			list[i] = NULL;
		}
	}
	*n_entp = n;
	final_list[n++] = NULL;

	/* clean up duplicates */
	for (i=0; i < n_entries; i++)
		if (list[i])
			free(list[i]);
	free(list);
	free(orig_list);

	*listp = final_list;
}


/*
 * Write a sat_file_info back to disk.  Ignore any contents of sat_buffer,
 * since if that was valid, they wouldn't call this routine.  If they do
 * anyway, it should just take a little longer.
 */
int
sat_write_file_info(FILE *out, struct sat_file_info *finfo)
{
	struct sat_filehdr *fh;
	struct sat_list_ent le;		/* disk version of sat_list_ent */
	struct sat_list_ent **mement;	/* in-memory sat_list_ent */
	long orig_file_pos;
	char *membuf;
	int membufsize;
	char *buf;

#	define MALLCHUNK 4000
#	define safecopy(membuf, source, len)			\
	{							\
		if (membuf-(char *)fh+len >= membufsize) {	\
			char *oldfh = (char *)fh;		\
			membufsize += MALLCHUNK;		\
			fh = realloc(fh, membufsize);		\
			if (fh == NULL)				\
				return SFI_ERROR;		\
			membuf = (char *)fh + (membuf - oldfh);	\
		}						\
		bcopy((char *)source, membuf, len);		\
		membuf += len;					\
	}

	if (out == NULL)
		return SFI_ERROR;

	/* "current" file header only -or- version 2*/

        if (!(finfo->sat_major == 2 && SAT_VERSION_MAJOR == 2) ||
	      (finfo->sat_major == SAT_VERSION_MAJOR &&
	       finfo->sat_minor == SAT_VERSION_MINOR)) {
		return SFI_ERROR;
	}

	orig_file_pos = ftell(out);

	if (finfo->sat_users == NULL || finfo->sat_groups == NULL ||
	    finfo->sat_hosts == NULL) {
		return SFI_ERROR;
	}

	membufsize = sizeof(struct sat_filehdr) + MALLCHUNK;
	fh = malloc(membufsize);
	bzero(fh, sizeof(struct sat_filehdr));
	membuf = (char *)fh + sizeof(struct sat_filehdr);

	strncpy( fh->sat_magic, SAT_FILE_MAGIC, sizeof(fh->sat_magic) );
	fh->sat_major = finfo->sat_major; /* SAT_VERSION_MAJOR; */
	fh->sat_minor = finfo->sat_minor; /* SAT_VERSION_MINOR; */
	fh->sat_start_time = finfo->sat_start_time;
	fh->sat_stop_time = finfo->sat_stop_time;
	fh->sat_host_id = finfo->sat_host_id;
	fh->sat_mac_enabled = finfo->sat_mac_enabled;
	fh->sat_cap_enabled = finfo->sat_cap_enabled;
	fh->sat_cipso_enabled = finfo->sat_cipso_enabled;
	fh->sat_user_entries = finfo->sat_user_entries;
	fh->sat_group_entries = finfo->sat_group_entries;
	fh->sat_host_entries = finfo->sat_host_entries;

	buf = (finfo->sat_timezone == NULL)? "": finfo->sat_timezone;
	fh->sat_timezone_len = (int)strlen(buf) + 1;
	safecopy(membuf, buf, fh->sat_timezone_len);

	buf = (finfo->sat_hostname == NULL)? "": finfo->sat_hostname;
	fh->sat_hostname_len = (int)strlen(buf) + 1;
	safecopy(membuf, buf, fh->sat_hostname_len);

	buf = (finfo->sat_domainname == NULL)? "": finfo->sat_domainname;
	fh->sat_domainname_len = (int)strlen(buf) + 1;
	safecopy(membuf, buf, fh->sat_domainname_len);

	/*
	 * align the following structs on long boundaries
	 * (add the adjustment to domainname_len for convenience)
	 */
	fh->sat_domainname_len += align[(membuf - (char *)fh) % 4];
	membuf += align[(membuf - (char *)fh) % 4];

	mement = finfo->sat_users;
	while (*mement != NULL) {
		le.sat_id = (*mement)->sat_id;
		le.sat_name.len = (int)strlen((*mement)->sat_name.data) + 1;
		le.sat_name.len += align[le.sat_name.len % 4];
		safecopy(membuf, &le, sizeof(le));
		safecopy(membuf, (*mement)->sat_name.data, le.sat_name.len);
		mement++;
	}

	mement = finfo->sat_groups;
	while (*mement != NULL) {
		le.sat_id = (*mement)->sat_id;
		le.sat_name.len = (int)strlen((*mement)->sat_name.data) + 1;
		le.sat_name.len += align[le.sat_name.len % 4];
		safecopy(membuf, &le, sizeof(le));
		safecopy(membuf, (*mement)->sat_name.data, le.sat_name.len);
		mement++;
	}

	mement = finfo->sat_hosts;
	while (*mement != NULL) {
		le.sat_id = (*mement)->sat_id;
		le.sat_name.len = (int)strlen((*mement)->sat_name.data) + 1;
		le.sat_name.len += align[le.sat_name.len % 4];
		safecopy(membuf, &le, sizeof(le));
		safecopy(membuf, (*mement)->sat_name.data, le.sat_name.len);
		mement++;
	}

	fh->sat_total_bytes = (int)(membuf - (char *)fh);

	/* write the whole thing out */
	fwrite(fh, fh->sat_total_bytes, 1, out);

	free(fh);

	if (ferror(out)) {
		fseek(out, orig_file_pos, SEEK_SET);
		return SFI_ERROR;
	}

	return SFI_OKAY;
}
