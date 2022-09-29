/*
** This file contains routines to manage the name service "file"
** structures.  A file represents a name service request, and
** a directory represents the domain, map, or callout.  The directory
** tree is created by parsing the nsswitch.conf file, the files are
** dynamically created for each new request, and are then filled in
** by walking the file structure through each of the appropriate
** callouts.  The file structures are cached in memory for a short
** time, and are then thrown out as we look up other files.  Files
** exist in a linked list tree starting at __nsd_mounts, and a btree
** indexed by the low-order 64 bits of the filehandle starting at
** __nsd_files.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <alloca.h>
#include <assert.h>
#include <alloca.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "nfs.h"

extern nsd_file_t *__nsd_mounts;
int __nsd_count = 0;
extern nsd_attr_t *__nsd_attrs;
extern nsd_callout_proc *nis_inlist;

/*
** The init routine just allocates space for a new file structure and
** fills in the defaults.
*/
int
nsd_file_init(nsd_file_t **file, char *name, int len, nsd_file_t *parent,
    int type, nsd_cred_t *cp)
{
	nsd_file_t *fp;

	/* 
	** fireall
	*/
	if (! name || len <= 0) {
		nsd_logprintf(NSD_LOG_MIN,
		    "nsd_file_init passwd bad name/len\n");
		return NSD_ERROR;
	}

	fp = (nsd_file_t *)nsd_calloc(1, sizeof(nsd_file_t) + len + 1);
	if (! fp) {
		nsd_logprintf(NSD_LOG_OPER, "nsd_file_init: failed malloc\n");
		return NSD_ERROR;
	}

	/*
	** If a parent is specified then we "inherit" information from
	** above.
	*/
	if (parent) {
		fp->f_mode = parent->f_mode;
		fp->f_uid = parent->f_uid;
		fp->f_gid = parent->f_gid;
		fp->f_map = parent->f_map;
		memcpy(fp->f_control, parent->f_control, sizeof(fp->f_control));
		nsd_attr_continue(&fp->f_attrs, parent->f_attrs);
	} else {
		nsd_attr_continue(&fp->f_attrs, __nsd_attrs);
	}

	fp->f_name = (char *)(fp + 1);
	memcpy(fp->f_name, name, len);
	fp->f_namelen = len;

	/*
	** Add a link to this file within the btree.
	*/
	if (parent) {
		fp->f_fh[1] = parent->f_fh[0];
	}
	fp->f_fh[0] = __nsd_count++;
	nsd_binsert((uint32_t)fp->f_fh[0], (void *)fp);

	gettimeofday(&fp->f_ctime);
	fp->f_atime.tv_sec = fp->f_ctime.tv_sec;
	fp->f_atime.tv_usec = fp->f_ctime.tv_usec;
	fp->f_mtime.tv_sec = fp->f_ctime.tv_sec;
	fp->f_mtime.tv_usec = fp->f_ctime.tv_usec;

	/*
	** Set file type and add initial entries on directories.
	*/
	fp->f_type = type;
	if (type == NFDIR) {
		nsd_file_appenddir(fp, fp, ".", 1);
		if (parent) {
			nsd_file_appenddir(fp, parent, "..", 2);
		}
	}

	/*
	** If we were given a credential then we add a reference to it.
	*/
	if (cp) {
		fp->f_cred = cp;
		CRED_REF(cp);
	}

	*file = fp;
	return NSD_OK;
}

/*
** This just looks up a file in the hash table using the file id.
*/
nsd_file_t *
nsd_file_byid(uint32_t id)
{
	if (! id) {
		return __nsd_mounts;
	}
	return nsd_bget(id);
}

