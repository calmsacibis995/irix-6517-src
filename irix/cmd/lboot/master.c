/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

#ident "$Revision: 2.11 $"

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

# include	<stdio.h>
# include	<ctype.h>
# include	<fcntl.h>
# include	<errno.h>
# include	"lboot.h"
# include	"mkboot.h"

extern	FILE		*yyin;		/* LEX's input stream */
jmp_buf *jmpbuf;

struct master		master, *Opthdr, *copy_master(struct master *, int);
struct depend		depend[MAXDEP], *ndepend;
struct routine		routine[MAXRTN], *nroutine;
struct alias		alias[MAXALIASES], *nalias;
char			string[MAXSTRING], *nstring;
admin_t			admin[MAXADMIN], *nadmin;

char master_file[1024];	/* current master file entity */

extern char *master_dot_d;	/* master database directory */
char *Argv;
static int		build_header(void);

/*
 * Process a driver master file.
 */
struct master *
mkboot(char *module)
{
	extern int yylineno;
	int size;
	jmp_buf	env;

	Argv = module;

	master.flag = 0;
	master.nsoft = 0;

	/* allocate memory to build optional header for driver */
	Opthdr = (struct master *) mymalloc(	sizeof(master) +
						sizeof(depend) +
						sizeof(routine) +
						sizeof(alias) +
						sizeof(admin) + 
						sizeof(string) );
	ndepend = depend;
	nroutine = routine;
	nalias = alias;
	nstring = &string[1];	/* so Offset can never be NULL */
	nadmin = admin;
	
	/*
	 * scan /etc/master.d to find the configurable declarations
	 */
	strcpy( master_file, master_dot_d );
	strcat( master_file, "/" );
	strcat( master_file, module );


	if ((yyin = fopen(master_file,"r")) == NULL)
		{
		warn( "cannot open master file %s", master_file );
		free(Opthdr);
		return( (struct master *)0 );
		}
	
	jmpbuf = &env;
	if ( setjmp(env) ) {
		jmpbuf = NULL;
		(void)fclose(yyin);
		warn( "%s: not processed", module );
		return( (struct master *)0 );
	}
	
	/*
	 *	Start parsing master file from the beginning
	 */

	lexinit();

	yylineno = 1;

	if ( yyparse() == 0 && check_master(module,&master) == TRUE ) {
		/*
		 * build the new optional header
		 */
		jmpbuf = NULL;
		size = build_header();

		(void)fclose(yyin);
		return( copy_master( Opthdr, size ) );
	} else {
		jmpbuf = NULL;
		warn( "%s: not processed", module );
		(void)fclose(yyin);
		return( (struct master *)0 );
	}
}


/*
 * copy_driver( opthdr, size )
 *
 * Copy the parsed master file to a smaller area.
 * Free opthdr.
 */
struct master *
copy_master(struct master *opthdr, int size)
{
	struct master *Hdr;
	Hdr = (struct master *) mymalloc( size );
	memcpy( Hdr, opthdr, size );
	free( opthdr );
	return( Hdr );
}

/*
 * build_header()
 *
 * Build the new optional header; return the total size
 */

struct	sort {
	char	*string;	/* -> the string or expression */
	offset	*referenced;	/* -> the offset that references the string or expression */
};

static int
cmpstrlen(struct sort *s1, struct sort *s2)
{
	return((int) (strlen(s2->string) - strlen(s1->string)) );
}


