*	- DEF6811B.INC -
*
*	Include file containing 6801/6301/68HC11
*	differences.

	P68H11
proc6811	DEFINE	1
proc6801	DEFINE	0
proc6301	DEFINE	0
banking		DEFINE	1

	MACRO	%XGDX
	XGDX
	ENDMAC