/*
** The clear routine will remove a file structure and recursively clear
** any files it references.
*/
void
nsd_file_clear(nsd_file_t **filep)
{
	nsd_file_t *member, *file;
	uint32_t *p;
	u_char *q;

	if (! filep || ! *filep) {
		return;
	}
	file = *filep;

	if ((--file->f_nlink) <= 0) {
		if (file->f_callouts) {
			for (q = (u_char *)file->f_callouts; *q;
			    q = (u_char *)(p + 1)) {
				p = (uint32_t *)q;
				p += WORDS(*q + 1);

				/* Don't recourse on ourselves. */
				if ((*p != FILEID(file->f_fh)) &&
				    ((*q != 1) || (q[1] != '.')) &&
				    ((*q != 2) || (q[1] != '.') ||
				    (q[2] != '.'))) {
					member = nsd_file_byid(*p);
					assert (member);
					nsd_file_clear(&member);
				}
			}

			free(file->f_callouts);
		}
		if (file->f_data) {
			if (file->f_type == NFDIR) {
				for (q = (u_char *)file->f_data; *q;
				    q = (u_char *)(p + 1)) {
					p = (uint32_t *)q;
					p += WORDS(*q + 1);

					/* Don't recourse on ourselves. */
					if ((*p != FILEID(file->f_fh)) &&
					    ((*q != 1) || (q[1] != '.')) &&
					    ((*q != 2) || (q[1] != '.') ||
					    (q[2] != '.'))) {
						member = nsd_file_byid(*p);
						assert (member);
						if (member->f_type != NFINPROG)
							nsd_file_clear(&member);
					}
				}
			}

			if (file->f_free) {
				(*file->f_free)(file->f_data);
			}
		}

		if (file->f_attrs) {
			nsd_attr_clear(file->f_attrs);
		}

		if (file->f_cred) {
			nsd_cred_clear(&file->f_cred);
		}

		nsd_bdelete(FILEID(file->f_fh));

		nsd_timeout_remove(file);

		free(file);
		*filep = 0;
	}
}

/*
** This routine just walks down the given directory and removes every
** entry.
*/
void
nsd_file_rmdir(char *dir)
{
	u_char *p;
	uint32_t *q;
	nsd_file_t *member;

	if (p = (u_char *)dir) {
		for (; *p; p = (u_char *)(q + 1)) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			member = nsd_file_byid(*q);
			if (member) {
				nsd_file_clear(&member);
			}
		}

		*(u_char *)dir = (u_char)0;
	}
}

/*
** The delete function removes the file matching the given handle from
** this directory.
*/
int
nsd_file_delete(nsd_file_t *dir, nsd_file_t *file)
{
	u_char *p;
	uint32_t *q;

	if (p = (u_char *)dir->f_data) {
		for (; *p; p = (u_char *)(q + 1)) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			if (*q == FILEID(file->f_fh)) {
				nsd_file_clear(&file);
				memcpy(p, ++q, (dir->f_used - ((u_char *)q -
				    (u_char *)dir->f_data)));
				dir->f_used -= ((u_char *)q - p);
				gettimeofday(&dir->f_mtime);
				return NSD_OK;
			}
		}
	}

	return NSD_ERROR;
}

/*
** The filebyname function locates a file in a particular directory
** by comparing names.  We delete old entries as we search the list.
** Because we can theoretically have multiple entries with the same
** name, we use the attribute list to further delineate the request.
** For instance if we have "hosts: nis(domain=engr) nis(domain=corp)"
** and we wanted to look up the key via the second we would have to
** qualify the name like ../nis(domain=engr)/key.
*/
nsd_file_t *
nsd_file_byname(char *dir, char *name, int len)
{
	nsd_file_t *result;
	u_char *p;
	uint32_t *q;

	if (! dir || ! name) {
		return (nsd_file_t *)0;
	}

	for (p = (u_char *)dir; *p; p = (u_char *)(q + 1)) {
		q = (uint32_t *)p;
		q += WORDS(*p + 1);
		if ((len == *p) && (memcmp(p + 1, name, len) == 0)) {
			result = nsd_file_byid(*q);
			return result;
		}
	}

	return (nsd_file_t *)0;
}

/*
** This routine searches the hash table for the file using the specified
** file handle.  All we really care about for the hash is the file id
** which is stored in the next to last long.
*/
nsd_file_t *
nsd_file_byhandle(uint64_t *handle)
{
	if (! (uint32_t)handle[0]) {
		return __nsd_mounts;
	}
	return nsd_bget((uint32_t)handle[0]);
}

