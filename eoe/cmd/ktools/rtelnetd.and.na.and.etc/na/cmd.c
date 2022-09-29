/*
 *****************************************************************************
 *
 *        Copyright 1989,1990, Xylogics, Inc.  ALL RIGHTS RESERVED.
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
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/cmd.c,v $
 *
 * Revision History:
 *
 * $Log: cmd.c,v $
 * Revision 1.3  1996/10/04 12:10:02  cwilson
 * latest rev
 *
 * Revision 3.46  1994/05/24  09:27:40  russ
 * Bigbird na/adm merge.
 *
 * Revision 3.45.2.6  1994/03/11  15:57:06  russ
 * the specified sync list on show was not being used.
 *
 * Revision 3.45.2.5  1994/03/07  13:10:27  russ
 * removed all X25 trunk code.
 *
 * Revision 3.45.2.4  1994/03/04  16:48:56  russ
 * don't allow attempted read or write command if not superuser.
 * don't allow attempted read if no default annexs.
 *
 * Revision 3.45.2.3  1994/02/14  09:23:21  russ
 * Added categories to the display list.
 *
 * Revision 3.45.2.2  1994/02/04  16:55:25  russ
 * Added box port and sync categories.
 *
 * Revision 3.45.2.1  1994/01/06  15:24:42  wang
 * Bigbird NA/Admin params support.
 *
 * Revision 3.45  1993/10/06  14:44:10  bullock
 * spr.2125 - Compile error caused by missing " when trying to
 * escape out the printing of "'s using \ character.
 *
 * Revision 3.44  1993/07/30  10:38:14  carlson
 * SPR 1923 -- rewrote pager to use file descriptor instead of file pointer.
 *
 * Revision 3.43  1993/06/10  11:42:19  wang
 * Added support for "copy interface" command and Fixed spr1715.
 * Reviewed by Thuan Tran.
 *
 * Revision 3.42  93/06/07  11:08:39  bullock
 * spr.1658 - in boot_sub allow combination of -a and -d switches
 * 
 * Revision 3.41  93/05/20  17:58:14  carlson
 * Fixed help code to search on aliases, retained more error codes,
 * and added more reliable messages (SPR 1167).
 * Reviewed by David Wang.
 * 
 * Revision 3.40  93/05/12  15:59:57  carlson
 * R7.1 -- Merged cases and added support for negative usage markers.
 * 
 * Revision 3.39  93/03/30  11:00:14  raison
 * do not display help for annex parms that are void_cat
 * 
 * Revision 3.38  93/02/24  13:15:56  carlson
 * Removed pager conditionals -- now taken care of in inc/na.h.
 * 
 * Revision 3.37  93/02/22  17:47:19  wang
 * Added reset interface command for NA.
 * 
 * Revision 3.36  93/01/22  18:14:32  wang
 * New parameters support for Rel8.0
 * 
 * Revision 3.35  92/08/09  14:19:07  emond
 * Removed a pile of rcslogs.
 * 
 * Revision 3.34  92/02/18  17:50:01  sasson
 * Added support for 3 LAT parameters(authorized_group, vcli_group, lat_q_max).
 * 
 * Revision 3.33  92/02/03  10:41:16  carlson
 * Fixed bad filename length error messages -- all should match actual
 * restriction used.
 * 
 * Revision 3.32  92/01/31  22:48:24  raison
 * change printer to have printer_set and added printer_speed
 *
 * Revision 3.31  91/12/23  11:20:44  raison
 * oops, bug added, now removed
 * 
 * Revision 3.30  91/12/20  15:42:05  raison
 * added flash as a new type of load_dump_seq
 * 
 * Revision 3.29  91/12/12  14:17:06  russ
 * un-include oem.h.
 * 
 * Revision 3.28  91/11/05  10:38:45  carlson
 * SPR 396 -- respect maximum string lengths in boot parameters.
 * 
 * Revision 3.27  91/10/14  16:50:35  russ
 * Added pager code.
 * 
 * Revision 3.26  91/08/22  11:37:29  raison
 * fixed added for erroneous free of default port set - correction from
 * A. Barnett.
 * 
 * Revision 3.25  91/07/12  15:29:46  sasson
 * Added code to adm_help_cmd() to ignore obsolete port parameters.
 *
 *
 * Removed a pile of rcs logs from here.
 *
 * 
 * Revision 1.2  85/11/15  17:57:25  parker
 * added port_multiplex parameter.
 * 
 * Revision 1.1  85/11/01  17:40:07  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:10:02 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/cmd.c,v 1.3 1996/10/04 12:10:02 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <strings.h>

#include <stdio.h>
#include "na.h"
#include "../inc/erpc/netadmp.h"
#include "cmd.h"
#include "help.h"

extern char *split_string();	/* in do.c */


adm_box_cmd()
{
	/* Assign the default annex list. */

	ANNEX_LIST *Ptemp_annex_list = NULL,
	           *Ptemp_annex_tail = NULL;

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\tenter default %s list", BOX, FALSE);
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Free the temp annex list in case you came back here after
	   punting. */
	free_annex_list(&Ptemp_annex_list);

	/* Let a temp point at the annex list in case the human makes a
	   mistake, so that the old default will still be there after
	   punting. */
	annex_list(&Ptemp_annex_list, &Ptemp_annex_tail);

	/* No human mistakes, so move the temp annex list to the default
	   annex list.  Free the default first in case it had been
	   previously entered. */
	free_annex_list(&Pdef_annex_list);
	Pdef_annex_list = Ptemp_annex_list;
	Pdef_annex_tail = Ptemp_annex_tail;

}	/* adm_box_cmd() */



adm_boot_cmd()
{
	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	/* FALSE argument to boot_sub() means no dump. */
	boot_sub(FALSE);

}	/* adm_boot_cmd() */



/*
 *****************************************************************************
 *
 * Function Name:
 *	boot_sub
 *
 * Functional Description:
 * Used to implement both the boot and dumpboot coommands.
 * If no arguments were supplied, set prompt_mode so that they
 * will be prompted for. The syntax for the boot and dumpboot
 * command is :
 * [dump]boot [-a][-h][-d][-q] <time> <annex_list> <filename> <warning>
 *
 *  where -a will abort any boots
 *        -h will cause a halt or reset diag
 *        -d will force an upline dump
 *        -q will make the dumps quiet, send no warnings
 *
 * Parameters:
 *	dump true if we are called from dumpboot.
 *
 * Return Value:
 *       none
 *
 * Exceptions:
 *	Switches must be used to modify the command they are not prompted
 *      for during prompt mode.
 *
 *****************************************************************************
 */

boot_sub(dump)
int	dump;

