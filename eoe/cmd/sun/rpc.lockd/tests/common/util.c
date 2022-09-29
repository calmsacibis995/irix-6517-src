#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <sys/stat.h>
#if _SUNOS
#include <sys/mnttab.h>
#endif /* _SUNOS */
#if _IRIX
#include <mntent.h>
#endif /* _IRIX */
#include <string.h>
#include <assert.h>
#include "util.h"

#define ROUNDDOWN( x, y )	( ((x) / (y)) * (y) )
#if _SUNOS
#undef ROUNDUP
#endif /* _SUNOS */
#define ROUNDUP( x, y ) 	ROUNDDOWN( (x) + (y) - 1, (y) )

#if _SUNOS
#define MOUNTED		MNTTAB
#endif /* _SUNOS */
static int Page_size = 0;
#if _IRIX
static int Sigsegv = 0;
#endif /* _IRIX */

extern char *Progname;
extern int Verbose;
extern int errno;

#if _SUNOS
static char *
alloc_vector( int size )
{
	static char *vec = NULL;
	static int veclen = 0;
	
	if ( vec ) {
		if ( veclen < size ) {
			if ( vec = realloc( vec, size ) ) {
				veclen = size;
			} else {
				veclen = 0;
				perror( "alloc_vector: realloc" );
			}
		}
	} else if ( vec = malloc( size ) ) {
		veclen = size;
	} else {
		veclen = 0;
		perror( "alloc_vector: malloc" );
	}
	return( vec );
}
#endif /* _SUNOS */

#if _IRIX
static void
sigsegv_handler( int sig, int code, struct sigcontext *scp )
{
	Sigsegv = 1;
}

/*
 * check all of the pages in the specified range for validity
 * only check for readability
 * check by attempting to read the first byte of each page
 * catch SIGSEGV to identify an invalid page without killing the program
 * this has the side effect of loading all of the pages into memory
 */
static int
test_pages( caddr_t start, int len )
{
	caddr_t test_addr;
	char ch;

	Sigsegv = 0;
	signal( SIGSEGV, sigsegv_handler );
	for ( test_addr = start;
		!Sigsegv && (test_addr < start + len);
		test_addr += Page_size ) {
			ch = *(char *)test_addr;
	}
	signal( SIGSEGV, SIG_DFL );
	return( !Sigsegv );
}
#endif /* _IRIX */

int
valid_addresses( caddr_t addr, int len )
{
#if _SUNOS
	char *vec = NULL;
#endif /* _SUNOS */
	int valid;
	caddr_t start_page;
	int page_bytes;

	if ( !Page_size ) {
		Page_size = getpagesize();
	}
	start_page = (caddr_t)ROUNDDOWN( (u_long)addr, (u_long)Page_size );
	page_bytes = (int)ROUNDUP( (u_long)len, (u_long)Page_size );
	/*
	 * if the address addr is NULL, it is invalid
	 * otherwise, use mincore to check the validity
	 */
	if ( !addr ) {
		valid = 0;
#if _SUNOS
	} else if ( !(vec = alloc_vector( (len / Page_size) + 1 )) ) {
		valid = 0;
#endif /* _SUNOS */
#if _IRIX
	} else if ( !test_pages( start_page, page_bytes ) ) {
#endif /* _IRIX */
#if _SUNOS
	} else if ( mincore( start_page, page_bytes, vec ) == -1 ) {
#endif /* _SUNOS */
		if ( errno == ENOMEM ) {
			fprintf( stderr, "%s: bad address range [0x%x, 0x%x)\n", Progname,
				addr, addr + len );
		} else {
			fprintf( stderr, "%s: ", Progname );
			perror( "valid_addresses: mincore" );
		}
		valid = 0;
	} else {
		valid = 1;
	}
	return( valid );
}

char *
locktype_to_str( int locktype )
{
	char *typestr = "UNKNOWN";

	switch ( locktype ) {
		case F_RDLCK:
			typestr = "F_RDLCK";
			break;
		case F_WRLCK:
			typestr = "F_WRLCK";
			break;
		case F_UNLCK:
			typestr = "F_UNLCK";
			break;
		default:
			if ( Verbose ) {
				printf( "%s: illegal lock type received: 0x%x\n", Progname,
					locktype );
			}
	}
	return ( typestr );
}

char *
lockcmd_to_str( int cmd )
{
	char *cmdstr = "UNKNOWN";

	switch ( cmd ) {
		case F_GETLK:
			cmdstr = "F_GETLK";
			break;
		case F_SETLK:
			cmdstr = "F_SETLK";
			break;
		case F_SETLKW:
			cmdstr = "F_SETLKW";
			break;
		default:
			if ( Verbose ) {
				printf( "%s: illegal lock command received: 0x%x\n", Progname,
					cmd );
			}
	}
	return ( cmdstr );
}

