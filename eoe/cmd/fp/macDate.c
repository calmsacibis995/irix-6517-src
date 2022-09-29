/* 
   (C) Copyright Digital Instrumentation Technology, Inc. 1990 - 1993 
       All Rights Reserved
*/

/*-- TransferPro macDate.c ------
 *
 */

#include "macSG.h"
#include "dm.h"
#include <time.h>

/*-- macDateToString --------
 *
 */

char *macDateToString( macDate )
    int macDate;
   {
    macDate -= MAC_DATEFACTOR;
    return( (char *)asctime((struct tm *)localtime((time_t *)&macDate)) );
   }

/*-- macDate -------
 *
 */

int macDate()
   {
    return( (int)time(0)+MAC_DATEFACTOR );
   }


