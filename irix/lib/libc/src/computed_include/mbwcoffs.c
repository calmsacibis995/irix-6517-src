/**************************************************************************
 *                                                                        *
 * Copyright (C) 1995 Silicon Graphics, Inc.                              *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * mbwcoffs.c
 *
 * Offset computation for mbwc_wrap.s
 *
 */

#include <stdlib.h>
#include <wchar.h>
#include "iconv_cnv.h"

void printit( char * s, int d )
{
    printf( "#define __MBWC_%s	    (%d)\n", s, d );
}

#define printoffs( arg ) \
    printit( #arg, ( (long) &((( _iconv_mbhandle_t )0)->iconv_meths->iconv_ ## arg ) ) / sizeof( void * ) )
    


int main()
{
    printoffs( btowc );	/* ptr to btowc function	*/
    printoffs( mbtowc );	/* ptr to mbtowc function	*/
    printoffs( mblen );	/* ptr to mblen function	*/
    printoffs( mbstowcs );	/* ptr to mbtowcs function	*/
    printoffs( mbwcctl );	/* ptr to ctl func for mbtowc	*/
        
    printoffs( wctob );	/* ptr to wctob function	*/
    printoffs( wctomb );	/* ptr to wctomb function	*/
    printoffs( wcstombs );	/* ptr to wcstombs function	*/
    printoffs( wcmbctl );	/* ptr to ctl func for wctomb	*/

    return 0;
}

