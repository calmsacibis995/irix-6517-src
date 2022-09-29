#ident "sys/hal2.h: $Revision: 1.12 $"

/*
 * hal2.h -- audio system specific defines for IP22, IP24 and IP26.
 */

/*
 *  Base physical addresses for two (total) possible HPC3's in system
 */
#define HPC3_0_ADDR	0x1fb80000	/* Start with this one in base system */
#define HPC3_1_ADDR	0x1fb00000	/* Later, maybe */ 

/*
 *  Offsets from HPC3 base address for HPC3 PIO chip selects
 */
#define HPC3_CS_0	0x00058000	/* pbus chip select 0 offset */
#define HPC3_CS_1	0x00058400	/* pbus chip select 1 offset */
#define HPC3_CS_2	0x00058800	/* pbus chip select 2 offset */
#define HPC3_CS_3	0x00058c00	/* pbus chip select 3 offset */
#define HPC3_CS_4	0x00059000	/* pbus chip select 4 offset */
#define HPC3_CS_5	0x00059400	/* pbus chip select 5 offset */
#define HPC3_CS_6	0x00059800	/* pbus chip select 6 offset */
#define HPC3_CS_7	0x00059c00	/* pbus chip select 7 offset */
#define HPC3_CS_8	0x0005a000	/* pbus chip select 8 offset */
#define HPC3_CS_9	0x0005a400	/* pbus chip select 9 offset */

/*
 *  HAL2: GIO64/PBUS addressable registers
 * 
 * HAL2_ADDR = (HPC3_0_ADDR+HPC3_CS_0)
 */
#define	HAL2_ADDR	0x1fbd8000	/* start here           */
/* XXX what's at (0x0000 + HAL2_ADDR)? */
#define	HAL2_ISR	0x1fbd8010	/* HAL2 Status          */
#define	HAL2_REV	0x1fbd8020	/* HAL2 Revision        */
#define	HAL2_IAR	0x1fbd8030	/* HAL2 Indirect Address */
#define	HAL2_IDR0	0x1fbd8040	/* HAL2 Indirect Data, LSB's */
#define	HAL2_IDR1	0x1fbd8050	/* HAL2 Indirect Data        */
#define	HAL2_IDR2	0x1fbd8060	/* HAL2 Indirect Data        */
#define	HAL2_IDR3	0x1fbd8070	/* HAL2 Indirect Data, MSB's */

/*
 *  AES RX and TX: GIO64/PBUS addressable registers
 * 
 * note that these differ from revs. < 0.7 of the HAL2 PMOD.
 * These were changed to match the actual design of the chip (as 
 * opposed to the spec)
 * 
 * AESRX_ADDR = (HPC3_0_ADDR+HPC3_CS_1)
 */
#define	AESTX_ADDR	0x1fbd8480		/* start here      */
#define	AESRX_ADDR	0x1fbd8400		/* start here      */

