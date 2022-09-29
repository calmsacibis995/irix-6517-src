#ifndef __NS_DAEMON_H__
#define __NS_DAEMON_H__

#include <mdbm.h>
#include <sys/time.h>
#include <abi_mutex.h>

/*
** NSD return types.
*/
#define NSD_ERROR	0	/* Something went wrong, return to client. */
#define NSD_OK		1	/* We are finished, return to client. */
#define NSD_CONTINUE	2	/* Go off and work on something else. */
#define NSD_NEXT	4	/* Skip to next callout. */
#define NSD_RETURN	8	/* Return to client with current results. */

/*
** NSD logging levels
*/
#define NSD_LOG_CRIT	0	/* Critical system failures */
#define NSD_LOG_RESOURCE 1	/* Resource allocation failures */
#define NSD_LOG_OPER	2	/* Imporant operation state changes */
#define NSD_LOG_MIN	3	/* debugging */
#define NSD_LOG_LOW	4	/* debugging */
#define NSD_LOG_HIGH	5	/* debugging */
#define NSD_LOG_MAX	6	/* debugging */

/*
** NSD callback flags.
*/
#define NSD_READ	1
#define	NSD_WRITE	2
#define NSD_EXCEPT	4

/*
** Request flags.
*/
#define NSD_FLAG_CACHE		0x1

/*
** Limits.
*/
#define NSD_CA_NAME_LEN	256

#define NFNON		0
#define	NFREG		1
#define	NFDIR		2
#define	NFBLK		3
#define	NFCHR		4
#define	NFLNK		5
#define	NFSOCK		6
#define NFFIFO		7
#define NFINPROG	8

#define NFSMODE_FMT	0170000
#define	NFSMODE_DIR	0040000
#define	NFSMODE_CHR	0020000
#define NFSMODE_BLK	0060000
#define	NFSMODE_REG	0100000
#define	NFSMODE_LNK	0120000
#define	NFSMODE_SOCK	0140000
#define	NFSMODE_FIFO	0010000

#define NFS_FHSIZE	32

/*
** This is the internal format for a request credential.
*/
typedef struct nsd_cred {
	int		c_nlink;
	uid_t		c_uid;
	int		c_gids;
	gid_t		c_gid[17];
	void		*c_data;
	struct {
		void	(*cv_free)(void *);
		int	(*cv_verify)(void *, struct nsd_cred *);
	} c_vops;
} nsd_cred_t;

#define NSD_CRED_UNIX	1
#define NSD_MAX_GIDS	17

#define CRED_REF(c)	(c)->c_nlink++;

/*
** Each map and callout can contain an unordered list of attributes as
** described by this structure.
*/
typedef struct nsd_attr {
	char		*a_key;
	char		*a_value;
	void		*a_data;
	int		a_size;
	void		(*a_free)(void *);
	int		a_nlink;
	struct nsd_attr	*a_next;
} nsd_attr_t;

/*
** The nsd_libraries structure contains information about a shared library
** method.  We maintain a list of open libraries so that we will not
** reopen libraries each time the resolv.conf file is changed.
*/
#define INIT	0
#define LOOKUP	1
#define LIST	2
#define	CONTROL	3
#define	SHAKE	4
#define UPDATE	5
#define DUMP	6
#define VERIFY	7

typedef struct nsd_libraries {
	char			*l_name;
	void			*l_handle;
	int			(*l_funcs[8])();
	struct nsd_libraries	*l_next;
	int			l_state;
} nsd_libraries_t;

#define LIB_INIT_FAILED	1
#define LIB_INIT_RETRY	2

/*
** The map structure represents the cache files for local name service.
*/
typedef struct nsd_maps {
	char			*m_file;
	uint32_t		m_flags;
	MDBM			*m_map;
	time_t			m_touched;
	struct nsd_maps		*m_next;
	char			m_name[1];
} nsd_maps_t;

#define NSD_MAP_ALLOC		1	/* alloc space request failed */

