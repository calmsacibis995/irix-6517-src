/* u_Lib.h - simulator UNIX/VxSim interface header */

/*
modification history
--------------------
02d,19jan94,gae  alphabetized.
02c,14dec93,gae  added setsid, system, execlp, fork.
02b,10jul93,gae  fixed dirent's.
02a,09jan93,gae  renamed from sysULib.h; discarded major portions.
01c,05aug92,gae  added non-ANSI forward declarations.
01b,29jul92,gae  added more prototypes; ANSIfied.
01a,04jun92,gae  written.
*/

/*
This header contains the prototype declarations for all UNIX
calls made by VxSim.
*/

#ifndef	INCu_Libh
#define	INCu_Libh

#ifndef SIMSPARCSUNOS
#define SIMSPARCSUNOS   50      /* CPU & CPU_FAMILY */
#endif

#ifndef SIMHPPA
#define SIMHPPA         60      /* CPU & CPU_FAMILY */
#endif

/*
 * Definitions for UNIX library routines operating on directories.
 */

#if	CPU==SIMSPARCSUNOS
typedef struct unix__dirdesc
    {
    int     dd_fd;          /* file descriptor */
    long    dd_loc;         /* buf offset of entry from last readddir() */
    long    dd_size;        /* amount of valid data in buffer */
    long    dd_bsize;       /* amount of entries read at a time */
    long    dd_off;         /* Current offset in dir (for telldir) */
    char    *dd_buf;        /* directory data buffer */
    } UNIX_DIR;

struct unix_dirent
    {
    long            d_off;          /* offset of next disk dir entry */
    unsigned long   d_fileno;       /* file number of entry */
    unsigned short  d_reclen;       /* length of this record */
    unsigned short  d_namlen;       /* length of string in d_name */
    char            d_name[255+1];  /* name (up to MAXNAMLEN + 1) */
    };

struct unix_stat
    {
    short		st_dev;
    unsigned long	st_ino;
    unsigned short	st_mode;
    unsigned short	st_nlink;
    unsigned short	st_uid;
    unsigned short	st_gid;
    short		st_rdev;
    long		st_size;
    long		st_atime;
    int			st_spare1;
    long		st_mtime;
    int			st_spare2;
    long		st_ctime;
    int			st_spare3;
    long		st_blksize;
    long		st_blocks;
    long                st_spare4[2];
    };
#endif	/* CPU==SIMSPARCSUNOS */

#if	CPU==SIMHPPA
typedef struct unix__dirdesc
    {
    int   dd_fd;            /* file descriptor */
    long  dd_loc;
    long  dd_size;
    long  dd_bbase;	    /* ? */
    /*  long  dd_entno;     / * directory entry number */
    long    dd_off;         /* Current offset in dir (for telldir) !XXX */
    long  dd_bsize;         /* buffer size */
    char  *dd_buf;          /* malloc'ed buffer */
    } UNIX_DIR;

struct unix_dirent
    {
    /* ino_t d_ino;        / * file number of entry */
    long  d_off;           /* offset of next disk dir entry !XXX */
    short d_reclen;        /* length of this record */
    short d_namlen;        /* length of string in d_name */
    char  d_name[255 + 1]; /* name must be no longer than this */
    };

typedef unsigned short hp__site_t;   /* see stat.h */
typedef unsigned short hp__cnode_t;  /* see stat.h */
typedef long hp_dev_t;                /* For device numbers */
typedef unsigned long hp_ino_t;       /* For file serial numbers */
typedef long hp_gid_t;                /* For group IDs */
typedef long hp_uid_t;                /* For user IDs */