{
    char 	filename[FILENAME_LENGTH + 1], 
		warning[WARNING_LENGTH + 1];
    short int 	switch_mode = LEX_OK, 
		switches = 0;
    time_t	boot_time = 0;


    if(dump)
	switches|=SWDUMP;

    prompt_mode = lex(); 

    if ((prompt_mode == LEX_OK) && (inswitch = *symbol == '-')) {
	while ((switch_mode = lex_switch()) == LEX_OK) {

	    switch (*symbol) {

		case 'a' : switches |= SWABORT; break;

		case 'd' : switches |= SWDUMP; break;

		case 'h' : switches |= SWDIAG; break;

		case 'q' : switches |= SWQUIET; break;

		case 'l' : switches |= SWFLASH; break;

		default : punt("unknown switch",(char *)NULL);
	    }
	}
	/* 
	 * legal switch combos are: 
	 * -a alone or -d or -h
	 * any combo of 2 or more of -l, -d, and/or -h
	 * -q or -t can modify -d,-h,or standalone
         * -a and -d (dumpboot -a)
	 */
	if (((switches & SWABORT) && (switches != SWABORT && 
              switches != (SWDUMP|SWABORT))) || 
	    ((switches & SWDUMP) && (switches & SWDIAG)) ||
	    ((switches & SWDUMP) && (switches & SWFLASH)) ||
	    ((switches & SWFLASH) && (switches & SWDIAG))) 
	    punt("bad switch combinations",(char *)NULL);

        if (!(switches & SWABORT)) {
	    (void) lex();
        }
    }

    /*
     * If scripted input an no more arg data... punt.
     */
    prompt_mode = (prompt_mode == LEX_EOS) || (switch_mode == LEX_EOS);
    if ((prompt_mode && script_input))
	punt("missing arguments", (char *)NULL);

    /*
     * Get time.
     */
    if (!(switches & SWABORT)) {
	if (prompt_mode && !script_input) {
	    (void)setjmp(prompt_env);
	    prompt("\ttime (return for `now')",NULLSP, TRUE);
	}
	if(symbol_length > 0) {
	    boot_time = delay_time(FALSE);	
	    if (boot_time == BADTIME)
		punt("bad time format", (char *)NULL);
	    if (boot_time > 0)
		switches |= SWDELAY;
	}
    }

    /*
     * Get annex list.
     */
    if (!prompt_mode)
	prompt_mode = lex();

    if (prompt_mode && !script_input) {
	/* ask for the annex list */
	(void)setjmp(prompt_env);
	prompt("\t%s list (return for default)", BOX, TRUE);
    }

    free_annex_list(&Pspec_annex_list);
    if (eos) {
	if (!Pdef_annex_list)
	    punt(NO_BOXES, (char *)NULL);
    }
    else {
	annex_list(&Pspec_annex_list,&Pspec_annex_tail);
	/*
	 * lex got called in the bowels of annex_list here.
	 * so do the following hack.
	 */
	prompt_mode = eos;
    }

    /*
     * Get file name.
     */
    if (!(switches & SWABORT)) {
	filename[0] = '\0';
	if (prompt_mode && !script_input) {
	    (void)setjmp(prompt_env); 
	    prompt("\tfilename (return for default)", NULLSP, TRUE);
	}
	lex_string();

	if (symbol_length > FILENAME_LENGTH)
	    punt(LONG_FILENAME, (char *)NULL);
	if (symbol_length > 0)
	    (void)strcpy(filename, symbol);
	symbol_length = 0;   /* used this symbol for filename */
    }

    /*
     * prompt for warning message maxlength = 250.
     */
    if (!(switches & SWQUIET)){
	if (!prompt_mode && !(switches & SWABORT))
	    prompt_mode = lex();
	if (prompt_mode && !script_input) {
	    (void)setjmp(prompt_env); 
	    if (switches & SWABORT)
		prompt("\tabort message (return for none)", NULLSP, TRUE);
	    else
		prompt("\twarning (return for none)", NULLSP, TRUE);
	}
	if (eos)
	    warning[0] = '\0';
	else
	    warning_message(warning);
    }
    else
	warning[0] = '\0';

    (void) lex();

    /*
     *  Turn off prompt_mode so that subsequent errors will be punted
     *  back to the command prompt. 
     */
    prompt_mode = FALSE;

    /* 
     *  If an annex list was specified, boot those annexes; otherwise,
     *  boot the annexes in the default annex list. 
     */

    if (Pspec_annex_list)
	do_boot(filename,Pspec_annex_list,boot_time,switches,warning);
    else
	if (Pdef_annex_list)
	    do_boot(filename,Pdef_annex_list,boot_time,switches,warning);
	else
	    punt(NO_BOXES, (char *)NULL);

}	/* boot_sub() */



adm_broadcast_cmd()

{
	struct
	    {
	    unsigned short length;
	    char           string[MSG_LENGTH + 2];
	    }              courier_text;

	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	/* Process the arguments. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);

	    if (Pspec_port_set == Pdef_port_set) {
		Pspec_port_set = NULL;
		Pspec_port_tail = NULL;
	    } else {
	        free_port_set(&Pspec_port_set);
	    }

	    prompt("\tenter port_set (hit return for default)", NULLSP, TRUE);

	    if (eos)
	        {
	        (void)lex();
	        if (!Pdef_port_set)
	            punt("default ports have not been specified",
	             (char *)NULL);
	        }
	    else
	        port_set(&Pspec_port_set, &Pspec_port_tail, VIRTUAL_OK);

	    (void)setjmp(prompt_env);

	    prompt("\tenter message", NULLSP, FALSE);
	    }
	else
	    {
	    if (Pspec_port_set == Pdef_port_set) {
		Pspec_port_set = NULL;
		Pspec_port_tail = NULL;
	    } else {
	        free_port_set(&Pspec_port_set);
	    }

	    if (symbol_length == 1 && symbol[0] == '=')
	        {
	        (void)lex();

	        if (eos)
	            punt("missing port identifier", (char *)NULL);
	        else
	            port_set(&Pspec_port_set, &Pspec_port_tail, VIRTUAL_OK);
	        }

	    if (eos)
	        punt("missing message", (char *)NULL);
	    }

	/* Parse the message.  Put it into a courier-style string with
	   a length word so that do_broadcast() doesn't have to copy it
	   into one later. */

	message(courier_text.string);

	courier_text.length = strlen(courier_text.string);

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Perform the broadcast. */

	if (Pspec_port_set)
	    do_broadcast(Pspec_port_set, (char *)&courier_text);
	else
	    if (Pdef_port_set)
	        do_broadcast(Pdef_port_set, (char *)&courier_text);
	    else
	        punt("default ports have not been specified", (char *)NULL);

}	/* adm_broadcast_cmd() */



adm_comment_cmd()

{
	/* Ignore the rest of the line. */

	eos = TRUE;

}	/* adm_comment_cmd() */



adm_copy_cmd()

