#ident "$Revision: 1.4 $"

#include "lboot.h"
#include <a.out.h>
#include <ldfcn.h>

/*
 * finddriver
 *
 * For each global procedure symbol found in the archive or object 'fname',
 * call 'fun' with the name of the symbol, 'arg0' and 'arg1'.
 */
void
symbol_scan(char *fname,
	    void (*fun)(char *, void *, void *, void *),
	    void *arg0, void *arg1, void *arg2)
{
	LDFILE *ldptr;
	SYMR symr;
	long imax, ibase, isym;
	char *procname;
	int off1 = 0;
	int off2 = 0;

	ldptr = NULL;
	do {
		if ((ldptr = ldopen(fname, ldptr)) != NULL) {
			if ( PSYMTAB(ldptr) == NULL ) {
				fprintf(stderr,
				    "could not read symbol table for %s/%s\n",
				     slash_boot, fname);
				return;
			}
	
			if (!ISCOFF(HEADER(ldptr).f_magic))
				error (ER32, fname); 
			if (((off2 = OFFSET(ldptr)) == off1) && (off1 != 0))
				error (ER32, fname);
			off1 = off2;

			imax = SYMHEADER(ldptr).isymMax + SYMHEADER(ldptr).iextMax;
			ibase = 0;
			/* iBase = SYMHEADER(ldptr).ismMax; */
			/* globals and undefined ? */

			for (isym = ibase; isym < imax; isym++) {

				/* read a symbol */
				if (ldtbread(ldptr, isym, &symr) != SUCCESS) {
					fprintf(stderr,
					 "could not read symbol %ld in %s/%s\n",
						isym, slash_boot, fname);
					return;
					return;
				}

				/* if its not text, continue... */
				if ((symr.sc != scText || symr.st != stProc) &&
				    ((symr.sc != scSData && symr.sc != scData) || symr.st != stGlobal) )
					continue;

				procname = ldgetname(ldptr, &symr);
				if (procname == (char *) NULL)
					continue;
		
				(*fun)(procname, arg0, arg1, arg2);
			}

	   	 } else {
			error (ER32, fname);
		 }

	} while (ldclose(ldptr) != SUCCESS);
}
