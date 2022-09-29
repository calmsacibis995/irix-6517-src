/* 
	 VB2	- 004, 31 dec 1992
*/
   

/* VB2 Frame Buffer RAM */
#define	VB2_VRAM0_A	12	/* Bank 0 bit 0 - 7 */
#define	VB2_VRAM1_A	29	/* Bank 1 bit 0 - 7 */
#define	VB2_VRAM2_A 	13	/* Bank 2 bit 0 - 7 */
#define	VB2_VRAM3_A	22	/* Bank 3 bit 0 - 7 */
#define	VB2_VRAM4_A	18	/* Bank 4 bit 0 - 7 */

#define	VB2_VRAM0_B	506	/* Bank 0 bit 8 - 15 */
#define	VB2_VRAM1_B	524	/* Bank 1 bit 8 - 15 */
#define	VB2_VRAM2_B 	511	/* Bank 2 bit 8 - 15 */
#define	VB2_VRAM3_B	518	/* Bank 3 bit 8 - 15 */
#define	VB2_VRAM4_B	516	/* Bank 4 bit 8 - 15 */

#define	VB2_VRAM0_C	10	/* Bank 0 bit 16 - 23 */
#define	VB2_VRAM1_C	28	/* Bank 1 bit 16 - 23 */
#define	VB2_VRAM2_C 	11	/* Bank 2 bit 16 - 23 */
#define	VB2_VRAM3_C	21	/* Bank 3 bit 16 - 23 */
#define	VB2_VRAM4_C	17	/* Bank 4 bit 16 - 23 */

#ifdef GR2_BITP
static unsigned short VB2_VRAM_U[] = {
VB2_VRAM0_A,VB2_VRAM1_A,VB2_VRAM2_A,VB2_VRAM3_A,VB2_VRAM4_A,
VB2_VRAM0_B,VB2_VRAM1_B,VB2_VRAM2_B,VB2_VRAM3_B,VB2_VRAM4_B,
VB2_VRAM0_C,VB2_VRAM1_C,VB2_VRAM2_C,VB2_VRAM3_C,VB2_VRAM4_C,
};
#endif

/* auxiliary planes */

#define VB2_CID_VRAM0 	505	/* Bank 0 bit 0 - 3 */
#define VB2_CID_VRAM1 	523	/* Bank 1 bit 0 - 3 */
#define VB2_CID_VRAM2 	510	/* Bank 2 bit 0 - 3 */
#define VB2_CID_VRAM3 	517	/* Bank 3 bit 0 - 3 */
#define VB2_CID_VRAM4 	515	/* Bank 4 bit 0 - 3 */
#ifdef GR2_BITP
static unsigned int VB2_CID_VRAM_U[] = {
VB2_CID_VRAM0,VB2_CID_VRAM1,VB2_CID_VRAM2,VB2_CID_VRAM3,VB2_CID_VRAM4 };
#endif

/* auxiliary planes */

#define VB2_AUX_VRAM0 	505	/* Bank 0 bit 0 - 3 */
#define VB2_AUX_VRAM1 	523	/* Bank 1 bit 0 - 3 */
#define VB2_AUX_VRAM2 	510	/* Bank 2 bit 0 - 3 */
#define VB2_AUX_VRAM3 	517	/* Bank 3 bit 0 - 3 */
#define VB2_AUX_VRAM4 	515	/* Bank 4 bit 0 - 3 */
#ifdef GR2_BITP
static unsigned int VB2_AUX_VRAM_U[] = {
VB2_AUX_VRAM0,VB2_AUX_VRAM1,VB2_AUX_VRAM2,VB2_AUX_VRAM3,VB2_AUX_VRAM4 };
#endif

/* XMAP7 location */

#define VB2_XMAP0_LOC		8
#define VB2_XMAP1_LOC		31
#define VB2_XMAP2_LOC		7
#define VB2_XMAP3_LOC		30
#define	VB2_XMAP4_LOC		6
#ifdef GR2_ERR_REPORT
static unsigned int VB2_XMAP_U[]= {
VB2_XMAP0_LOC,VB2_XMAP1_LOC,VB2_XMAP2_LOC,VB2_XMAP3_LOC,VB2_XMAP4_LOC };
#endif

#define VB2_VC1_LOC		23

#ifdef IP22
/* Support for Express on Indy */

