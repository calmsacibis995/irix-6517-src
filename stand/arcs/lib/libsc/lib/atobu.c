/*
 * atobu.c
 *
 * $Revision: 1.6 $
 *
 */

#include <libsc.h>

/*
 * Convert an alphanumeric string to binary (int), converting from the units
 * specified by a suffix into bytes.
 */
char *
atobu(const char *cp, unsigned *intp)
{
	cp = atob(cp, (int *)intp);
	switch (*cp) {
	  default:			/* unknown */
		return (char*)cp;
	  case 'k':			/* kilobytes */
	  case 'K':
		*intp *= 0x400;
		break;
	  case 'm':			/* megabytes */
	  case 'M':
		*intp *= 0x100000;
		break;
	  case 'p':			/* pages */
	  case 'P':
		*intp *= 4096;
		break;
	}
	return ((char*)cp + 1);
}

char *
atobu_ptr(const char *cp, __psunsigned_t *iptr)
{
#if (_MIPS_SZPTR == 32)
	return atobu(cp, (unsigned int *)iptr);
#elif (_MIPS_SZPTR == 64)
	return atobu_L(cp, (unsigned long long *)iptr);
#else
	<<<BOMB>>>
#endif
}

/*
 * Convert an alphanumeric string to binary (long long), converting from the
 * units specified by a suffix into bytes.
 */
char *
atobu_L(const char *cp, unsigned long long *intp)
{
	cp = atob_L(cp, (long long *)intp);
	switch (*cp) {
	  default:			/* unknown */
		return (char*)cp;
	  case 'k':			/* kilobytes */
	  case 'K':
		*intp *= 0x400;
		break;
	  case 'm':			/* megabytes */
	  case 'M':
		*intp *= 0x100000;
		break;
	  case 'p':			/* pages */
	  case 'P':
		*intp *= 4096;
		break;
	}
	return (char*)cp + 1;
}
