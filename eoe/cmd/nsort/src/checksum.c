/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 * checksum.c	- compute a record checksum for testing nsort
 *
 *
 *	$Ordinal-Id: checksum.c,v 1.5 1996/02/14 21:47:14 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include <stdio.h>
#include "ordinal.h"

/*
** compute_checksum	- calculate a checksum to detect mangled records
**
**	Paramters:
**		data	- The address of the first byte to checksum.
**			  The data might not be aligned to a word boundary,
**			  so byte-by-byte accesses may be required. Bummer.
**			  Even if it is aligned before the sort, it might
**			  not be aligned afterwards.
**		length	- The number of bytes to checksum. The length might
**			  not be aligned to a word boundary either. Bummer#2.
**
**	Returns:
**		a unsigned checksum value
**		It should be xored with the current 'running xor'
**		when checksumming a data set.
*/
unsigned compute_checksum(byte *data, int length)
{
    u4	sum;
    
    for (sum = 0; length >= sizeof(u4); length -= sizeof(u4),data += sizeof(u4))
    {
	/* xor in next word and then rotate sum to the left.
	 * this checksum doesn't distinguish 0 from 00 or 00...0
	 */
	sum ^= ulw(data);
	sum = (sum << 1) | ((sum & 0x80000000) ? 1 : 0);
    }

    if (length > 0)
    {
	/*
	** Now xor any bytes which weren't handled by the above
	** 32-bit-a-a-time loop. This avoids reading beyond the
	** end of the data.
	*/
	switch (length)
        {
          case 1:
            sum ^= data[0] << 24;
	    break;

          case 2:
            sum ^= (data[0] << 24) | (data[1] << 16);
	    break;

          case 3:
            sum ^= (data[0] << 24) | (data[1] << 16) | (data[2] << 8);
	    break;
        }
    }

    return (sum);
}
