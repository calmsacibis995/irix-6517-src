/* 
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 * 
 * exportsdem.c
 *
 *    Demangles C++ symbol names for use by showrefs -P option for 
 *    generating common.exports files.  The binary 'exportsdem' accepts 
 *    a stream of public symbols found by the showrefs script and combines 
 *    the demangled name as a '#' delimited comment above the symbol name.   
 *    Typical use of this program is:
 *
 *             showrefs -P libAS_32.so > common.exports.32 
 *
 *    The common.exports file is then complete,
 *    with only the need to add a header identifying the file.  Since name
 *    mangling is done differently depending on the -32, -n32 or -64 
 *    compiler switch used, different  common.exports files must be generated 
 *    for each resulting .so library.  The different common.exports files must
 *    then be referenced by the  appropriate DSOEXPORTS macro in the
 *    Makefile as follows: 
 *
 *             DSOEXPORTS_32=common.exports.32
 *             DSOEXPORTS_N32=common.exports.n32 
 *             DSOEXPORTS_64=common.exports.64 
 */

#include <stdio.h>
#include <dem.h>

main()
{
	char sbuf[MAXDBUF];
	DEM d;
	int ret, buf_len;
	char buf[1024];
	char buf2[1024];
	
	while (gets(buf) != NULL) {
		ret = dem(buf, &d, sbuf);
		if ((ret || &d == NULL) && buf[0] != '#') {  
			/* If name cannot be demangled, 
			   and it's not a comment,
			   list and note the problem.  */
			printf("# %s ******* (not demangled) *******\n%s \n"
			       , buf, buf);
		}
		else if (buf[0] == '#') {    /* Print comments as is */
			printf("%s \n", buf);
		}
		else {
			dem_print(&d, buf2);	
			printf("# %s \n%s\n", buf2, buf);
			
		}
	}
}
