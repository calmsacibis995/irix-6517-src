/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $"

#include "unistd.h"
#include "stdlib.h"
#include "wait.h"
#include "malloc.h"
#include "stdio.h"

int
main(int argc, char **argv)
{
	char *ptr2, *ptr;
	int nloops, cursize, size;

	if (argc <= 1) {
		fprintf(stderr, "Usage:%s nloops [size]\n", argv[0]);
		exit(-1);
	}
	nloops = atoi(argv[1]);
	printf("%s:nloops %d\n", argv[0], nloops);
	fprintf(stderr, "%s:nloops %d\n", argv[0], nloops);
	if (argc > 2)
		size = atoi(argv[2]);
	else
		size = 1024;
	printf("re-alloc size:%d\n", size);
	ptr = malloc(1024);
	cursize = size;

	while (nloops--) {
		if ((ptr2 = realloc(ptr, cursize)) == NULL) {
			fprintf(stderr, "realloc failed\n");
			exit(-1);
		}
		printf("realloc ptr:%x\n", ptr2);
		cursize += size;
		ptr = ptr2;
	}
	return 0;
}
