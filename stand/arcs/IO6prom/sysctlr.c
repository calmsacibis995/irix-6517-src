/***********************************************************************\
*	File:		sysctlr.c					*
*									*
*	Contains sundry routines and utilities for talking to 		*
*	the Everest system controller.					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <ksys/elsc.h>

/*
 * sysctlr_message()
 *	Displays a message on the system controller LCD.
 */

void
sysctlr_message(char *message)
{
    elsc_display_mesg(get_elsc(), message);
}
