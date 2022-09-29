#ifndef RCSAPI_H
#define RCSAPI_H
#include <sys/param.h>		/* time_t, mode_t */
#include <sys/stat.h>		/* struct stat */

typedef int bool;

typedef struct rcsTmp *RCSTMP;
typedef struct rcsArch *RCSARCH;
typedef struct rcsConn *RCSCONN;
typedef struct rcsStream RCSSTREAM;

#define ERR_OUT_OF_FILES	-10	/* rsrc limit on simultaneous files */
#define ERR_BAD_HOST		-11	/* bad name for server host */
#define ERR_BAD_SERVICE		-12	/* bad name for server service */
#define ERR_CANT_OPEN_SOCKET	-13	/* socket(2) failure */
#define ERR_CANT_CONNECT	-14     /* connect(2) failure */
#define ERR_BAD_FILEOPEN	-15	/* open(2) failure: internal error  */
#define ERR_BAD_FILEWRITE	-16	/* write(2) to file failed */
#define ERR_BAD_UTIME		-17	/* utime(2) failed */
#define ERR_NO_MEMORY		-18	/* out of memory (malloc failure) */
#define ERR_TOOMANY_CONNECTIONS	-19	/* only one connection in linked mode */
#define ERR_NOHANDLES		-20	/* too many tmp file handles in use */
#define ERR_OPEN_ARCH		-21	/* cannot open archive file (eg perm)*/
#define ERR_ARCHIVE_INUSE	-22	/* archive file locked by other proc */
#define ERR_NO_ARCHIVE		-23	/* requested archive does not exist */
#define ERR_ARCHIVE_CHANGED	-24	/* archive no longer a regular file,
					 * or contents changed when not allowed
					 */
#define ERR_NAME_USED		-151	/* -n<nameflag> w/ name already used */
#define ERR_REV_LOCKED		-152	/* ci when other used locked revision */
					/* co -u/-l when other holds lock */
#define ERR_READ_STREAM		-190	/* error reading appl's read stream */
#define ERR_WRITE_STREAM	-191	/* error writing appl's write stream */
#define ERR_BAD_HANDLE		-192	/* handle arg bad (e.g. not alloc'd) */
#define ERR_BAD_OPENMODE	-193	/* attempt to update arch opened r/o */
#define ERR_BAD_OLDREV		-194	/* old rev is unknown or lex. bad */
#define ERR_BAD_NEWREV		-195	/* new rev is unknown or lex. bad */
#define ERR_BAD_LOCK		-196	/* ci w/unlocked file, or ambig. lock */
					/* (co -u w/ambiguous lock)        */
#define ERR_ARCHIVE_EXISTS	-197	/* ci -i, when archive file exists */
#define ERR_APPL_ARGS		-198	/* bad application argument values */
#define ERR_NO_PERM		-199	/* no permission to open file */
#define ERR_NO_FILE		-200	/* appl. specified nonexistent file */
#define ERR_NO_ACCESS		-201	/* update w/user name not in  ACL */
#define ERR_BAD_JOINREV		-202	/* bad revision in join/merge request */
#define ERR_READONLY		-203	/* attempt to set readonly value */
#define ERR_BAD_NAME		-204	/* bad name of field */
#define ERR_BAD_VALUE		-205	/* value outside of permitted range */
#define ERR_BAD_FCREATE		-206	/* cannot create requested user file */
#define ERR_PARTIAL		-250	/* some success; check subcodes */
#define ERR_INTERNAL		-300	/* coding problem (internal bug) */
#define ERR_NETWORK_DOWN	-301	/* read/write on socket failing.
					 * connection has been terminated
					 */
#define ERR_FATAL		-302	/* fatal err (internal, nonrecoverable)
					 * connection has been terminated
					 */
#define ERR_SYSTEM		-303	/* transient system error (e.g. space)*/

/* ============== API Option Structs and Initialization Routines =========== */


/* --------------------------- struct rcsConnOpts -------------------------- */

/* Type of connection to make (i.e. full network, or intramachine)
 */
enum rcsConnType {
    RCS_CONN_DFLT = 0,	/* default: fork w/intramachine messages, if possible */
    RCS_CONN_NTWK	/* force network socket connection */
};

/* Options describing per-connection behaviour.  The first set of options
 * pertain to the client/server connection (what might, in SQL, correspond to
 * a SQL-connection); the latter set of options pertain to the behaviour
 * of the server itself (what might, in SQL, correspond to a SQL-session).
 *
 * All options are evaluated when the connection is established.
 */
struct rcsConnOpts {
    /* Server-oriented options */
    bool Ttimeflag;		/* -T flag to be used on all actions:
				 * adjust RCS archive file modification time,
				 * to input stream date (ci) or original
				 * time (if no substantial changes)
				 */
    bool noquietflag;		/* Turn off quiet (default is -q; quiet "on" */
    const char *suffixes;	/* -x: default if ,v/, */
    const char *debugout;	/* name of server file for debug info */
    

    /* Connection-oriented options */
    enum rcsConnType ctype;	/* Type of (network) connection */
    void (*interrupt)(int sig);	/* application interrupt handler - called
				 * after library does its own cleanup
				 * (SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
				 *  SIGXCPU, SIGXFSZ)
				 */
};
    
    
    /* Initialize to defaults: no application interrupt handler */
int rcsInitConnOpts(struct rcsConnOpts *opts);
#define rcsInitConnOpts(opts) \
	(memset(opts, '\0', sizeof(struct rcsConnOpts)), 0)

/* -------------------------- struct rcsStreamOpts ------------------------- */

/* Options for reading data streams.  Only set these if you want
 * other than default behaviour (see rcsReadInData()).
 *
 * The application create Output Method must also recognize these
 * options.  (How it makes use of these options is up to the application
 * designer; the library-provided rcsFile stream routine will create
 * a file of the given name, with the mode and modification time indicated.)
 */
