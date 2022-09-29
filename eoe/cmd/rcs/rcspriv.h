/* This file includes definitions, etc. which are private to RCS,
 * but used by the API implementation.
 *
 * This file, like its companion rcsapi.h, may be used without regard
 * to GNU restrictions, as the code is independent of GNU.
 *
 * Some implementations of the APIs do interface with the GNU code,
 * and thus may not be reused without accepting the GNU restrictions.
 */
#include "rcsapi.h"

/* Header of message that transmits a file */
struct sendFileHdr {
    unsigned long mtime;	/* modification time, htonl order */
    unsigned long mode;		/* stream mode, htonl order */
    unsigned long flags;	/* htonl order */
    unsigned short fnamelen;	/* filename length, incl. NULL (htons order)
				 * 0 == no filename
				 * 1 == filename is "" (just trailing \0)
				 */
    unsigned short pad;		/* guarantees filling out struct */
};
    /* flags for this header: */
#define FILE_TMP 0x1		/* "file" is only a temp file */


/* Header of message that transmits only a file's attributes: RCS_FATTR  */
struct sendFileAttr {
    unsigned long handle;	/* RCSARCH handle, htonl order */
    unsigned long mtime;	/* modification time, htonl order */
    unsigned long mode;		/* stream mode, htonl order */
};


/* Header of message that transmits archive field values */
struct sendFieldVal {
    unsigned long locknum;	/* number of locks */
    unsigned long accessnum;	/* number of access entries */
    unsigned long symnum;	/* number of symbols */
    unsigned long namenum;	/* number of name/val pairs */
    unsigned long branchnum;	/* number of branches in selected revision */
    unsigned long totalrev;	/* total number of revisions in archive */
    unsigned long selectrev;	/* number of selected revisions */
    unsigned long inserted;	/* lines inserted into selected rev */
    unsigned long deleted;	/* lines deleted from tselected rev */
    unsigned long strict;	/* boolean; strict locking */
};


/* Flag for rcsOpenArch return code */
#define RCS_ARCH_NOT_EXIST	0x4000	/* archive opened, but doesn't exist */


/* Types of messages (first byte of message identifies type) */
#define RCS_FTP_PUT	1	/* "put" a file */
#define RCS_FTP_GET	2	/* "get" a file */
#define RCS_RETURN_CODE	3	/* return code (an unsigned long) */
#define RCS_RPC		4	/* a command - next byte indicates which */
#define RCS_FATTR	5	/* data message - file (stream) attributes */
#define RCS_HDR_DATA	6	/* return of rcs hdr info (GetHdr) */

/* Types of RPC requests */
#define RCS_RPC_CONNECT 1	/* open a connection (session) */
#define RCS_RPC_OPEN_ARCH 2	/* open an archive file */
#define RCS_RPC_CLOSE_ARCH 3	/* close an archive file */
#define RCS_RPC_CHECKIN 4	/* check in a "file" into an "archive" (hdls) */
#define RCS_RPC_CHECKOUT 5	/* check out an archive into a temp file */
#define RCS_RPC_CHGHDR 6	/* change header info into arch (fixed fields)*/
#define RCS_RPC_GETHDR 7	/* get header information (fixed fields) */

/* Number of temp files generally available (TEMPNAMES), and number
 * of temp files used internally by the base RCS code (TMPRESERVED)
 */
#define TEMPNAMES  64
#define TMPRESERVED 5 /* must be at least DIRTEMPNAMES (see rcsedit.c) */

/* Memory management structs and routines.  Supports multiple heaps.
 * To use a heap, you must have a struct rcsHeap, with its .block member
 * initialized to NULL.
 */
struct rcsHeap {
	void *block;		/* allocated memory */
	struct rcsHeap *next;	/* next heap entry */
	struct rcsHeap **free;	/* ptr to ptr to heap block that can store
				 * the next block allocated.
				 * only meaningful in head block of heap.
				 * typically points to a "next" member
				 */
};

/* rcsmmgr.c */
void * rcsHeapAlloc(struct rcsHeap *, size_t);
void rcsHeapFree(struct rcsHeap *);

/* Internal routines */
/* rcsproto.c */
unsigned long getrc(RCSCONN, int);
int sendpacket (RCSSTREAM *outputStream, const void *data, size_t nbytes);

/* rcsReadInData.c */
extern RCSCONN tmpfileconn[];
