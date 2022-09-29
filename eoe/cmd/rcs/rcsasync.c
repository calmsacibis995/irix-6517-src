#include "rcsbase.h"

struct co_data {
	struct rcsCheckOutOpts *opts;		/* co options for request */
	RCSTMP hdl;				/* handle pointing to result */
	const char *rcspath;			/* path to map to ,v file */
	const char *fname;			/* name of file w/result */
	pid_t pid;				/* process running request */
	struct co_data *next;			/* linked list */
};

struct pcs {
	pid_t pid;			/* process ID */
	void *queuedata;		/* co_data, or other queue data */
	enum QType qtyp;		/* type of queue waiting for child */
	struct pcs *next;		/* next entry in list */
};

static int enqueue_co P((struct co_data *));
static int run_co P((struct co_data *, int add_to_list));
static struct co_data *dup_co
	P((const char*, RCSTMP,const char*, struct rcsCheckOutOpts*));
static void destroy_co P((struct co_data *));
static char * newstring2 P(( const char *, const char *));
static int sigchldhdlr P((pid_t, int));
static int add_pid P((pid_t, struct co_data *, enum QType));
static void * remove_pid P((pid_t));

#define MAXCHILDREN 10
static struct co_data *co_data_head = NULL;
static struct co_data *co_data_tail = NULL;
static struct co_data *co_data_inactive = NULL;

static struct pcs *pcs_head = NULL;
static struct pcs *pcs_tail = NULL;
static int children = 0;

int
co_async(rcspath, hdl, fname, opts)
	const char *rcspath;
	RCSTMP hdl;
	const char *fname;
	struct rcsCheckOutOpts *opts;
{
	/* creates a child process, in order to perform a co (check out)
	 * asynchronously.   Does appropriate bookeeping, so that
	 * any attempt to read from the handle for the co'd file will
	 * block until the co is complete.
	 *
	 * Bookkeeping is:
	 *	Create a co_data entry, that owns the request, so long
	 *	as it is outstanding (it remembers data for later running,
	 *	if the configured number of children are already running)
	 *
	 *	Associate the handle with the co_data (mark the handle
	 *	as "need to wait")
	 *
	 *	remember data for later use, if exceed configured number
	 *	of child processes.
	 *
	 *	If we can run a child, do so (associate process data with
	 *	co_data).
	 *
	 * Note: It is not necessary to create the file "fname"
	 * immediately.  The name of the file comes from mktemp(),
	 * which uses the handle number as part of the template
	 * to create the temp file name.   Thus, even if another
	 * async checkout were to be done immediately, it would
	 * be with a different handle, and thus, mktemp() would
	 * not generate the same temp name. 
	 */	

#if !has_fork
	return ERR_BAD_FORK;
#else
	struct co_data *codata;
	int rc;

	if (!(codata = dup_co(rcspath, hdl, fname, opts))) {
		rcsCloseTmp(NULL, hdl);
		return ERR_NO_MEMORY;
	}

	if (rcsQueueHdl(hdl, codata, COQUEUE) < 0) {
		/* Do not close handle - it is either invalid
		 * (and thus need not be closed), or is the handle
		 * for another file.   Either way, since the handle
		 * was created internally, this is an internal error.
		 */
		return ERR_INTERNAL;
	}
	ignoreints();	/* must use before we check no. of children */
#ifdef DEBUG
	printf("co_async:children = %d\n", children);
#endif
		
	if (children > MAXCHILDREN)
	    rc = enqueue_co(codata);
	else 
	    rc = run_co(codata, 1);
	restoreints();
	return rc;
#endif
}

static int
enqueue_co(codata)
	struct co_data *codata;
{
	/* Add codata to end of queue (not running, yet) */

	if (!co_data_head) co_data_head = codata;
	else co_data_tail->next = codata;

