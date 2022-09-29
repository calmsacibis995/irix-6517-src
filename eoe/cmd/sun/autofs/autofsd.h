/*
 *	autofsd.h
 *
 *	Copyright (c) 1988-1993 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ident	"@(#)autofsd.h	1.5	93/07/02 SMI"

#include <netinet/in.h>
#include <semaphore.h>

#define	MXHOSTNAMELEN  64
#define	MAXNETNAMELEN   255
#define	MAXFILENAMELEN  255
#define	LINESZ		2048
#define B_SIZE  	1024
#define VERBOSE_REMOUNT 4

#define	MNTOPT_FSTYPE	"fstype"
#define REMOUNTOPTS     "ignore,nest"

/* stack ops */
#define	ERASE		0
#define	PUSH		1
#define	POP		2
#define	INIT		3
#define STACKSIZ	30

struct mapline {
	char linebuf[LINESZ];
	char lineqbuf[LINESZ];
};

/*
 * Structure describing a host/filesystem/dir tuple in a NIS map entry
 */
struct mapfs {
	struct mapfs *mfs_next;		/* next in entry */
	int 	mfs_ignore;		/* ignore this entry */
	char	*mfs_host;		/* host name */
	struct in_addr mfs_addr;        /* address of host */
	char	*mfs_dir;		/* dir to mount */
};

/*
 * NIS entry - lookup of name in DIR gets us this
 */
struct mapent {
	char	*map_fstype;	/* file system type e.g. "nfs" */
	char	*map_mounter;	/* base fs e.g. "cachefs" */
	char	*map_root;	/* path to mount root */
	char	*map_mntpnt;	/* path from mount root */
	char	*map_mntopts;	/* mount options */
	struct mapfs *map_fs;	/* list of replicas for nfs */
	struct mapent *map_next;
	short	map_exflags;
};


/*
 * Descriptor for each directory served by the automounter
 */
struct autodir {
	char	*dir_name;		/* mount point */
	char	*dir_map;		/* name of map for dir */
	char	*dir_opts;		/* default mount options */
	int 	dir_direct;		/* direct mountpoint ? */
	int 	dir_remount;		/* a remount */
	struct autodir *dir_next;	/* next entry */
	struct autodir *dir_prev;	/* prev entry */
};

/*
 * This structure is used to build a list of
 * mtab structures from /etc/mtab.
 */
struct mntlist {
	u_long		mntl_flags;
	dev_t		mntl_dev;
	struct mntent	*mntl_mnt;
	struct mntlist	*mntl_next;
};

#define	MNTL_UNMOUNT	0x01	/* unmount this entry */
#define	MNTL_DIRECT	0x02	/* direct mount entry */

/*
 * This structure is used to build an array of
 * hostnames with associated penalties to be
 * passed to the nfs_cast procedure
 */
struct host_names {
	char *host;
	int  penalty;
};

time_t time_now;	/* set at start of processing of each RPC call */

sem_t mysubnet_hosts; /* used when getting hosts on my subnet */
sem_t pingnfs_lock;	/* protection in pingnfs() */
sem_t stack_lock;
sem_t xc_lock;
sem_t mtab_lock;
sem_t map_lock;
sem_t misc_lock;	/* a global lock used for minor misc. things which */
			/* don't justify a separate lock unto themselves */

#define AUTOFS_LOCKINIT(s)	if (multithread) sem_init(&(s), 0, 1)
#define AUTOFS_LOCK(s)		if (multithread) sem_wait(&(s))
#define AUTOFS_UNLOCK(s)	if (multithread) sem_post(&(s))

int verbose;
int trace;
int multithread;

#define gettext(fmt)	(fmt)

struct in_addr my_addr;	/* my inet address */

enum	nfs_level	{NFSDEFAULT = 0, NFS2 = 1, NFS3 = 2};

/* autod_nfs.c */
enum clnt_stat pingnfs(struct in_addr);
int nfsunmount(struct mntent *);
int loopbackmount(char *, char *, char *, int);
int mount_nfs(struct mapent *, char *, int);
int parse_nfs(char *, struct mapent *, char *, char *, char **, char **);

/* auto_mtab.c */
void del_mtab(char *);
int  add_mtab(struct mntent *);
void fix_mtab(struct mntent *);
struct mntlist *getmntlist(void);
void freemntlist(struct mntlist *);

/* auto_subr.c */
char *get_line(FILE *, char *, char *, int);
void dirinit(char *, char *, char *, int, char **, char ***);
void unquote(char *, char *);
int macro_expand(char *, char *, char *, int);
void getword(char *, char *, char **, char **, char delim, int bufsize);
int  mkdir_r(char *);
int  rmdir_r(char *, int);
int  autofs_mkdir_r(char *, int *);

/* ns_generic.c */
int loadmaster_map(char *, char *, char **, char ***);
int loaddirect_map(char *, char *, char *, char **, char ***);
int getmapent(char *, char *, struct mapline *, char **, char ***);
int getnetmask_byaddr(char *, char **);

/* ns_files.c */
int loadmaster_files(char *, char *, char **, char ***);
int loaddirect_files(char *, char *, char *, char **, char ***);
int getmapent_files(char *, char *, struct mapline *, char **, char ***);
int getnetmask_files(char *, char **);

/* ns_nis.c */
void init_names(void);
int loadmaster_nis(char *, char *, char **, char ***);
int loaddirect_nis(char *, char *, char *, char **, char ***);
int getmapent_nis(char *, char *, struct mapline *, char **, char ***);
int getnetmask_nis(char *, char **);

/* autod_mount.c */
struct umntrequest;
int do_mount1(char *, char *, char *, char *, struct authunix_parms *);
int do_unmount1(struct umntrequest *);
int mount_generic(char *, char *, char *, char *, int);
void free_mapent(struct mapent *);
void get_opts(char *, char *, char *);

/* autod_main.c */
int self_check(char *);

/* autofs.c */
void pr_msg(char *, ...);
