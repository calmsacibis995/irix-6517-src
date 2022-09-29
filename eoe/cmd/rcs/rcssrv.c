#include "rcsbase.h"
#include "rcspriv.h"

#ifdef LATER
static char fname[MAXPATHLEN] = {0};   /* buffer for file name of xmtd file */
static char fpath[MAXPATHLEN] = {0};   /* buffer for path of xmtd file */
#endif /* LATER */
static RCSCONN conn = (RCSCONN)-1; /* Session active for this server */

static void sendsyserr(int, int);
static int getlongstring(int, char **);
static void unlinkit(char *);
static int cleanup(int);
static void freestring();

#define DEBUG
#ifdef DEBUG
#define send_rc send_debug_rc
static int debugfd;
static char debugbuf[1024];
#endif

const char *cmdid = "rcslib";	/* used in rcslex.c error msgs */

#define MAXMSGSTRING 10
typedef char shrtstring[255];

/* The main program for the server side.  This contains the read/act/respond
 * loop; it is allowed to make calls to internal routines in the RCS library,
 * since it is linked with that library.  It also must do more sophisticated
 * locking on files than on the client side.  These are reasons why the
 * main event loop is not shared between server and client.  Also, the
 * client side must differentiate between normal I/O interaction with the
 * user (actually steam), and the events on the socket.  The server side
 * has no such divided attention.
 */
