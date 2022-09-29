/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ident	"cmd/xargs/xargs.c: $Revision: 1.14 $"
#define FALSE 0
#define TRUE 1

/*
 * want as large as possible, but sysconf(_SC_ARG_MAX) includes
 * environment and word for a pointer per arg also.
 */
#define MAXARGS 1024
#define MAXINSERTS 5
char Errstr[256];
char *arglist[MAXARGS+1];
char *argbuf;
char *next;
char *buffer;
char *lastarg = "";
char **ARGV = arglist;
char *LEOF = "_"; 
char *INSPAT = "{}";
struct inserts {
	char **p_ARGV;		/* where to put newarg ptr in arg list */
	char *p_skel;		/* ptr to arg template */
	} saveargv[MAXINSERTS];
char *ins_buf;
char *p_ibuf;
int PROMPT = -1;
int BUFLIM;	/* set at top of main now. */
int Maxstdinargs;	/* max args that can be picked up from stdin.
	set to MAXARGS - cmdargs from xargs cmd line */
int N_ARGS = 0;
int N_args = 0;
int N_lines = 0;
int DASHX = FALSE;
int MORE = TRUE;
int PER_LINE = FALSE;
int ERR = FALSE;
int OK = TRUE;
int LEGAL = FALSE;
int TRACE = FALSE;
int INSERT = FALSE;
int linesize = 0;
int ibufsize = 0;
int Ncargs;
int Maxibuf;
int Maxsbuf;
void ermsg(char *);
void addibuf(struct inserts *);

main(argc,argv)
int argc;
char **argv; 
{
    extern char *addarg(), *getarg(), *checklen(), *insert();
    char *cmdname, *initbuf, **initlist, *flagval;
    int  initsize;
    int  retval = 0;
    register int j, n_inserts;
    register struct inserts *psave;
    
    /* initialization */

    argc--; argv++;
    n_inserts = 0;
    psave = saveargv;

    j = getenvsize();	/* need to do this before arg processing */
    Ncargs = sysconf(_SC_ARG_MAX);
    Maxibuf = Ncargs-1024;
    Maxsbuf = Ncargs/2;

    if (((argbuf  = (char *)malloc(Ncargs)) == NULL) ||
	((ins_buf = (char *)malloc(Maxibuf)) == NULL) ||
	((buffer  = (char *)malloc(Maxsbuf)) == NULL)) {
	sprintf(Errstr, "Cannot allocate more memory\n");
	ermsg(Errstr);
	exit(1);
    }
    next = argbuf;

    BUFLIM = Ncargs - j;

    if(BUFLIM <= ((int)sizeof(char *)+32)) {
	sprintf(Errstr, "Environment is too large (%d); no room left for arguments\n",
		j);
	ermsg(Errstr);
	exit(1);
    }

    /* look for flag arguments */

    while  ( *argv && (*argv)[0] == '-'  ) {
	if(!strcmp(*argv, "--")) {
	  argv++;
	  argc--;
	  break;
	}
	flagval = *argv+1;
	switch ( *flagval++ ) {
	  case 'x': DASHX = LEGAL = TRUE;
	    break;

	  case 'l': 
	    PER_LINE = LEGAL = TRUE;
	    N_ARGS = 0;
	    INSERT = FALSE;
	    if( *flagval && (PER_LINE=atoi(flagval)) <= 0 ) {
		sprintf(Errstr, "#lines must be positive int: %s\n", *argv);
		ermsg(Errstr);
	    }
	    break;

	  case 'L':
	    PER_LINE = TRUE;
	    N_ARGS = 0;
	    INSERT = FALSE;
	    if(!(*flagval)){
		flagval = *(++argv);
		argc--;
	    }
	    if((PER_LINE=atoi(flagval)) <= 0 ) {
		sprintf(Errstr, "#lines must be positive int: %s\n", *argv);
		ermsg(Errstr);
	    }
	    break;
	    

	  case 'i': 
	    INSERT = PER_LINE = LEGAL = TRUE;
	    N_ARGS = 0;
	    if ( *flagval ) 
		INSPAT = flagval;
	    break;

	  case 'I':
	    INSERT = PER_LINE = LEGAL = TRUE;
	    N_ARGS = 0;
	    if( *flagval)
	      INSPAT = flagval;
	    else{
		INSPAT = *(++argv);
		argc--;
	    }
	    break;

	  case 't': TRACE = TRUE;
	    break;

	  case 'e': LEOF = flagval;
	    break;

	  case 'E':
	    if(*flagval)
	      LEOF = flagval;
	    else{
		LEOF = *(++argv);
		argc--;
	    }
	    break;

	  case 's': 
	    if(!(*flagval)){
		flagval = *++argv;
		argc--;
	    }
	    j = atoi(flagval);
	    if( j>=BUFLIM  ||  j<=0 ) {
		sprintf(Errstr, "%s invalid: must be > 0 and < %d (varies with environment)\n", *argv, BUFLIM);
		ermsg(Errstr);
	    }
	    else
	      BUFLIM = j;
	    break;

	  case 'n': 
	    if(!(*flagval)){
		flagval = *++argv;
		argc--;
	    }
	    if( (N_ARGS = atoi(flagval)) <= 0 ) {
		sprintf(Errstr, "#args must be positive int: %s\n", *argv);
		ermsg(Errstr);
	    }
	    else {
		LEGAL = DASHX || N_ARGS==1;
		INSERT = PER_LINE = FALSE;
	    }
	    break;

	  case 'p': 
	    if( (PROMPT = open("/dev/tty",0)) == -1) {
		ermsg("can't read from tty for -p\n");
	    }
	    else
	      TRACE = TRUE;
	    break;

	  default: 
	    sprintf(Errstr, "unknown option: %s\n", *argv);
	    ermsg(Errstr);
	    break;
	}

	argv++;
	if ( --argc < 1 ) break;
    }

    if( ! OK )
      ERR = TRUE;

    /* pick up command name */
    
    if ( argc == 0 ) {
	cmdname = "/bin/echo";
	*ARGV++ = addarg(cmdname);
    }
    else
      cmdname = *argv;
    
    /* pick up args on command line */

    while ( OK && argc-- ) {
	if ( INSERT && ! ERR ) {
	    if ( xindex(*argv, INSPAT) != -1 ) {
		if ( ++n_inserts > MAXINSERTS ) {
		    sprintf(Errstr, "too many args with %s\n", INSPAT);
		    ermsg(Errstr);
		    ERR = TRUE;
		}
		psave->p_ARGV = ARGV;
		(psave++)->p_skel = *argv;
	    }
	}
	*ARGV++ = addarg( *argv++ );
	if(ARGV == &arglist[MAXARGS-10])  {	/* leave some room for stdin */
	    sprintf(Errstr, "too many xargs command line arguments (%d max)\n",
		    MAXARGS-10);
	    ermsg(Errstr);
	    exit(1);
	}
    }
    
    Maxstdinargs = &arglist[MAXARGS-1] - ARGV;

    /* pick up args from standard input */

    initbuf = next;
    initlist = ARGV;
    initsize = linesize;
    
    while ( OK && MORE ) {
	next = initbuf;
	ARGV = initlist;
	linesize = initsize;
	if ( *lastarg )
	  *ARGV++ = addarg( lastarg );

	while ( (*ARGV++ = getarg()) && OK );

	/* insert arg if requested */

	if ( !ERR && INSERT ) {
	    p_ibuf = ins_buf;
	    ARGV--;
	    j = ibufsize = 0;
	    for ( psave=saveargv;  ++j<=n_inserts;  ++psave ) {
		addibuf(psave);
		if ( ERR ) break;
	    }
	}
	*ARGV = 0;

				/* exec command */

	if ( ! ERR ) {
	    if ( ! MORE && (PER_LINE && N_lines==0 || N_ARGS && N_args==0) ) exit (0);
	    OK = TRUE;
	    j = TRACE ? echoargs() : TRUE;
	    if( j ) {
		int rc;
		rc = lcall(cmdname, arglist);
		retval |= rc;
		if ( rc != -1 ) continue;
		sprintf(Errstr, "%s not executed, returned -1 or killed\n", cmdname);
		ermsg(Errstr);
	    }
	}
    }
    if ( OK && !retval) return (0); else return (1);
}

