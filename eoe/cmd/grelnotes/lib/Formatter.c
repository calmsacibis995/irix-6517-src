/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 * 			All Rights Reserved.				  *
 *									  *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.; *
 * the contents of this file may not be disclosed to third parties,	  *
 * copied or duplicated in any form, in whole or in part, without the	  *
 * prior written permission of Silicon Graphics, Inc.			  *
 *									  *
 * RESTRICTED RIGHTS LEGEND:						  *
 *	Use, duplication or disclosure by the Government is subject to    *
 *	restrictions as set forth in subdivision (c)(1)(ii) of the Rights *
 *	in Technical Data and Computer Software clause at DFARS 	  *
 *	252.227-7013, and/or in similar or successor clauses in the FAR,  *
 *	DOD or NASA FAR Supplement. Unpublished - rights reserved under   *
 *	the Copyright Laws of the United States.			  *
 **************************************************************************
 *
 * File: Formatter.c
 *
 * Description: Routines for formatting and reading ASCII and nroff 
 *	text into a program buffer.
 *
 **************************************************************************/


#ident "$Revision: 1.3 $"


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <malloc.h>
#include "Formatter.h"


#define MAXLINESIZE	BUFSIZ		/* BUFSIZ defined in stdio.h */
#define TABSIZE		8		/* Number of spaces in a tab */

/* Line buffer allocation constants */

#define CHARS_PER_LINE	40		/* Guess as to # chars per line */
#define ADD_MORE_LINES	100		/* Add in 100 line chunks */
#define ADD_MORE_CHARS	1024		/* Amount to grow the text buffer */

/* Characters */

#define CAR_RET		'\r'
#define TAB		'\t'
#define ESCAPE		'\033'
#define BACKSPACE	'\010'

/* Formatting commands */

#define NEQN_CMD	"/usr/bin/neqn /usr/pub/eqnchar"
#define TBL_CMD		"/usr/bin/tbl"
#define NROFF_CMD	"/usr/bin/nroff -i"
#define COL_CMD		"/usr/bin/col -f"
#define ZCAT_CMD	"/usr/bsd/zcat"
#define PCAT_CMD	"/usr/bin/pcat"
#define CAT_CMD		"/bin/cat"
#define GZCAT_CMD	"/usr/sbin/gzcat -c"
#define HTML_CMD	"/usr/sbin/html2term"

/* Font change test macro */
/*	Tests whether the current font is the same as the specified font.
	If the fonts are different, a new segment is added to the current
	line in the current font and the buffer is reset.
*/
#define FONT_CHANGE_TEST(font)	if (font_flag != UNKNOWN_STYLE && \
			    		font_flag != (font)) { \
			            *tbptr = '\0'; \
				    skip_under = 0; \
			            add_text(line, line_chars, temp_buf, \
						font_flag); \
				    *temp_buf = '\0'; \
			            tbptr = temp_buf; \
			        }


static char	*process_line(vwr_text_line_t*, char*, char**);
static char	*classify(char*, char*, int*);
static void	add_text(vwr_text_line_t*, char*, char*, int);


static vwr_text_style_t *text_styles;

/* Formatting commands */

static char *format_cmds[] = {
    {""},						     /* FMT_NULL */
    {CAT_CMD" %s > %s"},				     /* FMT_GENERIC */
    {NEQN_CMD" %s|"TBL_CMD"|"NROFF_CMD" -man|"COL_CMD">%s"}, /* FMT_NROFF_MAN */
    {NEQN_CMD" %s|"TBL_CMD"|"NROFF_CMD" -mm|"COL_CMD">%s"},  /* FMT_NROFF_MM */
    {ZCAT_CMD" %s|"COL_CMD">%s"},			     /* FMT_ZCAT */
    {PCAT_CMD" %s|"COL_CMD">%s"},			     /* FMT_PCAT */
    {GZCAT_CMD" %s|"COL_CMD">%s"},			     /* FMT_GZCAT */
    {GZCAT_CMD" %s|"HTML_CMD"|"COL_CMD">%s"},		     /* FMT_GZCAT_HTML*/
};



