.weakext	atomicIncWithWrap, _atomicIncWithWrap	
    .globl _atomicIncWithWrap
    .ent _atomicIncWithWrap 2
_atomicIncWithWrap:
    .set 	noreorder	
LM_try_again:
    ll      $8,0($4)        #load linked the contents of the address
    addiu   $9, $8, 1       #add 1
    bne	    $9,$5,LM_normal
    nop
    move    $9,$0
LM_normal:		
    sc      $9,0($4)
    beqzl   $9,LM_try_again
    nop
    j       $31
    move    $2, $8
    .set    reorder
    .end    _atomicIncWithWrap

