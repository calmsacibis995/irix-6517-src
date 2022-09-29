#if !defined(__ERROR_NUMBERS_H__)
#define __ERROR_NUMBERS_H__

/* Define error numbers reported for whenever a parallel port diagnostic  */
/* fails.                                                                 */

#define    CONTEXT_A_STUCK_AT_ONE                      1
#define    CONTEXT_A_STUCK_AT_ZERO                     2
#define    CONTEXT_B_STUCK_AT_ONE                      3
#define    CONTEXT_B_STUCK_AT_ZERO                     4
#define    PLP_DMA_RESET_BIT_STUCK_AT_ZERO             5
#define    PLP_DMA_RESET_BIT_STUCK_AT_ONE              6
#define    PLP_DMA_DIRECTION_BIT_STUCK_AT_ZERO         7
#define    PLP_DMA_DIRECTION_BIT_STUCK_AT_ONE          8
#define    PLP_DMA_ENABLE_BIT_STUCK_AT_ZERO            9
#define    PLP_DMA_ENABLE_BIT_STUCK_AT_ONE            10
#define    CONTROL_BAD_AFTER_RESET                    11
#define    DIAGNOSTIC_BAD_AFTER_RESET                 12
#define    CONTEXT_A_WRITE_TIMEOUT_DURING_LOOPBACK    13
#define    CONTEXT_B_WRITE_TIMEOUT_DURING_LOOPBACK    14
#define    CONTEXT_A_READ_TIMEOUT_DURING_LOOPBACK     15
#define    CONTEXT_B_READ_TIMEOUT_DURING_LOOPBACK     16

#endif
