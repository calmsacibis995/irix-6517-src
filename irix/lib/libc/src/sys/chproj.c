#ident "$Revision: 1.1 $"

/*
 * chproj - change project id associated with a given file.
 */

#ifdef __STDC__
#pragma weak chproj 	= _chproj
#pragma weak lchproj 	= _lchproj
#pragma weak fchproj 	= _fchproj
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/syssgi.h>
#include <errno.h>

int 
chproj(const char *path, prid_t project)
{
	if (path == NULL || project < 0) {
		setoserror(EINVAL);
		return (EINVAL);
	}
	
	/*
	 * project id is only 16-bits inside the inode,
	 * and that ought to be enough.
	 */
	if ((project & 0xffff) != project) {
		setoserror(EOVERFLOW);
		return (EOVERFLOW);
	}
		
	return ((int) syssgi(SGI_CHPROJ, path, &project));
}

int 
lchproj(const char *path, prid_t project)
{
	if (path == NULL || project < 0) {
		setoserror(EINVAL);
		return (EINVAL);
	}
	
	if ((project & 0xffff) != project) {
		setoserror(EOVERFLOW);
		return (EOVERFLOW);
	}
		
	return ((int) syssgi(SGI_LCHPROJ, path, &project));
}

int 
fchproj(int fd, prid_t project)
{
	if ((project & 0xffff) != project) {
		setoserror(EOVERFLOW);
		return (EOVERFLOW);
	}

	return ((int) syssgi(SGI_FCHPROJ, fd, &project));
}

