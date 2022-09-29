#ident "$Revision: 3.16 $"
#include "sys/asm.h"
#include "sys/regdef.h"


	.weakext	uspsema, _uspsema
	.weakext	uscpsema, _uscpsema
	.weakext	usvsema, _usvsema
	.globl	_Psema
	.globl	_CPsema
	.globl	_Vsema

LEAF(_uspsema)
	SETUP_GP
	USE_ALT_CP(t0)
	SETUP_GP64(t0,_uspsema)
	move	a1,ra
	LA	t9,_Psema
	j	t9
	END(_uspsema)

LEAF(_uscpsema)
	SETUP_GP
	USE_ALT_CP(t0)
	SETUP_GP64(t0,_uscpsema)
	move	a1,ra
	LA	t9,_CPsema
	j	t9
	END(_uscpsema)

LEAF(_usvsema)
	SETUP_GP
	USE_ALT_CP(t0)
	SETUP_GP64(t0,_usvsema)
	move	a1,ra
	LA	t9,_Vsema
	j	t9
	END(_usvsema)