char *
checklen(arg)
char *arg;
{
    register int oklen;

    oklen = TRUE;
    if ( (linesize += strlen(arg)+1) > BUFLIM ) {
	lastarg = arg;
	oklen = OK = FALSE;
	if ( LEGAL ) {
	    ERR = TRUE;
	    ermsg("arg list too long\n");
	}
	else if( N_args > 1 )
	  N_args = 1;
	else {
	    sprintf(Errstr, "single arg or initial list was > than max length (%d) with environment\n",
		    BUFLIM);
	    ermsg(Errstr);
	    ERR = TRUE;
	}
    }
    return ( oklen  ? arg : 0 );
}

char *
addarg(arg)
char *arg;
{
	strcpy(next, arg);
	arg = next;
	next += strlen(arg)+1;
	return ( checklen(arg) );
}

char *
getarg()
{
    register char c, c1, *arg;
    char *retarg;
    
    while ( (c=getchr()) == ' '
	   || c == '\n'
	   || c == '\t' );
    if ( c == '\0' ) {
	MORE = FALSE;
	return 0;
    }

    arg = next;
    for ( ; ; c = getchr() )
      switch ( c ) {
	  
	case '\t':
	case ' ' :
	  if ( INSERT ) { *next++ = c;
			  break;
		      }
	  
	case '\0':
	case '\n':
	  *next++ = '\0';
	  if( !strcmp(arg,LEOF) || c=='\0' ) {
	      MORE = FALSE;
	      if( c != '\n' )
		while( c=getchr() )
		  if( c=='\n' ) break;
	      return 0;
	  }
	  else {
	      ++N_args;
	      if( retarg = checklen(arg) ) {
		  if( (PER_LINE && c=='\n' && ++N_lines>=PER_LINE)
		     ||   (N_ARGS && N_args>=N_ARGS) ||
		     N_args >= Maxstdinargs) {
		      N_lines = N_args = 0;
		      lastarg = "";
		      OK = FALSE;
		  }
	      }
	      return retarg;
	  }
	  
	case '\\':
	  *next++ = getchr();
	  break;

	case '"':
	case '\'':
	  while( (c1=getchr()) != c) {
	      if( c1 == '\0' || c1 == '\n' ) {
		  *next++ = '\0';
		  sprintf(Errstr, "missing quote?: %s\n", arg);
		  ermsg(Errstr);
		  ERR = TRUE;
		  return (0);
	      }
	      *next++ = c1;
	  }
	  break;
	  
	default:
	  *next++ = c;
	  break;
      }
}

