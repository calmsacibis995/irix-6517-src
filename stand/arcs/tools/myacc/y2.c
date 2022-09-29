/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)yacc:y2.c	1.10" */
#ident	"$Header: /proj/irix6.5.7m/isms/stand/arcs/tools/myacc/RCS/y2.c,v 1.1 1996/03/17 02:05:33 csm Exp $"
# include "dextern"
# define IDENTIFIER 257
# define MARK 258
# define TERM 259
# define LEFT 260
# define RIGHT 261
# define BINARY 262
# define PREC 263
# define LCURLY 264
# define C_IDENTIFIER 265  /* name followed by colon */
# define NUMBER 266
# define START 267
# define TYPEDEF 268
# define TYPENAME 269
# define UNION 270
# define ENDFILE 0
#define LHS_TEXT_LEN		80	/* length of lhstext */
#define RHS_TEXT_LEN		640	/* length of rhstext */
	/* communication variables between various I/O routines */

#ifdef sgi		/* this is a hack so i don't need sgs.h */
#define ESG_PKG "Enhanced Programming Utilities "
#define ESG_REL "(EPU) 5.1 04/26/91"
#endif

static char *infile;			/* input file name 		*/
static int numbval;			/* value of an input number 	*/
static int toksize = NAMESIZE;
static char *tokname;	/* input token name		*/

char modname[80];		/* name of module */
char *modpref=NULL;		/* module name prefixed by underscore */

char *fname_parser;		/* location of common parser 	*/
char *fname_temp;		/* yacc output filenames */
char *fname_acts;
char *fname_debug;
char *fname_out;
char *fname_userout;
char *fname_defs;

static void finact();
static char *cstash();
static void defout();
static void cpyunion();
static void cpycode();
static void cpyact();
static void lhsfill();
static void rhsfill();
static void lrprnt();
static void beg_debug();
static void end_toks();
static void end_debug();
static void exp_tokname();
static void exp_prod();
static void exp_ntok();
static void exp_nonterm();
static defin();
static gettok();
static chfind();
static skipcom();

	/* storage of names */

static char cnamesblk0[CNAMSZ];	/* initial block to place token and 	*/
				/* nonterminal names are stored 	*/
static char *cnames = cnamesblk0;	/* points to initial block - more space	*/
				/* is allocated as needed.		*/
static char *cnamp = cnamesblk0;	/* place where next name is to be put in */
static int ndefout = 3;			/* number of defined symbols output */

	/* storage of types */
static int defunion = 0;	/* union of types defined? */
static int ntypes = 0;		/* number of types defined */
static char * typeset[NTYPES];	/* pointers to type tags */

	/* symbol tables for tokens and nonterminals */

int ntokens = 0;
int ntoksz = NTERMS;
TOKSYMB *tokset;
int *toklev;

int nnonter = -1;
NTSYMB *nontrst;
int nnontersz = NNONTERM;

#ifdef mips
int start_sym;	/* start symbol */
				/* The symbol start collides with the c startup routine */
#else
static int start;	/* start symbol */
#endif

	/* assigned token type values */
static int extval = 0;

	/* input and output file descriptors */

FILE * finput;		/* yacc input file */
FILE * faction;		/* file for saving actions */
FILE * fdefine;		/* file for # defines */
FILE * ftable;		/* C parser output file */
FILE * ftemp;		/* tempfile to pass 2 */
FILE * fdebug;		/* where the strings for debugging are stored */
FILE * foutput;		/* y.output file */

	/* output string */

static char *lhstext;
static char *rhstext;

	/* storage for grammar rules */

int *mem0; /* production storage */
int *mem;
int *tracemem;
extern int *optimmem;
int new_memsize = MEMSIZE;
int nprod= 1;	/* number of productions */
int nprodsz = NPROD;

int **prdptr;
int *levprd;
char *had_act;

static int gen_lines = 1;	/* flag for generating the # line's default is yes */
static int gen_testing = 0;	/* flag for whether to include runtime debugging */
static char *v_stmp = "n";	/* flag for version stamping--default turned off */

extern const char badopen[], badopenid[];
int is_y_plus_plus=0;

char *sconcat(char *s1, char *s2)
{
	char *s;
	s = malloc(strlen(s1) + strlen(s2) + 1);
	strcpy(s, s1);
	strcat(s, s2);
	return s;
}