struct unix_stat
    {
    short		st_dev;
    unsigned long	st_ino;
    unsigned short	st_mode;
    unsigned short	st_nlink;
    unsigned short st_reserved1; /* old st_uid, replaced spare positions */
    unsigned short st_reserved2; /* old st_gid, replaced spare positions */
    short		st_rdev;
    long		st_size;
    long		st_atime;
    int			st_spare1;
    long		st_mtime;
    int			st_spare2;
    long		st_ctime;
    int			st_spare3;
    long		st_blksize;
    long		st_blocks;
    unsigned int    st_pad:30;
    unsigned int    st_acl:1;
    unsigned int    st_remote:1;
    hp_dev_t   st_netdev;
    hp_ino_t   st_netino;
    hp__cnode_t       st_cnode;
    hp__cnode_t       st_rcnode;
    hp__site_t        st_netsite;
    short   st_fstype;
    hp_dev_t   st_realdev;
    unsigned short  st_basemode;
    unsigned short  st_spareshort;
#ifdef _CLASSIC_ID_TYPES
    unsigned short st_filler_uid;
    unsigned short st_uid;
#else
    hp_uid_t   st_uid;
#endif
#ifdef _CLASSIC_ID_TYPES
    unsigned short st_filler_gid;
    unsigned short st_gid;
#else
    hp_gid_t   st_gid;
#endif
#define _SPARE4_SIZE 3
    long    st_spare4[_SPARE4_SIZE];
    };
#endif  /* CPU==SIMHPPA */


struct unix_tm
    {
    int     tm_sec;
    int     tm_min;
    int     tm_hour;
    int     tm_mday;
    int     tm_mon;
    int     tm_year;
    int     tm_wday;
    int     tm_yday;
    int     tm_isdst;
#if	CPU==SIMSPARCSUNOS
    char    *tm_zone;
    long    tm_gmtoff;
#endif	/* CPU==SIMSPARCSUNOS */
    };

#ifdef __STDC__

/* UNIX routines */

extern int u_connect (int s, int name, int namelen);
extern int u_closedir (struct unix__dirdesc *dirp);
extern int u_close (int fd);
extern int u_dup2 (int fd1, int fd2);
extern int u_errno (void);
extern int u_execl (char *path, int arg0, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9);
extern int u_execlp (char *file, int arg0, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9);
extern int u_execvp (char *file, char *argv[]);
extern void u_exit (int status);
extern int u_fcntl (int fd, int cmd, int arg);
extern int u_fork (void);
extern int u_fpsetmask (int value);
extern int u_free (char *ptr);
extern int u_fstat (int fd, char *buf);
extern char *u_getcwd (char *pathname, int size);
extern char *u_getenv (char *name);
extern int u_getgid (void);
extern void *u_gethostbyname (char *name);
extern int u_gethostname (char *name, int namelen);
extern int u_getpgrp (int pid);
extern int u_getpid (void);
extern int u_getuid (void);
extern char *u_getwd (char *pathname);
extern int u_ioctl (int fd, int request, char *arg);
extern int u_isatty (int fd);
extern int u_kill (int pid, int sig);
extern int u_killpg (int pgrp, int sig);
extern struct tm *u_localtime (long *clock);
extern void u_longjmp (char *jmpbuf, int ret);
extern int u_lseek (int fd, int offset, int whence);
extern void *u_malloc (int size);
extern int u_mkdir (char *path, int mode);
extern int u_open (char *name, int flags, int mode);
extern struct unix__dirdesc *u_opendir (char *dirname);
extern int u_read (int fd, char *buf, int len);
extern struct unix_dirent *u_readdir (struct unix__dirdesc *dirp);
extern int u_rename (char *path1, char *path2);
extern int u_rmdir (char *path);
extern int u_select (int width, int *readfds, int *writefds, int *exceptfds, int *timeout);
extern int u_setitimer (int which, int *value, int *ovalue);
extern int u_setjmp (char *jmpbuf);
extern int u_setsid (void);
extern char *u_shmat (int shmid, char *shmaddr, int shmflg);
extern int u_shmctl (int shmid, int cmd, char *buf);
extern int u_shmget (int key, int size, int shmflg);
extern int u_sigaction (int sig, int *act, int *oact);
extern int u_sigaddset (int *set, int signo);
extern int u_sigblock (int mask);
extern int u_sigdelset (int *set, int signo);
extern int u_sigemptyset (int *set);
extern int u_sigfillset (int *set);
extern int u_sigsetmask (int mask);
extern int u_sigstack (int *stk, int *ostk);
extern int u_sigsuspend (int *pMask);
extern int u_socket (int domain, int type, int protocol);
extern int u_stat (char *path, char *buf);
extern int u_system (char *cmd);
extern int u_tcgetpgrp (int fd);
extern int u_time (int *tloc);
extern int u_uname (char *name);
extern int u_unlink (char *path);
extern int u_wait (int *statusp);
extern int u_write (int fd, char *buf, int len);

