/*
 * sgiversion.c
 *	The latest and greatest version number
 */

#include "ffsc.h"

#define FFSC_VERSION	"2.3"

#ifdef PRODUCTION
#define COMPLETE_VERSION	FFSC_VERSION
#else
#define COMPLETE_VERSION	FFSC_VERSION " " __DATE__ " " __TIME__
#endif  /* PRODUCTION */

/* the string "MMSC VERSION" will be put into the binary file right 
   before the actual version in ffscVersion.  This will allow flashmmsc
   to look for "MMSC VERSION" and find the actual version number right
   after it.  The string is spelled out by characters so that a terminating
   0 won't be put in by the compiler.
   */

const char placeHolder[13] = {'M','M','S','C',' ','V','E','R','S','I','O','N',' '}; 
const char ffscVersion[] = COMPLETE_VERSION;