void setup(argc,argv) int argc; char *argv[];
{	int ii,i,j,lev,t, ty; /*ty is the sequencial number of token name in tokset*/
	int c;
	int *p;
	char *cp;
	char actname[8];
	int userout;
	static const char illtok[] = "Missing tokens or illegal tokens";
	static const char illtokid[] = ":33";

	extern char	*optarg;
	extern int	optind;
#ifndef __STDC__
	extern int	getopt();
#endif

	foutput = NULL;
	i = 1;

	tokname = (char *) malloc(sizeof(char) * toksize);
	tokset = (TOKSYMB *)malloc(sizeof(TOKSYMB) * ntoksz);
	toklev = (int *) malloc(sizeof(int) * ntoksz);
	nontrst = (NTSYMB *) malloc (sizeof(NTSYMB) * nnontersz);
	mem0 = (int *)malloc(sizeof(int) * new_memsize);
	prdptr = (int **) malloc(sizeof(int *) * (nprodsz+2));
	levprd = (int *) malloc(sizeof(int) * (nprodsz+2));
	had_act = (char *) calloc((nprodsz+2), sizeof(char));
	lhstext = (char *) malloc(sizeof(char) * LHS_TEXT_LEN);
	rhstext = (char *) malloc(sizeof(char) * RHS_TEXT_LEN);
	aryfil(toklev,ntoksz,0);
	aryfil(levprd,nprodsz,0);
	for( ii =0; ii<ntoksz; ++ii) tokset[ii].value = 0;
	for( ii =0; ii<nnontersz; ++ii) nontrst[ii].tvalue = 0;
	aryfil(mem0, new_memsize,0);
	mem= mem0;
	tracemem = mem0;

	userout = 0;

	while ((c = getopt(argc, argv, "vVltp:Q:m:")) != EOF)
		switch (c) {
			case 'v':
				userout = 1;
				break;
			case 'V':
				(void) pfmt(stderr, MM_INFO, ":34:%s, %s\n", ESG_PKG, ESG_REL);
				break;
			case 'Q':
				v_stmp = optarg;
				if (*v_stmp != 'y' && *v_stmp != 'n')
					error(":35", "yacc: -Q should be followed by [y/n]");
				break;
			case 'l':
				gen_lines = 0;	/* don't gen #lines */
				break;
			case 't':
				gen_testing = 1;	/* set YYDEBUG on */
				break;
			case 'p':
				fname_parser = optarg;
				break;
			case '?':
			default:
				(void) pfmt(stderr,MM_ACTION,
					    ":36:Usage: yacc [-vVdlt] [-Q(y/n)] [-p driver_file] file\n");
				exit(1);
		}

	if (fname_parser == NULL) {
		(void) fprintf(stderr, "yacc: must specify parser via -p option\n");
		exit(1);
	}

	if (optind == argc) {
		(void) fprintf(stderr, "yacc: no input file specified\n");
		exit(1);
	}

	infile = argv[optind];

	if ((cp = strrchr(infile, '/')) != 0)
		cp++;
	else
		cp = infile;

	strcpy(modname, cp);

	if ((cp = strrchr(modname, '.')) != 0)
		*cp = 0;

	modpref = sconcat(modname, "_");

	/*
	 * Make up yacc output file names
	 */

	fname_temp = sconcat(modpref, "yacc.tmp");
	fname_acts = sconcat(modpref, "yacc.acts");
	fname_debug = sconcat(modpref, "yacc.debug");
	fname_out = sconcat(modpref, "y.tab.c");
	fname_userout = sconcat(modpref, "y.output");
	fname_defs = sconcat(modpref, "y.tab.h");

	if (userout) {
		foutput = fopen(fname_userout, "w" );
		if( foutput == NULL ) error(badopenid,
					    badopen, fname_userout,
					    strerror(errno));
	}

	fdefine = fopen( fname_defs, "w" );
	if ( fdefine == NULL )
		error(badopenid, badopen, fname_defs,
		      strerror(errno));

	fprintf(fdefine, "#ifndef %sy_tab_h\n", modpref);
	fprintf(fdefine, "#define %sy_tab_h\n", modpref);

	fdebug = fopen( fname_debug, "w" );
	if ( fdebug == NULL )
		error(badopenid, badopen, fname_debug, strerror(errno));
	ftable = fopen( fname_out, "w" );
	if ( ftable == NULL )
		error(badopenid, badopen, fname_out, strerror(errno));

	ftemp = fopen( fname_temp, "w" );
	if ( ftemp == NULL )
		error(badopenid, badopen, fname_temp, strerror(errno));
	faction = fopen( fname_acts, "w" );
	if ( faction == NULL )
		error(badopenid, badopen, fname_acts, strerror(errno));

	if ((finput=fopen( infile, "r" )) == NULL ) {
		if (infile == NULL)
			infile = "";
		error(badopenid, badopen, infile, strerror(errno));
	}

	/* if the input file has suffix .y++ or .yxx, then turn on
	   the is_y_plus_plus flag so we can skip // style
	   comments.
	*/

	if ((cp=strrchr(infile,'.'))!=NULL)
	 {
	 if (strcmp(cp,".y++")==0 || strcmp(cp,".yxx")==0)
		is_y_plus_plus=1;
	 }

	lineno =1;
	cnamp = cnames;
	(void) defin(0,"$end");
	extval = 0400;
	(void) defin(0,"error");
	(void) defin(1,"$accept");
	mem=mem0;
	lev = 0;
	ty = 0;
	i=0;
	beg_debug();	/* initialize fdebug file */

	/* sorry -- no yacc parser here.....
		we must bootstrap somehow... */

	t=gettok();
	if ( *v_stmp == 'y' )
		(void) fprintf(ftable, "#ident\t\"yacc: %s\"\n", ESG_REL);
	for( ; t!=MARK && t!= ENDFILE; ){
		int tok_in_line;
		switch( t ){

		case ';':
			t = gettok();
			break;

		case START:
			if( (t=gettok()) != IDENTIFIER ){
				error(":37", "Bad %%start construction" );
				}
#ifdef mips
			start_sym = chfind(1,tokname);
#else
			start = chfind(1,tokname);
#endif
			t = gettok();
			continue;

		case TYPEDEF:
			tok_in_line = 0;
			if( (t=gettok()) != TYPENAME ) error(":38", "bad syntax in %%type" );
			ty = numbval;
			for(;;){
				t = gettok();
				switch( t ){

				case IDENTIFIER:
					tok_in_line=1;
#ifndef sgi
					if (tokname[0] == ' ')	/* LITERAL */
						break;
					else
#endif
					if( (t=chfind( 1, tokname ) ) < NTBASE ) {
						j = TYPE( toklev[t] );
						if( j!= 0 && j != ty ){
							error(":39", "Type redeclaration of token %s",
								tokset[t].name );
							}
						else SETTYPE( toklev[t],ty);
					} else {
						j = nontrst[t-NTBASE].tvalue;
						if( j != 0 && j != ty ){
							error(":40", "Type redeclaration of nonterminal %s",
								nontrst[t-NTBASE].name );
							}
						else nontrst[t-NTBASE].tvalue = ty;
						}
					/* FALLTHRU */
				case ',':
					continue;

				case ';':
					t = gettok();
					break;
				default:
					break;
					}
				if (!tok_in_line)
					error(illtokid, illtok);
				break;
				}
			continue;

		case UNION:
			/* copy the union declaration to the output */
			cpyunion();
			defunion = 1;
			t = gettok();
			continue;

		case LEFT:
		case BINARY:
		case RIGHT:
			i++;
			/* FALLTHRU */
		case TERM:
			tok_in_line = 0;
			lev = (t-TERM) | 04;  /* nonzero means new prec. and assoc. */
			ty = 0;

			/* get identifiers so defined */

			t = gettok();
			if( t == TYPENAME ){ /* there is a type defined */
				ty = numbval;
				t = gettok();
				}

			for(;;) {
				switch( t ){

				case ',':
					t = gettok();
					continue;

				case ';':
					break;

				case IDENTIFIER:
					tok_in_line=1;
					j = chfind(0,tokname);
					if( lev & ~04 ){
						if( ASSOC(toklev[j])&~04 ) error(":41", "Redeclaration of precedence of %s", tokname );
						SETASC(toklev[j],lev);
						SETPLEV(toklev[j],i);
					} else {
						if( ASSOC(toklev[j]) ) (void)pfmt(stderr, MM_WARNING, ":42:Redeclaration of precedence of %s, line %d\n", tokname, lineno );
						SETASC(toklev[j],lev);
						}
					if( ty ){
						if( TYPE(toklev[j]) ) error(":43", "Redeclaration of type of %s", tokname );
						SETTYPE(toklev[j],ty);
						}
					if( (t=gettok()) == NUMBER ){
						tokset[j].value = numbval;
						if( j < ndefout && j>2 ){
							error(":44", "Type number of %s should be defined earlier",
								tokset[j].name );
							}
						if (numbval >= -YYFLAG1) {
							error(":45", "Token numbers must be less than %d", -YYFLAG1);
							}
						t=gettok();
						}
					continue;

					}
				if ( !tok_in_line )
					error(illtokid, illtok);
				break;
				}

			continue;

		case LCURLY:
			defout();
			cpycode();
			t = gettok();
			continue;

		default:
			error("uxlibc:81", "Syntax error" );

			}

		}

	if( t == ENDFILE ){
		error(":46", "Unexpected EOF before %%%%" );
		}

	/* t is MARK */

	defout();
	end_toks();	/* all tokens dumped - get ready for reductions */

	(void) fprintf( ftable, "\n#include \"%s\"\n\n", fname_defs );

	(void) fprintf( ftable, "#if %sYYMAXDEPTH <= 0\n", modpref );
	(void) fprintf( ftable, "#    include <malloc.h>\n" );
	(void) fprintf( ftable, "#    include <memory.h>\n" );
#ifdef sgi
	(void) fprintf( ftable, "#    include <unistd.h>\n" );
#endif
	(void) fprintf( ftable, "#    include <values.h>\n" );
	(void) fprintf( ftable, "#endif\n\n" );
	(void) fprintf( ftable, "#ifndef %syyerror\n", modpref);
	(void) fprintf( ftable, "#    if defined(__cplusplus) || defined(__STDC__)\n");
	(void) fprintf( ftable, "	extern void %syyerror(const char *, "
		       "void *lex_data, void *user_data);\n", modpref );
	(void) fprintf( ftable, "#    endif\n");
	(void) fprintf( ftable, "#endif\n\n");
	(void) fprintf( ftable, "#ifndef %syylex\n", modpref);
	(void) fprintf( ftable, "#    ifdef __cplusplus\n");
	(void) fprintf( ftable, "	extern \"C\" int %syylex(%sYYSTYPE *yylval, "
		       "void *lex_data);\n", modpref, modpref );
	(void) fprintf( ftable, "#    endif\n");
	(void) fprintf( ftable, "#    ifdef __STDC__\n");
	(void) fprintf( ftable, "	extern int %syylex(%sYYSTYPE *yylval, "
		       "void *lex_data);\n", modpref, modpref );
	(void) fprintf( ftable, "#    endif\n");
	(void) fprintf( ftable, "#endif\n\n");
	(void) fprintf( ftable, "#define %syyclearin Y.yychar = -1\n", modpref );
	(void) fprintf( ftable, "#define %syyerrok Y.yyerrflag = 0\n", modpref );
	if (!(defunion || ntypes)) {
		(void) fprintf(fdefine,"#ifndef %sYYSTYPE\n", modpref);
		(void) fprintf(fdefine, "#    define %sYYSTYPE int\n", modpref);
		(void) fprintf(fdefine, "#endif\n");
	}
	(void) fprintf( ftable, "\ntypedef int %syytabelem;\n\n", modpref);
	(void) fprintf( fdefine, "#ifndef %sYYMAXDEPTH\n", modpref);
	(void) fprintf( fdefine, "#    define %sYYMAXDEPTH 150\n", modpref);
	(void) fprintf( fdefine, "#endif\n\n");
	(void) fprintf( fdefine, "typedef struct %syystate_s {\n", modpref);
	(void) fprintf( fdefine, "	int yyerrflag;\n");
	(void) fprintf( fdefine, "	int yydebug;\n");
	(void) fprintf( fdefine, "	%sYYSTYPE yylval;\n", modpref);
	(void) fprintf( fdefine, "	%sYYSTYPE yyval;\n", modpref);
	(void) fprintf( fdefine, "	int yymaxdepth;\n");
	(void) fprintf( fdefine, "#if %sYYMAXDEPTH > 0\n", modpref );
	(void) fprintf( fdefine, "	int yy_yys[%sYYMAXDEPTH], *yys;\n", modpref );
	(void) fprintf( fdefine, "	%sYYSTYPE yy_yyv[%sYYMAXDEPTH], *yyv;\n", modpref, modpref );
	(void) fprintf( fdefine, "#else	/* user does initial allocation */\n" );
	(void) fprintf( fdefine, "	int *yys;\n");
	(void) fprintf( fdefine, "	%sYYSTYPE *yyv;\n", modpref );
	(void) fprintf( fdefine, "#endif\n" );
	(void) fprintf( fdefine, "	%sYYSTYPE *yypv;	/* top of value stack */\n", modpref);
	(void) fprintf( fdefine, "	int *yyps;		/* top of state stack */\n");
	(void) fprintf( fdefine, "	int yystate;		/* current state */\n");
	(void) fprintf( fdefine, "	int yytmp;		/* extra var (lasts between blocks) */\n");
	(void) fprintf( fdefine, "	int yynerrs;		/* number of errors */\n");
	(void) fprintf( fdefine, "	int yynerrflag;		/* error recovery flag */\n");
	(void) fprintf( fdefine, "	int yychar;		/* current input token number */\n");
	(void) fprintf( fdefine, "} %syystate_t;\n\n", modpref);

	(void) fprintf(fdefine,
		       "extern int %syyparse(void *lex_data, "
		       "void *user_data, int debug);\n", modpref );

	prdptr[0]=mem;
	/* added production */
	*mem++ = NTBASE;
#ifdef mips
	*mem++ = start_sym;  /* if start is 0, we will overwrite with the lhs of the first rule */
#else
	*mem++ = start;	 /* if start is 0, we will overwrite with the lhs of the first rule */
#endif
	*mem++ = 1;
	*mem++ = 0;
	prdptr[1]=mem;

	while( (t=gettok()) == LCURLY ) cpycode();

	if( t != C_IDENTIFIER ) error(":47", "Bad syntax on first rule" );

#ifdef mips
	if( !start_sym ) prdptr[0][1] = chfind(1,tokname);
#else
	if( !start ) prdptr[0][1] = chfind(1,tokname);
#endif

	/* read rules */

	while( t!=MARK && t!=ENDFILE ){


		/* process a rule */

		if( t == '|' ){
			rhsfill( (char *) 0 );	/* restart fill of rhs */
			*mem = *prdptr[nprod-1];
			if ( ++mem >= &tracemem[new_memsize] ) exp_mem(1);
			}
		else if( t == C_IDENTIFIER ){
			*mem = chfind(1,tokname);
			if( *mem < NTBASE )error(":48", "Illegal nonterminal in grammar rule");
			if ( ++mem >= &tracemem[new_memsize] ) exp_mem(1);
			lhsfill( tokname );	/* new rule: restart strings */
			}
		else error(":49", "Illegal rule: missing semicolon or | ?" );

		/* read rule body */


		t = gettok();
	more_rule:
		while( t == IDENTIFIER ) {
			*mem = chfind(1,tokname);
			if( *mem<NTBASE ) levprd[nprod] = toklev[*mem]& ~04;
			if ( ++mem >= &tracemem[new_memsize] ) exp_mem(1);
			rhsfill( tokname );	/* add to rhs string */
			t = gettok();
			}

		if( t == PREC ){
			if( gettok()!=IDENTIFIER) error(":50", "Illegal %%prec syntax" );
			j = chfind(2,tokname);
			if( j>=NTBASE)error(":51", "Nonterminal %s illegal after %%prec", nontrst[j-NTBASE].name);
			levprd[nprod]=toklev[j]& ~04;
			t = gettok();
			}

		if( t == '=' ){
			had_act[nprod] = 1;
			levprd[nprod] |= ACTFLAG;
			(void) fprintf( faction, "\ncase %d:", nprod );
			cpyact( mem-prdptr[nprod]-1 );
			(void) fprintf( faction, " break;" );
			if( (t=gettok()) == IDENTIFIER ){
				/* action within rule... */

				lrprnt();		/* dump lhs, rhs */
				(void) sprintf( actname, "$$%d", nprod );
				j = chfind(1,actname);	/* make it a nonterminal */

				/* the current rule will become rule number nprod+1 */
				/* move the contents down, and make room for the null */

				for( p=mem; p>=prdptr[nprod]; --p ) p[2] = *p;
				mem += 2;
				if ( mem >= &tracemem[new_memsize] ) exp_mem(1);

				/* enter null production for action */

				p = prdptr[nprod];

				*p++ = j;
				*p++ = -nprod;

				/* update the production information */

				levprd[nprod+1] = levprd[nprod] & ~ACTFLAG;
				levprd[nprod] = ACTFLAG;

				if( ++nprod >= nprodsz)
					exp_prod();
				prdptr[nprod] = p;

				/* make the action appear in the original rule */
				*mem++ = j;
				if ( mem >= &tracemem[new_memsize] ) exp_mem(1);

				/* get some more of the rule */

				goto more_rule;
				}

			}

		while ( t == ';' ) t=gettok();

		*mem++ = -nprod;
		if ( mem >= &tracemem[new_memsize] ) exp_mem(1);

		/* check that default action is reasonable */

		if( ntypes && !(levprd[nprod]&ACTFLAG) && nontrst[*prdptr[nprod]-NTBASE].tvalue ){
			/* no explicit action, LHS has value */
			register tempty;
			tempty = prdptr[nprod][1];
			if( tempty < 0 ) error(":52", "Must return a value, since LHS has a type" );
			else if( tempty >= NTBASE ) tempty = nontrst[tempty-NTBASE].tvalue;
			else tempty = TYPE( toklev[tempty] );
			if( tempty != nontrst[*prdptr[nprod]-NTBASE].tvalue ){
				error(":53", "Default action causes potential type clash" );
				}
			}

		if (++nprod >= nprodsz)
			exp_prod();
		prdptr[nprod] = mem;
		levprd[nprod]=0;

		}

	/* end of all rules */

	end_debug();		/* finish fdebug file's input */
	finact();
	if( t == MARK ){
		if ( gen_lines )
			(void) fprintf( ftable, "\n# line %d \"%s\"\n",
				lineno, infile );
		while( (c=getc(finput)) != EOF ) (void) putc( (char)c, ftable );
		}
	(void) fclose( finput );

	fprintf(fdefine, "#endif /* %sy_tab_h */\n", modpref);
	}

