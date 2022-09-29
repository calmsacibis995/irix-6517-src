/* __file__
 * stdlib_top_defn.m4
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

dnl
dnl czz_load_defs loads the converter segments
dnl
divert(-1)
define(blow_ICONV_LIB_METHODS,
    `
	define(`CZZ_btowc',_nil_func)
	define(`CZZ_mbtowc',_nil_func)
	define(`CZZ_mblen',_nil_func)
	define(`CZZ_mbstowcs',_nil_func)
	define(`CZZ_mbwcctl',0)

	define(`CZZ_wctob',_nil_func)
	define(`CZZ_wctomb',_nil_func)
	define(`CZZ_wcstombs',_nil_func)
	define(`CZZ_wcmbctl',0)
    '
)

define(create_ICONV_BLOCK,`
{   /* '__file__` line '__line__` ===================================== */
    ICONV_MAGIC_NUM,	/* int	    iconv_magic;    Magic word		*/

    (ICONV_BTOWC) CZZ_btowc,	/* ICONV_BTOWC	iconv_btowc;  ptr to btowc function	*/
    (ICONV_MBTOWC) CZZ_mbtowc,	/* ICONV_MBTOWC	iconv_mbtowc; ptr to mbtowc function	*/
    (ICONV_MBLEN) CZZ_mblen,	/* ICONV_MBLEN	iconv_mblen;  ptr to mblen function	*/
    (ICONV_MBSTOWCS) CZZ_mbstowcs,	/* ICONV_MBSTOWCS iconv_mbtowcs; ptr to mbtowcs func */
    (ICONV_CTLFUNC) CZZ_mbwcctl,	/* ICONV_CTLFUNC iconv_mbwcctl;	ptr to ctl func for mbtowc */
        
    (ICONV_WCTOB) CZZ_wctob,	/* ICONV_WCTOB	iconv_wctob;  ptr to wctob function	*/
    (ICONV_WCTOMB) CZZ_wctomb,	/* ICONV_WCTOMB	iconv_wctomb; ptr to wctomb function	*/
    (ICONV_WCSTOMBS) CZZ_wcstombs,	/* ICONV_WCSTOMBS iconv_wcstombs; ptr to wcstombs function	*/
    (ICONV_CTLFUNC) CZZ_wcmbctl,	/* ICONV_CTLFUNC iconv_wcmbctl; ptr to ctl func for wctomb	*/

    /* long		iconv_fill[  ];    Expansion room		*/
}
')

define(create_default_iconv_mbhandle,
`/* '__file__` line '__line__` ================================== */
struct _iconv_mbhandle_s  __libc_mbhandle[ 1 ] = {
{
    { 0, 0 },			/* void	      * iconv_handle[2];	*/
    0,				/* struct ICONV_LIBSPEC  * iconv_spec;	*/
    create_ICONV_BLOCK($1)
}
};
')

define(create_ICONV_LIB_METHODS,
`/* '__file__` line '__line__` ================================== */
syscmd(echo "__mbwc_$1" >> CZZ_SYMSFILE)
syscmd(echo "extern const ICONV_LIB_METHODS __mbwc_$1[];" >> CZZ_DCLSFILE)
const ICONV_LIB_METHODS __mbwc_$1[1] = {
    create_ICONV_BLOCK($1)
};
')
divert

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
czz_load_defs(sjiswc,,,cnv,stdlib_cnv_sjiswc.m4)
czz_load_defs(big5wc,,,cnv,stdlib_cnv_big5wc.m4)
czz_load_defs(eucjpwc,,,cnv,stdlib_cnv_eucjpwc.m4)
czz_load_defs(eucjp3wc,,,cnv,stdlib_cnv_eucjp3wc.m4)
czz_load_defs(euctwwc,,,cnv,stdlib_cnv_euctwwc.m4)
czz_load_defs(dbcswc,,,cnv,stdlib_cnv_dbcswc.m4)

/* Fixed length input templates for byte and aligned and unaligned 2 and 4 byte sequences */
czz_load_defs(flb,unsigned char,,arg,iconv_arg_in.m4)
czz_load_defs(flw,unsigned int,,arg,iconv_arg_in.m4)
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

/* mbs output templates                                                 */
czz_load_defs(mbs,unsigned char,,out,iconv_mbs_out.m4)
czz_load_defs(wceucjpmbs,unsigned char,,out,stdlib_wceucjp_out.m4)
czz_load_defs(wceucjp3mbs,unsigned char,,out,stdlib_wceucjp3_out.m4)
czz_load_defs(wceuctwmbs,unsigned char,,out,stdlib_wceuctw_out.m4)
czz_load_defs(wcsjismbs,unsigned char,,out,stdlib_wcsjis_out.m4)
czz_load_defs(wcbig5mbs,unsigned char,,out,stdlib_wcbig5_out.m4)
czz_load_defs(wcdbcsmbs,unsigned char,,out,stdlib_wcdbcs_out.m4)

/* sbs output templates                                                 */
czz_load_defs(wcsbcsmbs,unsigned char,,out,stdlib_wcsbcs_out.m4)

/* utf8 input and output templates					*/
czz_load_defs(utf8,unsigned char,,arg,iconv_argutf8_in.m4)
czz_load_defs(utf8,unsigned char,,in,iconv_utf8_in.m4)
czz_load_defs(utf8,unsigned char,,out,iconv_utf8_out.m4)

/* table index converter template for byte, 2 byte and 4 byte wide tables*/
czz_load_defs(idxb,unsigned char,,cnv,iconv_cnv_idx.m4)
czz_load_defs(idxh,unsigned short,,cnv,iconv_cnv_idx.m4)
czz_load_defs(idxw,unsigned int,,cnv,iconv_cnv_idx.m4)


/* This is the master iconv template					*/
include(iconv_tmpl.m4)
include(iconv_ctr.m4)

/* This is the master stdlib templates					*/
include(btowc_tmpl.m4)
include(mblen_tmpl.m4)
include(mbtowc_tmpl.m4)
include(mbstowcs_tmpl.m4)
include(wctob_tmpl.m4)
include(wctomb_tmpl.m4)
include(wcstombs_tmpl.m4)