struct rcsStreamOpts {
    time_t mtime;	/* modification time of stream 
			 * default on input: "now" (zero means default)
			 * default on output: none (library always specifies)
			 */
    mode_t mode;	/* mode of stream
			 * default on input: 0444 (zero means default)
			 * default on output: none (library always specifies)
			 */
    const char *fname;  /* name of stream ("file")
			 * default on input: NULL
			 * *SEE rcsReadInData() FOR SEMANTICS IF YOU
			 * CHOOSE TO OVERRIDE*
			 *
			 * default on output: none (not used)
			 */
};

int rcsInitStreamOpts(struct rcsStreamOpts *opts);
    /* Initialize to defaults */

/* ------------------------ struct rcsCheckInOpts -------------------------- */

/* Options for checking in a stream (creating a new revision) */
struct rcsCheckInOpts {	/* mostly ci(1)-like flags */
    bool lockflag;		/* -l (lock new revision) */
    bool forceflag;		/* -f (force delta even if no change) */
    bool keywordflag;		/* -k (use keyword values from inputstream) */
    bool mustexist;		/* -j (do not create file if it doesn't exist)*/
    bool initflag;		/* -i (initialize; file must not exist) */
				/* Note: if both initflag and mustexist are
				 * set, rcsCheckIn() must return a failure.
				 */
    const char *msg;		/* -mmessage (set revision message) */
    char *nameflag;		/* -nname[:[rev]] */
    char *Nameflag;		/* -Nname[:[rev]] */
    char *stateflag;		/* -s[state] */
    const char *textflag;	/* -t[textfile] (change file desc.) */
    RCSTMP textfile;		/* handle to file with desc. (-t<textfile>)*/
				/* Note: use textflag or textfile; not both */
    char *dateflag;		/* -d[date] (checkin date) */

    const char *oldrev;		/* old revision at which new revision
				 * is to be checked in
				 */
    const char *newrev;		/* new revision number */


    RCSTMP *outHandle;		/* if not null, then the revision is checked
				 * out, and a handle to the revision returned
				 * here.
				 */
    struct rcsStreamOpts *outOpts;  /* Attributes of checked out file (if any)
				     * returned here (if not null).
				     */
    bool serverfile;		/* Hint to server to keep output on same
				 * filesystem as archive file (for later rename)
				 */
};

    /* Initialize to defaults */
int rcsInitCheckInOpts(struct rcsCheckInOpts *opts);
#define rcsInitCheckInOpts(opts) \
	(memset(opts, '\0', sizeof(struct rcsCheckInOpts)), 0)


/* ------------------------ struct rcsCheckOutOpts ------------------------- */

/* Options for checking out a stream (extracting an existing revision) */
struct rcsCheckOutOpts { /* mostly co(1)-like flags */
    bool lockflag;		/* -l (lock checked out revision) */
    bool unlockflag;		/* -u (unlock checked out revision) */
				/* Don't specify both lockflag and unlockflag */
    const char *rev;		/* -r[rev] (revision to check out) */
    const char *joinflag;	/* -jjoinlist (revisions to merge) */
    const char *stateflag;	/* -s[state] */
    const char *dateflag;	/* -d[date] (checkin date) */

    const char *keyexpand;	/* -k[type]; keyword expansion: value is
				 * entire flag, starting with 'k'
				 */
};

    /* Initialize to defaults */
int rcsInitCheckoutOpts(struct rcsCheckOutOpts *opts);
#define rcsInitCheckOutOpts(opts) \
	(memset(opts, '\0', sizeof(struct rcsCheckOutOpts)), 0)

/* ------------------------- struct rcs*HdrOpts ---------------------------- */

/* Name/Value pairs; name may be qualified by application name space, revno */
struct rcsNameVal {
    const char *name;		/* name of field in RCS file (e.g. locks) */
    const char *revision;	/* for fields contained in revisions, which
				 * revision to use in locating field.
				 * "" revision may mean "top" revision
				 * for some standard (public) fields, so
				 * use NULL for admin (file) header fields.
				 */
    const char *namespace;	/* name for private namespace;
				 * NULL or "" means standard (public) fields
				 */
    const char *val;		/* value of field; for fields with multiple
				 * values, layout depends upon whether value
				 * is being set or retrieved; see rcsChangeHdr,
				 * rcsGetHdr
				 */
    unsigned int flags;		/* Modify action (for rcsChangeHdr, q.v.) */
#define rcsNameValUNIQUE	0x1	/* only set field if new (-n vs. -N)*/
#define rcsNameValDELETE	0x2	/* delete field or value, depending
					 * upon particular field
					 */
#define rcsNameValFILE		0x4	/* value is name of file (-A) */
    int errcode;		/* field-specific error returned */
};

/* Options for changing field values in an RCS file */
struct rcsChangeHdrOpts {
    bool initflag;		/* create new RCS file (just header) */
    bool nomail;		/* -M - no mail when breaking a lock */
    const char *mailtxt;	/* message to mail owner of lock to break */
    const char *obsRevisions;	/* -o<range> - obsolete (delete) revisions */
    
};

/* Options for fetching field values in an RCS file */
struct rcsGetHdrOpts {

    unsigned short hdrfields;	/* which header fields to report (see below) */

#define RCS_HDR_BASIC	0x1	/* "basic" parts of header:
				 * rcs pathname,
				 * pathname of workfile (as if on server),
				 * head (top revision),
				 * default branch,
				 * locks,
				 * access list,
				 * comment leader,
				 * keyword substition string (e.g. "o", "v"),
				 * total revisions in file,
				 * revisions selected (unless RCS_REV_NONE set)
				 */
#define RCS_HDR_TEXT	0x2	/* descriptive text */
#define RCS_HDR_SYMBOLS	0x4	/* symbolic names */

				/* rlog -h  = RCS_HDR_BASIC | RCS_HDR_SYMBOLS
				 * rlog -t  = as above | RCS_HDR_TEXT
				 * rlog -N  = ~RCS_HDR_SYMBOLS
				 */