static void finact(){
	/* finish action routine */

	(void) fclose(faction);

	(void) fprintf( ftable, "# define %sYYERRCODE %d\n", modpref, tokset[2].value );

	}

static char *
cstash( s ) register char *s; {
	char *temp;
	static int used = 0;
	static int used_save = 0;
	static int exp_cname = CNAMSZ;
	int len;

	if (s != NULL)
		len = strlen(s);
	else
		len = 0;

	/* 2/29/88 -
	** Don't need to expand the table, just allocate new space.
	*/
	used_save = used;
	while (len >= (exp_cname - used_save)) {
		exp_cname += CNAMSZ;
		if (!used) free((char *)cnames);
		if ((cnames = (char *) malloc(sizeof(char)*exp_cname)) == NULL)
			error(":54", "Not enough memory to expand string dump");
		cnamp = cnames;
		used = 0;
	}

	temp = cnamp;
	do {
		*cnamp++ = *s;
	} while ( *s++ );
	used += cnamp - temp;
	return( temp );
	}

static
defin( t, s ) register char  *s; {
	/* define s to be a terminal if t=0 or a nonterminal if t=1 */

	register val;
	static const char invalesc[] = "Invalid escape";
	static const char invalescid[] = ":55";

	if (t) {
		if (++nnonter >= nnontersz)
			exp_nonterm();
		nontrst[nnonter].name = cstash(s);
		return( NTBASE + nnonter );
		}
	/* must be a token */
	if( ++ntokens >= ntoksz )
		exp_ntok();
	tokset[ntokens].name = cstash(s);

	/* establish value for token */

	if( s[0]==' ' && s[2]=='\0' ) /* single character literal */
		val = s[1];
	else if ( s[0]==' ' && s[1]=='\\' ) { /* escape sequence */
		if( s[3] == '\0' ){ /* single character escape sequence */
			switch ( s[2] ){
					 /* character which is escaped */
			case 'a': (void)pfmt(stderr,MM_WARNING, ":56:\\a is ANSI C \"alert\" character, line %d\n", lineno);
				  val = '\a'; break;
			case 'v': val = '\v'; break;
			case 'n': val = '\n'; break;
			case 'r': val = '\r'; break;
			case 'b': val = '\b'; break;
			case 't': val = '\t'; break;
			case 'f': val = '\f'; break;
			case '\'': val = '\''; break;
			case '"': val = '"'; break;
			case '?': val = '?'; break;
			case '\\': val = '\\'; break;
			default: error(invalescid, invalesc);
			}
		} else if( s[2] <= '7' && s[2]>='0' ){ /* \nnn sequence */
			int i = 3;
			val = s[2] - '0';
			while(isdigit(s[i]) && i<=4) {
				if(s[i]>='0' && s[i]<='7') val = val * 8 + s[i]-'0';
				else error(":57", "Illegal octal number");
				i++;
			}
			if( s[i] != '\0' ) error(":58", "Illegal \\nnn construction" );
			if( val > 255 ) error(":59", " \\nnn exceed \\377");
			if( val == 0 && i >= 4) error(":60", "'\\000' is illegal" );
		} else if( s[2] == 'x' ) { /* hexadecimal \xnnn sequence */
			int i = 3;
			val = 0;
			(void)pfmt(stderr,MM_WARNING, ":61:\\x is ANSI C hex escape, line %d\n", lineno);
			if (isxdigit(s[i]))
				while (isxdigit(s[i])) {
					int tmpval;
					if(isdigit(s[i]))
						tmpval = s[i] - '0';
					else if ( s[i] >= 'a' )
						tmpval = s[i] - 'a' + 10;
					else
						tmpval = s[i] - 'A' + 10;
					val = 16 * val + tmpval;
					i++;
				}
			else error(":62", "Illegal hexadecimal number" );
			if( s[i] != '\0' ) error(":63", "Illegal \\xnn construction" );
			if( val > 255 ) error(":64", " \\xnn exceed \\xff");
			if( val == 0 ) error(":65", "'\\x00' is illegal");
		} else error(invalescid, invalesc);
	} else {
		val = extval++;
		}
	tokset[ntokens].value = val;
	toklev[ntokens] = 0;
	return( ntokens );
	}

