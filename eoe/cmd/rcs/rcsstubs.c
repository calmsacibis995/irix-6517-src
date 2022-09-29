#include "rcsapi.h"
#include "rcspriv.h"
#include <sys/types.h>
#include <netinet/in.h>		/* struct sockaddr_in, sin_addr */
#include <sys/socket.h>		/* SOCK_STREAM */
#include <sys/param.h>		/* MAXHOSTNAMELEN */
#include <netdb.h>		/* hostent, servent */
#include <stdio.h>		/* FILE * */
#include <signal.h>		/* SIGPIPE */
#include <stdlib.h>		/* malloc() */

/* Heap used to return values to client */
static struct rcsHeap rcsheap = {NULL, NULL, NULL};

/* Static buffers; we try to reuse these as much as possible */

#define LOCALLONGS 256	/* size of local buffer, in longs */


/* Ensure that local buf is at least as large as MAXPATHLEN */
#define LOCALBUFSZ \
   (MAXPATHLEN + sizeof(unsigned long) - 1)/sizeof(unsigned long) > LOCALLONGS \
    ?									\
   (MAXPATHLEN + sizeof(unsigned long) - 1)/sizeof(unsigned long)	\
    : LOCALLONGS


static union {
    unsigned char uc[(LOCALBUFSZ)*sizeof(unsigned long)];
    	     char ch[(LOCALBUFSZ)*sizeof(unsigned long)];
    unsigned long ul[(LOCALBUFSZ)];
} localbuf;

static char workpath[MAXPATHLEN];
static char head[0xff];
static char branch[0xff];
static char comment[0xff];
static char keysub[0xff];
static char revision[0xff];
static char date[0xff];
static char locker[0xff];
static char author[0xff];
static char state[0xff];
static char nextrev[0xff];

static char * workptr = workpath;
static char * lbufptr = localbuf.ch;

/* Local utilities */

static int getlongstring(RCSCONN, char **, int);
static int rcsMapTmpHdlToSrv(RCSTMP, RCSTMP *, RCSCONN *);
static int rcsAllocHdl(RCSCONN, RCSTMP, RCSTMP *);

/* Public APIs (stubs) */

	int
rcsCheckOut(conn, rcspath, outhandle, outopts, opts)
	RCSCONN conn;
	const char *rcspath;
	RCSTMP *outhandle;
	struct rcsStreamOpts *outopts;
	struct rcsCheckOutOpts *opts;
{

	/* Check out a revision of the specified archive (,v)
	 * file into a temp file.  Return a handle to the temp
	 * file.  Optionally, return the temp file attributes.
	 */

	int rc;
	unsigned char uc;
	unsigned char msgbuf[4];
	struct sendFileAttr fattr;

	/* Protocol:
	 * Byte 0: RCS_RPC
	 * Byte 1: RCS_RPC_CHECKOUT
	 * Byte 2: booleans (1 == lockflag; 2 == unlockflag)
	 * Byte 3: pad
	 * longstring: archive file full server pathname
	 * shortstring rev
	 * shortstring joinflag
	 * shortstring stateflag
	 * shortstring dateflag
	 * shortstring keyexpand
	 */

	if (!rcspath || !*rcspath || !outhandle) return ERR_APPL_ARGS;

	msgbuf[0] = RCS_RPC;
	msgbuf[1] = RCS_RPC_CHECKOUT;
	msgbuf[2] = (opts->lockflag ? 1 : 0) | (opts->unlockflag ? 2 : 0);

	/* Send request */
	if (write((int)conn, &msgbuf, sizeof(msgbuf)) != sizeof(msgbuf) ||
	    rcsPutLongString((int)conn, rcspath) < 0 ||
	    rcsPutShortString((int)conn,opts->rev) < 0 ||
	    rcsPutShortString((int)conn,opts->joinflag) < 0 ||
	    rcsPutShortString((int)conn,opts->stateflag) < 0 ||
	    rcsPutShortString((int)conn,opts->dateflag) < 0 ||
	    rcsPutShortString((int)conn,opts->keyexpand) < 0) {
		close((int)conn);
		return ERR_NETWORK_DOWN;
	}

	/* Read response */
	rc = getrc(conn, 0);
	if (rc < 0) return rc;

	/* We still need to get the attributes of the output "file".
	 * Protocol for incoming message:
	 * Byte 0: RCS_FATTR
	 * Byte 1-12: (struct sendFileAttr)
	 */
	if (read((int)conn, &uc, sizeof(uc)) != sizeof(uc) ||
		uc != RCS_FATTR ||
	        read((int)conn, &fattr, sizeof(fattr)) != sizeof(fattr)) {
		    close((int)conn);
		    return ERR_NETWORK_DOWN;
	}
	rc = rcsAllocHdl(conn, (RCSTMP)ntohl(fattr.handle), outhandle);
	if (rc < 0) {
		/* Should execute CloseTmp(server-side! handle) */
	}
	if (outopts) {
		outopts->mtime = ntohl(fattr.mtime);
		outopts->mode = ntohl(fattr.mode);
	}
	return rc;
}


	int
