/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <stdio.h>
#include <string.h>
#include "getabi.h"

typedef char* string;
#define same_string(a,b)	(strcmp(a,b) == 0)

#define USAGE	"usage:  getabi [size] <ignore|preserve|remove|add> [<args>]\n"

extern int
main (int argc, char **argv, char **envp)
{
	char **av;
        int modify;
	int generic;
	string s;
        abi_t abi;
	int i = 1;

	s = argv[i];	/* check for generic size */
	if (s == NULL) {
		fprintf(stderr, USAGE);
		return (noabi);
	} else if (same_string(s, "size")) {
		generic = GENERIC_ABI_SIZE;
		i++;
	} else {
		generic = SPECIFIC_ABI;
	}
	s = argv[i];	/* check for modify-abi-arg */
	if (s == NULL) {
		fprintf(stderr, USAGE);
		return (noabi);
	} else if (same_string(s, "ignore")) {
		modify = IGNORE_ABI_ARG;
	} else if (same_string(s, "preserve")) {
		modify = PRESERVE_ABI_ARG;
	} else if (same_string(s, "remove")) {
		modify = REMOVE_ABI_ARG;
	} else if (same_string(s, "add")) {
		modify = ADD_ABI_ARG;
	} else {
		fprintf(stderr, "getabi:  unknown option %s\n", s);
		fprintf(stderr, USAGE);
		return (noabi);
	}

	abi = getabi(generic, modify, &argv, &argc);

	av = argv + i + 1;
	if (*av != NULL) {
		printf("%s", *av);
		for (*av++; *av != NULL; *av++) {
			printf(" %s", *av);
		}
	}
	printf("\n");
	return (abi);
}
