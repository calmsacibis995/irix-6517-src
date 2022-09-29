#ident	"$Revision: 1.5 $"
/* 
	GR2, ZB4 and VM2 chips location 
	 gr2 	- rev A, 12 dec 1991
	 ZB4 	- rev A, 29 dec 1991
	 VM2	- rev A, 29 dec 1991
	 VB1.1	- rev A, 06 jan 1992
*/
   

#define HQ2_LOC		68
#define	GE7_0_LOC	43
#define	GE7_1_LOC	35
#define	GE7_2_LOC	34
#define	GE7_3_LOC	42
#define	GE7_4_LOC	0 /* N/A */
#define	GE7_5_LOC	0 /* N/A */
#define	GE7_6_LOC	0 /* N/A */
#define	GE7_7_LOC	0 /* N/A */
#define RE3_LOC		64
#define VC1_LOC 	8
#if 0
static unsigned short GE7_U[] = {
GE7_0_LOC,GE7_1_LOC,GE7_2_LOC,GE7_3_LOC,
GE7_4_LOC,GE7_5_LOC,GE7_6_LOC,GE7_7_LOC };
#endif

/* GE7 Ucode RAM location */
#define GE7_URAM0_LOC 	51  	/* bit  0-15   */	
#define GE7_URAM1_LOC 	56	/* bit 16-31   */	
#define GE7_URAM2_LOC 	61	/* bit 31-47   */	
#define GE7_URAM3_LOC 	66	/* bit 48-63   */	
#define GE7_URAM4_LOC 	67	/* bit 64-79   */	
#define GE7_URAM5_LOC 	76	/* bit 80-95   */	
#define GE7_URAM6_LOC 	79	/* bit 96-111  */	
#define GE7_URAM7_LOC 	84	/* bit 112-127 */	
#if 0
static unsigned short GE7_URAM_U[] = {
GE7_URAM0_LOC,GE7_URAM1_LOC,GE7_URAM2_LOC,GE7_URAM3_LOC,
GE7_URAM4_LOC,GE7_URAM5_LOC,GE7_URAM6_LOC,GE7_URAM7_LOC,
};
#endif

/* GE7 Share RAM location */

#define GE7_SRAM0_LOC	57	/* bit  0-7    */
#define GE7_SRAM1_LOC	58	/* bit  8-15   */
#define GE7_SRAM2_LOC	59	/* bit  16-23  */
#define GE7_SRAM3_LOC	60	/* bit  24-31  */
#if 0
static unsigned short GE7_SRAM_U[] = {
GE7_SRAM0_LOC,GE7_SRAM1_LOC,GE7_SRAM2_LOC,GE7_SRAM3_LOC };
#endif

/* HQ Ucode RAM */

#define HQ2_URAM0_LOC	86	/* bit 0-7      */
#define HQ2_URAM1_LOC	87	/* bit 8-15     */
#define HQ2_URAM2_LOC	88	/* bit 15-23    */

/* auxiliary planes */

#define AUX_VRAM0 	6	/* Bank 0 bit 0 - 3 */
#define AUX_VRAM1 	14	/* Bank 1 bit 0 - 3 */
#define AUX_VRAM2 	16	/* Bank 2 bit 0 - 3 */
#define AUX_VRAM3 	24	/* Bank 3 bit 0 - 3 */
#define AUX_VRAM4 	26	/* Bank 4 bit 0 - 3 */
#ifdef GR2_BITP
static unsigned short AUX_VRAM_U[] = {
AUX_VRAM0,AUX_VRAM1,AUX_VRAM2,AUX_VRAM3,AUX_VRAM4 };
#endif

/* cid planes */

#define CID_DRAM0 	5	/* Bank 0 bit 0 - 3 */
#define CID_DRAM1 	13	/* Bank 1 bit 0 - 3 */
#define CID_DRAM2 	15	/* Bank 2 bit 0 - 3 */
#define CID_DRAM3 	23	/* Bank 3 bit 0 - 3 */
#define CID_DRAM4 	25	/* Bank 4 bit 0 - 3 */
#ifdef GR2_BITP
static unsigned short CID_DRAM_U[] = {
CID_DRAM0,CID_DRAM1,CID_DRAM2,CID_DRAM3,CID_DRAM4 };
#endif

/* VM2 video RAM */
#define	VM2_VRAM0_LOW	12	/* Bank 0 bit 0 - 3 */
#define	VM2_VRAM0_HIGH	11	/* Bank 0 bit 4 - 7 */
#define	VM2_VRAM1_LOW	3	/* Bank 1 bit 0 - 3 */
#define	VM2_VRAM1_HIGH	2	/* Bank 1 bit 4 - 7 */
#define	VM2_VRAM2_LOW 	1	/* Bank 2 bit 0 - 3 */
#define	VM2_VRAM2_HIGH	10	/* Bank 2 bit 4 - 7 */
#define	VM2_VRAM3_LOW	5	/* Bank 3 bit 0 - 3 */
#define	VM2_VRAM3_HIGH	4	/* Bank 3 bit 4 - 7 */
#define	VM2_VRAM4_LOW	7	/* Bank 4 bit 0 - 3 */
#define	VM2_VRAM4_HIGH	6	/* Bank 4 bit 4 - 7 */
#ifdef GR2_BITP
static unsigned short VM2_VRAM_U[] = {
VM2_VRAM0_LOW,VM2_VRAM0_HIGH,VM2_VRAM1_LOW,VM2_VRAM1_HIGH,
VM2_VRAM2_LOW,VM2_VRAM2_HIGH,VM2_VRAM3_LOW,VM2_VRAM3_HIGH,
VM2_VRAM4_LOW,VM2_VRAM4_HIGH };
#endif

