#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/stream.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

/* One record for each file that points to a particular stream (with
 * vnode, snode and commonvp).
 */
typedef struct file_rec {
	struct file_rec *next;
	kaddr_t 		 fp;
	kaddr_t 		 vp;
	kaddr_t 		 sp;
	kaddr_t 		 cvp;
} file_rec_t;

/* One record for each stream open on the system
 */
typedef struct stream_rec {
	kaddr_t 		 str;
	file_rec_t 		*file;
} stream_rec_t;

/* Streams resources statistics struct (one each for streams, queues,
 * etc. Extracted from strstat.h).
 */
typedef struct {
	int use;    /* current item usage count */
	int total;  /* total item usage count */
	int max;    /* maximum item usage count */
	int fail;   /* count of allocation failures */
} alcdat;

/* Local function prototypes
 */
struct stream_rec *find_stream_rec();

#define STR_NUM_DZONE 3

#define _OTHERQ(q, qp) (kl_uint(K, qp, "queue", "q_flag", 0) & QREADR ? \
						  (q) + QUEUE_SIZE(K) : (q) - QUEUE_SIZE(K))