/**************************************************************************
 *
 * Function: VwrFormatText
 *
 * Description: Takes a file in a number of formats and converts it to
 *	nroff output format. The resulting file can then be read by
 *	VwrReadText into a program buffer.
 *
 * Parameters: 
 *	source_name (I) - source text file pathname
 *	dest_name (I) - formatted output file pathname
 *	source_format (I) - format of source file. One of:
 *
 *				FMT_NROFF_MAN 	- Unformatted nroff man page
 *				FMT_NROFF_MM 	- Unformatted nroff mm doc
 *				FMT_ZCAT	- Compressed formatted nroff
 *				FMT_PCAT	- Packed formatted nroff
 *				FMT_GENERIC	- A copy operation
 *				FMT_UNKNOWN	- Unknown, take best guess
 *
 * Return: 0 if successfull at formatting. If an error has occurred -1
 *	will be returned and errno will be set.
 *
 **************************************************************************/

int VwrFormatText(char *source_name, char *dest_name, int source_format)
{
    char cmd[BUFSIZ], buffer[BUFSIZ], new_name[BUFSIZ];
    char *suffix;
    int exit_status;

    /*
     * Determine the file suffix
     */
    suffix = strrchr(source_name, '.');

    /*
     * If format unknown try to determine it by
     * looking at the file suffix
     */
    if (source_format == FMT_UNKNOWN) {
	if (!suffix)
	    source_format = FMT_NROFF_MM;
	else if (strcmp(suffix, ".z") == 0)
	    source_format = FMT_PCAT;
	else if (strcmp(suffix, ".Z") == 0)
	    source_format = FMT_ZCAT;
	else
	    source_format = FMT_NROFF_MAN;
    }

    if (strcmp(suffix, ".gz") == 0) {  
        if (strstr(source_name, "/ch"))
	    source_format = FMT_GZCAT;
	else
	    source_format = FMT_GZCAT_HTML;
    }

    /*
     * pcat and zcat are very finnicky. They insist on the files they work
     * with ending in .z and .Z repsectively. We now ensure that this is the
     * case for those formatting situations.
     *
      * We finally issue the formatting command.
     */
    if (source_format == FMT_PCAT) {
	if (strcmp(source_name, ".z") != 0) {
	    sprintf(new_name, "%s.z", dest_name);
	    sprintf(buffer, "ln -s %s %s; %s; rm -f %s",
						source_name, new_name,
						format_cmds[source_format],
						new_name);
            sprintf(cmd, buffer, new_name, dest_name);
	} else 
            sprintf(cmd, format_cmds[source_format], source_name, dest_name);
    } else if (source_format == FMT_ZCAT) {
	if (strcmp(source_name, ".Z") != 0) {
	    sprintf(new_name, "%s.Z", dest_name);
	    sprintf(buffer, "ln -s %s %s; %s; rm -f %s",
						source_name, new_name,
						format_cmds[source_format],
						new_name);
            sprintf(cmd, buffer, new_name, dest_name);
	} else 
            sprintf(cmd, format_cmds[source_format], source_name, dest_name);
    } else {
        sprintf(cmd, format_cmds[source_format], source_name, dest_name);
    }
    exit_status = system(cmd);
    return(WEXITSTATUS(exit_status));
}


/**************************************************************************
 *
 * Function: VwrReadText
 *
 * Description: Reads the nroff output format text from the file specified.
 *	Each line of text in the file is processed such that the text and
 *	attributes are separated. The text goes into one large text buffer,
 *	while the attributes for each line are formed into a linked list
 *	of text attribute segment for each line. A text attribute segment
 *	is a continous piece of the text line in the same font. So a line
 *	all in normal font text would consist of one segment whereas a line
 *	of text with the first word bolded and all subsequent text in normal
 *	font would consist of two segments. Each segment contains the number
 *	of characters in the segment and the line structure contains a
 *	pointer to the \n terminated string of characters in the text buffer
 *	that comprises the line.
 *
 *	The motivation for one large text buffer is to allow regular
 *	expression text searching using the off-the-shelf search routines.
 *
 * Parameters: 
 *	filename (I) - name of text file to read
 *	text (O) - text structure to fill with newly read text
 *	styles (I) - array of styles for the text. The array of styles must
 *		     be organised as follows:
 *			styles[0] - style for normal text
 *			styles[1] - style for underlined text
 *			styles[2] - style for bold text
 *			styles[3] - style for bold underlined text
 *	blank_compress (I) - Blank line compression
 *
 * Return: If successful, 0 is returned. If an error occurs, a message
 *	is printed to stderr, errno is set and -1 is returned.
 *
 **************************************************************************/

