
#ifndef _DMA_DEF_H_
#define _DMA_DEF_H_

#include "sys/cosmo2_drv.h"
#include "sys/cosmo2_clrb.h"
#include "sys/cosmo2_locks.h"
#include "sys/cosmo2_dma.h"

#define MFG_COSMO2BoardLimit 1

#define	DespTableSize	3 
#define DMA_DATA_SIZE 32

#define DEFAULT_DMA_CONTROL 0
#define DEFAULT_BUG (cgi1_DTP_Overwrite_0|cgi1_DTP_Overwrite_1|cgi1_DTP_Overwrite_2|cgi1_DTP_Overwrite_3|cgi1_InvalidDTP|cgi1_InvalidDescAddr|cgi1_DataUnderflowError)

#define DEFAULT_INTR ~(cgi1_CH0_I0|cgi1_CH0_I1|cgi1_CH1_I0|cgi1_CH1_I1|cgi1_CH2_I0|cgi1_CH2_I1|cgi1_CH3_I0|cgi1_CH3_I1|DEFAULT_BUG)

#define DMA_RESET cgi1_RESET_3|cgi1_RESET_2|cgi1_RESET_1|cgi1_RESET_0

#endif