void
ermsg(messages)
char *messages;
{
    write(2,"xargs: ",7);
    write(2,messages,strlen(messages));
    OK = FALSE;
}

echoargs()
{
    register char **anarg;
    char yesorno[1], junk[1];
    register int j;

    anarg = arglist;
    anarg--;
    while ( *++anarg ) {
	write(2, *anarg, strlen(*anarg) );
	write(2," ",1);
    }
    if( PROMPT == -1 ) {
	write(2,"\n",1);
	return TRUE;
    }
    write(2,"?...",4);
    if( read(PROMPT,yesorno,1) == 0 )
      exit(0);
    if( yesorno[0] == '\n' )
	return FALSE;
    while( ((j=read(PROMPT,junk,1))==1) && (junk[0]!='\n') );
    if( j==0 )
	exit (0);
    return ( yesorno[0]=='y' );
}

char *
insert(pattern, subst)
char *pattern, *subst;
{
    int len, ipatlen;
    register char *pat;
    register char *bufend;
    register char *pbuf;

    len = strlen(subst);
    ipatlen = strlen(INSPAT)-1;
    pat = pattern-1;
    pbuf = buffer;
    bufend = &buffer[Maxsbuf];
    
    while ( *++pat ) {
	if( xindex(pat,INSPAT) == 0 ) {
	    if ( pbuf+len >= bufend ) break;
	    else {
		strcpy(pbuf, subst);
		pat += ipatlen;
		pbuf += len;
	    }
	}
	else {
	    *pbuf++ = *pat;
	    if (pbuf >= bufend ) break;
	}
    }
    
    if ( ! *pat ) {
	*pbuf = '\0';
	return (buffer);
    }
    else {
	sprintf(Errstr, "max arg size with insertion via %s's exceeded\n", INSPAT);
	ermsg(Errstr);
	ERR = TRUE;
	return 0;
    }
}

void
addibuf(p)
struct inserts *p;
{
    register char *newarg, *skel, *sub;
    int l;

    skel = p->p_skel;
    sub = *ARGV;
    linesize -= strlen(skel)+1;
    newarg = insert(skel,sub ? sub : "");
    if ( checklen(newarg) ) {
	if( (ibufsize += (l=strlen(newarg)+1)) > Maxibuf) {
	    ermsg("insert-buffer overflow\n");
	    ERR = TRUE;
	}
	strcpy(p_ibuf, newarg);
	*(p->p_ARGV) = p_ibuf;
	p_ibuf += l;
    }
}

getchr() {
    int c;
    c = getc(stdin);
    if (c == EOF)
	return(0);
    else
	return(c);
}

int lcall(sub,subargs)
char *sub, **subargs;
{

	int retcode;
	register int iwait, child;

	switch( child=fork() ) {
	default:
		while((iwait=waitpid(child, &retcode, 0)) == -1 && errno == ECHILD) ;
		if (iwait == -1
		 || WIFSIGNALED(retcode) && WTERMSIG(retcode) != SIGPIPE
		 || WIFEXITED(retcode)	&& WEXITSTATUS(retcode) == 0xff
		)
			return -1;
		if(WIFEXITED(retcode)){
		    if (WEXITSTATUS(retcode) == 126)
		      exit(126);
		    else if (WEXITSTATUS(retcode) == 127)
		      exit(127);
		}

		return WEXITSTATUS(retcode);
	case 0:
		execvp(sub,subargs);
		if(errno == EACCES) exit(126);
		if(errno == ENOENT) exit(127);
		return (0);
	case -1:
		return (-1);
		}
}
/*
	If `s2' is a substring of `s1' return the offset of the first
	occurrence of `s2' in `s1',
	else return -1.
*/

xindex(as1,as2)
char *as1,*as2;
{
	register char *s1,*s2,c;
	int offset;

	s1 = as1;
	s2 = as2;
	c = *s2;

	while (*s1)
		if (*s1++ == c) {
			offset = s1 - as1 - 1;
			s2++;
			while ((c = *s2++) == *s1++ && c) ;
			if (c == 0)
				return(offset);
			s1 = offset + as1 + 1;
			s2 = as2;
			c = *s2;
		}
	 return(-1);
}

/* figure out size of environment, including ptrs, so we
 * won't hit the sysconf(_SC_ARG_MAX) limit, but can still do as many
 * args as possible per exec.
*/
getenvsize()
{
	extern char **environ;
	char **e = environ;
	int len = 0;

	while(*e) {
		len += sizeof(*e);
		len += strlen(*e);
		e++;
	}
	return len;
}