static void
defout(){ /* write out the defines (at the end of the declaration section) */

	register int i, c;
	register char *cp;

	for( i=ndefout; i<=ntokens; ++i ){

		cp = tokset[i].name;
		if( *cp == ' ' )	/* literals */
		{
			(void) fprintf( fdebug, "\t\"%s%s\",\t%d,\n",
			       modpref, tokset[i].name + 1, tokset[i].value );
			continue;	/* was cp++ */
		}

		for( ; (c= *cp)!='\0'; ++cp ){

			if(islower(c)|| isupper(c)|| isdigit(c)|| c=='_')/* EMPTY */;
			else goto nodef;
			}

		(void) fprintf( fdebug, "\t\"%s%s\",\t%d,\n",
			       modpref, tokset[i].name, tokset[i].value );

		(void) fprintf( fdefine, "# define %s%s %d\n",
			       modpref, tokset[i].name, tokset[i].value );

	nodef:	;
		}

	ndefout = ntokens+1;

	(void) fprintf( fdefine, "\n");
	}

static
gettok() {
	register i, base;
	static int peekline; /* number of '\n' seen in lookahead */
	register c, match, reserve;

begin:
	reserve = 0;
	lineno += peekline;
	peekline = 0;
	c = getc(finput);
	while( c==' ' || c=='\n' || c=='\t' || c=='\f' ){
		if( c == '\n' ) ++lineno;
		c=getc(finput);
	}
	if( c == '/' ){ /* skip comment */
		lineno += skipcom();
		goto begin;
	}

	switch(c){

	case EOF:
		return(ENDFILE);
	case '{':
		(void) ungetc( c, finput );
		return( '=' );	/* action ... */
	case '<':  /* get, and look up, a type name (union member name) */
		i = 0;
		while( (c=getc(finput)) != '>' && c!=EOF && c!= '\n' ){
			tokname[i] = (char)c;
			if( ++i >= toksize )
				exp_tokname();
			}
		if( c != '>' ) error(":66", "Unterminated < ... > clause" );
		tokname[i] = '\0';
		if( i == 0) error(":67", "Missing type name in < ... > clause");
		for( i=1; i<=ntypes; ++i ){
			if( !strcmp( typeset[i], tokname ) ){
				numbval = i;
				return( TYPENAME );
				}
			}
		if ( (ntypes+1) >= NTYPES) {
			error(":67", "Maximum of %d types allowed",NTYPES);
		}
		typeset[numbval = ++ntypes] = cstash( tokname );
		return( TYPENAME );

	case '"':
	case '\'':
		match = c;
		tokname[0] = ' ';
		i = 1;
		for(;;){
			c = getc(finput);
			if( c == '\n' || c == EOF )
				error(":68", "Illegal or missing ' or \"" );
			if( c == '\\' ){
				c = getc(finput);
				tokname[i] = '\\';
				if( ++i >= toksize )
					exp_tokname();
				}
			else if( c == match ) break;
			tokname[i] = (char)c;
			if( ++i >= toksize )
				exp_tokname();
			}
		break;

	case '%':
	case '\\':

		switch(c=getc(finput)) {

		case '0':	return(TERM);
		case '<':	return(LEFT);
		case '2':	return(BINARY);
		case '>':	return(RIGHT);
		case '%':
		case '\\':	return(MARK);
		case '=':	return(PREC);
		case '{':	return(LCURLY);
		default:	reserve = 1;
			}

	default:

		if( isdigit(c) ){ /* number */
			numbval = c-'0' ;
			base = (c=='0') ? 8 : 10 ;
			for( c=getc(finput); isdigit(c) ; c=getc(finput) ){
				numbval = numbval*base + c - '0';
				}
			(void) ungetc( c, finput );
			return(NUMBER);
			}
		else if( islower(c) || isupper(c) || c=='_' || c=='.' || c=='$' ){
			i = 0;
			while( islower(c) || isupper(c) || isdigit(c) || c=='_' || c=='.' || c=='$' ){
				tokname[i] = (char)c;
				if( reserve && isupper(c) ) tokname[i] += 'a'-'A';
				if( ++i >= toksize )
					exp_tokname();
				c = getc(finput);
				}
			}
		else return(c);

		(void) ungetc( c, finput );
		}

	tokname[i] = '\0';

	if( reserve ){ /* find a reserved word */
		if( !strcmp(tokname,"term")) return( TERM );
		if( !strcmp(tokname,"token")) return( TERM );
		if( !strcmp(tokname,"left")) return( LEFT );
		if( !strcmp(tokname,"nonassoc")) return( BINARY );
		if( !strcmp(tokname,"binary")) return( BINARY );
		if( !strcmp(tokname,"right")) return( RIGHT );
		if( !strcmp(tokname,"prec")) return( PREC );
		if( !strcmp(tokname,"start")) return( START );
		if( !strcmp(tokname,"type")) return( TYPEDEF );
		if( !strcmp(tokname,"union")) return( UNION );
		error(":69", "Invalid escape, or illegal reserved word: %s", tokname );
		}

	/* look ahead to distinguish IDENTIFIER from C_IDENTIFIER */

	c = getc(finput);
	while( c==' ' || c=='\t'|| c=='\n' || c=='\f' || c== '/' ) {
		if( c == '\n' ) {
			++peekline;
		} else if( c == '/' ){ /* look for comments */
			peekline += skipcom();
			}
		c = getc(finput);
		}
	if( c == ':' ) return( C_IDENTIFIER );
	(void) ungetc( c, finput );
	return( IDENTIFIER );
}

