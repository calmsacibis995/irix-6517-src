/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(_KERNEL)
static const char *_identString = "$Id: krbcrc.c,v 65.4 1998/03/23 17:28:24 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif



/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: krbcrc.c,v $
 * Revision 65.4  1998/03/23 17:28:24  gwehrman
 * Source Identification changed to bracket with SGIMIPS && !_KERNEL.
 *
 * Revision 65.3  1998/01/07 17:21:08  jdoak
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS
 *
 * Revision 65.2  1997/11/06  19:57:09  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:02  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.318.2  1996/02/18  00:04:30  marty
 * 	Update OSF copyright years
 * 	[1996/02/17  22:56:45  marty]
 *
 * Revision 1.1.318.1  1995/12/08  00:20:49  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:59:37  root]
 * 
 * Revision 1.1.316.1  1994/01/21  22:37:54  cbrooks
 * 	RPC Code Cleanyp - Initial Submission
 * 	[1994/01/21  21:58:13  cbrooks]
 * 
 * Revision 1.1.2.3  1993/01/03  23:25:59  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  20:08:05  bbelch]
 * 
 * Revision 1.1.2.2  1992/12/23  20:51:58  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  15:39:51  zeliff]
 * 
 * Revision 1.1  1992/01/19  03:06:33  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**      Digital Equipment Corporation, Maynard, Mass.
**
**
**  NAME
**
**      krbcrc.c
**
**  FACILITY:
**
**      Remote Procedure Call (RPC) 
**
**  ABSTRACT:
**
**  Compute Cyclic Reduncancy Checks.
**
**
*/

#include <krbp.h>

static unsigned32 rpc__krb_crc_table[256];

/*
 * Note that the msb of the "polynomial" is the least significant coefficient,
 * and vice versa.
 * 
 * Coefficients of the AUTODIN-II polynomial,
 * x**32 + x**26 + x**23 + x**22 + x**16 + x**12 + x**11 + x**10 +
 * x**8 + x**7 + x**5 + x**4 + x**2 + x**1 + 1
 */

static unsigned32 gpoly = 0xedb88320U; 
        
static unsigned32 g[8];

/*
 * R P C _ _ K R B _ C R C _ I N I T
 *
 * Initialize the CRC table.
 *
 * This could be done at compile time, but I'm lazy.
 */

void rpc__krb_crc_init(void)
{
    int i, m;

    g[0] = gpoly;
        
    /* Compute g[i] */
    for (i=1; i < 8; i++)
    {
        g[i] = g[i-1]>>1 ^ gpoly * (g[i-1] & 1);
    }
    /* compute CRC table. */
    for (i=0; i < 256; i++)
    {
        int t = 0;
        for (m = 7; m >=0; m--)
        {
            if (i & (1<<m))
                t ^= g[7-m];
        }
        rpc__krb_crc_table[i] = t;
    }
}

/*
 * R P C _ _ K R B _ C R C
 *
 * Compute the CRC of the first "len" bytes of "buf"; start the CRC
 * with the given initial value.
 */

unsigned32 rpc__krb_crc
#ifdef _DCE_PROTO_
(
        register unsigned32 crc,
        unsigned char *buf,
        int len
)
#else
(crc, buf, len)
    register unsigned32 crc;
    unsigned char *buf;
    int len;
#endif
{
    for ( ; len ; --len)
    {
        unsigned char t = (*buf++) ^ crc;
        crc >>= 8;
        crc ^= rpc__krb_crc_table[t];
    }
    return crc;
}
