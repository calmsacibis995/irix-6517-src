/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 2.14 $"

# include	"lboot.h"
# include	"mkboot.h"
# include	<ctype.h>
# include	<syslog.h>

/*
 *	check_master( master ) - do some basic sanity checking for the
 *				 master file entry; the following table
 *				 shows the valid flag combinations
 *
 *                R                               N
 *                E                   F   F       O               K
 *                Q           B       U   U       T   F           E
 *                A   T       L   C   N   N   S   A   S           R
 *                D   T   R   O   H   D   M   O   D   T   O       N
 *                D   Y   E   C   A   R   O   F   R   Y   L       E
 *                R   S   Q   K   R   V   D   T   V   P   D       L
 *               (a) (t) (r) (b) (c) (f) (m) (s) (x) (j) (O) ( ) (k)
 *              +---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  ONCE    (o) | # | # | # | # | # | # | O | O | O |   | # |   | N |
 *              +---+---+---+---+---+---+---+---+---+---+---+---+---+
 *  REQADDR (a)     | # | # | Y | Y | Y | Y | N | Y |   | # |   | N |
 *              ----+---+---+---+---+---+---+---+---+---+---+---+---+
 *  TTYS    (t)         | # | # | O | O | # | # | # |   | # |   | N |
 *              --------+---+---+---+---+---+---+---+---+---+---+---+
 *  REQ     (r)             | # | # | # | O | O | O |   | # |   | N |
 *              ------------+---+---+---+---+---+---+---+---+---+---+
 *  BLOCK   (b)                 | Y | Y | Y | Y | N |   | Y |   | N |
 *              ----------------+---+---+---+---+---+---+---+---+---+
 *  CHAR    (c)                     | N | Y | Y | N |   | Y |   | N |
 *              --------------------+---+---+---+---+---+---+---+---+
 *  FUNDRV  (f)                         | Y | Y | N |   | Y |   | N |
 *              ------------------------+---+---+---+---+---+---+---+
 *  FUNMOD  (m)                             | Y | N |   | Y |   | N |
 *              ----------------------------+---+---+---+---+---+---+
 *  SOFT    (s)                                 | N |   | # |   | N |
 *              --------------------------------+---+---+---+---+---+
 *  FSTYP   (j)                                         |   |   | N |
 *              ----------------------------------------+---+---+---+
 *               (a) (t) (r) (b) (c) (f) (m) (s) (x) (j) ( ) ( ) (k)
 *  
 *  
 *              O: the flag is valid only in this combination
 *              Y: the flag combination is valid
 *              N: the flag combination is not valid
 *              #: not applicable
 *
 * For example, BLOCK is valid with CHAR, FUNDRV, FUNMOD and SOFT, but not valid
 * with NOTADRV.  As another example, ONCE may only be used with FUNMOD,
 * SOFT or NOTADRV; all other flags are immaterial with respect to ONCE.
 *
 * If everything is OK, then return TRUE; otherwise return FALSE if the object
 * file should be skipped.
 */
boolean
check_master(char *name, register struct master *mp)
{
	int i;
	boolean skip = FALSE;

	if ( mp->flag & KOBJECT ) {
		/*
		 * Kernel is handled a little differently
		 */
		if ( mp->flag & ~KOBJECT ) {
			warn( "%s: illegal flags -- no flags allowed for kernel object files", name );
			mp->flag = KOBJECT;
			}

		for (i=0; i<mp->nsoft; i++) {
			if ( mp->prefix[0] || (mp->soft[i] && mp->soft[i] != DONTCARE) || mp->ndev ) {
				warn( "%s: , prefix, major and devices ignored", name );
				strncpy( mp->prefix, "", sizeof(mp->prefix) );
				mp->soft[i] = 0;
				mp->ndev = 0;
				}
		}

		if ( mp->ndep > 0 ) {
			warn( "%s: dependencies not applicable for kernel", name );
			mp->ndep = 0;
		}

		if ( mp->nrtn > 0 ) {
			warn( "%s: routine definitions not applicable for kernel", name );
			mp->nrtn = 0;
		}
		if ( mp->naliases > 0 ) {
			warn( "%s: alias definitions not applicable for kernel", name );
			mp->naliases = 0;
		}
	}

	if ( mp->flag & TTYS && ! (mp->flag & (CHAR|FUNDRV)) ) {
		warn( "%s: illegal flag combination -- TTYS valid only with CHAR|FUNDRV", name );
		skip = TRUE;
	}

	if ( (mp->flag & (BLOCK|CHAR|FUNDRV|FUNMOD|SOFT))
	     && (mp->flag & NOTADRV) ) {
		warn( "%s: illegal flag combination -- BLOCK|CHAR|FUNDRV|FUNMOD|SOFT mutually exclusive with NOTADRV", name );
		skip = TRUE;
	}

	if ( (mp->flag & OLD)
	     && !(mp->flag & (BLOCK|CHAR|FUNDRV|FUNMOD)) ) {
		warn( "%s: illegal flag combination -- OLD valid only for BLOCK|CHAR|FUNDRV|FUNMOD", name );
		skip = TRUE;
	}

	if ( (mp->flag & CHAR) && (mp->flag & FUNDRV) ) {
		warn( "%s: illegal flag combination -- CHAR mutually exclusive with FUNDRV", name );
		skip = TRUE;
	}

	if ( (mp->flag & REQADDR) && (mp->flag & SOFT) ) {
		warn( "%s: illegal flag combination -- REQADDR not valid with SOFT", name );
		skip = TRUE;
	}

	if ( (mp->flag & ONCE) && ! (mp->flag & (SOFT|NOTADRV|FUNMOD)) ) {
		warn( "%s: illegal flag combination -- ONCE valid only for SOFT|NOTADRV|FUNMOD", name );
		skip = TRUE;
	}

	if ( (mp->flag & REQ) && ! (mp->flag & (SOFT|NOTADRV|FUNMOD)) ) {
		warn( "%s: illegal flag combination -- REQ valid only for SOFT|NOTADRV|FUNMOD", name );
		skip = TRUE;
	}
	for (i=0; i<mp->nsoft; i++) 
		if ( ((unsigned)mp->soft[i] >= NMAJORS) && (mp->soft[i] != DONTCARE) ) {
			warn( "%s: major number cannot exceed $d", name, 
					NMAJORS - 1 );
			skip = TRUE;
		}

	if ( mp->flag & (NOTADRV|FUNMOD) && ( mp->ndev ) ) {
		warn( "%s: #DEV field not applicable for NOTADRV|FUNMOD", name);
		skip = TRUE;
	}
	if ( (mp->sema & (S_PROC|S_MAJOR)) &&  (mp->sema & S_NONE) ) {
		warn( "%s: only one driver semaphoring spec allowed", name );
		skip = TRUE;
	}
	if ( (mp->sema & (S_NONE|S_MAJOR)) &&  (mp->sema & S_PROC) ) {
		warn( "%s: only one driver semaphoring spec allowed", name );
		skip = TRUE;
	}

	return( skip? FALSE : TRUE );
}

