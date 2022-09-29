/* MSG.C -- MISCELLANEOUS ASSEMBLER MESSAGE FUNCTIONS           */

/* history:                                                     */

/* n/a       E. Wilner   --  original error message code        */

/*  8/25/88  RJK         --  rev 0.16                           */
/*  9/12/88  RJK         --  rev 0.17                           */
/* 10/25/88  RJK         --  rev 0.18                           */
/* 11/22/88  RJK         --  rev 0.19                           */
/* 12/03/88  RJK         --  rev 0.20                           */
/*  2/28/89  RJK         --  rev 0.21 (1.0e0)                   */
/* 05/16/89  KADER       --  changed product name from asm29    */
/*                           to as29i (per danm)                */

/* ------------------------------------------------------------ */
/*                         definitions                          */
/* ------------------------------------------------------------ */



#include "asm29.h"
#include <stdarg.h>


#define e_indir     (-10)       /* indirect error message       */

static char *pt_str = "Assembly terminated.";
int dupmsg = 0;
static char *ind_msg;

/* See the  "error()" function for an explanation of  "e_indir" */
/* and "ind_msg".                                               */

/* ------------------------------------------------------------ */
/*                  general message functions                   */
/* ------------------------------------------------------------ */

/* "wmsg(prefix,s,a,b,c,...)"   accepts   a    "printf()"-style */
/* argument list "s,a,b,c,..." and  outputs a  message based on */
/* this list.                                                   */

/* If the  "prefix" argument is not a  null pointer, and if the */
/* string "prefix[]" is not  empty, "wmsg()" prepends  a prefix */
/* string based on "prefix[]" to the message.                   */

/* Additional notes:                                            */

/* (a) "wmsg()"  normally prints  the message produced  to  the */
/* standard-output stream and not to the standard-error stream. */

/* (b) If the global flag  "dupmsg" is  positive, "wmsg()" will */
/* print the message to both streams.                           */

/* (c) If the global flag  "dupmsg" is  negative, "wmsg()" will */
/* print  the message to the  standard-error stream and  not to */
/* the standard output stream.                                  */
/* VARARGS */
void
wmsg(char* prefix,char *s,...)
{
	va_list ap;
	char buf[256];			/* message buffer               */
	int n = 0;

	if ((prefix != NULL) && *prefix)
		n = sprintf(buf,"%s: ",prefix);
	else
		*buf = '\0';
	va_start(ap, s);
	(void)vsprintf (buf+n,s,ap);
	va_end(ap);
	(void)strcat(buf,"\n");

	if (dupmsg >= 0) {
		if (dupmsg != 0)
			fflush(stderr);
		(void)printf(buf);
		if (isatty (1))
			fflush(stdout);
	}

	if (dupmsg != 0) {
		fflush(stdout);
		(void)fprintf(stderr,buf);
		fflush(stderr);
	}
}


/* ------------------------------------------------------------ */

/* "xmsg()" is equivalent to "wmsg()", with the following three */
/* distinctions:                                                */

/* (a) "xmsg()" aborts the caller after printing  the specified */
/* message.                                                     */

/* (b) "xmsg()"  ignores  the  "dupmsg" flag.  The  message  is */
/* printed to the  standard-output stream,  by  default, and to */
/* both output streams if either stream has been redirected.    */

/* (c) If   the   global   variable  "pt_str"   points   to   a */
/* null-terminated line of text, "xmsg()" appends the specified */
/* line to the output.                                          */

/* VARARGS */
void
xmsg(char *prefix, char *s, ...)
{
	va_list ap;

	va_start(ap, s);
	dupmsg = !isatty (1) || !isatty (2);
	wmsg(prefix,s,ap);
	if ((pt_str != NULL) && *pt_str)
	    wmsg(0,pt_str);
	(void)unlink(dstname);
	exit(1);
	va_end(ap);
}

/* ------------------------------------------------------------ */

/* "zmsg()"   is  equivalent  to   "wmsg()",  with  the  single */
/* distinction that "zmsg()" does not take a prefix argument.   */

/* ------------------------------------------------------------ */

#ifdef NOTUSED
zmsg(s,a,b,c,d,e,f,g,h)
char s[];                       /* format string                */
int a,b,c,d,e,f,g,h;            /* dummy arguments              */
{
    wmsg (NULL,s,a,b,c,d,e,f,g,h);
}
#endif

/* ------------------------------------------------------------ */

/* "pmsg()"   is  equivalent  to   "zmsg()",  with  the  single */
/* distinction that "pmsg()" prints only to the standard-output */
/* stream; i.e., "pmsg()" ignores the global "dupmsg" flag.     */

/* ------------------------------------------------------------ */