static
fdtype( t ){ /* determine the type of a symbol */
	register v;
	if( t >= NTBASE ) v = nontrst[t-NTBASE].tvalue;
	else v = TYPE( toklev[t] );
	if( v <= 0 ) error(":70", "Must specify type for %s", (t>=NTBASE)?nontrst[t-NTBASE].name:
			tokset[t].name );
	return( v );
	}

static
chfind( t, s ) register char *s; {
	int i;

	if (s[0]==' ')t=0;
	TLOOP(i){
		if(!strcmp(s,tokset[i].name)){
			return( i );
			}
		}
	NTLOOP(i){
		if(!strcmp(s,nontrst[i].name)) {
			return( i+NTBASE );
			}
		}
	/* cannot find name */
	if( t>1 )
		error(":71", "%s should have been defined earlier", s );
	return( defin( t, s ) );
	}

static void
cpyunion(){
	/* copy the union declaration to the output, and the define file if present */

	int level, c;
	if ( gen_lines )
		(void) fprintf( ftable, "\n# line %d \"%s\"\n", lineno, infile );
	(void) fprintf( fdefine, "typedef union\n" );
	(void) fprintf( fdefine, "#ifdef __cplusplus\n");
	(void) fprintf( fdefine, "	%sYYSTYPE\n", modpref);
	(void) fprintf( fdefine, "#endif\n");

	level = 0;
	for(;;){
		if( (c=getc(finput)) == EOF ) error(":72", "EOF encountered while processing %%union" );
		(void) putc( (char)c, fdefine );

		switch( c ){

		case '\n':
			++lineno;
			break;

		case '{':
			++level;
			break;

		case '}':
			--level;
			if( level == 0 ) { /* we are finished copying */
				(void) fprintf( fdefine, " %sYYSTYPE;\n\n", modpref );
				return;
				}
			}
		}
	}