				/* zero (no flags) = all header information */

    bool nogetrev;		/* if set, do not retrive any revision info
				 * rlog -t, rlog -h imply nogetrev set
				 */

    bool defaultbranch;		/* include the default branch revisions
				 * as if selected with RCS_REV_REV
				 * (rcs -b)
				 */
    bool onlylocked;		/* only report on files where locked revisions
				 * have been selected (rcs -L)
				 */
    bool onlyRCSname;		/* only report RCSname field - overrides
				 * all formatting options, including
				 * hdrfields, revfields (implies RCS_REV_NONE).
				 * Does not write to output stream.
				 * (rcs -R)
				 */
    struct rcsNameVal *namesel;	/* array of specific namespace/names to
				 * look up.
				 */
    int nameselnum;		/* number of namespace values to look up */

    const char *namespace;	/* Look up all values in this namespace.
				 * "*" means all namespaces.
				 * Null means do not look up namespaces
				 * (except possibly specifice entries, in
				 * namesel).
				 */

    struct rcsNameVal *revsel;	/* revision selection criteria :
				 * name: "revision" (rlog -r)
				 *       "state" (rlog -s)
				 *	 "locker" (rlog -l)
				 *	 "date" (rlog -d)
				 *	 "writer" (rlog -w)
				 * (Only the first character is significant,
				 *  e.g. "dxx" means the same as "date")
				 *
				 * value: as described in rlog, though
				 * each value/range to match may be specified
				 * in a separate name/value entry, rather than
				 * as part of a comma-separated list
				 */
    int revselnum;		/* number of revision selection criteria */

};

/* Returned field values */
struct rcsGetHdrFields {
    const char *rcspath;	/* RCS file pathname */
    const char *workpath;	/* pathname (derived) of workfile */
    const char *head;		/* head revision number */
    const char *branch;		/* default branch */
    int strict;			/* boolean: strict locks? */

    const struct rcsNameVal **locks;
				/* null-term array of lock name/revno ptrs */
    const char **access;	/* null-terminated array of access list names */
    const struct rcsNameVal **symbols;
				/* null-term array of symbol/revno ptrs */

    const char *comment;	/* comment string */
    const char *keysub;		/* keyword substitution string */
    int totalrev;		/* total revisions in file */
    int selectrev;		/* number of selected revisions */
    const char *description;	/* descriptive text */

    const struct rcsNameVal **names;
				/* null-terminated array of namespace name/val
				 * ptrs
				 */

			/* First selected revision info is returned here: */
    const char *revision;	/* first revision number selected */
    const char *locker;		/* locked by (if any) */
    const char *date;		/* date of revision */
    const char *author;		/* author of revision */
    const char *state;		/* state of revision */
    int   inserted;		/* lines inserted */
    int   deleted;		/* lines deleted */
    const char **branches;	/* null-terminated array of branches */
    const char *nextrev;	/* next revision */
};
    

    /* Initialize to defaults */
int rcsInitChangeHdrOpts(struct rcsChangeHdrOpts *opts);
#define rcsInitChangeHdrOpts(opts) \
	(memset(opts, '\0', sizeof(struct rcsChangeHdrOpts)), 0)

int rcsInitGetHdrOpts(struct rcsGetHdrOpts *opts);
#define rcsInitGetHdrOpts(opts) \
	(memset(opts, '\0', sizeof(struct rcsGetHdrOpts)), 0)

/* ========================== Other API Structs ============================ */


/* ============================== API Declarations ========================= */

/* ----------------------------- rcsOpenConnection() ---------------------- */

int rcsOpenConnection(
    const char *host,		/* name of server machine (gethostbyname()) */
    struct rcsConnOpts *opts,	/* see below for meaning of all options */
    RCSCONN *conn		/* connection handle returned to caller */
);

    /* Establish a connection between a client and a server.  Returns
     * a handle for the connection in *conn.  Returns zero on success,
     * negative error code on failure.
     *
     * host	- hostname of server machine.  NULL (or "") implies
     *		  same machine as client.
     * opts	-
     *		service - name of service requested (NULL means "rcssrv")
     *		interrupt - application signal handler to be called while
     *			    connection is active
     *		Ttimeflag - -T flag applied on operations that affect RCS
     *			    archive file.  That is, the archive modification
     *			    time is set to match the input stream time
     *			    (for operations using an input stream, i.e. ci),
     *			    or the archive modification time is unchanged
     *			    (if no substantial changes made to archive)
     *		noquietflag - do not apply -q flag (normally run "quiet").
     *			    Only meaningful if debugout is not NULL.
     *	        auth	 - author (-w); defaults to login of user
     *	        suffixes - archive location/suffixes (-x flag)
     *	        debugout - name of server file to use for debug messages.
     *			   When used with noquietflag, more messages printed.
     *			   File is truncated when connection is established.
     *	        conntype - RCS_CONN_DFLT: default - fork if server and client
     *					on same host, else network connection
     *			   RCS_CONN_NTWK: use network conn, even if same host
     * conn	- returned handle for new connection (input to other APIs)
     */

int rcsCloseConnection(
    RCSCONN conn		/* connection to close */
);

    /* Close a connection; invalidates all handles associated with the
     * connection.
     *
     * Returns: 0 (success), ERR_APPL_ARGS (invalid connection)
     */

/* ------------------------------- rcsReadInData() ------------------------ */

