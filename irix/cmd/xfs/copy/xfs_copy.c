#define _SGI_MP_SOURCE

#define _KERNEL 1
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/buf.h>
#undef _KERNEL

#include <signal.h>
#include <ustat.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/uuid.h>
#include <sys/wait.h>
#include <signal.h>
#include <ulocks.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <assert.h>
#ifdef XFSCOPY_HAVE_BEHAVIORS
#include <ksys/behavior.h>
#endif /* XFSCOPY_HAVE_BEHAVIORS */
#include <sys/fs/xfs_macros.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include "sim.h"

#ifdef DEBUG_MALLOC
#include <malloc.h>
#endif

#include "locks.h"

#define	rounddown(x, y)	(((x)/(y))*(y))

/* private */

#define ARENA_NAME	"/usr/tmp/xfs_copy.arenaXXXXXX"
#define LOGFILE_NAME	"/usr/tmp/xfs_copy.log.XXXXXX"
#define MAX_TARGETS	100
#define MAX_THREADS	(MAX_TARGETS + 2)
#define T_STACKSIZE	(1024*16)

char *arena_name;

/* globals */

int	duplicate_uuids;
pid_t	parent_pid;

int	logfd;
char 	*logfile_name;
FILE	*logerr = NULL;

int	source_is_file;		/* is source a file? */
int	target_is_file;		/* is target a file? */

int	direct_io;

char	*source_name, *old_source_name, *tmp_name;
int	source_fd;

__uint64_t	source_blocks;		/* rough count of # to be copied */
__uint64_t	bytes_copied = 0;	/* exact count of bytes copied */

int	source_blocksize;	/* source filesystem blocksize */
int	source_sectorsize;	/* source disk sectorsize */

struct stat64	statbuf;

int	num_targets = 35;
char	**target_names;
int	*target_fds;
off64_t	*target_positions;
pid_t	*target_pids;
int	*target_states;
int	*target_errors;
int	*target_err_types;

typedef struct thread_args {
	int		id;
	usema_t		*wait;
	int		fd;
} t_args;

usptr_t		*arena;
#define	ANYCHILD	-1
#define	NUM_BUFS	1

wbuf		w_buf;
wbuf		btree_buf;

thread_control	glob_masks;

t_args	*targ;
/*
t_args	warg;

pid_t	source_pid;
int	source_state;
*/

usema_t	*mainwait;


/*
 * An on-disk allocation group header is composed of 4 structures,
 * each of which is 1 disk sector long where the sector size is at
 * least 512 bytes long (BBSIZE).
 *
 * Note that presently the 4 structures are laid out in sequence
 * (sb, agf, agi, agfl) but there's no guarantee they'll stay that
 * way.
 *
 * There's one ag_header per ag and the superblock in the first ag
 * is the contains the real data for the entire filesystem (although
 * most of the relevant data won't change anyway even on a growfs).
 *
 * The filesystem superblock specifies the number of ag's and
 * the ag size.  That splits the filesystem up into N pieces,
 * each of which is an ag and has an ag_header at the beginning.
 *
 * When I want an ag_header, I read in the first chunk of an ag and
 * set the pointers to point to the right places in the buffer.
 */

typedef struct ag_header  {
	/* superblock for filesystem or aggregate group */

	xfs_sb_t	*xfs_sb;

	/* free space info */

	xfs_agf_t	*xfs_agf;

	/* free inode info */

	xfs_agi_t	*xfs_agi;

	/*
	 * allocator freelist -- freelist blocks reserved
	 * for use by the btree allocation code and also the
	 * freeblocks of last resort for the ag
	 */

	xfs_agfl_t	*xfs_agfl;

	char		*residue;
	int		residue_length;
} ag_header_t;

/* first fs block that contains real (non-ag-header) data */

xfs_agblock_t	first_agbno = 0;

void
check_errors(void)
{
	int i, first_error = 0;

	/* now check for errors */

	for (i = 0; i < num_targets; i++)  {
		if (target_states[i] == INACTIVE)  {
			if (first_error == 0)  {
				first_error++;
				fprintf(logerr,
				"THE FOLLOWING COPIES FAILED TO COMPLETE.\n");
				fprintf(stderr,
				"THE FOLLOWING COPIES FAILED TO COMPLETE.\n");
			}
			fprintf(logerr, "    %s -- ", target_names[i]);
			fprintf(stderr, "    %s -- ", target_names[i]);
			if (target_err_types[i] == 0)  {
				fprintf(logerr, "write error");
				fprintf(stderr, "write error");
			} else  {
				fprintf(logerr, "lseek64 error");
				fprintf(stderr, "lseek64 error");
			}

			fprintf(logerr,
				" at offset %lld\n", target_positions[i]);
			fprintf(stderr,
				" at offset %lld\n", target_positions[i]);
		}
	}
	if (first_error == 0)  {
		fprintf(stdout, "All copies completed.\n");
		fflush(NULL);
	} else  {
		fprintf(stderr, "See \"%s\" for more details.\n",
			logfile_name);
		exit(1);
	}

	return;
}

/* the prefix should be relatively *short* */