static void
cpycode(){ /* copies code between \{ and \} */

	int c;
	c = getc(finput);
	if( c == '\n' ) {
		c = getc(finput);
		lineno++;
		}
	if ( gen_lines )
		(void) fprintf( ftable, "\n# line %d \"%s\"\n", lineno, infile );
	while( c != EOF ){
		if( c=='\\' ) {
			if( (c=getc(finput)) == '}' )  return;
			else (void) putc('\\', ftable );
		} else if( c=='%' ) {
			if( (c=getc(finput)) == '}' ) return;
			else (void) putc('%', ftable );
		}
		(void) putc( (char)c , ftable );
		if( c == '\n' ) ++lineno;
		c = getc(finput);
		}
	error(":73", "EOF before %%}" );
	}

static const char EOFincomment[] = "EOF inside comment";
static const char EOFincommentid[] = ":74";

static
skipcom(){ /* skip over comments */
	register c, i=0;  /* i is the number of lines skipped */

	/* skipcom is called after reading a / */

	c = getc(finput);
	if (is_y_plus_plus && c=='/')
	 {
	 /* C++ style comment */
	 while(c = getc(finput))
	  {
	  if (c==EOF)
		error(EOFincommentid, EOFincomment);
	  if (c == '\n' ) return(1);
	  }
	 }

	if( c != '*' ) error(":75", "Illegal comment" );
	c = getc(finput);
	while( c != EOF ){
		while( c == '*' ){
			if( (c=getc(finput)) == '/' ) return( i );
			}
		if( c == '\n' ) ++i;
		c = getc(finput);
		}
	error(EOFincommentid, EOFincomment);
	/* NOTREACHED */
	}

