/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)tsort:tsort.c	1.8"
/*	topological sort
 *	input is sequence of pairs of items (blank-free strings)
 *	nonidentical pair is a directed edge in graph
 *	identical pair merely indicates presence of node
 *	output is ordered list of items consistent with
 *	the partial ordering specified by the graph
*/
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

/*	the nodelist always has an empty element at the end to
 *	make it easy to grow in natural order
 *	states of the "live" field:*/
#define DEAD 0	/* already printed*/
#define LIVE 1	/* not yet printed*/
#define VISITED 2	/*used only in findloop()*/

static
struct nodelist {
	struct nodelist *nextnode;
	struct predlist *inedges;
	char *name;
	int live;
} firstnode = {NULL, NULL, NULL, DEAD};

/*	a predecessor list tells all the immediate
 *	predecessors of a given node
*/
struct predlist {
	struct predlist *nextpred;
	struct nodelist *pred;
};

static struct nodelist *index();
static struct nodelist *findloop();
static struct nodelist *mark();
static char *zmalloc();

static present(), anypred(); 
static void errmsg(), warn();

/*	the first for loop reads in the graph,
 *	the second prints out the ordering
*/
main(argc,argv)
char **argv;
{
	register struct predlist *t;
	FILE *input = stdin;
	register struct nodelist *i, *j;
	int x;
	char precedes[100], follows[100];

	switch( argc ) {
	case 1:
		break;
	case 2:
		if( !strcmp( argv[1], "-" ) )
			break;
		if( (input = fopen( argv[1], "r" )) == NULL )
			errmsg("cannot open for reading: ", argv[1]);
		break;
	case 3:
		if( strcmp( argv[1], "--" ) ) 
			errmsg( "usage: tsort [ file ]", "");
		if( !strcmp( argv[2], "-" ) )
			break;
		if( (input = fopen( argv[2], "r" )) == NULL )
			errmsg("cannot open for reading: ", argv[1]);
		break;
	default:
		errmsg( "usage: tsort [ file ]", "");
	}
	for(;;) {
		x = fscanf(input,"%99s%99s",precedes, follows);
		if(x==EOF)
			break;
		if(x!=2)
			errmsg ("odd data", "");
		i = index(precedes);
		j = index(follows);
		if(i==j||present(i,j)) 
			continue;
		t = (struct predlist *)zmalloc(sizeof(struct predlist));
		t->nextpred = j->inedges;
		t->pred = i;
		j->inedges = t;
	}
	for(;;) {
		x = 0;	/*anything LIVE on this sweep?*/
		for(i= &firstnode; i->nextnode!=NULL; i=i->nextnode) {
			if(i->live==LIVE) {
				x = 1;
				if(!anypred(i))
					break;
			}
		}
		if(x==0)
			break;
		if(i->nextnode==NULL)
			i = findloop();
		(void)puts(i->name);
		i->live = DEAD;
	}
	return 0;	/* Ensure zero return on normal termination */
}

/*	is i present on j's predecessor list?
*/
static
present(i,j)
struct nodelist *i, *j;
{
	register struct predlist *t;
	for(t=j->inedges; t!=NULL; t=t->nextpred)
		if(t->pred==i)
			return(1);
	return(0);
}

/*	is there any live predecessor for i?
*/
static
anypred(i)
struct nodelist *i;
{
	register struct predlist *t;
	for(t=i->inedges; t!=NULL; t=t->nextpred)
		if(t->pred->live==LIVE)
			return(1);
	return(0);
}

/*	turn a string into a node pointer
*/
static
struct nodelist *
index(s)
register char *s;
{
	register struct nodelist *i;
	register char *t;
	for(i= &firstnode; i->nextnode!=NULL; i=i->nextnode)
		if(!strcmp(s,i->name))
			return(i);
	for(t=s; *t; t++) ;
	t = zmalloc( (unsigned)(t+1-s));
	i->nextnode = (struct nodelist *)zmalloc( sizeof(struct nodelist));
	i->name = t;
	i->live = LIVE;
	i->nextnode->nextnode = NULL;
	i->nextnode->inedges = NULL;
	i->nextnode->live = DEAD;
	while(*t++ = *s++);
	return(i);
}


/*	given that there is a cycle, find some
 *	node in it
*/
static
struct nodelist *
findloop()
{
	register struct nodelist *i, *j;

	for(i= &firstnode; i->nextnode!=NULL; i=i->nextnode)
		if(i->live==LIVE)
			break;
	warn("warning: cycle in data", "");
	i = mark(i);
	if(i==NULL)
		errmsg("program error", "");
	for(j= &firstnode; j->nextnode!=NULL; j=j->nextnode)
		if(j->live==VISITED)
			j->live = LIVE;
	return(i);
}

/*	depth-first search of LIVE predecessors
 *	to find some element of a cycle;
 *	VISITED is a temporary state recording the
 *	visits of the search
*/
static
struct nodelist *
mark(i)
register struct nodelist *i;
{
	register struct nodelist *j;
	register struct predlist *t;

	if(i->live==DEAD)
		return(NULL);
	if(i->live==VISITED)
		return(i);
	i->live = VISITED;
	for(t=i->inedges; t!=NULL; t=t->nextpred) {
		j = mark(t->pred);
		if(j!=NULL) {
			warn("\t", i->name);
			return(j);
		}
	}
	return(NULL);
}

/*	print concatenated error message and exit
*/
static void errmsg(s,t)
char *s, *t;
{
	warn(s,t);
	exit(255);
}

/*	print a 'warning' to the user on standard error
*/
static void warn(s,t)
char *s,*t;
{
	(void)fprintf(stderr,"tsort: %s%s\n",s,t);
	(void)fflush(stderr);
}

/*	internal malloc routine that checks for errors
*/
static char *
zmalloc(n)
unsigned int	n;
{
	char	*p;
	char	tmp[60];

	if( (p = (char *)malloc(n)) == NULL ) {
		(void)sprintf(tmp, "error, malloc cannot allocate block of %d bytes", n);
		errmsg(tmp, "");
	}

	return  p;
}