rcsCheckIn(conn, inputhandle, rcspath, opts)
	RCSCONN conn;
	RCSTMP inputhandle;
	const char *rcspath;
	struct rcsCheckInOpts *opts;
{

	/* Check in a previously "pushed" file, into the specified
	 * archive (,v) file.  Optionally, generate an output
	 * file (on the server), and receive back a handle.
	 */

	RCSTMP srvhdl;
	int rc;
	union {
	    char buf[2*sizeof(unsigned long)];
	    unsigned long ul[2];
	} msg;
	unsigned char uc;

	/* Protocol:
	 * Byte 0: RCS_RPC
	 * Byte 1: RCS_RPC_CHECKIN
	 * Byte 2: booleans (1 == lockflag; 2 == forceflag;
	 *		     4 == keywordflag; 8 == initflag;
	 *		    0x10 == mustexist;
	 *		    0x20 == request outHandle;
	 *		    0x40 == serverfile)
	 * Byte 3: pad
	 * Bytes 4-7: input handle (htonl format)
	 * longstring: archive file full server pathname
	 * shortstring oldrev
	 * shortstring newrev
	 * shortstring nameflag
	 * shortstring Nameflag
	 * shortstring stateflag
	 * shortstring dateflag
	 * shortstring msg
	 * longstring textflag
	 *
	 * (if outHandle is requested, server will send back attributes,
	 *  too, which this stub may discard)
	 */

	if (!rcspath || !*rcspath) return ERR_APPL_ARGS;

	/* Bad input handle if table says it is unused */
	if (rcsMapTmpHdlToSrv(inputhandle, &srvhdl, &conn) < 0)
		return ERR_APPL_ARGS;

	msg.buf[0] = RCS_RPC;
	msg.buf[1] = RCS_RPC_CHECKIN;
	msg.buf[2] = (opts->lockflag ? 1 : 0) |
		     (opts->forceflag ? 2 : 0) |
		     (opts->keywordflag ? 4 : 0) |
		     (opts->initflag ? 8 : 0) |
		     (opts->mustexist ? 0x10 : 0) |
		     (opts->outHandle ? 0x20 : 0) |
		     (opts->serverfile ? 0x40 : 0);

	msg.ul[1] = htonl((unsigned long)(int)srvhdl);


	/* Send request */
	if (write((int)conn, &msg, sizeof(msg)) != sizeof(msg) ||
	    rcsPutLongString((int)conn, rcspath) < 0 ||
	    rcsPutShortString((int)conn,opts->oldrev) < 0 ||
	    rcsPutShortString((int)conn,opts->newrev) < 0 ||
	    rcsPutShortString((int)conn,opts->nameflag) < 0 ||
	    rcsPutShortString((int)conn,opts->Nameflag) < 0 ||
	    rcsPutShortString((int)conn,opts->stateflag) < 0 ||
	    rcsPutShortString((int)conn,opts->dateflag) < 0 ||
	    rcsPutShortString((int)conn,opts->msg) < 0 ||
	    rcsPutLongString((int)conn,opts->textflag) < 0) {
		close((int)conn);
		return ERR_NETWORK_DOWN;
	}

	/* Read response */
	rc = getrc(conn, 0);
	if (rc < 0) return rc;

	if (opts->outHandle) {
	    struct sendFileAttr fattr;

	    /* We still need to get the attributes of the output "file".
	     * Protocol for incoming message:
	     * Byte 0: RCS_FATTR
	     * Byte 1-12: (struct sendFileAttr)
	     */
	    if (read((int)conn, &uc, sizeof(uc)) != sizeof(uc) ||
		uc != RCS_FATTR ||
	        read((int)conn, &fattr, sizeof(fattr)) != sizeof(fattr)) {
		    close((int)conn);
		    return ERR_NETWORK_DOWN;
	    }
	    rc = rcsAllocHdl(conn,
				(RCSTMP)ntohl(fattr.handle), opts->outHandle);
	    if (rc < 0) {
		/* Should execute CloseTmp(server-side! handle) */
	    }
	    if (opts->outOpts) {
		opts->outOpts->mtime = ntohl(fattr.mtime);
		opts->outOpts->mode = ntohl(fattr.mode);
	    }
	}
	return rc;
}



	int
