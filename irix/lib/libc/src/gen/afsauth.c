#include "synonyms.h"

#include <stdio.h>
#include <limits.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pfmt.h>
#include <string.h>

/*
 * Use DSOs to get at alternate authentication routines -
 * This includes a new rcmd(3N) as well as other routines
 * used by login-style programs
 * We handle kerberos style authentication by looking
 * for a kerberos compatible library first - afskauthlib.so
 * If that is available - use it otherwise look for afsauthlib.so
 */

static void *authlib_handle;

/* make constant to minimize data in libc */
#define AUTHMAXNAMSZ	16
static const char authlib_name[][AUTHMAXNAMSZ] = {
	"afskauthlib.so",
	"afsauthlib.so",
};
static const char authlib_search[][AUTHMAXNAMSZ] = {
	"/usr/vice/etc/",	/* clients */
	"/usr/afsws/lib/",	/* clients */
	"",			/* default search path */
};

static char *libname;
static int iskauth;		/* set to 1 if found the kauth lib */

int (*__afs_getsym(char *sym))()
{
	register int ni, si;
	char *lname, libpath[PATH_MAX];
	int (*retv)() = NULL;
	struct stat sb;

	if (authlib_handle == NULL) {
		/*
		 * To speed up searches when AFS is not installed -
		 * look for the /usr/vice/etc directory - if it
		 * doesn't exist then skip all this
		 */
		if (stat("/usr/vice/etc", &sb) != 0)
			return NULL;
		for (ni = 0; ni < sizeof(authlib_name)/AUTHMAXNAMSZ; ++ni) {
			lname = (char *)authlib_name[ni];
			for (si = 0; si < sizeof(authlib_search)/AUTHMAXNAMSZ; ++si) {
				strcpy(libpath, authlib_search[si]);
				strcat(libpath, lname);
				/*
				 * BUG in rld - it exits if lib doesn't exist
				 */
				if (stat(libpath, &sb) != 0)
					continue;
				if ((authlib_handle = dlopen(libpath, RTLD_LAZY)) != NULL) {
					libname = strdup(libpath);
					if (strcmp(lname, "afskauthlib.so") == 0)
						iskauth = 1;
					goto found;
				}
			}
		}
	}
found:
	if (authlib_handle) {
		retv = (int (*)()) dlsym(authlib_handle, sym);
		if (retv == NULL) {
			pfmt(stderr, MM_ERROR,
				"uxsgicore:706:Can't find function %s in %s; Proceeding with local authentication\n", sym, libname);
		}
	}
	return retv;
}

int
__afs_iskauth(void)
{
	return iskauth;
}