#define AESTX_SR	0x1fbd8480	/* Status (=AESTX_ADDR) */
#define	AESTX_CR1	0x1fbd8484	/* Control Register 1 */
#define	AESTX_CR2	0x1fbd8488	/* Control Register 2 */
#define	AESTX_CR3	0x1fbd848c	/* Control Register 3 */
#define AESTX_UD0	0x1fbd8490	/* user data window, byte 0 */
#define AESTX_UD1	0x1fbd8494	/* user data window, byte 1 */
#define AESTX_UD2	0x1fbd8498  	/* user data window, byte 2 */
#define AESTX_UD3	0x1fbd849c	/* user data window, byte 3 */
#define AESTX_CS0	0x1fbd84a0  	/* channel status data byte 0 */
#define AESTX_CS1	0x1fbd84a4  	/* channel status data byte 1 */
#define AESTX_CS2	0x1fbd84a8  	/* channel status data byte 2 */
#define AESTX_CS3	0x1fbd84ac  	/* channel status data byte 3 */
#define AESTX_CS4	0x1fbd84b0	/* channel status data byte 4 */
#define AESTX_CS5	0x1fbd84b4  	/* channel status data byte 5 */
#define AESTX_CS6	0x1fbd84b8  	/* channel status data byte 6 */
#define AESTX_CS7	0x1fbd84bc  	/* channel status data byte 7 */
#define AESTX_CS8	0x1fbd84c0  	/* channel status data byte 8 */
#define AESTX_CS9	0x1fbd84c4  	/* channel status data byte 9 */
#define AESTX_CS10	0x1fbd84c8  	/* channel status data byte 10*/
#define AESTX_CS11	0x1fbd84cc  	/* channel status data byte 11*/
#define AESTX_CS12	0x1fbd84d0  	/* channel status data byte 12*/
#define AESTX_CS13	0x1fbd84d4  	/* channel status data byte 13*/
#define AESTX_CS14	0x1fbd84d8  	/* channel status data byte 14*/
#define AESTX_CS15	0x1fbd84dc  	/* channel status data byte 15*/
#define AESTX_CS16	0x1fbd84e0  	/* channel status data byte 16*/
#define AESTX_CS17	0x1fbd84e4  	/* channel status data byte 17*/
#define AESTX_CS18	0x1fbd84e8  	/* channel status data byte 18*/
#define AESTX_CS19	0x1fbd84ec  	/* channel status data byte 19*/
#define AESTX_CS20	0x1fbd84f0  	/* channel status data byte 20*/
#define AESTX_CS21	0x1fbd84f4  	/* channel status data byte 21*/
#define AESTX_CS22	0x1fbd84f8  	/* channel status data byte 22*/
#define AESTX_CS23	0x1fbd84fc  	/* channel status data byte 23*/

#define AESRX_SR1	0x1fbd8400	/* Status 1 (=AESRX_ADDR) */
#define	AESRX_SR2	0x1fbd8404  	/* Status 2 */
#define	AESRX_CR1	0x1fbd8408  	/* Control Register 1 */
#define	AESRX_CR2	0x1fbd840c  	/* Control Register 2 */
#define AESRX_UD0	0x1fbd8410  	/* user data window, byte 0 */
#define AESRX_UD1	0x1fbd8414  	/* user data window, byte 1 */
#define AESRX_UD2	0x1fbd8418  	/* user data window, byte 2 */
#define AESRX_UD3	0x1fbd841c	/* user data window, byte 3 */
#define AESRX_CS0	0x1fbd8420  	/* channel status data byte 0 */
#define AESRX_CS1	0x1fbd8424  	/* channel status data byte 1 */
#define AESRX_CS2	0x1fbd8428  	/* channel status data byte 2 */
#define AESRX_CS3	0x1fbd842c  	/* channel status data byte 3 */
#define AESRX_CS4	0x1fbd8430  	/* channel status data byte 4 */
#define AESRX_CS5	0x1fbd8434  	/* channel status data byte 5 */
#define AESRX_CS6	0x1fbd8438  	/* channel status data byte 6 */
#define AESRX_CS7	0x1fbd843c  	/* channel status data byte 7 */
#define AESRX_CS8	0x1fbd8440  	/* channel status data byte 8 */
#define AESRX_CS9	0x1fbd8444  	/* channel status data byte 9 */
#define AESRX_CS10	0x1fbd8448  	/* channel status data byte 10*/
#define AESRX_CS11	0x1fbd844c  	/* channel status data byte 11*/
#define AESRX_CS12	0x1fbd8450  	/* channel status data byte 12*/
#define AESRX_CS13	0x1fbd8454  	/* channel status data byte 13*/
#define AESRX_CS14	0x1fbd8458  	/* channel status data byte 14*/
#define AESRX_CS15	0x1fbd845c  	/* channel status data byte 15*/
#define AESRX_CS16	0x1fbd8460  	/* channel status data byte 16*/
#define AESRX_CS17	0x1fbd8464  	/* channel status data byte 17*/
#define AESRX_CS18	0x1fbd8468  	/* channel status data byte 18*/
#define AESRX_CS19	0x1fbd846c      /* channel status data byte 19*/
#define AESRX_CS20	0x1fbd8470  	/* channel status data byte 20*/
#define AESRX_CS21	0x1fbd8474  	/* channel status data byte 21*/
#define AESRX_CS22	0x1fbd8478  	/* channel status data byte 22*/
#define AESRX_CS23	0x1fbd847c  	/* channel status data byte 23*/

