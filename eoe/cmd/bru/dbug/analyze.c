/*
 * Analyze the profile file (cmon.out) written out by the dbug
 * routines with profiling enabled.
 *
 * Copyright June 1987, Binayak Banerjee
 * All rights reserved.
 *
 * This program may be freely distributed under the same terms and
 * conditions as Fred Fish's Dbug package.
 *
 * Compile with -- cc -O -s -o %s analyze.c
 *
 * Analyze will read an trace file created by the dbug package
 * (when run with traceing enabled).  It will then produce a
 * summary on standard output listing the name of each traced
 * function, the number of times it was called, the percentage
 * of total calls, the time spent executing the function, the
 * proportion of the total time and the 'importance'.  The last
 * is a metric which is obtained by multiplying the proportions
 * of calls and the proportions of time for each function.  The
 * greater the importance, the more likely it is that a speedup
 * could be obtained by reducing the time taken by that function.
 *
 * Note that the timing values that you obtain are only rough
 * measures.  The overhead of the dbug package is included
 * within.  However, there is no need to link in special profiled
 * libraries and the like.
 *
 * CHANGES:
 *
 *	24-Jul-87: fnf
 *	Because I tend to use functions names like
 *	"ExternalFunctionDoingSomething", I've rearranged the
 *	printout to put the function name last in each line, so
 *	long names don't screw up the formatting unless they are
 *	*very* long and wrap around the screen width...
 *
 *	24-Jul-87: fnf
 *	Modified to put out table very similar to Unix profiler
 *	by default, but also puts out original verbose table
 *	if invoked with -v flag.
 */

# include <stdio.h>
# include "useful.h"

char *my_name;
int verbose;

/*
 * Structure of the stack.
 */

# define PRO_FILE	"dbugmon.out" /* Default output file name */
# define STACKSIZ	100	/* Maximum function nesting */
# define MAXPROCS	1000	/* Maximum number of function calls */

struct stack_t {
	unsigned int pos;		/* which function? */
	unsigned long time;		/* Time that this was entered */
	unsigned long children;		/* Time spent in called funcs */
}
	fn_stack[STACKSIZ+1];

unsigned int stacktop = 0;	/* Lowest stack position is a dummy */

/*
 * top() returns a pointer to the top item on the stack.
 * (was a function, now a macro)
 */

# define top()	&fn_stack[stacktop]

/*
 * Push - Push the given record on the stack.
 */

void
push(name_pos,time_entered)
register unsigned int name_pos;
register unsigned long time_entered;
{
	register struct stack_t *t;

	DBUG_ENTER("push");
	if (++stacktop > STACKSIZ)
	 {
		fprintf(DBUG_FILE,"%s: stack overflow (%s:%d)\n",
			my_name,__FILE__,__LINE__);
		exit(EX_SOFTWARE);
	 }

	DBUG_PRINT("push",("%d %ld",name_pos,time_entered));
	t = &fn_stack[stacktop];
	t->pos = name_pos;
	t->time = time_entered;
	t->children = 0;

	DBUG_VOID_RETURN;
}

/*
 * Pop - pop the top item off the stack, assigning the field values
 * to the arguments. Returns 0 on stack underflow, or on popping first
 * item off stack.
 */

unsigned int
pop(name_pos, time_entered, child_time)
register unsigned int *name_pos;
register unsigned long *time_entered;
register unsigned long *child_time;
{
	register struct stack_t *temp;

	DBUG_ENTER("pop");

	if (stacktop < 1)
		DBUG_RETURN(0);
	
	temp =  &fn_stack[stacktop];
	*name_pos = temp->pos;
	*time_entered = temp->time;
	*child_time = temp->children;

	DBUG_PRINT("pop",("%d %d %d",*name_pos,*time_entered,*child_time));
	DBUG_RETURN(stacktop--);
}

/*
 * We keep the function info in another array (serves as a simple
 * symbol table)
 */

struct module_t {
	char *name;
	unsigned long m_time;
	unsigned long m_calls;
}
	modules[MAXPROCS];

/*
 * We keep a binary search tree in order to look up function names
 * quickly (and sort them at the end.
 */

struct {
	unsigned int lchild;	/* Index of left subtree */
	unsigned int rchild;	/* Index of right subtree */
	unsigned int pos;	/* Index of module_name entry */
}
	s_table[MAXPROCS];

unsigned int n_items = 0;	/* No. of items in the array so far */

/*
 * Need a function to allocate space for a string and squirrel it away.
 */

char *
strsave(s)
char *s;
{
	register char *retval;
	register unsigned int len;
	extern char *malloc ();

	DBUG_ENTER("strsave");
	DBUG_PRINT("strsave",("%s",s));
	if (s == Nil(char) || (len = strlen(s)) == 0)
		DBUG_RETURN(Nil(char));

	MALLOC(retval, ++len, char);
	strcpy(retval,s);
	DBUG_RETURN(retval);
}

/*
 * add() - adds m_name to the table (if not already there), and returns
 * the index of its location in the table.  Checks s_table (which is a
 * binary search tree) to see whether or not it should be added.
 */