/*
** This routine assumes a directory tree of /domain/table/callout?/key
** and returns a pointer to the last element of the tree specified on
** the command line.  If we return NSD_ERROR, then the result will be
** set to the last entry that we got to.
*/
int
nsd_file_find(char *domain, char *table, char *callout, char *key,
    nsd_file_t **result)
{
	nsd_file_t *fp;

	*result = (nsd_file_t *)0;

	if (! domain || ! __nsd_mounts) {
		return NSD_ERROR;
	}
	*result = nsd_file_byname(__nsd_mounts->f_dir, domain, strlen(domain));
	if (! *result) {
		return NSD_ERROR;
	}
	if (! table) {
		return NSD_OK;
	}
	fp = nsd_file_byname((*result)->f_dir, table, strlen(table));
	if (! fp) {
		return NSD_ERROR;
	}
	*result = fp;
	if (! callout && ! key) {
		return NSD_OK;
	}
	if (callout) {
		fp = nsd_file_byname(fp->f_callouts, callout, strlen(callout));
		if (! fp) {
			return NSD_ERROR;
		}
		*result = fp;
		if (! key) {
			return NSD_OK;
		}
	}
	fp = nsd_file_byname(fp->f_dir, key, strlen(key));
	if (! fp) {
		return NSD_ERROR;
	}
	*result = fp;
	return NSD_OK;
}

/*
** This routine will recursively walk the tree freeing everything with a
** non-infinite timeout.  We assume all children of directories with a
** timeout can be thrown away at the same time.
*/
void
nsd_file_timeout(nsd_file_t **head, time_t to)
{
	nsd_file_t *current;
	u_char *p;
	char *end;
	uint32_t *q;
	int len, changed=0;

	if (! head) {
		head = &__nsd_mounts;
	}

	if ((! to || ((*head)->f_timeout < to)) &&
	    ((*head)->f_type != NFINPROG) && ((*head)->f_timeout != 0)) {
		nsd_logprintf(NSD_LOG_MIN,
		    "timing out %s, fileid: %u refs: %d\n",
		    (*head)->f_name, FILEID((*head)->f_fh), (*head)->f_nlink);
		nsd_file_clear(head);
	} else if ((*head)->f_type == NFDIR) {
		/*
		** Walk the directory timing out whatever we find.
		*/
		p = (u_char *)(*head)->f_data;
		while (p && *p) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			if (((p[0] == 1) && (p[1] == '.')) || ((p[0] == 2) &&
			    (p[1] == '.') && (p[2] == '.'))) {
				/* we skip . and .. */
				p = (u_char *)(q + 1);
				continue;
			}
			current = nsd_file_byid(*q);
			if (current) {
				nsd_file_timeout(&current, to);
			}
			if (current) {
				p = (u_char *)(q + 1);
			} else {
				/* shift down */
				end = (*head)->f_data + (*head)->f_used;
				len = end - (char *)(q + 1);
				(*head)->f_used -= ((u_char *)(q + 1) - p);
				memcpy(p, q + 1, len);
				changed = 1;
			}
		}

		/*
		** Now we have to do the same in the callout direcory.
		*/
		p = (u_char *)(*head)->f_callouts;
		while (p && *p) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			if (((p[0] == 1) && (p[1] == '.')) || ((p[0] == 2) &&
			    (p[1] == '.') && (p[2] == '.'))) {
				/* we skip . and .. */
				p = (u_char *)(q + 1);
				continue;
			}
			current = nsd_file_byid(*q);
			if (current) {
				nsd_file_timeout(&current, to);
			}
			if (current) {
				p = (u_char *)(q + 1);
			} else {
				/* shift down */
				end = (*head)->f_callouts + (*head)->f_c_used;
				len = end - (char *)(q + 1);
				(*head)->f_c_used -= ((u_char *)(q + 1) - p);
				memcpy(p, q + 1, len);
				changed = 1;
			}
		}

		if (changed) {
			gettimeofday(&(*head)->f_mtime);
		}
	}
}