int rcsReadInData(
    int (*readMethod)(RCSSTREAM *inputStream, void **ptr, size_t nbytes),
    RCSSTREAM *inputStream,
    RCSCONN conn,
    struct rcsStreamOpts *opts,
    RCSTMP *hdl_ptr
);

    /* Read in a complete data stream (usually representing data to
     * be used to create one or more new revisions).
     *
     * Returns: handle to the data, to identify the data to subsequent
     *		function calls (typically, rcsCheckin()).
     *
     *		return value is zero (success) or negative error code.
     *
     * Parameters:
     *    readMethod() - a function pointer that this routine may
     *		call in order to read the data from the stream.
     *		This function executes a blocking (synchronous) read.
     *
     *		Returns: number of bytes read (0 == EOF, -1 == error).
     *
     *		Its parameters are:
     *		    inputStream - same as inputStream parameter to
     *	                          rcsReadInData().  It is a pointer
     *				  to whatever context is required by
     *				  the implementation to make the fcn
     *				  work.
     *              ptr - the address of the data read is returned as *ptr.
     *		    nbytes - maximum number of bytes desired.
     *
     *          This method may be invoked multiple times by rcsReadInData(),
     *          but will not be invoked by the library once readMethod()
     *          returns an error or EOF, or after rcsReadInData() returns.
     *
     *	inputStream - a context passed to readMethod(), to allow the
     *		readMethod() implementation to work.  The context could,
     *		e.g., contain a FILE * pointer (if the implementation were
     *		built upon stdio), or a file descriptor to a socket
     *          (if the application were connected to another process
     *          generating the input stream), or any other data necessary
     *          to provide an input stream.
     *
     *  conn - an opaque handle representing a previously established
     *		connection with the server.  Each read in data stream is
     *		associated with a particular connection, and remains valid
     *		no longer than the duration of that connection.
     *
     *  opts -
     *		mtime - the modification time of the stream.  This may
     *			represent the time that the stream data were
     *			created/changed, or the current time.  A value
     *			of zero is interpreted as the current time.
     *			For a file, this would be (in struct stat) st_mtime.
     *
     *			This time may be used for such tasks as setting
     *			the time of a revision created from the input stream,
     *			and setting the time of the RCS archive file
     *			(if so desired, and indicated using appropriate
     *			option flags).
     *
     *		mode -	the mode of the stream.  RCS uses the mode to set
     *			the mode of a newly created archive file (only the
     *			read and execute bits are used).  Shell scripts
     *			should have the execute bit(s) on, so that the
     *			scripts checked out from the archive also have execute
     *			bit(s) on.
     *	
     *			A value of zero is interpreted as 0444 (r--r--r--).
     *			This is the default.  For a file, mode would be
     *			(in struct stat) st_mode.
     *
     *		fname - Typically NULL (default).  Setting this to provide
     *			an input file name changes the semantics of
     *			rcsReadInData(), and should be used sparingly.
     *
     *			If fname is specified (and is not ""), then the
     *			input stream is coming from a file.  The library
     *			will attempt to use the named file on the server,
     *			which will probably not make sense if the caller
     *			is linked with library stubs (and the server is
     *			a different machine).
     *
     *			The value of this option is that it may save one
     *			act of copying the input stream.  (It is somewhat
     *			analagous to lp -c, in reverse; the default for
     *			RCS is to make copies, while the default for lp
     *			is not to.)
     *
     *			The same problems that lp encounters when it does
     *			not copy a file may be encountered here.  Notably,
     *			that the file changes while the operation takes
     *			place.  (As with lp, merely calling rcsReadInData()
     *			does not guarantee that the file is read immediately,
     *			if fname is specified.)  RCS attempts to mitigate
     *			these problems by checking that the file does not
     *			change in the middle of any action (like rcsCheckin()),
     *			but does not protect against the file changing
     *			between two operations.  So, one could ReadIn a
     *			file, change the file, then invoke rcsCheckin(), and
     *			RCS would be quite happy, using the newer version
     *			of the file.
     *
     *			Since RCS will read in the file as needed (and not
     *			start until rcsCheckIn() is called), the application
     *			must take particular care that the file not change.
     *			Otherwise, results may be unpredictable, depending
     *			upon when the file were to change. 
     */ 


/* ------------------------------ rcsReadOutData() ------------------------ */

int rcsReadOutData(
    int (*writeMethod)
		(RCSSTREAM *outputStream, const void *ptr, size_t nbytes),
    RCSSTREAM *outputStream,
    RCSCONN conn,
    RCSTMP datahandle
);

    /* Read out a complete data stream (usually representing data output
     * from a previous checkout).
     *
     * Returns: 0 (success)
     *		ERR_APPL_ARGS: bad arguments (e.g. null pointers, invalid
     *				handle)
     *		ERR_WRITE_STREAM: call to writeMethod() failed
     *		ERR_NETWORK_DOWN: unable to transmit entire data stream
     *
     * Parameters:
     *    writeMethod() - a function pointer that this routine may
     *		call in order to write the data from the library.
     *		This function executes a blocking (synchronous) write.
     *
     *		Returns: number of bytes written (<= 0 means error;
     *			 it is not required to write the full amount
     *			 requested on each call, but it must write some
     *			 non-negative number of bytes on each call.
     *
     *		Its parameters are:
     *		    outputStream - same as outputStream parameter to
     *	                          rcsReadOutData().  It is a pointer
     *				  to whatever context is required by
     *				  the implementation to make the fcn
     *				  work.
     *              ptr - the address of the data to be written.
     *		    nbytes - number of bytes to be written.
     *
     *          This method may be invoked multiple times by rcsReadOutData(),
     *          but will not be invoked by the library once writeMethod()
     *          fails, or after rcsReadOutData() returns.
     *
     *	outputStream - a context passed to writeMethod(), to allow the
     *		writeMethod() implementation to work.  The context could,
     *		e.g., contain a FILE * pointer (if the implementation were
     *		built upon stdio), or a file descriptor to a socket
     *          (if the application were connected to another process
     *          generating the input stream), or any other data necessary
     *          to provide an input stream.
     *
     *  conn - the connection through which the data stream may be retrieved
     *
     *  datahandle - an opaque handle representing a previously established
     *		     set of data, usually from rcsCheckOut() (or from
     *		     rcsCheckIn() if an output stream is requested).
     */


