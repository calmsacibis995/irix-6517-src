
#define ccdir0	V_CC_SYSCON		
#define ccdir1	V_CC_INDIR1		
#define ccdir2	V_CC_INDIR2		
#define ccdir3	V_CC_FRAME_BUF		
#define ccdir4	V_CC_FLD_FRM_SEL	
#define ccdir5	V_CC_FRM_BUF_DATA	
#define ccdir6	V_CC_IIC_CNTL		
#define ccdir7	V_CC_IIC_DATA		

/*
 *	Indirect Registers 	(8 bit register space)
 */
#define ccind0	 V_CC_G_CHAN_A_SETUP		
#define ccind1	 V_CC_G_CHAN_A_LINE_1		
#define ccind2	 V_CC_G_CHAN_B_SETUP		
#define ccind3	 V_CC_G_CHAN_BC_LINE_1		
#define ccind4	 V_CC_G_CHAN_C_SETUP		

#define ccind8	 V_CC_A_LEFT			
#define ccind9	 V_CC_A_RIGHT			
#define ccind10	 V_CC_A_BLACK			
#define ccind11	 V_CC_A_TOP			
#define ccind12	 V_CC_A_BOTTOM			
#define ccind13	 V_CC_A_PHS_DET_HI		
#define ccind14	 V_CC_A_PHS_DET_LO		

#define ccind16	 V_CC_B_LEFT			
#define ccind17	 V_CC_B_RIGHT			
#define ccind18	 V_CC_B_BLACK			
#define ccind19	 V_CC_B_TOP			
#define ccind20	 V_CC_B_BOTTOM			
#define ccind21	 V_CC_B_PHS_DET_HI		
#define ccind22	 V_CC_B_PHS_DET_LO		

#define ccind24	 V_CC_C_LEFT			
#define ccind25	 V_CC_C_RIGHT			
#define ccind26	 V_CC_C_BLACK			
#define ccind27	 V_CC_C_TOP			
#define ccind28	 V_CC_C_BOTTOM			
#define ccind29	 V_CC_C_PHS_DET_HI		
#define ccind30	 V_CC_C_PHS_DET_LO		

#define ccind32	 V_CC_MISC			

#define ccind33	 V_CC_V_GEN_UP			
#define ccind34	 V_CC_V_GEN_HOR			
#define ccind35	 V_CC_V_GEN_VERT			

#define ccind36	 V_CC_V_NONBLANK			

#define ccind38	 V_CC_INT_CTRL			

#define ccind39	 V_CC_CUR_VIDEO_LINE		

#define ccind40	 V_CC_IN_INT_HI			
#define ccind41	 V_CC_IN_INT_LO			
#define ccind42	 V_CC_OUT_INT_HI			
#define ccind43	 V_CC_OUT_INT_LO			
#define ccind44	 V_CC_IN_LINE_HI			
#define ccind45	 V_CC_IN_LINE_LO			
#define ccind46	 V_CC_OUT_LINE_HI		
#define ccind47	 V_CC_OUT_LINE_LO		

#define ccind48	 V_CC_FRM_BUF_SETUP		
#define ccind49	 V_CC_FRM_BUF_PTR		

#define ccind50	 V_CC_ALPHA_BKGRD		
#define ccind51	 V_CC_ALPHA_Y			
#define ccind52	 V_CC_ALPHA_U			
#define ccind53	 V_CC_ALPHA_V			

#define ccind54	 V_CC_CC1_1			
#define ccind55	 V_CC_CC1_2			
#define ccind56	 V_CC_CC1_3			
#define ccind57	 V_CC_CC1_4			
#define ccind58	 V_CC_CC1_5			


#define ccind59	 V_CC_ALPHA_COMPOS		

#define ccind64	 V_CC_KEY_GEN_1			
#define ccind65	 V_CC_KEY_GEN_2			
#define ccind66	 V_CC_KEY_GEN_3			
#define ccind67	 V_CC_KEY_GEN_4			
#define ccind68	 V_CC_KEY_GEN_5			
#define ccind69	 V_CC_KEY_GEN_6			
#define ccind70	 V_CC_KEY_GEN_7			
#define ccind71	 V_CC_KEY_GEN_8			
#define ccind72	 V_CC_KEY_GEN_9			
#define ccind73	 V_CC_KEY_GEN_10			
#define ccind74	 V_CC_KEY_GEN_11			
#define ccind75	 V_CC_KEY_GEN_12			
#define ccind76	 V_CC_KEY_GEN_13			
#define ccind77	 V_CC_KEY_GEN_14			
#define ccind78	 V_CC_KEY_GEN_15			

#define ccind79	 V_CC_ALPHA_SHADOW_1		
#define ccind80	 V_CC_ALPHA_SHADOW_2		

#define ccind128	 V_CC_VERS_ID			

