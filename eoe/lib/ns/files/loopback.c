/*
** This file contains code to implement a simple generic loopback function
** to use in the files routines.  Several of the files support +/- escapes
** in them which can occasionally only be done using reintrant calls to the
** name server.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_files.h"

file_data_t *file_data_init(nsd_file_t *rq);

static int pipes[2] = {-1, -1};

typedef struct file_request {
	nsd_file_t		*rq;
	nsd_callout_proc	*proc;
} file_request_t;

/*
** free a list of file_name_t's
*/
void
passwd_free_name(file_name_t *fdt)
{
	file_name_t *fdtn;

	while (fdt) {
		fdtn = fdt->next;
		free(fdt->value);
		free(fdt);
		fdt = fdtn;
	}
}

/*
** This routine just frees everything in a file_data_t structure.
*/
void
file_data_clear(file_data_t *fd)
{
	if (fd->answer && *(fd->answer)) free(fd->answer);

	if (fd->save) 		free(fd->save);
	if (fd->name) 		free(fd->name);
	if (fd->entry)		free(fd->entry);

	if (fd->group_list) 	passwd_free_name(fd->group_list);
	if (fd->name_list)	passwd_free_name(fd->name_list);
	if (fd->save_list)	passwd_free_name(fd->save_list);
	if (fd->dont_list)	passwd_free_name(fd->dont_list);

	free(fd);
}

/*
** Initialize file data.
*/
file_data_t *
file_data_init(nsd_file_t *rq)
{
	file_data_t *fd;

	fd = (file_data_t *)nsd_calloc(1, sizeof(file_data_t));
	if (! fd) {
		nsd_logprintf(NSD_LOG_RESOURCE, "file_data_init: malloc failed\n");
		return (file_data_t *) 0;
	}

	fd->entry = (char *) 0;
	fd->name = (char *) 0;
	fd->answer = (char *) 0;
	fd->save = (char *) 0;
	fd->name_list = (file_name_t *) 0;
	fd->group_list = (file_name_t *) 0;
	fd->save_list = (file_name_t *) 0;
	fd->dont_list = (file_name_t *) 0;
	fd->group_count = 0;
	fd->compat = 1;
	fd->nbufs = 0;
	fd->naked_plus = 0;

	fd->dom = file_domain_lookup(rq);
	if (! fd->dom) {
		free(fd);
		return (file_data_t *) 0;
	}

	fd->key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! fd->key) {
		free(fd);
		return (file_data_t *) 0;
	}

	fd->len = strlen(fd->key);

	fd->version = fd->dom->version;
	fd->offset = 0;

	return fd;
}

/*
** This routine reads the answer from the pipe and calls the appropriate
** routine.
*/
int
file_loop_callback(nsd_file_t **rqp, int fd)
{
	int len;
	nsd_file_t *rq, *fp;
	nsd_callout_proc *proc;
	file_request_t *sd;
	file_data_t *data;
	char *library;

	nsd_logprintf(NSD_LOG_MIN, "file_loop_callback:\n");
	*rqp = 0;

	len = read(fd, &fp, sizeof(&fp));
	if (len < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE, "file_loop_callback: failed read: %s\n",
		    strerror(errno));
		return NSD_ERROR;
	}

	sd = (file_request_t *)fp->f_sender;
	if (sd) {
		proc = sd->proc;
		rq = sd->rq;
		if (rq->f_cmd_data) {
			data = (file_data_t *)rq->f_cmd_data;
			if ((rq->f_status == NS_SUCCESS) && fp->f_data) {
				data->answer = fp->f_data;
				library = nsd_attr_fetch_string(fp->f_attrs,
								"library",
								"unknown");
				nsd_attr_store(&rq->f_attrs, 
					       "library_compat", library);
				fp->f_data = 0;
			} else {
				data->answer = "";
			}
		}

		free(sd);
		nsd_file_clear(&fp);

		if (proc) {
			*rqp = rq;
			return (*proc)(rq);
		}
	}

	nsd_file_clear(&fp);
	return NSD_CONTINUE;
}

/*
** This is the routine used to send a recursive request to nsd.
** It creates the request here and sends the address to it over the
** wire.  The daemon will fill it in then reply.
*/
int
file_loop_new(nsd_file_t *rq, nsd_callout_proc *proc, char *table, char *key)
{
	nsd_file_t *fp;
	file_request_t *sd;

	nsd_logprintf(NSD_LOG_LOW, "file_loop_new: %s %s\n", table, key);

	/*
	** Create a file for the life of this request.
	*/
	if (nsd_file_init(&fp, "recurse", sizeof("recurse"), rq, NFREG,
	    rq->f_cred) != NSD_OK) {
		nsd_logprintf(NSD_LOG_LOW, "\tfailed file init\n");
		return NSD_ERROR;
	}

	/*
	** Remember who we have to call when we are done.
	*/
	sd = (file_request_t *)nsd_malloc(sizeof(file_request_t));
	if (! sd) {
		nsd_file_clear(&fp);
		nsd_logprintf(NSD_LOG_LOW, "\tfailed malloc\n");
		return NSD_ERROR;
	}

	sd->rq = rq;
	sd->proc = proc;
	fp->f_sender = (void *)sd;

	nsd_attr_clear(fp->f_attrs);
	fp->f_attrs = nsd_attr_copy(rq->f_attrs);
	nsd_attr_store(&fp->f_attrs, "table", table);
	nsd_attr_store(&fp->f_attrs, "key", key);

	if (nsd_file_copycallouts(rq, fp, 0) != NSD_OK) {
		nsd_file_clear(&fp);
		nsd_logprintf(NSD_LOG_LOW, "\tfailed copycallouts\n");
		return NSD_ERROR;
	}

	/*
	** If we do not want to include the current callout then we
	** pop it off the head of the list.
	*/
	if (! nsd_file_nextcallout(fp)) {
		nsd_file_clear(&fp);
		nsd_logprintf(NSD_LOG_LOW, "\tfailed nextcallout\n");
		return NSD_ERROR;
	}

	fp->f_index = LOOKUP;

	/*
	** Send our request.
	*/
	if (write(pipes[1], &fp, sizeof(&fp)) < 0) {
		nsd_file_clear(&fp);
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "file_loop_new: failed write: %s\n", strerror(errno));
		    return NSD_ERROR;
	}

	return NSD_CONTINUE;
}

