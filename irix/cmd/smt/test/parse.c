/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

#include <stdio.h>
#include <smt_parse.h>
#include <sm_log.h>

extern struct node * test_parse(FILE *);
extern struct tree * test_build_tree(struct node *);

main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *fp;
	struct node *nodes;
	struct tree *tp;

	sm_openlog(SM_LOGON_STDERR, LOG_DEBUG+1, 0, 0, 0);
	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		sm_log(LOG_ERR, 0, "open %s failed", argv[1]);
		return 1;
	}

	sm_log(LOG_DEBUG, 0, "**** PARSE START ****");
	nodes = test_parse(fp);
	sm_log(LOG_DEBUG, 0, "**** PARSE END ****");

	sm_log(LOG_DEBUG, 0, "**** BUILD TREE START ****");
	tp = test_build_tree(nodes);
	sm_log(LOG_DEBUG, 0, "**** BUILD TREE END ****");

	sm_log(LOG_DEBUG, 0, "**** PRINTING TREE ****");
	print_subtree(tp, 0);
	fclose(fp);
}