unsigned int
add(m_name)
char *m_name;
{
	register unsigned int ind = 0;
	register int cmp;

	DBUG_ENTER("add");
	if (n_items == 0)	/* First item to be added */
	 {
		s_table[0].pos = ind;
		s_table[0].lchild = s_table[0].rchild = MAXPROCS;
addit:
		modules[n_items].name = strsave(m_name);
		modules[n_items].m_time = modules[n_items].m_calls = 0;
		DBUG_RETURN(n_items++);
	 }
	while (cmp = strcmp(m_name,modules[ind].name)) {
		if (cmp < 0) {	/* In left subtree */
			if (s_table[ind].lchild == MAXPROCS) {
				/* Add as left child */
				if (n_items >= MAXPROCS) {
					fprintf(DBUG_FILE,
					 "%s: Too many functions being profiled\n",
						my_name);
					exit(EX_SOFTWARE);
				}
				s_table[n_items].pos =
					s_table[ind].lchild = n_items;
				s_table[n_items].lchild =
					s_table[n_items].rchild = MAXPROCS;
# ifdef notdef
				modules[n_items].name = strsave(m_name);
				modules[n_items].m_time = modules[n_items].m_calls = 0;
				DBUG_RETURN(n_items++);
# else
				goto addit;
# endif

			}
			ind = s_table[ind].lchild; /* else traverse l-tree */
		} else {
			if (s_table[ind].rchild == MAXPROCS) {
				/* Add as right child */
				if (n_items >= MAXPROCS) {
				     fprintf(DBUG_FILE,
				      "%s: Too many functions being profiled\n",
						my_name);
					exit(EX_SOFTWARE);
				}
				s_table[n_items].pos =
					s_table[ind].rchild = n_items;
				s_table[n_items].lchild =
					s_table[n_items].rchild = MAXPROCS;
# ifdef notdef
				modules[n_items].name = strsave(m_name);
				modules[n_items].m_time = modules[n_items].m_calls = 0;
				DBUG_RETURN(n_items++);
# else
				goto addit;
# endif

			}
			ind = s_table[ind].rchild; /* else traverse r-tree */
		}
	}
	DBUG_RETURN(ind);
}

unsigned long int tot_time = 0;
unsigned long int tot_calls = 0;

/*
 * process() - process the input file, filling in the modules table.
 */

void
process(inf)
FILE *inf;
{
	char buf[BUFSIZ];
	char fn_name[25];	/* Max length of fn_name */
	char fn_what[2];
	unsigned long fn_time;
	unsigned int pos;
	unsigned long time;
	unsigned int oldpos;
	unsigned long oldtime;
	unsigned long oldchild;
	struct stack_t *t;

	DBUG_ENTER("process");
	while (fgets(buf,BUFSIZ,inf) != NULL)
	 {
		sscanf(buf,"%24s %1s %ld", fn_name, fn_what, &fn_time);
		pos = add(fn_name);
		DBUG_PRINT("enter",("%s %s %d",fn_name,fn_what,fn_time));
		if (fn_what[0] == 'E')
			push(pos,fn_time);
		else {
			/*
			 * An exited function implies that all stacked
			 * functions are also exited, until the matching
			 * function is found on the stack.
			 */
			while( pop(&oldpos, &oldtime, &oldchild) ) {
				DBUG_PRINT("popped",("%d %d",oldtime,oldchild));
				time = fn_time - oldtime;
				t = top();
				t -> children += time;
				DBUG_PRINT("update",("%s",modules[t->pos].name));
				DBUG_PRINT("update",("%d",t->children));
				time -= oldchild;
				modules[oldpos].m_time += time;
				modules[oldpos].m_calls++;
				tot_time += time;
				tot_calls++;
				if (pos == oldpos)
					goto next_line;	/* Should be a break2 */
			}

			/*
			 * Assume that item seen started at time 0.
			 * (True for function main).  But initialize
			 * it so that it works the next time too.
			 */
			t = top();
			time = fn_time - t->time - t->children;
			t->time = fn_time; t->children = 0;
			modules[pos].m_time += time;
			modules[pos].m_calls++;
			tot_time += time;
			tot_calls++;
		}
		next_line:;
	 }

	/*
	 * Now, we've hit eof.  If we still have stuff stacked, then we
	 * assume that the user called exit, so give everything the exited
	 * time of fn_time.
	 */
	while (pop(&oldpos,&oldtime,&oldchild))
	 {
		time = fn_time - oldtime;
		t = top();
		t->children += time;
		time -= oldchild;
		modules[oldpos].m_time += time;
		modules[oldpos].m_calls++;
		tot_time += time;
		tot_calls++;
	 }

	DBUG_VOID_RETURN;
}

/*
 * out_header() -- print out the header of the report.
 */

void
out_header(outf)
FILE *outf;
{
	DBUG_ENTER("out_header");
	if (verbose) {
		fprintf(outf,"Profile of Execution\n");
		fprintf(outf,"Execution times are in milliseconds\n\n");
		fprintf(outf,"    Calls\t\t\t    Time\n");
		fprintf(outf,"    -----\t\t\t    ----\n");
		fprintf(outf,"Times\tPercentage\tTime Spent\tPercentage\n");
		fprintf(outf,"Called\tof total\tin Function\tof total    Importance\tFunction\n");
		fprintf(outf,"======\t==========\t===========\t==========  ==========\t========\t\n");
	} else {
		fprintf(outf,"   %%time     sec   #call ms/call  %%calls  weight  name\n");
	}
	DBUG_VOID_RETURN;
}