/* ------------------------------- rcsCheckIn() --------------------------- */

int rcsCheckIn(
    RCSCONN conn,		/* Connection to server */
    RCSTMP inputhandle,		/* "handle" to previously ReadIn stream,
				 * used as data for new revision
				 */
    const char *rcspath,	/* full pathname (on server) of archive file */
    struct rcsCheckInOpts *opts	/* a subset of the ci(1) flags; those
				 * flags that affect the arcive file directly
				 * (see struct RcsCheckInOpts, above)
				 */
);

    /* Check in a new revision of the archive.
     * opts -
     *	lockflag:  if set, lock the newly created revision (ci -l)
     *  forceflag: if set, allow new revisions which are unchanged
     *		   from old revisions.  If not set, and the new revision
     *		   represents no change, the lock on the old revision
     *		   (if any) is removed, and the nameflag and stateflag
     *		   options (if any) are applied to the old revision.
     *		   In that case, this function returns 1, rather than zero.
     *		   (ci -f).
     *  keywordflag: use keyword values from input stream, rather than
     *		   other options here, to determine new revision number,
     *		   creation date, state, and author.  If these options
     *		   are also set (e.g. stateflag), they take precidence.
     *		   (ci -k)
     *  nameflag:  set the name of the checked in revision to this value.
     *		   The checkin fails if <nameflag> is already used;
     *		   ERR_NAME_USED is returned. (ci -n<nameflag>)
     *	Nameflag:  set the name of the checked in revision; overrides
     *		   preexisting use of <Nameflag>.  (ci -N<Nameflag>)
     *  stateflag: set the state of this revision (ci -s).  The default
     *		   state is Exp.
     *  textflag:  override the existing text (archive description)
     *		   with the value of this flag (ci -t-<textflag>).
     *		   In order to effect ci -t<file>, the application must
     *		   convert the file contents to a string.
     *		   [NOTE: an alternative would be to require the application
     *		    to ReadIn the text stream, and then textflag would be
     *		    an RCSTMP handle; only one of these mechanisms will be
     *		    used, though.]
     *  dateflag:  set the time/date of the revision to the value of
     *		   this flag (ci -d).
     *
     * oldrev -  a revision number (e.g. 1.3.2.1) to be used as the
     *		old revision, from which a new revision is made.
     *		If this is not specified (NULL), it is inferred using
     *		the usual ci rules (looking for locked revision by the
     *		user, using the new revision number, etc.).
     *		If this is specified, it must be a specific revision
     *		and not a branch; i.e. it must contain an even number of
     *		components, and not be something like 1.31.2.
     *		The effect of specifying oldrev is as if the old revision
     *		had been locked by this user, and ci located the old
     *		revision using its usual rules for finding locked files.
     *		Note that you cannot check in using an oldrev which has
     *		been locked by another user.  ERR_REV_LOCKED is returned.
     *
     * newrev - this is equivalent to the <rev> value that the ci(1) command
     *		takes on flags like -l<rev>, -u<rev>, etc.  If not specified,
     *		the usual rules for computing the new revision number apply.
     *
     *		As usual, it is impermissible to specify a newrev which is
     *		not higher than the oldrev (whether the oldrev is stated
     *		explicitly, or inferred by RCS).
     *
     * outHandle - If not null, provide a handle to the newly checked in
     *		revision (which may be retrieved with rcsReadOutData()).
     *		This allows implementation of ci -u (or ci -l if lockflag
     *		is set).  The handle to the revision is returned as *outHandle.
     *
     * outOpts - the modification time and mode attributes of the output stream.
     *		If this is null, but outHandle is not, the output stream is
     *		still produced, but the attribute information is not returned.
     *
     * serverfile - if true, the library is alerted to the possibility that
     *		the output data may ultimately be deposited in a server file
     *		on the same filesystem as the archive file.  (A performance
     *		matter only.)
     *
     * Once a revision is checked in, the application must close the archive,
     * and reopen it to perform additional actions (implementation limitation).
     * Thus, outHandle (along with outOpts) is particularly useful.
     *
     * Returns: 0 - success
     *		1 - success, but no checkin (no revision change; see -f)
     *  	negative error codes - failures
     */


/* ------------------------------ rcsCheckOut() -------------------------- */

int rcsCheckOut(
    RCSCONN conn,		/* connection to server */
    const char *rcspath,	/* full pathname (on server) of archive file */
    RCSTMP *outhandle,		/* "handle" for output data */
    struct rcsStreamOpts *outopts, /* place to rcv attributes of output data*/
    struct rcsCheckOutOpts *opts /* checkout options (see below) */
);

    /* Check out a revision of the archive file.
     *
     * The caller is returned a handle (outhandle) to the data, which may
     * then be used to retrieve the data via rcsReadOutData().  If outopts
     * is not null, the modification time and "file" permission mode of
     * the output data is returned.  The modification time is the time
     * of the revision checked out (if any), or "now" (zero) if the
     * check out was of an empty archive, or included a join.
     *
     * The mode of the "file" has the same read/execute bit settings as
     * the archive file.  The owner write bit is turned on if:
     *   -kv was not specified AND 
     *	      lockflag (-l) was specified OR there is not strict locking
     * or if the archive was empty AND lockflag (-l) was specified..
     *
     * opts -
     *  rev: specifies the revision to check out (exactly the same as co -r)
     *		This may be NULL or "" (which is equivalent), in which case
     *		the top revision is retrieved.
     *  lockflag: if set, lock the revision checked out (co -l).
     *  unlockflag: if set, unlock the revision checked out (co -u).  Lockflag
     *		takes precidence if both lockflag and unlockflag set.
     *  joinflag: a list of revisions to merge (co -j)
     *  stateflag: alter the algorithm to find the highest revision with
     *		the specified state to check out (co -s).
     *  dateflag: retrieve the latest revision on the selected branch whose
     *		timestamp is not later than dateflag (co -d).
     *  keyexpand: -k suite of flags (co -k?) that control keyword expansion.
     *		The entire flag (beginning with 'k') must be specified.
     */
    
