#include <synonyms.h>

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

void __C_runtime_error (int trap_code, char *name, int line_no, ... )
{
	
	char *fmt;
	va_list ap;
	
	va_start ( ap, line_no );
	
	switch ( trap_code ) {
		
	/* For subscript range violations, the varargs arguments are a
	 * printf format followed by whatever arguments it requires, e.g.:
	 *
	 *  __rt_error ( BRK_RANGE, "proc", line,
	 *               "array '%s', subscript %d", "vec", sub );
	 *  __rt_error ( BRK_RANGE, "proc", line,
	 *               "%s(%d,%d)", "vec", sub1, sub2 );
	 */

	case BRK_RANGE:
		fprintf ( stderr, "error: Subscript range violation in '%s'", name );
		if ( line_no != -1 )
			fprintf ( stderr, " (line %d)", line_no );
		fmt = (char *) va_arg(ap, char*);
		fprintf ( stderr, ": " );
		vfprintf ( stderr, fmt, ap );
		break;

		/* For divide by zero and overflow, there are no extra arguments,
		 * e.g.:
		 *
		 *  __rt_error ( BRK_DIVZERO, "proc", line );
		 *  __rt_error ( BRK_OVERFLOW, "proc", line );
		 */
	case BRK_DIVZERO:
		fprintf ( stderr, "error: Divide by zero in '%s'", name );
		if ( line_no != -1 )
			fprintf ( stderr, " (line %d)", line_no );
		break;
	case BRK_OVERFLOW:
		fprintf ( stderr, "error: Overflow in '%s'", name );
		if ( line_no != -1 )
			fprintf ( stderr, " (line %d)", line_no );
		break;

		/* For others (unknown trap codes), the varargs arguments are a
		 * printf format followed by whatever arguments it
		 * requires, just like range violations.
		 */
	default:
		fprintf ( stderr, "error: Trap %d in '%s'", trap_code, name );
		if ( line_no != -1 )
			fprintf ( stderr, " (line %d)", line_no );
		fmt = (char *) va_arg(ap, char*);
		fprintf ( stderr, ": " );
		vfprintf ( stderr, fmt, ap );
		break;
		
	}

	fprintf ( stderr, "\n" );

	exit (99);
}
