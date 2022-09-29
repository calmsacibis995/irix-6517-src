/* __file__
 * iconv_top_src.m4
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
#include    <unistd.h>
#include    <errno.h>
#include    <sys/types.h>

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

czz_load_defs(fl,int,,cnv,iconv_cnv_nul.m4)

/* unicode to iso-8859-1 converter template , it is basically a copy	*/
czz_load_defs(lat1,,,cnv,iconv_cnv_lat1.m4)

/* euc converter							*/
czz_load_defs(euc,,,cnv,iconv_cnv_euc.m4)

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

/* mbs output templates							*/
czz_load_defs(mbs,unsigned char,,out,iconv_mbs_out.m4)

/* This is the master template						*/
include(iconv_tmpl.m4)

/* TOP __file__ line __line__ ================================== */
/* UTF8 to 8bit code - 8859-?, koi8 etc */
create_iconv_3(utf8,idxb,flb)
/* TOP __file__ line __line__ ================================== */
/* 8bit code - 8859-?, koi8 etc to UTF8 */
create_iconv_3(flb,idxh,utf8)
/* TOP __file__ line __line__ ================================== */
/* 8bit code - 8859-?, koi8 etc to Unicode to 8 bit code */
create_iconv_4(flb,idxh,idxb,flb)

/* TOP __file__ line __line__ ================================== */
/* UTF8 to iso8859-1 special */
create_iconv_3(utf8,lat1,flb)
/* TOP __file__ line __line__ ================================== */
/* iso8859-1 to UTF8 special */
create_iconv_2(flb,utf8)