/*
 *	lcase(str) - convert all upper case characters in 'str' to lower
 *			case and return pointer to start of string
 */

char *
lcase(char *str)
{
	register char	*ptr;

	for (ptr = str; *ptr != '\0'; ++ptr)
		if (isascii(*ptr) && isupper(*ptr))
			*ptr = tolower(*ptr);
	return( str );
}
 

/*
 *	basename(path) - obtain the file name associated with the (possibly)
 *			 full pathname argument 'path'
 */

char *
basename(char *path)
{
	char	*retval;

	/* return the entire name if no '/'s are found */
	retval = path;

	while (*path != NULL)
		if (*path++ == '/')
			retval = path;	/* now path points past the '/' */
		
	return( retval );
}

/*
 * Copy a string to the string table; handle all of the C language escape
 * sequences with the exception of \0
 */
char *
copystring(register char *text)
{
	register char c;
	char *start;
	int count, byte;

	if ( nstring-string > MAXSTRING-strlen(text)-1 )
		yyfatal( "string table overflow" );

	start = nstring--;

	while( *text ) {
		if ( (*++nstring = *text++) == '\\' ) {
			switch( c = *text++ )
			{
			default:
				*nstring = c;
				break;
			case 'n':
				*nstring = '\n';
				break;
			case 'v':
				*nstring = '\v';
				break;
			case 't':
				*nstring = '\t';
				break;
			case 'b':
				*nstring = '\b';
				break;
			case 'r':
				*nstring = '\r';
				break;
			case 'f':
				*nstring = '\f';
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				for( count=0,byte=c-'0'; ++count<3 && ((c=(*text))>='0'&&c<='7'); ++text )
					byte = 8 * byte + c - '0';
				if ( byte == 0 )
					yyfatal( "\\0 illegal within string" );
				if ( byte > 255 )
					yyfatal( "illegal character escape: \\%o", byte );
				*nstring = byte;
				break;
			}
		}
	}

	*++nstring = 0;
	++nstring;

	return( start );
}


/*
 * malloc() with error checking
 */
char *
mymalloc(size_t size)
{
	char *p = malloc(size);
	if (p == (char *) NULL) {
		fprintf(stderr, "no memory--malloc(3) failed\n");
		myexit(2);
	}
	return(p);
}

/*
 * calloc() with error checking
 */
char *
mycalloc(size_t size, size_t num)
{
	char *p = calloc(size,num);
	if (p == (char *) NULL) {
		fprintf(stderr, "no memory--calloc(3) failed\n");
		myexit(2);
	}
	return(p);
}

/*
 * myexit()
 * Issue any appropriate diags to user before exiting.
 */
void
myexit(int mystatus)
{
	if (mystatus != 0) {
		if (do_mreg_diags == 1) {
			error(ER120);
			syslog(LOG_WARNING, "Loadable module registration \
may not have been done (e.g. tpsc)");
			syslog(LOG_WARNING, "See mload(4) manual page for \
more information on auto registration");
		}
	}
	exit(mystatus);
}

/*
 * create a copy of the concatenation of 2 strings.
 * 	The original string should be freed by the caller.
 *	Either string may be null.
 */
char *
concat(char *prefix, char* suffix)
{
	char *str;
	int plen, slen;

	plen = (prefix!=0 ? (int)strlen(prefix) : 0);
	slen = (suffix!=0 ? (int)strlen(suffix) : 0);
	str = mymalloc(plen + 1 + slen);
	if (plen != 0)
		(void)strcpy(str, prefix);
	else
		str[0] = '\0';
	if (slen != 0)
		(void)strcat(str, suffix);

	return(str);
}

/*
 * concatenate and free
 */
char *
concat_free(char *prefix, char* suffix)
{
	char *s = prefix;
	prefix = concat(prefix, suffix);
	if (s)
		free(s);
	return prefix;
}


/* 
 * emit an external function declaration
 */
int
dcl_fnc(FILE *f, int j, char *nm)
{
	int len;

	if (strcmp("nodev", nm)
	    && strcmp("nulldev", nm)
	    && strcmp("fsstray", nm)) {
		len = (int)strlen(nm);
		j += len+4;
		if (j <= 72) {
			fprintf(f, ", %s()", nm);
		} else {
			fprintf(f, ";\nextern %s()", nm);
			j = len+8;
		}
	}

	return(j);
}