	co_data_tail = codata;
	if (!co_data_inactive) co_data_inactive = codata;

#ifdef DEBUG
	printf(
	    "enqueue_co(%#x): head= %#x (%d); tail= %#x (%d), inactive= %#x\n",
	    (int)codata, (int)co_data_head,
	    co_data_head ? co_data_head->pid : 0, (int)co_data_tail,
	    co_data_tail ? co_data_tail->pid : 0, (int)co_data_inactive);
#endif
	return 0;
}

static int
run_co(codata, add_to_list)
	struct co_data *codata;
	int add_to_list;		/* true if need to insert codata */
{
	int rc;
	struct rcsCheckOutOpts *opts = codata->opts;
#define COOPTSNUM 8	/* -j, -s, -d, -l, -u, -k, -p -q */
	const char *argv[COOPTSNUM + 4];  /* skip 0, cmd, file, NULL at end */
	const char **argptr = argv;
	const char **endptr = NULL;
	char keyexpand[6];

	/* Add codata to head of queue (running), create child
	 * (add child data to list of known children for later wait(2))
	 */
	static int sethdlr = 0;
	pid_t pid;

	if (!sethdlr) {		/* Set SIGCHLD handler, first time */
	    setsigchld(sigchldhdlr);
	    sethdlr++;
	}

	argptr++;
	*argptr++ = CO;
	if (opts->joinflag && *(opts->joinflag))
	    if (!(*argptr++ = newstring2("-j", opts->joinflag))) goto mem2_err;
	if (opts->stateflag && *(opts->stateflag))
	    if (!(*argptr++ = newstring2("-s", opts->stateflag))) goto mem2_err;
	if (opts->dateflag && *(opts->dateflag))
	    if (!(*argptr++ = newstring2("-d", opts->dateflag))) goto mem2_err;
	endptr = argptr;
	if (opts->lockflag) *argptr++  = "-l";
	if (opts->unlockflag) *argptr++ = "-u";
	if (opts->keyexpand && *(opts->keyexpand)) {
		keyexpand[0] = '-';
		strcpy(keyexpand+1, opts->keyexpand);
		*argptr++ = keyexpand;
	}
	*argptr++ = "-p";		/* to stdout; redirected to tmp file */
	*argptr++ = "-q";		/* no noise */
	*argptr++ = codata->rcspath;	/* file to check out */
	*argptr = NULL;

    mem2_err:
	/* free all allocated memory for the argv array */
	if (!endptr) endptr = argptr-1;	/* in case of failed malloc */

	/* endptr now points to the first argv[] entry past the malloc'd ones */

	while (--endptr != &argv[1]) {
	    free((void *)*endptr);
	}

	if ((pid = runv_async(-1, codata->fname, NULL, argv)) < 0) {
		destroy_co(codata);
		rcsCloseTmp(NULL, codata->hdl);
		return ERR_CANT_FORK;
	}

	rc = add_pid(pid, codata, COQUEUE);
	if (rc < 0) {
#ifdef DEBUG
	    printf("add_pid FAILED.  rc = %d\n", rc);
#endif
	    destroy_co(codata);
	    rcsCloseTmp(NULL, codata->hdl);
	    return rc;
	}
	codata->pid = pid;
		
	if (!add_to_list) {
	    return 0;
	}

	/* Add to head of list */
	if (!(codata->next = co_data_head))
		co_data_tail = codata;
	co_data_head = codata;
	return 0;
}

static struct co_data *
dup_co(rcspath, hdl, fname, opts)
	const char *rcspath;
	RCSTMP hdl;
	const char *fname;
	struct rcsCheckOutOpts *opts;
{
	struct co_data *codata;
	struct rcsCheckOutOpts *newopts;
	size_t len;
	static struct rcsCheckOutOpts nullopts = { 0,0,0,"","","","",""};	

	if (!(codata = malloc(sizeof(*codata)))) return NULL;

	if (!opts) opts = &nullopts;
	else {
	    if (!(opts->rev)) opts->rev = "";
	    if (!(opts->joinflag)) opts->joinflag = "";
	    if (!(opts->stateflag)) opts->stateflag = "";
	    if (!(opts->dateflag)) opts->dateflag = "";
	    if (!(opts->keyexpand)) opts->keyexpand = "";
	}

