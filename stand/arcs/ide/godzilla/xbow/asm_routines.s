#include <asm.h>
#include <sys/R10k.h>
#include <regdef.h>
#include <sys/sbd.h>


  .text

  .set noreorder

/* void rot_pattern_write(ptr,size,data);
        ptr -- target memory address
        size -- number of cache-lines to output
        data -- data to write (will be shifted left by
                one byte each dw)
        */
LEAF(rot_pattern_write)
        .set noreorder
1:      sd      a2, 0x00(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x08(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x10(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x18(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x20(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x28(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x30(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x38(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x40(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x48(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x50(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x58(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x60(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x68(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x70(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        sd      a2, 0x78(a0)
        dsrl32  a3, a2, 24
        dsll    a2, a2, 8
        or      a2, a2, a3
        daddiu  a1, -1
        bne     zero, a1, 1b
        daddiu  a0, 0x80
        j       ra
        nop
        .set reorder

        END(rot_pattern_write)
