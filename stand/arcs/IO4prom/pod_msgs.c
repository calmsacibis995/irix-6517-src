/***********************************************************************\
*	File:		pod_msgs.c					*
*									*
\***********************************************************************/

#ident "arcs/IO4prom/pod_msgs.c $Revision: 1.7 $"

#include "pod_fvdefs.h"
#include <stdarg.h>
#include "libsc.h"
#include "libsk.h"


char tmpbuf[128]; /* This message */
int  Nlevel=0;

/* 
 * Function : mk_msg
 * Description : 
 *	Print the Three '*' in the begining to indicate the Error.
 *	Depending on the Nesting level, print the sufficient No of blanks.
 *	Print the message.
 */
#ifndef	NEWTSIM
/*VARARGS1*/
void
mk_msg(char *fmt, ...)
{
    va_list	ap;
    int 	i;


    for (i=0; i < Nlevel*4; i++)
	tmpbuf[i] = ' '; 
    tmpbuf[i++] = '*'; tmpbuf[i++] = '*'; tmpbuf[i++] = '*'; 

    va_start(ap,fmt);
    vsprintf(&tmpbuf[i], fmt, ap);
    va_end(ap);
    message(tmpbuf);

}

/*VARARGS1*/
void
message(char *fmt, ...)
{
    va_list 	ap;
    int		i;

    for (i=0; i< Nlevel; i++)
	printf("    ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}
#endif
