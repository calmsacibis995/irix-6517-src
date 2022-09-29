.weakext	atomicDec, _atomicDec
    .globl _atomicDec
    .ent _atomicDec 2
_atomicDec:
LL_try_again:
    ll      $8,0($4)        #load linked the contents of the address
    addiu   $9, $8, -1       #sub 1
    sc      $9,0($4)
    beq     $9,0,LL_try_again
    move    $2, $8
    j       $31
    .end    _atomicDec
	
