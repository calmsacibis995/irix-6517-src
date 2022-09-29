/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 * 	%$(Description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/sub.c,v 1.3 1996/10/04 12:11:03 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/sub.c,v $
 *
 * Revision History:
 *
 * $Log: sub.c,v $
 * Revision 1.3  1996/10/04 12:11:03  cwilson
 * latest rev
 *
 * Revision 2.31  1993/09/03  15:18:46  grant
 * Made lex not try to handle escaping anything but the " byte.
 *
 * Revision 2.30  1993/07/30  10:38:52  carlson
 * SPR 1923 -- rewrote pager to use file descriptor instead of file pointer.
 *
 * Revision 2.29  1993/05/26  14:46:02  raison
 * remove case sensitivity to parameter values (esp. 'Y' and 'N')
 *
 * Revision 2.28  93/05/12  16:09:54  carlson
 * R7.1 -- Added new netadm error messages and token_error routine
 * for parse.c.
 * 
 * Revision 2.27  93/03/17  15:27:37  wang
 * Changed lexical parser to support escape sequences for default_zone_list param.
 * 
 * Revision 2.26  93/02/24  13:10:08  carlson
 * Removed pager conditionals -- now taken care of in inc/na.h.
 * 
 * Revision 2.25  92/07/22  11:29:26  reeve
 * Fixed match() so that last alias for a table entry can
 * be matched via minimum uniqueness.
 * 
 * Revision 2.24  92/06/15  14:16:51  raison
 * don't fail the match when the requested parm has a matching alias
 * 
 * Revision 2.23  92/01/22  16:34:29  emond
 * Fixed COMPILE error due to missing comma after previously last netadm_err
 * string.
 * 
 * Revision 2.22  92/01/17  10:16:04  reeve
 * Added new error string for when an Annex does not support a parameter.
 * 
 * Revision 2.21  91/12/30  16:12:56  reeve
 * Added new error string for reject due to an Annex not supporting NA.
 * 
 * Revision 2.20  91/10/14  17:12:52  russ
 * Added pager.
 * 
 * Revision 2.19  91/09/30  10:27:06  carlson
 * Added check for NULL in match() so that multiple arrays can be searched.
 * 
 * Revision 2.18  91/08/01  10:19:36  dajer
 * Changed punt() to flag an error in the exit status.
 * 
 * Revision 2.17  91/04/08  23:34:15  emond
 * Fix some compile errors on SysV and accommodate generic API I/F
 * 
 * Revision 2.16  89/12/19  16:02:23  russ
 * Added some reverse compatability code for annex parameters.
 * 
 * Revision 2.15  89/05/18  10:22:22  grant
 * Added lex_switch() for boot shutdown.
 * 
 * Revision 2.14  89/04/05  12:42:14  loverso
 * Changed copyright notice
 * 
 * Revision 2.13  88/07/11  16:30:34  mattes
 * Allow newlines to be used in "broadcast" messages by not
 * translating them to spaces
 * 
 * Revision 2.12  88/07/08  14:00:33  harris
 * New error messages NAE_SESSION (CMJ_SESSION) NAE_RSRC (TOO_MANY_SESSIONS).
 * 
 * Revision 2.11  88/05/24  18:38:19  parker
 * Changes for new install-annex script
 * 
 * Revision 2.10  88/05/05  16:51:49  harris
 * Changed error NAE_SREJECT to "erpc reject, wrong password".
 * 
 * Revision 2.9  88/05/04  23:04:12  harris
 * Added match_flag for di-state parameters.  Added reject/abort codes.
 * 
 * Revision 2.8  88/01/19  09:56:55  mattes
 * Survive EINTR on input
 * 
 * Revision 2.7  87/09/28  11:54:26  mattes
 * Added argument to prompt(); generalized box name
 * 
 * Revision 2.6  87/06/10  18:12:05  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.5  86/12/22  17:12:44  parker
 * added new name server parms
 * 
 * Revision 2.4  86/12/08  12:18:34  parker
 * R2.1 NA parameter changes
 * 
 * Revision 2.3  86/11/14  17:06:03  harris
 * Include files for XENIX and UMAX V.
 * 
 * Revision 2.2  86/02/27  19:06:08  goodmon
 * Standardized string lengths.
 * 
 * Revision 2.1  86/02/26  12:04:24  goodmon
 * Changed configuration files to script files.
 * 
 * Revision 2.0  86/02/20  15:17:45  parker
 * First development revision for Release 2
 * 
 * Revision 1.7  86/02/18  14:22:50  goodmon
 * Removed case-insensitive changes.
 * 
 * Revision 1.6  86/02/05  17:47:37  goodmon
 * Made na input case-insensitive.
 * 
 * Revision 1.5  86/01/10  12:17:07  goodmon
 * New improved error messages.
 * 
 * Revision 1.4  85/12/12  11:46:18  goodmon
 * Modified to allow hyphens in annex names and to complain if no
 * parameter list is given to the set command.
 * 
 * Revision 1.3  85/12/06  16:16:29  goodmon
 * Added port_mode parameter value 'adaptive' and character parameter
 * value minimum uniqueness.
 * 
 * Revision 1.2  85/12/05  18:23:47  goodmon
 * Added ^c handling and fixed port_multiplexing bug.
 * 
 * Revision 1.1  85/11/01  17:41:39  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:11:03 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/sub.c,v 1.3 1996/10/04 12:11:03 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <strings.h>

#include <setjmp.h>
#include <stdio.h>
#include <errno.h>
#include "na.h"
#include "../netadm/netadm_err.h"

/*
 *	External Definitions
 */
extern int errno;

/*
 *	Defines and Macros
 */


/*
 *	Structure Definitions
 */


/*
 *	Forward Routine Definitions
 */


/*
 *	Global Data Declarations
 */


/*
 *	Static Declarations
 */
static char *netadm_errs[] = {
	"no errors",				/* NAE_SUCC */
	"unsupported address family",		/* NAE_ADDR */
	"erpc timeout",				/* NAE_TIME */
#if TLI
	"tli error",				/* NAE_SOCK init_socket.c */
#else
	"socket error",				/* NAE_SOCK */
#endif
	"read_memory count too large",		/* NAE_CNT */
	"read_memory response too short",	/* NAE_SRES */
	"incorrect parameter type",		/* NAE_TYPE */
	"unsupported response type",		/* NAE_RTYP */
	"invalid courier response",		/* NAE_CTYP */
	"erpc reject; details unknown",		/* NAE_REJ */
	"erpc reject; invalid program number",	/* NAE_PROG */
	"erpc reject; invalid version number",	/* NAE_VER */
	"erpc reject; invalid procedure number",/* NAE_PROC */
	"erpc reject; invalid argument",	/* NAE_ARG */
	"erpc reject; wrong password",		/* NAE_SREJECT */
	"erpc reject; must use SRPC protocol",	/* NAE_SESSION */
	"erpc message abort; details unknown",	/* NAE_ABT */
	"erpc abort; invalid parameter type",	/* NAE_PTYP */
	"erpc abort; invalid parameter count",	/* NAE_PCNT */
	"erpc abort; invalid parameter value",	/* NAE_PVAL */
	"erpc abort; eeprom write error",	/* NAE_E2WR */
	"erpc abort; too many sessions",	/* NAE_RSRC */
	"srpc error; session aborted for unknown reasons", /* NAE_SABORT */
	"does not support program",		/* NAE_NOANXSUP */
	"srpc abort; bad device requested",	/* NAE_BADDEV */
	"srpc abort; internal error"		/* NAE_INTERNAL */
};



prompt(string, arg, empty_ok)

	char string[];
	char *arg;
	int  empty_ok;

{
	int	cmd_cnt,
		read_cnt;
	char	*cmd_p;

	/* Print the prompt string and wait for a response.  If empty_ok
	   is FALSE, repeat until the response is non-empty. */

	do
	    {
	    if (!script_input)
		{
		if (arg == NULLSP)
		    printf("%s: ", string);
		else
		    {
		    printf(string, arg);
		    printf(": ");
		    }
		}

	    cmd_p = command_line;
	    cmd_cnt = sizeof(command_line);
	    while(1)
		{
		if (!fgets(cmd_p, cmd_cnt, cmd_file))
		    {
		    if(ferror(cmd_file) && (errno == EINTR))
			continue;
		    if (!script_input)
			exit(0);
		    else
			{
			eos = TRUE;
			return;
			}
		    }
		read_cnt = strlen(cmd_p);
		cmd_cnt -= read_cnt;
		cmd_p += read_cnt;
	        if(cmd_p == command_line || *(cmd_p - 2) != '\\')
			break;

		*(--cmd_p - 1) = '\n'; /* remove backslash if before newline */
		}
	    Psymbol = command_line;
	    eos = FALSE;
	    } while(lex() == LEX_EOS && !empty_ok);

}	/* prompt() */



int lex()

{
	/* Take the next symbol from the command line. */

	if (eos)
	    return LEX_EOS;

	symbol_length = 0;

	/* Skip white space. */

	skip_white_space();

	if (!*Psymbol)
	    {
	    eos = TRUE;
	    return LEX_EOS;
	    };

	/* Copy the symbol into "symbol"; advance Psymbol; count the symbol's
	   length. */

	if (*Psymbol == '"')
	    {
            int in_escape = 0;
	    Psymbol++;
	    while (*Psymbol)
		if (in_escape) {
		    symbol[symbol_length++] = *Psymbol++;
		    in_escape = 0;
		} else if (*Psymbol == '\\') {
		    symbol[symbol_length++] = *Psymbol++;
		    in_escape = 1;
		} else if (*Psymbol == '"') 
		    break;
                else
		    symbol[symbol_length++] = *Psymbol++;
	    if (*Psymbol)
	        Psymbol++;
	    else
		punt("missing closing delimiter", (char *)NULL);
	    }
	else
	    if (*Psymbol && index(PUNCTUATION, *Psymbol))
	        symbol[symbol_length++] = *Psymbol++;
	    else
	        while (*Psymbol && !index(SEPARATORS, *Psymbol))
	            symbol[symbol_length++] = *Psymbol++;

	symbol[symbol_length] = '\0';

	return LEX_OK;

}	/* lex() */

/*
 *	returns the next switch charaxter in the input or LEX_EOSW
 *	if none are left.  A switch is "-abcd" or -a -b -cd.  The
 *	next switch is returned in symbol.  The inswitch flag tells
 *	if we are in a group of switch chars or we are looking for
 *	a new one.
*/

int lex_switch()

{
	int looking = TRUE;

	/* Take the next symbol from the command line. */

	if (eos)
	    return (LEX_EOS);

	symbol_length = 0;
	while(looking)
	    if (!inswitch){
	        skip_white_space();

       	        if (*Psymbol == '-'){
		    inswitch = TRUE;
	    	    Psymbol++;
		    symbol[symbol_length++] = *Psymbol++;
		    looking = FALSE;
	        }
		else {
		    if(*Psymbol == '\0')
			return (LEX_EOS);
		    break;
		}
	    }
	    else
	       if(*Psymbol && !index(SEPARATORS, *Psymbol)){
	           symbol[symbol_length++] = *Psymbol++;
		   looking = FALSE;
	       }
	       else 
		   inswitch = FALSE;

	symbol[symbol_length] = '\0';

	return (looking ? LEX_EOSW : LEX_OK);

}	/* lex_switch() */

match_flag(string, falseval, trueval, message, defalt)

char	*string,		/* string to be looked up */
	*falseval,		/* string representing bit not set */
	*trueval,		/* string representing bit is set */
	*message;		/* error message to use if no match */

int	defalt;		/* value to return if string is "default" */
{
	int	value;		/* final return value */
	char	*table[4];	/* table */

	/* create table for match() to use */

	table[0] = falseval;
	table[1] = trueval;
	table[2] = "default";
	table[3] = (char *)NULL;

	value = match(string, table, message);

	/* if we matched "default", return the given default value */

	if(value == 2)
	  value = defalt;

	return value;
}


match(string, table, error_string)

	char *string,
	     *table[],
	     *error_string;

{
	int  loop,
	     string_length,
	     match_count = 0,
	     match_index;
	char error_msg[80];
	char test_delim[80];
	char *start, *delim;
	char alias = FALSE;

	/* Match the string to a spelling in the given lex table.  If a match
	   is found, return its index; if not, punt. */

	string_length = strlen(string);

	for (loop = 0; table[loop]; loop++) {
	    strcpy(test_delim, table[loop]);
	    start = test_delim;
	    alias = FALSE;
	    while (delim = index(start, ',')) {
		*delim = '\0';
	        alias = TRUE;
		if (strncasecmp(string, start, string_length) == 0) {
		    match_index = loop;
		    if (string_length == strlen(start)) {
			match_count = 1;
			goto found_exact;
		    }
		    else
			match_count++;
		}
		*delim++ = ',';
		start = delim;
		alias = FALSE;
	    }
	    if (strncasecmp(string, start, string_length) == 0) {
		match_index = loop;
		if (string_length == strlen(start)) {
		    match_count = 1;
		    goto found_exact;
		} else {
		    if (alias == FALSE)
		        match_count++;
		}
	    }
	}

found_exact:
	if (match_count == 0)
	    {
	    if (error_string == NULL)
		return -1;
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, error_string);
	    (void)strcat(error_msg, ": ");
	    (void)strcat(error_msg, string);

	    punt("invalid ", error_msg);
	    }
	else if (match_count > 1)
	    punt("non-unique symbol: ", string);

	return match_index;

}	/* match() */