rcsReadOutData(writeMethod, outputStream, conn, datahdl)
	int (*writeMethod)(RCSSTREAM *, const void *, size_t);
	RCSSTREAM *outputStream;
	RCSCONN conn;
	RCSTMP datahdl;
{
	/* Given a handle to an output file on the server, write
	 * the contents of that file to the user-provided output stream.
	 *
	 * The attributes of the file (mtime, mode) will already have
	 * been provided to the user when the handle was returned.
	 */

	RCSCONN dataconn;	/* connection, according to datahdl */
	RCSTMP srvhdl;		/* server-side handle for data file */

	/* Bad data handle if table says it is unused */
	if (rcsMapTmpHdlToSrv(datahdl, &srvhdl, &dataconn) < 0)
		return ERR_APPL_ARGS;

	/* Read in file must be associated with specified
	 * session (connection)
	 */
	if (conn != dataconn)
		return ERR_APPL_ARGS;

	/* Protocol:
	 * Byte 0: RCS_FTP_GET
	 * Bytes 1-3: pad (usable for option bits)
	 * Bytes 4-7: server-side handle (htonl form)
	 */
	{ struct {
	    char c[2*sizeof(unsigned long)];
	    unsigned long ul[2];
	  } msg;

	  msg.c[0] = RCS_FTP_GET;
	  msg.ul[1] = htonl((unsigned long)(int)srvhdl);

	  if (write((int)conn, &msg, sizeof(msg)) != sizeof(msg)) {
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	  }
	}

	/* Get the actual file, writing it into the stream.
	 * Returns 0 on success, negative error codes.
	 */
	return getstream((int)conn, writeMethod, outputStream);
}


	int
rcsChangeHdr(conn, rcspath, nameval, namevalNum, opts)
	RCSCONN conn;
	const char *rcspath;
	struct rcsNameVal nameval[];
	int namevalNum;
	struct rcsChangeHdrOpts *opts;
{

	/* Make specified changes to admin and delta headers.  */

	int rc;
	union {
	    char buf[2*sizeof(unsigned long)];
	    unsigned long ul[2];
	} msg;
	int i;
	struct rcsNameVal *nv;
	unsigned char *ptr;

	/* Protocol:
	 * Byte 0: RCS_RPC
	 * Byte 1: RCS_RPC_CHGHDR
	 * Byte 2: booleans (1 == initflag; 2 == nomail)
	 * Byte 3: pad
	 * Bytes 4-7: namevalNum (htonl format)
	 * longstring: archive file full server pathname
	 * longstring: mailtxt
	 * array of namevalNum bytes (the flags for each entry)
	 * shortstring: obsRevisions
	 * for each name/val entry:
	 *    shortstring name
	 *    shortstring revision
	 *    shortstring namespace
	 *    longstring val
	 */

	if (!rcspath || !*rcspath) return ERR_APPL_ARGS;
	if (namevalNum > sizeof(localbuf.ul)/sizeof(localbuf.ul[0]))
		return ERR_APPL_ARGS;

	msg.buf[0] = RCS_RPC;
	msg.buf[1] = RCS_RPC_CHGHDR;
	msg.buf[2] = (opts->initflag ? 1 : 0) |
		     (opts->nomail ? 2 : 0);

	msg.ul[1] = htonl((unsigned long)namevalNum);

	/* load array of flags */
	for (ptr = localbuf.uc, i = namevalNum, nv = nameval;
		i > 0;
	        *ptr++ = nv->flags, i--, nv++); 

	/* Send request */
	if (write((int)conn, &msg, sizeof(msg)) != sizeof(msg) ||
	    rcsPutLongString((int)conn, rcspath) < 0 ||
	    rcsPutLongString((int)conn, opts->mailtxt) < 0 ||
	    (namevalNum &&
		(write((int)conn, localbuf.uc, namevalNum) != namevalNum)) || 
	    rcsPutShortString((int)conn, opts->obsRevisions) < 0) {
	    
		close((int)conn);
		return ERR_NETWORK_DOWN;
	}

	/* Send each name/val piece of information:
	 * shortstring: name
	 * shortstring: revision 
	 * shortstring: namespace
	 * longstring:  val
	 */
	for (i = namevalNum, nv = nameval; i > 0; i--, nv++) {
	    if (rcsPutShortString((int)conn, nv->name) < 0 ||
		rcsPutShortString((int)conn, nv->revision) < 0 ||
		rcsPutShortString((int)conn, nv->namespace) < 0 ||
		rcsPutLongString((int)conn, nv->val) < 0) {

		    close((int)conn);
		    return ERR_NETWORK_DOWN;
	    }
	}

	/* Read response */
	rc = getrc(conn, 0);

	if (rc != ERR_PARTIAL) return rc;

	/* For partial failures, we need to gather all return codes */
	/* Protocol: array of namevalNum unsigned long return codes */
	if (read((int)conn, &localbuf.ul, sizeof(unsigned long)*namevalNum) !=
		sizeof(unsigned long) * namevalNum) {
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	}

	for (i = namevalNum, nv = nameval; i > 0; i--, nv++) {
	    nv->errcode = (int)ntohl(localbuf.ul[i]);
	}

	return rc;
}


	int