#ifdef NOTUSED
pmsg(s,a,b,c,d,e,f,g,h)
char s[];                       /* format string                */
int a,b,c,d,e,f,g,h;            /* dummy arguments              */
{
    int x = dupmsg;             /* save duplicate-message flag  */
    dupmsg = 0;
    zmsg (s,a,b,c,d,e,f,g,h);
    dupmsg = x;                 /* restore flag                 */
}
#endif


void
panic(char *s)
{
    xmsg ("internal error",s);
}



/* "x_error(s,a,b,c,...)" accepts a  "printf()"-style  argument */
/* list, outputs a  message based on this list, and  aborts the */
/* calling program.                                             */

/* "x_error()" is used to handle miscellaneous fatal errors.    */
/* VARARGS */
void
x_error(char *s, ...)
{
	va_list ap;

	va_start(ap, s);
	xmsg ("fatal error",s,ap);
	va_end(ap);
}



/* "eprintf(s,a,b,c,...)" is  equivalent to "fprintf (stderr,s, */
/* a,b,c,...)".                                                 */
/* VARARGS */
void
eprintf(char *s, ...)
{
	va_list ap;

	va_start(ap, s);
	(void)fprintf(stderr,s, ap);
	va_end(ap);
}

/* ------------------------------------------------------------ */
/*                  pre-defined error messages                  */
/* ------------------------------------------------------------ */

/* The  following  error-message strings  must  be in assembler */
/* error-message code order.                                    */

/* See the  error-message  codes in  "asm29.h"  for  additional */
/* information.                                                 */

/* ------------------------------------------------------------ */

static char *error_msgs[] = {
    "phase error",
    "bad format:  missing delimiter",
    "malformed operand",
    "operand out of range",
    "illegal size",
    "multiply-defined symbol",
    "undefined symbol",         /* warning                      */
    "extra stuff on line",
    "missing label",
    "missing operand",
    "macro too big",
    "missing or extra .endm",
    "not in conditional code",
    "excessive condition nesting",
    "inconsistent \"else\" or \"endif\"",
    "expression must be absolute",
    "missing .endif",
    "bad number",
    "label useless",
    "misc. relocation error",
    "illegal operand",
    ") expected",
    "no such segment exists",
    "bad option field",
    "conflicting options",
    "index specifier expected (+/- nn)",
    "undefined instruction",
    "missing close-quote",
    "missing or invalid string delimiter",
    "not a special purpose register",
    "target address not word aligned",
    "user-generated error",
    "not allowed outside macro",
    "repeat-block nesting error",
    "symbol attribute directive error",
    "recursive macro error",
    "illegal in a bss section",
    "illegal characters/missing colon in label",
    "illegal digit in number",
    "illegal register",
    "source line truncated",
    "illegal local label",
    "illegal storage multiplier",
    "bad or missing tagname",
    "divide by zero",
    "only .block's allowed in dsect",
    "unaligned object",         /* warning                      */
    "file does not exist",
    "bad $-function",
    "floating-point value is too small",
    "floating-point value is too large",
    "illegal character(s) in source line",
    "conflicting section attributes",
    "section type not specified",
    "long symbol truncated",    /* warning                      */
    "too many operands",
    "expression overflow; result truncated"
};

/* ------------------------------------------------------------ */
/*             line-oriented error-message functions            */
/* ------------------------------------------------------------ */

/* "error(e)"  prints the  error (or warning) message specified */
/* by the error code "e".                                       */

/* If "e" is equal to  "e_indir",  "error()" prints the message */
/* pointed to by the global (or static) variable "ind_msg".     */

/* ------------------------------------------------------------ */

/* "error()" normally echoes the current  input line as part of */
/* the message produced.                                        */

/* "error()" normally prints at most  one  warning message  and */
/* one  error  message  per   input  line.  The  special   call */
/* "error (0)" resets this ceiling for the current input line.  */

/* Output is deferred until the second assembly pass.           */

/* ------------------------------------------------------------ */

#define SE_LEN      75          /* max # of input chars to echo */

