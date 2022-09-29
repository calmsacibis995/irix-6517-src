#if defined(IP22)
 # mgv_llvgi1_asm.s.s
 #
 #      >>> Double word load and store routines <<<
 #
 #  Leveraged from Cosmo2
 #
 #
 # Copyright 1995, Silicon Graphics, Inc.
 # ALL RIGHTS RESERVED
 #
 # UNPUBLISHED -- Rights reserved under the copyright laws of the United
 # States.   Use of a copyright notice is precautionary only and does not
 # imply publication or disclosure.
 #
 # U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 # Use, duplication or disclosure by the Government is subject to restrictions
 # as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 # in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 # in similar or successor clauses in the FAR, or the DOD or NASA FAR
 # Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 # 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 #
 # THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 # INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 # DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 # PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 # GRAPHICS, INC.
 #


	.option	pic0
	.text	
	.align	2
	.file	2 "mgv_llvgi1_asm.s"
	.globl	mgv_vgi1_store_ll
 #   1	void mgv_vgi1_store_ll(ulonglong_t val, ulonglong_t *ploc);
	.ent	mgv_vgi1_store_ll 2
mgv_vgi1_store_ll:
	.frame	$sp, 0, $31
 #   register 4 contains top, register 5 contains bottom of value
 #   register 6 points to memory	
	dsll $4,$4,32                  # convert int top to double phase 1
 #   3  
 #   4                          *loc = (top2<<32 | bottom);
        dsll $2,$5,32                  #  convert int bottom to double phase 1  
        dsrl $4,$4,32                  #  convert int top to double phase 2 
        dsrl $2,$2,32                  #  convert int bottom to double phase 2  
        dsll $4,$4,32                  #  shift top left 32 bits  
        or $4,$4,$2                    #  or with bottom;   
 #   3	
 #   4				*loc =  value;
	.set volatile
        sd $4,0($6)                    #  store double what we wanted to do
	.set novolatile

 #   5	}
	.livereg	0x0000FF0E,0x00000FFF
	j	$31
	.end	mgv_vgi1_store_ll
	.text	
	.align	2
	.file	2 "mgv_llvgi1_asm.s"
	.globl	mgv_vgi1_load_ll
 #  16					
 #  17	ulonglont_t mgv_vgi1_load_ll(ulonglong_t *ploc);
	.ent	mgv_vgi1_load_ll 2
mgv_vgi1_load_ll:
	.frame	$sp, 0, $31
 #  18	
 #  19				return *loc;
 # debugging stuff
 # end debugging stuff
	.set volatile
	ld $4,0($4)                    # load double 
	.set novolatile
	dsrl $2,$4,32                  # [1]  
	sll	$2,$2,0			# high 32 bits
	sll	$3,$4,0			# low 32 bits
	.livereg	0x3000FF0E,0x00000FFF
	j	$31
	.end	mgv_vgi1_load_ll
#endif
