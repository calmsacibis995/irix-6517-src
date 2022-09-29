/*
**  NAME
**	getcatNAME - parse formatted manual page and output NAME section info
**
**  SYNOPIS
**	getcatNAME
**
**  DESCRIPTION
**	getcatNAME is used by man(1) to create the whatis database entry
**	for a given manual page.
**
**	getcatNAME reads a formatted manual page on its standard input,
**	parses the manual page, and outputs the NAME section information
**	and manual section number in whatis database format.
**
**	getcatNAME expects to read a single manual page on stdin formatted as:
**
**		PAGENAME(sectionnumber)    blah blah    PAGENAME(sectionnumber)
**
**		NAME
**			pagename - description 
**
**	This gets translated to the whatis database format which is basically:
**
**		pagename (sectionnumber)        - description
**
**	The sectionnumber is extracted from the header and pagename and
**	description are extracted from the NAME section.  If no section number
**	is present in the header, no sectionnumber is output in the final
**	result.
**
**  SEE ALSO
**	apropos(1), makewhatis(1M), man(1), whatis(1)
**
**  AUTHOR
**	Dave Ciemiewicz (ciemo@sgi)
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>

#define	BACKSPACE	0x08
#define TAB	'\t'
#define SPACE	' '
#define NEWLINE	'\n'

#define FMTWIDTH 23

int
match(register char* str1, register char* str2)
{
    for (; *str1 == *str2 && *str1 != '\0'; str1++, str2++) ;
    return (*str1 == '\0');
}
    
int
main()
{
    char str[4096];
    char in[4096];
    char sectionNumberStr[256]; 
    char *sectionNumberPattern; 
    register char *strp;
    register char *inp;
    register char last;
    register unsigned int spaces;
    register unsigned int havesection = 0;
    register unsigned int gotname = 0;
    register unsigned int pastfirstlineofname = 0;
    register unsigned int printedsec = 0;

    sectionNumberStr[0] = '\0';
    sectionNumberPattern = regcmp("(\\([0-9l][+0-9a-zA-Z]*\\))$0", (char*)0);
			    /* section number is '(' [0-9l][0-9a-zA-Z+]* ')' */
	

    while (gets(str) != NULL) {
	/*
	**  Read input stream.  Change tabs, multiple spaces, and newlines
	**  to single character spaces.  Backspaces act as real backspaces
	**  in the "in" buffer.
	*/
	for (strp=str, inp=in, last=NEWLINE, spaces=0; *strp; strp++) {
	    if (*strp == TAB) *strp = SPACE;
	    if ((*strp == SPACE) && (last == NEWLINE)) {
		if (pastfirstlineofname) {
		    *strp = SPACE;
		} else {
		    continue;
		}
	    }
	    if (*strp == SPACE) spaces++; else spaces = 0;
	    if (spaces > 1) continue;

	    if (*strp == BACKSPACE && inp != in) {
		inp--;
		continue;
	    }

	    *inp++ = *strp;
	    last = *strp;
	}
	*inp = '\0';


	/*
	**  Parse section number from header.  Looking for:
	**
	** 	'(' [0-9l][0-9a-zA-Z+]* ')'
	**
	**  This pattern handles diverse section numbers such as:
	**
	**	(l)
	**	(3X11)
	**	(1EXP)
	**	(3C++)
	**
	**  If have already gotten the NAME section, don't mistakenly
	**  extract bogus section number information.
	**
	**  Some manual page headers are completely busted and look like:
	**
	**	foo(Silicon Graphics foo(3X)
	** 
	**  or
	**	foo(3X(tentative)(3X)
	**
	**  Since there is little chance we can get all of these fixed, and
	**  since this is a common formatting error, we'll for section numbers
	**  which have a more specific format than just '(' .* ')'.
	*/
	if (!gotname && !havesection) {
	    (void) regex(sectionNumberPattern, in, sectionNumberStr);
	    if (sectionNumberStr[0] != '\0') {
		havesection = 1;	/* section number is valid */
		continue;		/* done parsing, get next line */
	    }
	    /* Valid section not gotten so try matching other patterns */
	}


	/*
	**  Begin parsing NAME section of manual page.
	**  Clearcase manual pages use SUBCOMMAND NAME instead.
	*/
	if (match("NAME", in) || match("SUBCOMMAND NAME", in)) {
	    gotname = 1;
	    continue;	/* done parsing, get next line */
	}


	/*
	**  Terminate parsing of NAME section.
	**
	**  The name section may be multiple lines of text. We stop parsing
	**  NAME section text when we hit a blank line or if we run into any
	**  commonly known section names which include:
	**
	**  	SYNTAX, SYNOPSIS, DESCRIPTION, C SYNOPSIS, FORTRAN SYNOPSIS,
	**	C SPECIFICATION, FORTRAN SPECIFICATION
	*/
	if (in[0] == '\0') {
	    if (pastfirstlineofname) {
		break;
	    } else {
		continue;  /* name section is multiple lines, get next line */
	    }
	}
	if (match("SYNTAX", in)) break;
	if (match("SYNOPSIS", in)) break;
	if (match("DESCRIPTION", in)) break;
	strp = strchr(in, ' ');
	if (strp != NULL && match("SYNOPSIS", strp+1)) break;	
	if (strp != NULL && match("SPECIFICATION", strp+1)) break;	


	/*
	**  Output the NAME section of the manual in whatis format.
	*/
	if (gotname) {
	    unsigned int i;
	    unsigned int fmtcount = 0;

	    for (i = 0; in[i] != '\0'; i++) {
		if (in[i] == ' ' && in[i+1] == '-' &&
			(in[i+2] == ' ' || in[i+2] == '\0')) {
		    putc(' ', stdout);
		    fputs(sectionNumberStr, stdout);
		    printedsec = 1;
		    if (!pastfirstlineofname) {
			fmtcount += 1 + strlen(sectionNumberStr);
			for (; fmtcount < FMTWIDTH; fmtcount++) {
			    putc(' ', stdout);
			}
		    }
		    break;
		}

		putc(in[i], stdout);
		fmtcount++;
	    }
	    fputs(&(in[i]), stdout);
	    pastfirstlineofname = 1;
	}
    }

    if (!printedsec) {
	putc(' ', stdout);
	fputs(sectionNumberStr, stdout);
    }
    putc('\n', stdout);
    fflush(stdout);

    return(0);
}