{

	ANNEX_ID	   source_annex_id;
	int                source_is_printer;
	unsigned short     source_port_number;
	unsigned short     source_sync_number;
	unsigned short     source_printer_number;
	unsigned short     source_interface_number;

	int category;

	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	/* Process the arguments. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);

	    prompt("\tenter \"%s\", \"port\",\"synchronous\", or \"printer\", \"interface\"",
                   BOX, FALSE);
	    category = match(symbol, param_classes, "copy argument");

	    (void)setjmp(prompt_env);

	    switch (category)
		{
		case BOX_CLASS:
	            prompt("\tenter source %s", BOX, FALSE);

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
			annex_name(&source_annex_id, (char *)NULL, 0);
		    break;

		case PRINTER_CLASS:
	            prompt("\tenter source printer", NULLSP, FALSE);

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_printer(&source_printer_number,&source_annex_id);
		    break;

		case PORT_CLASS:
	            prompt("\tenter source port", NULLSP, FALSE);

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_port(&source_port_number, &source_annex_id);
		    break;

		case INTERFACE_CLASS:
	            prompt("\tenter source interface", NULLSP, FALSE);

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_interface(&source_interface_number, &source_annex_id);
		    break;

		case SYNC_CLASS:
	            prompt("\tenter source synchronous port", NULLSP, FALSE);

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_sync(&source_sync_number, &source_annex_id);
		    break;

		} /* end switch */

	    (void)setjmp(prompt_env);

	    /* case off the next appropriate prompt */
	    switch (category)
	       {
		case BOX_CLASS:
		    free_annex_list(&Pspec_annex_list);
		    prompt("\tenter destination %s (hit return for default)", 
			   BOX, TRUE);
		    break;

		case PRINTER_CLASS:
	            if (Pspec_printer_set == Pdef_printer_set) {
		        Pspec_printer_set = NULL;
		        Pspec_printer_tail = NULL;
	            } else {
	                free_printer_set(&Pspec_printer_set);
	            }
		    prompt("\tenter destination printer_set (hit return for default)", NULLSP, TRUE);
		    break;

		case PORT_CLASS:
	            if (Pspec_port_set == Pdef_port_set) {
		        Pspec_port_set = NULL;
		        Pspec_port_tail = NULL;
	            } else {
	                free_port_set(&Pspec_port_set);
	            }
		    prompt("\tenter destination port_set (hit return for default)", NULLSP, TRUE);
		    break;

		case INTERFACE_CLASS:
	            if (Pspec_interface_set == Pdef_interface_set) {
		        Pspec_interface_set = NULL;
		        Pspec_interface_tail = NULL;
	            } else {
	                free_interface_set(&Pspec_interface_set);
	            }
		    prompt("\tenter destination interface_set (hit return for default)", NULLSP, TRUE);
		    break;

		case SYNC_CLASS:
	            if (Pspec_sync_set == Pdef_sync_set) {
		        Pspec_sync_set = NULL;
		        Pspec_sync_tail = NULL;
	            } else {
	                free_sync_set(&Pspec_sync_set);
	            }
		    prompt("\tenter destination sync_set (hit return for default)", NULLSP, TRUE);
		    break;

	       }

	    }
	else  
	    { 

	    /* Parse source args off command line, no prompting here */

	    category = match(symbol, param_classes, "copy argument");
	    (void)lex();

	    switch (category)
	        {
		case BOX_CLASS:
	            free_annex_list(&Pspec_annex_list);

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
			annex_name(&source_annex_id, (char *)NULL, 0);

	            break;

		case PRINTER_CLASS:
	            if (Pspec_printer_set == Pdef_printer_set) {
		        Pspec_printer_set = NULL;
		        Pspec_printer_tail = NULL;
	            } else {
	                free_printer_set(&Pspec_printer_set);
	            }

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_printer(&source_printer_number,&source_annex_id);

	            break;

		case PORT_CLASS:
	            if (Pspec_port_set == Pdef_port_set) {
		        Pspec_port_set = NULL;
		        Pspec_port_tail = NULL;
	            } else {
	                free_port_set(&Pspec_port_set);
	            }

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_port(&source_port_number, &source_annex_id);

	            break;

		case SYNC_CLASS:
	            if (Pspec_sync_set == Pdef_sync_set) {
		        Pspec_sync_set = NULL;
		        Pspec_sync_tail = NULL;
	            } else {
	                free_sync_set(&Pspec_sync_set);
	            }

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_sync(&source_sync_number, &source_annex_id);

	            break;

		case INTERFACE_CLASS:
	            if (Pspec_interface_set == Pdef_interface_set) {
		        Pspec_interface_set = NULL;
		        Pspec_interface_tail = NULL;
	            } else {
	                free_interface_set(&Pspec_interface_set);
	            }

		    if (eos)
		        punt(NO_BOXES, (char *)NULL);
		    else
	       		single_interface(&source_interface_number, &source_annex_id);

	            break;

		}	/* switch (category) */

	        if (eos)
	            punt("missing destination list", (char *)NULL);
	    }

	/*
	 * Turn off prompt_mode so that subsequent errors will be punted
	 * back to the command prompt. 
	 */
	prompt_mode = FALSE;

	/* Parse the destination list and perform the copy operations. */

	switch (category)
	    {
	    case BOX_CLASS:
		/* Get the destination annex list */
	        if (eos)
		    {
		    (void)lex();
		    if (!Pdef_annex_list)
			punt(NO_BOXES, (char *)NULL);
		    }
	        else
		    annex_list(&Pspec_annex_list, &Pspec_annex_tail);

		/* Do the annex parameter copy */
	        if (Pspec_annex_list)
	            do_copy_annex(&source_annex_id, Pspec_annex_list);
	        else
	            if (Pdef_annex_list)
		        do_copy_annex(&source_annex_id, Pdef_annex_list);

	        break;

	    case PORT_CLASS:
		/* Parse off the destination ports to be copied to */
	        if (eos)
		    {
		    (void)lex();
		    if (!Pdef_port_set)
			punt("default ports have not been specified",
			 (char *)NULL);
		    }
	        else
		    port_set(&Pspec_port_set, &Pspec_port_tail, VIRTUAL_NOT_OK);

		/* Do the copy now */
	        if (Pspec_port_set)
	            do_copy(source_port_number, &source_annex_id, 
			    Pspec_port_set);
	        else
	            if (Pdef_port_set)
		        do_copy(source_port_number, &source_annex_id,
		         Pdef_port_set);
	        break;

	    case SYNC_CLASS:
		/* Parse off the destination sync ports to be copied to */
	        if (eos)
		    {
		    (void)lex();
		    if (!Pdef_sync_set)
			punt("default sync ports have not been specified",
			 (char *)NULL);
		    }
	        else
		    sync_set(&Pspec_sync_set, &Pspec_sync_tail );

		/* Do the copy now */
	        if (Pspec_sync_set)
	            do_copy_sync(source_sync_number, &source_annex_id, 
			    Pspec_sync_set);
	        else
	            if (Pdef_sync_set)
		        do_copy_sync(source_sync_number, &source_annex_id,
		         Pdef_sync_set);
	        break;

	    case INTERFACE_CLASS:
		/* Parse off the destination interfaces to be copied to */
	        if (eos)
		    {
		    (void)lex();
		    if (!Pdef_interface_set)
			punt("default interfaces have not been specified",
			 (char *)NULL);
		    }
	        else
		    interface_set(&Pspec_interface_set, &Pspec_interface_tail,  VIRTUAL_NOT_OK);

		/* Do the copy now */
	        if (Pspec_interface_set)
	            interface_copy(source_interface_number, &source_annex_id, 
			    Pspec_interface_set);
	        else
	            if (Pdef_interface_set)
		       interface_copy(source_interface_number, &source_annex_id,
		         Pdef_interface_set);
	        break;

	    case PRINTER_CLASS:
		/* Parse off the destination printers to be copied to */
	        if (eos)
		    {
		    (void)lex();
		    if (!Pdef_printer_set)
			punt("default printers have not been specified",
			 (char *)NULL);
		    }
	        else
		    printer_set(&Pspec_printer_set, &Pspec_printer_tail);

		/* Do the copy now */
	        if (Pspec_printer_set)
	            printer_copy(source_printer_number, &source_annex_id, 
			    Pspec_printer_set);
	        else
	            if (Pdef_printer_set)
		        printer_copy(source_printer_number, &source_annex_id,
		         Pdef_printer_set);
	        break;

	    }


}	/* adm_copy_cmd() */

