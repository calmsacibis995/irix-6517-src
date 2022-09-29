extern	void __missing_function_user_error_trap(void);

/*
 ***************************************************************************
 *	__missing_function_
 ***************************************************************************
 */

void __missing_function_() {

	/*  
	 *  __missing_function_user_error_trap is just a BREAK 13 
	 *  instruction to force a trap 
	 */
	__missing_function_user_error_trap();

}

#pragma	weak	__missing_function_

#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 721))
#pragma	optional __missing_function_
#endif


/*
 ***************************************************************************
 *	__missing_object_		(not used)
 *
 *	Currently, we don't know the best way to deal with the issue
 ***************************************************************************
 */