/* VB2 Frame Buffer RAM */
#define	VB2_VRAM0_A_INDY	523	/* Bank 0 bit 0 - 7 */
#define	VB2_VRAM1_A_INDY	526	/* Bank 1 bit 0 - 7 */
#define	VB2_VRAM2_A_INDY 	527	/* Bank 2 bit 0 - 7 */
#define	VB2_VRAM3_A_INDY	528	/* Bank 3 bit 0 - 7 */
#define	VB2_VRAM4_A_INDY	522	/* Bank 4 bit 0 - 7 */

#define VB2_VRAM0_B_INDY     523     /* Bank 0 bit 8 - 15 */
#define VB2_VRAM1_B_INDY     526     /* Bank 1 bit 8 - 15 */
#define VB2_VRAM2_B_INDY     527     /* Bank 2 bit 8 - 15 */
#define VB2_VRAM3_B_INDY     528     /* Bank 3 bit 8 - 15 */
#define VB2_VRAM4_B_INDY     522     /* Bank 4 bit 8 - 15 */

#define	VB2_VRAM0_C_INDY	29	/* Bank 0 bit 16 - 23 */
#define	VB2_VRAM1_C_INDY	31	/* Bank 1 bit 16 - 23 */
#define	VB2_VRAM2_C_INDY 	32	/* Bank 2 bit 16 - 23 */
#define	VB2_VRAM3_C_INDY	33	/* Bank 3 bit 16 - 23 */
#define	VB2_VRAM4_C_INDY	28	/* Bank 4 bit 16 - 23 */

static unsigned short VB2_VRAM_U_INDY[] = {
VB2_VRAM0_A_INDY,VB2_VRAM1_A_INDY,VB2_VRAM2_A_INDY,
VB2_VRAM3_A_INDY,VB2_VRAM4_A_INDY,
VB2_VRAM0_B_INDY,VB2_VRAM1_B_INDY,VB2_VRAM2_B_INDY,
VB2_VRAM3_B_INDY,VB2_VRAM4_B_INDY,
VB2_VRAM0_C_INDY,VB2_VRAM1_C_INDY,VB2_VRAM2_C_INDY,
VB2_VRAM3_C_INDY,VB2_VRAM4_C_INDY,
};

/* auxiliary planes */

#define VB2_CID_VRAM0_INDY 	29	/* Bank 0 bit 0 - 3 */
#define VB2_CID_VRAM1_INDY 	31	/* Bank 1 bit 0 - 3 */
#define VB2_CID_VRAM2_INDY 	32	/* Bank 2 bit 0 - 3 */
#define VB2_CID_VRAM3_INDY 	33	/* Bank 3 bit 0 - 3 */
#define VB2_CID_VRAM4_INDY 	28	/* Bank 4 bit 0 - 3 */
static unsigned int VB2_CID_VRAM_U_INDY[] = {
VB2_CID_VRAM0_INDY,VB2_CID_VRAM1_INDY,VB2_CID_VRAM2_INDY,
VB2_CID_VRAM3_INDY,VB2_CID_VRAM4_INDY };

/* auxiliary planes */

#define VB2_AUX_VRAM0_INDY 	29	/* Bank 0 bit 0 - 3 */
#define VB2_AUX_VRAM1_INDY 	31	/* Bank 1 bit 0 - 3 */
#define VB2_AUX_VRAM2_INDY 	32	/* Bank 2 bit 0 - 3 */
#define VB2_AUX_VRAM3_INDY 	33	/* Bank 3 bit 0 - 3 */
#define VB2_AUX_VRAM4_INDY 	28	/* Bank 4 bit 0 - 3 */
static unsigned int VB2_AUX_VRAM_U_INDY[] = {
VB2_AUX_VRAM0_INDY,VB2_AUX_VRAM1_INDY,VB2_AUX_VRAM2_INDY,
VB2_AUX_VRAM3_INDY,VB2_AUX_VRAM4_INDY };

/* XMAP7 location */

#define VB2_XMAP0_LOC_INDY		24
#define VB2_XMAP1_LOC_INDY		513
#define VB2_XMAP2_LOC_INDY		20
#define VB2_XMAP3_LOC_INDY		22
#define	VB2_XMAP4_LOC_INDY		515
static unsigned int VB2_XMAP_U_INDY[]= {
VB2_XMAP0_LOC_INDY,VB2_XMAP1_LOC_INDY,VB2_XMAP2_LOC_INDY,
VB2_XMAP3_LOC_INDY,VB2_XMAP4_LOC_INDY };

#define VB2_VC1_LOC_INDY		502
#endif	/* IP22 */