adm_dumpboot_cmd()

{
	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	/* TRUE argument to boot_sub() means dump is requested. */

	boot_sub(TRUE);

}	/* adm_dumpboot_cmd() */



adm_echo_cmd()

{
	/* Echo the rest of the line to standard output. */

	skip_white_space();

	printf("%s\n", Psymbol);

	eos = TRUE;

}	/* adm_echo_cmd() */

int
do_sub_help_match(str,symlen)
char *str;
int symlen;
{
	char *cp;
	int idx = 1;

	while (cp = split_string(str,idx)) {
		if (strncmp(symbol,cp,symlen) == 0)
			return 1;
		idx++;
	}
	return 0;
}

adm_help_cmd()

{
	int help_nr,usage;
	int symlen;
	int found = 0;

	(void)lex();

	open_pager();
	if (eos)
	{
	    printf("\ncommands are:\n");
	    for (help_nr = 0; cmd_string[help_nr]; help_nr++)
	        printf("\t%s\n", cmd_string[help_nr]);
	}
	else
	{
	    while(!eos)
	    {
		symlen = strlen(symbol);
		for (help_nr = 0; D_key(help_nr) != NULL; help_nr++) {
		    usage = D_usage(help_nr);
		    if (usage < 0)
			usage = -usage-1;

		    /* ignore obsolete port parameters */
		    if ((usage == PORT_PARAM || usage == PORT_CATEGORY) &&
			Sp_category(D_index(help_nr)) == VOID_CAT)
			continue;

		    /* ignore obsolete annex parameters */
		    if ((usage == BOX_PARAM || usage == BOX_CATEGORY) &&
			Ap_category(D_index(help_nr)) == VOID_CAT)
			continue;

		    if ((symlen == 1 && symbol[0] == '*') ||
		        strncmp(symbol,D_key(help_nr),symlen) == 0 ||
			do_sub_help_match(D_key(help_nr),symlen)) {
			found++;
			printf("\n\t%s (%s):\n", D_key(help_nr),
			    usage_table[usage]);
			printf("\t%s\n", D_text(help_nr));
		    }
		}
		if (found == 0)
		    printf("\n\tNo help found for \"%s\"\n",
			symbol);
		(void)lex();
	    }
	}
	printf("\n");
	close_pager();
}	/* adm_help_cmd() */


adm_password_cmd()
{
	char	*pass;
	char	nullstring = 0;

	(void)lex();

	if (eos)			/* prompt if no argument */
	{
	    if (script_input)
		pass = &nullstring;
	    else
		pass = get_password((struct in_addr *)0);
	}

	else				/* otherwise get it */
	{
	    (void)lex();
	    pass = symbol;		/* from command line */
	}

	set_global_password(pass);

}	/* adm_password_cmd() */


adm_port_cmd()

{
	PORT_SET *Ptemp_port_set = NULL,
	         *Ptemp_port_tail = NULL;

	/* Assign the default port set. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\tenter default port set", NULLSP, FALSE);
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Free the temp port set in case you came back here after punting. */
	free_port_set(&Ptemp_port_set);

	/* Let a temp point at the port set in case the human makes a mistake,
	   so that the old default will still be there after punting. */
	port_set(&Ptemp_port_set, &Ptemp_port_tail, VIRTUAL_NOT_OK);

	/* No human mistakes, so move the temp port set to the default
	   port set.  Free the default first in case it had been previously
	   entered. */
	free_port_set(&Pdef_port_set);
	Pdef_port_set = Ptemp_port_set;
	Pdef_port_tail = Ptemp_port_tail;

}	/* adm_port_cmd() */

adm_sync_cmd()

{
	SYNC_SET *Ptemp_sync_set = NULL,
	         *Ptemp_sync_tail = NULL;

	/* Assign the default sync set. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\tenter default sync set", NULLSP, FALSE);
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Free the temp sync set in case you came back here after punting. */
	free_sync_set(&Ptemp_sync_set);

	/* Let a temp point at the sync set in case the human makes a mistake,
	   so that the old default will still be there after punting. */
	sync_set(&Ptemp_sync_set, &Ptemp_sync_tail);

	/* No human mistakes, so move the temp sync set to the default
	   sync set.  Free the default first in case it had been previously
	   entered. */
	free_sync_set(&Pdef_sync_set);
	Pdef_sync_set = Ptemp_sync_set;
	Pdef_sync_tail = Ptemp_sync_tail;

}	/* adm_sync_cmd() */


adm_interface_cmd()

{
	INTERFACE_SET *Ptemp_interface_set = NULL,
	         *Ptemp_interface_tail = NULL;

	/* Assign the default interface set. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\tenter default interface set", NULLSP, FALSE);
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Free the temp interface set */
	free_interface_set(&Ptemp_interface_set);

	/* Let a temp point at the interface set in case he makes a mistake,
	   so that the old default will still be there after punting. */
	/* Free the temp interface set */
	interface_set(&Ptemp_interface_set, &Ptemp_interface_tail);

	/* No human mistakes, so move the temp interface set to the default
	   interface set.  Free the default first in case it had been previously
	   entered. */
	free_interface_set(&Pdef_interface_set);
	Pdef_interface_set = Ptemp_interface_set;
	Pdef_interface_tail = Ptemp_interface_tail;

}	/* adm_interface_cmd() */

adm_quit_cmd()

{

	/* Terminate the main loop (in main.c). */

	done = TRUE;
	eos = TRUE;

}	/* adm_quit_cmd() */



adm_read_cmd()

{
	char filename[FILENAME_LENGTH + 1];

	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	if (Pdef_annex_list == NULL)
	    punt(NO_BOXES, (char *)NULL);


	/* Process the arguments. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\tfilename", NULLSP, FALSE);
	    lex_string();
	    if (symbol_length > FILENAME_LENGTH)
		punt(LONG_FILENAME, (char *)NULL);
	    (void)strcpy(filename, symbol);
	    (void)lex();
	    }
	else
	    {
	    lex_string();
	    if (symbol_length > FILENAME_LENGTH)
		punt(LONG_FILENAME, (char *)NULL);
	    (void)strcpy(filename, symbol);
	    (void)lex();
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Perform the read operation. */

	do_read(filename);

}	/* adm_read_cmd() */


/* ====================================== */
/* Will not support trunk reset just yet! */
/* ====================================== */

adm_reset_cmd()