/* ------------------------------ rcsChangeHdr() -------------------------- */

int rcsChangeHdr(
    RCSCONN conn,		/* connection to server */
    const char *rcspath,	/* full pathname (on server) of archive file */
    struct rcsNameVal *nameval, /* array of name/value pairs to set */
    int namevalNum,		/* number of elements in array, above */
    struct rcsChangeHdrOpts *opts /* header field options (see below) */
);

    /* Change the value(s) of specified fields in the RCS file header,
     * or in the fields associated with revisions within the RCS file.
     * Examples of the former are symbols, locks; examples of the latter
     * are state, date.  RCS file header fields are referred to by
     * specifying a NULL revision in the rcsNameVal struct; an empty
     * string ("") implies the top revision on the trunk or default branch.
     *
     * If multiple entries change the same field value, the last change
     * takes precidence.  If one clears all values, and then others add
     * values (e.g. to the access list), then both have effect.  Basically,
     * all changes are applied in order, whatever that means to the
     * field in question.  (An exception is for "strict"; see below.)
     *
     * If some fields cannot be changed (e.g. attempting to change a
     * READONLY field), but the  operation is generally successful,
     * then ERR_PARTIAL is returned, and the caller must look in
     * nameval[i].errorcode to see which fields could not be set, and why.
     * If namevalNum is 1, the particular error code is also returned,
     * rather than ERR_PARTIAL.  Only errors that are independent of the
     * RCS file's content are flagged this way.  It is not an error,
     * for example, to remove a nonexistent lock (the revision simply
     * remains unlocked).
     * Other return codes indicate complete failure, or complete success (0).
     *
     * Standard fields include:
     * "head"	- read only
     * "branch"	- default branch; if value is NULL or "", branch is
     *		  reset to the (dynamically) highest branch on trunk
     *		  (rcs -b[value])
     , "access"	- appends the login name to the accessor list.
     *		  Value may be a single login, or a comma-separated
     *		  list of logins.  Multiple "access" entries may
     *		  appear in nameval[].  (rcs -a<value>)
     *		  If rcsNameValFILE is set, then the value is actually
     *		  a path to another RCS archive file, from whence
     *		  an access list to append is to be obtained
     *		  (rcs -A<value>).
     *		  If rcsNameValDELETE is set, then the login is to
     *		  be deleted from the accessor list.  If the value
     *		  (login name) is NULL or "", then the entire accessor
     *		  list is deleted.  (rcs -e<value>).
     * "symbols" - associate a symbolic name with a branch or revision.
     *		  Value is of the form <symbolic name>[:[<rev>]]
     *		  If ':' is omitted, or rcsNameValDELETE flag is set,
     *		  then <symbolic name> is deleted from the list of symbols.
     *		  If <rev> is omitted, then the latest revision on
     *		  the current branch is used.
     *		  <rev> may be symbolic; it is converted into a numeric
     *		  revision number before being added to the list of symbols.
     *		  If rcsNameValUNIQUE flag is set, then the <symbolic name>
     *		  must not already appear in the list of symbols.
     *		  Multiple "symbols" entries may appear in nameval[].
     *		  (rcs -n and rcs -N).
     * "locks"	- lock the revision specified by the value (which
     *		  may be a symbolic revision).  This creates an entry
     *		  of the form <login>:<numeric revision>.  The
     *		  numeric revision is derived from the symbolic
     *		  revision specified.  If the revision is omitted,
     *		  the latest revision on the default branch is locked.
     *		  If rcsNameValDELETE is set, then the indicated
     *		  revision is unlocked (the lock is deleted), following
     *		  the usual RCS rules for mapping the value (revision)
     *		  to the lock to be removed.
     *		  If the lock is owned by someone else, the unlock
     *		  fails unless nomail is set, or mailtxt is set
     *		  (providing the mail message to send the owner of
     *		  the lock to be broken).
     *		  As a convenience, the user may specify a value
     *		  in the form <login>:<rev> (instead of simply <rev>).
     *		  However, if <login> does not match the author specified
     *		  or inferred for the connection, an addition of a
     *		  will fail.  The <login> is ignored when deleting
     *		  a lock (only a single user may own a lock on a given
     *		  revision; no checking whether <login> matches the
     *		  lock owner is performed).
     *		  Multiple "locks" entries may appear in nameval[].
     * 		  (rcs -l, rcs -u -M).
     * "strict" - Value is ignored.  If rcsNameValDELETE is set, this
     *		  field is removed from the archive; else it is added
     *		  (if not already there).  (rcs -L, rcs -U).  If "strict"
     *		  is set by any rcsNameVal entry, it will be set even if
     *		  a later entry unsets it.  rcs -L takes precidence over
     *		  rcs -U.
     * "comment"- this field represents the string used to prefix every
     *		  line when expanding the Log keyword during checkout (rcs -c).
     * "expand"	- sets the default keyword substitution to <value>.
     *		  Value must be one of: "k", "o", "v", "kvl".  If "kv"
     *		  is used, the "expand" field is removed from the header.
     *		  It is also removed if rcsNameValDELETE is set (and then
     *		  value is ignored).  If value is any other string, it is
     *		  ignored (but ERR_BAD_VALUE is set). (rcs -k<val>).
     * "desc"   - sets the descriptive text of the file.   Only the
     *		  most current descriptive text (top of trunk or default
     *		  but otherwise is ignored.  (rcs -t-<value>).
     *
     * Standard revision fields include:
     * "date"	- currently read only (could be changed in the future).
     * "author" - currently read only (could be changed in the future).
     * "state"  - a free form string; semantics defined by application.
     *		  (rcs -s).
     * "branches" - read only.
     * "next"   - read only.
     * "log"	- the log message for the revision.  (rcs -m).
     * "text"	- use rcsCheckIn to change!
     *
     * Application fields have no semantics, and are defined by the
     * user.  If rcsNameValDELETE is set, the specified field is deleted;
     * otherwise its value is replaced by the specified value, or if the
     * field does not already exist, it is created.  Likewise, if the
     * application namespace does not already exist, it is created.
     * (An application namespace is never deleted, though it may have no
     *  members.)
     *
     * Other options:
     *  initflag - Create and initialize a new RCS file (which must not
     *		   already exist).  Sets a default descriptive text message
     *		   if no value provided for "desc" (see above). (rcs -i).
     *  nomail	- do not send mail to owner of lock broken.  See "locks",
     *		  above.  (rcs -M).
     *  mailtxt	- the mail message to be sent if a lock is broken.  If
     *		  nomail is not set, and mailtxt is NULL, then the unlock
     *		  fails (but the rest of the operation may still succeed).
     * obsRevisions - what revisions to remove (rcs -o).
     */
    