int VwrReadText(char *filename, vwr_text_t *text, vwr_text_style_t *styles,
						int blank_compress)
{
    FILE *fp;
    char buf[MAXLINESIZE], *new_buf, *bptr;
    char *processed_line;
    vwr_text_line_t *tlptr;
    int nlines_guess, len;
    int buf_amount;
    register int char_buf_off;
    register int buf_used;
    register int i, line_num, blank_count, base;
    struct stat fileinfo;

    /*
     * Save the list of styles for future reference
     */
    text_styles = styles;

    /*
     * Open the file.
     */
    if ((fp = fopen(filename, "rb")) == NULL) {
	(void)fprintf(stderr, "Unable to open %s\n", filename);
	return(-1);
    }

    /*
     * Based on the size of the file and a guess as to the number of
     * characters per line we will take a guess as to the number of lines
     * of text in the file. We will allocate the line buffers based on
     * this guess. If it proves to be insufficient we will grow the
     * buffer dynamically based on ADD_MORE_LINES
     */
    if (fstat(fileno(fp), &fileinfo)) {
	(void)fprintf(stderr, "Unable to stat %s\n", filename);
	return(-1);
    }
    VwrFreeText(text);
    buf_amount = fileinfo.st_size;
    nlines_guess = buf_amount / CHARS_PER_LINE;
    text->lines = (vwr_text_line_t*)malloc(nlines_guess * 
						sizeof(vwr_text_line_t));

    /*
     * Based on the size of the unprocessed text file we will allocate
     * the text buffer. This is ussually sufficient, but in uncommon cases
     * where the file contains a large number of tab characters we will have
     * to grow this buffer.
     */
    text->char_buf = (char*)malloc(buf_amount);

    /*
     * Read each line of the file into the buffer, process the line
     * to interpret bolds, underlines and tabs. Some lines
     * (mainly tbl output) contain multiple lines. These are broken
     * into separate lines and added to the text structure.
     */
    char_buf_off = 0;			/* Start at beginning of char buf */
    line_num = blank_count = 0;		/* First line, no blanks */
    buf_used = 0;			/* Haven't used any of the char buf */
    while ((bptr = fgets(buf, MAXLINESIZE, fp)) != NULL) {
	do {
	    /*
	     * Blank compression
	     */
	    if (blank_compress && (*bptr == '\n')) {
	        if (++blank_count / blank_compress)
	            continue;
	    }
	    else
	        blank_count = 0;

	    /*
	     * Line buffer allocation heuristic
	     */
	    if ((line_num + 1) > nlines_guess) {
	        nlines_guess += ADD_MORE_LINES;
	        text->lines = (vwr_text_line_t*)realloc((char*)text->lines,
				nlines_guess * sizeof(vwr_text_line_t));
	    }

	    /*
	     * Process the line into attributes and plain text
	     */
	    tlptr = &text->lines[line_num];
	    processed_line = process_line(tlptr, bptr, &new_buf);

	    /*
	     * Add a \n to the end of the plain text line and
	     * append this line to the character buffer. The buffer
	     * is grown if necessary
	     */
	    len = tlptr->line_nchars;
	    processed_line[len++] = '\n';
	    processed_line[len] = '\0';
	    buf_used += len + 2;
	    if (buf_used >= buf_amount) {
		buf_amount += ADD_MORE_CHARS;
		text->char_buf = (char*)realloc(text->char_buf, buf_amount);
	    }
	    (void)memcpy(text->char_buf + char_buf_off, processed_line, len+1);
	    tlptr->line_chars = (char*)char_buf_off;
	    char_buf_off += len;

	    /*
	     * Check max line length stat
	     */
	    if (tlptr->line_len > text->max_line_len)
		text->max_line_len = tlptr->line_len;

	    /*
	     * Next line
	     */
	    line_num++;

	    /*
	     * Multiple lines on a single line processing
	     */
	    if (new_buf)
	        bptr = new_buf;
	} while (new_buf);	/* Do while multiple lines on single line */
    }

    /*
     * Close the file.
     */
    (void)fclose(fp);

    /*
     * Save the number of lines
     */
    text->nlines = line_num;

    /*
     * Convert the text buffer offsets into text buffer addresses.
     * This is done by adding the text buffer base address to the
     * offsets for each line
     */
    base = (int)(text->char_buf);
    for (i = 0, tlptr = text->lines; i < line_num; i++, tlptr++)
        tlptr->line_chars += base;

    return(0);
}


