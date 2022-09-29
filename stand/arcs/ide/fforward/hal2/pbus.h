/************************************************************************
 *
 * pbus.h --
 *
 *     holds values for pbus specific hardware parameters (read/write
 *     cycle lengths). these values are only valid for bringup and not
 *     the real operation of the pbus system.
 *
 ************************************************************************/

/*#define PIE_EMULATION*/   /* XXX remove when we are through with emulation.*/
				   

#ifdef PIE_EMULATION
#define   HAL2_PIO_RD_P2            1
#define   HAL2_PIO_RD_P3            0x0
#define   HAL2_PIO_RD_P4            0x0
#define   HAL2_PIO_WR_P2            1
#define   HAL2_PIO_WR_P3            0x0
#define   HAL2_PIO_WR_P4            0x0
#define   HAL2_PIO_DS_16            1
#define   HAL2_PIO_EVEN_HIGH        0

#define   AES_PIO_RD_P2             1
#define   AES_PIO_RD_P3             0x0 
#define   AES_PIO_RD_P4             0x0
#define   AES_PIO_WR_P2             1
#define   AES_PIO_WR_P3             0x0
#define   AES_PIO_WR_P4             0x0
#define   AES_PIO_DS_16             0
#define   AES_PIO_EVEN_HIGH         0

#define   VOLUME_PIO_RD_P2          1
#define   VOLUME_PIO_RD_P3          0x0
#define   VOLUME_PIO_RD_P4          0x0
#define   VOLUME_PIO_WR_P2          1
#define   VOLUME_PIO_WR_P3          0x0 
#define   VOLUME_PIO_WR_P4          0x0
#define   VOLUME_PIO_DS_16          0
#define   VOLUME_PIO_EVEN_HIGH      0

#define   SYNTH_PIO_RD_P2           1
#define   SYNTH_PIO_RD_P3           2
#define   SYNTH_PIO_RD_P4           1
#define   SYNTH_PIO_WR_P2           1
#define   SYNTH_PIO_WR_P3           2
#define   SYNTH_PIO_WR_P4           1
#define   SYNTH_PIO_DS_16           1
#define   SYNTH_PIO_EVEN_HIGH       1

/*
 * PBUS DMA parameters
 */
 
#define   PBUS_DMA_RD_D3            3
#define   PBUS_DMA_RD_D4            0
#define   PBUS_DMA_RD_D5            0
#define   PBUS_DMA_WR_D3            1
#define   PBUS_DMA_WR_D4            0
#define   PBUS_DMA_WR_D5            0
#define   PBUS_DMA_DS_16            1
#define   PBUS_DMA_EVEN_HIGH        0
#define   PBUS_DMA_REAL_TIME        1
#define   PBUS_DMA_BURST_COUNT      0  /* don't know what this should be */
#define   PBUS_DMA_DRQ_LIVE         1  /* don't know what this should be */

#else     /* HAL2 NOT UNDER EMULATION */

#define   HAL2_PIO_RD_P2            1
#define   HAL2_PIO_RD_P3            2
#define   HAL2_PIO_RD_P4            2
#define   HAL2_PIO_WR_P2            1
#define   HAL2_PIO_WR_P3            2
#define   HAL2_PIO_WR_P4            2
#define   HAL2_PIO_DS_16            1
#define   HAL2_PIO_EVEN_HIGH        0

#define   AES_PIO_RD_P2             1
#define   AES_PIO_RD_P3             3
#define   AES_PIO_RD_P4             2
#define   AES_PIO_WR_P2             1
#define   AES_PIO_WR_P3             3
#define   AES_PIO_WR_P4             2
#define   AES_PIO_DS_16             0
#define   AES_PIO_EVEN_HIGH         1

#define   VOLUME_PIO_RD_P2          1
#define   VOLUME_PIO_RD_P3          4
#define   VOLUME_PIO_RD_P4          4
#define   VOLUME_PIO_WR_P2          1
#define   VOLUME_PIO_WR_P3          4
#define   VOLUME_PIO_WR_P4          4
#define   VOLUME_PIO_DS_16          0
#define   VOLUME_PIO_EVEN_HIGH      0

#define   SYNTH_PIO_RD_P2           1
#define   SYNTH_PIO_RD_P3           2
#define   SYNTH_PIO_RD_P4           1
#define   SYNTH_PIO_WR_P2           1
#define   SYNTH_PIO_WR_P3           2
#define   SYNTH_PIO_WR_P4           1
#define   SYNTH_PIO_DS_16           1
#define   SYNTH_PIO_EVEN_HIGH       1

/*
 * PBUS DMA parameters
 */
 
#define   PBUS_DMA_RD_D3            0
#define   PBUS_DMA_RD_D4            2
#define   PBUS_DMA_RD_D5            2
#define   PBUS_DMA_WR_D3            0
#define   PBUS_DMA_WR_D4            2
#define   PBUS_DMA_WR_D5            2
#define   PBUS_DMA_DS_16            1
#define   PBUS_DMA_EVEN_HIGH        0
#define   PBUS_DMA_REAL_TIME        1
#define   PBUS_DMA_BURST_COUNT      0  /* don't know what this should be */
#define   PBUS_DMA_DRQ_LIVE         1  /* don't know what this should be */
#endif
extern void hal2_configure_pbus_pio(void);
extern void hal2_configure_pbus_dma(void);
extern int hal2_configure_dma(int);
extern int hal2_rev(void);