/* ------------------------------- rcsGetHdr() ---------------------------- */

int rcsGetHdr(
    RCSCONN conn,		/* connection to server */
    const char *rcspath,	/* full pathname (on server) of archive file */
    struct rcsGetHdrOpts *opts, /* header field options (see below) */

    struct rcsGetHdrFields *ret,/* returned field values */

				/* stream for receiving revision data */
    int (*writeMethod)		
		(RCSSTREAM *outputStream, const void *ptr, size_t nbytes),
    RCSSTREAM *outputStream	/* context for stream */
);

    /* Retrieve the value(s) of specified fields in the RCS file
     * (excluding the actual delta text, which is extracted via
     *  rcsCheckOut()).
     *
     * The caller uses various options (opts) to indicate which fields are
     * to be returned, and which revisions are to be reported.
     * The file header fields are returned, parsed, in the rcsGetHdrFields
     * struct, while the revision data are written into the caller-provided
     * stream, in rlog(1) format.
     *
     * The memory used parsed fields is not guaranteed to persist past
     * the next RCS API call, so needed information should be copied into
     * the caller's memory.  Only those fields which are requested are
     * filled in.
     *
     * In essence, this function performs a select and a project on a
     * table (the RCS file).  The projection is described by specifying
     * the particular fields to be retrieved.  The selection is described
     * by specifying particular revisions of interest.
     *
     * If no revision selection criteria are specified, all revisions
     * are reported.  Otherwise, any revision reported must satsify the
     * "state" (rlog -s), "date" (rlog -d), AND "locker" (rlog -l) criteria,
     * if specified.  In addition, if "revision" (rlog -r) or
     * opts->defaultbranch (rcs -b) is specified, then the revision is
     * restricted to matching the "revision" criteria (if specified) OR
     * to being on the main branch (if defaultbranch is set).
     *
     * One exception to this model is opts->onlylocked.  If this is set,
     * NO data for the RCS file will be reported, unless there is a locked
     * revision (or if "locker" is specified, there is a locked revision
     * matching this ONE criterion).
     *
     * Returns: 0, or negative value in case of error.
     *
     * opts - flags and strings describing selections and projections.
     *	    hdrfields	- which file header fields are to be returned.
     *			By default, all fields are reported (zero == default).
     *			These flags are "or'd" together:
     *			RCS_HDR_BASIC: basic fields, includiing RCS pathname,
     *				workfile pathname, head, locks, access list,
     *				comment leader, keyword substitution string,
     *				total number of revisions, and number of
     *				revisions selected (unless nogetrev set)
     *				(rlog -h)
     *			RCS_HDR_TEXT: descriptive text (rlog -t)
     *			RCS_HDR_SYMBOLS: symbols (turned off by rlog -N)
     *
     *     nogetrev	- boolean indicating that revisions not to be retrieved.
     *			(Implied by rlog -t, rlog -h, rlog -R)
     *
     *    defaultbranch - include all default branch revisions in the list
     *			   of revisions to be used as candidates for
     *			   matching the selection criteria. (rlog -b)
     *
     *     onlylocked	- only report information for the file if there is
     *			  a locked revision matching "locker" (if specified)
     *			  or some locked revision (if "locker" not specified).
     *			  (rlog -L).  If there are no matches, return
     *			  ERR_BAD_LOCK.
     *
     *			  Note that even if no locked revision is selected,
     *			  file information may still be reported, since
     *			  some other revision may be locked.  For example,
     *			  if only revision 1.15 is locked, then a call with
     *			  "onlylocked" and "revision" == "1.16" specified
     *			  (rlog -L -r1.16) is successful, even though
     *			  revision 1.16 is not locked.
     *
     *	   onlyRCSname - boolean indicating that only the RCS pathname is
     *			to be returned (and not written to output stream).
     *			Overrides all other field options (but selection
     *			options onlylocked, "locker", still apply).
     *
     *	   namesel	- array of struct rcsNameVal namespace identifiers.
     *			  Values of identifiers are to be looked up and
     *			  returned.  If namespaces are in revisions, and
     *			  nogetrev is specified, these identifiers are not
     *			  looked up.  It is not an error to reqest an
     *			  identifier which is not in the RCS file.  It is
     *			  simply omitted from the return list.
     *
     *     nameselnum  -  the number of entries in namesel.
     *
     *     namespace   -  a string specifying the namespace whose identifiers
     *			  are to be listed in the output stream (and returned)
     *			  for the file header and revision listings.  A "*"
     *			  means all namespaces.  A NULL means no namespaces.
     *
     *     revsel	- array of revision selection criteria
     *			  The name indicates which criterion is specified,
     *			  the value indicates what to match.
     *
     *		name	- (only the first character is significant)
     *				"locker":  match locker (rlog -l)
     *				"state":   match state  (rlog -s)
     *				"date":    match date range (rlog -d)
     *				"revision":match set of revisions (rlog -r)
     *				"writer":  match writer (rlog -w)
     *
     *	        val	-  The values which must be matched.  All may be
     *			   lists of values (though the date value is a
     *			   semi-colon separated list; the others are comma-
     *			   separated).  Alternatively, each value may be
     *			   specified as a different entry in the revsel array.
     *
     *			   Dates are free-form (see ci(1)).
     *
     *			   Revisions are specified as [rev][:][rev].  If
     *			   no revision is given in the string, then the top
     *			   revision is inferred. (rlog -r).
     *
     *    revselnum	- the number of revision selection entries in revsel.
     *
     *
     *    The revision selection rule is:
     *		match any locker (satisfied if no lockers specified) AND
     *		match any date (satisfied if no date or range specified) AND
     *		match any state (satisfied if no date or range specified) AND
     *		be a qualified revision.
     *
     *		A qualified revision is any revision if "defaultbranch"
     *		 is not set, and no "revision" is specified.
     *		Otherwise, a qualified revision must meet the "revision"
     *		 specification (if any) OR be on the main branch (if
     *		 "defaultbranch" is set).
     *
     * ret - returned header field values.  Only those fields requested
     *	     in opts are returned.  They are returned here, even if they
     *	     are also sent to the application stream.
     *
     *    rcspath	- the pathname to the RCS file (including ,v).
     *
     *    workpath	- the filename of the workfile (clear text file).
     *			  If the caller provided the "rcspath" as a clear
     *			  text file (as opposed to a ,v file), then this
     *			  is just the filename component of that value.
     *
     *	  head		- the top (head) revision in the file (string value).
     *
     *    branch	- the default branch (string value).
     *
     *    locks		- a list of locks, represented as
     *			  a null-terminated array of pointers to rcsNameVal
     *			  name/value pairs.  The name is the login owner
     *			  of a lock.  The value is the revision locked.
     *			  Example: *locks[0] == {"wft", "1.2"}, locks[1] == 0
     *			  means the file has a single lock, which is on
     *			  revision 1.2, owned by wft.
     *
     *    strict	- boolean; true if strict locking is set in file.
     *
     *    access	- a null-terminated array of access list names (rcs -a)
     *
     *    symbols	- a list of tags (rcs -n) represented as
     *			  a null-terminated array of pointers to rcsNameVal
     *			  name/value pairs.  The name is the symbol (tag),
     *			  and the value is the associated id.  See locks, above.
     *
     *   keysub		- a string representing the expansion to be performed
     *			  on keywords upon checkout.  (See co -k).
     * 
     *   totalrev	- the total number of revisions in the file.
     *
     *   selectrev	- the number of revisions selected.
     *
     *   description	- the descriptive text (rcs -t).
     *
     *   names		- an array of pointers to namespace name/val pairs.
     *			  *names[0] is the first name/val pair, *names[1]
     *			  is the second, until names[i] is NULL. 
     *
     * writeMethod    - an application stream into which to write the
     *			  revision data (and header data, if "printall"
     *			  is set).  This pointer may be NULL, in which
     *			  case no revision information is returned.
     *			  See rcsReadOutData for the full description of
     *			  a writeMethod stream.
     *
     * outputStream	- the context required by writeMethod, q.v.
     */
    