/**************************************************************************
 *
 * Function: VwrInitText
 *
 * Description: Initializes a text buffer
 *
 * Parameters: 
 *	text (I) - newly instanciated text buffer
 *
 * Return: none
 *
 **************************************************************************/

void VwrInitText(vwr_text_t *text)
{
    text->nlines = 0;
    text->lines = (vwr_text_line_t*)NULL;
    text->char_buf = NULL;
    text->max_line_len = 0;
}


/**************************************************************************
 *
 * Function: VwrFreeText
 *
 * Description: Frees the text buffer if one has been allocated.
 *
 * Parameters: none
 *
 * Return: none
 *
 **************************************************************************/

void VwrFreeText(vwr_text_t *tbuf)
{
    register int i;
    register vwr_text_line_t *tptr;
    register vwr_text_seg_t *tsptr, *temp;

    /*
     * Free each string segment, then free each lines segment array
     * and finally free the array of lines. Make sure that we have a
     * non-NULL pointer and a non-zero line count.
     */
    if (tbuf->lines && tbuf->nlines) {
        for (i = 0, tptr = tbuf->lines; i < tbuf->nlines; i++, tptr++) {
	    if (tptr->segs) {
		tsptr = tptr->segs;
		while (tsptr) {
		    temp = tsptr->next;
		    free((char*)tsptr);
		    tsptr = temp;
	        }
	    }
	    tptr->line_len = 0;
	    tptr->segs = tptr->last_seg = NULL;
	}
	free((char*)tbuf->lines);
    }
    if (tbuf->char_buf)
	free((char*)tbuf->char_buf);
    VwrInitText(tbuf);
}


/*
 ==========================================================================
			 	LOCAL FUNCTIONS
 ==========================================================================
*/


/**************************************************************************
 *
 * Function: process_line
 *
 * Description: Takes the specified, null terminated line and constructs
 *	a string segment array based on the text in that string. The
 *	function interprets normal text, boldifying and underlining and
 *	converts these to different fonts. The function also expands
 *	tabs. The function also handles the occurence of multiple lines
 *	contained on a single input line. This occurs with tbl output
 *	since it contains forward half-newlines followed by the \r
 *	character.
 *
 * Parameters: 
 *	line (O) - Pointer to the text line structure
 *	buf (I) - Null terminated line of text
 *	new_bufp (O) - A new line of text extracted from the original
 *		       line and detected by the presence of a ^M. If
 *		       the original line does not contain another line,
 *		       this variable will be set to NULL.
 *
 * Return: Pointer to processed plain text line buffer.
 *
 **************************************************************************/

