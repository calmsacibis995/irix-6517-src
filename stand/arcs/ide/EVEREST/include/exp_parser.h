
/*
 * parser.h -- definitions for command parser routines
 */

/*
 * mc3_range structure  for parse_addr()
 */
struct mc3_range {
	__psunsigned_t lo;		/* beginning address */
	__psunsigned_t hi;		/* ending address */
	char size;			/* byte, half word or word */
};

/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
/* struct cmd_table { */
/*	char *ct_string;		 command name */
/*	int (*ct_routine)();		 implementing routine */
/*	char *ct_usage;			 syntax */
/* }; */

#define	LINESIZE	128		/* line buffer size */

/* define error level */

#define VERBOSE		1
#define SILENT		2
#define STOP		3
#define LOOPBACK	4

/*
 * help -- basic table for each subtest.
 */

struct help {
	char num; 	/* number of test */
	int (*routine)();
	char *desc; 
};

/* 
 * test -- structure of main test table 
 */
 
struct test {
	int test_type;
	char *id;
	int  cpu_type; /* value returned from cpuboard() */
/*	int  (*routine)(); */
	struct excmd *sanity;
	struct help *menu;
	char *desc;
};
