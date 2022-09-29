#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <alloca.h>
#include <ctype.h>
#include "nfs.h"
#include <t6net.h>
#include <sys/mac.h>
#include <assert.h>
#include <ns_api.h>
#include <ns_daemon.h>

extern long nsd_timeout;
static uint32_t *nfs_buf;
static int _nfs_sock;
struct sockaddr_in _nfs_sin;
#define NFS_BUF_SIZE	65535

typedef struct {
	int			fd;		/* file descriptor for reply */
	struct sockaddr_in	sin;		/* address to reply to */
	mac_t			lbl;		/* label of sender */
	uint32_t		xid;		/* request ID */
	uint32_t		proc;		/* proceedure being called */
	uint32_t		dir;		/* directory fileid */
	nsd_cred_t		*cred;		/* request credentials */
} nfs_reply_t;

typedef int (nfs_proc)(nsd_file_t **, nfs_reply_t *, uint32_t *);

#define OP_READ		1
#define OP_WRITE	2
#define	OP_EXECUTE	3
#define OP_LIST		4

#ifdef DEBUG
void
printpacket(char *buf, int len) 
{
	char *p, *end, *q;
	unsigned i;
	char line[20];

	p = end = buf;
	end += len;
	while (p < end) {
		printf("%016lx: ", p);
		for (i = 0, q = line; i < 16 && p < end; i++, p++, q++) {
			if ((i % 4) == 0) {
				putchar(' ');
			}
			printf("%02x", *p);
			if (isspace(*p)) {
				*q = ' ';
			} else if (isprint(*p)) {
				*q = *p;
			} else {
				*q = '.';
			}
		}
		for (; i < 16; i++) {
			fputs("  ", stdout);
			if ((i % 4) == 0) {
				putchar(' ');
			}
		}
		*q = (char)0;
		printf("  %s\n", line);
	}
}

void
dirlist(char *dir)
{
	u_char *p;
	uint32_t *q;
	char buf[256];

	if (! dir) {
		return;
	}

	for (p = (u_char *)dir; p && *p; p = (u_char *)(q + 1)) {
		q = (uint32_t *)p;
		q += WORDS(*p + 1);
		memcpy(buf, p + 1, *p);
		buf[*p] = (char)0;
		printf("name: %s, len: %d, fileid: %d\n", buf, *p, *q);
	}
}

void
attrlist(nsd_attr_t *ap)
{
	printf("attributes are:\n");
	for (; ap; ap = ap->a_next) {
		if (ap->a_key) {
			printf("\tkey: %s, value: %s\n", ap->a_key,
			    ap->a_value);
		} else {
			printf("\tforward\n");
		}
	}
}
#endif

/*
** This will check that the user as defined in the reply structure is
** allowed the operation specified in op on the given file.
*/
static int
nfs_check(nsd_cred_t *cp, nsd_file_t *fp, int op)
{
	int i;

	switch (op) {
	    case OP_READ:
		if (((cp->c_uid == 0) || (cp->c_uid == fp->f_uid)) &&
		    (fp->f_mode & S_IRUSR)) {
			return NSD_OK;
		} else if (fp->f_mode & S_IROTH) {
			return NSD_OK;
		} else {
			if (fp->f_mode & S_IRGRP) {
				for (i = 0; i < cp->c_gids; i++) {
					if (cp->c_gid[i] == fp->f_gid) {
						return NSD_OK;
					}
				}
			}
		}
		return NSD_ERROR;
	    case OP_WRITE:
		if (((cp->c_uid == 0) || (cp->c_uid == fp->f_uid)) &&
		    (fp->f_mode & S_IWUSR)) {
			return NSD_OK;
		} else if (fp->f_mode & S_IWOTH) {
			return NSD_OK;
		} else {
			if (fp->f_mode & S_IWGRP) {
				for (i = 0; i < cp->c_gids; i++) {
					if (cp->c_gid[i] == fp->f_gid) {
						return NSD_OK;
					}
				}
			}
		}
		return NSD_ERROR;
	    case OP_EXECUTE:
	    case OP_LIST:
		if (((cp->c_uid == 0) || (cp->c_uid == fp->f_uid)) &&
		    (fp->f_mode & S_IXUSR)) {
			return NSD_OK;
		} else if (fp->f_mode & S_IXOTH) {
			return NSD_OK;
		} else {
			if (fp->f_mode & S_IXGRP) {
				for (i = 0; i < cp->c_gids; i++) {
					if (cp->c_gid[i] == fp->f_gid) {
						return NSD_OK;
					}
				}
			}
		}
		return NSD_ERROR;
	}

	return NSD_ERROR;
}

/*
** This verifies that the request is coming from the local host on a
** reserved port.
*/
static int
islocal(nfs_reply_t *sd)
{
	if (sd && nsd_local(&sd->sin) && (sd->sin.sin_port < 1024)) {
		return NSD_OK;
	}
	nsd_logprintf(NSD_LOG_MIN,
	    "non-local host requesting local information: %s.%d\n",
	    inet_ntoa(sd->sin.sin_addr), sd->sin.sin_port);

	return NSD_ERROR;
}

/*
** This routine just frees memory associated with the reply structure.
*/
static void
reply_clear(nfs_reply_t **sdp)
{
	if (sdp && *sdp) {
		if ((*sdp)->lbl) {
			free((*sdp)->lbl);
		}
		if ((*sdp)->cred) {
			nsd_cred_clear(&((*sdp)->cred));
		}
		free(*sdp);
		*sdp = 0;
	}
}