main(argc, argv)
	int argc;
	char *argv[];
{
	int sockfd;		/* file descriptor for socket */
	time_t mtime;		/* mod time of file */
	mode_t fmode;		/* mode of file */

	int filefd;		/* file descriptor for workfile */

	int rc;			/* return code */
	char *ptr;
	unsigned char ch;
	short s;
	int i;

#ifdef DEBUG
	debugfd = creat("/tmp/debug1", 0777);
#endif
	if (argc < 2) exit(1);	/* no socket descriptor passed */
	sockfd = atoi(argv[1]);


#ifdef DEBUG
	sprintf(debugbuf, "socket descriptor = %d\n", sockfd);
	write(debugfd, debugbuf, strlen(debugbuf));
	sleep(15);
#endif

#ifdef LATER
	/* initialize inter-message context */
	fname[0] = 0;
	fpath[0] = '\0';
	workfile[0] = '\0';
#endif /* LATER */

	while(read(sockfd, &ch, sizeof(ch)) > 0) {
	  /* Handle different types of messages */
#ifdef DEBUG
	sprintf(debugbuf, "received msg type %d\n", ch);
	write(debugfd, debugbuf, strlen(debugbuf));
#endif
	  switch(ch) {

	  case RCS_FTP_PUT:	/* upload a file */
	  {
	    RCSTMP handle;	/* handle of temporary file */

	    struct stat sbuf;

	    /* Get the file, return the handle (as a positive RC) */
	    if (getfile(sockfd, &handle) < 0 ||
		rcsSendRC(sockfd, (int)handle) < 0) {
		return(cleanup(1));
	    }
	    break;
	  }

	  case RCS_FTP_GET:	/* download a file */
	  {
	    /* Bytes 1-3: pad
	     * Bytes 4-7: handle (htonl format)
	     */
	    struct {
		char buf[2 * sizeof(unsigned long)];
		unsigned long ul[2];
	    } msg;
	    RCSTMP hdl;

	    if (read(sockfd, &msg.buf[1], sizeof(msg)-1) != sizeof(msg)-1) {
		sendsyserr(sockfd, ERR_FATAL);
		/* NOTREACHED */
	    }
	    
	    hdl = (RCSTMP)ntohl(msg.ul[1]);

	    /* Should not have user-created errors: bad handle shouldn't
	     * happen, because the client library maps the user handle
	     * to a valid server handle; the temp file should exist so
	     * long as the session is still active and the user has not
	     * closed it (in which case the client library would identify
	     * the handle as invalid, and not get this far).  So we
	     * are justified in dying if we cannot read the filesystem,
	     * or write to the network.
	     */
	    if (sendfile(sockfd, hdl) < 0) return(cleanup(1));
	    break;
	  }
	    
	  case RCS_RPC:
#ifdef DEBUG
	    strcpy(debugbuf, "about to get RPC command\n");
	    write(debugfd, debugbuf, strlen(debugbuf));
	    if(read(sockfd, &ch, sizeof(ch)) <= 0) ch = (unsigned char)-1;
	    sprintf(debugbuf, "got %d RPC cmd\n", ch);
	    write(debugfd, debugbuf, strlen(debugbuf));
#else
	    if(read(sockfd, &ch, sizeof(ch)) <= 0) ch = (unsigned char)-1;
#endif
	    switch(ch) {
	    case RCS_RPC_CONNECT:     /* Create connection/sesion */
	    {
		struct rcsConnOpts opts;
		shrtstring auth;	/* -w flag */
		int authlen;
		shrtstring suffixes;	/* -x flag */
		int suflen;
		int debuglen;

		/* Read the options for the connection */
		/* Byte 2 - booleans (1 == Ttimeflag, 2 == noquietflag)
		 * shortstring - auth
		 * shortstring - suffixes
		 * longstring - debugout
		 */
		if (read(sockfd, &ch, sizeof(char)) <= 0 ||
		    ((authlen = rcsGetShortString(sockfd, (char *)auth)) < 0) ||
		    ((suflen = rcsGetShortString(sockfd,(char *)suffixes)) < 0)
		    || ((debuglen = getlongstring(sockfd, &ptr)) < 0)
		 ) {
		    sendsyserr(sockfd, ERR_FATAL);
		    /* NOTREACHED */
		}

		rcsInitConnOpts(&opts);

		/* "Copy" strings to options struct, if not NULL */
		/* if (authlen > 0) opts.auth = auth; */
		if (suflen > 0)  opts.suffixes = suffixes;
		if (debuglen > 0) opts.debugout = ptr;

		/* Handle flags */
		if (ch & 0x1) opts.Ttimeflag = 1;
		if (ch & 0x2) opts.noquietflag = 1;

		/* Open the connection locally (do bookkeeping) */
		if ((rc = rcsOpenConnection(NULL, &opts, &conn)) < 0)
		    sendsyserr(sockfd, rc);

#ifdef LATER
		/* If auth sent, network connection; set permissions/id */
		if (authlen > 0) setupid(auth);
#endif /* LATER */

		/* Send back connection number as (positive) return code */
		rcsSendRC(sockfd, (int)conn);
		break;
	    }

	    case RCS_RPC_CHECKIN:	/* Check in file */
	    {
		union {
		    char buf[2*sizeof(unsigned long)];
		    unsigned long ul[2];
		} msg;
	        unsigned char uc;
		struct sendFileAttr fattr;
		char dateflag[0xff];
		char nameflag[0xff];
		char Nameflag[0xff];
		char stateflag[0xff];
		char msgflag[0xff];
		char oldrev[0xff];
		char newrev[0xff];
		char *textflag;
		int datelen, namelen, Namelen, statelen, oldlen, newlen;
		int textlen, msglen;

		RCSTMP inputhandle;
		struct rcsCheckInOpts opts;
		RCSTMP outputhandle;
		struct rcsStreamOpts outputopts;

		/* Read in checkin request.
		 * Protocol:
		 * Byte 2: booleans (1 == lockflag; 2 == forceflag;
		 *		     4 == keywordflag; 8 == initflag;
		 *		     0x10 == mustexist;
		 *		     0x20 request outHandle;
		 *		     0x40 == serverfile)
		 * Byte 3: pad
		 * Byte 4-7: input handle (htonl format)
		 * longstring: archive file full server pathname
		 * shortstring oldrev
		 * shortstring newrev
		 * shortstring nameflag
		 * shortstring Nameflag
		 * shortstring stateflag
		 * shortstring dateflag
		 * shortstring msgflag
		 * longstring textflag
		 */
		if (read(sockfd, &(msg.buf[2]), sizeof(msg)-2) < sizeof(msg)-2
		    || getlongstring(sockfd, &ptr) <= 0
		    || (oldlen = rcsGetShortString(sockfd, oldrev)) < 0
		    || (newlen = rcsGetShortString(sockfd, newrev)) < 0
		    || (namelen = rcsGetShortString(sockfd, nameflag)) < 0 
		    || (Namelen = rcsGetShortString(sockfd, Nameflag)) < 0
		    || (statelen = rcsGetShortString(sockfd, stateflag)) < 0
		    || (datelen = rcsGetShortString(sockfd, dateflag)) < 0 
		    || (msglen = rcsGetShortString(sockfd, msgflag)) < 0 
		    || (textlen = getlongstring(sockfd, &textflag)) < 0) {
		    sendsyserr(sockfd, ERR_NETWORK_DOWN);
		    /* NOTREACHED */
		}

		rc = 0;

		/* Convert handle (mapping archive handle along the way) */
	 	inputhandle = (RCSTMP)ntohl(msg.ul[1]);

		/* Initialize options for call to CheckIn */
		rcsInitCheckInOpts(&opts);

		/* Fill in options from socket */
		if (oldlen > 0) opts.oldrev = oldrev;
		if (newlen > 0) opts.newrev = newrev;
		if (namelen > 0) opts.nameflag = nameflag;
		if (Namelen > 0) opts.Nameflag = Nameflag;
		if (statelen > 0) opts.stateflag = stateflag;
		if (datelen > 0) opts.dateflag = dateflag;
		if (msglen > 0) opts.msg = msgflag;
		if (textlen > 0) opts.textflag = textflag;

		opts.lockflag = ((msg.buf[2] & 0x1) != 0);
		opts.forceflag = ((msg.buf[2] & 0x2) != 0);
		opts.keywordflag = ((msg.buf[2] & 0x4) != 0);
		opts.initflag = ((msg.buf[2] & 0x8) != 0);
		opts.mustexist = ((msg.buf[2] & 0x10) != 0);
		opts.serverfile = ((msg.buf[2] & 0x40) != 0);

		if (msg.buf[2] & 0x20) {
		    opts.outHandle = &outputhandle;
		    opts.outOpts = &outputopts;
		}

		/* Make the call */
		if (!rc) rc = rcsCheckIn(conn, inputhandle, ptr, &opts);
		rcsSendRC(sockfd, rc);

		/* If output is generated, send it back */
		/* Protocol:
		 * Byte 0: RCS_FATTR
		 * Byte 1-12: (struct sendFileAttr)
		 */
		if (rc >= 0 && opts.outHandle) {
		  fattr.handle = htonl((unsigned long)(long)outputhandle);  
		  fattr.mtime = htonl((unsigned long)outputopts.mtime);
		  fattr.mode = htonl((unsigned long)outputopts.mode);
		  uc = RCS_FATTR;

		  if (write(sockfd, &uc, sizeof(uc)) < sizeof(uc) ||
			write(sockfd, &fattr, sizeof(fattr)) < sizeof(fattr))
		  {
			close(sockfd);
			break;
		  }
		}
		break;
	    }	

	    case RCS_RPC_CHECKOUT:	/* Check out a file */
	    {
		unsigned char msgbuf[2];
	        unsigned char uc;
		struct sendFileAttr fattr;
		char dateflag[0xff];
		char stateflag[0xff];
		char joinflag[0xff];
		char rev[0xff];
		char keyexpand[0xff];
		int datelen, statelen, joinlen, revlen, keylen;

		RCSTMP handle;
		struct rcsCheckOutOpts opts;
		struct rcsStreamOpts outputopts;

		/* Read in checkout request.
		 * Protocol:
		 * Byte 2: booleans (1 == lockflag; 2 == unlockflag)
		 * Byte 3: pad
		 * longstring: archive file full server pathname
		 * shortstring rev
		 * shortstring joinflag
		 * shortstring stateflag
		 * shortstring dateflag
		 * shortstring keyexpand
		 */
		if (read(sockfd, msgbuf, sizeof(msgbuf)) < sizeof(msgbuf)
		    || getlongstring(sockfd, &ptr) <= 0
		    || (revlen = rcsGetShortString(sockfd, rev)) < 0
		    || (joinlen = rcsGetShortString(sockfd, joinflag)) < 0
		    || (statelen = rcsGetShortString(sockfd, stateflag)) < 0
		    || (datelen = rcsGetShortString(sockfd, dateflag)) < 0 
		    || (keylen = rcsGetShortString(sockfd, keyexpand)) < 0) { 
		    sendsyserr(sockfd, ERR_NETWORK_DOWN);
		    /* NOTREACHED */
		}

		/* Initialize options for call to CheckIn */
		rcsInitCheckOutOpts(&opts);

		/* Fill in options from socket */
		if (revlen > 0) opts.rev = rev;
		if (joinlen > 0) opts.joinflag = joinflag;
		if (statelen > 0) opts.stateflag = stateflag;
		if (datelen > 0) opts.dateflag = dateflag;
		if (keylen > 0) opts.keyexpand = keyexpand;

		opts.lockflag = ((msgbuf[0] & 0x1) != 0);
		opts.unlockflag = ((msgbuf[0] & 0x2) != 0);

		/* Make the call */
		rc = rcsCheckOut(conn, ptr, &handle, &outputopts, &opts);
		rcsSendRC(sockfd, rc);

		/* If successful, send the temp file handle and attributes */
		/* Protocol:
		 * Byte 0: RCS_FATTR
		 * Byte 1-12: (struct sendFileAttr)
		 */
		if (rc >= 0) {
		  fattr.handle = htonl((unsigned long)(long)handle);  
		  fattr.mtime = htonl((unsigned long)outputopts.mtime);
		  fattr.mode = htonl((unsigned long)outputopts.mode);
		  uc = RCS_FATTR;

		  if (write(sockfd, &uc, sizeof(uc)) < sizeof(uc) ||
			write(sockfd, &fattr, sizeof(fattr)) < sizeof(fattr))
		  {
			close(sockfd);
			break;
		  }
		}
		break;
	    }	

	    case RCS_RPC_CHGHDR:	/* Change header (fixed field) info */
	    {
		union {
		    char buf[2*sizeof(unsigned long)];
		    unsigned long ul[2];
		} msg;
		int namevalNum;
		char *mailtxt;
		shrtstring *stringbuf;
		shrtstring *ss;		/* pointer to a shortstring array */
		shrtstring obsRevisions;
		int namelen;
		int revlen;
		int apllen;
		int vallen;
	        unsigned long *errcodes;
		unsigned long *ulptr;
		unsigned char *flags = NULL;
		unsigned char *f;

		struct rcsChangeHdrOpts opts;
		struct rcsNameVal *nvptr, *nvptr2;

		/* Read in checkin request.
		 * Protocol:
		 * Byte 2: booleans (1 == initflag; 2 == nomail)
		 * Byte 3: pad
		 * Byte 4-7: number of name/val entries (htonl format)
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

		if (read(sockfd, &(msg.buf[2]), sizeof(msg)-2) < sizeof(msg)-2
		    || getlongstring(sockfd, &ptr) <= 0
		    || getlongstring(sockfd, &mailtxt) < 0
		    || ((namevalNum = (int)ntohl(msg.ul[1])) &&
			(flags = tnalloc(unsigned char, namevalNum)) &&
			read(sockfd, flags, namevalNum) != namevalNum
		       )
		    || rcsGetShortString(sockfd, (char *)obsRevisions) < 0) {
		    sendsyserr(sockfd, ERR_NETWORK_DOWN);
		    /* NOTREACHED */
		}

		/* Allocate space for the name/val struct, and for
		 * all the "shortstrings" it will contain
		 */
		if (namevalNum) {
		  nvptr = tnalloc(struct rcsNameVal, namevalNum);
		  stringbuf = tnalloc(shrtstring, namevalNum * 3);
		}
		else {
		  nvptr = NULL;
		  stringbuf = NULL;
		}

		/* Set up name/val struct array */
		for (nvptr2 = nvptr, ss = stringbuf, i = namevalNum, f = flags;
		      --i >= 0; nvptr2++) {

		    /* Read the fields:
		     * shortstring: name
		     * shortstring: revision
		     * shortstring: namespace
		     * longstring:  val
		     */
		    if ((namelen = rcsGetShortString(sockfd, (char *)ss)) < 0
		    || (revlen = rcsGetShortString(sockfd, (char *)(ss+1))) < 0
		    || (apllen = rcsGetShortString(sockfd, (char *)(ss+2))) < 0
		    || (vallen = getlongstring(sockfd, (char**)&(nvptr2->val)))
									< 0) {
			sendsyserr(sockfd, ERR_NETWORK_DOWN);
			/* NOTREACHED */
		    }

		    nvptr2->name = (const char *)ss++;

		    if (revlen > 0)
			nvptr2->revision = (const char *)ss++;
		    else {
			nvptr2->revision = NULL;
			ss++;
		    }

		    nvptr2->namespace = (const char *)ss++;
						/*NULL and "" are equiv */

		    if (!vallen) nvptr2->val = NULL;

		    nvptr2->flags = *f++;
		}

		/* Set up opts struct */
		opts.initflag = ((msg.buf[2] & 0x1) != 0);
		opts.nomail   = ((msg.buf[2] & 0x2) != 0);
		opts.mailtxt  = mailtxt;
		opts.obsRevisions = (const char *)obsRevisions;

		/* Make call */
		rc = rcsChangeHdr(conn, ptr, nvptr, namevalNum, &opts);
		rcsSendRC(sockfd, rc);

		/* If ERR_PARTIAL (only failures are on indiv. items,
		 * send back array of error codes
		 */
		if (rc == ERR_PARTIAL) {
		    errcodes = tnalloc(unsigned long, namevalNum);
		    for (nvptr2 = nvptr, ulptr = errcodes, i = namevalNum;
			 --i >= 0; nvptr2++) {
		      *ulptr++ = htonl((unsigned long)(nvptr2->errcode));
		    }

		    if (write(sockfd, ulptr, sizeof(unsigned long) * namevalNum)
			    != sizeof(unsigned long) * namevalNum) {
		        close(sockfd);
		    }
		    tfree(errcodes);
		}

		/* Clean up memory */
		/* getlongstring only manages MAXMSGSTRING - 2 buffers
		 * (the -2 is for the rcspath and the mailtxt).
		 * If namevalNum is greater than this, the onus is on us
		 * here to free the extra memory.
		 */
		if (namevalNum > MAXMSGSTRING - 2) {
		    for (nvptr2 = nvptr + (MAXMSGSTRING - 2),
			 i = namevalNum - (MAXMSGSTRING - 2);
			--i >= 0; nvptr2++) {
			tfree(nvptr2->val);
		    }
		}

		/* Now clean up the explicit memory requests above */
		if (namevalNum) {
		    tfree(flags);
		    tfree(nvptr);
		    tfree(stringbuf);
		}
		break;
	    }	

	    case RCS_RPC_GETHDR:	/* Get header (fixed field) info */
	    {
		union {
		    char buf[4*sizeof(unsigned long)];
		    unsigned short us
			[4*(sizeof(unsigned long)/sizeof(unsigned short))];
		    unsigned long ul[4];
		} msg;

		int namevalNum;
		char namespace[0xff];	/* namespace field */
		int namesplen;
		int namelen;
		int revlen;
		int apllen;
		int vallen;
		bool sendstream;	/* do we send output stream to caller */

		shrtstring *stringbuf;
		shrtstring *ss;		/* pointer to a shortstring array */
		struct rcsNameVal *nvptr;
		const struct rcsNameVal **nvpptr;
		const char **pptr;

		struct rcsGetHdrOpts opts;
		struct rcsGetHdrFields ret;

		struct sendFieldVal fieldhdr;
		unsigned char uc;

		/* Read in checkin request.
		 * Protocol:
		 * Byte 2: booleans (0x1 == nogetrev;	0x2 == defaultbranch
		 *		     0x4 == onlylocked;	0x8 == onlyRCSname
		 *		    0x10 == ret struct returned
		 *		    0x20 == receiveStream returned
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

		if (read(sockfd, &(msg.buf[2]), sizeof(msg)-2) < sizeof(msg)-2
		    || getlongstring(sockfd, &ptr) <= 0
		    || (namesplen = rcsGetShortString(sockfd, namespace)) < 0) {
		    sendsyserr(sockfd, ERR_NETWORK_DOWN);
		    /* NOTREACHED */
		}

		/* Fill in parts of opts that we have, so far */
		opts.nogetrev = (msg.buf[2] & 0x1);
		opts.defaultbranch = (msg.buf[2] & 0x2);
		opts.onlylocked = (msg.buf[2] & 0x4);
		opts.onlyRCSname = (msg.buf[2] & 0x8);
		/* pass &ret only if msg.buf[2] & 0x10 */

		/* send output stream only if requested, and caller not
		 * just asking for file name
		 */
		sendstream = (msg.buf[2] & 0x20) && !opts.onlyRCSname;

		opts.hdrfields = ntohs(msg.us[2]);
	
		opts.nameselnum = ntohl(msg.ul[2]);
		opts.revselnum = ntohl(msg.ul[2]);	
		opts.namespace = namesplen ? namespace : NULL;

		/* Allocate space for all the shortstrings in the namesel
		 * entries, and in the revsel entries
		 */
		if ((i = opts.nameselnum * 3 + opts.revselnum * 1) > 0)
		    stringbuf = tnalloc(shrtstring, i);
		else stringbuf = NULL;

		/* Allocate space for the name/val namesel struct (if any) */
		if (opts.nameselnum) {
		    opts.namesel = tnalloc(struct rcsNameVal, opts.nameselnum);
		}
		else opts.namesel = NULL;

		/* Allocate space for the name/val revsel struct (if any) */
		if (opts.revselnum) {
		    opts.revsel = tnalloc(struct rcsNameVal, opts.revselnum);
		}
		else opts.revsel = NULL;

		/* Fill in the namesel array */
		for (nvptr = opts.namesel, ss = stringbuf, i = opts.nameselnum;
			--i >= 0; nvptr++) {

		    /* Read the fields:
		     * shortstring: name
		     * shortstring: revision
		     * shortstring: namespace
		     */
		    if ((namelen = rcsGetShortString(sockfd, (char *)ss)) < 0
		    || (revlen = rcsGetShortString(sockfd, (char *)(ss+1))) < 0
		    || (apllen = rcsGetShortString(sockfd, (char *)(ss+2))) < 0)
		    {
			sendsyserr(sockfd, ERR_NETWORK_DOWN);
			/* NOTREACHED */
		    }

		    nvptr->name = (const char *)ss++;

		    if (revlen > 0)
			nvptr->revision = (const char *)ss++;
		    else {
			nvptr->revision = NULL;
			ss++;
		    }

		    nvptr->namespace = (const char *)ss++;
						/*NULL and "" are equiv */

		}

		/* Fill in the revsel array */
		for (nvptr = opts.revsel, i = opts.revselnum; --i > 0; nvptr++)
		{

		    /* Read the fields:
		     * shortstring: name
		     * longstring:  val
		     */
		    if ((namelen = rcsGetShortString(sockfd, (char *)ss)) <= 0
		    || (vallen = getlongstring(sockfd, (char**)&(nvptr->val)))
									< 0) {
			sendsyserr(sockfd, ERR_NETWORK_DOWN);
			/* NOTREACHED */
		    }
		    nvptr->name = (const char *)ss++;
		}

		/* Make call */
		rc = rcsGetHdr(conn, ptr, &opts,
			       (msg.buf[2] & 0x10) ? &ret : NULL,
			        sendstream ? sendpacket : NULL,
			        sendstream ? (RCSSTREAM *)sockfd : NULL
		     );

		/* If rcsGetHdr sent a stream, terminate it (EOS) */
		if (sendstream) {
		    i = sendpacket((RCSSTREAM *)sockfd, NULL, 0);
		    if (!rc) rc = i;
		}

		/* Send return code */
		rcsSendRC(sockfd, rc);

		/* If parsed field values requested, send them back
		 * (in case of error, just send "blanks")
		 */
		if (msg.buf[2] & 0x10) {

	 	    /* Protocol:
		     *    Byte 0: RCS_HDR_DATA
	 	     *    struct sendFieldVal (longs, in htonl fmt,
		     *		containing returned numbers, and numbers
		     *		of elements in various arrays)
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
		     *	    shortstring: revno
		     *
		     *    For each access entry:
		     *	    shortstring: name
		     *
		     *    For each symbol:
		     *	    shortstring: name
		     *	    shortstring: val
		     *
		     *    For each name/val pair:
		     *	    shortstring: revision
		     *	    shortstring: namespace
		     *	    shortstring: name
		     *	    longstring:  val
		     *
		     *    For each branch ID:
		     *	  shortstring: branch
		     */
		    if (!rc) {
			/* Count number of locks */
			for (i = 0, nvpptr = ret.locks; *nvpptr++; i++);
			fieldhdr.locknum = htonl(i);

			/* Count number of access entries */
			for (i = 0, pptr = ret.access; *pptr++; i++);
			fieldhdr.accessnum = htonl(i);

			/* Count number of symbols */
			for (i = 0, nvpptr = ret.symbols; *nvpptr++; i++);
			fieldhdr.symnum = htonl(i);

			/* Count number of namespace entries */
			for (i = 0, nvpptr = ret.names; *nvpptr++; i++);
			fieldhdr.namenum = htonl(i);

			/* Count number of branches */
			for (i = 0, pptr = ret.branches; *pptr++; i++);
			fieldhdr.branchnum = htonl(i);
		    }
		    else {	/* ensure client is not looking for arrays */
			fieldhdr.locknum = 0;
			fieldhdr.accessnum = 0;
			fieldhdr.symnum = 0;
			fieldhdr.namenum = 0;
			fieldhdr.branchnum = 0;
		    }

		    fieldhdr.totalrev = ret.totalrev;
		    fieldhdr.selectrev = ret.selectrev;
		    fieldhdr.inserted = ret.inserted;
		    fieldhdr.deleted = ret.deleted;
		    fieldhdr.strict = ret.strict;

		    /* Send all of message but arrays */
		    uc = RCS_HDR_DATA;
		    if (write(sockfd, &uc, 1) != 1 ||
			write(sockfd, &fieldhdr, sizeof(fieldhdr)) !=
				sizeof(fieldhdr) ||
			rcsPutLongString(sockfd, ret.rcspath) < 0 ||
			rcsPutLongString(sockfd, ret.workpath) < 0 ||
			rcsPutShortString(sockfd, ret.head) < 0 ||
			rcsPutShortString(sockfd, ret.branch) < 0 ||
			rcsPutShortString(sockfd, ret.comment) < 0 ||
			rcsPutShortString(sockfd, ret.keysub) < 0 ||
			rcsPutLongString(sockfd, ret.description) < 0 ||
			rcsPutShortString(sockfd, ret.revision) < 0 ||
			rcsPutShortString(sockfd, ret.locker) < 0 ||
			rcsPutShortString(sockfd, ret.date) < 0 ||
			rcsPutShortString(sockfd, ret.author) < 0 ||
			rcsPutShortString(sockfd, ret.state) < 0 ||
			rcsPutShortString(sockfd, ret.nextrev) < 0)
		    {
		        close(sockfd);
			break;
		    }

		    /* Send each array (if call failed, we have already
		     * reset the numbers of elements to zero, so this
		     * will work correctly in any case)
		     */

			/* locks */
		    for (i = fieldhdr.locknum, nvpptr = ret.locks; i > 0; --i) {
			if (rcsPutShortString(sockfd, (*nvpptr)->name) < 0 ||
			    rcsPutShortString(sockfd, (*nvpptr++)->val) < 0)
			        break;
		    }
		    if (i > 0) {
			close(sockfd);
			break;
		    }

			/* access */
		    for (i = fieldhdr.accessnum, pptr = ret.access; i > 0; --i)
		    {
			if (rcsPutShortString(sockfd, *pptr) < 0) break;
		    }
		    if (i > 0) {
			close(sockfd);
			break;
		    }

			/* symbols */
		    for (i = fieldhdr.symnum, nvpptr = ret.symbols; i > 0; --i)
		    {
			if (rcsPutShortString(sockfd, (*nvpptr)->name) < 0 ||
			    rcsPutShortString(sockfd, (*nvpptr)->val) < 0)
				break;
		    }
		    if (i > 0) {
			close(sockfd);
			break;
		    }

			/* namespace entries */
		    for (i = fieldhdr.namenum, nvpptr = ret.names; i > 0; --i)
		    {
			if (rcsPutShortString(sockfd,(*nvpptr)->revision) < 0 ||
			    rcsPutShortString(sockfd,(*nvpptr)->namespace)< 0 ||
			    rcsPutShortString(sockfd,(*nvpptr)->name)< 0 ||
			    rcsPutLongString(sockfd,(*nvpptr)->val)< 0)
				break;
		    }
		    if (i > 0) {
			close(sockfd);
			break;
		    }
				
			
			/* branches */
		    for (i = fieldhdr.branchnum, pptr=ret.branches; i > 0; --i)
		    {
			if (rcsPutShortString(sockfd, *pptr) < 0) break;
		    }
		    if (i > 0) {
			close(sockfd);
			break;
		    }
		}

		/* Clean up memory */
		/* getlongstring only manages MAXMSGSTRING - 1 buffers
		 * (the -1 is for the rcspath).
		 * If opts.revselnum is greater than this, the onus is on us
		 * here to free the extra memory.
		 */
		if (opts.revselnum > MAXMSGSTRING - 2) {
		    for (nvptr = opts.revsel + (MAXMSGSTRING - 1),
			 i = opts.revselnum - (MAXMSGSTRING - 1);
			--i >= 0; nvpptr++) {
			tfree(nvptr->val);
		    }
		}

		/* Now clean up the explicit memory requests above */
		if (opts.namesel) tfree(opts.namesel);
		if (opts.revsel)  tfree(opts.revsel);
		if (stringbuf)    tfree(stringbuf);

		break;
	    }	

#ifdef LATER
	    case RCS_RPC_BEGIN_SESS:  /* Begin (create + open) session */
		if (sess) {
		    /* only one open session allowed */
		    send_rc(RCS_RC,stdout,RCS_RPC_BEGIN_SESS,ERR_SESS_OPEN,
			    NULL,0);
		    break;
		}
		if (!state) {
		    /* must set state before session */
		    send_rc(RCS_RC, stdout, RCS_RPC_BEGIN_SESS,
			    ERR_NO_SESS_STATE, NULL,0);
		    break;
		}
		if (!(sess = CreateRcsSession(state, NULL, NULL, NULL, NULL))) {
		    send_sys_err(stdout, RCS_RPC_BEGIN_SESS,
				ERR_CREATE_SESS_FAIL);
		    /* NOTREACHED */
		}
		if ((sess->openSession)(sess)) {
		    send_sys_err(stdout, RCS_RPC_BEGIN_SESS,ERR_OPEN_SESS_FAIL);
		    /* NOTREACHED */
		}
		send_rc(RCS_RC, stdout, RCS_RPC_BEGIN_SESS, RCS_SUCCESS,
			NULL, 0);
		break;

	    case RCS_RPC_CREATE_SESS_STATE:  /* Create (init) session state */
	    {
		bool gotsuffixes;

		/* FIX: Ignore -t for now */
		
		/* CreateSessionState message struct:
		 *  Byte 2: boolean flags (-I, -T, -q)
		 *  Byte 3: length of -x (suffixes) string (w/ null terminator)
		 *  Bytes 4,N: suffixes string (null terminated)
		 *  Byte N+1: length of -w (auth) string (w/ null terminator)
		 *  Bytes N+2,M auth string (null terminated)
		 */
		if ((ch = getchar()) == EOF) return(cleanup(1));
#ifdef DEBUG
		sprintf(debugbuf, "CREATE_SESS_STATE: boolean flags %#x\n", ch);
		write(debugfd, debugbuf, strlen(debugbuf));
#endif
		switch (getshortstring(stdin,suffixes)) {
		case -1: return(cleanup(1));
		case 0:  gotsuffixes = false; break; /* no string shipped */
		default: gotsuffixes = true; break; /* suffixes has string */
		}
#ifdef DEBUG
		sprintf(debugbuf, "CREATE_SESS_STATE: suffixes = %s\n",
			gotsuffixes ? suffixes : "(NULL)");
		write(debugfd, debugbuf, strlen(debugbuf));
#endif
		if (getshortstring(stdin,auth) <= 0) return(cleanup(1));
#ifdef DEBUG
		sprintf(debugbuf, "CREATE_SESS_STATE: auth = %s\n", auth);
		write(debugfd, debugbuf, strlen(debugbuf));
#endif

		if (state) {
		    /* only one state/session */
		    send_rc(RCS_RC, stdout, RCS_RPC_CREATE_SESS_STATE,
			    ERR_STATE_EXISTS, NULL, 0);
		    break;
		}

#ifdef DEBUG
		strcpy(debugbuf, "About to create session state\n");
		write(debugfd, debugbuf, strlen(debugbuf));
#endif
		state = CreateRcsSessionState();

		if (ch & STATE_I_FLAG) state->interactive = true;
		if (ch & STATE_T_FLAG) state->Ttimeflag = true;
		if (ch & STATE_q_FLAG) state->quietflag = true;

		if (gotsuffixes)	state->suffixes = suffixes;
		else			state->suffixes = X_DEFAULT; 

		state->auth = auth;

		send_rc(RCS_RC, stdout, RCS_RPC_CREATE_SESS_STATE, 
			RCS_SUCCESS, NULL, 0);
		break;
	    }

	    case RCS_RPC_CREATE_DATASTREAM:
		/* OpenRcsDataFile message struct:
		 * Byte 2: boolean flags (openforwrite, mustexist)
		 * Byte 3-4:  length of base name (not null terminated)
		 * Bytes 5-N: base name
		 * Bytes N+1,N+2: length of path name (not null terminated)
		 * Bytes N+3,M: path name
		 */
	    {
		char *base;
		char *path;
		unsigned char ch_ret;
		
		if ((ch = getchar()) == EOF) return(cleanup(1));
		if (getlongstring(stdin, &base, true) < 0) return(cleanup(1));
		if (getlongstring(stdin, &path, true) < 0) return(cleanup(1));
		if (!(rc = OpenRcsDataFile(sess, &rcsfiletbl[0].ptr,base,path,
				(ch & RCSFILE_WRITE), (ch & RCSFILE_EXIST))))
		{
		    /* Not fatal error; user may have misnamed RCS file */
		    send_rc(RCS_RC, stdout, RCS_RPC_CREATE_DATASTREAM, 
			    ERR_RCS_FILE_OPEN_FAIL, NULL, 0);
		    break;
		}

		/* Rely upon internal checking that we don't have multiple
		 * RCS files open at once.
		 */
		rcsfiletbl[0].is_set = true;
		ch_ret = 0;		/* handle to return */
		send_rc(RCS_RC, stdout,RCS_RPC_CREATE_DATASTREAM, 
			rc > 0 ? RCS_SUCCESS : ERR_RCS_NO_FILE,
			&ch_ret, sizeof(ch));
		break;
	    }

		
	    case RCS_RPC_DELTA:		/* apply delta (ci) */
	    {
		bool keepfile = false;	/* will file be kept (ship back)? */
		char oldrev[255];
		char newrev[255];

		/* Only keep one deltaopts around; no memory leak here */
		if (!deltaopts) deltaopts = CreateRcsDeltaOpts();

		/* delta message struct:
		 * Byte 2: boolean flags
		 * Byte 3: indicators for whether string flags are present
		 *  (needed because some strings may be "", which is different
		 *   from NULL)
		 *
		 * The remaining string arguments are present only if
		 * the appropriate flag in byte 3 indicates so.  Depending
		 * upon the string, getshortstring or getlongstring format
		 * is used (one byte or two byte length).
		 *
		 * The flags, in order of appearance are:
		 *  -mmessage
		 *  -nname[:[rev]]
		 *  -Nname[:[rev]]
		 *  -s[state]
		 *  -d[date]
		 *
		 * The remaining portions of information passed are:
		 *  (workfile is remembered in workfile)
		 *  rcsfile (char handle)
		 *  olddelta (getshortstring, may be zero length)
		 *  newdelta (getshortstring, may be zero length)
		 */
		if ((ch = getchar()) == EOF) return(cleanup(1));

		if (ch & DELTA_i_FLAG)	deltaopts->initflag = true;
		else			deltaopts->initflag = false;

		if (ch & DELTA_j_FLAG)	deltaopts->mustexist = true;
		else			deltaopts->mustexist = false;

		if (ch & DELTA_l_FLAG){	deltaopts->lockflag = true;
					keepfile=true;
		}
		else 			deltaopts->lockflag = false;

		if (ch & DELTA_u_FLAG){ deltaopts->unlockflag = true;
					keepfile = true;
		}
		else			deltaopts->unlockflag = false;

		if (ch & DELTA_f_FLAG)	deltaopts->forceflag = true;
		else			deltaopts->forceflag = false;

		if (ch & DELTA_k_FLAG){	deltaopts->keepflag = true;
					keepfile = true;
		}
		else			deltaopts->keepflag = false;

		if (ch & DELTA_M_FLAG)	deltaopts->modworktimeflag = true;
		else			deltaopts->modworktimeflag = false;

		if ((ch = getchar()) == EOF) return(cleanup(1));

		if (ch & DELTA_m_FLAG) 
		    if (!getlongstring(stdin, (char **)&deltaopts->msg, true))
			return(cleanup(1));

		if (ch & DELTA_n_FLAG)
		    if (!getlongstring(stdin, &deltaopts->nameflag, true))
			return(cleanup(1));

		if (ch & DELTA_N_FLAG)
		    if (!getlongstring(stdin, &deltaopts->Nameflag, true))
			return(cleanup(1));

		if (ch & DELTA_s_FLAG) {
		    switch(getlongstring(stdin, &deltaopts->stateflag,true)) {
		    case 0:  /* Zero length flag */
			deltaopts->stateflag = "";
			break;
		    case -1: /* error */
			return(cleanup(1));
		    default: /* okay, non-zero length string */
			break;
		    }
		}
		
		if (ch & DELTA_d_FLAG) {
		    switch(getlongstring(stdin, &deltaopts->dateflag, true)) {
		    case 0:  /* Zero length flag */
			deltaopts->dateflag = "";
			break;
		    case -1: /* error */
			return(cleanup(1));
		    default: /* okay, non-zero length string */
			break;
		    }
		}
		
		if ((ch = getchar()) == EOF) return(cleanup(1));
		if (ch >= sizeof(rcsfiletbl)/sizeof(rcsfiletbl[0]) ||
			!rcsfiletbl[ch].is_set) {
		    send_sys_err(stdout, RCS_RPC_DELTA, ERR_RCS_BAD_HANDLE);
		    /* NOTREACHED */
		}

		if (getshortstring(stdin, oldrev) < 0 ||
		    getshortstring(stdin, newrev) < 0) {
		        return(cleanup(1));
		}

		if (!sess) {
		    send_rc(RCS_RC, stdout, RCS_RPC_DELTA, ERR_NO_SESS,
			    NULL, 0);
		    break;
		}
		
		/* Yea! We make the fcn call, at last. */
		rc = delta(sess, workfileptr, rcsfiletbl[ch].ptr,
			   oldrev, (const char *)newrev, deltaopts);

		if (rc < 0) {	/* library has already handled error msgs */
		    send_rc(RCS_RC, stdout, RCS_RPC_DELTA, ERR_RCS_CALL,
			    NULL, 0);
		    break;
		}

		/* If file is "kept" (or regenerated) download it to client */
		/* If shipping file fails, we've lost contact with client,
		 * and all we can really do is cleanup
		 */
		if (keepfile) 
		   switch (sendfile(fname, fpath, workfile, stdout)) {
		   case 1:  /* File not found; shouldn't happen */
	    		send_rc(RCS_RC, stdout, RCS_NOT_RPC,
				ERR_CANT_OPEN_WORKFILE, NULL,0);
			/* fall through */

		   default: return(cleanup(1));

		   case 0: break;
		}
		unlinkit(fname);
		send_rc(RCS_RC, stdout,RCS_RPC_DELTA, RCS_SUCCESS, NULL, 0);
		break;
	    }

	    case RCS_RPC_DESTROY_DATASTREAM:
		/* CloseRcsDataFile message struct:
		 * Byte 2: handle (map to RcsDataFile)
		 */
		if ((ch = getchar()) == EOF) return(cleanup(1));
		if (!sess) {
		    send_rc(RCS_RC, stdout, RCS_RPC_DESTROY_DATASTREAM,
			    ERR_NO_SESS, NULL, 0);
		    break;
		}
		if (ch >= sizeof(rcsfiletbl)/sizeof(rcsfiletbl[0]) ||
			!rcsfiletbl[ch].is_set) {
		    send_sys_err(stdout, RCS_RPC_DELTA, ERR_RCS_BAD_HANDLE);
		    /* NOTREACHED */
		}
		CloseRcsDataFile(sess, rcsfiletbl[ch].ptr);
		send_rc(RCS_RC, stdout,RCS_RPC_DESTROY_DATASTREAM, RCS_SUCCESS,
			NULL, 0);
		rcsfiletbl[ch].is_set = false;
		break;

	    case RCS_RPC_END_SESS:
		/* sess->destroy message contains no data (sess is implicit) */
		if (!sess) {
		    send_rc(RCS_RC, stdout, RCS_RPC_END_SESS, ERR_NO_SESS,
			    NULL, 0);
		}
		else {
		    (sess->destroy)(sess);
		    sess = NULL;
		    send_rc(RCS_RC, stdout, RCS_RPC_END_SESS, RCS_SUCCESS,
				NULL,0);
		}
		return(cleanup(0));

#endif /* LATER */
	    }  /* end case RCS_RPC */
	} /* end switch on message type */
	freestring();
    } /* end loop on receiving messages */

    exit(cleanup(0));
}



	static void