{
	int reset_box = FALSE;
	int reset_printer = FALSE;
	int reset_interface = FALSE;
	unsigned short length;

	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	/* Process the arguments. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\tenter \"%s\", printer or port set (return resets default ports)",
		   BOX, TRUE);

	    /*
	     * Some broken machines (NCR) implement string macros in assembly
	     * and don't allow nested assembler macro calls (gak!)
	     */
	    length = strlen(symbol);
	    if(!eos && !strncmp(symbol, BOX, length))
		{
		reset_box = TRUE;
		free_annex_list(&Pspec_annex_list);
	        (void)setjmp(prompt_env);
	        prompt("\tenter %s list (hit return for default)",
		       BOX, TRUE);

		if (eos)
		    {
		    (void)lex();
		    if (!Pdef_annex_list)
			punt(NO_BOXES, (char *)NULL);
		    }
		else
		    annex_list(&Pspec_annex_list, &Pspec_annex_tail);

	        prompt("\tenter reset subsystem list (hit return for all)",
	    		NULLSP, TRUE);
		}
	    else if(!strncmp(symbol, PRINTER, length))
		{
		(void)lex();
		reset_printer = TRUE;
	        if (Pspec_printer_set == Pdef_printer_set) {
		    Pspec_printer_set = NULL;
		    Pspec_printer_tail = NULL;
	        } else {
	            free_printer_set(&Pspec_printer_set);
	        }
		if (eos)
		    {
	            prompt("\tenter printer set (return resets default printer set)", (char *) 0, TRUE);
		    if (eos)
			{
		        if (!Pdef_printer_set)
			    punt("default printers have not been specified",
			             (char *)NULL);
			Pspec_printer_set = Pdef_printer_set;
			}
		    else
		        printer_set(&Pspec_printer_set,&Pspec_printer_tail);
		    }
		else
		    {
		    printer_set(&Pspec_printer_set, &Pspec_printer_tail);
		    }
		}
	    else if(!strncmp(symbol, INTERFACES, length))
		{
		(void)lex();
		reset_interface = TRUE;
	        if (Pspec_interface_set == Pdef_interface_set) {
		    Pspec_interface_set = NULL;
		    Pspec_interface_tail = NULL;
	        } else {
	            free_interface_set(&Pspec_interface_set);
	        }
		if (eos)
		    {
	            prompt("\tenter interface set (return resets default interface set)", (char *) 0, TRUE);
		    if (eos)
			{
		        if (!Pdef_interface_set)
			    punt("default interfaces have not been specified",
			             (char *)NULL);
			Pspec_interface_set = Pdef_interface_set;
			}
		    else
		        interface_set(&Pspec_interface_set,&Pspec_interface_tail);
		    }
		else
		    {
		    interface_set(&Pspec_interface_set, &Pspec_interface_tail);
		    }
		}
	    else
		{
	        if (Pspec_port_set == Pdef_port_set) {
		    Pspec_port_set = NULL;
		    Pspec_port_tail = NULL;
	        } else {
	            free_port_set(&Pspec_port_set);
	        }
		if (eos)
		    {
		    (void)lex();
		    if (!Pdef_port_set)
			punt("default ports have not been specified",
			     (char *)NULL);
		    else
			Pspec_port_set = Pdef_port_set;
		    }
		else
		    {
		    port_set(&Pspec_port_set, &Pspec_port_tail, VIRTUAL_OK);
		    }
		}
	    }
	else
	    {
	    /*
	     * Some broken machines (NCR) implement string macros in assembly
	     * and don't allow nested assembler macro calls (gak!)
	     */
	    length = strlen(symbol);
	    if(!strncmp(symbol, BOX, length))
	        {
		reset_box = TRUE;

	        (void)lex();

		free_annex_list(&Pspec_annex_list);

	        if (symbol_length == 1 && symbol[0] == '=')
		    {
		    (void)lex();

		    if (eos)
			punt(NO_BOX, (char *)NULL);
		    else
			annex_list(&Pspec_annex_list, &Pspec_annex_tail);
		    }
		}
	    else if(!strncmp(symbol, PRINTER, length))
		{
		reset_printer = TRUE;
		(void)lex();
	        if (Pspec_printer_set == Pdef_printer_set) {
		    Pspec_printer_set = NULL;
		    Pspec_printer_tail = NULL;
	        } else {
	            free_printer_set(&Pspec_printer_set);
	        }

		if (eos)
		    {
	            prompt("\tenter printer set (return resets default printer set)", (char *) 0, TRUE);
		    if (eos)
			{
		        if (!Pdef_printer_set)
			        punt("default printers have not been specified",
			             (char *)NULL);
			Pspec_printer_set = Pdef_printer_set;
			}
		    else
		        printer_set(&Pspec_printer_set,&Pspec_printer_tail);
		    }
		else
		    printer_set(&Pspec_printer_set, &Pspec_printer_tail);
		}
	    else if(!strncmp(symbol, INTERFACES, length))
		{
		reset_interface = TRUE;
		(void)lex();
	        if (Pspec_interface_set == Pdef_interface_set) {
		    Pspec_interface_set = NULL;
		    Pspec_interface_tail = NULL;
	        } else {
	            free_interface_set(&Pspec_interface_set);
	        }

		if (eos)
		    {
	            prompt("\tenter interface set (return resets default interface set)", (char *) 0, TRUE);
		    if (eos)
			{
		        if (!Pdef_interface_set)
			        punt("default interfaces have not been specified", (char *)NULL);
			Pspec_interface_set = Pdef_interface_set;
			}
		    else
		        interface_set(&Pspec_interface_set,&Pspec_interface_tail);
		    }
		else
		    interface_set(&Pspec_interface_set, &Pspec_interface_tail);
		}
	    else
		{
	        if (Pspec_port_set == Pdef_port_set) {
		    Pspec_port_set = NULL;
		    Pspec_port_tail = NULL;
	        } else {
	            free_port_set(&Pspec_port_set);
	        }

		if (eos)
		    punt("missing port identifier", (char *)NULL);
		else
		    port_set(&Pspec_port_set, &Pspec_port_tail, VIRTUAL_OK);
		}
	    }


	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Perform one of the requested reset functions - box or port list */

	if(reset_box)

	        if (Pspec_annex_list)
	            do_reset_box(Pspec_annex_list);
	        else
		{
	            if (Pdef_annex_list)
	                do_reset_box(Pdef_annex_list);
	            else
			punt(NO_BOXES, (char *)NULL);
		}

	else if(reset_printer)

	        if (Pspec_printer_set)
	            do_reset_printer(Pspec_printer_set);
	        else
		{
	            if (Pdef_port_set)
	                do_reset_printer(Pdef_printer_set);
	            else
	                punt("default printer ports have not been specified",
			 (char *)NULL);
		}

	else if(reset_interface)

	        if (Pspec_interface_set)
	            do_reset_interface(Pspec_interface_set);
	        else
		{
	            if (Pdef_interface_set)
	                do_reset_interface(Pdef_interface_set);
	            else
	                punt("default interfaces have not been specified",
			 (char *)NULL);
		}

	else
	        if (Pspec_port_set)
	            do_reset_port(Pspec_port_set);
	        else
		{
	            if (Pdef_port_set)
	                do_reset_port(Pdef_port_set);
	            else
	                punt("default ports have not been specified",
			 (char *)NULL);
		}

}	/* adm_reset_cmd() */