rcsGetHdr(conn, rcspath, opts, ret, receiveMethod, receiveStream)
	RCSCONN conn;
	const char *rcspath;
	struct rcsGetHdrOpts *opts;
	struct rcsGetHdrFields *ret;
	int (*receiveMethod)(RCSSTREAM *, const void *, size_t);
	RCSSTREAM *receiveStream;
{

	/* Get specified fields from admin and delta headers.  */

	int rc1, rc2;
	union {
	    char buf[4*sizeof(unsigned long)];
	    unsigned short us[4*(sizeof(unsigned long)/sizeof(unsigned short))];
	    unsigned long ul[4];
	} msg;
	int i;
	struct rcsNameVal *nvptr, **nvpptr;
	char *ptr, **pptr;
	unsigned char uc;
	struct sendFieldVal fieldhdr;
	static char *descript = NULL;	/* file description */
	static int descriptlen = 0;	/* length malloc'd (only grow it) */

	int locknum;
	int symnum;
	int accessnum;
	int namenum;
	int branchnum;

	char shortbuf[0xff];	/* scratch area for rcsGetShortString() */
	int shortlen;

	/* Protocol:
	 * Byte 0: RCS_RPC
	 * Byte 1: RCS_RPC_GETHDR
	 * Byte 2: booleans (0x1 == nogetrev;	0x2 == defaultbranch
	 *		     0x4 == onlylocked;	0x8 == onlyRCSname
	 *		    0x10 == ret struct returned
	 *		    0x20 == receiveStream returned)
	 *
	 * Byte 3: pad
	 * Bytes 4-5: hdrfields (htons format)
	 * Bytes 6-7: pad
 	 * Bytes 8-11: nameselnum (htonl format)
	 * Bytes 12-15: revselnum (htonl format)
	 * longstring: archive file full server pathname
	 * shortstring: namespace
	 *
	 * nameselnum of the namesel entries:
	 *   shortstring: name
	 *   shortstring: revision
	 *   shortstring: namespace
	 *
	 * revselnum of the revsel entries:
	 *   shortstring: name
	 *   longstring:  val
	 */

	if (!rcspath || !*rcspath) return ERR_APPL_ARGS;

	msg.buf[0] = RCS_RPC;
	msg.buf[1] = RCS_RPC_GETHDR;
	msg.buf[2] = (opts->nogetrev ? 0x1 : 0) |
		     (opts->defaultbranch ? 0x2 : 0) |
		     (opts->onlylocked ? 0x4 : 0) |
		     (opts->onlyRCSname ? 0x8 : 0) |
		     (ret ? 0x10 : 0) |
		     (receiveMethod ? 0x20 : 0);

	msg.us[2] = htons((unsigned short)(opts->hdrfields));
	msg.ul[2] = htonl((unsigned long)(opts->nameselnum));
	msg.ul[3] = htonl((unsigned long)(opts->revselnum));

	/* Send request */
	if (write((int)conn, &msg, sizeof(msg)) != sizeof(msg) ||
	    rcsPutLongString((int)conn, rcspath) < 0 ||
	    rcsPutShortString((int)conn, opts->namespace) < 0) {
	    
		close((int)conn);
		return ERR_NETWORK_DOWN;
	}

	/* Send each namesel name piece of information:
	 * shortstring: name
	 * shortstring: revision 
	 * shortstring: namespace
	 */
	for (i = opts->nameselnum, nvptr = opts->namesel; i > 0; i--, nvptr++) {
	    if (rcsPutShortString((int)conn, nvptr->name) < 0 ||
		rcsPutShortString((int)conn, nvptr->revision) < 0 ||
		rcsPutShortString((int)conn, nvptr->namespace) < 0) {

		    close((int)conn);
		    return ERR_NETWORK_DOWN;
	    }
	}

	/* Send each revsel name/val piece of information:
	 * shortstring: name
	 * longstring:  val
	 */
	for (i = opts->revselnum, nvptr = opts->revsel; i > 0; i--, nvptr++) {
	    if (rcsPutShortString((int)conn, nvptr->name) < 0 ||
		rcsPutLongString((int)conn, nvptr->val) < 0) {

		    close((int)conn);
		    return ERR_NETWORK_DOWN;
	    }
	}

	/* If there is an output stream, pass it on to the caller.
	 * This stream may be "empty" if there was an error
	 * ("empty" meaning just an EOF marker)
	 */
	rc1 = 0;
	if (receiveMethod && !opts->onlyRCSname) {
	    switch((rc1 = getstream((int)conn, receiveMethod, receiveStream))){
	    case ERR_FATAL:
	    case ERR_NETWORK_DOWN:
		rcsCloseConnection(conn);
		return rc1;
	    default: break;
	    }	
	}

	/* Read response */
	rc2 = getrc(conn, 0);

	/* If no return struct, then we're done */
	if (!ret) {
	    if (!rc2) return rc1;
	    return rc2;
	}

	/* If unsuccessful, no data returned */
	if (rc2) return rc2;

	/* If successful, retrieve parsed info.
	 * Protocol:
	 *    Byte 0: RCS_HDR_DATA
	 *    struct sendFieldVal (longs, in htonl fmt, containing returned
	 *	numbers, and numbers of elements in various arrays)
	 *
	 *    longstring:  rcspath
	 *    longstring:  workpath
	 *    shortstring: head
	 *    shortstring: branch
	 *    shortstring: comment
	 *    shortstring: keysub
	 *    longstring:  description
	 *    shortstring: revision
	 *    shortstring: locker
	 *    shortstring: date
	 *    shortstring: author
	 *    shortstring: state
	 *    shortstring: nextrev
	 *
	 *    For each lock:
	 *      shortstring: name
	 *	shortstring: revno
	 *
	 *    For each access entry:
	 *	shortstring: name
	 *
	 *    For each symbol:
	 *	shortstring: name
	 *	shortstring: val
	 *
	 *    For each name/val pair:
	 *	shortstring: revision
	 *	shortstring: namespace
	 *	shortstring: name
	 *	longstring:  val
	 *
	 *    For each branch ID:
	 *	shortstring: branch
	 */

	if (read((int)conn, &uc, 1) != 1 ||
		uc != RCS_HDR_DATA ||
		read((int)conn,&fieldhdr,sizeof(fieldhdr)) != sizeof(fieldhdr))
	{
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	}

	ret->strict = ntohl(fieldhdr.strict);
	ret->totalrev = ntohl(fieldhdr.totalrev);
	ret->selectrev = ntohl(fieldhdr.selectrev);
	ret->inserted = ntohl(fieldhdr.inserted);
	ret->deleted = ntohl(fieldhdr.deleted);

	locknum = ntohl(fieldhdr.locknum);
	accessnum = ntohl(fieldhdr.accessnum);
	symnum = ntohl(fieldhdr.symnum);
	namenum = ntohl(fieldhdr.namenum);
	branchnum = ntohl(fieldhdr.branchnum);

	if (getlongstring(conn, &lbufptr, -sizeof(localbuf.ch)) <= 0 ||
	    getlongstring(conn, &workptr, -sizeof(workpath)) <= 0 ||
	    rcsGetShortString(conn, head) < 0 ||
	    rcsGetShortString(conn, branch) < 0 ||
	    rcsGetShortString(conn, comment) < 0 ||
	    rcsGetShortString(conn, keysub) < 0 ||
	    (i = getlongstring(conn, &descript, descriptlen)) <= 0 ||
	    rcsGetShortString(conn, revision) < 0 ||
	    rcsGetShortString(conn, locker) < 0 ||
	    rcsGetShortString(conn, date) < 0 ||
	    rcsGetShortString(conn, author) < 0 ||
	    rcsGetShortString(conn, state) < 0 ||
	    rcsGetShortString(conn, nextrev) < 0) {
		if (i > descriptlen) descriptlen = i;
		rcsCloseConnection(conn);
		return ERR_NETWORK_DOWN;
	}

	if (i > descriptlen) descriptlen = i;	/* description buffer grew */

	/* Return the buffers used for the string data */
	ret->rcspath = localbuf.ch;
	ret->workpath = workpath;
	ret->head = head;
	ret->branch = branch;
	ret->comment = comment;
	ret->keysub = keysub;
	ret->description = descript;
	ret->revision = revision;
	ret->locker = locker;
	ret->date = date;
	ret->author = author;
	ret->state = state;
	ret->nextrev = nextrev;

	/* Allocate the arrays */
	ret->locks = rcsHeapAlloc(&rcsheap, 
				(locknum+1) * sizeof(struct rcsNameVal *));

	ret->access = rcsHeapAlloc(&rcsheap,
					(accessnum+1) * sizeof(char *));

	ret->symbols = rcsHeapAlloc(&rcsheap,
				(symnum+1) * sizeof(struct rcsNameVal *));

	ret->names = rcsHeapAlloc(&rcsheap,
				(namenum+1) * sizeof(struct rcsNameVal *));

	ret->branches = rcsHeapAlloc(&rcsheap,
				(branchnum+1) * sizeof(char *));

	/* Retrieve lock array */
	nvptr = rcsHeapAlloc(&rcsheap, locknum * sizeof(struct rcsNameVal));
	for (i = locknum, nvpptr = (struct rcsNameVal **)(ret->locks);
             i > 0; i--, nvptr++)
	{
	    *nvpptr++ = nvptr;	/* store a pointer to the next name/val */

	    /* Get the name part of the lock (owner) */
	    if ((shortlen = rcsGetShortString((int)conn,shortbuf)) <=0) break;
	    nvptr->name = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy((char *)(nvptr->name), shortbuf, shortlen);

	    /* Get the value part of the lock (revision) */
	    if ((shortlen = rcsGetShortString((int)conn, shortbuf)) <=0) break;
	    nvptr->val = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy((char *)(nvptr->val), shortbuf, shortlen);
	}
	if (i > 0) {
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	}
	*nvpptr = NULL;		/* null-terminate array of pointers */


	/* Retrieve access array */
	for (i = accessnum, pptr = (char **)(ret->access); i > 0; i--, pptr++)
	{
	    /* Get the next login on the list */
	    if ((shortlen = rcsGetShortString((int)conn, shortbuf)) <=0) break;
	    *pptr = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy(*pptr, shortbuf, shortlen);
	}
	if (i > 0) {
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	}
	*pptr = NULL;		/* null-terminate array of pointers */


	/* Retrieve symbol array */
	nvptr = rcsHeapAlloc(&rcsheap, symnum * sizeof(struct rcsNameVal));
	for (i = symnum, nvpptr = (struct rcsNameVal **)(ret->symbols);
             i > 0; i--, nvptr++) 
	{
	    *nvpptr++ = nvptr;	/* store a pointer to the next name/val */

	    /* Get the name part of the symbol (tag) */
	    if ((shortlen = rcsGetShortString((int)conn,shortbuf)) <=0) break;
	    nvptr->name = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy((char *)(nvptr->name), shortbuf, shortlen);

	    /* Get the value part of the symbol (ID) */
	    if ((shortlen = rcsGetShortString((int)conn, shortbuf)) <=0) break;
	    nvptr->val = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy((char *)(nvptr->val), shortbuf, shortlen);
	}
	if (i > 0) {
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	}
	*nvpptr = NULL;		/* null-terminate array of pointers */

	/* Retrieve namespace entries */
	nvptr = rcsHeapAlloc(&rcsheap, namenum * sizeof(struct rcsNameVal));
	for (i = namenum, nvpptr = (struct rcsNameVal **)(ret->names);
             i > 0; i--, nvptr++)
	{
	    *nvpptr++ = nvptr;	/* store a pointer to the next name/val */

	    /* Get the revision part of the namespace entry */
	    if ((shortlen = rcsGetShortString((int)conn, shortbuf)) < 0) break;
	    if (shortlen) {
		nvptr->revision =  rcsHeapAlloc(&rcsheap, shortlen);
		memcpy((char *)(nvptr->revision), shortbuf, shortlen);
	    }
	    else nvptr->revision = NULL;

	    /* Get the namespace part of the namespace entry */
	    if ((shortlen = rcsGetShortString((int)conn, shortbuf)) <=0) break;
	    nvptr->namespace = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy((char *)(nvptr->namespace), shortbuf, shortlen);

	    /* Get the name part of the namespace entry */
	    if ((shortlen = rcsGetShortString((int)conn, shortbuf)) <=0) break;
	    nvptr->name = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy((char *)(nvptr->name), shortbuf, shortlen);

	    /* Get the value part of the namespace entry */
	    if (getlongstring(conn, (char **)&(nvptr->val), 0) < 0) break;
	}
	if (i > 0) {
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	}
	*nvpptr = NULL;		/* null-terminate array */


	/* Retrieve branches array */
	for (i = branchnum, pptr = (char **)(ret->branches); i > 0; i--, pptr++)
	{
	    /* Get the next revision ID on the list */
	    if ((shortlen = rcsGetShortString((int)conn, shortbuf)) <=0) break;
	    *pptr = rcsHeapAlloc(&rcsheap, shortlen);
	    memcpy(*pptr, shortbuf, shortlen);
	}
	if (i > 0) {
	    close((int)conn);
	    return ERR_NETWORK_DOWN;
	}
	*pptr = NULL;		/* null-terminate array of pointers */

	/* Return appropriate error code */
	if (rc1) return rc1;	/* usually ERR_WRITE_STREAM */
	else return rc2;	/* should be zero if we got to here */
}