/*
** The file structure incorporates all the information for a file.  On
** each new request we look to see if the "file" already exists, and if
** so we just return the requested data.  If not we allocate a structure
** and walk it through the callout libraries to fill it in.  This same
** structure is used for everything within the daemon, but some of the
** fields are used to have different meaning.
*/
typedef struct nsd_file {
	char		*f_name;	/* label for file - malloc'd */
	unsigned	f_namelen;	/* length of name string */

	uint64_t	f_fh[4];	/* hash of name is dir fileid hash,
					** dir fileid, fileid hash, fileid */

	nsd_attr_t	*f_attrs;	/* attributes on file */

	unsigned	f_type;		/* file type */
	unsigned	f_mode;		/* permissions - also includes type */
	int		f_nlink;	/* link count - always 1 */

	unsigned	f_uid;		/* user id */
	unsigned	f_gid;		/* group id */
	nsd_cred_t	*f_cred;	/* credentials for this lookup */

	struct timeval	f_atime;	/* access time - now */
	struct timeval	f_mtime;	/* modify time */
	struct timeval	f_ctime;	/* create time */

	time_t		f_timeout;	/* when this record will be released */
	unsigned	f_tries;	/* lookup retry counter */
	char		f_control[NS_RESULTS];	/* override default behavior */

	unsigned	f_size;		/* bytes allocated for data */
	unsigned	f_used;		/* bytes of used data */
#define f_dir f_data
	char		*f_data;	/* file data or directory list */
	nsd_libraries_t	*f_lib;		/* library pointer for callout */
	void		(*f_free)(void *);	/* data free routine */
	unsigned	f_status;	/* function status */

	unsigned	f_c_size;	/* bytes allocated for callouts */
	unsigned	f_c_used;	/* bytes of callouts used */
	char		*f_callouts;	/* file IDs of callout files */
	unsigned	f_index;	/* which command for callout */

	int		(*f_send)(struct nsd_file *);	/* results function */
	void		*f_sender;	/* data used by f_send function */

	nsd_maps_t	*f_map;		/* local cache file pointer */
	void		*f_cmd_data;	/* free pointer for use by libraries */

	uint32_t        f_flags;        /* flags for this file */
	struct nsd_file	*f_loop;	/* loopback file pointer */
} nsd_file_t;

#define nsd_status(rq, s)	{ rq->f_status = s; }

/*
** The following two structures are used to create a simple btree
** for filehandles.  All files exist in a linked list structure
** starting at __nsd_mounts, and in a filehandle index starting
** at __nsd_files.
*/
#define BSIZE 64
typedef struct nsd_leaf {
	uint32_t	l_key[BSIZE];
	nsd_file_t	*l_value[BSIZE];
	unsigned	l_index;
} nsd_leaf_t;

typedef struct nsd_node {
	struct nsd_node		*n_zero;
	struct nsd_node		*n_one;
	struct nsd_leaf		*n_data;
} nsd_node_t;

/*
** The time_list structure is a node in the timeout queue.  It contains
** an absolute time for when the timer will go off, and information
** about the callback routine to call.
*/
typedef struct nsd_times {
	void			*t_clientdata;
	nsd_file_t		*t_file;
	int			(*t_proc)(nsd_file_t **, struct nsd_times *);
	struct timeval		t_timeout;
	struct nsd_times	*t_next;
} nsd_times_t;

typedef struct nsd_signal {
	nsd_file_t		*s_file;
	pid_t			s_pid;
	int			(*s_callback)(nsd_file_t *);
	struct nsd_signal	*s_next;
} nsd_signal_t;

/*
** Function pointer types.  Our compiler does not allow for forward
** references in typedefs so 
*/
typedef int (nsd_init_proc)(char *);
typedef int (nsd_shake_proc)(int);
typedef int (nsd_callout_proc)(nsd_file_t *);
typedef int (nsd_verify_proc)(nsd_file_t *, nsd_cred_t *);
typedef int (nsd_callback_proc)(nsd_file_t **, int);
typedef int (nsd_timeout_proc)(nsd_file_t **, nsd_times_t *);
typedef int (nsd_send_proc)(nsd_file_t *);
typedef int (nsd_dump_proc)(FILE *);
typedef void (nsd_free_proc)(void *);

typedef struct {
	uint32_t		lb_flags;
	nsd_callout_proc	*lb_proc;
	nsd_file_t		*lb_dir;
	char			*lb_domain;
	char			*lb_table;
	char			*lb_protocol;
	char			*lb_key;
	char			*lb_attrs;
} nsd_loop_t;

#define NSD_LOOP_SKIP		(1<<0)
#define NSD_LOOP_FIRST		(1<<1)

/*
** Result memory classes for set_result().
*/
#define STATIC		(nsd_free_proc *)0
#define VOLATILE	(nsd_free_proc *)1
#define DYNAMIC		(nsd_free_proc *)2

/*
** Bitfield flags for nsd_file_t.f_flags
*/
#define NSD_FLAGS_MKDIR   1<<0	/* Directory supports directory creation */
#define NSD_FLAGS_DYNAMIC 1<<1	/* Directory was dynamicly created */

/*
** Function prototypes for lamed routines.
*/

/* attr.c */
nsd_attr_t *nsd_attr_store(nsd_attr_t **, char *, char *);
nsd_attr_t *nsd_attr_append(nsd_attr_t **, char *, char *, int);
int nsd_attr_data_set(nsd_attr_t *, void *, int, void (*)(void *));
nsd_attr_t *nsd_attr_continue(nsd_attr_t **, nsd_attr_t *);
nsd_attr_t *nsd_attr_fetch(nsd_attr_t *, char *);
long nsd_attr_fetch_long(nsd_attr_t *, char *, int, long);
char *nsd_attr_fetch_string(nsd_attr_t *, char *, char *);
int nsd_attr_fetch_bool(nsd_attr_t *, char *, int);
int nsd_attr_delete(nsd_attr_t **, char *);
void nsd_attr_clear(nsd_attr_t *);
nsd_attr_t *nsd_attr_copy(nsd_attr_t *);
int nsd_attr_subset(nsd_attr_t *, nsd_attr_t *);
int nsd_attr_parse(nsd_attr_t **, char *, int, char *);