int
adm_set_cmd()
{
	int	category;
	char	*msg,*msgf,*msg2;
	int	error = 0;

	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	/* Process the arguments. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);

	    prompt("\tenter \"%s\", \"port\", \"synchronous\", \"interface\", or \"printer\"", 
                   BOX, FALSE);

	    category = match(symbol, param_classes, "set argument");

	    (void)setjmp(prompt_env);

	    switch (category)
		{
		case BOX_CLASS:
		    free_annex_list(&Pspec_annex_list);
	            prompt("\tenter %s list (hit return for default)",
			   BOX, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_annex_list)
			    punt(NO_BOXES, (char *)NULL);
			}
		    else
		        annex_list(&Pspec_annex_list, &Pspec_annex_tail);
		    break;

		case PRINTER_CLASS:
	            if (Pspec_printer_set == Pdef_printer_set) {
		        Pspec_printer_set = NULL;
		        Pspec_printer_tail = NULL;
	            } else {
	                free_printer_set(&Pspec_printer_set);
	            }
	            prompt("\tenter printer_set (hit return for default)",
			   NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_printer_set)
			    punt("default printers have not been specified",
			     (char *)NULL);
			}
		    else
		        printer_set(&Pspec_printer_set, &Pspec_printer_tail);
		    break;

		case PORT_CLASS:
	            if (Pspec_port_set == Pdef_port_set) {
		        Pspec_port_set = NULL;
		        Pspec_port_tail = NULL;
	            } else {
	                free_port_set(&Pspec_port_set);
	            }
	            prompt("\tenter port_set (hit return for default)",
			   NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_port_set)
			    punt("default ports have not been specified",
			     (char *)NULL);
			}
		    else
		        port_set(&Pspec_port_set, &Pspec_port_tail,
  			         VIRTUAL_NOT_OK);
		    break;

		case SYNC_CLASS:
	            if (Pspec_sync_set == Pdef_sync_set) {
		        Pspec_sync_set = NULL;
		        Pspec_sync_tail = NULL;
	            } else {
	                free_sync_set(&Pspec_sync_set);
	            }
	            prompt("\tenter sync_set (hit return for default)",
			   NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_sync_set)
			    punt("default sync ports have not been specified",
			     (char *)NULL);
			}
		    else
		        sync_set(&Pspec_sync_set, &Pspec_sync_tail);
		    break;

		case INTERFACE_CLASS:
	            if (Pspec_interface_set == Pdef_interface_set) {
		        Pspec_interface_set = NULL;
		        Pspec_interface_tail = NULL;
	            } else {
	                free_interface_set(&Pspec_interface_set);
	            }
	            prompt("\tenter interface_set (hit return for default)",
			   NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_interface_set)
			    punt("default interfaces have not been specified",
			     (char *)NULL);
			}
		    else
		        interface_set(&Pspec_interface_set, &Pspec_interface_tail);
		    break;

		}

	    (void)setjmp(prompt_env);

	    prompt("\tenter parameter list", NULLSP, FALSE);
	    }
	else
	    {
	    category = match(symbol, param_classes, "set argument");

	    (void)lex();

	    switch (category)
	        {
		case BOX_CLASS:
	            free_annex_list(&Pspec_annex_list);

	            if (symbol_length == 1 && symbol[0] == '=')
		        {
		        (void)lex();

			if (eos)
			    punt(NO_BOX, (char *)NULL);
			else
			    annex_list(&Pspec_annex_list, &Pspec_annex_tail);
			}

	            break;

		case PRINTER_CLASS:
	            if (Pspec_printer_set == Pdef_printer_set) {
		        Pspec_printer_set = NULL;
		        Pspec_printer_tail = NULL;
	            } else {
	                free_printer_set(&Pspec_printer_set);
	            }

	            if (symbol_length == 1 && symbol[0] == '=')
		        {
		        (void)lex();

			if (eos)
			    punt("missing printer identifier", (char *)NULL);
			else
			    printer_set(&Pspec_printer_set,&Pspec_printer_tail);
			}
	            break;

		case PORT_CLASS:
	            if (Pspec_port_set == Pdef_port_set) {
		        Pspec_port_set = NULL;
		        Pspec_port_tail = NULL;
	            } else {
	                free_port_set(&Pspec_port_set);
	            }

	            if (symbol_length == 1 && symbol[0] == '=')
		        {
		        (void)lex();

			if (eos)
			    punt("missing port identifier", (char *)NULL);
			else
			    port_set(&Pspec_port_set, &Pspec_port_tail,
	                     VIRTUAL_NOT_OK);
			}
	            break;

		case SYNC_CLASS:
	            if (Pspec_sync_set == Pdef_sync_set) {
		        Pspec_sync_set = NULL;
		        Pspec_sync_tail = NULL;
	            } else {
	                free_sync_set(&Pspec_sync_set);
	            }

	            if (symbol_length == 1 && symbol[0] == '=')
		        {
		        (void)lex();

			if (eos)
			    punt("missing sync identifier", (char *)NULL);
			else
			    sync_set(&Pspec_sync_set, &Pspec_sync_tail);
			}
	            break;

		case INTERFACE_CLASS:
	            if (Pspec_interface_set == Pdef_interface_set) {
		        Pspec_interface_set = NULL;
		        Pspec_interface_tail = NULL;
	            } else {
	                free_interface_set(&Pspec_interface_set);
	            }

	            if (symbol_length == 1 && symbol[0] == '=')
		        {
		        (void)lex();

			if (eos)
			    punt("missing interface identifier", (char *)NULL);
			else
			    interface_set(&Pspec_interface_set, &Pspec_interface_tail);
			}
	            break;

		}	/*switch (category) */

	        if (eos)
	            punt("missing parameter list", (char *)NULL);
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Parse the parameter list and perform the set operations. */

	switch (category)
	    {
	    case BOX_CLASS:
	        if (Pspec_annex_list)
	            error = annex_pair_list(Pspec_annex_list);
	        else if (Pdef_annex_list)
	            error = annex_pair_list(Pdef_annex_list);
	        else
	            punt(NO_BOXES, (char *)NULL);

		if (!script_input && !error) {
		    printf(changes_will);
		    msgf = next_boot;
		    msg2 = cr_reset_all;
		    switch(Pset_list->param_num) {
		    case BOX_PASSWORD:
			msgf = imm_local;
			msg = annex_reset_secureserver;
			msg2 = or_reset_all;
			break;

		    case VCLI_PASSWORD:
		    case VCLI_SEC_ENA:
			printf(immediately);
			msgf = NULL;
			break;

		    case ACP_KEY:
			printf(or_passwd_cmd);
			msg2 = or_reset_all;
		    case NET_TURNAROUND:
		    case PREF_SECURE_1:
		    case PREF_SECURE_2:
		    case SECURSERVER_BCAST:
		    case ENABLE_SECURITY:
			msg = annex_reset_secureserver;
			break;

		    case NRWHOD:
		    case NMIN_UNIQUE:
		    case HTABLE_SZ:
		    case PRIMARY_NS:
		    case PRIMARY_NS_ADDR:
		    case SECONDARY_NS:
		    case SECONDARY_NS_ADDR:
		    case NAMESERVER_BCAST: 
			msg = annex_reset_nameserver;
			break;

		    case MOTD:
			msg = annex_reset_motd;
			break;

		    case HOST_NAME:
		    case HOST_NUMBER:
		    case SERVICE_LIMIT:
		    case KA_TIMER:
		    case CIRCUIT_TIMER:
		    case RETRANS_LIMIT:
		    case GROUP_CODE:
		    case QUEUE_MAX:
		    case VCLI_GROUPS:
			msg = annex_reset_lat;
			break;

		    default:
			msg = NULL;
			break;
		    }
		    if (msgf == NULL)
			break;
		    printf(msgf,BOX);
		    if (msg != NULL) {
			printf(msg,BOX);
			printf(msg2,BOX);
		    } else
			printf(annex_msg,BOX);
		}
	        break;

	    case PORT_CLASS:
	        if (Pspec_port_set)
	            error = port_pair_list(Pspec_port_set);
	        else if (Pdef_port_set)
	            error = port_pair_list(Pdef_port_set);
	        else
	            punt(NO_BOXES, (char *)NULL);

	        if (!script_input && !error) {
		    printf(changes_will);
		    printf(next_boot,BOX);
		    printf(port_msg);
		}
	        break;

	    case SYNC_CLASS:
	        if (Pspec_sync_set)
	            error = sync_pair_list(Pspec_sync_set);
	        else if (Pdef_sync_set)
	            error = sync_pair_list(Pdef_sync_set);
	        else
	            punt(NO_BOXES, (char *)NULL);

	        if (!script_input && !error) {
		    printf(changes_will);
		    printf(next_boot,BOX);
		    printf(sync_msg);
		}
	        break;

	    case INTERFACE_CLASS:
	        if (Pspec_interface_set)
		    error = interface_pair_list(Pspec_interface_set);
	        else if (Pdef_interface_set)
		    error = interface_pair_list(Pdef_interface_set);
		else
		    punt(NO_BOXES, (char *)NULL);

	        if (!script_input && !error) {
		    printf(changes_will);
		    printf(next_boot,BOX);
		    printf(interface_msg);
		}
	        break;

	    case PRINTER_CLASS:
	        if (Pspec_printer_set)
	            error = printer_pair_list(Pspec_printer_set);
	        else if (Pdef_printer_set)
	            error = printer_pair_list(Pdef_printer_set);
	        else
	            punt(NO_BOXES, (char *)NULL);

	        if (!script_input && !error) {
		    printf(changes_will);
		    printf(next_boot,BOX);
		    printf(printer_msg);
		}
	        break;
	    }
	return error;
}	/* adm_set_cmd() */



