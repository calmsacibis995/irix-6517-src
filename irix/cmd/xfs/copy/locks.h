#define ACTIVE		1
#define INACTIVE	2
#define UNUSED		0

extern int	*target_states;

/*
 * ugh.  the position/buf_position, length/buf_length, data/buffer pairs
 * exist because of alignment constraints for direct i/o and dealing
 * with scenarios where either the source or target or both is a file
 * and the blocksize of the filesystem where file resides is different
 * from that of the filesystem image being duplicated.  You can get
 * alignment problems resulting in things like ag's starting on
 * non-aligned points in the filesystem.  So you have to be able
 * to read from points "before" the requested starting point and
 * read in more data than requested.
 */

typedef struct working_buffer  {
	int		id;		/* buffer id */
	size_t		size;		/* size of buffer -- fixed */
	size_t		min_io_size;	/* for direct i/o */
	off64_t		position;	/* requested position */
	size_t		length;		/* length of buffer (bytes) */
	char		*data;		/* pointer to data buffer */
} wbuf;

typedef struct thread_state_control  {
	usema_t		*mutex;
/*	int		num_threads; */
	int		num_working;
	wbuf		*buffer;
} thread_control;

typedef int thread_id;
typedef int tm_index;			/* index into thread mask array */
typedef __uint32_t thread_mask;		/* a thread mask */

/* function declarations */

thread_control *
thread_control_init(thread_control *mask, int num_threads);

wbuf *
wbuf_init(wbuf *buf, int data_size, int data_alignment, int min_io_size, int id);

void
buf_read_start(void);

void
buf_read_end(thread_control *tmask, usema_t *mainwait);

void
buf_read_error(thread_control *tmask, usema_t *wait, thread_id id);

void
buf_write_start(void);

void
buf_write_end(thread_control *tmask, usema_t *mainwait);