static char *process_line(vwr_text_line_t *line, char *buf, char **new_bufp)
{
    int skip_under = 0;
    int len, again;
    int fill, cpos;
    int font_flag, style;
    register int i, j;
    char temp_buf[MAXLINESIZE];
    char line_chars[MAXLINESIZE];
    char base_char, next_base;
    register char *tbptr, *bptr;

    /*
     * Get length of string and strip trailing \n if any
     */
    len = strlen(buf);
    if (buf[len-1] == '\n')
	buf[len-1] = '\0';

    /*
     * Interpret the line
     *
     * buf - original string
     * temp_buf - processed string segment. Added to string segment array
     *		  with each font change and at end of line.
     */
    line->segs = NULL;			/* Initially empty list */
    line->last_seg = NULL;		/* Nothing in list */
    line->line_len = 0;			/* And no line length */
    line->line_nchars = 0;		/* And no characters */
    line->end_select = False;		/* End of line not selected */
    line->line_chars = NULL;		/* Start with empty line */
    *line_chars = '\0';			/* Start with empty accum buffer */
    cpos = 0;				/* Processed line character position */
    bptr = buf;				/* Line buffer pointer */
    tbptr = temp_buf;			/* String building buffer ptr */
    font_flag = UNKNOWN_STYLE;		/* Font is not known at start */
    *new_bufp = (char*)NULL;		/* Assume no addtional lines */
    again = TRUE;			/* Continue flag */
    for (i = 0; i <= len && again; i++) {
	switch (*bptr) {

	    /*
	     * A NULL indicates the end of the line. Just tack it
	     * onto the accumulation buffer and signal to stop processing.
	     */
	    case '\0':
		*tbptr = *bptr;		/* Pass character from buf to temp */
		again = FALSE;		/* End of the line */
		break;

	    /*
	     * Tabs will be expanded with enough ' ' characters to get
	     * to the next tab stop. The number of ' ' characters between
	     * tab stops is specified by TABSIZE.
	     */
	    case TAB:
		fill = (cpos / TABSIZE + 1) * TABSIZE - cpos;
		cpos += fill;
		for (j = 0; j < fill; j++)
		    *tbptr++ = ' ';
		bptr++;
		break;

	    /*
	     * tbl output processed by col -f has forward half linefeeds
	     * and can follow these with a \r character for table lines.
	     * We break this kind of thing into multiple lines and use the
	     * new_buf pointer to indicate the condition.
	     */
	    case CAR_RET:
		*bptr = '\0';
		*new_bufp = bptr + 1;
		break;

	    /*
	     * Escape characters will be ignored. We must also ignore
	     * the '9' charcter that follows an escape since an ESC-9
	     * is the forward half-linefeed indication output by col -f.
	     */
	    case ESCAPE:
		bptr++;
		if (*bptr == '9')
		    bptr++;
		break;

	    /*
	     * Handle printable characters. This part of the switch 
	     * supervises the classification of the unprocessed
	     * character and the addition of text to the output
	     * text buffer.
	     */
	    default:

		bptr = classify(bptr, &base_char, &style);
		/*
		 * For bold, underline and bold underlined text
		 */
		if (style != NORMAL_STYLE) {
		    FONT_CHANGE_TEST(style);
		    font_flag = style;
		    *tbptr = base_char;

		    /*
		     * If the current character is caps then decide
		     * if '_' should be skipped for this segment. We
		     * skip '_' if this segment has any words that start
		     * with cap letters.
		     */
		    if (isupper((int)base_char)) {
			if (tbptr == temp_buf) {
			    skip_under = 1;
			} else if (tbptr[-1] == ' ') {
			    skip_under = 1;
			}
		    }

		    tbptr++;
		}
		
		/*
		 * For normal text
		 */
		 else {

		    /*
	     	     * Note that when in underline or bold underline font
		     * words are separated with '_' instead of ' '. This 
		     * posess a problem for variables in these fonts that
		     * use underscores as part of the variable. We use a
		     * heuristic to determine whether to print the '_'.
		     * The heuristic is:
		     *
		     *	if (current font is underline or bold underline)
		     *		If (no upper case chars have started words 
		     *		    in this text segment and we have not
		     *		    skipped any '_' in this segment)
		     *			If (char after '_' is lower case or
		     *			    is a digit or segment started
		     *			    with '|' [tbl])
		     *				print '_'
		     *		else
		     *			print ' '
		     *			skip all rest of '_' in this segment
		     */
		    if (base_char == '_' && (font_flag == UNDERLINE_STYLE ||
				         font_flag == BOLDUNDER_STYLE)) {
			(void)classify(bptr, &next_base, &style);
			if (skip_under || (!islower((int)next_base) &&
				       !isdigit((int)next_base)) &&
				       *temp_buf != '|') {
		    	    *tbptr++ = ' ';
			    skip_under = 1;
			} else {
			    *tbptr++ = base_char;
			}
		    }

		    /*
		     * Purely normal text
		     */
		    else {
			FONT_CHANGE_TEST(style);
		        font_flag = style;
		        *tbptr++ = base_char;
		    }
		}
		cpos++;
		break;
	}
    }

    /*
     * End last part of line, if any
     */
    if (tbptr != temp_buf && font_flag != UNKNOWN_STYLE)
	add_text(line, line_chars, temp_buf, font_flag);

    return line_chars;
}


