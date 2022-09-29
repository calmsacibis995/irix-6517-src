/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $"

#include <wait.h>

int
_wstat(int code, int data)
{
        register int stat = (data & 0377);

        switch (code) {
                case CLD_EXITED:
                        stat <<= 8;
                        break;
                case CLD_DUMPED:
                        stat |= WCOREFLAG; /* XXX compatibility? */
                        break;
                case CLD_KILLED:
                        break;
                case CLD_TRAPPED:
                case CLD_STOPPED:
                        stat <<= 8;
                        stat |= WSTOPFLG;
                        break;
                case CLD_CONTINUED:
                        stat = WCONTFLG;
                        break;
        }
        return stat;
}