static void
cpyact(offset){ /* copy C action to the next ; or closing } */
	int brac, c, match, i, t, j, s, tok, argument, m;

	if ( gen_lines )
		(void) fprintf( faction, "\n# line %d \"%s\"\n", lineno, infile );

	brac = 0;

loop:
	c = getc(finput);
swt:
	switch( c ){

	    case ';':
		if( brac == 0 ){
			(void) putc( (char)c , faction );
			return;
			}
		goto lcopy;

	    case '{':
		brac++;
		goto lcopy;

	    case '$':
		s = 1;
		tok = -1;
		argument = 1;
		while ((c=getc(finput)) == ' ' || c == '\t') /* EMPTY */;
		if( c == '<' ){ /* type description */
			(void) ungetc( c, finput );
			if( gettok() != TYPENAME )
				 error(":76", "Bad syntax on $<ident> clause" );
			tok = numbval;
			c = getc(finput);
			}
		if( c == '$' ){
			(void) fprintf( faction, "Y.yyval");
			if( ntypes ){ /* put out the proper tag... */
				if( tok < 0 ) tok = fdtype( *prdptr[nprod] );
				(void) fprintf( faction, ".%s", typeset[tok] );
				}
			goto loop;
			}
		if ( isalpha(c) ) {
			int same = 0;
			(void) ungetc(c, finput);
			if (gettok() != IDENTIFIER) error(":77", "Bad action format");
			t = chfind(1, tokname);
			while((c=getc(finput))==' ' || c=='\t')/* EMPTY */;
			if (c == '#') {
				while((c=getc(finput))==' ' || c=='\t')/* EMPTY */;
				if ( isdigit(c) ) {
					m = 0;
					while (isdigit(c)) {
						m = m*10+c-'0';
						c = getc(finput);
					}
					argument = m;
				} else error(":78", "Illegal character \"#\"");
			}
			if ( argument<1 )
				 error(":79", "Illegal action argument no.");
			for( i=1; i<=offset; ++i)
				if (prdptr[nprod][i] == t)
					if (++same == argument) {
						(void) fprintf(faction, "yypvt[-%d]",offset-i);
						if(ntypes) {
							if (tok<0)
								tok=fdtype(prdptr[nprod][i]);
							(void) fprintf(faction,".%s",typeset[tok]);
						}
						goto swt;
					}
			error(":80", "Illegal action arguments");
		}
		if( c == '-' ){
			s = -s;
			c = getc(finput);
			}
		if( isdigit(c) ){
			j=0;
			while( isdigit(c) ){
				j= j*10+c-'0';
				c = getc(finput);
				}

			j = j*s - offset;
			if( j > 0 ){
				error(":81", "Illegal use of $%d", j+offset );
				}

			(void) fprintf( faction, "yypvt[-%d]", -j );
			if( ntypes ){ /* put out the proper tag */
				if( j+offset <= 0 && tok < 0 ) error(":82", "Must specify type of $%d", j+offset );
				if( tok < 0 ) tok = fdtype( prdptr[nprod][j+offset] );
				(void) fprintf( faction, ".%s", typeset[tok] );
				}
			goto swt;
			}
		(void) putc( '$' , faction );
		if( s<0 ) (void) putc('-', faction );
		goto swt;

	    case '}':
		if( --brac ) goto lcopy;
		(void) putc( (char)c, faction );
		return;

	    case '/':	/* look for comments */
		(void) putc( (char)c , faction );
		c = getc(finput);
		if (is_y_plus_plus && c=='/')
		 {
		 /* C++ style comment */
		 (void) putc( (char)c , faction );
		 while(c = getc(finput))
		  {
		  if (c==EOF)
			{
			error(EOFincommentid, EOFincomment);
			}
		  if (c=='\n') break;
		  (void) putc( (char)c , faction );
		  }
		 ++lineno;
		 goto lcopy;
		 }

		if( c != '*' ) goto swt;

		/* it really is a comment */

		(void) putc( (char)c , faction );
		c = getc(finput);
		while( c != EOF ){
			while( c=='*' ){
				(void) putc( (char)c , faction );
				if( (c=getc(finput)) == '/' ) goto lcopy;
				}
			(void) putc( (char)c , faction );
			if( c == '\n' )++lineno;
			c = getc(finput);
			}
		error(EOFincommentid, EOFincomment);
		/* FALLTHRU */

	    case '\'':	/* character constant */
	    case '"':	/* character string */
		match = c;
		(void) putc( (char)c , faction );
		while( (c=getc(finput)) != EOF ){
			if( c=='\\' ){
				(void) putc( (char)c , faction );
				c=getc(finput);
				if( c == '\n' ) ++lineno;
				}
			else if( c==match ) goto lcopy;
			else if( c=='\n' ) error(":83", "Newline in string or char. const." );
			(void) putc( (char)c , faction );
			}
		error(":84", "EOF in string or character constant" );
		/* FALLTHRU */

	    case EOF:
		error(":85", "Action does not terminate" );
		/* FALLTHRU */

	    case '\n':	++lineno;
		goto lcopy;

		}

lcopy:
	(void) putc( (char)c , faction );
	goto loop;
	}

static void
lhsfill( s )	/* new rule, dump old (if exists), restart strings */
	char *s;
{
	static int lhs_len = LHS_TEXT_LEN;
	int s_lhs;

	if (s != NULL)
		s_lhs = strlen(s);
	else
		s_lhs = 0;
	if ( s_lhs >= lhs_len ) {
		lhs_len = s_lhs + 2;
		lhstext=(char *)realloc((char *)lhstext, sizeof(char)*lhs_len);
		if (lhstext == NULL ) error(":86", "Not enough memory to expand LHS length");
	}
	rhsfill( (char *) 0 );
	(void) strcpy( lhstext, s );	/* don't worry about too long of a name */
}