#define RCSSRV "rcssrv"		/* service name */
#define RCSPGM "rcssrv"		/* server program name */

int
rcsOpenConnection(const char *host, struct rcsConnOpts *opts, RCSCONN *conn)
{
	/* Create a session with the specified server (NULL means
	 * use default RCS server) on the specified host.
	 *
	 * Returns 0 on success, -<error code> on error.
	 */
	struct hostent *hostp;
	struct servent *servp;
	struct sockaddr_in peer;
	int sd;
	const char *serv;
	const char *auth;
	char msgbuf[3];
	char hostbuf[MAXHOSTNAMELEN];
	int rc;
	int havehost;
	int didfork;
	void (*oldsigpipe)() = NULL;	/* old SIGPIPE handler */

	*conn = NULL;

	/* Four cases:
	 * 1. Host specified, fork okay: get client machine, see if matches host
	 * 2. Host specified, fork not allowed: connect to specified host
         * 3. Host blank, fork okay: fork/exec server
         * 4. Host blank, fork not allowed: get client machine, connect to it
         */

	/* Get client machine in cases (1) and (4) */
	if ((havehost = (host && *host)) && (opts->ctype == RCS_CONN_DFLT) ||
	     !havehost && (opts->ctype != RCS_CONN_DFLT)) {
	    if (gethostname(hostbuf, sizeof(hostbuf)) < 0) {
		return ERR_BAD_HOST;
	    }
	}

	/* Fork if allowed (cases (1) and (3)), and the host, if specified,
	 *  matches the client machine (case (1)).
	 */
	if (opts->ctype == RCS_CONN_DFLT &&
		(!havehost || !strcmp(host, hostbuf))) {

	    int sv[2];		/* socket pair */
	    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		return ERR_CANT_OPEN_SOCKET;
	    }

	    if (!fork()) {	/* child */
		char ascii_fd[10];
		sprintf(ascii_fd, "%d", sv[1]);
		close(sv[0]);
	        execlp(RCSPGM, RCSPGM, ascii_fd, 0);
		exit(1);
	    }
	    else {
		close(sv[1]);
		sd = sv[0];	/* Connection established */
		didfork = 1;
#ifdef SIGPIPE
		/* In case child dies, and first write fails */
		oldsigpipe = signal(SIGPIPE, SIG_IGN);
#endif
		
	    }
	}
	else {	/* different server; find host machine/port, and connect */
	    if (!havehost) host = hostbuf;	/* case (4) */
	    if (!(hostp = gethostbyname(host))) return ERR_BAD_HOST;
	    /* if (!(serv = opts->service)) */ serv = RCSSRV;
	    if (!(servp = getservbyname(serv, NULL))) return ERR_BAD_SERVICE;

	    if ((sd = socket (hostp->h_addrtype, SOCK_STREAM, 0)) < 0)
		return ERR_CANT_OPEN_SOCKET;
	
	    memset(&peer, '\0', sizeof(peer));
	    memcpy(&peer.sin_addr, hostp->h_addr, hostp->h_length);
	    peer.sin_family = hostp->h_addrtype;
	    peer.sin_port = htons(servp->s_port);


	    /* Make connection */
	    if (connect(sd, (struct sockaddr *)&peer, sizeof(peer)) < 0) {
		close(sd);
		return ERR_CANT_CONNECT;
	    }
	    didfork = 0;
	}

	/* Send over options */
	/* Message format:
	 *  Byte 0 - RCS_RPC
	 *  Byte 1 - RCS_RPC_CONNECT
	 *  Byte 2 - booleans (1 == Ttimeflag; 2 == noquietflag)
	 *  shortstring - author
	 *  shortstring - suffixes
	 *  longstring - debugout
	 */
	msgbuf[0] = RCS_RPC;
	msgbuf[1] = RCS_RPC_CONNECT;
	msgbuf[2] = (opts->Ttimeflag ? 0x1 : 0) |
		    (opts->noquietflag ? 0x2 : 0);
	/* auth = (opts->auth && *opts->auth) ? opts->auth : cuserid(NULL); */
	auth = didfork ? NULL : cuserid(NULL);
	

	if (write(sd, msgbuf, sizeof(msgbuf)) != sizeof(msgbuf) ||
	    rcsPutShortString(sd, auth) < 0 ||
	    rcsPutShortString(sd, opts->suffixes) < 0 ||
	    rcsPutLongString(sd, opts->debugout) < 0) {
		close(sd);
		return ERR_CANT_CONNECT;
	}