#define ccind37	 V_CC_GENERAL_OUT		


/* Phillips' DACs on the I2C */
#define dac0	 V_DAC_0		
#define dac1	 V_DAC_1		
#define dac2	 V_DAC_2		
#define dac3	 V_DAC_3		
#define dac4	 V_DAC_4		
#define dac5	 V_DAC_5		
#define dac6	 V_DAC_6		
#define dac7	 V_DAC_7		

/*
		AB CHIP (Offsets from base in 32 bit register space)
*/
#define abdir0	 V_AB_EXT_MEM		
#define abdir1	 V_AB_INT_REG		
#define abdir2	 V_AB_TEST_REGS		
#define abdir3	 V_AB_LOW_ADDR		
#define abdir4	 V_AB_HIGH_ADDR		
#define abdir5	 V_AB_SYSCON		
#define abdir6	 V_AB_CS_ID		

/*
	AB INDIRECTS 	(Offsets in 8 bit register space)
*/
#define abind0	 V_AB_A_X_OFFSET_LO		
#define abind1	 V_AB_A_X_OFFSET_HI		
#define abind2	 V_AB_A_Y_OFFSET_LO		
#define abind3	 V_AB_A_Y_OFFSET_HI		
#define abind4	 V_AB_A_XMOD			
#define abind5	 V_AB_A_WIN_STATUS		
#define abind6	 V_AB_A_VIDEO_INFO		
#define abind7	 V_AB_A_DECIMATION		

#define abind10	 V_AB_B_X_OFFSET_LO		
#define abind11	 V_AB_B_X_OFFSET_HI		
#define abind12	 V_AB_B_Y_OFFSET_LO		
#define abind13	 V_AB_B_Y_OFFSET_HI		
#define abind14	 V_AB_B_XMOD			
#define abind15	 V_AB_B_WIN_STATUS		
#define abind16	 V_AB_B_VIDEO_INFO		
#define abind17	 V_AB_B_DECIMATION		

#define abind20	 V_AB_C_X_OFFSET_LO		
#define abind21	 V_AB_C_X_OFFSET_HI		
#define abind22	 V_AB_C_Y_OFFSET_LO		
#define abind23	 V_AB_C_Y_OFFSET_HI		
#define abind24	 V_AB_C_XMOD			
#define abind25	 V_AB_C_WIN_STATUS		
#define abind26	 V_AB_C_VIDEO_INFO		


/*
 * DMSD subaddresses.
 */
#define decode0	 V_DMSD_REG_IDEL			
#define decode1	 V_DMSD_REG_HSYB50		
#define decode2	 V_DMSD_REG_HSYE50		
#define decode3	 V_DMSD_REG_HCLB50		
#define decode4	 V_DMSD_REG_HCLE50		
#define decode5	 V_DMSD_REG_HSP50		
#define decode6	 V_DMSD_REG_LUMA			
#define decode7	 V_DMSD_REG_HUE			
#define decode8	 V_DMSD_REG_CKQAM		
#define decode9	 V_DMSD_REG_CKSECAM		
#define decode10	 V_DMSD_REG_SENPAL		
#define decode11	 V_DMSD_REG_SENSECAM		
#define decode12	 V_DMSD_REG_GC0			
#define decode13	 V_DMSD_REG_STDMODE		
#define decode14	 V_DMSD_REG_IOCLK		
#define decode15	 V_DMSD_REG_CTRL3		
#define decode16	 V_DMSD_REG_CTRL4		
#define decode17	 V_DMSD_REG_CHCV			
#define decode18	 V_DMSD_REG_HSYB60		
#define decode19	 V_DMSD_REG_HSYE60		
#define decode20	 V_DMSD_REG_HCLB60		
#define decode21	 V_DMSD_REG_HCLE60		
#define decode22	 V_DMSD_REG_HSP60		

/* Color space reg only one subaddr */
#define csc0	 V_CSC_REG_0				


/*
 * DENC internal register indices.
 */
#define enc0	 V_DENC_REG_IN00			
#define enc1	 V_DENC_REG_IN01			
#define enc2	 V_DENC_REG_IN02			
#define enc3	 V_DENC_REG_IN03			
#define enc4	 V_DENC_REG_SYNC00		
#define enc5	 V_DENC_REG_SYNC01		
#define enc6	 V_DENC_REG_SYNC02		
#define enc7	 V_DENC_REG_SYNC03		
#define enc8	 V_DENC_REG_OUT00		
#define enc9	 V_DENC_REG_OUT01		
#define enc10	 V_DENC_REG_OUT02		
#define enc11	 V_DENC_REG_OUT03		
#define enc12	 V_DENC_REG_ENCODE00		
#define enc13	 V_DENC_REG_ENCODE01		
#define enc14	 V_DENC_REG_ENCODE02		

