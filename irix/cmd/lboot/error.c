#include <sys/types.h>
#include "lboot.h"

#ident "$Revision: 2.28 $"

static struct errortab {
	enum error_num msgnum;
	enum {
		_PERROR_,		/* perror(text); return(0) */
		_PERROR_EXIT_,		/* perror(text); exit(2) */
		_RETURN_,		/* return(0) */
		_PANIC_			/* exit(2) */
	} action;
	char* text;
} errortab[] = {
	{ ER1, _PANIC_, "%s\n" },
	{ ER2, _PERROR_EXIT_, "" },
	{ ER7, _PERROR_, "" },
	{ ER13, _RETURN_, "Driver %s: major number greater than NMAJOR\n" },
	{ ER15, _RETURN_, "Driver %s: not a valid object file\n" },
	{ ER16, _RETURN_, "%s: %s object format is incompatible with %s kernel; object ignored\n" },
	{ ER18, _RETURN_, "INCLUDE: %s; driver not found\n" },
	{ ER19, _RETURN_, "INCLUDE: %s; driver is EXCLUDED\n" },
	{ ER20, _RETURN_, "INCLUDE: %s; device not equipped\n" },
	{ ER21, _RETURN_, "EXCLUDE: %s; driver is INCLUDED\n" },
	{ ER22, _RETURN_, "%s: dependent driver %s not available\n" },
	{ ER23, _RETURN_, "%s: dependent driver %s is EXCLUDED\n" },
	{ ER24, _RETURN_, "%s: device not equipped for dependent driver %s\n" },
	{ ER32, _PANIC_, "%s: not an object file\n" },
	{ ER33, _PANIC_, "%s: no section headers\n" },
	{ ER36, _RETURN_, "%s: required driver is EXCLUDED\n" },
	{ ER37, _RETURN_, "%s: flagged as ONCE only; #C set to 1\n" },
	{ ER49, _PANIC_, "%s: truncated read\n" },
	{ ER53, _RETURN_, "%s, line %d: %s\n" },
	{ ER55, _PANIC_, "mtune file '%s' line %d:%s\n" },
	{ ER100, _PANIC_, "%s is not a directory\n" },
	{ ER101, _PANIC_, "Failed to find 'KERNEL:' in %s\n" },
	{ ER102, _PANIC_, "Driver %s: conflicting interrupt destination for the same IRQ level\nInterrupt is being asked to send to cpu %d\n" },
	{ ER103, _PANIC_, "Conflicting interrupt destination for IRQ level %d\nDriver %s to cpu %d, driver %s to cpu %d\n" },
	{ ER104, _PANIC_, "Conflicting interrupt destination for the IRQ level %d\n" },
	{ ER106, _PANIC_, "All IRQ levels of proc locked driver %s must be locked to the same cpu\nCurrent IRQ level %d is locked to cpu %d\n" },
	{ ER107, _PANIC_, "Driver %s, unit %d, and driver %s, unit %d, have the same IRQ or vector, %x\n" },
	{ ER108, _PANIC_, "More than 254 interrupts"},
	{ ER109, _PANIC_, "Illegal cpuid specified to NOINTR"},
	{ ER110, _RETURN_,"stune file '%s' line %d:%s\n"},
	{ ER120, _RETURN_, "Loadable module registration may not have been done (e.g. tpsc)\nSee mload(4) manual page for more information on auto registration\n" },
	{ ER121, _PANIC_, "Driver %s missing \'devflag\'\n"},
	{ ER122, _RETURN_, "%s not specified in system file\n" },
	{ ER0 } };

/*
 * Print an error message, and determine the recovery action (if any) to
 * be taken. 
 */

/*VARARGS1*/
void error(enum error_num msgnum, ...)
{
	register struct errortab *ep;
	va_list ap;

	va_start(ap, msgnum);

	for (ep=errortab; ep->msgnum != ER0; ++ep) {
		if (ep->msgnum == msgnum) {

			switch (ep->action) {
			case _PANIC_:
				fprintf(stderr, "lboot:ERROR:");
				vfprintf(stderr, ep->text, ap);
				va_end(ap);
				myexit(2);
			case _PERROR_:
				fprintf(stderr, "lboot:WARNING:");
				perror(va_arg(ap, char *));
				va_end(ap);
				return;
			case _PERROR_EXIT_:
				fprintf(stderr, "lboot:ERROR:");
				perror(va_arg(ap, char *));
				va_end(ap);
				/* 
				 * Attempt to register loadable modules if 
				 * possible.
				 */
				do_mregister();
				myexit(2);
			case _RETURN_:
				fprintf(stderr, "lboot:WARNING:");
				vfprintf(stderr, ep->text, ap);
				va_end(ap);
				return;
			}

			break;
		}
	}
	va_end(ap);
	fprintf(stderr, "Unknown error number");
}
