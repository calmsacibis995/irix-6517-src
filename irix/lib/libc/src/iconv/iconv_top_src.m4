#if defined(_COMPILER_VERSION)
#pragma set woff 3201	/* the "handle" param isn't referenced by many of
	* the generated functions, and it's too painful to turn this warning
	* off at each one; but only want this for non-o32, because o32 doesn't
	* understand the pragma and warns, and o32 doesn't warn about unused
	* parameters in the first place... */
#endif

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

define(`CZZ_SYMSFILE',`iconv.syms')
syscmd(rm -f CZZ_SYMSFILE)
define(`CZZ_DCLSFILE',`iconv.dcls.h')
syscmd(rm -f CZZ_DCLSFILE)

/* Get all macro definitions */
include(iconv_top_defn.m4)

include(../computed_include/iconv_top_include.m4)

/* Control function */
/* TOP __file__ line __line__ ================================== */
/* control function */
create_control()
