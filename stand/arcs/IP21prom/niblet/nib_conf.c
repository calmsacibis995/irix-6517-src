#include "nib_procs.h"
#include "niblet.h"

int test_array[][PROCS_PER_TEST + 1] = {
    {INVALID, -1},                   /* 0x0 */
    {MEMTEST, -1},                   /* 0x1 */
    {COUNTER, -1},                   /* 0x2 */
    {COUNTER, COUNTER, -1},          /* 0x3 */
    {MEMTEST, MEMTEST, -1},          /* 0x4 */
    {COUNTER, MEMTEST, -1},          /* 0x5 */
    {COUNTER, INVALID, -1},          /* 0x6 */
    {MPROVE, MPROVE, -1},            /* 0x7 */
    {MPINTADD, MPINTADD, -1},        /* 0x8 */
    {MPMON, MPMON, -1},              /* 0x9 */
    {MPHLOCK, MPHLOCK, -1},          /* 0xa */

	{INVALID, MPROVE, MPROVE, -1},     /* 0xb */
    {INVALID, COUNTER, COUNTER, -1},   /* 0xc */ 
    {MPINTADD, INVALID, MPINTADD, -1}, /* 0xd */
    {MPSLOCK, MPSLOCK, INVALID, -1},   /* 0x3 */

    {MPINTADD, MPSLOCK, MPINTADD, MPSLOCK, INVALID, -1},  /* 0xf */ 
    {MEMTEST, MEMTEST, MEMTEST, MEMTEST, MEMTEST, -1},    /* 0x10 */
    {MPSLOCK, MPMON, INVALID, MPSLOCK, MPMON, -1},        /* 0x11 */
    {MPINTADD_4_BIG, MPINTADD_4_BIG, MPINTADD_4_BIG, MPINTADD_4_BIG, -1}, /* 0x12 */

    {MEMTEST, MEMTEST, MEMTEST, INVALID, INVALID, INVALID, -1},           /* 0x13 */

    {INVALID, MPMON, MPMON, MPSLOCK, MPSLOCK, 
     MPINTADD, MPINTADD, MPHLOCK, MPHLOCK, -1},                           /* 0x14 */ 

    {MPINTADD_4_BIG, MPINTADD_4_BIG, MPINTADD_4_BIG, MPINTADD_4_BIG,
     INVALID, MPMON, MPSLOCK,
     MPHLOCK, MPHLOCK, MPSLOCK, MPMON, -1},                               /* 0x15 */

    {MPINTADD_4_BIG, MPINTADD_4_BIG, MPINTADD_4_BIG, MPINTADD_4_BIG,
     MPROVE_BIG, MPROVE_BIG, MPROVE_BIG, 
     MPHLOCK_BIG, MPHLOCK_BIG, 
     MEMTEST, MEMTEST, MEMTEST, 
     INVALID, -1},                                                         /* 0x16 */ 

    {MPROVE, MPROVE, MPROVE, MPROVE,
    MPROVE, MPROVE, MPROVE, MPROVE, -1},                                   /* 0x17 */
};

int num_tests = sizeof(test_array) / (sizeof(int) * PROCS_PER_TEST + 1);
int avail_procs = NUM_PROCS;

