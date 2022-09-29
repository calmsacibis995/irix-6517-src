.weakext	atomicIncWithWrapAndTime64, _atomicIncWithWrapAndTime64	
    .globl _atomicIncWithWrapAndTime64
    .ent _atomicIncWithWrapAndTime64 2
_atomicIncWithWrapAndTime64:	
LM_try_again64:
    ll      $8,0($4)        #load linked the contents of the address
    addiu   $9, $8, 1       #add 1
LM_timer_wrapped64:		
    lw	    $11, 0($7)
    lw	    $12, 4($7)
    addiu   $12, $12, 1	
    lw	    $13, 0($7)
    bne     $11, $13, LM_timer_wrapped64
    sw      $11, 0($6)
    sw      $12, 4($6)
    bne	    $9,$5,LM_normal64
    andi    $9, $9, 0
LM_normal64:		
    sc      $9,0($4)
    beq     $9,0,LM_try_again64
    move    $2, $8
    j       $31
    .end    _atomicIncWithWrapAndTime64

.weakext	atomicIncWithWrapAndTime32, _atomicIncWithWrapAndTime32	
    .globl _atomicIncWithWrapAndTime32
    .ent _atomicIncWithWrapAndTime32 2
_atomicIncWithWrapAndTime32:	
LM_try_again32:
    ll      $8,0($4)		# load linked the contents of the address
    addiu   $9, $8, 1		# add 1
    lw	    $11, 0($7)		# get timestamp		
    sw      $11, 0($6)		# store timestamp
    bne	    $9,$5,LM_normal32	# check for index wrap
    andi    $9, $9, 0		# wrap index
LM_normal32:		
    sc      $9,0($4)		
    beq     $9,0,LM_try_again32
    move    $2, $8
    j       $31
    .end    _atomicIncWithWrapAndTime32