/*
** This routine sets up a file structure to request the given netgroup
** from the name service daemon.
*/
int
file_loop_netgroup(nsd_file_t *rq, nsd_callout_proc *proc, char *table, 
    char *netgroup, int len)
{
	nsd_file_t *dp, *fp;
	file_request_t *sd;
	file_data_t *fd;
	char *domain;

	nsd_logprintf(NSD_LOG_LOW, "file_loop_netgroup: %s %s\n", table,
	    netgroup);

	domain = nsd_attr_fetch_string(rq->f_attrs, "domain", ".local");
	if (nsd_file_find(domain, table, 0, netgroup, &dp) == NSD_OK) {
		if (dp->f_status == NS_SUCCESS && dp->f_type == NFREG) {
			/*
			** Results are already in our tree.  We just return it.
			*/
			fd = (file_data_t *)rq->f_cmd_data;
			fd->answer = (char *)nsd_malloc(dp->f_used);
			if (! fd->answer) {
				return NSD_ERROR;
			}
			memcpy(fd->answer, dp->f_data, dp->f_used);
			return NSD_OK;
		} else {
			return NSD_ERROR;
		}
	}

	if (! dp) {
		return NSD_ERROR;
	}

	if (nsd_file_init(&fp, netgroup, len, dp, NFREG, rq->f_cred) !=
	    NSD_OK) {
		return NSD_ERROR;
	}

	/*
	** Remember who we have to call when we are done.
	*/
	sd = (file_request_t *)nsd_malloc(sizeof(file_request_t));
	if (! sd) {
		nsd_file_clear(&fp);
		nsd_logprintf(NSD_LOG_RESOURCE, "\tfailed malloc\n");
		return NSD_ERROR;
	}

	sd->rq = rq;
	sd->proc = proc;
	fp->f_sender = (void *)sd;

	nsd_attr_clear(fp->f_attrs);
	fp->f_attrs = nsd_attr_copy(rq->f_attrs);
	nsd_attr_continue(&fp->f_attrs, dp->f_attrs);

	nsd_attr_store(&fp->f_attrs, "key", netgroup);

	if (nsd_file_copycallouts(dp, fp, 0) != NSD_OK) {
		nsd_file_clear(&fp);
		nsd_logprintf(NSD_LOG_LOW, "\tfailed copycallouts\n");
		return NSD_ERROR;
	}

	if (! nsd_file_getcallout(fp)) {
		nsd_file_clear(&fp);
		nsd_logprintf(NSD_LOG_LOW, "\tfailed getcallback\n");
		return NSD_ERROR;
	}

	fp->f_index = LOOKUP;

	/*
	** Send our request.
	*/
	if (write(pipes[1], &fp, sizeof(&fp)) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE, "file_loop_new: failed write: %s\n",
		    strerror(errno));
		    return NSD_ERROR;
	}

	return NSD_CONTINUE;
}

/*
** This is the routine which replies on a recursive request after the
** request has been filled.  We just write the transaction ID and the
** data over the pipe.
*/
int
file_loop_send(nsd_file_t *rq)
{
	if (! rq) {
		return NSD_ERROR;
	}

	if (write(pipes[0], &rq, sizeof(&rq)) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE, "file_loop_send: failed write: %s\n",
		    strerror(errno));
		return NSD_ERROR;
	}

	return NSD_OK;
}

/*
** This routine reads the request off of the wire and starts it on its
** way through the remaining callouts.
*/
int
file_loop_dispatch(nsd_file_t **rqp, int fd)
{
	int len;
	nsd_file_t *rq, *cp;

	nsd_logprintf(NSD_LOG_MIN, "file_loop_dispatch:\n");

	*rqp = 0;

	len = read(fd, &rq, sizeof(&rq));
	if (len < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE, "file_loop_dispatch: failed read: %s\n",
		    strerror(errno));
		return NSD_ERROR;
	}

	for (cp = nsd_file_getcallout(rq); cp; cp = nsd_file_nextcallout(rq)) {
		if (cp->f_lib && cp->f_lib->l_funcs[rq->f_index]) {
			nsd_attr_continue(&rq->f_attrs, cp->f_attrs);
			*rqp = rq;
			rq->f_send = file_loop_send;
			return (*cp->f_lib->l_funcs[rq->f_index])(rq);
		}
	}

	nsd_logprintf(NSD_LOG_LOW, "no callbacks supporting function\n");
	rq->f_status = NS_NOTFOUND;
	write(fd, &rq, sizeof(&rq));
	return NSD_CONTINUE;
}

/*
** This routine is called once on startup to open up our pipes and
** setup the callbacks on their file descriptors.
*/
int
file_loop_init(void)
{
	if (pipes[0] == -1) {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE, "file_loop_init: failed pipe: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}

		nsd_callback_new(pipes[0], file_loop_dispatch, NSD_READ);
		nsd_callback_new(pipes[1], file_loop_callback, NSD_READ);
	}

	return NSD_OK;
}
