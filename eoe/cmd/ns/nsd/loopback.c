/*
** This file contains a hunk of code to do loopbacks into nsd.  This
** passes the thread of control to a different lookup, then returns
** it to the original caller when finished.  We setup a pipe into
** nsd during on the first call, and pass state through the pipe.
** Our caller must deal with NSD_CONTINUE, and must free the resulting
** file.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ns_api.h>
#include <ns_daemon.h>

static int pipes[2] = {-1, -1};

typedef struct {
	nsd_file_t		*rq;
	nsd_callout_proc	*proc;
} loop_send_t;

/*
** This just writes the address of the file to one end of the pipe and
** falls back to the select loop.  The select will wakeup and the callback
** below will be called to start running in the other context again.
*/
static int
loop_send(nsd_file_t *rq)
{
	nsd_logprintf(NSD_LOG_LOW, "loop_send:\n");

	if (! rq) {
		return NSD_ERROR;
	}
	
	if (write(pipes[0], &rq, sizeof(&rq)) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE, "loop_send: failed write: %s\n",
		    strerror(errno));
		return NSD_ERROR;
	}
	
	return NSD_OK;
}

/*
** We pull the address of the file off of the pipe and call the proceedure
** we were passed in the nsd_loopback routine.
*/
static int
loop_callback(nsd_file_t **rqp, int fd)
{
	int len;
	nsd_file_t *rq, *fp;
	nsd_callout_proc *p;
	loop_send_t *sd;
	
	nsd_logprintf(NSD_LOG_MIN, "loop_callback:\n");
	
	*rqp = 0;
	
	len = read(fd, &fp, sizeof(&fp));
	if (len < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "loop_callback: failed read: %s\n", strerror(errno));
		return NSD_ERROR;
	}
	
	sd = fp->f_sender;
	if (! sd) {
		nsd_file_clear(&fp);
		return NSD_ERROR;
	}
	
	rq = sd->rq;
	p = sd->proc;
	
	free(sd);
	fp->f_sender = 0;
	fp->f_send = 0;
	
	rq->f_loop = fp;
	
	*rqp = rq;
	return (*p)(rq);
}

/*
** This routine pulls the address of the file off of the pipe and walks
** it through the callout list.  The callout list was setup by nsd_loopback
** below.
*/
static int
loop_dispatch(nsd_file_t **rqp, int fd)
{
	int len;
	nsd_file_t *rq, *cp;
	
	nsd_logprintf(NSD_LOG_MIN, "loop_dispatch:\n");
	
	*rqp = 0;
	
	len = read(fd, &rq, sizeof(&rq));
	if (len < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "loop_dispatch: failed read: %s\n", strerror(errno));
		return NSD_ERROR;
	}
	
	for (cp = nsd_file_getcallout(rq); cp; cp = nsd_file_nextcallout(rq)) {
		if (cp->f_lib && cp->f_lib->l_funcs[rq->f_index]) {
			nsd_attr_continue(&rq->f_attrs, cp->f_attrs);
			*rqp = rq;
			rq->f_send = loop_send;
			return (*cp->f_lib->l_funcs[rq->f_index])(rq);
		}
	}
	
	nsd_logprintf(NSD_LOG_LOW, "\tno callbacks supporting function\n");
	rq->f_status = NS_NOTFOUND;
	write(fd, &rq, sizeof(&rq));

	return NSD_CONTINUE;
}