static int
build_header(void)
{
	struct	depend	*dp;
	struct	routine	*rp;
	struct	alias	*al;
	admin_t		*ad;
	char		*xp, *p, *q, *s;
	int		size = 0;
	int		i, j, k;
	struct sort	*sorted;

	/*
	 * copy master structure information
	 */
	*Opthdr = master;

	/*
	 * copy dependencies
	 *
	 * dp points to the word boundary one past the end
	 * of the master struct pointed to by Opthdr
	 */
	dp = (struct depend *) ROUNDUP(Opthdr+1);

	if (Opthdr->ndep > 0) {
		Opthdr->o_depend = Offset(dp,Opthdr);

		for (i=0; i<Opthdr->ndep; i++) {
			dp[i] = depend[i];
		}
	}
	else
		Opthdr->o_depend = NULL;

	
	/*
	 * copy routine information
	 *
	 * rp points to the word boundary one past the end
	 * of the last dependency structure
	 */
	rp = (struct routine *) ROUNDUP(dp+(Opthdr->ndep));

	if (Opthdr->nrtn > 0) {
		Opthdr->o_routine = Offset(rp,Opthdr);

		/*                             routine name */
		for ( i=0; i<Opthdr->nrtn; i++ )
			rp[i] = routine[i];
	}
	else
		Opthdr->o_routine = NULL;

	/*
	 * SARAH: See comments in mkboot.y. We currently parse the alias
	 * strings from a master file, if they exist, and place the info
	 * in the master structure, but don't use this information yet.
	 */

	/*
	 * copy alias information
	 *
	 * al points to the word boundary one past the end
	 * of the last routine structure
	 */
	al = (struct alias *) ROUNDUP(rp+(Opthdr->nrtn));

	if (Opthdr->naliases > 0) {
		Opthdr->o_alias = Offset(al,Opthdr);

		/* alias string */
		for ( i=0; i<Opthdr->naliases; i++ ) {
			al[i] = alias[i];
		}
	}
	else
		Opthdr->o_alias = NULL;

	/*
	 * copy admin information
	 *
	 * al points to the word boundary one past the end
	 * of the last alias structure
	 */
	ad = (admin_t *) ROUNDUP(al+(Opthdr->naliases));

	if (Opthdr->nadmins > 0) {
		Opthdr->o_admin = Offset(ad,Opthdr);

		/* alias string */
		for ( i=0; i<Opthdr->nadmins; i++ ) {
			ad[i] = admin[i];
		}
	}
	else
		Opthdr->o_admin = NULL;

	/*
	 * xp points to memory beyond the last admin structure
	 */

	xp =(char *) ROUNDUP(ad+(Opthdr->nadmins));

	/*
	 * copy the dependency strings and build a string table;
	 * this is done in a way to minimize the size of the string table
	 *
	 *	1. extract dependency strings
	 *	2. sort into decreasing order by length
	 *	3. for each string, search the string table for an exact
	 *		match, and if found use that one, otherwise, copy
	 *		the unique string into the new string table. 
	 *
	 */
	
	if (Opthdr->ndep) {
	
		sorted = (struct sort*)mymalloc(Opthdr->ndep*sizeof(*sorted));

		i = 0;

		for( j=0; j<Opthdr->ndep; ++j ) {
			sorted[i].string = (char*)POINTER(dp[j].name,string);
			sorted[i++].referenced = &dp[j].name;
		}

		qsort( sorted, Opthdr->ndep, sizeof(*sorted), (int (*)())cmpstrlen );

		for( i=0; i<Opthdr->ndep; ++i ) {
			k = (int)strlen( s=sorted[i].string );

			/* search the string table for a duplicate */
			for( p=xp, q=xp+size-k; p < q; ++p ) {
				if ( 0 == strcmp(p,s) )
					break;
			}

			if ( p < q )
				/* its already in the new string table */
				*sorted[i].referenced = Offset(p,Opthdr);
			else {
				/* copy the unique string into the new string table */
				*sorted[i].referenced = Offset(strcpy(xp+size,s),Opthdr);
				size += k+1;
			}
		}

		free( sorted );
	}

	/*
	 * copy the routine strings and build a string table;
	 * cannot make the same minimizations as dependency string table,
	 * because dependency strings may be stubbed out "in situ".
	 */
	
	for( j=0; j<Opthdr->nrtn; ++j ) {
		k = (int)strlen(s=POINTER(rp[j].name,string));
		rp[j].name = Offset(strcpy(xp+size,s),Opthdr);
		size += k+1;
	}

	for( j=0; j<Opthdr->naliases; ++j ) {
		k = (int)strlen(s=POINTER(al[j].str,string));
		al[j].str = Offset(strcpy(xp+size,s),Opthdr);
		size += k+1;
	}

	/*
	 * copy the admin pair strings and build a string table.
	 */
	for( j=0; j<Opthdr->nadmins; ++j ) {
		k = (int)strlen(s=POINTER(ad[j].admin_name,string));
		ad[j].admin_name = Offset(strcpy(xp+size,s),Opthdr);
		size += k + 1;
		k = (int)strlen(s=POINTER(ad[j].admin_val,string));
		ad[j].admin_val = Offset(strcpy(xp+size,s),Opthdr);
		size += k+1;
	}

	return( (int)ROUNDUP(Offset(xp+size, Opthdr)) );
}