void
do_error2(char *prefix, int errornum, int finalmsg)
{
	size_t tot_len;
	char *errstring, *errstring2;
	char buffer[1024];

	errstring = strerror(errornum);

	tot_len = sprintf(buffer, "%s:  \"%s\"\n", prefix, errstring);

	if (tot_len == 0)
		goto broken;

	/* log an exit message to logfile and stderr, too */

	if (fprintf(logerr, "%s", buffer) == tot_len)  {
		fprintf(stderr, "%s:  \"%s\"\n", prefix, errstring);
		if (finalmsg)
			fprintf(stderr,
				"Check logfile \"%s\" for more details\n",
				logfile_name);
		return;
	}

broken:
	/* crap, logfile is broken, have to write to stderr */

	fprintf(stderr, "xfs_copy:  could not write to logfile \"%s\".\n",
		logfile_name);
	fprintf(stderr, "xfs_copy message was -- %s:  %s\n", prefix, errstring);

	errstring2 = strerror(oserror());

	fprintf(stderr, "Aborting xfs_copy -- logfile error -- reason:  %s\n",
		errstring2);
	exit(1);
}

void
do_error(char *prefix)
{
	do_error2(prefix, oserror(), 1);

	return;
}

/*
 * don't have to worry about alignment and mins because those
 * are taken care of when the buffer's read in
 */

/* ARGSUSED */
void
begin_reader(void *arg, size_t ignore)
{
	t_args *args = arg;
	int res;
	int error = 0;

	/*usadd(arena);*/
	usinit(arena);

	for (;;) {
		uspsema(args->wait);

		buf_read_start();

		/* write */
		if (target_positions[args->id] != w_buf.position)  {
			if (lseek64(args->fd, w_buf.position, SEEK_SET) < 0)  {
				error = 1;
				target_err_types[args->id] = 1;
			} else  {
				target_positions[args->id] = w_buf.position;
			}
		}

		if ((res = write(target_fds[args->id], w_buf.data,
				w_buf.length)) == w_buf.length)  {
			target_positions[args->id] += res;
		} else  {
			error = 2;
		}

		if (error)  {
			goto handle_error;
		}

		buf_read_end(&glob_masks, mainwait);
	}
	/* NOTREACHED */

handle_error:
	/* error will be logged by primary thread */

	target_errors[args->id] = oserror();
	target_positions[args->id] = w_buf.position;

	buf_read_error(&glob_masks, mainwait, args->id);
	exit(1);
}

int kids;

void
killall(void)
{
	int i;

	/* only the parent gets to kill things */

	if (getpid() != parent_pid)
		return;

	for (i = 0; i < num_targets; i++)  {
		if (target_states[i] == ACTIVE)  {
			/* kill up target threads */
			kill(target_pids[i], SIGKILL);
			usvsema(targ[i].wait);
		}
	}
}

void
handler()
{
	pid_t pid = getpid();
	int status, i;
	char buffer[512];

	pid = wait(&status);

	kids--;

	for (i = 0; i < num_targets; i++)  {
		if (target_pids[i] == pid)  {
			if (target_states[i] == INACTIVE)  {
				/* thread got an I/O error */

				if (target_err_types[i] == 0)  {
					fprintf(logerr,
	"xfs_copy:  write error on target %d \"%s\" at offset %lld\n",
						i, target_names[i],
						target_positions[i]);
				} else  {
					fprintf(logerr,
	"xfs_copy:  lseek64 error on target %d \"%s\" at offset %lld\n",
						i, target_names[i],
						target_positions[i]);
				}
					
				sprintf(buffer, "Aborting target %d - reason", i);
				do_error2(buffer, target_errors[i], 0);

				if (kids == 0)  {
	fprintf(logerr, "Aborting xfs_copy - no more targets.\n");
	fprintf(stderr, "Aborting xfs_copy - no more targets.\n");
					check_errors();
					exit(1);
				}

				sigset(SIGCLD, handler);
				return;
			} else  {
				/* it just croaked it bigtime, log it*/

				fprintf(logerr,
	"xfs_copy:  thread %d died unexpectedly, target \"%s\" incomplete\n",
						i, target_names[i]);

				fprintf(logerr,
					"xfs_copy:  offset was probably %lld\n",
					target_positions[i]);
				do_error2("Aborting xfs_copy - reason",
					target_errors[i], 1);
				exit(1);
			}
		}
	}

	/* unknown child -- something very wrong */

	fprintf(logerr, "HELP:  UNKNOWN CHILD DIED!  THIS SHOULD NEVER HAPPEN!!!\n");
	do_error("Aborting xfs_copy - reason");
	exit(1);

	sigset(SIGCLD, handler);
}

void
usage(void)
{
	fprintf(stderr,
"Usage:  xfs_copy [-d] fromdev|fromfile todev [todev todev ...]\n");
	fprintf(stderr,
	"        xfs_copy [-d] fromdev|fromfile tofile\n");
}

__uint64_t barcount[11];
int howfar = 0;
char *bar[11] = {
	" 0% ",
	" ... 10% ",
	" ... 20% ",
	" ... 30% ",
	" ... 40% ",
	" ... 50% ",
	" ... 60% ",
	" ... 70% ",
	" ... 80% ",
	" ... 90% ",
	" ... 100%\nDone.\n",
};

void
bump_bar(int tenths)
{
	printf("%s", bar[tenths]);
	fflush(stdout);
}

static off64_t source_position = -1;

