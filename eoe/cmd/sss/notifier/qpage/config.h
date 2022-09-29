/* config.h.  Generated automatically by configure.  */

/*
** QuickPage specific definitions
*/

/* Define if this is a Solaris 2.x machine.  */
/* #undef SOLARIS2 */

/* Define if this is a SunOS 4.x machine.  */
/* #undef SUNOS4 */

/* Define as the name(s) of the default SNPP server.  */
#define SNPP_SERVER "localhost"

/* Define as the location of a file containing the name(s) of
   the default SNPP server.  */
#define SNPP_SERVER_FILE "/etc/qpage.servers"

/* Define as the location of the qpage configuration file.  */
#define QPAGE_CONFIG "/etc/qpage.cf"

/* Define as the location of the lock directory.  */
#define DEFAULT_LOCKDIR "/var/spool/qpage"

/* Define as the user the qpage daemon runs as.  */
#define DAEMON_USER "daemon"

/* Define as the syslog facility used to log messages (i.e. LOG_DAEMON).  */
#define SYSLOG_FACILITY LOG_DAEMON

/* Define if you want to link qpage with the "tcp_wrappers" package.  */
/* #undef TCP_WRAPPERS */

/* Define as the complete path to sendmail (or equivalent).  */
#define SENDMAIL_PATH "/usr/lib/sendmail"



/*
** Generic platform definitions
*/

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef gid_t */

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you have a working fnmatch function.  */
/* #undef HAVE_FNMATCH */

/* Define if you support file names longer than 14 characters.  */
/* #undef HAVE_LONG_FILE_NAMES */

/* Define if you have sys_siglist.  */
/* #undef SYS_SIGLIST_DECLARED */

/* Define if you have a working `mmap' system call.  */
/* #undef HAVE_MMAP */

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if your struct stat has st_rdev.  */
#define HAVE_ST_RDEV 1

/* Define if you have the strchr function.  */
#define HAVE_STRCHR 1

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#define HAVE_SYS_WAIT_H 1

/* Define if you have the waitpid function.  */
#define HAVE_WAITPID 1

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the vsyslog function.  */
#define HAVE_VSYSLOG 1

/* Define if major, minor, and makedev are declared in <mkdev.h>.  */
/* #undef MAJOR_IN_MKDEV */

/* Define if major, minor, and makedev are declared in <sysmacros.h>.  */
/* #undef MAJOR_IN_SYSMACROS */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define if you have <memory.h>, and <string.h> doesn't declare the
   mem* functions.  */
#define HAVE_MEMORY_H 1

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef uid_t */

/* Define if you have the mktime function.  */
#define HAVE_MKTIME 1

/* Define if you have the poll function.  */
/* #define HAVE_POLL 1 */

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the socket function.  */
#define HAVE_SOCKET 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strstr function.  */
#define HAVE_STRSTR 1

/* Define if you have the uname function.  */
#define HAVE_UNAME 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <setjmp.h> header file.  */
#define HAVE_SETJMP_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <syslog.h> header file.  */
#define HAVE_SYSLOG_H 1

/* Define if you have the <poll.h> header file.  */
#define HAVE_POLL_H 1

/* Define if you have the <sys/poll.h> header file.  */
#define HAVE_SYS_POLL_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/ioctl.h> header file.  */
#define HAVE_SYS_IOCTL_H 1

/* Define if you have the <sys/modem.h> header file.  */
/* #undef HAVE_SYS_MODEM_H */

/* Define if you have the <sys/mkdev.h> header file.  */
#define HAVE_SYS_MKDEV_H 1

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/systeminfo.h> header file.  */
/* #undef HAVE_SYS_SYSTEMINFO_H */

/* Define if you have the <sys/time.h> header file.  */
/* #undef HAVE_SYS_TIME_H */

/* Define if you have the <sys/utsname.h> header file.  */
/* #undef HAVE_SYS_UTSNAME_H */

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <unistd.h> header file.  */
/* #undef HAVE_IOCTL_TYPES_H */

/* Define if you have the <sys/digi.h> header file.  */
/* #undef HAVE_SYS_DIGI_H */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <stdarg.h>
#include <pwd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <grp.h>


#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

#ifdef HAVE_POLL
# ifdef HAVE_POLL_H
#  include <poll.h>
# else
#  ifdef HAVE_SYS_POLL_H
#   include <sys/poll.h>
#  endif
# endif
#endif

#ifdef HAVE_SYS_MKDEV_H
#include <sys/mkdev.h>
#endif

#ifdef HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
/*
** <sys/ioctl.h> has namespace conflict with <termios.h> in SunOS 4.x
*/
#ifndef SUNOS4
#include <sys/ioctl.h>
#endif
#endif

#ifdef HAVE_SYS_MODEM_H
#include <sys/modem.h>
#endif

#ifdef HAVE_IOCTL_TYPES_H
#include <ioctl-types.h>
#endif

/*
** SCO apparently needs this (?)
*/
#ifdef HAVE_SYS_DIGI_H
#include <sys/digi.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif


#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif


#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif


#if STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif
