/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.2 $"

#include <libkl.h>
#include <kllibc.h>

__uint64_t
atox(char *cp)
{
    __uint64_t          i;

    /* Ignore leading 0x or 0X */

    if (*cp == '0' && (cp[1] == 'x' || cp[1] == 'X'))
        cp += 2;

    i = 0;

    while (isxdigit(*cp)) {
        i = i * 16 + (DIGIT(*cp));
        cp++;
    }

    return i;
}

