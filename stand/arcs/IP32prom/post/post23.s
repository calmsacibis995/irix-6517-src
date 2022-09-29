#include <asm.h>
#include <regdef.h>


/* Dummy post routines */
	
#define post2FSIZE 16
NESTED(post2,post2FSIZE,ra)		# NOTE:	 Temporary post2
	subu	sp,post2FSIZE
	sw	ra,8(sp)
  #	TRACE(POST2_ENTRY)
	lw	ra,8(sp)
	addu	sp,post2FSIZE
	jr	ra
	END(post2)

#define post3FSIZE 16
NESTED(post3,post3FSIZE,ra)		# NOTE:	 Temporary post3
	subu	sp,post3FSIZE
	sw	ra,8(sp)
  #	TRACE(POST3_ENTRY)
	lw	ra,8(sp)
	addu	sp,post3FSIZE
	jr	ra
	END(post3)	