/*
 * out_trailer() - writes out the summary line of the report.
 */
void
out_trailer(outf,sum_calls,sum_time)
FILE *outf;
unsigned long int sum_calls, sum_time;
{
	DBUG_ENTER("out_trailer");
	if (verbose) {
		fprintf(outf,"======\t==========\t===========\t==========\t========\n");
		fprintf(outf,"%6d\t%10.2f\t%11d\t%10.2f\t\t%-15s\n",
			sum_calls,100.0,sum_time,100.0,"Totals");
	}
	DBUG_VOID_RETURN;
}

/*
 * out_item() - prints out the output line for a single entry,
 * and sets the calls and time fields appropriately.
 */

void
out_item(outf,m,called,timed)
FILE *outf;
register struct module_t *m;
unsigned long int *called, *timed;
{
	char *name = m->name;
	register unsigned int calls = m->m_calls;
	register unsigned long time = m->m_time;
	unsigned int import;
	double per_time = 0.0;
	double per_calls = 0.0; 
	double ms_per_call, ftime;

	DBUG_ENTER("out_item");

	if (tot_time > 0) {
		per_time = (double) (time * 100) / (double) tot_time;
	}
	if (tot_calls > 0) {
		per_calls = (double) (calls * 100) / (double) tot_calls;
	}
	import = (unsigned int) (per_time * per_calls);

	if (verbose) {
		fprintf(outf,"%6d\t%10.2f\t%11d\t%10.2f  %10d\t%-15s\n",
			calls, per_calls, time, per_time, import, name);
	} else {
		ms_per_call = time;
		ms_per_call /= calls;
		ftime = time;
		ftime /= 1000;
		fprintf(outf,"%8.2f%8.3f%8u%8.3f%8.2f%8u  %-s\n",
			per_time, ftime, calls, ms_per_call, per_calls, import, name);
	}
	*called = calls; *timed = time;
	DBUG_VOID_RETURN;
}

/*
 * out_body(outf,root,s_calls,s_time) -- Performs an inorder traversal
 * on the binary search tree (root).  Calls out_item to actually print
 * the item out.
 */

void
out_body(outf,root,s_calls,s_time)
FILE *outf;
register unsigned int root;
register unsigned long int *s_calls, *s_time;
{
	unsigned long int calls, time;

	DBUG_ENTER("out_body");
	DBUG_PRINT("out_body",("%d,%d",*s_calls,*s_time));
	if (root == MAXPROCS) {
		DBUG_PRINT("out_body",("%d,%d",*s_calls,*s_time));
		DBUG_VOID_RETURN;
	}

	while (root != MAXPROCS) {
		out_body(outf,s_table[root].lchild,s_calls,s_time);
		out_item(outf,&modules[s_table[root].pos],&calls,&time);
		DBUG_PRINT("out_body",("-- %d -- %d --", calls, time));
		*s_calls += calls;
		*s_time += time;
		root = s_table[root].rchild;
	}

	DBUG_PRINT("out_body",("%d,%d",*s_calls,*s_time));
	DBUG_VOID_RETURN;
}
/*
 * output() - print out a nice sorted output report on outf.
 */

void
output(outf)
FILE *outf;
{
	unsigned long int sum_calls = 0;
	unsigned long int sum_time = 0;

	DBUG_ENTER("output");
	if (n_items == 0)
	 {
		fprintf(outf,"%s: No functions to trace\n",my_name);
		exit(EX_DATAERR);
	 }
	out_header(outf);
	out_body(outf,0,&sum_calls,&sum_time);
	out_trailer(outf,sum_calls,sum_time);
	DBUG_VOID_RETURN;
}


# define usage() fprintf(DBUG_FILE,"Usage: %s [-v] [prof-file]\n",my_name)

main(argc, argv, environ)
int argc;
char *argv[], *environ[];
{
	extern int optind, getopt();
	extern char *optarg;
	register int c;
	int badflg = 0;
	FILE *infile;
	FILE *outfile = {stdout};

	DBUG_ENTER("main");
	DBUG_PROCESS(argv[0]);
	my_name = argv[0];

	while ((c = getopt(argc,argv,"#:v")) != EOF)
	 {
		switch (c)
		 {
			case '#': /* Debugging Macro enable */
				DBUG_PUSH(optarg);
				break;

			case 'v': /* Verbose mode */
				verbose++;
				break;

			default:
				badflg++;
				break;
		 }
	 }
	
	if (badflg)
	 {
		usage();
		DBUG_RETURN(EX_USAGE);
	 }

	if (optind < argc)
		FILEOPEN(infile,argv[optind],"r");
	else
		FILEOPEN(infile,PRO_FILE,"r");
	
	process(infile);
	output(outfile);
	DBUG_RETURN(EX_OK);
}
