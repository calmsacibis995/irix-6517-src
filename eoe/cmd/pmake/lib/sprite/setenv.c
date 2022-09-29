/* 
 * setenv.c --
 *
 *	Contains the source code for the "setenv" library procedure.
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#if !defined(lint) && defined(keep_rcsid)
static char rcsid[] = "Header: setenv.c,v 1.2 88/07/25 11:11:00 ouster Exp $ SPRITE (Berkeley)";
#endif not lint

#include <stdio.h>

extern char **environ;

/*
 *----------------------------------------------------------------------
 *
 * setenv --
 *
 *	Associate the value "value" with the environment variable
 *	"name" in this process's environment.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The storage for the environment is modified.  If there already
 *	was an environment variable by the given name, then it is
 *	replaced.  Otherwise a new environment variable is created.
 *	The new value will be visible to this process, and also will
 *	be passed to children processes.
 *
 *----------------------------------------------------------------------
 */

void
setenv(name, value)
    char *name;			/* Name of environment variable. */
    char *value;		/* (New) value for variable. */
{
    register int    i;
    register char **envPtr;
    register char **newEnvPtr;
    register char *charPtr;
    register char *namePtr;
    char *newEnvValue;

    newEnvValue = (char *)malloc ((unsigned) (strlen (name) +
					      strlen (value) + 2));
    if (newEnvValue == 0) {
	return;
    }
    (void) sprintf(newEnvValue, "%s=%s", name, value);

    /*
     * Although this procedure allocates new storage when necessary,
     * it can't de-allocate the old storage, because it doesn't know
     * which things were allocated with malloc and which things were
     * allocated statically when the process was created.
     */

    for (envPtr = environ, i=0; *envPtr != 0; envPtr++, i++) {
	for (charPtr = *envPtr, namePtr = name;
	     *charPtr == *namePtr; namePtr++) {
	     charPtr++;
	     if (*charPtr == '=') {
		 namePtr++;
		 if (*namePtr == '\0') {
		     *envPtr = newEnvValue;
		     return;
		 }
		 break;
	     }
	 }
    }
    newEnvPtr = (char **) malloc ((unsigned) ((i + 2) * sizeof *newEnvPtr));
    if (newEnvPtr == 0) {
	return;
    }
    for (envPtr = environ, i = 0; *envPtr; envPtr++, i++) {
	newEnvPtr[i] = *envPtr;
    }
    newEnvPtr[i] = newEnvValue;
    newEnvPtr[i+1] = 0;
    environ = newEnvPtr;
}