void
print_svcreq( struct svc_req *rqstp, char *leader )
{
	printf( "%srq_prog = %d\n", leader, rqstp->rq_prog );
	printf( "%srq_vers = %d\n", leader, rqstp->rq_vers );
	printf( "%srq_proc = %d\n", leader, rqstp->rq_proc );
}

void
report_lock_error( char *operation, char *function, char *errmsg, int status )
{
	(void)fprintf( stderr, "%s: %s: %s\n", Progname, function, errmsg );
	if ( status > 0 ) {
		errno = status;
		perror( operation );
	}
}

char *
absolute_path(char *path)
{
	static char newpath[MAXPATHLEN];
	char *sp;
	char *sp2;

	if (path) {
		switch (*path) {
			case '/':		/* already absolute */
				break;
			case '.':		/* possibly a relative path name */
				if (*(path + 1) == '/') {
					path++;
				}
			default:
				if (!getcwd(newpath, MAXPATHLEN)) {
					report_lock_error( "getcwd", "absolute_path", path, errno );
					exit(errno);
				}
				strcat(newpath, "/");
				strcat(newpath, path);
				path = newpath;
		}
		/*
		 * eliminate all "./" patterns
		 */
		while (sp = strstr(path, "/./")) {
			if (strlen(sp + 3) == 0) {
				*sp = '\0';
				break;
			} else {
				strcpy(sp + 1, sp + 3);
			}
		}
		/*
		 * eliminate all "../" patterns
		 */
		while (sp = strstr(path, "/../")) {
			sp2 = strrchr(sp - 1, '/');
			if (!sp2) {
				fprintf(stderr, "invalid path %s\n", path);
				exit(-1);
			}
			if (strlen(sp + 4) == 0) {
				*sp2 = '\0';
				break;
			} else {
				strcpy(sp2 + 1, sp + 4);
			}
		}
	} else {
		path = getcwd(newpath, MAXPATHLEN);
		if (!path) {
			report_lock_error( "getcwd", "absolute_path", path, errno );
			exit(errno);
		}
	}
	return(path);
}

struct mntent *
find_mount_point(char *path)
{
	FILE *fp;
#if _IRIX
	struct mntent *mnt = NULL;
	struct mntent *found = NULL;
#endif /* _IRIX */
#if _SUNOS
	struct mntent mnt;
	static struct mntent found;
#endif /* _SUNOS */
	int len = 0;
	int foundlen = 0;

	fp = setmntent(MOUNTED, "r");
	if (fp) {
		/*
		 * find the lenght of the name of the matching mount point
		 */
#if _IRIX
		while(mnt = getmntent(fp)) {
			len = strlen(mnt->mnt_dir);
			if ((len > foundlen) && (strncmp(mnt->mnt_dir, path, len) == 0)) {
				foundlen = len;
			}
		}
		/*
		 * rewind the file and locate the mount point with a name of the
		 * proper length which also matches the path
		 */
		rewind(fp);
		while(mnt = getmntent(fp)) {
			if ((strlen(mnt->mnt_dir) == foundlen) &&
				(strncmp(mnt->mnt_dir, path, foundlen) == 0)) {
					found = mnt;
					break;
			}
		}
#endif /* _IRIX */
#if _SUNOS
		while(getmntent(fp, &mnt) == 0) {
			len = strlen(mnt.mnt_mountp);
			if ((len > foundlen) &&
				(strncmp(mnt.mnt_mountp, path, len) == 0)) {
					foundlen = len;
			}
		}
		/*
		 * rewind the file and locate the mount point with a name of the
		 * proper length which also matches the path
		 */
		rewind(fp);
		while(getmntent(fp, &mnt) == 0) {
			if ((strlen(mnt.mnt_mountp) == foundlen) &&
				(strncmp(mnt.mnt_mountp, path, foundlen) == 0)) {
					found = mnt;
					break;
			}
		}
#endif /* _SUNOS */
		endmntent(fp);
#if _IRIX
		assert(found);
#endif /* _IRIX */
	} else {
		report_lock_error( "setmntent", "find_mount_point", path, errno );
		exit(errno);
	}
#if _IRIX
	return(found);
#endif /* _IRIX */
#if _SUNOS
	return(&found);
#endif /* _SUNOS */
}

void
get_host_info(char *fsname, char **hostname, char **path)
{
	static char namebuf[MAXPATHLEN];
	char *cp = NULL;

	if (cp = strchr(fsname, ':')) {
		*hostname = fsname;
		*cp = '\0';
		*path = cp + 1;
	} else if ( gethostname( namebuf, MAXPATHLEN ) ) {
		report_lock_error( "gethostname", "get_host_info", fsname, errno );
		exit(errno);
	} else {
		*hostname = namebuf;
	}
}

int
isfile(char *path)
{
	struct stat sb;

	if (stat(path, &sb) == -1) {
		if (errno != ENOENT) {
			report_lock_error( "stat", "isfile", path, errno );
		}
		return(0);
	}
	return(S_ISREG(sb.st_mode));
}
