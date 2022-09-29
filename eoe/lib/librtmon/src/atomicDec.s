.weakext	atomicDec, _atomicDec
    .globl _atomicDec
    .ent _atomicDec 2
	.set	noreorder
_atomicDec:
LL_try_again:
    ll      $8,0($4)        #load linked the contents of the address
    addiu   $9, $8, -1      #sub 1
    sc      $9,0($4)
    beqzl   $9,LL_try_again
    nop
    j       $31
    move    $2, $8
	.set	reorder
    .end    _atomicDec
	
