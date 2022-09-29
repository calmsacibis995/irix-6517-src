#ident "$Revision: 1.5 $"
/*
*
* Copyright 1995, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#include <wchar.h>
#include <ctype.h>

#undef iswalpha
#undef iswupper
#undef iswlower
#undef iswdigit
#undef iswxdigit
#undef iswalnum
#undef iswspace
#undef iswpunct
#undef iswprint
#undef iswgraph
#undef iswcntrl
#undef iswascii
#undef towupper
#undef towlower

#pragma weak iswalpha = _iswalpha
#pragma weak iswupper = _iswupper
#pragma weak iswlower = _iswlower
#pragma weak iswdigit = _iswdigit
#pragma weak iswxdigit = _iswxdigit
#pragma weak iswalnum = _iswalnum
#pragma weak iswspace = _iswspace
#pragma weak iswpunct = _iswpunct
#pragma weak iswprint = _iswprint
#pragma weak iswgraph = _iswgraph
#pragma weak iswcntrl = _iswcntrl
#pragma weak towupper = _towupper
#pragma weak towlower = _towlower
#pragma weak iswascii = _iswascii
#if !_LIBC_ABI
#pragma weak isphonogram = _isphonogram
#pragma weak isideogram = _isideogram
#pragma weak isenglish = _isenglish
#pragma weak isnumber = _isnumber
#pragma weak isspecial = _isspecial
#endif

#include "synonyms.h"

int 
iswalpha(wint_t wc) 
{
	return	((wc) > 255 ? __iswctype(wc,_ISwalpha) : isalpha(wc));
}

int 
iswupper(wint_t wc) 
{
	return	((wc) > 255 ? __iswctype(wc,_ISwupper) : isupper(wc));
}

int 
iswlower(wint_t wc) 
{
	return	((wc) > 255 ? __iswctype(wc,_ISwlower) : islower(wc));
}

int 
iswdigit(wint_t wc) 
{
	return	((wc) > 255 ? __iswctype(wc,_ISwdigit) : isdigit(wc));
}

int 
iswxdigit(wint_t wc)
{
	return	((wc) > 255 ? __iswctype(wc,_ISwxdigit) : isxdigit(wc));
}

int 
iswalnum(wint_t wc) 
{
	return	((wc) > 255 ? __iswctype(wc,_ISwalnum) : isalnum(wc));
}

int 
iswspace(wint_t wc) 
{
	return ((wc) > 255 ? __iswctype(wc,_ISwspace) : isspace(wc));
}

int 
__iswblank(wint_t wc) 
{
	return ((wc) > 255 ? __iswctype(wc,_ISwblank) : __isblank(wc));
}

int	
iswpunct(wint_t wc)
{
	return	((wc) > 255 ? __iswctype(wc,_ISwpunct) : ispunct(wc));
}

int 
iswprint(wint_t wc)
{
	return	((wc) > 255 ? __iswctype(wc,_ISwprint) : isprint(wc));
}

int 
iswgraph(wint_t wc) 
{
	return	((wc) > 255 ? __iswctype(wc,_ISwgraph) : isgraph(wc));
}

int 
iswcntrl(wint_t wc)
{
	return	((wc) > 255 ? __iswctype(wc,_ISwcntrl) : iscntrl(wc));
}

wint_t	
towupper(wint_t wc)
{
	return	((wc) > 255 ? __trwctype(wc,_L) : toupper((int)wc));
}

wint_t	
towlower(wint_t wc)
{
	return	((wc) > 255 ? __trwctype(wc,_U) : tolower((int)wc));
}

int 
iswascii(wint_t wc)
{
	return(isascii(wc));
}

int
isphonogram(wint_t wc)
{
	return wc > 255 && __iswctype(wc, _ISwphonogram);
}

int
isideogram(wint_t wc)
{
	return wc > 255 && __iswctype(wc, _ISwideogram);
}

int
isenglish(wint_t wc)
{
	return wc > 255 && __iswctype(wc, _ISwenglish);
}

int
isnumber(wint_t wc)
{
	return wc > 255 && __iswctype(wc, _ISwnumber);
}

int
isspecial(wint_t wc)
{
	return wc > 255 && __iswctype(wc, _ISwspecial);
}