adm_show_cmd()

{
	int category;

	/* Process the arguments. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);

	    prompt("\tenter \"%s\", \"port\", \"synchronous\", \"interface\", or \"printer\"", 
                   BOX, FALSE);

	    category = match(symbol, param_classes, "show argument");

	    (void)setjmp(prompt_env);

	    switch (category)
		{
		case BOX_CLASS:
		    free_annex_list(&Pspec_annex_list);
	            prompt("\tenter %s list (hit return for default)",
		           BOX, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_annex_list)
			    punt(NO_BOXES, (char *)NULL);
			}
		    else
		        annex_list(&Pspec_annex_list, &Pspec_annex_tail);
		    break;

		case PRINTER_CLASS:
	            if (Pspec_printer_set == Pdef_printer_set) {
		        Pspec_printer_set = NULL;
		        Pspec_printer_tail = NULL;
	            } else {
	                free_printer_set(&Pspec_printer_set);
	            }
	            prompt("\tenter printer_set (hit return for default)",
		           NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_printer_set)
			    punt("default printers have not been specified",
			     (char *)NULL);
			}
		    else
		        printer_set(&Pspec_printer_set, &Pspec_printer_tail, 
	  			 VIRTUAL_NOT_OK);
	            break;

		case PORT_CLASS:
	            if (Pspec_port_set == Pdef_port_set) {
		        Pspec_port_set = NULL;
		        Pspec_port_tail = NULL;
	            } else {
	                free_port_set(&Pspec_port_set);
	            }
	            prompt("\tenter port_set (hit return for default)",
		           NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_port_set)
			    punt("default ports have not been specified",
			     (char *)NULL);
			}
		    else
		        port_set(&Pspec_port_set, &Pspec_port_tail, 
	  			 VIRTUAL_NOT_OK);
	            break;

		case SYNC_CLASS:
	            if (Pspec_sync_set == Pdef_sync_set) {
		        Pspec_sync_set = NULL;
		        Pspec_sync_tail = NULL;
	            } else {
	                free_sync_set(&Pspec_sync_set);
	            }
	            prompt("\tenter sync_set (hit return for default)",
		           NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_sync_set)
			    punt("default sync ports have not been specified",
			     (char *)NULL);
			}
		    else
		        sync_set(&Pspec_sync_set, &Pspec_sync_tail);
	            break;

		case INTERFACE_CLASS:
	            if (Pspec_interface_set == Pdef_interface_set) {
		        Pspec_interface_set = NULL;
		        Pspec_interface_tail = NULL;
	            } else {
	                free_interface_set(&Pspec_interface_set);
	            }
	            prompt("\tenter interface_set (hit return for default)",
		           NULLSP, TRUE);
		    if (eos)
			{
			(void)lex();
			if (!Pdef_interface_set)
			    punt("default interfaces have not been specified",
			     (char *)NULL);
			}
		    else
		        interface_set(&Pspec_interface_set, &Pspec_interface_tail, 
	  			 VIRTUAL_NOT_OK);
	            break;

		}

	    (void)setjmp(prompt_env);

	    prompt("\tenter parameter list (hit return for all)",
	    	   NULLSP, TRUE);

	    if (eos)
		{
		(void)strcpy(command_line, "all");
		Psymbol = command_line;
		eos = FALSE;
		(void)lex();
		}

	    }
	else
	    {
	    category = match(symbol, param_classes, "show argument");

	    (void)lex();

		switch (category)
		    {
		    case BOX_CLASS:
			free_annex_list(&Pspec_annex_list);

	                if (symbol_length == 1 && symbol[0] == '=')
		            {
		            (void)lex();

			    if (eos)
				punt(NO_BOX, (char *)NULL);
			    else
			        annex_list(&Pspec_annex_list,
				 &Pspec_annex_tail);
			    }

			break;

		    case PRINTER_CLASS:
	                if (Pspec_printer_set == Pdef_printer_set) {
		            Pspec_printer_set = NULL;
		            Pspec_printer_tail = NULL;
	                } else {
	                    free_printer_set(&Pspec_printer_set);
	                }

	                if (symbol_length == 1 && symbol[0] == '=')
		            {
		            (void)lex();

			    if (eos)
			        punt("missing printer identifier", (char *)NULL);
			    else
			        printer_set(&Pspec_printer_set,&Pspec_printer_tail);
			    }
	                break;

		    case PORT_CLASS:
	                if (Pspec_port_set == Pdef_port_set) {
		            Pspec_port_set = NULL;
		            Pspec_port_tail = NULL;
	                } else {
	                    free_port_set(&Pspec_port_set);
	                }

	                if (symbol_length == 1 && symbol[0] == '=')
		            {
		            (void)lex();

			    if (eos)
			        punt("missing port identifier", (char *)NULL);
			    else
			        port_set(&Pspec_port_set, &Pspec_port_tail,
				 VIRTUAL_NOT_OK);
			    }
	                 break;

		    case SYNC_CLASS:
	                if (Pspec_sync_set == Pdef_sync_set) {
		            Pspec_sync_set = NULL;
		            Pspec_sync_tail = NULL;
	                } else {
	                    free_sync_set(&Pspec_sync_set);
	                }

	                if (symbol_length == 1 && symbol[0] == '=')
		            {
		            (void)lex();

			    if (eos)
			        punt("missing sync identifier", (char *)NULL);
			    else
			        sync_set(&Pspec_sync_set, &Pspec_sync_tail);
			    }
	                 break;

		    case INTERFACE_CLASS:
	                if (Pspec_interface_set == Pdef_interface_set) {
		            Pspec_interface_set = NULL;
		            Pspec_interface_tail = NULL;
	                } else {
	                    free_interface_set(&Pspec_interface_set);
	                }

	                if (symbol_length == 1 && symbol[0] == '=')
		            {
		            (void)lex();

			    if (eos)
			        punt("missing interface identifier", (char *)NULL);
			    else
			        interface_set(&Pspec_interface_set, &Pspec_interface_tail,
				 VIRTUAL_NOT_OK);
			    }
	                 break;
		    }
		}

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Parse the parameter list and perform the show operations. */

	open_pager();

	switch (category)
	    {
	    case BOX_CLASS:
	        if (Pspec_annex_list)
	            annex_show_list(Pspec_annex_list);
	        else
		    if (Pdef_annex_list)
			annex_show_list(Pdef_annex_list);
		    else
			punt(NO_BOXES, (char *)NULL);
	        break;

	    case PORT_CLASS:
	        if (Pspec_port_set)
	            port_show_list(Pspec_port_set);
	        else
	            if (Pdef_port_set)
	                port_show_list(Pdef_port_set);
	            else
	                punt("default ports have not been specified",
			 (char *)NULL);
	        break;

	    case SYNC_CLASS:
	        if (Pspec_sync_set)
	            sync_show_list(Pspec_sync_set);
	        else
	            if (Pdef_sync_set)
	                sync_show_list(Pdef_sync_set);
	            else
	                punt("default sync ports have not been specified",
			 (char *)NULL);
	        break;

	    case INTERFACE_CLASS:
	        if (Pspec_interface_set)
	            interface_show_list(Pspec_interface_set);
	        else
	            if (Pdef_interface_set)
	                interface_show_list(Pdef_interface_set);
	            else
	                punt("default interfaces have not been specified",
			 (char *)NULL);
	        break;

	    case PRINTER_CLASS:
	        if (Pspec_printer_set)
	            printer_show_list(Pspec_printer_set);
	        else
	            if (Pdef_printer_set)
	                printer_show_list(Pdef_printer_set);
	            else
	                punt("default printers have not been specified",
			 (char *)NULL);
	        break;
	    }
	close_pager();

}	/* adm_show_cmd() */



