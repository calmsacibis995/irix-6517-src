/*	@(#)rpc_util.h	1.3 90/07/25 4.1NFSSRC SMI	*/

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * @(#) from SUN 1.9
 */

/*
 * rpc_util.h, Useful definitions for the RPC protocol compiler 
 */
extern char *malloc();

#define alloc(size)		malloc((unsigned)(size))
#define ALLOC(object)   (object *) malloc(sizeof(object))

#define s_print	(void) sprintf
#define f_print (void) fprintf

struct list {
	char *val;
	struct list *next;
};
typedef struct list list;

/*
 * Global variables 
 */
#define MAXLINESIZE 1024
extern char curline[MAXLINESIZE];
extern char *where;
extern int linenum;

extern char *infilename;
extern FILE *fout;
extern FILE *fin;

extern list *defined;

/*
 * All the option flags
 */
extern int inetdflag;
extern int tblflag;
extern int logflag;

/*
 * Other flags related with inetd jumpstart.
 */
extern int indefinitewait;
extern int exitnow;
extern int timerflag;

extern int nonfatalerrors;

/*
 * rpc_util routines 
 */
void storeval();

#define STOREVAL(list,item)	\
	storeval(list,(char *)item)

char *findval();

#define FINDVAL(list,item,finder) \
	findval(list, (char *) item, finder)

char *fixtype();
char *stringfix();
char *locase();
void pvname();
void ptype();
int isvectordef();
int streq();
void error(char *);
void perrorf(char *,...);
void expected1();
void expected2();
void expected3();
void tabify();
void record_open();

/*
 * rpc_cout routines 
 */
void emit();

/*
 * rpc_hout routines 
 */
void print_datadef();

/*
 * rpc_svcout routines 
 */
void write_most();
void write_register();
void write_rest();
void write_programs();
void write_svc_aux();

/*
 * rpc_clntout routines
 */
void write_stubs();

/*
 * rpc_tblout routines
 */
void write_tables();

extern int do_prototypes;
#define CLNT_DEF	"_RPCGEN_CLNT"
#define SVC_DEF		"_RPCGEN_SVC"
