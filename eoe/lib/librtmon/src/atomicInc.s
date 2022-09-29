.weakext	atomicInc, _atomicInc
    .globl _atomicInc
    .ent _atomicInc 2
	.set	noreorder	
_atomicInc:
L_try_again:
    ll      $8,0($4)        #load linked the contents of the address
    addiu   $9, $8, 1       #add 1
    sc      $9,0($4)
    beqzl   $9,L_try_again
    nop
    j       $31
    move    $2, $8
	.set	reorder
    .end    _atomicInc
	
