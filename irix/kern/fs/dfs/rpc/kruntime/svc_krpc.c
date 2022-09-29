/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: svc_krpc.c,v 65.5 1998/03/23 17:27:05 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * 
 */
/*
 * HISTORY
 * $Log: svc_krpc.c,v $
 * Revision 65.5  1998/03/23 17:27:05  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.4  1998/01/07 17:20:55  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.3  1997/11/06  19:56:54  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/20  19:16:29  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.4.2  1996/02/18  00:00:56  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:53:57  marty]
 *
 * Revision 1.1.4.1  1995/12/08  00:15:37  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:36  root]
 * 
 * Revision 1.1.2.2  1994/10/06  20:27:58  agd
 * 	expand copyright
 * 	[1994/10/06  14:27:23  agd]
 * 
 * Revision 1.1.2.1  1994/09/15  13:36:20  rsarbo
 * 	stripped down version of S12Y interface for KRPC
 * 	[1994/09/15  13:19:07  rsarbo]
 * 
 * $EndLog$
 */

#include <rpcsvc.h>
#include <stdarg.h>

/*
 * All we need in kernel space is a dummy
 * handle.  In user space this gets initialized
 * in rpcsvc.c using the global macro 
 * DCE_SVC_DEFINE_HANDLE
 */
dce_svc_handle_t rpc_g_svc_handle;


void dce_ksvc_printf(
    dce_svc_handle_t            handle,
#if     defined(DCE_SVC_WANT__FILE__)
    const char                  *file,
    const int                   line,
#endif  /* defined(DCE_SVC_WANT__FILE__) */
    char                  	*argtypes,
    const unsigned32            table_index,
    const unsigned32            attributes,
    const unsigned32            message_index,
    ...
)
{
	int size, j, numargs = 0;
	char *a;
        dce_msg_table_t *mp;
	va_list ap;
#ifdef SGIMIPS
	extern void panic(char *, ...);
	extern void printf(char *fmt, ...);
#endif


        size = sizeof(rpc_g_svc_msg_table) / sizeof(dce_msg_table_t);
        for (mp = rpc_g_svc_msg_table, j = 0; j < size; j++)
            if (mp[j].message == message_index)
		break;

	if (j == size)
	    panic("dce_ksvc_printf: out of range message id!\n");

	/*
	 * Determine the number of printf arguments. 
	 */
	for(a = argtypes; *a != '\0'; a++) {
	    switch (*a) {
		case '%':
		    numargs++;
		    break;
		case 'b': case 'f': case 'e': case 'E': case 'g': case 'G':
		    printf("dce_ksvc_printf: can't handle format char\n");
		    return;
		default:
		    break;
	    }
	}

	va_start(ap, message_index);
	/*
	 * This code assumes the arguments passed along with
	 * format characters in a printf are of size 32 bits
	 * on the stack (except for those listed in the 
	 * switch above).
	 *
	 * If this is not true for your platform, you'll need
	 * to modify this routine.
	 */
	switch (numargs) {
	    case 0:
		printf(mp[j].text); 
		break;
	    case 1:
		printf(mp[j].text, va_arg(ap, unsigned32));
		break;
	    case 2:
		printf(mp[j].text, va_arg(ap, unsigned32), 
				   va_arg(ap, unsigned32));
		break;
	    case 3:
		printf(mp[j].text, va_arg(ap, unsigned32), 
				   va_arg(ap, unsigned32),
				   va_arg(ap, unsigned32));
		break;
	    case 4:
		printf(mp[j].text, va_arg(ap, unsigned32), 
				   va_arg(ap, unsigned32),
				   va_arg(ap, unsigned32),
				   va_arg(ap, unsigned32));
		break;
	    case 5:
		printf(mp[j].text, va_arg(ap, unsigned32), 
				   va_arg(ap, unsigned32),
				   va_arg(ap, unsigned32),
				   va_arg(ap, unsigned32),
				   va_arg(ap, unsigned32));
		break;
	    default:
		printf("dce_ksvc_printf: invalid number of args: %d\n",numargs);
		break;
	}
	printf("\n");
	va_end(ap);
}
