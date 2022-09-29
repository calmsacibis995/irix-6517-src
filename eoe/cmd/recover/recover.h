/* recover.h */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* limits */

#define		Maxargs		32		/* command mode arguments */
#define		Strsize		256		/* general buffers */
#define		Pathlen		1024		/* path name length */

#define		Silent		1	/* run process silently */
#define		Noisy		2	/* run process noisily */

#define		Root		1	/* interested in /root device */
#define		Usr		2	/* interested in /root/usr device */

#define N(p)	(sizeof (p) / sizeof (*p))