void
error(int e)			/* error code                   */
{
    char *bp;                   /* scratch                      */
    int c;                      /* scratch                      */
    char *cp;                   /* scratch                      */
    static int errline;         /* error   line number (or -1)  */
    struct include *ip;         /* include-file pointer         */
    int iswarn = FALSE;         /* is-warning flag              */
    static int warnline;        /* warning line number (or -1)  */

/* reset line status?           */

    if (e == 0)
    {                           /* yes                          */
	errline = warnline = (-1);
	return;                 /* done, exit                   */
    }

/* skip lines already in error  */

    if (lineno == errline) return;

/* handle warnings vs. errors   */

    switch (e)
    {
case e_longname:
case e_noalign:
case e_undef:
case w_fsize:
	iswarn = TRUE;          /* warning                      */
	if (omit_warn || (lineno == warnline)) return;
	warnline = lineno;
	warncnt++;
	break;

default:                        /* error                        */
	errline = lineno;
	errcnt++;
	break;
    }

/* handle misc. conditions      */

    if (errcnt > MAXERRS) {
	x_error ("too many errors at line %d", mainsrc->if_lineno);
    }
    if (pass != 3 && !debugon)
	return;

/* echo part of input line      */

    bp  = srcline;
    cp  = bp + SE_LEN;
    c   = *cp;
    *cp = EOS;
    printf ("*** %s\n^^^ %s line %d: ",bp,src->if_name,lineno);
    *cp = c;

/* print appropriate message    */

    if (e == w_fsize) e = e_size;

    switch (e)
    {
case e_indir:                   /* indirect error message       */
	printf (ind_msg);
	break;

default:
	if ((e < 0) || (e > MAX_ERROR_NO))
	{                       /* unrecognized error code      */
	    printf ("Error: error # %d",e);
	    break;
	}
				/* normal error or warning      */
	cp = iswarn ? "Warning" : "Error";
	printf ("%s: %s",cp,error_msgs[e-1]);
	break;
    }

    printf ("\n");
    pageline += 2;

/* handle include files         */

    for (ip = src->if_link; ip != NULL; ip = ip->if_link)
    {
	printf ("^^^ from %s line %d\n",
	    ip->if_name,ip->if_lineno);
	pageline++;
    }

/* wrap it up                   */

    if (ttyout) fflush (stdout);
}


/* "fatal(e)" is  similar  to  "error(e)",  but  this variation */
/* prints  the  specified  error  message   immediately  (i.e., */
/* regardless of the pass) and aborts the calling program.      */
void
fatal(int e)				/* error-message code           */
{
	error(0);			/* reset error line             */
	pass = 3;			/* force the message            */
	error(e);			/* print the message            */

	if ((pt_str != NULL) && *pt_str) {  /* append a termination line    */
		printf ("%s\n",pt_str);
	}

	(void)unlink(dstname);
	exit(1);
}



/* "s_fatal(s)"  passes the  error-message string  "s[]" to the */
/* "fatal()" function through the "e_indir" facility.           */
void
s_fatal(char *s)
{
	ind_msg = s;
	fatal(e_indir);
}

/* ------------------------------------------------------------ */
/*                          help text                           */
/* ------------------------------------------------------------ */

/* general description          */

static char *h_desc[] = {
    "Usage:",
    NS,
    "    as29i [-option(s)] sourcefile[.s]",
    NS,
    "Options:",
    NS,
    "    -Dsym[=val]  --  define symbol with optional value",
    "    -L           --  export local $L$ and $.L$ symbols",
    "    -R           --  suppress $.data$ directive",
    NS,
    "    -h           --  help",
    "    -l           --  generate a listing",
    "    -f[flags]    --  listing flags",
    "    -o outfile   --  specify output file",
    "    -s           --  $silent$ operation",
    NULL
};

/* ------------------------------------------------------------ */

/* listing flags                */

static char *h_listf[] = {
    "    Listing flags (used with -f):",
    NS,
    "    c            --  include suppressed code",
    "    g            --  include internal symbols",
    "    i            --  list include files",
    "    l###         --  specify line length (default is 132)",
    "    m            --  include macro and rep expansions",
    "    o            --  list data storage overflow",
    "    p###         --  specify page length (see below)",
    "    s            --  list symbols",
    "    w            --  suppress warning messages",
    "    x            --  cross-reference",
    NS,
    "$-p$ page length does not include headers and footers.",
    "Default value is 52 lines; $-p0$ suppresses pagination.",
    NULL
};

/* ------------------------------------------------------------ */
/*                        help functions                        */
/* ------------------------------------------------------------ */

/* If "ma[]" is an array of string pointers ending  with a null */
/* pointer, "print_ma(ma)"  prints the specified strings to the */
/* standard-error stream, one per line.                         */

/* "print_ma()" appends a newline to each string;  e.g., a null */
/* (empty) string will print as a newline.                      */

/* "print_ma()"  interprets a  dollar  sign  ($)  as  a  double */
/* quote (").                                                   */

/* ------------------------------------------------------------ */

print_ma(ma)
char *ma[];                     /* message array                */
{
    int c;                      /* message character            */
    char *cp;                   /* pointer into current line    */
    int i;                      /* line number (0+)             */

    for (i = 0; (cp = ma[i]) != NULL; i++)
    {
	while ((c = *cp++) != EOS)
	{
	    if (c == DS) c = DQ;
	    putc(c,stderr);
	}
	putc(NL,stderr);
    }
}