#ifdef SIGPIPE
	/* Restore client's signal handler */
	if (oldsigpipe) signal(SIGPIPE, oldsigpipe);
#endif

	switch (rc = (int)getrc((RCSCONN)sd, 0)) {
	case ERR_BAD_FCREATE: return rc;
	default: return ERR_CANT_CONNECT;
	case 0: break;
	}

	*conn = (RCSCONN) sd;
	return 0;
}
		
int
rcsCloseConnection(RCSCONN conn)
{
	/* Close the specified session.
	 *
	 * Returns 0 on success, ERR_APPL_ARGS on error.
	 */

	return (close((int)conn) < 0) ? ERR_APPL_ARGS : 0;
}



#define STREAM_MTIME(x)	((x) ? (x) : time(NULL))
#define STREAM_MODE(x)	((x) ? (x) : 0444) 

static struct {
    RCSCONN conn;			/* connection assoc'd w/this handle */
    RCSTMP handle;			/* server-side handle */
    bool is_set;			/* is this entry used? */
} maptmp[TEMPNAMES];

/* "Function" to locate the next free slot (handle) for client-side tmp handle
 *  May be replaced by more elegant search, if table grows large.
 *
 *  Returns index (handle) into maptmp if empty slot found, else ERR_NOHANDLES.
 *  (Return is via parameter n.)
 */
