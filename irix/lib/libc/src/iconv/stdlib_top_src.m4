#if defined(_COMPILER_VERSION)
#pragma set woff 3201	/* the "handle" param isn't referenced by many of
	* the generated functions, and it's too painful to turn this warning
	* off at each one; but only want this for non-o32, because o32 doesn't
	* understand the pragma and warns, and o32 doesn't warn about unused
	* parameters in the first place... */
#endif

/* __file__
 * stdlib_top_src.m4
 *
 *  Warning - if this is a C source file you are reading the
 *  output of an M4 script.  Do not edit this code - edit the
 *  M4 scripts that are it's source.
 *
 *  Created __date__
 *
 *  The `__file__' and `__line__' macro's are expanded by GNU m4
 *  and it is suggested that this be done for debugging since it
 *  simplifies finding the right source.
 *
*/

define(`CZZ_SYMSFILE',`stdlib.syms')
syscmd(rm -f CZZ_SYMSFILE)
define(`CZZ_DCLSFILE',`stdlib.dcls.h')
syscmd(rm -f CZZ_DCLSFILE)

/* Get all macro definitions */
include(stdlib_top_defn.m4)

blow_ICONV_LIB_METHODS()

/* TOP __file__ line __line__ ================================== */

/* SBCS iso-8859-* koi8 etc */

create_btowc_2(flb,flw)
create_mblen_2(flb,flw)
create_mbtowc_2(flb,flw)
create_mbstowcs_2(flb,flw)
create_wctob_2(flw,wcsbcsmbs)
create_wctomb_2(flw,wcsbcsmbs)
create_wcstombs_2(flw,wcsbcsmbs)

create_default_iconv_mbhandle(sbcs)
create_ICONV_LIB_METHODS(sbcs)

/* SJIS mbtowc functions */

create_btowc_3(flb,sjiswc,flw)
create_mblen_3(flb,sjiswc,flw)
create_mbtowc_3(flb,sjiswc,flw)
create_mbstowcs_3(flb,sjiswc,flw)
create_wctob_2(flw,wcsjismbs)
create_wctomb_2(flw,wcsjismbs)
create_wcstombs_2(flw,wcsjismbs)

create_ICONV_LIB_METHODS(sjis)

/* eucJP mbtowc functions */

create_btowc_3(flb,eucjpwc,flw)
create_mblen_3(flb,eucjpwc,flw)
create_mbtowc_3(flb,eucjpwc,flw)
create_mbstowcs_3(flb,eucjpwc,flw)
create_wctob_2(flw,wceucjpmbs)
create_wctomb_2(flw,wceucjpmbs)
create_wcstombs_2(flw,wceucjpmbs)

create_ICONV_LIB_METHODS(eucjp)

/* eucJP3 mbtowc functions */

create_btowc_3(flb,eucjp3wc,flw)
create_mblen_3(flb,eucjp3wc,flw)
create_mbtowc_3(flb,eucjp3wc,flw)
create_mbstowcs_3(flb,eucjp3wc,flw)
create_wctob_2(flw,wceucjp3mbs)
create_wctomb_2(flw,wceucjp3mbs)
create_wcstombs_2(flw,wceucjp3mbs)

create_ICONV_LIB_METHODS(eucjp3)

/* BIG5 mbtowc functions */

create_btowc_3(flb,big5wc,flw)
create_mblen_3(flb,big5wc,flw)
create_mbtowc_3(flb,big5wc,flw)
create_mbstowcs_3(flb,big5wc,flw)
create_wctob_2(flw,wcbig5mbs)
create_wctomb_2(flw,wcbig5mbs)
create_wcstombs_2(flw,wcbig5mbs)

create_ICONV_LIB_METHODS(big5)

/* eucTW mbtowc functions */

create_btowc_3(flb,euctwwc,flw)
create_mblen_3(flb,euctwwc,flw)
create_mbtowc_3(flb,euctwwc,flw)
create_mbstowcs_3(flb,euctwwc,flw)
create_wctob_2(flw,wceuctwmbs)
create_wctomb_2(flw,wceuctwmbs)
create_wcstombs_2(flw,wceuctwmbs)

create_ICONV_LIB_METHODS(euctw)

/* DBCS (eucCN eucKR) mbtowc functions */

create_btowc_3(flb,dbcswc,flw)
create_mblen_3(flb,dbcswc,flw)
create_mbtowc_3(flb,dbcswc,flw)
create_mbstowcs_3(flb,dbcswc,flw)
create_wctob_2(flw,wcdbcsmbs)
create_wctomb_2(flw,wcdbcsmbs)
create_wcstombs_2(flw,wcdbcsmbs)

create_ICONV_LIB_METHODS(dbcs)

/* UTF-8 Converters - wide char is unicode */

create_btowc_2(utf8,flw)
create_mbtowc_2(utf8,flw)
create_mblen_2(utf8,flw)
create_mbstowcs_2(utf8,flw)
create_wctob_2(flw,utf8)
create_wctomb_2(flw,utf8)
create_wcstombs_2(flw,utf8)

create_ICONV_LIB_METHODS(utf8)

/* Unicode Converters - wide char is unicode */

create_btowc_3(flb,idxw,flw)
create_mblen_3(flb,idxw,flw)
create_mbtowc_3(flb,idxw,flw)
create_mbstowcs_3(flb,idxw,flw)
create_wctob_3(flw,idxb,flb)
create_wctomb_3(flw,idxb,flb)
create_wcstombs_3(flw,idxb,flb)

create_ICONV_LIB_METHODS(sbcs_ucs_w)
/* Unicode Converters - wide char is unicode */

create_btowc_3(flb,idxh,flw)
create_mblen_3(flb,idxh,flw)
create_mbtowc_3(flb,idxh,flw)
create_mbstowcs_3(flb,idxh,flw)

dnl Use previous wc-mb routines
dnl

create_ICONV_LIB_METHODS(sbcs_ucs_h)

create_btowc_3(flb,7dbcs,flw)
create_mblen_3(flb,7dbcs,flw)
create_mbtowc_3(flb,7dbcs,flw)
create_mbstowcs_3(flb,7dbcs,flw)
create_wctob_3(flw,idxh,mbs)
create_wctomb_3(flw,idxh,mbs)
create_wcstombs_3(flw,idxh,mbs)

create_ICONV_LIB_METHODS(7dbcs_ucs)

create_btowc_3(flb,dbcs,flw)
create_mblen_3(flb,dbcs,flw)
create_mbtowc_3(flb,dbcs,flw)
create_mbstowcs_3(flb,dbcs,flw)

dnl Use previous wc-mb routines
dnl

create_ICONV_LIB_METHODS(dbcs_ucs)

create_btowc_3(flb,euc,flw)
create_mblen_3(flb,euc,flw)
create_mbtowc_3(flb,euc,flw)
create_mbstowcs_3(flb,euc,flw)
create_wctob_3(flw,idxw,mbs)
create_wctomb_3(flw,idxw,mbs)
create_wcstombs_3(flw,idxw,mbs)

create_ICONV_LIB_METHODS(euc_ucs)