/*
 *  SYNTH: GIO64/PBUS addressable registers
 * 
 * SYNTH_ADDR = (HPC3_0_ADDR+HPC3_CS_3)
 */
#define	SYNTH_ADDR	0x1fbd8c00	/* start here      */
#define SYNTH_PAGE	0x1fbd8c08  	/* DOC page register */
#define SYNTH_REGSEL	0x1fbd8c0c  	/* DOC/global register select*/
#define SYNTH_DLOW	0x1fbd8c10  	/* DOC/global data low byte */
#define SYNTH_DHI	0x1fbd8c14  	/* DOC/global data high byte */
#define SYNTH_IRQ	0x1fbd8c18  	/* IRQ status */
#define SYNTH_DRAM	0x1fbd8c1c  	/* DRAM access */

/*
 *  VOLUME: GIO64/PBUS addressable registers
 * 		headphone/speaker volume control
 * 
 * VOLUME_ADDR = (HPC3_0_ADDR+HPC3_CS_2)
 */
#define	VOLUME_ADDR	0x1fbd8800	/* start here      */
#define VOLUME_RIGHT	0x1fbd8800	/* right output volume */
#define VOLUME_LEFT	0x1fbd8804	/* left output volume */

/*
 *  HAL2: Internal Addressing
 * 
 *  Each indirect register has a separate read and write address.
 *  Though these always differ by a constant (0x80),  we explicitly
 *  give the two addresses here; this should minimize the need for
 *  obfuscated constructs elsewhere.
 */
 
#define HAL2_RELAY_CONTROL_W	0x09100	/* state of RELAY pin signal */
#define HAL2_DMA_ENABLE_W	0x09104	/* global DMA channel enable */
#define HAL2_DMA_ENDIAN_W	0x09108	/* global DMA endian select  */
#define HAL2_DMA_DRIVE_W	0x0910c	/* global DMA bus drive enable */

#define HAL2_RELAY_CONTROL_R	0x09180	/* state of RELAY pin signal */
#define HAL2_DMA_ENABLE_R	0x09184	/* global DMA channel enable */
#define HAL2_DMA_ENDIAN_R	0x09188	/* global DMA endian select  */
#define HAL2_DMA_DRIVE_R	0x0918c	/* global DMA bus drive enable */

#define	HAL2_SYNTH_CTRL_W	0x01104	/* synth dma control write */
#define	HAL2_AESRX_CTRL_W	0x01204	/* aes rx dma control write  */
#define	HAL2_AESTX_CTRL_W	0x01304	/* aes tx dma control write */
#define HAL2_SYNTH_MAP_CTRL_W	0x01604	/* synth dma handshake ctrl write */

#define	HAL2_SYNTH_CTRL_R	0x01184	/* synth dma control read */
#define	HAL2_AESRX_CTRL_R	0x01284	/* aes rx dma control read  */
#define	HAL2_AESTX_CTRL_R	0x01384	/* aes tx dma control read */
#define HAL2_SYNTH_MAP_CTRL_R	0x01684	/* synth dma handshake ctrl read */

/*
 * note: in Indigo mode,  CODECA is the DAC, CODECB is the ADC.
 */