/*
** This routine will walk the tree printing out names similar to ls -R.
*/
void
nsd_file_list(nsd_file_t *head, FILE *fp, int level)
{
	nsd_file_t *current;
	u_char *p;
	uint32_t *q;
	nsd_attr_t *ap;

	if (! head) {
		head = __nsd_mounts;
		fprintf(fp, "global attributes\n");
		for (ap = __nsd_attrs; ap; ap = ap->a_next) {
			fprintf(fp, "%s = %s\n", ap->a_key, ap->a_value);
		}
	}
	fprintf(fp, "%*s%llu: %s refs: %d\n", level, "", head->f_fh[0],
	    head->f_name, head->f_nlink);

	for (ap = head->f_attrs; ap && ap->a_key; ap = ap->a_next) {
		fprintf(fp, "%*s%s = %s\n", level, "", ap->a_key, ap->a_value);
	}

	p = (u_char *)head->f_callouts;
	while (p && *p) {
		q = (uint32_t *)p;
		q += WORDS(*p + 1);
		if (((p[0] == 1) && (p[1] == '.')) || ((p[0] == 2) &&
		    (p[1] == '.') && (p[2] == '.'))) {
			/* we skip . and .. */
			p = (u_char *)(q + 1);
			continue;
		}
		current = nsd_file_byid(*q);
		if (current) {
			nsd_file_list(current, fp, level + 1);
			p = (u_char *)(q + 1);
		} else {
			fprintf(fp, "**could not find: %*.*s\n",
			    *p, *p, p + 1);
		}
	}

	if (head->f_type == NFDIR) {
		p = (u_char *)head->f_data;
		while (p && *p) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			if (((p[0] == 1) && (p[1] == '.')) || ((p[0] == 2) &&
			    (p[1] == '.') && (p[2] == '.'))) {
				/* we skip . and .. */
				p = (u_char *)(q + 1);
				continue;
			}
			current = nsd_file_byid(*q);
			if (current) {
				nsd_file_list(current, fp, level + 1);
				p = (u_char *)(q + 1);
			} else {
				fprintf(fp, "**could not find: %*.*s\n",
				    *p, *p, p + 1);
			}
		}
	}
}

/*
** This routine will append a new directory entry to a directory.
*/
int
nsd_file_appenddir(nsd_file_t *dp, nsd_file_t *fp, char *name, int len) {
	u_char *p;
	uint32_t *q;
	char buf[260];
	int total;
	char *np;

	if (!dp || ! fp || ! name || (len <= 0) || (len > 255)) {
		nsd_logprintf(NSD_LOG_MIN, "nsd_file_appenddir: bad argument\n");
		return NSD_ERROR;
	}

	/*
	** Check to make sure this name is unique and find end.
	*/
	if (p = (u_char *)dp->f_data) {
		for (; *p; p = (u_char *)(q + 1)) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			if ((*p == len) && (memcmp(p + 1, name, len) == 0)) {
				nsd_logprintf(NSD_LOG_LOW,
				 "nsd_file_appenddir: name exists: %s\n", name);
				return NSD_ERROR;
			}
		}
	}

	/*
	** Build directory entry.
	*/
	*(u_char *)buf = len;
	memcpy(buf + 1, name, len);
	q = (uint32_t *)buf;
	q += WORDS(*(u_char *)buf + 1);
	*q++ = FILEID(fp->f_fh);
	total = (char *)q - buf;
	
	/*
	** Realloc buffer if needed.
	*/
	if (! dp->f_data) {
		dp->f_size = dp->f_used = 0;
	}
	if ((dp->f_size - dp->f_used) < total) {
		np = nsd_malloc(dp->f_size + total + 1);
		if (! np) {
			nsd_logprintf(NSD_LOG_OPER, 
			    "nsd_file_appenddir: failed malloc\n");
			return NSD_ERROR;
		}
		if (dp->f_used) {
			memcpy(np, dp->f_data, dp->f_used);
		}
		if (p) {
			p = (u_char *)np + (p - (u_char *)dp->f_data);
		} else {
			p = (u_char *)np;
		}
		dp->f_size += total + 1;
		if (dp->f_free) {
			(*dp->f_free)(dp->f_data);
		}
		dp->f_data = np;
		dp->f_free = free;
	}

	/*
	** Append entry to directory list.
	*/
	memcpy(p, buf, total);
	p += total;
	*p++ = 0;
	dp->f_used = (char *)p - dp->f_data;

	/*
	** Increase link count in file.  We don't bump the link count for
	** . or ..
	*/
	if (!((len == 1 && *name == '.') ||
	    (len == 2 && name[0] == '.' && name[1] == '.'))) {
		fp->f_nlink++;
	}
	gettimeofday(&dp->f_mtime);

	return NSD_OK;
}

