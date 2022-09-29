/* __file__
 * iconv_top_defn.m4
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
dnl
dnl This is an m4 script to create iconv translation functions
dnl

/* __file__ line __line__ ================================== */
#include    "synonyms.h"
#include    <wchar.h>
#include    <locale.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <unistd.h>
#include    <errno.h>
#include    <sys/types.h>
#include    "iconv_cnv.h"
#include    "_wchar.h"

#define EUCWC_SHFT  28


/* __file__ line __line__ ================================== */

dnl
dnl czz_load_defs loads the converter segments
dnl
define(
    czz_load_defs,
    `
	pushdef(`CZZ_NAME',$1$3)
	pushdef(`CZZ_CNAME',czz_tpl_$4_$1$3)
	pushdef(`CZZ_TNAME',czz_tpl_t_$4_$1$3)
	pushdef(`CZZ_UNAME',czz_tpl_u_$4_$1$3)
	pushdef(`CZZ_TYPE',$2)
	pushdef(`CZZ_PACK',$3)
include($5)
	popdef(`CZZ_NAME')
	popdef(`CZZ_CNAME')
	popdef(`CZZ_TNAME')
	popdef(`CZZ_UNAME')
	popdef(`CZZ_TYPE')
	popdef(`CZZ_PACK')
    '
)

divert(-1)
# czz_forloop(i, from, to, stmt)

define(`czz_forloop', `pushdef(`$1', `$2')_czz_forloop(`$1', `$2', `$3', `$4')popdef(`$1')')
define(`_czz_forloop',
       `$4`'ifelse($1, `$3', ,
			 `define(`$1', incr($1))_czz_forloop(`$1', `$2', `$3', `$4')')')
divert

define(czz_cat,$1$2$3$4)

define(czz_count,`$1 define(`$1',incr($1))')

czz_load_defs(fl,int,,cnv,iconv_cnv_nul.m4)

/* unicode to iso-8859-1 converter template , it is basically a copy	*/
czz_load_defs(lat1,,,cnv,iconv_cnv_lat1.m4)

/* euc converter							*/
czz_load_defs(euc,,,cnv,iconv_cnv_euc.m4)
czz_load_defs(dbcs,,,cnv,iconv_cnv_dbcs.m4)
czz_load_defs(7dbcs,,,cnv,iconv_cnv_7dbcs.m4)

/* SJIS & EUCJP - to IRIX-WC - this is specifically for output		*/
czz_load_defs(sjiswc,,,cnv,stdlib_cnv_sjiswc.m4)
czz_load_defs(eucjpwc,,,cnv,stdlib_cnv_eucjpwc.m4)

/* BIG5 & EUCTW - to IRIX-WC - this is specifically for output		*/
czz_load_defs(big5wc,,,cnv,stdlib_cnv_big5wc.m4)
czz_load_defs(euctwwc,,,cnv,stdlib_cnv_euctwwc.m4)

/* SJIS & EUCJP Corresponding output templates				*/
czz_load_defs(wcbig5mbs,unsigned char,,out,stdlib_wcbig5_out.m4)
czz_load_defs(wcsjismbs,unsigned char,,out,stdlib_wcsjis_out.m4)

/* BIG5 & EUCTW Corresponding output templates				*/
czz_load_defs(wceucjpmbs,unsigned char,,out,stdlib_wceucjp_out.m4)
czz_load_defs(wceuctwmbs,unsigned char,,out,stdlib_wceuctw_out.m4)

/* table index converter template for byte, 2 byte and 4 byte wide tables*/
czz_load_defs(idxb,unsigned char,,cnv,iconv_cnv_idx.m4)
czz_load_defs(idxh,unsigned short,,cnv,iconv_cnv_idx.m4)
czz_load_defs(idxw,unsigned int,,cnv,iconv_cnv_idx.m4)

/* Fixed length input templates for byte and aligned and unaligned 2 and 4 byte sequences */
czz_load_defs(flb,unsigned char,,in,iconv_fl_in.m4)
czz_load_defs(flh,unsigned short,p,in,iconv_fl_in.m4)
czz_load_defs(flh,unsigned short,,in,iconv_fl_in.m4)
czz_load_defs(flw,unsigned int,p,in,iconv_fl_in.m4)
czz_load_defs(flw,unsigned int,,in,iconv_fl_in.m4)

/* Fixed length input templates for as above				*/
czz_load_defs(flb,unsigned char,,out,iconv_fl_out.m4)
czz_load_defs(flh,unsigned short,p,out,iconv_fl_out.m4)
czz_load_defs(flh,unsigned short,,out,iconv_fl_out.m4)
czz_load_defs(flw,unsigned int,p,out,iconv_fl_out.m4)
czz_load_defs(flw,unsigned int,,out,iconv_fl_out.m4)

/* utf8 input and output templates					*/
czz_load_defs(utf8,unsigned char,,in,iconv_utf8_in.m4)
czz_load_defs(utf8,unsigned char,,out,iconv_utf8_out.m4)

/* utf16 conversion and output templates				*/
czz_load_defs(utf16,unsigned int,,cnv,iconv_cnv_utf16.m4)
czz_load_defs(utf16,unsigned short,,out,iconv_utf16_out.m4)
czz_load_defs(utf16,unsigned short,p,out,iconv_utf16_out.m4)

/* Unicode - Byte Order Mark or Signature byte swapper			*/
czz_load_defs(bom,,,cnv,iconv_cnv_bom.m4)

/* mbs output templates							*/
czz_load_defs(mbs,unsigned char,,out,iconv_mbs_out.m4)

/* UNIX wide character converter					*/
czz_load_defs(uwc,unsigned int,,cnv,iconv_cnv_uwc.m4)

/* X KeySyms converter                                                  */
czz_load_defs(hash,unsigned int,,cnv,iconv_cnv_xkeys.m4)

/* This is the master iconv template					*/
include(iconv_tmpl.m4)
include(iconv_ctr.m4)

/* This is the master stdlib templates					*/
include(mbtowc_tmpl.m4)
include(mbstowcs_tmpl.m4)
include(wctomb_tmpl.m4)
include(wcstombs_tmpl.m4)