	memset(codata, '\0', sizeof(*codata));

	if (!(newopts = codata->opts = malloc(sizeof(*newopts)))) goto mem_err;

	if (!(newopts->rev = malloc(len = strlen(opts->rev)+1))) goto mem_err;
	memcpy((void *)(newopts->rev), opts->rev, len);

	if (!(newopts->joinflag = malloc(len = strlen(opts->joinflag)+1)))
		goto mem_err;
	memcpy((void *)(newopts->joinflag), opts->joinflag, len);

	if (!(newopts->stateflag = malloc(len = strlen(opts->stateflag)+1)))
		goto mem_err;
	memcpy((void *)(newopts->stateflag), opts->stateflag, len);

	if (!(newopts->dateflag = malloc(len = strlen(opts->dateflag)+1)))
		goto mem_err;
	memcpy((void *)(newopts->dateflag), opts->dateflag, len);

	if (!(newopts->keyexpand = malloc(len = strlen(opts->keyexpand)+1)))
		goto mem_err;
	memcpy((void *)(newopts->keyexpand), opts->keyexpand, len);

	if (!(codata->fname = malloc(len = strlen(fname)+1))) goto mem_err;
	memcpy((void *)(codata->fname), fname, len);

	if (!(codata->rcspath = malloc(len = strlen(rcspath)+1))) goto mem_err;
	memcpy((void *)(codata->rcspath), rcspath, len);

	codata->pid = 0;
	codata->hdl = hdl;

	newopts->lockflag = opts->lockflag;
	newopts->unlockflag = opts->unlockflag;

	return codata;

    mem_err:
	for(;;) {
	    if (!newopts) break;
	    if (!newopts->rev) break;
	    free((void *)(newopts->rev));
	    if (!newopts->joinflag) break;
	    free((void *)(newopts->joinflag));
	    if (!newopts->stateflag) break;
	    free((void *)(newopts->stateflag));
	    if (!newopts->dateflag) break;
	    free((void *)(newopts->dateflag));
	    if (!newopts->keyexpand) break;
	    free((void *)(newopts->keyexpand));
	    free(newopts);
	    if (!codata->fname) break;
	    free((void *)(codata->fname));
	    if (!codata->rcspath) break;
	    free((void *)(codata->rcspath));
	    break;
	}
	free(codata);
	return NULL;
}

static void
destroy_co(codata)
	struct co_data *codata;
{
	struct co_data *ptr;
	struct rcsCheckOutOpts *opts;

	/* Remove from linked list; reset co_data_head (if at head of list),
	   co_data_tail (if at tail of list), co_data_inactive
	   (if first inactive entry)
	 */

	/* Head of list (implies active) */
	if (codata == co_data_head) {
		if (!(co_data_head = codata->next))	/* was entire list */
		    co_data_tail = NULL;
	}
	else {
	    /* Locate position in list */
	    for (ptr = co_data_head; ptr; ptr = ptr->next) {
	    	if (ptr->next == codata) break;
	    }

	    /* In middle of list */
	    if (ptr) {
		    ptr->next = codata->next;		/* remove from list */
		    if (co_data_inactive == codata)	/* if 1st inactive */
			co_data_inactive = codata->next;
		    if (co_data_tail == codata)		/* at end of list */
			co_data_tail = ptr;
	    }
	}

	/* If there was a process associated with this, kill it, and
	 * decrement count.  We use SIGHUP, since that is a relatively
	 * safe signal, which RCS (and most programs) should be catching.
	 */
	if (codata->pid) {
		kill(codata->pid, SIGHUP);
		remove_pid(codata->pid);
	}
	
	/* Free memory */
	opts = codata->opts;
	free((void *)(opts->rev));
	free((void *)(opts->joinflag));
	free((void *)(opts->stateflag));
	free((void *)(opts->dateflag));
	free((void *)(opts->keyexpand));
	free((void *)(codata->fname));
	free((void *)(codata->rcspath));
	free(opts);
	free(codata);
}