adm_write_cmd()

{
	ANNEX_ID	   annex_id;
	char               name[FILENAME_LENGTH + 2],
			   filename[FILENAME_LENGTH + 2];
	if (!is_super)
	    punt("must be superuser", (char *)NULL);

	/* Process the arguments. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\t%s identifier", BOX, FALSE);
	    (void)annex_name(&annex_id, name, 0);

	    (void)setjmp(prompt_env);
	    prompt("\tfilename", NULLSP, FALSE);
	    lex_string();
	    if (symbol_length > FILENAME_LENGTH)
		punt(LONG_FILENAME, (char *)NULL);
	    (void)strcpy(filename, symbol);
	    (void)lex();
	    }
	else
	    {
	    (void)annex_name(&annex_id, name, 0);

	    if (eos)
		punt("missing filename", (char *)NULL);
	    else
		{
		lex_string();
	        if (symbol_length > FILENAME_LENGTH)
		    punt(LONG_FILENAME, (char *)NULL);
	        (void)strcpy(filename, symbol);
		(void)lex();
		}
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Perform the write operation. */

	do_write(filename, &annex_id, name);

}	/* adm_write_cmd() */

adm_printer_cmd()

{
	PRINTER_SET *Ptemp_printer_set = NULL,
	         *Ptemp_printer_tail = NULL;

	/* Assign the default printer set. */

	/* If no arguments were supplied, set prompt_mode so that they
	   will be prompted for. */
	prompt_mode = (lex() == LEX_EOS);

	if (prompt_mode)
	    {
	    if (script_input)
		punt("missing arguments", (char *)NULL);

	    (void)setjmp(prompt_env);
	    prompt("\tenter default printer set", NULLSP, FALSE);
	    }

	/* Turn off prompt_mode so that subsequent errors will be punted
	   back to the command prompt. */
	prompt_mode = FALSE;

	/* Free the temp printer set in case you came back here after punting.*/
	free_printer_set(&Ptemp_printer_set);

	/*Let a temp point at the printer set in case the human makes a mistake,
	   so that the old default will still be there after punting. */
	printer_set(&Ptemp_printer_set, &Ptemp_printer_tail);

	/* No human mistakes, so move the temp printer set to the default
	   printer set.  Free the default first in case it had been previously
	   entered. */
	free_printer_set(&Pdef_printer_set);
	Pdef_printer_set = Ptemp_printer_set;
	Pdef_printer_tail = Ptemp_printer_tail;

}	/* adm_printer_cmd() */

#if 0
int alternate_table[16];
int alternate_count = 0;
#endif

init_tables()
{
	int seq,indx,usage;

/*  go through help table  */
	for (seq = 0; D_key(seq) != NULL; seq++) {
	    indx = D_index(seq);
	    usage = D_usage(seq);
#if 0
	    if (usage < 0) {
		alternate_table[alternate_count++] = seq;
		continue;
	    }
#else
	    if (usage < 0)
		continue;
#endif
	    switch (usage) {
	    case A_COMMAND:
		if (indx < NCOMMANDS) {
		    cmd_spellings[indx] = D_key(seq);
		    continue;
		}
		break;

	    case PARAM_CLASS:
		if(indx < NCLASSES) {
		    param_classes[indx] = D_key(seq);
		    continue;
		}
		break;

	    case BOX_PARAM:
	    case BOX_CATEGORY:
		if (indx < NBOXP) {
		    annex_params[indx] = D_key(seq);
		    continue;
		}
		break;

	    case PORT_PARAM:
	    case PORT_CATEGORY:
		if (indx < NPORTP) {
		    port_params[indx] = D_key(seq);
		    continue;
		}
		break;

	    case SYNC_PARAM:
	    case SYNC_CATEGORY:
		if (indx < NSYNCP) {
		    sync_params[indx] = D_key(seq);
		    continue;
		}
		break;

	    case PRINTER_PARAM:
		if (indx < NPRINTP) {
		    printer_params[indx] = D_key(seq);
		    continue;
		}
		break;

	    case INTERFACE_PARAM:
		if (indx < NINTERFACEP) {
		    interface_params[indx] = D_key(seq);
		    continue;
		}
		break;

	    case HELP_ENTRY:
		continue;

	    default:
		printf("Help entry %s invalid usage:  %d\n",
		    D_key(seq),usage);
		continue;
	    }
	    printf("Help entry %s (%s) invalid value: %d\n",
		D_key(seq), usage_table[usage], indx);
	}
	cmd_spellings[NCOMMANDS] = (char *)NULL;
	param_classes[NCLASSES]  = (char *)NULL;
	annex_params[NBOXP]      = (char *)NULL;
	port_params[NPORTP]      = (char *)NULL;
	sync_params[NSYNCP]      = (char *)NULL;
	printer_params[NPRINTP]  = (char *)NULL;
	interface_params[NINTERFACEP]  = (char *)NULL;
}	/* init_tables() */