/* ============================= File Streams ============================== */
/* Generic routines for handling files; these manipulate file streams,
 * much as stdio manipulates FILE * "streams".
 *
 * These are basic wrappers for the stdio routines, provided as a convenience.  
 * The RCS library calls for functions with slightly different signatures
 * than the stdio functions, thus the wrappers.  For instance, the read method
 * returns a pointer to the data read, rather than copying the data to the
 * caller-supplied location.  This improves performance, especially in the
 * case where the input stream is a memory-mapped file.
 */

/* ---------------------------- rcsFileOpen() ------------------------------ */

int rcsFileOpen(
    const char *fname,			/* name of file to open */
    const char *mode,			/* open mode (like fopen) */
    RCSSTREAM **context			/* place in which to return stream */
);

    /* Open a file, and return a pointer to an RCSSTREAM.  The return
     * value is zero, or a negative number in case of an error
     * (e.g. the named file does not exist, or the process does not
     *  have permission to access the file).
     */
 

/* --------------------------- rcsFileRead() ------------------------------- */
int rcsFileRead(
    RCSSTREAM *inputStream,		/* input stream (like FILE *) */
    void **ptr,				/* returns pointer to data buffer */
    size_t nbytes			/* max number of bytes to read */	
);

    /* The generic file reading stream.
     * Applications may use a pointer to this function when asked for
     * a read method on a stream.
     */


/* --------------------------- rcsFileClose() ------------------------------ */
int rcsFileClose(
    RCSSTREAM *inputStream		/* input stream (like FILE *) */
);

    /* Close a file stream; returns zero on success, negative number on error */


/* -------------------------- rcsFileWrite() ------------------------------ */
int rcsFileWrite(
    RCSSTREAM *outputStream,		/* result of rcsFileCreateOutStream()*/
    const void *data,			/* data to be written */
    size_t nbytes			/* number of bytes to write */
);

    /* Write nbytes into the stream (file) created by rcsFileCreateOutStream().
     * Returns the number of bytes written (zero or negative error code on
     * error).
     *
     * This is provided as a convenience to be used with rcsReadOutData().
     * It is a trivial wrapper for fwrite(); if its address were not required,
     * the entire implementation would be:
     *
     * #define rcsFileWrite(outStream, data, nbytes) \
     *		fwrite(data, 1, nbytes, (FILE *)outStream)
     *
     * Do not intermix rcsFileWrite with any of the other rcsFile* routines
     * here.
     */


/* --------------------------- rcsFileStat() ------------------------------- */
int rcsFileStat(
    RCSSTREAM *inputStream,		/* input stream (like FILE *) */
    struct stat *statbuf		/* buffer in which to return stat(2) */
);
    /* Perform an fstat(2), using RCSSTREAM * as input */

#endif /* RCSAPI_H */