#define gettmphdl(n) \
    {   int i; \
	for (i = 0; i < sizeof(maptmp)/sizeof(maptmp[0]); i++)  \
	    if (!maptmp[i].is_set) break; \
	*n = (i >= sizeof(maptmp)/sizeof(maptmp[0]) ? ERR_NOHANDLES : i); \
    }

int
rcsReadInData(readMethod, inputStream, conn, opts, handle_ptr)
	int readMethod(RCSSTREAM *inputStream, void **ptr, size_t nbytes);
	RCSSTREAM *inputStream;
	RCSCONN conn;
	struct rcsStreamOpts *opts;
	RCSTMP *handle_ptr;
{
	/* Takes an input stream, and its read method.
	 * Makes a copy of the complete stream, and hands back
         * a "handle" to this snapshot copy.
	 *
	 * Return value is zero on success; negative error code.
	 *
	 * If opts->fname is provided, the input stream is a file,
	 * which the library may choose to read as needed, rather
	 * than immediately.  It is strictly advisory; the library
	 * may still choose to make an immediate copy, using the
	 * read method.
	 *
	 * The virtue of this parameter is that one less copy
	 * of the data may be made (esp. when the application
	 * is linked directly with the server code).
	 * to be made. 
	 *
	 * 
	 *  opts->mtime - the "modification time" of the stream.
	 *		This may represent the time that the stream
	 *		data were created/changed, or the current
	 *		time.  A value of zero is interpreted as
	 *		the current time.
	 *
	 *		This time may be used for such tasks as
	 *		setting the time of revision created from the
	 *		input stream, and setting the time of the RCS
	 *		archive file (if so desired and indicated using
	 *		appropriate option flags).
	 *
	 *		If fname is used, the file's st_mtime is used
	 *		as a safety check to ensure that a file has
	 *		not changed during checkin.
	 *
	 * opts->mode -	the "mode" of the stream.  The r/x bits of the
	 *		mode are used by RCS to set the corresponding
	 *		bits of a new archive file.  (For example,
	 *		shell scripts should be executable.)
	 *
	 *		The w bits of the mode are used to infer whether
	 *		the stream was created/modified by the application,
	 *		or is merely a reflection of a version already
	 *		in the archive (which has been checked out, read-only)
	 *
	 *		This latter use (checking w bits) of the mode
	 *		generally does not apply to the library.  In the
	 *		RCS commands, a checkout (output stream) may
	 *		ask the user whether to overwrite a writable file.
	 * 
	 * If fname is used, then other struct stat fields have significance:
	 * st_size -    used internally as a simple filter to see whether
	 *		the file being checked in might have changed in
	 *		the middle of the operation.  Not used unless
	 *		"fname" set.
	 *
	 * st_ino, st_dev - used to verify that the file (if "fname" is
	 *		    set) is not the same file as the archive file.
	 *
	 */


	int rc = sendstream(readMethod, inputStream, conn,
			  STREAM_MTIME(opts->mtime), STREAM_MODE(opts->mode),
			  NULL, FILE_TMP);

	return rc >= 0 ? rcsAllocHdl(conn, (RCSTMP)rc, handle_ptr) : rc;
}


	int
