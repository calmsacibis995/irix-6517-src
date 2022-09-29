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

/* Get all macro definitions */
include(iconv_top_defn.m4)


/* TOP __file__ line __line__ ================================== */
/* UTF8 to iso8859-1 special */
create_iconv_3(utf8,lat1,flb)
/* TOP __file__ line __line__ ================================== */
/* iso8859-1 to UTF8 special */
create_iconv_2(flb,utf8)
/* TOP __file__ line __line__ ================================== */
/* iso8859-1 to UCS-2 special */
create_iconv_2(flb,flh)
/* TOP __file__ line __line__ ================================== */
/* UCS-2 to iso8859-1 special */
create_iconv_2(flh,flb)