void
read_wbuf(int fd, wbuf *buf, xfs_mount_t *mp)
{
	int res = 0;
	off64_t lres = 0;
	off64_t newpos;
	size_t diff;

	newpos = rounddown(buf->position, (off64_t) buf->min_io_size);

	if (newpos != buf->position)  {
		diff = buf->position - newpos;
		buf->position = newpos;

		buf->length += diff;
	}

	if (source_position != buf->position)  {
		lres = lseek64(fd, buf->position, SEEK_SET);
		if (lres < 0LL)  {
			fprintf(logerr,
				"xfs_copy:  lseek64 failure at offset %lld\n",
				source_position);
			do_error("Aborting xfs_copy - reason");
			exit(1);
		}
		source_position = buf->position;
	}

	assert(source_position % source_sectorsize == 0);

	/* round up length for direct i/o if necessary */

	if (buf->length % buf->min_io_size != 0)
		buf->length = roundup(buf->length, buf->min_io_size);

	if (buf->length > buf->size)  {
		fprintf(stderr,
			"assert error:  buf->length = %d, buf->size = %d\n",
			buf->length, buf->size);
		killall();
		abort();
	}

	if ((res = read(fd, buf->data, buf->length)) < 0)  {
		fprintf(logerr, "xfs_copy:  read failure at offset %lld\n",
				source_position);
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if (res < buf->length &&
	    source_position + res == mp->m_sb.sb_dblocks * source_blocksize)
		res = buf->length;
	else
		assert(res == buf->length);
	source_position += res;
	buf->length = res;
}

int
read_ag_header(int fd, xfs_agnumber_t agno, wbuf *buf, ag_header_t *ag,
		xfs_mount_t *mp, int blocksize, int sectorsize)
{
	daddr_t off;
	int length;
	off64_t newpos;
	size_t diff;

	/* initial settings */

	diff = 0;
	off = XFS_AG_DADDR(mp, agno, XFS_SB_DADDR);
	buf->position = (off64_t) off * (off64_t) BBSIZE;
	length = buf->length = first_agbno * blocksize;
	
	/* handle alignment stuff */

	newpos = rounddown(buf->position, (off64_t) buf->min_io_size);

	if (newpos != buf->position)  {
		diff = buf->position - newpos;
		buf->position = newpos;

		buf->length += diff;
	}

	/* round up length for direct i/o if necessary */

	if (buf->length % buf->min_io_size != 0)
		buf->length = roundup(buf->length, buf->min_io_size);

	assert(length != 0);

	read_wbuf(fd, buf, mp);

	assert(buf->length >= length);

	ag->xfs_sb = (xfs_sb_t *) (buf->data + diff);

	assert(ag->xfs_sb->sb_magicnum == XFS_SB_MAGIC);

	ag->xfs_agf = (xfs_agf_t *) (buf->data + diff + sectorsize);

	assert(ag->xfs_agf->agf_magicnum == XFS_AGF_MAGIC);

	ag->xfs_agi = (xfs_agi_t *) (buf->data + diff + 2*sectorsize);

	assert(ag->xfs_agi->agi_magicnum == XFS_AGI_MAGIC);

	ag->xfs_agfl = (xfs_agfl_t *) (buf->data + diff + 3*sectorsize);

	return(1);
}

void
write_wbuf(void)
{
	int i;

	/* verify target threads */

	for (i = 0; i < num_targets; i++)  {
		if (target_states[i] != INACTIVE)  {
			glob_masks.num_working++;
		}
	}

	/* release target threads */

	for (i = 0; i < num_targets; i++)  {
		if (target_states[i] != INACTIVE)  {
			/* wake up target threads */

			usvsema(targ[i].wait);
		}
	}

	sigrelse(SIGCLD);
	uspsema(mainwait);
	sighold(SIGCLD);
}


int
main(int argc, char **argv)
{
	int i, write_last_block = 0;
	int open_flags;
	off64_t pos;
	size_t length;
	int c, size, sizeb, first_residue, tmp_residue;
	__uint64_t numblocks = 0;
	int wblocks = 0;
	int num_threads = 0;
	struct dioattr d_info;
	int wbuf_size;
	int wbuf_align;
	int wbuf_miniosize;
	uuid_t fsid;
	uint_t status;
	uint btree_levels, current_level;
	ag_header_t ag_hdr;
	xfs_mount_t *mp;
	xfs_agnumber_t num_ags, agno;
	xfs_agblock_t bno;
	daddr_t begin, next_begin, ag_begin, new_begin, ag_end;
	xfs_alloc_block_t *block;
	xfs_alloc_ptr_t *ptr;
	xfs_alloc_rec_t *rec_ptr;
	extern char *optarg, *findrawpath(char *);
	extern int optind;
	sim_init_t simargs;
	t_args *tcarg;
	struct ustat ustat_buf;

#ifdef DEBUG_MALLOC
	mallopt(M_DEBUG, 1);
#endif
	xlog_debug = 0;

	/* open up log file */

	logfile_name = mktemp(LOGFILE_NAME);

	if (*logfile_name == '\0')  {
		fprintf(stderr,
			"xfs_copy:  could not generate unique logfile name.");
		fprintf(stderr,
			"xfs_copy:  check /usr/tmp for xfs_copy.log.* files.");
		perror("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((logfd = open(logfile_name, O_CREAT|O_TRUNC|O_APPEND|O_WRONLY,
				0644)) < 0)  {
		fprintf(stderr, "xfs_copy:  couldn't open log file \"%s\"\n",
				logfile_name);
		perror("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((logerr = fdopen(logfd, "w")) == NULL)  {
		fprintf(stderr, "xfs_copy:  couldn't set up logfile stream\n");
		perror("Aborting xfs_copy - reason");
		exit(1);
	}

	/* argument processing */

	duplicate_uuids = 0;

	while ((c = getopt(argc, argv, "d")) != EOF)  {
		switch (c) {
		case 'd':
			duplicate_uuids = 1;
			break;
		case '?':
			usage();
			exit(1);
			break;
		}
	}

	if (argc - optind < 2)  {
		usage();
		exit(1);
	}

	source_name = argv[optind];
	source_fd = -1;

	optind++;

	/* set up fd and name array */

	num_targets = argc - optind;

	if (num_targets > MAX_TARGETS)  {
		fprintf(logerr,
			"xfs_copy:  number of targets exceeds maximum of %d\n",
			MAX_TARGETS);
		fprintf(stderr,
			"xfs_copy:  number of targets exceeds maximum of %d\n",
			MAX_TARGETS);
		fprintf(logerr, "Aborting xfs_copy.\n");
		fprintf(stderr, "Aborting xfs_copy.\n");
		exit(1);
	}

	if ((target_names = malloc(sizeof(char *)*num_targets)) == NULL)  {
		fprintf(logerr,
			"xfs_copy:  couldn't malloc target name array\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((target_positions = malloc(sizeof(off64_t)*num_targets)) == NULL)  {
		fprintf(logerr,
			"xfs_copy:  couldn't malloc target position array\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((target_pids = malloc(sizeof(pid_t) * num_targets)) == NULL)  {
		fprintf(logerr, "Couldn't allocate target pid array\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((target_states = malloc(sizeof(int) * num_targets)) == NULL)  {
		fprintf(logerr, "Couldn't allocate target state array\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((target_errors = malloc(sizeof(int) * num_targets)) == NULL)  {
		fprintf(logerr, "Couldn't allocate target errno array\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((target_err_types = malloc(sizeof(int) * num_targets)) == NULL)  {
		fprintf(logerr, "Couldn't allocate target error type array\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	for (i = 0; i < num_targets; i++)  {
		target_positions[i] = -1;
		target_states[i] = INACTIVE;
		target_errors[i] = 0;
		target_err_types[i] = 0;
	}

	if ((target_fds = malloc(sizeof(int)*num_targets)) == NULL)  {
		fprintf(logerr, "xfs_copy:  couldn't malloc target fd array\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	for (i = 0; optind < argc; i++, optind++)  {
		target_names[i] = argv[optind];
		target_fds[i] = -1;
	}

	parent_pid = getpid();

	if (atexit(killall))  {
		fprintf(logerr,
			"xfs_copy:  couldn't register atexit function.\n");
		do_error("Aborting xfs_copy -- reason");
		exit(1);
	}

	/* open up source -- is it a file? */

	open_flags = O_RDONLY;

	if ((source_fd = open(source_name, open_flags)) < 0)  {
		fprintf(logerr, "xfs_copy:  couldn't open source \"%s\"\n",
			source_name);
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if (fstat64(source_fd, &statbuf) < 0)  {
		fprintf(logerr, "xfs_copy:  couldn't stat source \"%s\"\n",
			source_name);
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if (S_ISREG(statbuf.st_mode))  {
		source_is_file = 1;
		open_flags |= O_DIRECT;

		/* close and reopen for direct i/o */

		if (close(source_fd) < 0)  {
			fprintf(logerr,
				"Couldn't close fd to set up direct i/o.\n");
			do_error("Aborting xfs_copy - reason");
			exit(1);
		}

		if ((source_fd = open(source_name, open_flags)) < 0)  {
			fprintf(logerr,
				"xfs_copy:  couldn't re-open source \"%s\"\n",
				source_name);
			do_error("Aborting xfs_copy - reason");
			exit(1);
		}

		/* set direct i/o parameters */

		if (fcntl(source_fd, F_DIOINFO, &d_info) < 0)  {
			fprintf(logerr,
			"xfs_copy:  fcntl on file \"%s\" failed.\n",
				source_name);
			do_error("Aborting xfs_copy - reason");
			exit(1);
		}

		wbuf_align = d_info.d_mem;
		wbuf_size = d_info.d_maxiosz;
		wbuf_miniosize = d_info.d_miniosz;
	} else  {
		source_is_file = 0;

		/* set arbitrary i/o params, miniosize at least 1 disk block */

		wbuf_align = 4096*4;
		wbuf_size = 1024 * 4000;
		wbuf_miniosize = -1;	/* set after mounting source fs */

		/*
		 * check to make sure a filesystem isn't mounted
		 * on the device
		 */
		if (ustat(statbuf.st_rdev, &ustat_buf) == 0)  {
			fprintf(stderr,
"xfs_copy:  Warning -- a filesystem is mounted on the source device.\n");
			fprintf(logerr,
"xfs_copy:  Warning -- a filesystem is mounted on the source device.\n");

fprintf(stderr, "\t\tGenerated copies may be corrupt unless the source is\n");
fprintf(logerr, "\t\tGenerated copies may be corrupt unless the source is\n");
fprintf(stderr, "\t\tunmounted or mounted read-only.  Copy proceeding...\n");
fprintf(logerr, "\t\tunmounted or mounted read-only.  Copy proceeding...\n");
		}
		if (S_ISBLK(statbuf.st_mode))  {
	fprintf(logerr, "xfs_copy:  source \"%s\" is not a raw device.\n",
				source_name);

			old_source_name = source_name;

			if ((source_name = findrawpath(old_source_name))
								== NULL)  {
	fprintf(logerr, "xfs_copy:  cannot locate raw device for \"%s\"\n",
					old_source_name);
	fprintf(stderr, "xfs_copy:  cannot locate raw device for \"%s\"\n",
					old_source_name);
				fprintf(logerr, "Aborting xfs_copy.\n");
				fprintf(stderr, "Aborting xfs_copy.\n");
				exit(1);
			}

			/* close and reopen raw device */

			if (close(source_fd) < 0)  {
	fprintf(logerr, "Couldn't close fd to open raw device.\n");
				do_error("Aborting xfs_copy - reason");
				exit(1);
			}

			if ((source_fd = open(source_name, open_flags)) < 0)  {
	fprintf(logerr, "xfs_copy:  couldn't open source \"%s\"\n",
					source_name);
				do_error("Aborting xfs_copy - reason");
				exit(1);
			}

			fprintf(logerr,
				"xfs_copy:  using \"%s\" instead.\n",
				source_name);
			fprintf(logerr, "xfs_copy:  continuing...\n");
		}
	}

	/* xfs_mount source */

	simargs.volname = NULL;
	simargs.dname = NULL;
	simargs.rtname = NULL;
	simargs.logname = NULL;
	simargs.disfile = 0;
	simargs.dcreat = 0;
	simargs.lisfile = 0;
	simargs.lcreat = 0;
	simargs.risfile = 0;
	simargs.rcreat = 0;

	simargs.notvolmsg = "oh no %s";
	simargs.isreadonly = XFS_SIM_ISREADONLY;
	simargs.notvolok = 1;

	if (source_is_file)  {
		simargs.dname = source_name;
		simargs.disfile = 1;
	} else
		simargs.volname = source_name;

	if (!xfs_sim_init(&simargs))  {
		fprintf(logerr,
			"xfs_copy:  couldn't initialize simulation library\n");
		fprintf(stderr,
			"xfs_copy:  couldn't initialize simulation library\n");
		fprintf(logerr, "xfs_copy:  Aborting.\n");
		fprintf(stderr, "xfs_copy:  Aborting.\n");
		exit(1);
	}

	mp = xfs_mount(simargs.ddev, simargs.logdev, simargs.rtdev);

	if (mp == NULL || mp->m_sb.sb_inprogress) {
		fprintf(logerr, "xfs_copy:  %s is not a valid filesystem.\n",
				source_name);
		fprintf(stderr, "xfs_copy:  %s is not a valid filesystem.\n",
				source_name);
		fprintf(logerr, "xfs_copy:  Aborting.\n");
		fprintf(stderr, "xfs_copy:  Aborting.\n");
		exit(1);
	}

	if (mp->m_sb.sb_logstart == 0)  {
		/* source has an external log */

		fprintf(logerr,
			"xfs_copy:  %s has an external log.\n", source_name);
		fprintf(stderr,
			"xfs_copy:  %s has an external log.\n", source_name);
		fprintf(logerr, "xfs_copy:  Aborting.\n");
		fprintf(stderr, "xfs_copy:  Aborting.\n");
		exit(1);
	}

	if (mp->m_sb.sb_rextents != 0)  {
		/* source has a real-time section */

		fprintf(logerr,
			"xfs_copy:  %s has a real-time section.\n",
				source_name);
		fprintf(stderr,
			"xfs_copy:  %s has a real-time section.\n",
				source_name);
		fprintf(logerr, "xfs_copy:  Aborting.\n");
		fprintf(stderr, "xfs_copy:  Aborting.\n");
		exit(1);
	}

	source_blocksize = mp->m_sb.sb_blocksize;
	source_sectorsize = mp->m_sb.sb_sectsize;

	if (wbuf_miniosize == -1)
		wbuf_miniosize = source_sectorsize;

	assert(source_blocksize % source_sectorsize == 0);
	assert(source_sectorsize % BBSIZE == 0);

	if (source_blocksize > source_sectorsize)  {
		/* get number of leftover sectors in last block of ag header */

		tmp_residue = ((XFS_AGFL_DADDR + 1) * source_sectorsize)
					% source_blocksize;
		first_residue = (tmp_residue == 0) ? 0 :
			source_blocksize - tmp_residue;
		assert(first_residue % source_sectorsize == 0);
	} else if (source_blocksize == source_sectorsize)  {
		first_residue = 0;
	} else  {
		fprintf(logerr,
	"Error:  filesystem block size is smaller than the disk sectorsize.\n");
		fprintf(logerr, "Aborting xfs_copy now.\n");
	}

	/*
	 * WARNING:  note that the XFS_AGFL_DADDR+1 will have to
	 * be changed if the layout of the first 4 disk sectors of
	 * each ag changes from the current layout (sb, agf, agi, agfl).
	 */

	first_agbno = (((XFS_AGFL_DADDR + 1) * source_sectorsize)
				+ first_residue) / source_blocksize;
	assert(first_agbno != 0);
	assert( ((((XFS_AGFL_DADDR + 1) * source_sectorsize)
				+ first_residue) % source_blocksize) == 0);

	if (!duplicate_uuids)  {
		uuid_create(&fsid, &status);

		if (status != uuid_s_ok)  {
			fprintf(logerr, "xfs_copy:  uuid creation failed.\n");
			do_error("Aborting xfs_copy -- reason");
			exit(1);
		}
	}

	/* now open targets */

	open_flags = O_RDWR;

	if (num_targets > 1)  {
		/* just open them all */

		for (i = 0; i < num_targets; i++)  {
			if ((target_fds[i] = open(target_names[i],
						open_flags)) < 0)  {
				fprintf(logerr,
				"xfs_copy:  couldn't open target \"%s\"\n",
					target_names[i]);
				do_error("Aborting xfs_copy - reason");
				exit(1);
			}
			if (fstat64(target_fds[i], &statbuf) < 0)  {
				fprintf(logerr,
				"xfs_copy:  couldn't stat target \"%s\"\n",
						target_names[i]);
				do_error("Aborting xfs_copy - reason");
				exit(1);
			}

			if (S_ISREG(statbuf.st_mode))  {
				fprintf(logerr,
				"xfs_copy:  target \"%s\" is a regular file.\n",
					target_names[i]);
				fprintf(stderr,
				"xfs_copy:  target \"%s\" is a regular file.\n",
					target_names[i]);
				usage();
				exit(1);
			}
			/*
			 * check to make sure a filesystem isn't mounted
			 * on the device
			 */
			if (ustat(statbuf.st_rdev, &ustat_buf) == 0)  {
fprintf(stderr, "xfs_copy:  a filesystem is mounted on the target device.\n");
fprintf(logerr, "xfs_copy:  a filesystem is mounted on the target device.\n");
fprintf(stderr, "xfs_copy cannot copy onto mounted filesystems.  Aborting.\n");
fprintf(logerr, "xfs_copy cannot copy onto mounted filesystems.  Aborting.\n");
				exit(1);
			}
			if (S_ISBLK(statbuf.st_mode))  {
				fprintf(logerr,
			"xfs_copy:  target \"%s\" is not a raw device.\n",
					target_names[i]);
				tmp_name = target_names[i];
				if ((target_names[i] = findrawpath(tmp_name))
							== NULL)  {
					fprintf(logerr,
			"xfs_copy:  cannot locate raw device for \"%s\"\n",
					tmp_name);
					fprintf(stderr,
			"xfs_copy:  cannot locate raw device for \"%s\"\n",
					tmp_name);
					fprintf(logerr, "Aborting xfs_copy.\n");
					fprintf(stderr, "Aborting xfs_copy.\n");
					exit(1);
				}

				/* close and reopen raw device */

				if (close(target_fds[i]) < 0)  {
					fprintf(logerr,
			"Couldn't close fd to open raw device.\n");
					do_error("Aborting xfs_copy - reason");
					exit(1);
				}

				if ((target_fds[i] = open(target_names[i],
							open_flags)) < 0)  {
					fprintf(logerr,
			"xfs_copy:  couldn't open target \"%s\"\n",
						target_names[i]);
					do_error("Aborting xfs_copy - reason");
					exit(1);
				}

				fprintf(logerr,
				"xfs_copy:  using raw device \"%s\" instead\n",
					target_names[0]);
				fprintf(logerr, "xfs_copy:  continuing...\n");
			}
		}
	} else  {
		/* see if it's a file  */

		if (stat64(target_names[0], &statbuf) < 0)  {
			/* ok, assume it's a file and create it */

			printf("Creating file %s\n", target_names[0]);
			fprintf(logerr, "Creating file %s\n", target_names[0]);

			open_flags |= O_CREAT|O_DIRECT;
			write_last_block = 1;
		} else if (S_ISREG(statbuf.st_mode))  {
			open_flags |= O_TRUNC|O_DIRECT;
			write_last_block = 1;
		} else  {
			/*
			 * check to make sure a filesystem isn't mounted
			 * on the device
			 */
			if (ustat(statbuf.st_rdev, &ustat_buf) == 0)  {
fprintf(stderr, "xfs_copy:  a filesystem is mounted on the target device.\n");
fprintf(logerr, "xfs_copy:  a filesystem is mounted on the target device.\n");
fprintf(stderr, "xfs_copy cannot copy onto mounted filesystems.  Aborting.\n");
fprintf(logerr, "xfs_copy cannot copy onto mounted filesystems.  Aborting.\n");
				exit(1);
			}
			if (S_ISBLK(statbuf.st_mode))  {
				fprintf(logerr,
		"xfs_copy:  target \"%s\" is not a raw device.\n",
					target_names[0]);

				tmp_name = target_names[0];
				if ((target_names[0] = findrawpath(tmp_name))
						== NULL)  {
					fprintf(logerr,
			"xfs_copy:  cannot locate raw device for \"%s\"\n",
						tmp_name);
					fprintf(stderr,
			"xfs_copy:  cannot locate raw device for \"%s\"\n",
						tmp_name);
					fprintf(logerr, "Aborting xfs_copy.\n");
					fprintf(stderr, "Aborting xfs_copy.\n");
					exit(1);
				}

			fprintf(logerr, "xfs_copy:  using raw device \"%s\"\n",
				target_names[i]);
			fprintf(logerr, "xfs_copy:  continuing...\n");
			}
		}

		if ((target_fds[0] = open(target_names[0],
					open_flags, 0644)) < 0)  {
			fprintf(logerr,
				"xfs_copy:  couldn't open target \"%s\"\n",
				target_names[0]);
			do_error("Aborting xfs_copy - reason");
			exit(1);
		}

		if (write_last_block)  {
			if (fcntl(target_fds[0], F_DIOINFO, &d_info) < 0)  {
				fprintf(logerr,
				"xfs_copy:  fcntl on file \"%s\" failed.\n",
					target_names[0]);
				do_error("Aborting xfs_copy - reason");
				exit(1);
			}

			wbuf_align = MAX(wbuf_align, d_info.d_mem);
			wbuf_size = MIN(d_info.d_maxiosz, wbuf_size);
			wbuf_miniosize = MAX(d_info.d_miniosz, wbuf_miniosize);
		}
	}

	direct_io = (source_is_file && write_last_block) ? 1 : 0;

	/* initialize shared semaphore arena */

	if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) < 0)  {
		do_error("xfs_copy - error setting up semaphore area");
		exit(1);
	}

	if (usconfig(CONF_INITUSERS, MAX_THREADS) < 0)  {
		do_error("xfs_copy - error setting up semaphore area");
		exit(1);
	}

	arena_name = mktemp(ARENA_NAME);

	if (*arena_name == '\0')  {
		fprintf(logerr,
			"xfs_copy:  could not generate unique arena filename.");
		fprintf(logerr,
			"xfs_copy:  check /usr/tmp for xfs_copy.arena* files.");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((arena = usinit(arena_name)) == NULL)  {
		fprintf(logerr, "xfs_copy:  could not initialize arena.");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	/* initialize locks and bufs */

	if (thread_control_init(&glob_masks, num_targets+1) == NULL)  {
		fprintf(logerr, "Couldn't initialize global thread mask\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if (wbuf_init(&w_buf, wbuf_size, wbuf_align,
					wbuf_miniosize, 0) == NULL)  {
		fprintf(logerr, "Error initializing wbuf 0\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	wblocks = wbuf_size / BBSIZE;

	if (wbuf_init(&btree_buf, MAX(MAX(source_blocksize, source_sectorsize),
					wbuf_miniosize), wbuf_align,
					wbuf_miniosize, 1) == NULL)  {
		fprintf(logerr, "Error initializing btree buf 1\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	if ((mainwait = usnewsema(arena, 0)) == NULL)  {
		fprintf(logerr, "Error creating first semaphore.\n");
		fprintf(logerr, "Something's really wrong.\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	/* set up sigchild signal handler */

	sigset(SIGCLD, handler);
	sighold(SIGCLD);

	/* make children */

	if ((targ = malloc(num_targets * sizeof(t_args))) == NULL)  {
		fprintf(logerr, "Couldn't malloc space for thread args\n");
		do_error("Aborting xfs_copy - reason");
		exit(1);
	}

	for (i = 0, tcarg = targ; i < num_targets; i++, tcarg++)  {
		if ((tcarg->wait = usnewsema(arena, 0)) == NULL)  {
			fprintf(logerr,
				"error creating sproc semaphore %d\n", i);
			fprintf(logerr, "Try a smaller number of targets\n");
			do_error("Aborting xfs_copy - reason");
			exit(1);
		}
	}

	for (i = 0, tcarg = targ; i < num_targets; i++, tcarg++)  {
		tcarg->id = i;
		tcarg->fd = target_fds[i];

		target_states[i] = ACTIVE;

		num_threads++;

		target_pids[i] = sprocsp(begin_reader, PR_SALL, tcarg,
					NULL, T_STACKSIZE);

		if ((target_pids[i]) < 0)  {
			fprintf(logerr,
				"error creating sproc for target %d\n", i);
			do_error("Aborting xfs_copy - reason");
			exit(1);
		}
	}

	assert(num_targets == num_threads);

	/* set up statistics */

	num_ags = mp->m_sb.sb_agcount;

	source_blocks = mp->m_sb.sb_blocksize / BBSIZE
			* ((__uint64_t)mp->m_sb.sb_dblocks
			    - (__uint64_t)mp->m_sb.sb_fdblocks + 10 * num_ags);

	for (i = 0; i < 11; i++)
		barcount[i] = (source_blocks/10)*i;

	kids = num_targets;
	block = (xfs_alloc_block_t *) btree_buf.data;

	for (agno = 0; agno < num_ags && kids > 0; agno++)  {
		/* read in first blocks of the ag */

		read_ag_header(source_fd, agno, &w_buf, &ag_hdr, mp,
			source_blocksize, source_sectorsize);

		/* reset uuid and if applicable the in_progress bit */

		if (!duplicate_uuids)
			ag_hdr.xfs_sb->sb_uuid = fsid;

		if (agno == 0)  {
			ag_hdr.xfs_sb->sb_inprogress = 1;
		}

		/* save what we need (agf) in the btree buffer */

		bcopy(ag_hdr.xfs_agf, btree_buf.data, source_sectorsize);
		ag_hdr.xfs_agf = (xfs_agf_t *) btree_buf.data;
		btree_buf.length = source_blocksize;

		/* write the ag header out */

		write_wbuf();

		/* traverse btree until we get to the leftmost leaf node */

		bno = ag_hdr.xfs_agf->agf_roots[XFS_BTNUM_BNOi];
		current_level = 0;
		btree_levels = ag_hdr.xfs_agf->agf_levels[XFS_BTNUM_BNOi];

		ag_end = XFS_AGB_TO_DADDR(mp, agno,
				ag_hdr.xfs_agf->agf_length - 1)
				+ source_blocksize/BBSIZE;

		for (;;) {
			/* none of this touches the w_buf buffer */

			assert(current_level < btree_levels);

			current_level++;

			btree_buf.position = pos =
				(off64_t)XFS_AGB_TO_DADDR(mp,agno,bno) << BBSHIFT;
			btree_buf.length = source_blocksize;

			read_wbuf(source_fd, &btree_buf, mp);
			block = (xfs_alloc_block_t *) ((char *) btree_buf.data
					+ pos - btree_buf.position);

			assert(block->bb_magic == XFS_ABTB_MAGIC);

			if (block->bb_level == 0)
				break;;

			ptr = XFS_BTREE_PTR_ADDR(sourceb_blocksize, xfs_alloc,
				block, 1, mp->m_alloc_mxr[1]),

			bno = *ptr;
		}

		/* align first data copy but don't overwrite ag header */

		pos = w_buf.position >> BBSHIFT;
		length = w_buf.length >> BBSHIFT;
#if 0
		/*
		 * we know pos and length are aligned, now calculate
		 * address of the first real ag blocks in the ag and
		 * make sure it's aligned properly
		 */

		next_begin = XFS_AG_DADDR(mp, agno, first_agbno);
		
		if (next_begin % (w_buf.min_io_size >> BBSHIFT) != 0)  {
			/* have to align -- duplicate read_ag_header actions */

			next_begin = rounddown(next_begin,
					w_buf.min_io_size >> BBSHIFT);

			if (next_begin < pos)  {
				/*
				 * bump it back up to next boundary,
				 * the ag header write already copied
				 * the first few disk blocks
				 */

				next_begin = roundup(next_begin+1,
					w_buf.min_io_size >> BBSHIFT);
			}
		}
#else
		next_begin = pos + length;
#endif
		ag_begin = next_begin;

		assert(w_buf.position % source_sectorsize == 0);

		/* handle the rest of the ag */

		for (;;) {
			if (block->bb_level != 0)  {
		fprintf(logerr, "WARNING:  source filesystem inconsistent.\n");
		fprintf(stderr, "WARNING:  source filesystem inconsistent.\n");
		fprintf(logerr,
			"  A leaf btree rec isn't a leaf.  Aborting now.\n");
		fprintf(stderr,
			"  A leaf btree rec isn't a leaf.  Aborting now.\n");
				exit(1);
			}

			rec_ptr = XFS_BTREE_REC_ADDR(source_blocksize, xfs_alloc,
					block, 1, mp->m_alloc_mxr[0]);

			for (i = 0; i < block->bb_numrecs; i++, rec_ptr++)  {
				/* calculate in daddr's */

				begin = next_begin;

				/*
				 * protect against pathological case of a
				 * hole right after the ag header in a
				 * mis-aligned case
				 */

				if (begin < ag_begin)
					begin = ag_begin;

				/*
				 * round size up to ensure we copy a
				 * range bigger than required
				 */

				sizeb = XFS_AGB_TO_DADDR(mp, agno,
					rec_ptr->ar_startblock) - begin;
				size = roundup(sizeb << BBSHIFT, wbuf_miniosize);

#if 0
				if (w_buf.min_io_size != wbuf_miniosize)  {
					fprintf(stderr,
		"assert error:  w_buf.min_io_size = %d, wbuf_miniosize = %d\n",
						w_buf.min_io_size,
						wbuf_miniosize);
					killall();
					abort();
				}
#endif
				if (size > 0)  {
					/* copy extent */

					w_buf.position = (off64_t) begin
								<< BBSHIFT;

					while (size > 0)  {
						/*
						 * let lower layer do alignment
						 */
						if (size > w_buf.size)  {
							w_buf.length = w_buf.size;
							size -= w_buf.size;
							sizeb -= wblocks;
							numblocks += wblocks;
						} else  {
							w_buf.length = size;
							numblocks += sizeb;
							size = 0;
						}

						read_wbuf(source_fd, &w_buf, mp);
#ifndef NO_COPY
						write_wbuf();
#endif
						w_buf.position += w_buf.length;

						while (howfar < 10 && numblocks 
							> barcount[howfar])  {
							bump_bar(howfar);
							howfar++;
						}
					}
				}

				/* round next starting point down */

				new_begin = XFS_AGB_TO_DADDR(mp, agno,
						rec_ptr->ar_startblock +
						rec_ptr->ar_blockcount);
				next_begin = rounddown(new_begin,
						w_buf.min_io_size >> BBSHIFT);
			}

			if (block->bb_rightsib == NULLAGBLOCK)
				break;

			/* read in next btree record block */

			btree_buf.position = pos = (off64_t)XFS_AGB_TO_DADDR(mp,
				agno, block->bb_rightsib) << BBSHIFT;
			btree_buf.length = source_blocksize;

			/* let read_wbuf handle alignment */

			read_wbuf(source_fd, &btree_buf, mp);

			block = (xfs_alloc_block_t *) ((char *) btree_buf.data
					+ pos - btree_buf.position);

			assert(block->bb_magic == XFS_ABTB_MAGIC);
		}

		/*
		 * write out range of used blocks after last range
		 * of free blocks in AG
		 */
		if (next_begin < ag_end)  {
			begin = next_begin;

			sizeb = ag_end - begin;
			size = roundup(sizeb << BBSHIFT, wbuf_miniosize);

			if (size > 0)  {
				/* copy extent */

				w_buf.position = (off64_t) begin
							<< BBSHIFT;

				while (size > 0)  {
					/*
					 * let lower layer do alignment
					 */
					if (size > w_buf.size)  {
						w_buf.length = w_buf.size;
						size -= w_buf.size;
						sizeb -= wblocks;
						numblocks += wblocks;
					} else  {
						w_buf.length = size;
						numblocks += sizeb;
						size = 0;
					}

					read_wbuf(source_fd, &w_buf, mp);
#ifndef NO_COPY
					write_wbuf();
#endif
					w_buf.position += w_buf.length;

					while (howfar < 10 && numblocks 
						> barcount[howfar])  {
						bump_bar(howfar);
						howfar++;
					}
				}
			}
		}
	}

	if (kids > 0)  {
		if (write_last_block)
			if (ftruncate64(target_fds[0], mp->m_sb.sb_dblocks *
						source_blocksize))  {
		fprintf(logerr, "xfs_copy:  growing data section failed.\n");
		fprintf(stderr, "xfs_copy:  growing data section failed.\n");
				do_error("Aborting xfs_copy:  reason");
				exit(1);
			}
			
		/* reread and rewrite the first ag */

		read_ag_header(source_fd, 0, &w_buf, &ag_hdr, mp,
			source_blocksize, source_sectorsize);

		ag_hdr.xfs_sb->sb_inprogress = 0;

		if (!duplicate_uuids)
			ag_hdr.xfs_sb->sb_uuid = fsid;

		write_wbuf();
		bump_bar(10);
	}

	check_errors();
	killall();

	return 0;
}