/*
** This is the exported routine.  It takes a current request, and a pointer
** to a control structure defining the request.  It returns an nsd return
** value which must be dealt with in the caller.  We create a new request
** structure based on the information in the control structure and write
** it on the loopback pipe.  After our caller has fallen back to the main
** select loop the daemon wakes up and calls the dispatch routine above.
** When the request is complete the send above is called out of the main
** loop which writes the response to the end of the pipe and falls back to
** the select loop again.  Finally, the daemon wakes up and calls the callback
** routine above which ends up calling the routine passed in our control
** structure.
*/
int
nsd_loopback(nsd_file_t *rq, nsd_loop_t *lp)
{
	nsd_file_t *fp, *cp;
	loop_send_t *sd;
	char *domain, *key;
	
	nsd_logprintf(NSD_LOG_MIN, "nsd_loopback:\n");

	if (pipes[0] < 0) {
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nsd_loopback: failed pipe: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}
		
		nsd_callback_new(pipes[0], loop_dispatch, NSD_READ);
		nsd_callback_new(pipes[1], loop_callback, NSD_READ);
	}
	
	if (! lp->lb_dir) {
		/*
		** If table is set then we lookup the right directory
		** in the filesystem tree, else we use the request
		** structure like a directory.
		*/
		if (lp->lb_table && ! (lp->lb_flags & NSD_LOOP_SKIP) &&
		    ! (lp->lb_flags & NSD_LOOP_FIRST)) {
			if (lp->lb_domain) {
				domain = lp->lb_domain;
			} else {
				domain = nsd_attr_fetch_string(rq->f_attrs,
				    "domain", ".local");
			}
			key = (lp->lb_key) ? lp->lb_key : ".all";
			if (nsd_file_find(domain, lp->lb_table,
			    lp->lb_protocol, key, &lp->lb_dir) ==
			    NSD_OK) {
				/*
				** Results are already in the tree so we
				** just up the reference count and return
				** it.
				*/
				lp->lb_dir->f_nlink++;
				rq->f_loop = lp->lb_dir;
				nsd_logprintf(NSD_LOG_LOW,
				    "\talready in tree: %s/%s/%s/%s\n",
				    domain, lp->lb_table, lp->lb_protocol, key);
				return NSD_OK;
			}

			if (! lp->lb_dir) {
				/*
				** Not found.
				*/
				nsd_logprintf(NSD_LOG_LOW,
				    "\tno directory: %s/%s/%s/%s\n",
				    domain, lp->lb_table, lp->lb_protocol, key);
				return NSD_OK;
			}
		} else {
			lp->lb_dir = rq;
		}
	}
	
	if (nsd_file_init(&fp, "loopback", sizeof("loopback") - 1, lp->lb_dir,
	    NFREG, rq->f_cred) != NSD_OK) {
		nsd_logprintf(NSD_LOG_LOW, "\tfailed file init\n");
		return NSD_ERROR;
	}
	
	sd = nsd_calloc(1, sizeof(*sd));
	if (! sd) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsd_loopback: failed malloc\n");
		nsd_file_clear(&fp);
		return NSD_ERROR;
	}
	
	sd->proc = lp->lb_proc;
	sd->rq = rq;
	fp->f_sender = sd;
	
	/*
	** Setup attributes.
	*/
	nsd_attr_clear(fp->f_attrs);
	fp->f_attrs = nsd_attr_copy(rq->f_attrs);
	nsd_attr_continue(&fp->f_attrs, lp->lb_dir->f_attrs);

	if (lp->lb_key) {
		nsd_attr_store(&fp->f_attrs, "key", lp->lb_key);
		fp->f_index = LOOKUP;
	} else {
		fp->f_index = LIST;
	}
	if (lp->lb_table) {
		nsd_attr_store(&fp->f_attrs, "table", lp->lb_table);
	}
	if (lp->lb_domain) {
		nsd_attr_store(&fp->f_attrs, "domain", lp->lb_domain);
	}
	
	if (lp->lb_attrs) {
		nsd_attr_parse(&fp->f_attrs, lp->lb_attrs, 0, 0);
	}
	
	/*
	** Setup callout list.
	*/
	if (lp->lb_flags & NSD_LOOP_FIRST) {
		cp = nsd_file_getcallout(rq);
		nsd_file_appendcallout(fp, cp, cp->f_name, cp->f_namelen);
	} else if (nsd_file_copycallouts(lp->lb_dir, fp, 0) != NSD_OK) {
		nsd_logprintf(NSD_LOG_LOW, "\tfailed copycallouts\n");
		nsd_file_clear(&fp);
		free(sd);
		return NSD_ERROR;
	}

	if (lp->lb_flags & NSD_LOOP_SKIP) {
		if (! nsd_file_nextcallout(fp)) {
			nsd_logprintf(NSD_LOG_LOW, "\tno callouts\n");
			nsd_file_clear(&fp);
			free(sd);
			return NSD_OK;
		}
	}
	
	if (write(pipes[1], &fp, sizeof(&fp)) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsd_loopback: failed write: %s\n", strerror(errno));
		nsd_file_clear(&fp);
		free(sd);
		return NSD_ERROR;
	}
	
	return NSD_CONTINUE;
}