/*
** This routine is called when we have an error in parsing the RPC
** packet.  It indicates that this was a bogus request for the supplied
** reason.
*/
static int
reject_err(nfs_reply_t *sd, uint32_t error)
{
	uint32_t *p;

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	*p++ = htonl(sd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_DENIED);
	*p++ = htonl(error);
	
	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/*
** This routine is called when we have an internal error with the RPC
** request.  This indicates that the RPC message seemed alright, but
** that we were unable to complete it.
*/
static int
accept_err(nfs_reply_t *sd, uint32_t error)
{
	uint32_t *p;

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	*p++ = htonl(sd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	/* null verf  */
	*p++ = 0; *p++ = 0;

	*p++ = htonl(error);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/*
** This routine is called to return an RPC answer which indicates failed
** response.  All of the proceedure routines use this.
*/
static int
nfs_err(nfs_reply_t *sd, uint32_t error)
{
	uint32_t *p;

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	*p++ = htonl(sd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	/* null verf  */
	*p++ = 0; *p++ = 0;
	*p++ = htonl(SUCCESS);

	*p++ = htonl(error);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

static int
nfs_msg(nfs_reply_t *sd, uint32_t *buf)
{
	uint32_t *p;

	if (! sd || ! buf) {
		return 0;
	}

	p = buf;

	*p++ = htonl(sd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	/* null verf  */
	*p++ = 0; *p++ = 0;
	*p++ = htonl(SUCCESS);

	*p++ = htonl(NFS_OK);

	return (p - buf);
}

/*
** This is the obligatory NULL routine for this RPC server.  All RPC
** programs are required to implement one.  We just send back a successful
** result with no data section.
*/
/* ARGSUSED */
static int
nfs2_null(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs2_null:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

static int
nfs2_attrs(nsd_file_t *fp, uint32_t *buf)
{
	uint32_t *p;

	if (! fp || ! buf) {
		return 0;
	}

	p = buf;

	*p++ = htonl(fp->f_type);				/* type */
	switch (fp->f_type) {					/* mode */
	    case NFINPROG:
	    case NFREG:	*p++ = htonl(fp->f_mode + NFSMODE_REG); break;
	    case NFDIR: *p++ = htonl(fp->f_mode + NFSMODE_DIR); break;
	    case NFLNK:	*p++ = htonl(fp->f_mode + NFSMODE_LNK); break;
	}
	*p++ = htonl(fp->f_nlink);				/* nlink */
	*p++ = htonl(fp->f_uid);				/* uid */
	*p++ = htonl(fp->f_gid);				/* gid */
	*p++ = htonl(fp->f_used);				/* size */
	*p++ = htonl(4096);					/* blocksize */
	*p++ = 0;						/* rdev */
	*p++ = htonl(BLOCKS(fp->f_size, 4096));			/* blocks */
	*p++ = 0;						/* fsid */
	*p++ = htonl(FILEID(fp->f_fh));				/* fileid */
	gettimeofday(&fp->f_atime);
	*p++ = htonl(fp->f_atime.tv_sec);			/* atime */
	*p++ = htonl(fp->f_atime.tv_usec);
	*p++ = htonl(fp->f_mtime.tv_sec);			/* mtime */
	*p++ = htonl(fp->f_mtime.tv_usec);
	*p++ = htonl(fp->f_ctime.tv_sec);			/* ctime */
	*p++ = htonl(fp->f_ctime.tv_usec);

	return (p - buf);
}

/*
** This routine implements the GETATTR NFS proceedure.  We simply walk
** The tree looking for the requested filehandle and return the status
** information if we find it.  If we do not find it then we return an
** error that says the requested filehandle is stale and should be looked
** up again.
*/
/* ARGSUSED */
static int
nfs2_getattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_file_t *fp, *cp;
	uint64_t fh[4];

	nsd_logprintf(NSD_LOG_LOW, "nfs_getattr:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	memcpy(fh, p, NFS_FHSIZE);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs_err(sd, NFSERR_STALE);
	}

	if ((fp->f_type == NFREG) && (cp = nsd_file_getcallout(fp)) &&
	    cp->f_lib && cp->f_lib->l_funcs[VERIFY] &&
	    ((*cp->f_lib->l_funcs[VERIFY])(fp, sd->cred) != NSD_OK)) {
		cp = nsd_file_byid(PARENT(fp->f_fh));
		if (cp) {
			nsd_file_delete(cp, fp);
		}
		return nfs_err(sd, NFSERR_STALE);
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** struct fattr
	*/
	p += nfs2_attrs(fp, p);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/*
** This routine implements the NFS SETATTR proceedure.  For now
** we are a read only filesystem.  Sets need to go through a separate
** channel.
*/
/* ARGSUSED */
static int
nfs2_setattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs_setattr:\n");

	nfs_err(sd, NFSERR_ROFS);

	return NSD_OK;
}

/*
** This is the reply routine for the lookup routine below.  If we
** found the answer to the lookup request then we return it to the
** client, stuff it into the cache, and attach it to our tree,
** assuming a later read.
*/
static int
nfs2_lookup_reply(nsd_file_t *rq)
{
	uint32_t *p;
	nfs_reply_t *sd;
	nsd_file_t *dp;

	nsd_logprintf(NSD_LOG_LOW, "nfs_lookup_reply:\n");

	sd = (nfs_reply_t *)rq->f_sender;
	assert(sd && !rq->f_cmd_data);
	rq->f_send = 0, rq->f_sender = 0;

	/* Find parent directory. */
	dp = nsd_file_byid(sd->dir);
	if (! dp) {
		nsd_file_clear(&rq);
		return nfs_err(sd, NFSERR_NOENT);
	}

	switch (rq->f_status) {
	    case NS_SUCCESS:
		nsd_map_update(rq);
		rq->f_type = NFREG;
		if ((nfs_check(sd->cred, dp, OP_LIST) != NSD_OK) ||
		    (nsd_attr_fetch_bool(rq->f_attrs, "local", FALSE) &&
		    (! islocal(sd)))) {
			return nfs_err(sd, NFSERR_ACCES);
		}

		/* Return results to client. */
		p = nfs_buf;
		p += nfs_msg(sd, p);

		/* diropenres structure */
		memcpy(p, rq->f_fh, NFS_FHSIZE);
		p += (NFS_FHSIZE/sizeof(*p));
		p += nfs2_attrs(rq, p);

		tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
				&sd->sin, sizeof(sd->sin), sd->lbl);

		reply_clear(&sd);

		break;
	/*
	** This is all pretty screwy, there is an ugly race condition
	** with nis retries versus our in-progress state.  Since for
	** errors the file is never released we hold a file in memory
	** until the next lookup, but here we cannot know that the
	** client request has not timed out so we return EIO and let
	** the libc ns_lookup routine retry and return the correct error
	** out of the nfs2_lookup routine below.
	*/
	    case NS_NOTFOUND:
	    default:
		nsd_map_update(rq);
		/* fall through */
	    case NS_TRYAGAIN:
	    case NS_UNAVAIL:
		rq->f_type = NFREG;
		return nfs_err(sd, NFSERR_IO);
	}

	return NSD_OK;
}

/*
** This is the guts of the protocol.  This routine implements the
** NFS LOOKUP routine which is called to translate a filename into
** a handle.  If this name corresponds to a directory which is part
** of the infrastructure, built when parsing the config file, or
** it is in the "cache", we just return the answer.  Otherwise we
** setup a callout list so that the lookup will be forwarded through
** the libraries one at a time until we find an answer.
*/
/* ARGSUSED */
static int
nfs2_lookup(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	int len, i;
	nsd_file_t *fp, *dp, *cp;
	char *r, *s;
	char *key;
	uint64_t fh[4];
	time_t now;

	nsd_logprintf(NSD_LOG_LOW, "nfs_lookup:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	memcpy(fh, p, NFS_FHSIZE);
	sd->dir = FILEID(fh);
	dp = nsd_file_byid(sd->dir);
	if (! dp) {
		return nfs_err(sd, NFSERR_STALE);
	}

	p += (NFS_FHSIZE/sizeof(*p));
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		return nfs_err(sd, NFSERR_NAMETOOLONG);
	}

	/* look up in tree */
	nsd_logprintf(NSD_LOG_HIGH, "\tname: %*.*s\n", len, len, (char *)p);
	if ((fp = nsd_file_byname(dp->f_data, (char *)p, len)) ||
	    (fp = nsd_file_byname(dp->f_callouts, (char *)p, len))) {
		if (fp->f_type == NFINPROG) {
			nsd_logprintf(NSD_LOG_LOW,
			    "nfs2_lookup: lookup of %s already in progress\n",
			    fp->f_name);
			return nfs_err(sd, NFSERR_IO);
		}

		switch (fp->f_status) {
		    case NS_TRYAGAIN:
		    case NS_UNAVAIL:
			if (nsd_attr_fetch_bool(fp->f_attrs,
			    "wait_for_server", 0)) {
				nsd_file_delete(dp, fp);
				return nfs_err(sd, NFS3ERR_IO);
			} else {
				nsd_file_delete(dp, fp);
				return nfs_err(sd, NFS3ERR_NOENT);
			}
		    case NS_NOTFOUND:
			nsd_file_delete(dp, fp);
			return nfs_err(sd, NFS3ERR_NOENT);
		}

		/*
		** Verify that the file in the tree is actually alright.
		*/
		i = NSD_OK;
		if ((fp->f_type == NFREG) && (cp = nsd_file_getcallout(fp)) &&
		    cp->f_lib && cp->f_lib->l_funcs[VERIFY]) {
			i = (*cp->f_lib->l_funcs[VERIFY])(fp, sd->cred);
		}

		if (i == NSD_OK) {
			/*
			** Check permissions.
			*/
			if ((nfs_check(sd->cred, dp, OP_LIST) != NSD_OK) ||
			    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
			    (! islocal(sd)))) {
				return nfs_err(sd, NFSERR_ACCES);
			}

			/* we found the answer so we just return it */

			p = nfs_buf;
			p += nfs_msg(sd, p);

			/*
			** diropenres structure
			*/
			memcpy(p, fp->f_fh, NFS_FHSIZE);
			p += (NFS_FHSIZE/sizeof(*p));
			p += nfs2_attrs(fp, p);

			tsix_sendto_mac(sd->fd, nfs_buf,
			    (p - nfs_buf)*sizeof(*p), 0, &sd->sin,
			    sizeof(sd->sin), sd->lbl);
			reply_clear(&sd);

			return NSD_OK;
		} else {
			nsd_file_delete(dp, fp);
		}
	}

	/*
	** We did not find the answer in our tree so we check to see 
	** if the parent directory is marked NSD_FLAGS_MKDIR.  If so, 
	** we return ENOENT and allow libc to attempt a mkdir().  Otherwise,
	** we build up a callout list for this request and walk it through, 
	** remembering to use the original name length.
	**
	** This prevents directories marked NSD_FLAGS_MKDIR from answering
	** queries directly.  They can only contain dynamic maps.
	*/
	if (dp->f_flags & NSD_FLAGS_MKDIR) {
		return nfs_err(sd, NFSERR_NOENT);
	} 
	
	if (nsd_file_init(&fp, (char *)p, len, dp, NFINPROG, sd->cred) 
		   != NSD_OK) {
		return accept_err(sd, SYSTEM_ERROR);
	}
	fp->f_mode &= 0666; /* don't inherit execute from table */

	/*
	** Walk name changing backslash to slash.  Two backslashes means that
	** one stays.
	*/
	key = alloca(fp->f_namelen + 1);
	for (i = 0, r = fp->f_name, s = key; i < fp->f_namelen; i++, r++, s++) {
		if (r[0] == '\\') {
			if (r[1] == '\\') {
				*s = '\\';
				r++;
			} else {
				*s = '/';
			}
		} else {
			*s = *r;
		}
	}
	*s = (char)0;

	/*
	** Check for attributes and set them.  Attributes look like:
	**	name(key1=value1,key2=value2,. . .)
	** attributes can be used to limit the scope of the search.
	** Only callouts that match all these attributes will be used
	** to locate the data.
	*/
	r = strchr(key, '(');
	if (r) {
		*r++ = (char)0;
		nsd_attr_parse(&fp->f_attrs, r, 0, (char *)0);

		/* There are some attributes we do not allow to be changed. */
		nsd_attr_delete(&fp->f_attrs, "table");
		nsd_attr_delete(&fp->f_attrs, "file");
		nsd_attr_delete(&fp->f_attrs, "local");
		nsd_attr_delete(&fp->f_attrs, "SGI_MAC_FILE");
		nsd_attr_delete(&fp->f_attrs, "SGI_CAP_FILE");
	}

	/*
	** If we are accessing the magic .all file then we go through a
	** different library entrance routine.
	*/
	if (strcmp(key, NS_ALL_FILE) == 0) {
		fp->f_index = LIST;
		fp->f_timeout = nsd_attr_fetch_long(fp->f_attrs,
		    "list_timeout", 10, nsd_timeout/1000);
	} else {
		fp->f_index = LOOKUP;
		fp->f_timeout = nsd_attr_fetch_long(fp->f_attrs,
		    "lookup_timeout", 10, nsd_timeout/1000);
	}

	time(&now);
	if (fp->f_timeout < 0) {
		fp->f_timeout = now;
	} else {
		fp->f_timeout += now;
	}

	/*
	** If our parent is a callout then we need to duplicate it so as
	** not to create a loop in the file structure.  Otherwise we just
	** copy our parents callout list.
	*/
	if (dp->f_lib && dp->f_lib->l_funcs[fp->f_index]) {
		cp = nsd_file_dup(dp, fp);
		nsd_attr_continue(&cp->f_attrs, dp->f_attrs);
		nsd_file_appendcallout(fp, cp, cp->f_name, cp->f_namelen);
	} else {
		nsd_file_copycallouts(dp, fp, 0);
	}

	nsd_attr_store(&fp->f_attrs, "key", key);
	if (fp->f_index != LOOKUP) {
		nsd_attr_store(&fp->f_attrs, "timeout", "0");
	}

	/*
	** Loop through the callouts.  We return the results of the first
	** one we find.  If it returns failure then our caller is responsible
	** for continuing on through the list.
	*/
	for (cp = nsd_file_getcallout(fp); cp; cp = nsd_file_nextcallout(fp)) {
		if (cp->f_lib && cp->f_lib->l_funcs[fp->f_index]) {
			nsd_attr_continue(&fp->f_attrs, cp->f_attrs);
			fp->f_send = nfs2_lookup_reply;
			fp->f_sender = (void *)sd;
			if (nsd_file_appenddir(dp, fp, fp->f_name,
			    fp->f_namelen) != NSD_OK) {
				nsd_logprintf(NSD_LOG_LOW,
			    "nfs2_lookup: failed appenddir for %s: try again\n",
				    fp->f_name);
				nsd_file_clear(&fp);
				return nfs_err(sd, NFSERR_IO);
			}
			*rqp = fp;
			return (*cp->f_lib->l_funcs[fp->f_index])(fp);
		}
	}

	/*
	** No callouts defined with the proper function enabled.
	*/
	nsd_file_clear(&fp);
	return nfs_err(sd, NFSERR_NOENT);
}

/*
** This routine implements the NFS READLINK proceedure.  At the moment
** we do not support symlinks so this should never be called.
*/
/* ARGSUSED */
static int
nfs2_readlink(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_file_t *fp;
	uint64_t fh[4];

	nsd_logprintf(NSD_LOG_LOW, "nfs_readlink:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	memcpy(fh, p, NFS_FHSIZE);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs_err(sd, NFSERR_STALE);
	}

	/*
	** We only return the data in a link.
	*/
	if (fp->f_type != NFLNK) {
		return nfs_err(sd, NFSERR_NOENT);
	}

	/*
	** Check permissions.
	*/
	if ((nfs_check(sd->cred, fp, OP_READ) != NSD_OK) ||
	    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs_err(sd, NFSERR_ACCES);
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** data
	*/
	p[fp->f_used/sizeof(*p)] = 0;
	memcpy(p, fp->f_data, fp->f_used);
	p += WORDS(fp->f_used);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/*
** This routine implements the NFS READ proceedure.  We lookup the handle
** in our tree and return the specified portion of the data.  If we cannot
** find the handle then we reply with a stale handle error.  The client
** should relookup a stale handle.
*/
/* ARGSUSED */
static int
nfs2_read(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	unsigned offset, count, len, max;
	nsd_file_t *fp, *cp;
	uint64_t fh[4];

	nsd_logprintf(NSD_LOG_LOW, "nfs_read:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	memcpy(fh, p, NFS_FHSIZE);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs_err(sd, NFSERR_STALE);
	}
	p += (NFS_FHSIZE/sizeof(*p));
	offset = ntohl(*p++);
	count = ntohl(*p);

	/*
	** Verify that the file is still good.
	*/
	if ((fp->f_type == NFREG) && (cp = nsd_file_getcallout(fp)) &&
	    cp->f_lib && cp->f_lib->l_funcs[VERIFY] &&
	    ((*cp->f_lib->l_funcs[VERIFY])(fp, sd->cred) != NSD_OK)) {
		cp = nsd_file_byid(PARENT(fp->f_fh));
		if (cp) {
			nsd_file_delete(cp, fp);
		}
		return nfs_err(sd, NFSERR_STALE);
	}

	/*
	** Check permissions.
	*/
	if ((nfs_check(sd->cred, fp, OP_READ) != NSD_OK) ||
	    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs_err(sd, NFSERR_ACCES);
	}

	/*
	** Build result.
	*/
	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** struct fattr
	*/
	p += nfs2_attrs(fp, p);

	/*
	** nfsdata
	*/
	max = NFS_BUF_SIZE - ((p - nfs_buf) * sizeof(*p)) - sizeof(*p);
	if (offset > fp->f_used) {
		len = 0;
	} else if (count < (fp->f_used - offset)) {
		if (count < max) {
			len = count;
		} else {
			len = max;
		}
	} else if ((fp->f_used - offset) < max) {
		len = fp->f_used - offset;
	} else {
		len = max;
	}

	*p++ = htonl(len);
	if (len > 0) {
		p[len/sizeof(*p)] = 0;
		memcpy(p, fp->f_data + offset, len);
	}
	p += WORDS(len);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/*
** This function implements the NFS WRITECACHE proceedure.  At the moment
** we do not support writes so we just return a read-only filesystem
** error.
*/
/* ARGSUSED */
static int
nfs2_writecache(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs_writecache:\n");

	nfs_err(sd, NFSERR_ROFS);

	return NSD_OK;
}

/*
** This function implements the NFS WRITE proceedure.  At the moment
** we do not support writes so we just return a read-only filesystem
** error.
*/
/* ARGSUSED */
static int
nfs2_write(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs_write:\n");

	nfs_err(sd, NFSERR_ROFS);

	return NSD_OK;
}

/*
** This function implements the NFS CREATE proceedure.  At the moment
** we do not support writes so we just return a read-only filesystem
** error.
*/
/* ARGSUSED */
static int
nfs2_create(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs_create:\n");

	nfs_err(sd, NFSERR_ROFS);

	return NSD_OK;
}

/*
** This routine implements the NFS REMOVE proceedure.  We translate this
** into a cache flush.  We remove this element from our internal tree if
** they have a timeout.  We also flush the shared cache.  This will not
** remove any of the infrastructure built while parsing the config file
** as they have an infinite timeout.
*/
/* ARGSUSED */
static int
nfs2_remove(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_file_t *dp, *fp;
	uint64_t fh[4];
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs2_remove:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	memcpy(fh, p, NFS_FHSIZE);
	sd->dir = FILEID(fh);
	dp = nsd_file_byid(sd->dir);
	if (! dp) {
		return nfs_err(sd, NFSERR_STALE);
	}
	if (dp->f_type != NFDIR) {
		return nfs_err(sd, NFSERR_NOTDIR);
	}
	p += NFS_FHSIZE/sizeof(*p);

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		return nfs_err(sd, NFSERR_NAMETOOLONG);
	}

	/* Look it up in tree. */
	if ((fp = nsd_file_byname(dp->f_callouts, (char *)p, len)) ||
	    (fp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		/*
		** Check permissions.
		*/
		if ((nfs_check(sd->cred, dp, OP_WRITE) != NSD_OK) ||
		    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
		    (! islocal(sd)))) {
			return nfs_err(sd, NFSERR_PERM);
		}

		p = nfs_buf;
		p += nfs_msg(sd, p);

		p += htonl(NFS_OK);

		/*
		** We do not delete directories, instead we do a timeout.
		*/
		switch (fp->f_type) {
		    case NFDIR:
	          	if (fp->f_flags & NSD_FLAGS_DYNAMIC) {
				nsd_map_unlink(fp);
				nsd_file_delete(dp, fp);
			} else {
				nsd_map_flush(fp);
				nsd_file_timeout(&fp, 0);
			}
			break;
		    case NFREG:
			nsd_map_remove(fp);
			nsd_file_delete(dp, fp);
		}

		tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
				&sd->sin, sizeof(sd->sin), sd->lbl);
		reply_clear(&sd);

		return NSD_OK;
	}

	return nfs_err(sd, NFSERR_NOENT);
}

/*
** This routine implements the NFS RENAME proceedure.  We do not allow
** items to be renamed yet.  For now we just claim to be a read-only
** filesystem.
*/
/* ARGSUSED */
static int
nfs2_rename(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs_rename:\n");

	nfs_err(sd, NFSERR_ROFS);

	return NSD_OK;
}

/*
** This routine implements the NFS LINK proceedure.  For now we do not
** allow this.  We just claim to be a read-only filesystem.
*/
/* ARGSUSED */
static int
nfs2_link(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs_link:\n");

	nfs_err(sd, NFSERR_ROFS);

	return NSD_OK;
}

/*
** This routine implements the NFS SYMLINK proceedure.  For now we do not
** allow this.  We just claim to be a read-only filesystem.
*/
/* ARGSUSED */
static int
nfs2_symlink(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs_symlink:\n");

	nfs_err(sd, NFSERR_ROFS);

	return NSD_OK;
}

/*
** This routine implements the NFS MKDIR proceedure.  This does not make
** much sense dynamically.  We build all the directories as part of parsing
** the configuration files.  For now we just claim to be a read-only
** filesystem here.
**
** Unless the parent directory (pp) is marked NSD_FLAGS_MKDIR.  In this
** case, we create a new directory for the user.
*/
/* ARGSUSED */
static int
nfs2_mkdir(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[4];
	nsd_file_t *pp, *dp;
	int len, i;
	char *q;

	nsd_logprintf(NSD_LOG_LOW, "nfs2_mkdir:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	memcpy(fh, p, NFS_FHSIZE);
	sd->dir = FILEID(fh);
	pp = nsd_file_byid(sd->dir);
	if (! pp || (pp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs_err(sd, NFSERR_STALE);
	}

	/*
	** Try to find the name in the handle
	*/
	p += NFS_FHSIZE/sizeof(*p);
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		return nfs_err(sd, NFSERR_NAMETOOLONG);
	}
	
	/*
	** Ignore any attributes from table name by not including them
	** in the length. 
	*/
	for (i = len, len = 0, q = (char *)p;
	     len < i  && q && *q != '(' && *q != '[' &&  !isspace(*q);
	     len++, q++);

	nsd_logprintf(NSD_LOG_HIGH, "\tname: %*.*s\n", len, len, (char *)p);

	/*
	** Lookup the name.
	*/
	if ((dp = nsd_file_byname(pp->f_data, (char *)p, len)) ||
	    (dp = nsd_file_byname(pp->f_callouts, (char *)p, len))){
	  
		if (dp->f_status != NS_SUCCESS) {
			nsd_logprintf(NSD_LOG_HIGH,
				      "\treturning NFSERR_STALE\n");
			return nfs_err(sd, NFSERR_STALE);
		}

		/* 
		** It exists...
		*/
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_EXIST\n");
		return nfs_err(sd, NFSERR_EXIST);
	}
	/*
	** The lookup by name failed.  If the parent directory allows for 
	** mkdirs, mkdir.
	*/
	if (pp->f_flags & NSD_FLAGS_MKDIR) {

		nsd_logprintf(NSD_LOG_HIGH, "\tcreating directory\n");
		dp = nsd_file_mkdir(pp, (char *) p, len, sd->cred);
		if (!dp) {
			return nfs_err(sd, NFSERR_REMOTE);
		}

		/* Tag this directory as DYNAMIC */
		dp->f_flags |= NSD_FLAGS_DYNAMIC;

		/*
		** Return results to client.
		*/
		p = nfs_buf;
		p += nfs_msg(sd, p);
		
		/*
		** file handle
		*/
		*p++ = htonl(NFS_FHSIZE);
		memcpy(p, dp->f_fh, NFS_FHSIZE);
		p += (NFS_FHSIZE/sizeof(uint32_t));
		
		/*
		** file attributes
		*/
		p += nfs2_attrs(dp, p);
		
		tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0, 
				&sd->sin, sizeof(sd->sin), sd->lbl);
				
		reply_clear(&sd);

		return NSD_OK;
	}

	/*
	** Directory does not support dynamic directory creation or
	** nsd_file_mkdir failed.
	*/

	nfs_err(sd, NFSERR_ROFS);
	return NSD_OK;
}


/*
** This routine implements the NFS READDIR proceedure.  We just walk the
** f_callouts and f_members lists appending the elements to the buffer
** until we are done or reach the maximum specified in the request.
** If we are given a cookie we just forward into the lists as far as
** the cookie.
*/
/* ARGSUSED */
static int
nfs2_readdir(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint32_t cookie, count, *entry=0;
	nsd_file_t *dp;
	char *start;
	unsigned i;
	uint64_t fh[4];
	u_char *s;
	uint32_t *t;

	nsd_logprintf(NSD_LOG_LOW, "nfs2_readdir:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	memcpy(fh, p, NFS_FHSIZE);
	dp = nsd_file_byhandle(fh);
	if (! dp) {
		return nfs_err(sd, NFSERR_STALE);
	}
	if (dp->f_type != NFDIR) {
		return nfs_err(sd, NFSERR_NOTDIR);
	}
	if ((nfs_check(sd->cred, dp, OP_READ) != NSD_OK) ||
	    (nsd_attr_fetch_bool(dp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs_err(sd, NFSERR_ACCES);
	}
	p += (NFS_FHSIZE/sizeof(*p));

	/*
	** Cookie is an offset into the list.  The first bit determines
	** which list.  0 = callouts, 1 = members
	*/
	cookie = ntohl(p[0]);
	count = ntohl(p[1]);

	p = nfs_buf;
	p += nfs_msg(sd, p);

	start = (char *)p;
	if ((cookie < 0x80000000) && dp->f_callouts) {
		for (s = (u_char *)dp->f_callouts, i = 0;
		     *s && i < cookie;
		     s = (u_char *)(t + 1), i++) {
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
		}
		for (; *s; s = (u_char *)(t + 1), i++) {
			entry = p;
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
			*p++ = htonl(1);		/* pointer */
			*p++ = htonl(*t);		/* fileid */
			*p++ = htonl(*s);		/* name len */
			p[*s/sizeof(*p)] = 0;		/* null pad name */
			memcpy(p, s + 1, *s);		/* name */
			p += WORDS(*s);
			*p++ = htonl(i);		/* cookie */
			if ((((char *)p) - start) > count) {
				p = entry;
				break;
			}
		}
	}
	if ((p != entry) && dp->f_data) {
		for (s = (u_char *)dp->f_data, i = 0x80000000;
		     *s && i < cookie;
		     s = (u_char *)(t + 1), i++) {
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
		}
		for (; *s; s = (u_char *)(t + 1), i++) {
			entry = p;
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
			*p++ = htonl(1);		/* pointer */
			*p++ = htonl(*t);		/* fileid */
			*p++ = htonl(*s);		/* name len */
			p[*s/sizeof(*p)] = 0;		/* null pad name */
			memcpy(p, s + 1, *s);		/* name */
			p += WORDS(*s);
			*p++ = htonl(i);		/* cookie */
			if ((((char *)p) - start) > count) {
				p = entry;
				break;
			}
		}
	}
	*p++ = 0;					/* pointer */
	if (s && *s) {
		*p++ = htonl(0);			/* eof - no */
	} else {
		*p++ = htonl(1);			/* eof - yes */
	}

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/*
** This returns information about the specified mount point.  Mount
** points are created while we are parsing the corresponding config
** file, and stored in the structure tree as regular file nodes.
*/
/* ARGSUSED */
static int
nfs2_statfs(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs2_statfs:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** struct statfsres
	*/
	*p++ = htonl(4096);		/* tsize */
	*p++ = htonl(4096);		/* bsize */
	*p++ = htonl(0);		/* blocks */
	*p++ = htonl(0);		/* bfree */
	*p++ = htonl(0);		/* bavail */

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

static int
nfs3_attrs(nsd_file_t *fp, uint32_t *buf)
{
	uint32_t *p;

	if (! fp || ! buf) {
		return 0;
	}

	p = buf;

	if (fp->f_type == NFINPROG) {				/* type */
		*p++ = htonl(NFREG);
	} else {
		*p++ = htonl(fp->f_type);
	}
	*p++ = htonl(fp->f_mode);				/* mode */
	*p++ = htonl(fp->f_nlink);				/* nlink */
	*p++ = htonl(fp->f_uid);				/* uid */
	*p++ = htonl(fp->f_gid);				/* gid */
	*p++ = 0; *p++ = htonl(fp->f_used);			/* size */
	*p++ = 0; *p++ = htonl(fp->f_used);			/* used */
	*p++ = 0; *p++ = 0;					/* rdev */
	*p++ = 0; *p++ = 0;					/* fsid */
	*p++ = 0; *p++ = htonl(FILEID(fp->f_fh));		/* fileid */
	gettimeofday(&fp->f_atime);
	*p++ = htonl(fp->f_atime.tv_sec);			/* atime */
	*p++ = htonl(fp->f_atime.tv_usec);
	*p++ = htonl(fp->f_mtime.tv_sec);			/* mtime */
	*p++ = htonl(fp->f_mtime.tv_usec);
	*p++ = htonl(fp->f_ctime.tv_sec);			/* ctime */
	*p++ = htonl(fp->f_ctime.tv_usec);

	return (p - buf);
}

/* ARGSUSED */
static int
nfs3_null(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs3_null:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/*
** This is the error routine for nfs3.  It just replies with a header,
** an error number, and a variable list of attributes.
*/
/*VARARGS3*/
static int
nfs3_err(nfs_reply_t *sd, uint32_t error, int count, ...)
{
	uint32_t *p;
	nsd_file_t *fp;
	va_list ap;
	int i;

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	*p++ = htonl(sd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	/* null verf  */
	*p++ = 0; *p++ = 0;
	*p++ = htonl(SUCCESS);

	*p++ = htonl(error);

	va_start(ap, count);
	for (i = 0; i < count; i++) {
		fp = va_arg(ap, nsd_file_t *);
		if (fp) {
			*p++ = htonl(TRUE);
			p += nfs3_attrs(fp, p);
		} else {
			*p++ = htonl(FALSE);
		}
	}
	va_end(ap);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_getattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp, *cp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_getattr:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 0);
	}
	memcpy(fh, p, len);
	nsd_logprintf(NSD_LOG_MAX, "\tfileid: %lld\n", FILEID(fh));
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 0);
	}

	if ((fp->f_type == NFREG) && (cp = nsd_file_getcallout(fp)) &&
	    cp->f_lib && cp->f_lib->l_funcs[VERIFY] &&
	    ((*cp->f_lib->l_funcs[VERIFY])(fp, sd->cred) != NSD_OK)) {
		cp = nsd_file_byid(PARENT(fp->f_fh));
		if (cp) {
			nsd_file_delete(cp, fp);
		}
		return nfs_err(sd, NFSERR_STALE);
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** post_op_attr
	*/
	p += nfs3_attrs(fp, p);

	nsd_logprintf(NSD_LOG_MAX, "\tsending response to %s port %d\n",
	    inet_ntoa(sd->sin.sin_addr), sd->sin.sin_port);
	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_setattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_setattr:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}

	nfs3_err(sd, NFS3ERR_ROFS, 2, (nsd_file_t *)0, fp);

	return NSD_OK;
}

/*
** This is the reply routine for the lookup routine below.  If we
** found the answer to the lookup request then we return it to the
** client, stuff it into the cache, and attach it to our tree,
** assuming a later read.
*/
static int
nfs3_lookup_reply(nsd_file_t *rq)
{
	uint32_t *p;
	nfs_reply_t *sd;
	nsd_file_t *dp;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_lookup_reply:\n");

	assert(rq);
	sd = (nfs_reply_t *)rq->f_sender;
	assert(sd && !rq->f_cmd_data);
	rq->f_send = 0, rq->f_sender = 0;

	/* Find parent directory. */
	dp = nsd_file_byid(sd->dir);
	if (! dp) {
		rq->f_sender = 0;
		nsd_file_clear(&rq);
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}

	switch (rq->f_status) {
	    case NS_SUCCESS:
		nsd_map_update(rq);
		rq->f_type = NFREG;
		if ((nfs_check(sd->cred, dp, OP_LIST) != NSD_OK) ||
		    (nsd_attr_fetch_bool(rq->f_attrs, "local", FALSE) &&
		    (! islocal(sd)))) {
			return nfs3_err(sd, NFS3ERR_ACCES, 1, (nsd_file_t *)0);
		}

		/* Return results to client. */
		p = nfs_buf;
		p += nfs_msg(sd, p);

		/* file handle */
		*p++ = htonl(NFS3_FHSIZE);
		memcpy(p, rq->f_fh, NFS3_FHSIZE);
		p += (NFS3_FHSIZE/sizeof(uint32_t));

		/* file attributes */
		*p++ = htonl(TRUE);
		p += nfs3_attrs(rq, p);

		/* directory attributes */
		*p++ = htonl(TRUE);
		p += nfs3_attrs(dp, p);

		tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
				&sd->sin, sizeof(sd->sin), sd->lbl);

		reply_clear(&sd);

		break;
	/*
	** This is all pretty screwy, there is an ugly race condition
	** with nis retries versus our in-progress state.  Since for
	** errors the file is never released we hold a file in memory
	** until the next lookup, but here we cannot know that the
	** client request has not timed out so we return EIO and let
	** the libc ns_lookup routine retry and return the correct error
	** out of the nfs3_lookup routine below.
	*/
	    case NS_NOTFOUND:
	    default:
		nsd_map_update(rq);
		/* fall through */
	    case NS_TRYAGAIN:
	    case NS_UNAVAIL:
		rq->f_type = NFREG;
		return nfs3_err(sd, NFS3ERR_IO, 1, dp);
	}

	return NSD_OK;
}

/*
** This is the guts of the protocol.  This routine implements the
** NFS LOOKUP routine which is called to translate a filename into
** a handle.  If this name corresponds to a directory which is part
** of the infrastructure, built when parsing the config file, or
** it is in the "cache", we just return the answer.  Otherwise we
** setup a callout list so that the lookup will be forwarded through
** the libraries one at a time until we find an answer.
*/
/* ARGSUSED */
int
nfs3_lookup(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	int len, i;
	nsd_file_t *fp, *dp, *cp;
	char *r, *s;
	char *key;
	uint64_t fh[8];
	time_t now;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_lookup:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	sd->dir = FILEID(fh);
	dp = nsd_file_byid(sd->dir);
	if (! dp) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}
	p += len/sizeof(*p);

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		return nfs3_err(sd, NFS3ERR_NAMETOOLONG, 1, dp);
	}

	/* Look it up in tree. */
	nsd_logprintf(NSD_LOG_HIGH, "\tname: %*.*s\n", len, len, (char *)p);
	if ((fp = nsd_file_byname(dp->f_callouts, (char *)p, len)) ||
	    (fp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		if (fp->f_type == NFINPROG) {
			nsd_logprintf(NSD_LOG_LOW,
			    "nfs3_lookup: lookup of %s already in progress\n",
			    fp->f_name);
			return nfs3_err(sd, NFS3ERR_IO, 1, dp);
		}

		switch (fp->f_status) {
		    case NS_TRYAGAIN:
		    case NS_UNAVAIL:
			if (nsd_attr_fetch_bool(fp->f_attrs,
			    "wait_for_server", 0)) {
				nsd_file_delete(dp, fp);
				return nfs3_err(sd, NFS3ERR_IO, 1, 0);
			} else {
				nsd_file_delete(dp, fp);
				return nfs3_err(sd, NFS3ERR_NOENT, 1, 0);
			}
		    case NS_NOTFOUND:
			nsd_file_delete(dp, fp);
			return nfs3_err(sd, NFS3ERR_NOENT, 1, 0);
		}
		
		/*
		** Verify that the file in the tree is actually alright.
		*/
		i = NSD_OK;
		if ((fp->f_type == NFREG) && (cp = nsd_file_getcallout(fp)) &&
		    cp->f_lib && cp->f_lib->l_funcs[VERIFY]) {
			i = (*cp->f_lib->l_funcs[VERIFY])(fp, sd->cred);
		}

		if (i == NSD_OK) {
			/*
			** Check permissions.
			*/
			if ((nfs_check(sd->cred, dp, OP_LIST) != NSD_OK) ||
			    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
			    (! islocal(sd)))) {
				return nfs3_err(sd, NFS3ERR_ACCES, 1,
				    (nsd_file_t *)0);
			}

			/* we found the answer so we just return it */
			p = nfs_buf;
			p += nfs_msg(sd, p);

			*p++ = htonl(NFS3_FHSIZE);
			memcpy(p, fp->f_fh, NFS3_FHSIZE);
			p += (NFS3_FHSIZE/sizeof(uint32_t));

			/*
			** file attributes
			*/
			*p++ = htonl(TRUE);
			p += nfs3_attrs(fp, p);

			/*
			** directory attributes
			*/
			*p++ = htonl(TRUE);
			p += nfs3_attrs(dp, p);

			tsix_sendto_mac(sd->fd, nfs_buf,
			    (p - nfs_buf)*sizeof(*p), 0, &sd->sin,
			    sizeof(sd->sin), sd->lbl);
			reply_clear(&sd);

			return NSD_OK;
		} else {
			nsd_file_delete(dp, fp);
		}
	}

	/*
	** We did not find the answer in our tree so we check to see 
	** if the parent directory is marked NSD_FLAGS_MKDIR.  If so, 
	** we return ENOENT and allow libc to attempt a mkdir().  Otherwise,
	** we build up a callout list for this request and walk it through, 
	** remembering to use the original name length.
	**
	** This prevents directories marked NSD_FLAGS_MKDIR from answering
	** queries directly.  They can only contain dynamic maps.
	*/
	if (dp->f_flags & NSD_FLAGS_MKDIR) {
		return nfs3_err(sd, NFS3ERR_NOENT, 1, (nsd_file_t *)0);
	} 
	
	if (nsd_file_init(&fp, (char *)p, len, dp, NFINPROG, sd->cred)
	    != NSD_OK) {
		return accept_err(sd, SYSTEM_ERROR);
	}
	fp->f_mode &= 0666; /* don't inherit execute from table */

	/*
	** Walk name changing backslash to slash.  Two backslashes means that
	** one stays.
	*/
	key = alloca(fp->f_namelen + 1);
	for (i = 0, r = fp->f_name, s = key; i < fp->f_namelen; i++, r++, s++) {
		if (r[0] == '\\') {
			if (r[1] == '\\') {
				*s = '\\';
				r++;
			} else {
				*s = '/';
			}
		} else {
			*s = *r;
		}
	}
	*s = (char)0;

	/*
	** Check for attributes and set them.  Attributes look like:
	**	name(key1=value1,key2=value2,. . .)
	** attributes can be used to limit the scope of the search.
	** Only callouts that match all these attributes will be used
	** to locate the data.
	*/
	r = strchr(key, '(');
	if (r) {
		*r++ = (char)0;
		nsd_attr_parse(&fp->f_attrs, r, 0, (char *)0);

		/* There are some attributes we do not allow to be changed. */
		nsd_attr_delete(&fp->f_attrs, "table");
		nsd_attr_delete(&fp->f_attrs, "file");
		nsd_attr_delete(&fp->f_attrs, "local");
		nsd_attr_delete(&fp->f_attrs, "SGI_MAC_FILE");
		nsd_attr_delete(&fp->f_attrs, "SGI_CAP_FILE");
	}

	/*
	** If we are looking up the magic .all file then we go through
	** a different library routine.
	*/
	if (strcmp(key, NS_ALL_FILE) == 0) {
		fp->f_index = LIST;
		fp->f_timeout = nsd_attr_fetch_long(fp->f_attrs, 
						    "list_timeout", 
						    10, nsd_timeout/1000);
	} else {
		fp->f_index = LOOKUP;
		fp->f_timeout = nsd_attr_fetch_long(fp->f_attrs, 
						    "lookup_timeout", 
						    10, nsd_timeout/1000);
	}

	time(&now);
	if (fp->f_timeout < 0) {
		fp->f_timeout = now;
	} else {
		fp->f_timeout += now;
	}

	/*
	** If our parent is a callout then we need to duplicate it so as
	** not to create a loop in the file structure.  Otherwise we just
	** copy our parents callout list.
	*/
	if (dp->f_lib && dp->f_lib->l_funcs[fp->f_index]) {
		cp = nsd_file_dup(dp, fp);
		nsd_attr_continue(&cp->f_attrs, dp->f_attrs);
		nsd_file_appendcallout(fp, cp, cp->f_name, cp->f_namelen);
	} else {
		nsd_file_copycallouts(dp, fp, 0);
	}

	nsd_attr_store(&fp->f_attrs, "key", key);
	if (fp->f_index != LOOKUP) {
		nsd_attr_store(&fp->f_attrs, "timeout", "0");
	}

	/*
	** Walk the callout list looking for the first good callout.
	** We return the results of the first one.  If this returns
	** an error then it is up to our caller to continue walking
	** through the list.
	*/
	for (cp = nsd_file_getcallout(fp); cp; cp = nsd_file_nextcallout(fp)) {
		if (cp->f_lib && cp->f_lib->l_funcs[fp->f_index]) {
			nsd_attr_continue(&fp->f_attrs, cp->f_attrs);
			fp->f_send = nfs3_lookup_reply;
			fp->f_sender = (void *)sd;
			if (nsd_file_appenddir(dp, fp, fp->f_name,
			    fp->f_namelen) != NSD_OK) {
				nsd_logprintf(NSD_LOG_LOW,
			    "nfs3_lookup: failed appenddir for %s: try again\n",
				    fp->f_name);
				nsd_file_clear(&fp);
				return nfs3_err(sd, NFS3ERR_IO, 1, dp);
			}
			*rqp = fp;
			return (*cp->f_lib->l_funcs[fp->f_index])(fp);
		}
	}

	/*
	** No callouts defined with the proper function enabled.
	*/
	nsd_file_clear(&fp);
	return nfs3_err(sd, NFS3ERR_NOENT, 1, dp);
}

/* ARGSUSED */
static int
nfs3_access(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint32_t check, access=0;
	uint64_t fh[8];
	nsd_file_t *fp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_access:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	p += len/sizeof(*p);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}
	check = ntohl(*p);

	if (nfs_check(sd->cred, fp, OP_READ) == NSD_OK) {
		access |= ACCESS3_READ;
	}
	if (nfs_check(sd->cred, fp, OP_WRITE) == NSD_OK) {
		access |= ACCESS3_MODIFY;
		access |= ACCESS3_EXTEND;
		if (fp->f_type == NFDIR) {
			access |= ACCESS3_DELETE;
		}
	}
	if (nfs_check(sd->cred, fp, OP_EXECUTE) == NSD_OK) {
		if (fp->f_type == NFDIR) {
			access |= ACCESS3_LOOKUP;
		} else {
			access |= ACCESS3_EXECUTE;
		}
	}
	if ((nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		access = 0;
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** file attributes
	*/
	*p++ = htonl(TRUE);
	p += nfs3_attrs(fp, p);

	/*
	** access
	*/
	*p++ = htonl(access & check);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_readlink(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_file_t *fp;
	uint64_t fh[8];
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_readlink:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}

	/*
	** We only return the data in links here.
	*/
	if (fp->f_type != NFLNK) {
		return nfs3_err(sd, NFS3ERR_INVAL, 1, (nsd_file_t *)0);
	}

	/*
	** Check permissions.
	*/
	if ((nfs_check(sd->cred, fp, OP_LIST) != NSD_OK) ||
	    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs3_err(sd, NFS3ERR_ACCES, 1, fp);
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** file attributes
	*/
	*p++ = htonl(TRUE);
	p += nfs3_attrs(fp, p);

	/*
	** data
	*/
	p[fp->f_used/sizeof(*p)] = 0;
	memcpy(p, fp->f_data, fp->f_used);
	p += WORDS(fp->f_used);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_read(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t offset;
	uint32_t len, count, max;
	nsd_file_t *fp, *cp;
	uint64_t fh[8];

	nsd_logprintf(NSD_LOG_LOW, "nfs3_read:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	p += (len/sizeof(*p));

	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}

	memcpy(&offset, p, sizeof(offset));
	offset = ntohll(offset);
	p += 2;

	count = ntohl(*p);

	/*
	** Verify that the file is still good.
	*/
	if ((fp->f_type == NFREG) && (cp = nsd_file_getcallout(fp)) &&
	    cp->f_lib && cp->f_lib->l_funcs[VERIFY] &&
	    ((*cp->f_lib->l_funcs[VERIFY])(fp, sd->cred) != NSD_OK)) {
		cp = nsd_file_byid(PARENT(fp->f_fh));
		if (cp) {
			nsd_file_delete(cp, fp);
		}
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}

	/*
	** Check permissions.
	*/
	if ((nfs_check(sd->cred, fp, OP_READ) != NSD_OK) ||
	    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs3_err(sd, NFS3ERR_ACCES, 1, fp);
	}

	/*
	** Build result.
	*/
	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** file attributes
	*/
	*p++ = htonl(TRUE);
	p += nfs3_attrs(fp, p);

	/*
	** nfsdata
	**
	** we send the least of our current buffer space, the requested count,
	** and the data left.
	*/
	max = NFS_BUF_SIZE - ((p - nfs_buf) * sizeof(*p)) - (2 * sizeof(*p));
	if (offset > fp->f_used) {
		len = 0;
	} else if (count < (fp->f_used - offset)) {
		if (count < max) {
			len = count;
		} else {
			len = max;
		}
	} else if ((fp->f_used - offset) < max) {
		len = fp->f_used - offset;
	} else {
		len = max;
	}

	*p++ = htonl(len);	/* count */
	if (len <= count) {
		*p++ = htonl(TRUE);	/* eof */
	} else {
		*p++ = htonl(FALSE);
	}
	*p++ = htonl(len);
	if (len > 0) {
		p[len/sizeof(*p)] = 0;
		memcpy(p, fp->f_data + offset, len);
	}
	p += WORDS(len);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_write(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_write:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, (nsd_file_t *)0, 
		    (nsd_file_t *)0);
	}

	nfs3_err(sd, NFS3ERR_ROFS, 2, (nsd_file_t *)0, fp);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_create(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_create:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}

	nfs3_err(sd, NFS3ERR_ROFS, 2, (nsd_file_t *)0, fp);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_mkdir(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *pp, *dp;
	int len, i;
	char *q;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_mkdir:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	pp = nsd_file_byhandle(fh);
	if (! pp || (pp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}

	/*
	** Try to find the name in the handle
	*/
	p += len/sizeof(*p);
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_HIGH,
			      "\treturning NFSERR_NAMETOOLONG\n");
		return nfs3_err(sd, NFS3ERR_NAMETOOLONG, 2, pp, 
				(nsd_file_t *) 0);
	}
	
	/*
	** Ignore any attributes from table name by not including them
	** in the length. 
	*/
	for (i = len, len = 0, q = (char *)p;
	     len < i  && q && *q != '(' && *q != '[' &&  !isspace(*q);
	     len++, q++);

	nsd_logprintf(NSD_LOG_HIGH, "\tname: %*.*s \n", len, len, (char *)p);

	/*
	** Lookup the name.
	*/
	if ((dp = nsd_file_byname(pp->f_data, (char *)p, len)) ||
	    (dp = nsd_file_byname(pp->f_callouts, (char *)p, len))){

		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_EXIST\n");
		return nfs3_err(sd, NFS3ERR_EXIST, 2, (nsd_file_t *) 0,
				(nsd_file_t *) 0);
	}

	/*
	** The directory does not exist.  If the parent directory allows for 
	** mkdirs, mkdir.
	*/
	if (pp->f_flags & NSD_FLAGS_MKDIR) {
		
		dp = nsd_file_mkdir(pp, (char *) p, len, sd->cred);
		if (!dp) {
			return nfs3_err(sd, NFS3ERR_SERVERFAULT, 2, pp,
					(nsd_file_t *) 0);
		}

		/* Tag this directory as DYNAMIC */
		dp->f_flags |= NSD_FLAGS_DYNAMIC;

		/*
		** Return results to client.
		*/
		p = nfs_buf;
		p += nfs_msg(sd, p);
		
		/*
		** file handle
		*/
		*p++ = ntohl(TRUE);
		*p++ = htonl(NFS3_FHSIZE);
		memcpy(p, dp->f_fh, NFS3_FHSIZE);
		p += (NFS3_FHSIZE/sizeof(uint32_t));
		
		/*
		** file attributes
		*/
		*p++ = htonl(TRUE);
		p += nfs3_attrs(dp, p);
		
		/*
		** Weak Cache data
		*/
		/* Before */
		*p++ = htonl(FALSE); 

		/* after  */
		*p++ = htonl(TRUE);
		p += nfs3_attrs(pp, p);

		tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0, 
				&sd->sin, sizeof(sd->sin), sd->lbl);
				
		reply_clear(&sd);

		return NSD_OK;
	}
			
	/*
	** Directory does not support dynamic directory creation
	*/
	nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_ROFS\n");
	nfs3_err(sd, NFS3ERR_ROFS, 2, (nsd_file_t *)0, pp);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_symlink(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_symlink:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}

	nfs3_err(sd, NFS3ERR_ROFS, 2, (nsd_file_t *)0, fp);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_mknod(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_mknod:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}

	nfs3_err(sd, NFS3ERR_ROFS, 2, (nsd_file_t *)0, fp);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_remove(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_file_t *dp, *fp;
	uint64_t fh[8];
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_remove:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "returning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, 0, 0);
	}
	memcpy(fh, p, len);
	sd->dir = FILEID(fh);
	dp = nsd_file_byid(sd->dir);
	if (! dp) {
		nsd_logprintf(NSD_LOG_HIGH, "returning NFS3ERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, 0, 0);
	}
	if (dp->f_type != NFDIR) {
		nsd_logprintf(NSD_LOG_HIGH, "returning NFS3ERR_NOTDIR\n");
		return nfs3_err(sd, NFS3ERR_NOTDIR, 2, dp, dp);
	}
	p += len/sizeof(*p);

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_HIGH, "returning NFS3ERR_NAMETOOLONG\n");
		return nfs3_err(sd, NFS3ERR_NAMETOOLONG, 2, dp, dp);
	}

	/* Look it up in tree. */
	if ((fp = nsd_file_byname(dp->f_callouts, (char *)p, len)) ||
	    (fp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		/*
		** Check permissions.
		*/
		if ((nfs_check(sd->cred, dp, OP_WRITE) != NSD_OK) ||
		    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
		    (! islocal(sd)))) {
			nsd_logprintf(NSD_LOG_HIGH,
			    "returning NFS3ERR_ACCES\n");
			return nfs3_err(sd, NFS3ERR_ACCES, 2, dp, dp);
		}

		p = nfs_buf;
		p += nfs_msg(sd, p);

		/* wcc.before */
		p += htonl(TRUE);
		*p++ = 0;
		*p++ = ntohl(dp->f_size);
		*p++ = htonl(dp->f_mtime.tv_sec); /* mtime */
		*p++ = htonl(dp->f_mtime.tv_usec);
		*p++ = htonl(dp->f_ctime.tv_sec); /* ctime */
		*p++ = htonl(dp->f_ctime.tv_usec);

		/*
		** We do not delete directories, instead we do a timeout.
		** Unless the directory was dynamically created; then
		** we delete it and it's cache file.
		*/
		switch (fp->f_type) {
		    case NFDIR:
			if (fp->f_flags & NSD_FLAGS_DYNAMIC) {    
				nsd_map_unlink(fp);
				nsd_file_delete(dp, fp);
			} else {
				nsd_map_flush(fp);
				nsd_file_timeout(&fp, 0);
			}
			break;
		    case NFREG:
			nsd_map_remove(fp);
			nsd_file_delete(dp, fp);
		}

		/* wcc.after */
		p += htonl(TRUE);
		p += nfs3_attrs(dp, p);

		tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
				&sd->sin, sizeof(sd->sin), sd->lbl);
		reply_clear(&sd);

		return NSD_OK;
	}

	nsd_logprintf(NSD_LOG_HIGH, "returning NFS3ERR_NOENT\n");
	return nfs3_err(sd, NFS3ERR_NOENT, 2, dp, dp);
}

/* ARGSUSED */
static int
nfs3_rename(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs3_rename:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	nfs3_err(sd, NFS3ERR_ROFS, 4, (nsd_file_t *)0, (nsd_file_t *)0,
	    (nsd_file_t *)0, (nsd_file_t *)0);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_link(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "nfs3_link:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	nfs3_err(sd, NFS3ERR_ROFS, 3, (nsd_file_t *)0, (nsd_file_t *)0,
	    (nsd_file_t *)0);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_readdir(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint32_t *entry=0;
	uint64_t count, cookie, verf, i;
	nsd_file_t *dp;
	char *start;
	uint64_t fh[8];
	u_char *s;
	uint32_t *t, len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_readdir:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	dp = nsd_file_byhandle(fh);
	if (! dp) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}
	if (dp->f_type != NFDIR) {
		return nfs3_err(sd, NFS3ERR_NOTDIR, 1, dp);
	}
	if ((nfs_check(sd->cred, dp, OP_READ) != NSD_OK) ||
	    (nsd_attr_fetch_bool(dp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs3_err(sd, NFS3ERR_ACCES, 1, dp);
	}
	p += (len/sizeof(*p));

	/*
	** Cookie is an offset into the list.  The first bit determines
	** which list.  0 = callouts, 1 = members
	*/
	memcpy(&cookie, p, sizeof(cookie));
	p += 2;

	memcpy(&verf, p, sizeof(verf));
	p += 2;

	if (cookie != 0 && verf != 0 &&
	    (memcmp(&verf, &dp->f_mtime, NFS3_COOKIEVERFSIZE) != 0)) {
		return nfs3_err(sd, NFS3ERR_BAD_COOKIE, 1, dp);
	}

	count = ntohl(*p);

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** directory attributes
	*/
	*p++ = htonl(TRUE);
	p += nfs3_attrs(dp, p);

	/*
	** cookieverf
	*/
	memcpy(p, &dp->f_mtime, NFS3_COOKIEVERFSIZE);
	p += NFS3_COOKIEVERFSIZE/sizeof(*p);

	start = (char *)p;
	if ((cookie < 0x80000000) && dp->f_callouts) {
		for (s = (u_char *)dp->f_callouts, i = 0;
		     *s && i < cookie;
		     s = (u_char *)(t + 1), i++) {
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
		}
		for (; *s; s = (u_char *)(t + 1), i++) {
			entry = p;
			t = (uint32_t *)s;
			t += WORDS(*s + 1);

			*p++ = htonl(TRUE);		/* pointer */

			*p++ = 0;			/* upper of fileid */
			*p++ = htonl(*t);		/* fileid */

			*p++ = htonl(*s);		/* name len */
			p[*s/sizeof(*p)] = 0;		/* null pad name */
			memcpy(p, s + 1, *s);		/* name */
			p += WORDS(*s);

			*p++ = (uint32_t)(i >> 32);	/* upper of cookie */
			*p++ = (uint32_t)(i);		/* lower of cookie */

			if ((((char *)p) - start) > count) {
				p = entry;
				break;
			}
		}
	}
	if ((p != entry) && dp->f_data) {
		for (s = (u_char *)dp->f_data, i = 0x80000000;
		     *s && i < cookie;
		     s = (u_char *)(t + 1), i++) {
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
		}
		for (; *s; s = (u_char *)(t + 1), i++) {
			entry = p;
			t = (uint32_t *)s;
			t += WORDS(*s + 1);

			*p++ = htonl(TRUE);		/* pointer */

			*p++ = 0;			/* upper of fileid */
			*p++ = htonl(*t);		/* fileid */

			*p++ = htonl(*s);		/* name len */
			p[*s/sizeof(*p)] = 0;		/* null pad name */
			memcpy(p, s + 1, *s);		/* name */
			p += WORDS(*s);

			*p++ = (uint32_t)(i >> 32);	/* upper of cookie */
			*p++ = (uint32_t)(i);		/* lower of cookie */

			if ((((char *)p) - start) > count) {
				p = entry;
				break;
			}
		}
	}
	*p++ = 0;				/* pointer */
	if (s && *s) {
		*p++ = htonl(0);		/* eof - no */
	} else {
		*p++ = htonl(1);		/* eof - yes */
	}

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_readdirplus(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint32_t dircount, maxcount, *entry=0;
	uint64_t cookie, verf, i, count=0;
	nsd_file_t *fp, *dp;
	char *start;
	uint64_t fh[8];
	u_char *s;
	uint32_t *t, len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_readdirplus:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	len = ntohl(*p++);
	if (len != NFS_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	dp = nsd_file_byhandle(fh);
	if (! dp) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}
	if (dp->f_type != NFDIR) {
		return nfs3_err(sd, NFS3ERR_NOTDIR, 1, dp);
	}
	if ((nfs_check(sd->cred, dp, OP_READ) != NSD_OK) ||
	    (nsd_attr_fetch_bool(dp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs3_err(sd, NFS3ERR_ACCES, 1, dp);
	}
	p += len/sizeof(*p);

	/*
	** Cookie is an offset into the list.  The first bit determines
	** which list.  0 = callouts, 1 = members
	*/
	memcpy(&cookie, p, sizeof(cookie));
	p += 2;

	memcpy(&verf, p, sizeof(verf));
	p += 2;
	if (cookie != 0 && verf != 0 &&
	    (memcmp(&verf, &dp->f_mtime, NFS3_COOKIEVERFSIZE) != 0)) {
		return nfs3_err(sd, NFS3ERR_BAD_COOKIE, 1, dp);
	}

	dircount = ntohl(p[0]);
	maxcount = ntohl(p[1]);

	p = nfs_buf;
	p += nfs_msg(sd, p);

	start = (char *)p;
	/*
	** directory attributes
	*/
	*p++ = htonl(TRUE);
	p += nfs3_attrs(dp, p);

	/*
	** cookieverf
	*/
	memcpy(p, &dp->f_mtime, NFS3_COOKIEVERFSIZE);
	p += NFS3_COOKIEVERFSIZE/sizeof(*p);

	if ((cookie < 0x80000000) && dp->f_callouts) {
		for (s = (u_char *)dp->f_callouts, i = 0;
		     *s && (i < cookie);
		     s = (u_char *)(t + 1), i++) {
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
		}
		for (; *s; s = (u_char *)(t + 1), i++) {
			entry = p;
			t = (uint32_t *)s;
			t += WORDS(*s + 1);

			*p++ = htonl(TRUE);		/* pointer */

			*p++ = 0;			/* upper of fileid */
			*p++ = htonl(*t);		/* fileid */

			*p++ = htonl(*s);		/* name len */
			p[*s/sizeof(*p)] = 0;		/* null pad name */
			memcpy(p, s + 1, *s);		/* name */
			p += WORDS(*s);

			*p++ = 0;			/* upper of cookie */
			*p++ = (uint32_t)(i);		/* lower of cookie */

			fp = nsd_file_byid(*t);
			if (! fp) {
				p = entry;
				continue;
			}

			count += (p - entry) * sizeof(*p);
			if (count > dircount) {
				p = entry;
				break;
			}

			/*
			** file attributes
			*/
			*p++ = htonl(TRUE);
			p += nfs3_attrs(fp, p);

			/*
			** file handle
			*/
			*p++ = htonl(TRUE);
			*p++ = htonl(NFS3_FHSIZE);
			memcpy(p, fp->f_fh, NFS3_FHSIZE);
			p += NFS3_FHSIZE/sizeof(*p);

			if ((((char *)p) - start) > maxcount) {
				p = entry;
				break;
			}
		}
	}

	if ((p != entry) && dp->f_data) {
		for (s = (u_char *)dp->f_data, i = 0x80000000;
		    *s && i < cookie; s = (u_char *)(t + 1), i++) {
			t = (uint32_t *)s;
			t += WORDS(*s + 1);
		}
		for (; *s; s = (u_char *)(t + 1), i++) {
			entry = p;
			t = (uint32_t *)s;
			t += WORDS(*s + 1);

			*p++ = htonl(TRUE);		/* pointer */

			*p++ = 0;			/* upper of fileid */
			*p++ = htonl(*t);		/* fileid */

			*p++ = htonl(*s);		/* name len */
			p[*s/sizeof(*p)] = 0;		/* null pad name */
			memcpy(p, s + 1, *s);		/* name */
			p += WORDS(*s);

			*p++ = 0;			/* upper of cookie */
			*p++ = (uint32_t)(i);		/* lower of cookie */

			fp = nsd_file_byid(*t);
			if (! fp) {
				p = entry;
				continue;
			}

			count += (p - entry) * sizeof(*p);
			if (count > dircount) {
				p = entry;
				break;
			}

			/*
			** file attributes
			*/
			*p++ = htonl(TRUE);
			p += nfs3_attrs(fp, p);

			/*
			** file handle
			*/
			*p++ = htonl(TRUE);
			*p++ = htonl(NFS3_FHSIZE);
			memcpy(p, fp->f_fh, NFS3_FHSIZE);
			p += NFS3_FHSIZE/sizeof(*p);

			if ((((char *)p) - start) > maxcount) {
				p = entry;
				break;
			}
		}
	}

	*p++ = htonl(FALSE);				/* pointer */

	if (s && *s) {
		*p++ = htonl(FALSE);		/* eof - no */
	} else {
		*p++ = htonl(TRUE);		/* eof - yes */
	}

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_fsstat(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	uint32_t len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_fsstat:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** post_op_attr
	*/
	*p++ + htonl(TRUE);
	p += nfs3_attrs(fp, p);

	*p++ = 0; *p++ = 0;					/* tbytes */
	*p++ = 0; *p++ = 0;					/* fbytes */
	*p++ = 0; *p++ = 0;					/* abytes */
	*p++ = 0; *p++ = 0;					/* tfiles */
	*p++ = 0; *p++ = 0;					/* ffiles */
	*p++ = 0; *p++ = 0;					/* afiles */
	*p++ = 0;						/* invarsec */

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_fsinfo(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	uint32_t len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_fsinfo:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** post_op_attr
	*/
	*p++ = htonl(TRUE);
	p += nfs3_attrs(fp, p);

	*p++ = htonl(32768);				/* rtmax */
	*p++ = htonl(32768);				/* rtpref */
	*p++ = htonl(32768);				/* rtmult */
	*p++ = htonl(32768);				/* wtmax */
	*p++ = htonl(32768);				/* wtpref */
	*p++ = htonl(32768);				/* wtmult */
	*p++ = htonl(32768);				/* dtpref */
	*p++ = htonl(0xffffffff);			/* maxfilesize */
	*p++ = htonl(0xffffffff);
	*p++ = 0;					/* time_delta */
	*p++ = htonl(1);
	*p++ = htonl(FSF3_SYMLINK | FSF3_LINK | FSF_HOMOGENEOUS); /*properties*/

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_pathconf(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	uint32_t len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_pathconf:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 1, (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 1, (nsd_file_t *)0);
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	/*
	** post_op_attr
	*/
	*p++ = htonl(TRUE);
	p += nfs3_attrs(fp, p);

	*p++ = htonl(65536);		/* linkmax */
	*p++ = htonl(MAXNAMELEN);	/* name_max */
	*p++ = htonl(FALSE);		/* no_trunc */
	*p++ = htonl(TRUE);		/* chown_restricted */
	*p++ = htonl(TRUE);		/* case_insensitive */
	*p++ = htonl(TRUE);		/* case_preserving */

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
nfs3_commit(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nfs3_commit:\n");

	if (! sd) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFS3ERR_BADHANDLE\n");
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NFSERR_STALE\n");
		return nfs3_err(sd, NFS3ERR_STALE, 2, (nsd_file_t *)0,
		    (nsd_file_t *)0);
	}

	nfs3_err(sd, NFS3ERR_ROFS, 2, (nsd_file_t *)0, fp);

	return NSD_OK;
}

/* ARGSUSED */
static int
xattr1_null(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "xattr1_null:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	p = nfs_buf;
	p += nfs_msg(sd, p);

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
xattr1_getxattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp;
	char *name;
	uint32_t len, maxlen;
	nsd_attr_t *na;

	nsd_logprintf(NSD_LOG_LOW, "xattr1_getxattr:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 0);
	}
	p += (len/sizeof(*p));

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		return nfs_err(sd, NFSERR_NAMETOOLONG);
	}
	name = (char *)alloca(len + 1);
	memcpy(name, p, len);
	name[len] = (char)0;
	nsd_logprintf(NSD_LOG_HIGH, "\tname: %s\n", name);
	p += WORDS(len);

	maxlen = ntohl(*p++);

	/*
	** Check permissions.
	*/
	if ((nfs_check(sd->cred, fp, OP_READ) != NSD_OK) ||
	    (nsd_attr_fetch_bool(fp->f_attrs, "local", FALSE) &&
	    (! islocal(sd)))) {
		return nfs3_err(sd, NFS3ERR_ACCES, 0);
	}

	/*
	** Build result.
	*/
	p = nfs_buf;
	p += nfs_msg(sd, p);

	na = nsd_attr_fetch(fp->f_attrs, name);
	if (na) {
		nsd_logprintf(NSD_LOG_HIGH, "\tvalue = %s\n", na->a_value);

		/*
		** post_op_attr
		*/
		*p++ = htonl(TRUE);
		p += nfs3_attrs(fp, p);

		if (! na->a_data) {
			/*
			** Gross hack.  A number of the attributes are
			** expected to be in a binary format.
			*/
			if (strcmp(name, "SGI_ACL_FILE") == 0) {
				struct acl *list;

				/*
				** Access list.
				*/
				list = acl_from_text(na->a_value);
				if (! list) {
					return nfs3_err(sd, ENOATTR, 1, fp);
				}
				nsd_attr_data_set(na, list, acl_size(list),
				    free);
			} else if (strcmp(name, "SGI_ACL_DEFAULT") == 0) {
				struct acl *list;

				/*
				** Access list.
				*/
				list = acl_from_text(na->a_value);
				if (! list) {
					return nfs3_err(sd, ENOATTR, 1, fp);
				}
				nsd_attr_data_set(na, list, acl_size(list),
				    free);
			} else if (strcmp(name, "SGI_CAP_FILE") == 0) {
				cap_t cap;

				/*
				** Capability.
				*/
				cap = cap_from_text(na->a_value);
				if (! cap) {
					return nfs3_err(sd, ENOATTR, 1, fp);
				}
				nsd_attr_data_set(na, cap, cap_size(cap), free);
			} else if (strcmp(name, "SGI_MAC_FILE") == 0) {
				mac_t mac;

				/*
				** MAC label.
				*/
				mac = mac_from_text(na->a_value);
				if (! mac) {
					return nfs3_err(sd, ENOATTR, 1, fp);
				}
				nsd_attr_data_set(na, mac, mac_size(mac), free);
			}
		}

		if (na->a_size > maxlen) {
			return nfs3_err(sd, NFS3ERR_TOOSMALL, 1, fp);
		}
		*p++ = htonl(na->a_size);
		p[len/sizeof(*p)] = 0;
		if (na->a_data) {
			memcpy(p, na->a_data, na->a_size);
		} else {
			memcpy(p, na->a_value, na->a_size);
		}
		p += WORDS(na->a_size);
	} else {
		return nfs3_err(sd, ENOATTR, 1, fp);
	}

	tsix_sendto_mac(sd->fd, nfs_buf, (p - nfs_buf)*sizeof(*p), 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

/* ARGSUSED */
static int
xattr1_setxattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "xattr1_setxattr:\n");

	nfs3_err(sd, NFS3ERR_ROFS, 0);

	return NSD_OK;
}

/* ARGSUSED */
static int
xattr1_rmxattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_LOW, "xattr1_rmxattr:\n");

	nfs3_err(sd, NFS3ERR_ROFS, 0);

	return NSD_OK;
}

/*
** The format of a list block is:
**	count last? offset[0] offset[1] --->
**	<--- len[1] name[1] len[0] name[0] cursor
*/
/* ARGSUSED */
static int
xattr1_listxattr(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	int len, count;
	uint32_t maxbytes, flags, cursor[4], *start, *last, *end;
	nsd_attr_t *ap;
	nsd_file_t *fp;

	nsd_logprintf(NSD_LOG_LOW, "xattr1_listxattr:\n");

	if (! sd) {
		return NSD_ERROR;
	}

	/*
	** Try to find file handle in lists.
	*/
	len = ntohl(*p++);
	if (len != NFS3_FHSIZE) {
		return nfs3_err(sd, NFS3ERR_BADHANDLE, 0);
	}
	memcpy(fh, p, len);
	fp = nsd_file_byhandle(fh);
	if (! fp || (fp->f_status != NS_SUCCESS)) {
		return nfs3_err(sd, NFS3ERR_STALE, 0);
	}
	p += (len/sizeof(*p));

	maxbytes = ntohl(*p++);
	flags = ntohl(*p++);

	/*
	** Cursor looks like:
	**	mtime(seconds), count, 0, 0
	*/
	memcpy(cursor, p, sizeof(cursor));
	nsd_logprintf(NSD_LOG_HIGH,
	    "\tmaxbytes: %u, flags: 0x%x, count: %u, time: 0x%x\n",
	    maxbytes, flags, cursor[1], cursor[0]);
	if (cursor[1] != (uint32_t)fp->f_mtime.tv_sec) {
		cursor[0] = 0;
	}

	/*
	** Setup reply.
	*/
	memset(nfs_buf, 0, NFS_BUF_SIZE);
	p = nfs_buf;
	p += nfs_msg(sd, p);

	*p++ = htonl(TRUE);
	p += nfs3_attrs(fp, p);

	if (maxbytes > 8000) {
		maxbytes = 8000;
	}
	last = &nfs_buf[(maxbytes/sizeof(*nfs_buf)) - 4];
	end = last - 1;
	*p++ = (end - p + 1) * sizeof(*start);
	start = p;
	p += 2;
	for (count = 0, ap = fp->f_attrs; ap; ap = ap->a_next) {
		if (! ap->a_key) {
			continue;
		}
		if (count++ < cursor[1]) {
			continue;
		}
		len = strlen(ap->a_key) + 1;
		end -= WORDS(len) + 1;
		if (end <= p) {
			break;
		}
		nsd_logprintf(NSD_LOG_HIGH,
		    "\tadding: %s, len: %d\n", ap->a_key, len);
		*end = len;
		memcpy(end + 1, ap->a_key, len);
		*p++ = (end - start) * sizeof(*start);
	}
	start[0] = count;
	start[1] = (ap) ? 1 : 0;
	last[0] = (uint32_t)fp->f_mtime.tv_sec;
	last[1] = count;
	nsd_logprintf(NSD_LOG_HIGH, "\t%d elements returned\n", count);

	tsix_sendto_mac(sd->fd, nfs_buf, maxbytes, 0,
			&sd->sin, sizeof(sd->sin), sd->lbl);
	reply_clear(&sd);

	return NSD_OK;
}

int
nfs2_dispatch(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	static nfs_proc *nfs2[] = { nfs2_null, nfs2_getattr, nfs2_setattr,
	    nfs2_null, nfs2_lookup, nfs2_readlink, nfs2_read, nfs2_writecache,
	    nfs2_write, nfs2_create, nfs2_remove, nfs2_rename, nfs2_link,
	    nfs2_symlink, nfs2_mkdir, nfs2_remove, nfs2_readdir, nfs2_statfs };

	if (sd->proc >= sizeof(nfs2)) {
		return accept_err(sd, PROC_UNAVAIL);
	}

	return (*nfs2[sd->proc])(rqp, sd, p);
}

int
nfs3_dispatch(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	static nfs_proc *nfs3[] = { nfs3_null, nfs3_getattr, nfs3_setattr,
	    nfs3_lookup, nfs3_access, nfs3_readlink, nfs3_read, nfs3_write,
	    nfs3_create, nfs3_mkdir, nfs3_symlink, nfs3_mknod, nfs3_remove,
	    nfs3_remove, nfs3_rename, nfs3_link, nfs3_readdir, nfs3_readdirplus,
	    nfs3_fsstat, nfs3_fsinfo, nfs3_pathconf, nfs3_commit };
	
	if (sd->proc >= sizeof(nfs3)) {
		return accept_err(sd, PROC_UNAVAIL);
	}

	return (*nfs3[sd->proc])(rqp, sd, p);
}

int
xattr1_dispatch(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	static nfs_proc *xattr1[] = { xattr1_null, xattr1_getxattr,
	    xattr1_setxattr, xattr1_rmxattr, xattr1_listxattr };
	
	if (sd->proc >= sizeof(xattr1)) {
		return accept_err(sd, PROC_UNAVAIL);
	}

	return (*xattr1[sd->proc])(rqp, sd, p);
}

int
nsd_dispatch(nsd_file_t **rqp, nfs_reply_t *sd, uint32_t *p)
{
	uint64_t fh[8];
	nsd_file_t *fp, *dp;
	int len;

	nsd_logprintf(NSD_LOG_LOW, "nsd_dispatch:\n");

	switch (sd->proc) {
	    case NSDPROC1_NULL:
		return nfs3_null(rqp, sd, p);
	    case NSDPROC1_CLOSE:
		/* Try to find file handle in lists. */
		len = ntohl(*p++);
		if (len != NFS3_FHSIZE) {
			nsd_logprintf(NSD_LOG_HIGH,
			    "\treturning NFS3ERR_BADHANDLE: size=%d\n",
			    len);
			return nfs3_err(sd, NFS3ERR_BADHANDLE, 0);
		}
		memcpy(fh, p, len);
		nsd_logprintf(NSD_LOG_MAX, "\tfileid: %lld\n", FILEID(fh));
		fp = nsd_file_byhandle(fh);
		if (! fp) {
			nsd_logprintf(NSD_LOG_HIGH,
			    "\treturning NFSERR_STALE\n");
			return nfs3_err(sd, NFS3ERR_STALE, 0);
		}

		/* Delete file. */
		if (fp->f_timeout && fp->f_type != NFINPROG) {
			dp = nsd_file_byid(PARENT(fp->f_fh));
			if (dp) {
				nsd_file_delete(dp, fp);
			} else {
				nsd_logprintf(NSD_LOG_HIGH,
				    "\tparent no longer exists\n");
				nsd_file_clear(&fp);
			}
		}
		
		return nfs3_null(rqp, sd, p);
	    default:
		return NSD_ERROR;
	}
}

/*
** This routine reads and parses an incoming RPC request and hands it
** off to one of the above functions to do the actual work.
*/
static int
nfs_dispatch(nsd_file_t **rqp, int fd)
{
	uint32_t *p, program, version;
	nfs_reply_t *sd;
	int len, i, n;

	*rqp = 0;
	if (! nfs_buf) {
		nfs_buf = (uint32_t *)valloc(NFS_BUF_SIZE);
		if (! nfs_buf) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nfs_dispatch: failed malloc\n");
			return NSD_ERROR;
		}
	}

	sd = (nfs_reply_t *)nsd_calloc(1, sizeof(nfs_reply_t));
	if (! sd) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nfs_dispatch: failed malloc\n");
		return NSD_ERROR;
	}

	len = sizeof(sd->sin);
	n = tsix_recvfrom_mac(fd, nfs_buf, 8192, 0, &sd->sin, &len, &sd->lbl);
	if (n < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nfs_dispatch: bad read: %s\n", strerror(errno));
		reply_clear(&sd);
		return NSD_ERROR;
	}

	sd->fd = fd;

	/*
	** Now we parse the request into the request structure.
	*/
	p = nfs_buf;

	sd->xid = ntohl(p[0]);
	if (ntohl(p[1]) != CALL) {
		return reject_err(sd, RPC_MISMATCH);
	}
	if (ntohl(p[2]) != RPC_MSG_VERSION) {
		return reject_err(sd, RPC_MISMATCH);
	}
	program = ntohl(p[3]);
	if ((program != NFS_PROGRAM) && (program != XATTR_PROGRAM) &&
	    (program != NSDPROGRAM)) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning PROG_MISMATCH\n");
		return accept_err(sd, PROG_MISMATCH);
	}
	version = ntohl(p[4]);
	if ((version != NFS_VERSION) && (version != NFS3_VERSION) &&
	    (version != XATTR1_VERSION) && (version != NSDVERSION)) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning PROG_UNAVAIL\n");
		return accept_err(sd, PROG_UNAVAIL);
	}
	sd->proc = ntohl(p[5]);
	p += 6;

	/*
	** Check credentials.
	*/
	switch (ntohl(*p)) {
	    case AUTH_NONE:
		sd->cred = nsd_cred_new(NFS_UID_NOBODY, 1, NFS_GID_NOBODY);
		if (! sd->cred) {
			return accept_err(sd, SYSTEM_ERROR);
		}
		break;
	    case AUTH_UNIX:
		p += WORDS(ntohl(p[3]));
		p += 4;
		sd->cred = nsd_cred_new((uid_t)ntohl(p[0]), 0);
		if (! sd->cred) {
			return accept_err(sd, SYSTEM_ERROR);
		}
		sd->cred->c_gid[0] = (gid_t)ntohl(p[1]);
		sd->cred->c_gids = ntohl(p[2]);
		for (i = 0; i < sd->cred->c_gids; i++) {
			sd->cred->c_gid[i+1] = (gid_t)ntohl(p[i+3]);
		}
		p += (i + 3);
		break;
	    case AUTH_DES:
		nsd_logprintf(NSD_LOG_OPER, 
		    "nfs_dispatch: received AUTH_DES authorization\n");
		return reject_err(sd, AUTH_FAILED);
	    default:
		nsd_logprintf(NSD_LOG_OPER, 
		    "nfs_dispatch: received unknown authorization type: 0x%x\n",
		    ntohl(*p));
		return reject_err(sd, AUTH_FAILED);
	}

	/*
	** Skip Verifier.
	*/
	p += (2 + (ntohl(p[1]) / sizeof(uint32_t)));

	/*
	** Call appropriate dispatch routine.
	*/
	if (program == XATTR_PROGRAM) {
		return xattr1_dispatch(rqp, sd, p);
	}
	if (program == NSDPROGRAM) {
		return nsd_dispatch(rqp, sd, p);
	}
	if (version == NFS_VERSION) {
		return nfs2_dispatch(rqp, sd, p);
	} else {
		return nfs3_dispatch(rqp, sd, p);
	}
}

/*
** This is the init routine for the NFS protocol.  We simply open up a
** socket and register it with the global callback handler.  This is only
** called once during the startup of the daemon.
*/
int
nsd_nfs_init(void)
{
	int len=sizeof(struct sockaddr_in);

	_nfs_sin.sin_family = AF_INET;
	_nfs_sin.sin_port = 0;
	_nfs_sin.sin_addr.s_addr = INADDR_ANY;
	memset(_nfs_sin.sin_zero, 0, sizeof(_nfs_sin.sin_zero));

	if (((_nfs_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	    || (tsix_on(_nfs_sock) != 0)
	    || (bind(_nfs_sock, &_nfs_sin, len) != 0)
	    || (getsockname(_nfs_sock, &_nfs_sin, &len) != 0) ) {
		close(_nfs_sock);
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nfs_init: failed setup socket: %s\n", strerror(errno));
		return NSD_ERROR;
	}
	nsd_logprintf(NSD_LOG_MIN, "Added nfs support on port: %d\n",
	    _nfs_sin.sin_port);

	nsd_callback_new(_nfs_sock, nfs_dispatch, NSD_READ);

	return NSD_OK;
}