/**************************************************************************
 *
 * Function: classify
 *
 * Description: Classifies the font style of the specified unprocessed
 * 	character.
 *
 * 	The font styles are identified in the text as follows:
 *
 *		abcd			Normal font
 *		_^Ha_^Hb_^Hc_^Hd	Underline font
 *		a^Ha^Hab^Hb^Hb		Bold font
 *		_^Ha^Ha^Ha_^Hb^Hb^Hb	Bold underline font
 *
 * 	For underline, bold and bold underline fonts the base character
 *	(ie. the character that is actually printed on the screen) is
 * 	the character following the first backspace.
 *
 * Parameters: 
 *	bptr (I) - Pointer to location in unprocessed character buffer
 *		   where character starts,
 *	basep (O) - Actual character represented
 *	stylep (O) - Character font style. One of NORMAL_STYLE, BOLD_STYLE,
 *		     UNDERLINE_STYLE or BOLDUNDER_STYLE.
 *
 * Return: Pointer to start of next unprocessed character
 *
 **************************************************************************/

static char *classify(char *bptr, char *basep, int *stylep)
{
    /*
     * Non-normal font with have a BACKSPACE immediately after
     * the current character.
     */
    if (bptr[1] == BACKSPACE) {
        *basep = bptr[2];

	/* Bold font */
	if (*basep == *bptr) {
	    *stylep = BOLD_STYLE;
	    while (bptr[1] == BACKSPACE)    /* Skip repeats */
	        bptr += 2;
	    bptr++;
	}

	/* Underline or bold underline font */
	else if (*bptr == '_') {

	    /* Bold underline font */
	    if (bptr[3] == BACKSPACE) {
		*stylep = BOLDUNDER_STYLE;
		while (bptr[1] == BACKSPACE)    /* Skip repeats */
		    bptr += 2;
		bptr++;
	    }

	    /* Underline font */
	    else {
	        *stylep = UNDERLINE_STYLE;
	        bptr += 3;
	    }
	}

        /* Normal font - bullet character a^Hb */
        else {
	    *stylep = NORMAL_STYLE;
	    bptr += 3;
        }
    }

    /*
     * Normal font. Also need to handle the '_' character
     * if in underline or bold underline font
     */
    else {
	*basep = *bptr;
	*stylep = NORMAL_STYLE;
	bptr++;
    }

    /*
     * Return pointer to next unprocessed character
     */
    return bptr;
}


/**************************************************************************
 *
 * Function: add_text
 *
 * Description: Adds the new string segment to the lines strings segment
 *	array using the specified GC.
 *
 * Parameters: 
 *	line (I) - current text line structure
 *	line_buf (I,O) - text line accumaltion buffer
 *	new_text (I) - new text to be added to the end of the original
 *		       string segment array.
 *	font_flag (I) - font style to render new string. One of
 *			NORMAL_STYLE, UNDERLINE_STYLE, BOLD_STYLE,
 *			or BOLDUNDER_STYLE.
 *
 * Return: none
 *
 **************************************************************************/

static void add_text(vwr_text_line_t *line, char *line_buf,
					char *new_text, int font_flag)
{
    vwr_text_seg_t *sptr;
    int nchars;
    static char *buf_ptr;

    /*
     * Get the segment length
     */
    nchars = strlen(new_text);

    /*
     * Copy new text to the accumulation text buffer
     */
    if (!(*line_buf))
	buf_ptr = line_buf;
    (void)memcpy(buf_ptr, new_text, nchars + 1);
    buf_ptr += nchars;

    /*
     * Allocate a new string segment and fill it with the
     * relevant info
     */
    sptr = (vwr_text_seg_t*)malloc(sizeof(vwr_text_seg_t));
    sptr->next = NULL;
    sptr->seg_type = font_flag;
    sptr->seg_nchars = nchars;
    sptr->seg_len = XTextWidth(text_styles[font_flag].font, new_text,
						sptr->seg_nchars);
    
    /*
     * Update the line length and char count fields
     */
    line->line_len += sptr->seg_len;
    line->line_nchars += nchars;

    /*
     * Link this new segment onto segment list and update segment
     * count
     */
    if (line->segs) {
	line->last_seg->next = sptr;
	line->last_seg = sptr;
    } else {
	line->segs = line->last_seg = sptr;
    }
}