/* ZB4 DRAM */

#define	ZB4_DRAMA_01	45	/* bank A bit 0 - 15 low */
#define ZB4_DRAMA_23	39	/* bank A bit 16 - 23 low */
#define	ZB4_DRAMA_45	44	/* bank A bit 0 - 15 high */
#define ZB4_DRAMA_67	38	/* bank A bit 16 - 23 high */

#define	ZB4_DRAMB_01	36	/* bank B  */
#define ZB4_DRAMB_23	18	/* bank B  */
#define	ZB4_DRAMB_45	27	/* bank B  */
#define ZB4_DRAMB_67	9	/* bank B  */

#define	ZB4_DRAMC_01	28	/* bank C  */
#define ZB4_DRAMC_23	30	/* bank C  */
#define	ZB4_DRAMC_45	29	/* bank C  */
#define ZB4_DRAMC_67	31	/* bank C  */

#define	ZB4_DRAMD_01	19	/* bank D  */
#define ZB4_DRAMD_23	21	/* bank D  */
#define	ZB4_DRAMD_45	20	/* bank D  */
#define ZB4_DRAMD_67	22	/* bank D  */

#define	ZB4_DRAME_01	32	/* bank E  */
#define ZB4_DRAME_23	34	/* bank E  */
#define	ZB4_DRAME_45	33	/* bank E  */
#define ZB4_DRAME_67	35	/* bank E  */

#define	ZB4_DRAMF_01	23	/* bank F  */
#define ZB4_DRAMF_23	25	/* bank F  */
#define	ZB4_DRAMF_45	24	/* bank F  */
#define ZB4_DRAMF_67	26	/* bank F  */

#define	ZB4_DRAMG_01	10	/* bank G  */
#define ZB4_DRAMG_23	12	/* bank G  */
#define	ZB4_DRAMG_45	11	/* bank G  */
#define ZB4_DRAMG_67	13	/* bank G  */

#define	ZB4_DRAMH_01	1	/* bank H  */
#define ZB4_DRAMH_23	3	/* bank H  */
#define	ZB4_DRAMH_45	2	/* bank H  */
#define ZB4_DRAMH_67	4	/* bank H  */

#define	ZB4_DRAMI_01	14	/* bank I  */
#define ZB4_DRAMI_23	16	/* bank I  */
#define	ZB4_DRAMI_45	15	/* bank I  */
#define ZB4_DRAMI_67	17	/* bank I  */

#define	ZB4_DRAMJ_01	5	/* bank J  */
#define ZB4_DRAMJ_23	7	/* bank J  */
#define	ZB4_DRAMJ_45	6	/* bank J  */
#define ZB4_DRAMJ_67	8	/* bank J  */

#ifdef GR2_BITP
static unsigned short ZB4_DRAM_U[] = {
ZB4_DRAMA_01,ZB4_DRAMA_23,ZB4_DRAMA_45,ZB4_DRAMA_67,
ZB4_DRAMB_01,ZB4_DRAMB_23,ZB4_DRAMB_45,ZB4_DRAMB_67,
ZB4_DRAMC_01,ZB4_DRAMC_23,ZB4_DRAMC_45,ZB4_DRAMC_67,
ZB4_DRAMD_01,ZB4_DRAMD_23,ZB4_DRAMD_45,ZB4_DRAMD_67,
ZB4_DRAME_01,ZB4_DRAME_23,ZB4_DRAME_45,ZB4_DRAME_67,
ZB4_DRAMF_01,ZB4_DRAMF_23,ZB4_DRAMF_45,ZB4_DRAMF_67,
ZB4_DRAMG_01,ZB4_DRAMG_23,ZB4_DRAMG_45,ZB4_DRAMG_67,
ZB4_DRAMH_01,ZB4_DRAMH_23,ZB4_DRAMH_45,ZB4_DRAMH_67,
ZB4_DRAMI_01,ZB4_DRAMI_23,ZB4_DRAMI_45,ZB4_DRAMI_67,
ZB4_DRAMJ_01,ZB4_DRAMJ_23,ZB4_DRAMJ_45,ZB4_DRAMJ_67,
};
#endif

/* 
	VB1 board chips location 
*/
   
/* XMAP5 location */

#define GR2_XMAP0_LOC		16
#define GR2_XMAP1_LOC		15
#define GR2_XMAP2_LOC		14
#define GR2_XMAP3_LOC		1
#define	GR2_XMAP4_LOC		17
#ifdef GR2_ERR_REPORT
static unsigned int GR2_XMAP_U[]= {
GR2_XMAP0_LOC,GR2_XMAP1_LOC,GR2_XMAP2_LOC,GR2_XMAP3_LOC,GR2_XMAP4_LOC };
#endif

extern void print_GR2_loc(unsigned int);
extern void print_VM2_loc(unsigned int, unsigned int);
extern void print_VB2_loc(unsigned int);
extern void print_RU1_loc(unsigned int);
extern void print_ZB4_loc(unsigned int);