static char *
newstring2(s1, s2)
	const char *s1, *s2;
{
	size_t l1, l2;
	char *ptr = malloc((l1 = strlen(s1)) + (l2 = strlen(s2)) + 1);
	if (!ptr) return NULL;
	memcpy(ptr, s1, l1);
	memcpy(ptr+l1, s2, l2+1);
	return ptr;
}
 
static int
sigchldhdlr(pid, w)
	pid_t pid;
	int w;
{
	/* Routine that processes (does bookkeeping) for SIGCHLDs;
	 * called by actual handler.
	 *
	 * Returns the pid if it was an expected process (neg num if error),
	 * returns zero if the pid was not on its expected list.
	 */
	struct co_data *codata;
	int rc, rc2;

	/* Check whether this is a PID we care about, and remove it
	 * from the "care about" list (returning the data assoc'd with it)
	 */
#ifdef DEBUG
	printf("sigchldhdlr(%d)\n", pid);
#endif
	if ((codata = remove_pid(pid))) {
	    /* Mark associated handle as async complete
	     * (so that return code is checked before getting
	     *  data from handle)
	     */
	    if (!WIFEXITED(w)) {
#ifdef DEBUG
		if (WIFSIGNALED(w)) {
		    psignal(WTERMSIG(w), "Signal exit");
		}
#endif
		rc = ERR_FATAL;
	    }
	    else rc = WEXITSTATUS(w);
#ifdef DEBUG
	    if (rc)
	        printf("got bad rc: w = %#x, rc = %d\n", w, rc);
#endif
	    if (rc > 0) rc = -rc;
	    rc = rcsDQueueHdl(codata->hdl, rc);

	    codata->pid = 0;	/* no longer associated with a PID */
	    destroy_co(codata); /* free memory, remove from list */

	    /* Start next entry in list */
	    if (co_data_inactive) {
		codata = co_data_inactive;
		co_data_inactive = co_data_inactive->next;
		rc2 = run_co(codata, false);
		if (rc2 < 0) rc = rc2;
	    }
	    return rc;
	}
	return 0;
}


static int
add_pid(pid, codata, qtyp)
	pid_t pid;
	struct co_data *codata;
	enum QType qtyp;
{
	struct pcs *pcsptr;
	pcsptr = malloc(sizeof(*pcsptr));
	if (!pcsptr) return ERR_NO_MEMORY;

	pcsptr->pid = pid;
	pcsptr->queuedata = codata;
	pcsptr->qtyp = qtyp;
	pcsptr->next = NULL;

	/* Add to tail */
	if (!pcs_tail) 
	    pcs_head = pcsptr;
	else 
	    pcs_tail->next = pcsptr;

	pcs_tail = pcsptr;

	children++;
#ifdef DEBUG
	printf("add_pid(%d) called\n", pid);
	printf("add_pid: children increased to %d\n", children);
#endif
	return 0;
}

static void *
remove_pid(pid)
	pid_t pid;
{
	struct pcs *oldpcsptr;
	void *retptr;
#ifdef DEBUG
	printf("remove_pid(%d) called\n", pid);
#endif
	if (!pcs_head) return NULL;
	if (pcs_head->pid == pid) {
	    oldpcsptr = pcs_head;
	    if (!(pcs_head = pcs_head->next)) pcs_tail = NULL;
	}
	else {
	    struct pcs *pcsptr;
	    oldpcsptr = NULL;
	    for (pcsptr = pcs_head; pcsptr->next; pcsptr = pcsptr->next) {
	        if (pcsptr->next->pid == pid) {
		    oldpcsptr = pcsptr->next;
		    pcsptr->next = oldpcsptr->next;
		    if (pcs_tail == oldpcsptr) pcs_tail = pcsptr;
		    break;
		}
	    }
	    if (!oldpcsptr) return NULL;
	}
	retptr = oldpcsptr->queuedata;
	free(oldpcsptr);
	children--;
#ifdef DEBUG
	printf("remove_pid: children decremented to %d\n", children);
#endif
	return retptr;
}