/* btree.c */
int nsd_binsert(uint32_t, nsd_file_t *);
void *nsd_bdelete(uint32_t);
void *nsd_bget(uint32_t);
void nsd_bclear(nsd_node_t *, void (*)(void *));
void nsd_blist(nsd_node_t *, FILE *);

/* callback.c */
nsd_callback_proc *nsd_callback_new(int, nsd_callback_proc *, unsigned);
int nsd_callback_remove(int);
nsd_callback_proc *nsd_callback_get(int);

/* cred.c */
nsd_cred_t *nsd_cred_new(uid_t, int, ...);
void nsd_cred_clear(nsd_cred_t **);
int nsd_cred_verify(nsd_file_t *, nsd_cred_t *);
int nsd_cred_data_set(nsd_file_t *, void *, void (*)(void *),
    int (*)(nsd_file_t *, nsd_cred_t *));

/* file.c */
int nsd_file_init(nsd_file_t **, char *, int, nsd_file_t *, int, nsd_cred_t *);
nsd_file_t *nsd_file_byid(uint32_t);
void nsd_file_clear(nsd_file_t **);
void nsd_file_rmdir(char *);
int nsd_file_delete(nsd_file_t *, nsd_file_t *);
nsd_file_t *nsd_file_byname(char *, char *, int);
nsd_file_t *nsd_file_byhandle(uint64_t *);
void nsd_file_timeout(nsd_file_t **, time_t);
int nsd_file_appenddir(nsd_file_t *, nsd_file_t *, char *, int);
int nsd_file_appendcallout(nsd_file_t *, nsd_file_t *, char *, int);
int nsd_file_copycallouts(nsd_file_t *, nsd_file_t *, nsd_attr_t *);
nsd_file_t *nsd_file_getcallout(nsd_file_t *);
nsd_file_t *nsd_file_nextcallout(nsd_file_t *);
int nsd_file_dupcallouts(nsd_file_t *, nsd_file_t *);
int nsd_file_find(char *, char *, char *, char *, nsd_file_t **);
void nsd_file_list(nsd_file_t *, FILE *, int);
nsd_file_t *nsd_file_dup(nsd_file_t *, nsd_file_t *);
nsd_file_t *nsd_file_mkdir(nsd_file_t *, char *, int, nsd_cred_t *);

/* loopback.c */
int nsd_loopback(nsd_file_t *, nsd_loop_t *);

/* map.c */
void nsd_map_shake(int, time_t);
char *nsd_map_file(char *);
int nsd_map_open(nsd_file_t *);
nsd_maps_t *nsd_map_get(char *);
int nsd_map_update(nsd_file_t *);
void nsd_map_close_all(void);
void nsd_map_delete(nsd_maps_t *, char *, int);
int nsd_map_flush(nsd_file_t *);
int nsd_map_remove(nsd_file_t *);
int nsd_map_unlink(nsd_file_t *);

/* mount.c */
void nsd_mount_clear(void);
int nsd_mount_init(void);
nsd_file_t *nsd_mount_byname(char *);

/* nfs.c */
int nsd_nfs_init(void);

/* nsw.c */
int nsd_nsw_parse(nsd_file_t *, char *);
int nsd_nsw_default(nsd_file_t *);
void nsd_nsw_init(void);

/* portmap.c */
void nsd_portmap_shake(void);
void nsd_portmap_register(int, int, int, int);
void nsd_portmap_unregister(int, int);
int nsd_portmap_getport(int, int, int, struct in_addr *,
    int (*)(nsd_file_t **, void *), uint32_t *, void *);

/* timeout.c */
struct timeval *nsd_timeout_set(void);
nsd_times_t *nsd_timeout_next(void);
nsd_times_t *nsd_timeout_new(nsd_file_t *, long, nsd_timeout_proc *, void *);
int nsd_timeout_remove(nsd_file_t *);

/* util.c */
int nsd_set_result(nsd_file_t *, int, char *, int, nsd_free_proc *);
int nsd_append_result(nsd_file_t *, int, char *, int);
int nsd_append_element(nsd_file_t *, int, char *, int);
void nsd_free_file(nsd_file_t *);
nsd_file_t *nsd_init_file(void);
void nsd_logprintf(int, char *format, ...);
void nsd_shake(int);
void *nsd_malloc(size_t);
void *nsd_calloc(size_t, size_t);
void *nsd_realloc(void *, size_t);
char *nsd_strdup(char *);
void *nsd_mmap(void *, size_t, int, int, int, off_t);
int nsd_open(const char *, int, mode_t);
MDBM *nsd_mdbm_open(char *, int, mode_t, int);
int nsd_local(struct sockaddr_in *);
void nsd_dump(FILE *);
void *nsd_bmalloc(size_t);
void *nsd_brealloc(void *, size_t);
void nsd_bfree(void *);
int nsd_cat(char *, int, int, ...);

#endif /* not __NS_DAEMON_H__ */