/*
** This routine will append a new callout to a file.
*/
int
nsd_file_appendcallout(nsd_file_t *dp, nsd_file_t *cp, char *name, int len)
{
	u_char *p;
	uint32_t *q;
	char buf[260];
	int total, nlen, i;
	char *np;

	if (! dp || !cp || ! name || (len <= 0) || (len > 255) ||
	    (FILEID(dp->f_fh) == FILEID(cp->f_fh))) {
		return NSD_ERROR;
	}

	np = (char *)alloca(len+4);
	memcpy(np, name, len);
	np[len] = 0;
	nlen = len;

	/*
	** Verify uniqueness of names and handles.  Unlike the directory,
	** if we have a name collision here we generate a new name, and
	** try again.
	*/
	if (p = (u_char *)dp->f_callouts) {
		i = 1;
		while (*p) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			if (*q == FILEID(cp->f_fh)) {
				return 0;
			}
			if ((*p == nlen) &&
			    (memcmp(p+1, np, nlen) == 0)) {
				/* rewrite name and start again */
				nlen = sprintf(np, "%*.*s.%d", len, len,
				    name, ++i);
				p = (u_char *)dp->f_callouts;
				continue;
			}
			p = (u_char *)(q + 1);
		}
	}

	/*
	** Create directory entry.
	*/
	*(u_char *)buf = nlen;
	memcpy(buf + 1, np, nlen);
	q = (uint32_t *)buf;
	q += WORDS(*(u_char *)buf + 1);
	*q++ = FILEID(cp->f_fh);
	total = (char *)q - buf;
	
	/*
	** Realloc buffer if needed.
	*/
	if ((dp->f_c_size - dp->f_c_used) < total) {
		np = nsd_malloc(dp->f_c_size + total + 1);
		if (! np) {
			nsd_logprintf(NSD_LOG_OPER, 
			    "nsd_file_appendcallout: failed malloc\n");
			return NSD_ERROR;
		}
		if (dp->f_c_used) {
			memcpy(np, dp->f_callouts, dp->f_c_used);
		}
		if (p) {
			p = (u_char *)np + (p - (u_char *)dp->f_callouts);
		} else {
			p = (u_char *)np;
		}
		dp->f_c_size += total + 1;
		if (dp->f_callouts) {
			free(dp->f_callouts);
		}
		dp->f_callouts = np;
	}

	/*
	** Append directory entry.
	*/
	memcpy(p, buf, total);
	p += total;
	*p++ = 0;
	dp->f_c_used = (char *)p - dp->f_callouts;

	/*
	** Increase link count in file.  We don't bump the link count for
	** . or ..
	*/
	if (!((len == 1 && *name == '.') ||
	    (len = 2 && name[0] == '.' && name[1] == '.'))) {
		cp->f_nlink++;
	}
	gettimeofday(&dp->f_mtime);

	return NSD_OK;
}

/*
** The copycallouts method copies references to the callout files which
** match the limits.  To copy the callout files themselves use dupcallouts
** below.
*/
int
nsd_file_copycallouts(nsd_file_t *src, nsd_file_t *dst, nsd_attr_t *limits) {
	u_char *p;
	uint32_t *q;
	nsd_file_t *member;

	if (! src || ! dst) {
		return NSD_ERROR;
	}
	if (! src->f_callouts) {
		return NSD_OK;
	}

	for (p = (u_char *)src->f_callouts; *p; p = (u_char *)(q + 1)) {
		q = (uint32_t *)p;
		q += WORDS(*p + 1);
		member = nsd_file_byid(*q);
		if (member && (! limits ||
		    nsd_attr_subset(limits, member->f_attrs))) {
			nsd_file_appendcallout(dst, member, (char *)p + 1, *p);
		}
	}

	return NSD_OK;
}

/*
** The copycallouts method copies references to callout files.  This routine
** makes copies of each of the callout files and appends them to the dst
** directory.
*/
int
nsd_file_dupcallouts(nsd_file_t *src, nsd_file_t *dst)
{
	nsd_file_t *cp, *member;
	u_char *p;
	uint32_t *q;

	if (! src || ! dst) {
		return NSD_ERROR;
	}
	if (! src->f_callouts) {
		return NSD_OK;
	}

	for (p = (u_char *)src->f_callouts; *p; p = (u_char *)(q + 1)) {
		q = (uint32_t *)p;
		q += WORDS(*p + 1);
		member = nsd_file_byid(*q);
		if (member) {
			if (nsd_file_init(&cp, member->f_name,
			    member->f_namelen, dst, member->f_type,
			    member->f_cred) != NSD_OK) {
				return NSD_ERROR;
			}
			cp->f_lib = member->f_lib;
			nsd_attr_clear(cp->f_attrs);
			cp->f_attrs = nsd_attr_copy(member->f_attrs);
			memcpy(cp->f_control, member->f_control,
			    sizeof(cp->f_control));
			nsd_attr_continue(&cp->f_attrs, dst->f_attrs);
			nsd_file_appendcallout(dst, cp, (char *)p + 1, *p);
		}
	}

	return NSD_OK;
}

