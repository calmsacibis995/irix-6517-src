/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>

main(int argc, char **argv)
{
    int		verbose = 0;
    extern int	errno;

    if (argc > 1 && strcmp(argv[1], "-v") == 0) {
	verbose = 1;
	argc--;
	argv++;
    }
    if (argc != 2 || (argc == 2 && strcmp(argv[1], "-?") == 0)) {
	fprintf(stderr, "Usage: pmlock [-v] file\n");
	exit(1);
    }
    if (open(argv[1], O_CREAT|O_EXCL|O_RDONLY, 0) < 0) {
	if (verbose) {
	    if (errno == EACCES) {
		char	*p = dirname(argv[1]);
		if (access(p, W_OK) == -1)
		    printf("%s: Directory not writeable\n", p);
		else
		    printf("%s: %s\n", argv[1], strerror(EACCES));
	    }
	    else
		printf("%s: %s\n", argv[1], strerror(errno));
	}
	exit(1);
    }
	
    exit(0);
    /*NOTREACHED*/
}