static void
rhsfill( s )
	char *s;	/* either name or 0 */
{
	static char *loc;	/* next free location in rhstext */
	static int rhs_len = RHS_TEXT_LEN;
	static int used = 0;
	int s_rhs;
	register char *p;

	if (s != NULL)
		s_rhs = strlen(s);
	else
		s_rhs = 0;
	if ( !s )	/* print out and erase old text */
	{
		if ( *lhstext )		/* there was an old rule - dump it */
			lrprnt();
		( loc = rhstext )[0] = '\0';
		return;
	}
	/* add to stuff in rhstext */
	p = s;

	used = loc - rhstext;
	if ( (s_rhs + 3) >= (rhs_len - used) ) {
		static char *textbase;
		textbase = rhstext;
		rhs_len += s_rhs + RHS_TEXT_LEN;
		rhstext=(char *)realloc((char *)rhstext, sizeof(char)*rhs_len);
		if (rhstext == NULL ) error(":87", "Not enough memory to expand RHS length");
		loc = loc - textbase + rhstext;
	}

	*loc++ = ' ';
	if ( *s == ' ' )/* special quoted symbol */
	{
		*loc++ = '\'';	/* add first quote */
		p++;
	}
	while ( *loc = *p++ )
		if ( loc++ > &rhstext[ RHS_TEXT_LEN ] - 3 ) break;

	if ( *s == ' ' )
		*loc++ = '\'';
	*loc = '\0';		/* terminate the string */
}

static void
lrprnt()	/* print out the left and right hand sides */
{
	char *rhs;

	if ( !*rhstext )		/* empty rhs - print usual comment */
		rhs = " /* empty */";
	else
		rhs = rhstext;
	(void) fprintf( fdebug, "	\"%s :%s\",\n", lhstext, rhs );
}


static void
beg_debug()	/* dump initial sequence for fdebug file */
{
	(void) fprintf( fdebug, "typedef struct\n");
	(void) fprintf( fdebug,"#ifdef __cplusplus\n\t%syytoktype\n", modpref);
	(void) fprintf( fdebug,"#endif\n{ char *t_name; int t_val; } %syytoktype;\n", modpref);
	(void) fprintf( fdebug,
		"#ifndef %sYYDEBUG\n#\tdefine %sYYDEBUG\t%d", modpref, modpref, gen_testing );
	(void) fprintf( fdebug, "\t/*%sallow debugging */\n#endif\n\n",
		gen_testing ? " " : " don't " );
	(void) fprintf( fdebug, "#if %sYYDEBUG\n\n%syytoktype %syytoks[] =\n{\n", modpref, modpref, modpref );
}


static void
end_toks()	/* finish yytoks array, get ready for yyred's strings */
{
	(void) fprintf( fdebug, "\t\"-unknown-\",\t-1\t/* ends search */\n" );
	(void) fprintf( fdebug, "};\n\nchar * %syyreds[] =\n{\n", modpref );
	(void) fprintf( fdebug, "\t\"-no such reduction-\",\n" );
}


static void
end_debug()	/* finish yyred array, close file */
{
	lrprnt();		/* dump last lhs, rhs */
	(void) fprintf( fdebug, "};\n#endif /* %sYYDEBUG */\n", modpref );
	(void) fclose( fdebug );
}


/* 2/29/88 -
** The normal length for token sizes is NAMESIZE - if a token is
** seen that has a longer length, expand "tokname" by NAMESIZE.
*/
static void
exp_tokname()
{
    toksize += NAMESIZE;
    tokname = (char *) realloc(tokname, sizeof(char) * toksize);
}


/* 2/29/88 -
**
*/
static void
exp_prod()
{
	int i;
	nprodsz += NPROD;

	prdptr = (int **) realloc((char *)prdptr, sizeof(int *) * (nprodsz+2));
	levprd	= (int *)  realloc((char *)levprd, sizeof(int) * (nprodsz+2));
	had_act = (char *) realloc((char *)had_act, sizeof(char) * (nprodsz+2));
	for (i=nprodsz-NPROD; i<nprodsz+2; ++i) had_act[i] = 0;

	if ((*prdptr == NULL) || (levprd == NULL) || (had_act == NULL))
		error(":88", "Not enough memory to expand productions");
}

/* 2/29/88 -
** Expand the number of terminals.  Initially there are NTERMS;
** each time space runs out, the size is increased by NTERMS.
** The total size, however, cannot exceed MAXTERMS because of
** the way LOOKSETS (struct looksets) is set up.
** Tables affected:
**	tokset, toklev : increased to ntoksz
**
**	tables with initial dimensions of TEMPSIZE must be changed if
**	(ntoksz + NNONTERM) >= TEMPSIZE : temp1[]
*/
static void
exp_ntok()
{
	ntoksz += NTERMS;

	tokset = (TOKSYMB *) realloc((char *)tokset, sizeof(TOKSYMB) * ntoksz);
	toklev = (int *) realloc((char *)toklev, sizeof(int) * ntoksz);

	if ((tokset == NULL) || (toklev == NULL))
		error(":89", "Not enough memory to expand NTERMS");
}


static void
exp_nonterm()
{
	nnontersz += NNONTERM;

	nontrst = (NTSYMB *)
		realloc((char *)nontrst, sizeof(TOKSYMB) * nnontersz);
	if (nontrst == NULL) error(":90", "Not enough memory to expand NNONTERM");
}

void exp_mem( flag )
int flag;
{
	int i;
	static int *membase;
	new_memsize += MEMSIZE;

	membase = tracemem;
	tracemem = (int *) realloc((char *)tracemem, sizeof(int) * new_memsize);
	if (tracemem == NULL) error(":91", "Not enough memory to expand mem table");
	if ( flag ) {
		for ( i=0; i<=nprod; ++i)
			prdptr[i] = prdptr[i] - membase + tracemem;
		mem = mem - membase + tracemem;
	} else {
		size += MEMSIZE;
		temp1 = (int *)realloc((char *)temp1, sizeof(int)*size);
		optimmem = optimmem - membase + tracemem;
	}
}
