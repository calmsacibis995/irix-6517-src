.weakext	atomicInc, _atomicInc
    .globl _atomicInc
    .ent _atomicInc 2
_atomicInc:
L_try_again:
    ll      $8,0($4)        #load linked the contents of the address
    addiu   $9, $8, 1       #add 1
    sc      $9,0($4)
    beq     $9,0,L_try_again
    move    $2, $8
    j       $31
    .end    _atomicInc
	