sendsyserr(fd, rc)
	int fd;
	int rc;
{
	rcsSendRC(fd, rc);
	exit(cleanup(1));
}

	static int
cleanup(rc)
	int rc;
{
	/* Close/remove all workfiles;
	 * terminate session (automatically closes RCS files)
	 * return rc
	 */
	if (conn != (RCSCONN)-1) {
	    rcsCloseConnection(conn);
	    conn = (RCSCONN)-1;
	}
	return rc;
}


	static void
unlinkit(fname)
	char *fname;
{
	un_link(fname);
	fname[0] = '\0';
}

/* Memory management specifically for getlongstring!
 * Since 0-N buffers are allocated when a request comes in, and all
 * are freed when the response is sent, we use an array of size N, and
 * an index to keep track of the next available slot.
 *
 * If the message needs more than N buffers, it is the responsibility
 * of the caller to explicitly tfree() the memory returned, as that
 * will not be stored in the array here.
 */
static int stringindx = 0;	/* first free index */
static char *stringalloc[MAXMSGSTRING];


	static void
freestring()
{
	while (--stringindx >= 0) free(stringalloc[stringindx]);
	stringindx = 0;
}


/* The routine getlongstring() would normally belong in rcsproto.c (protocol
 * utilities), except that it does memory mgmt that is server specific, and
 * that it is only required on the server side.  Thus it is in this file.
 */
	static int