#define	HAL2_CODECA_CTRL1_W	0x01404	/* codec A control 1 write */
#define	HAL2_CODECA_CTRL2_W	0x01408	/* codec A control 2 write */
#define	HAL2_CODECB_CTRL1_W	0x01504	/* codec B control 1 write */
#define	HAL2_CODECB_CTRL2_W	0x01508	/* codec B control 2 write */

#define	HAL2_CODECA_CTRL1_R	0x01484	/* codec A control 1 read */
#define	HAL2_CODECA_CTRL2_R0	0x01488	/* codec A control 2 read word 0*/
#define	HAL2_CODECA_CTRL2_R1	0x01489	/* codec A control 2 read word 1*/
#define	HAL2_CODECB_CTRL1_R	0x01584	/* codec B control 1 read */
#define	HAL2_CODECB_CTRL2_R0	0x01588	/* codec B control 2 read word 0*/
#define	HAL2_CODECB_CTRL2_R1	0x01589	/* codec B control 2 read word 1*/

#define	HAL2_BRES1_CTRL1_W	0x02104	/* clock gen 1 ctrl 1 write */
#define	HAL2_BRES1_CTRL2_W	0x02108	/* clock gen 1 ctrl 2 write */
#define	HAL2_BRES2_CTRL1_W	0x02204	/* clock gen 2 ctrl 1 write */
#define	HAL2_BRES2_CTRL2_W	0x02208	/* clock gen 2 ctrl 2 write */
#define	HAL2_BRES3_CTRL1_W	0x02304	/* clock gen 3 ctrl 1 write */
#define	HAL2_BRES3_CTRL2_W	0x02308	/* clock gen 3 ctrl 2 write */

#define	HAL2_BRES1_CTRL1_R	0x02184	/* clock gen 1 ctrl 1 read */
#define	HAL2_BRES1_CTRL2_R0	0x02188	/* clock gen 1 ctrl 2 read 0 INC */
#define	HAL2_BRES1_CTRL2_R1	0x02189	/* clock gen 1 ctrl 2 read 1 MOD */
#define	HAL2_BRES2_CTRL1_R	0x02284	/* clock gen 2 ctrl 1 read */
#define	HAL2_BRES2_CTRL2_R0	0x02288	/* clock gen 2 ctrl 2 read 0 INC */
#define	HAL2_BRES2_CTRL2_R1	0x02289	/* clock gen 2 ctrl 2 read 1 MOD */
#define	HAL2_BRES3_CTRL1_R	0x02384	/* clock gen 3 ctrl 1 read */
#define	HAL2_BRES3_CTRL2_R0	0x02388	/* clock gen 3 ctrl 2 read 0 INC */
#define	HAL2_BRES3_CTRL2_R1	0x02389	/* clock gen 3 ctrl 2 read 1 MOD */

#define	HAL2_UTIME_W		0x03104	/* unix timer write */
#define	HAL2_UTIME_R		0x03184	/* unix timer read */

/*
 * HAL2 ISR bits
 */
#define HAL2_ISR_TSTATUS	0x01	/* r  transaction status bit 1=busy */
#define HAL2_ISR_USTATUS	0x02	/* r  utime status bit 1=armed      */
#define HAL2_ISR_CODEC_MODE	0x04	/* rw codec mode 1=quad, 0=indigo   */
#define HAL2_ISR_GLOBAL_RESET_N	0x08	/* rw chip global reset 0=reset     */
#define HAL2_ISR_CODEC_RESET_N	0x10	/* rw codec/synth reset 0=reset     */

/*
 * CODEC non audio bits
 */
#define HAL2_NAB_ERROR		(0xf<<28)   /* codec error has occurred */
#define HAL2_NAB_REVISION	(0xf<<24)   /* codec revision number */
#define HAL2_NAB_VALID		0x8	    /* A/D data valid bit */
#define HAL2_NAB_LEFT_CLIP	0x4	    /* left channel clipping */
#define HAL2_NAB_RIGHT_CLIP	0x2	    /* right channel clipping */
