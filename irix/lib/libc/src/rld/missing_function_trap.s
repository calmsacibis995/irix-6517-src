#include	<asm.h>
#include	<regdef.h>

	.text
.weakext __missing_function_user_error_trap
LEAF(__missing_function_user_error_trap)

	break 13
	j ra

END(__missing_function_user_error_trap)