getlongstring(fd, ptr)
	int fd;
	char **ptr;
{
	/* Gets an arbitrary length ("long") string from the input stream.
	 * The length of the string is specified in two bytes (and hence
	 * is limited to 65535 bytes).  This is not intended for use shipping
	 * file data.
	 *
	 * Allocates a buffer sufficient to hold the string, and returns
	 * it.  Returns length of string (which may be zero), or -1 on failure.
	 * The length returned INCLUDES the terminating null.  (Hence a length
	 * of zero represents a NULL pointer.)
	 *
	 * The string sent in the message includes the null terminator
	 * (except if the length is zero, which means no string, not "").
	 */
	unsigned short s;
	int len;

	if (read(fd, &s, sizeof(s)) != sizeof(s)) return -1;
	len = ntohs(s);
	if (!len) {*ptr = NULL; return 0;}

	/* Check whether we have used up all our static string pointers */
	if (stringindx >= MAXMSGSTRING) {
		*ptr = tnalloc(char, len);
	}
	else {
	    /* Malloc space for the incoming string */
	    *ptr = stringalloc[stringindx++] = tnalloc(char, len);
	}

	if (read(fd, *ptr, len) != len) return -1;
	return len;
}

#ifdef LATER
#ifdef DEBUG
#undef send_rc
send_debug_rc(msgtyp, fp, req, rc, buf, size)
	int msgtyp;
	FILE *fp;
	char *req;
	int rc;
	char *buf;
	int size;
{
	int i;
	sprintf(debugbuf, "return: type = %d, req = %d, rc = %d\n",
			msgtyp, req, rc);
	write(debugfd, debugbuf, strlen(debugbuf));

	if (size > 0) {
	    for (i = 0; i < size; i++) {
		sprintf(debugbuf, "%#x ", buf[i]);
		write(debugfd, debugbuf, strlen(debugbuf));
	    }
	    write(debugfd, "\n", 1);
	}
	else write(debugfd, "no extras\n", 10);
	return send_rc(msgtyp, fp, req, rc, buf, size);
}
#endif
#endif /* LATER */
