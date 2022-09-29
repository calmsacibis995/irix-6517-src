/*
 * COPYRIGHT NOTICE
 * Copyright 1995, Silicon Graphics, Inc. All Rights Reserved.
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 *
 */

/*
 * chcap - set the requested capability set on files.
 *
 */

#ident	"$Revision: 1.2 $"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <errno.h>
#include <sys/syssgi.h>

/*
 * cap_delete_file() is not in libc in order to guarantee
 * forward binary compatibility within Irix 6.5.x
 * It could be moved to libc in Irix 6.6
 */

int
cap_delete_file(const char *path)
{
        if (syssgi(SGI_CAP_SET, path, -1, NULL) == -1)
                return (-1);
        else
                return 0;
}


int
main (int argc, char *argv[])
{
	cap_t capset;		/* capabilities on file */
	char *new_string;
	size_t new_len;
	int i;
	int exitcode = 0;

	/*
	 * Check number of arguments
	 */
	if (argc <= 1) {
		fprintf (stderr, "%s: No capabilities specified\n", argv[0]);
		exit (1);
	}
	if (argc == 2) {
		fprintf (stderr, "%s: No files specified\n", argv[0]);
		exit (1);
	}

	if ( strcmp ( argv[1], "-r" ) == 0 )
	{
	    for (i = 2; i < argc; i++)
		if ( cap_delete_file ( argv[i] ) != 0 )
		{
		    exitcode = 1;
		    perror ( argv[i] );
		    continue;
		}
	    exit ( exitcode );
	}

	/*
	 * The second argument is the delta to the old capability state
	 */
	if ((capset = cap_from_text(argv[1])) == NULL) {
	    fprintf (stderr, "%s: Invalid capability specification\n",
		     argv[0]);
	    exit (1);
	}
	cap_free(capset);
	new_len = strlen(argv[1]);
	    
	/*
	 * Set file capabilities. Report errors for each file.
	 */
	for (i = 2; i < argc; i++) {
	    if ((capset = cap_get_file(argv[i])) == NULL) {
		if ( errno == ENOATTR )
		    /* This file has no capabilities:
		     * build the new capabilities from 
		     * the command argument */
		    capset = cap_from_text( argv[1] );

		else
		{
		    exitcode = 1;
		    perror (argv[i]);
		    continue;
		}
	    }
	    else
	    {
		/* Build the new capabilities by combining the old 
		 * and the command argument
		 */
		char *old_string;
		size_t old_len;
		
		old_string = cap_to_text(capset, &old_len);
		cap_free(capset);
		
		if (old_string == NULL) {
		    exitcode = 1;
		    perror (argv[i]);
		    continue;
		}
		new_string = malloc(old_len + new_len + 2);
		sprintf(new_string, "%s %s", old_string, argv[1]);
		cap_free(old_string);
		
		capset = cap_from_text(new_string);
		free(new_string);
	    }
	    
	    if (capset == NULL) {
		exitcode = 1;
		perror (argv[i]);
		continue;
	    }
	    
	    if (cap_set_file(argv[i], capset) == -1)
	    {
		exitcode = 1;
		perror (argv[i]);
	    }
	    
	    cap_free(capset);
	}

	return ( exitcode );
}