in_table(table)

	char *table[];

{
	int loop;

	/* Match the current symbol to a spelling in the given lex table.
	   If a match is found, return TRUE; if not, return FALSE. */

	for (loop = 0; table[loop]; loop++)
	    if (strncasecmp(symbol, table[loop], symbol_length) == 0)
	        return TRUE;

	return FALSE;

}	/* in_table() */



punt(string1, string2)

	char *string1,
	     *string2;

{

	/* This status will be returned by main(). */
	status = 1;

	/* Print the error text, then long-jump back to the last prompt. */

	if (string1)
	    printf("\t%s", string1);

	if (string2)
	    printf("%s", string2);

	if (prompt_mode)
	    {
	    if (string1 || string2);
	    printf("\n");

	    longjmp(prompt_env, 1);
	    }
	else
	    longjmp(cmd_env, 1);

}	/* punt */



skip_white_space()

{
	/* Advance the command-line pointer to the next non-white character. */

	while (*Psymbol && index(WHITE_SPACE, *Psymbol))
	    Psymbol++;

}	/* skip_white_space() */

static void
print_netadm_error(ename,error)
char *ename;
int error;
{
	printf("%s:  %s\n",ename,
		(error >= 0 && error <
		 sizeof(netadm_errs)/sizeof(netadm_errs[0])) ?
			netadm_errs[error] :
			"internal error; unsupported error code"
		);
}

void
netadm_error(error)
int error;
{
	print_netadm_error("Netadm error",error);
	punt((char *)0,(char *)0);
}

void
token_error(error)
int error;
{
	if (error == NAE_TIME)
		printf("%s:  Not responding\n",symbol);
	else
		print_netadm_error(symbol,error);
}