#else

extern int u_connect (/* int s, int name, int namelen */);
extern int u_closedir (/* struct unix__dirdesc *dirp */);
extern int u_close (/* int fd */);
extern int u_dup2 (/* int fd1, int fd2 */);
extern int u_errno (/* void */);
extern int u_execl (/* char *path, int arg0, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9 */);
extern int u_execlp (/* char *file, int arg0, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9 */);
extern int u_execvp (/* char *file, char *argv[] */);
extern void u_exit (/* int status */);
extern int u_fcntl (/* int fd, int cmd, int arg */);
extern int u_fork (/* void */);
extern int u_free (/* char *ptr */);
extern int u_fstat (/* int fd, char *buf */);
extern char *u_getcwd (/* char *pathname, int size */);
extern char *u_getenv (/* char *name */);
extern int u_getgid (/* void */);
extern void *u_gethostbyname (/* char *name */);
extern int u_gethostname (/* char *name, int namelen */);
extern int u_getpgrp (/* int pid */);
extern int u_getpid (/* void */);
extern int u_getuid (/* void */);
extern char *u_getwd (/* char *pathname */);
extern int u_ioctl (/* int fd, int request, char *arg */);
extern int u_isatty (/* int fd */);
extern int u_kill (/* int pid, int sig */);
extern int u_killpg (/* int pgrp, int sig */);
extern struct tm *u_localtime (/* long *clock */);
extern void u_longjmp (/* char *jmpbuf, int ret */);
extern int u_lseek (/* int fd, int offset, int whence */);
extern void *u_malloc (/* int size */);
extern int u_mkdir (/* char *path, int mode */);
extern int u_open (/* char *name, int flags, int mode */);
extern struct unix__dirdesc *u_opendir (/* char *dirname */);
extern int u_read (/* int fd, char *buf, int len */);
extern struct unix_dirent *u_readdir (/* struct unix__dirdesc *dirp */);
extern int u_rename (/* char *path1, char *path2 */);
extern int u_rmdir (/* char *path */);
extern int u_select (/* int width, int *readfds, int *writefds, int *exceptfds, int *timeout */);
extern int u_setjmp (/* char *jmpbuf */);
extern int u_setitimer (/* int which, int *value, int *ovalue */);
extern int u_setsid (/* void */);
extern char *u_shmat (/* int shmid, char *shmaddr, int shmflg */);
extern int u_shmctl (/* int shmid, int cmd, char *buf */);
extern int u_shmget (/* int key, int size, int shmflg */);
extern int u_sigaction (/* int sig, int *act, int *oact */);
extern int u_sigaddset (/* int *set, int signo */);
extern int u_sigblock (/* int mask */);
extern int u_sigdelset (/* int *set, int signo */);
extern int u_sigemptyset (/* int *set */);
extern int u_sigfillset (/* int *set */);
extern int u_sigsetmask (/* int mask */);
extern int u_sigstack (/* int *stk, int *ostk */);
extern int u_sigsuspend (/* int *pMask */);
extern int u_socket (/* int domain, int type, int protocol */);
extern int u_stat (/* char *path, char *buf */);
extern int u_system (/* char *cmd */);
extern int u_tcgetpgrp (/* int fd */);
extern int u_time (/* int *tloc */);
extern int u_uname (/* char *name */);
extern int u_unlink (/* char *path */);
extern int u_wait (/* int *statusp */);
extern int u_write (/* int fd, char *buf, int len */);
#endif	/* __STDC__ */

#endif	/* INCu_Libh */