nsd_file_t *
nsd_file_dup(nsd_file_t *dp, nsd_file_t *pp)
{
	nsd_file_t *fp;

	if (! dp) {
		return 0;
	}

	if (! nsd_file_init(&fp, dp->f_name, dp->f_namelen, pp,
	    dp->f_type, dp->f_cred)) {
	}
	fp->f_lib = dp->f_lib;

	nsd_attr_clear(fp->f_attrs);
	fp->f_attrs = nsd_attr_copy(dp->f_attrs);
	if (pp) {
		nsd_attr_continue(&fp->f_attrs, pp->f_attrs);
	}

	return fp;
}

/*
** This will just read the next entry in the callout directory and
** lookup the file entry associated with it.
*/
nsd_file_t *
nsd_file_getcallout(nsd_file_t *fp)
{
	u_char *p;
	uint32_t *q;

	if (! fp) {
		return (nsd_file_t *)0;
	}

	p = (u_char *)fp->f_callouts;
	if (p && *p) {
		q = (uint32_t *)p;
		q += WORDS(*p + 1);
		return nsd_file_byid(*q);
	}

	return (nsd_file_t *)0;
}

/*
** This routine will throw away the first callout file in a list.
*/
nsd_file_t *
nsd_file_nextcallout(nsd_file_t *fp)
{
	u_char *p;
	uint32_t *q;
	nsd_file_t *tp;
	int len;

	if (! fp) {
		return (nsd_file_t *)0;
	}

	p = (u_char *)fp->f_callouts;
	if (p && *p) {
		q = (uint32_t *)p;
		q += WORDS(*p + 1);
		
		/*
		** Decrement link count on callout.
		*/
		tp = nsd_file_byid(*q);
		if (tp) {
			nsd_file_clear(&tp);
		}

		/*
		** Shift things down.
		*/
		len = ((u_char *)++q - p);
		if (fp->f_c_used > len) {
			fp->f_c_used -= len;
			memcpy(fp->f_callouts, q, fp->f_c_used);
		} else {
			fp->f_c_used = 0;
			*p = 0;
		}

		/*
		** Find and return next file.
		*/
		if (*p) {
			q = (uint32_t *)p;
			q += WORDS(*p + 1);
			return nsd_file_byid(*q);
		}
	}

	return (nsd_file_t *)0;
}

/*
** Given a pointer to a parent directory and a directory name, make a 
** child directory of the parent, copying the callouts list to the
** new subdirectory.
*/
nsd_file_t *
nsd_file_mkdir(nsd_file_t *pp, char *name, int len, nsd_cred_t *cred) 
{
	nsd_file_t *dp;
	char *cache_name;

	nsd_logprintf(NSD_LOG_HIGH, "Entering nsd_file_mkdir\n");
	
	if (!pp || !name || !len) 
		return NULL;

	if (nsd_file_init(&dp, name, len, pp, NFDIR, cred) != NSD_OK){
		nsd_logprintf(NSD_LOG_HIGH, "\tmkdir failed\n");
		return NULL;
	}

	/* Override the table name with the new filename */
	nsd_attr_store(&dp->f_attrs, "table", dp->f_name);

	if (nsd_file_dupcallouts(pp, dp) != NSD_OK) {
		nsd_logprintf(NSD_LOG_HIGH, 
			      "\tnsd_file_dupecallouts() failed\n");
		nsd_file_clear(&dp);
		return NULL;
	}

	if (nsd_file_appenddir(pp, dp, dp->f_name, dp->f_namelen) != NSD_OK) {
		nsd_logprintf(NSD_LOG_HIGH, "\tnsd_file_appenddir() failed\n");
		nsd_file_clear(&dp);
		return NULL;
	}

	/* and create a unique cache file */
	cache_name = alloca(pp->f_namelen + dp->f_namelen + 2);
	memset(cache_name, 0, pp->f_namelen + dp->f_namelen + 2);
	snprintf(cache_name, pp->f_namelen + dp->f_namelen + 2, "%s:%s",
		 pp->f_name, dp->f_name);
	nsd_logprintf(NSD_LOG_MAX, "nsd_file_mkdir: creating cachefile "
		      "\"%s\"\n", cache_name);
	dp->f_map = nsd_map_get(cache_name);

	return dp;
}