rcsInitStreamOpts(opts)
	struct rcsStreamOpts *opts;
{
	/* Initialize default values for stream */
	if (!opts) return ERR_APPL_ARGS;

	memset(opts, '\0', sizeof(*opts));
	return 0;
}


/* Utilities for manipulating message types */

	static int
getlongstring(conn, bufptr, size)
	RCSCONN conn;
	char **bufptr;
	int size;
{
	/* Retrieve a long string from connection conn.
	 * If size is positive, that is the current size of a malloc'd
	 * buffer.  Free and re-malloc if not large enough.
	 * If size is zero, no buffer has been allocated.  Simply malloc.
	 * If size is negative, then buffer cannot be reallocated.
	 * Fill as much as possible (up to |size| bytes).
	 *
	 * Returns number of bytes in received string, including null.
	 * Returns zero if no string sent (length of zero means NULL, not "").
	 * Returns -1 on error.
	 */
	
	unsigned short s;
	char ch;
	int len;
	int domalloc;

	if (read((int)conn, &s, sizeof(s)) != sizeof(s)) return -1;
	len = ntohs(s);
	if (!len) return 0;

	/* Check whether we need to malloc space */
	domalloc = 0;
	if (size > 0) {
	    if (size < len) {
		free (*bufptr);
		domalloc = 1;
	    }
	    else size = len;
	}
	else if (!size) domalloc = 1;
	else {	/* size < 0 */
	    if ((size = -size) > len) size = len;
	}

	if (domalloc) {
	    if (!(*bufptr = malloc(len))) return -1;
	    size = len;
	}

	/* Now size has the number of bytes, including null, that we can
	 * copy
	 */
	if (read((int)conn, *bufptr, size) != size) return -1;

	/* flush any extra bytes */
	/* we can do this slowly, since it should never happen */
	if (len > size) {
	    (*bufptr)[size-1] = '\0';
	    for (len -= size; len--;) {
		read((int)conn, &ch, 1);
	    }
	}

	return size;
}
	    
/* Utilities for handling tmp file handles on client side */

	static int
rcsMapTmpHdlToSrv(handle, srvhdl, conn)
	RCSTMP handle;
	RCSTMP *srvhdl;
	RCSCONN *conn;
{
	/* Given a temp handle, return the server handle and connection
	 * that the client handle is associated with.  (Multiple connections
	 * may have the same server handle, so there must be a non-trivial
	 * mapping between client handle and server handle x connection.)
	 */

	if (!maptmp[(int)handle].is_set) return ERR_APPL_ARGS;  /* bad input */

	*srvhdl = maptmp[(int)handle].handle;
	*conn = maptmp[(int)handle].conn;
	return 0;
}

	static int
rcsAllocHdl(conn, srvhdl, handle)
	RCSCONN conn;
	RCSTMP srvhdl;
	RCSTMP *handle;
{
	/* Given a server side handle and connection, allocate a client
	 * side handle, and store the server side info for mapping
	 * between client and server info.
	 */

	int n;

	gettmphdl(&n);  	/* get an available slot (handle) */
	if (n >= 0) {
	      maptmp[n].is_set = 1;
	      maptmp[n].handle = (RCSTMP)srvhdl;
	      maptmp[n].conn = conn;
	      *handle = (RCSTMP)n;
	      return 0;
	}
	return n;
}

