/* 
	 GR3 	- 003, 31 dec 1992
*/
   
/* GR3 chip designation */ 

#define GR3_HQ2_LOC	16
#define	GR3_GE7_0_LOC	24
#define	GR3_GE7_1_LOC	24
#define	GR3_GE7_2_LOC	24
#define	GR3_GE7_3_LOC	24
#define	GR3_GE7_4_LOC	0 /* N/A */
#define	GR3_GE7_5_LOC	0 /* N/A */
#define	GR3_GE7_6_LOC	0 /* N/A */
#define	GR3_GE7_7_LOC	0 /* N/A */
#define GR3_RE3_LOC	23	
#if 0
static unsigned short GR3_GE7_U[] = {
GR3_GE7_0_LOC,GR3_GE7_1_LOC,GR3_GE7_2_LOC,GR3_GE7_3_LOC,
GR3_GE7_4_LOC,GR3_GE7_5_LOC,GR3_GE7_6_LOC,GR3_GE7_7_LOC };
#endif

/* GE7 Ucode RAM location */

#define GR3_GE7_URAM0_LOC 	12  	/* bit  0-15   */	
#define GR3_GE7_URAM1_LOC 	11	/* bit 16-31   */	
#define GR3_GE7_URAM2_LOC 	10	/* bit 31-47   */	
#define GR3_GE7_URAM3_LOC 	9	/* bit 48-63   */	
#define GR3_GE7_URAM4_LOC 	504	/* bit 64-79   */	
#define GR3_GE7_URAM5_LOC 	505	/* bit 80-95   */	
#define GR3_GE7_URAM6_LOC 	506	/* bit 96-111  */	
#define GR3_GE7_URAM7_LOC 	507	/* bit 112-127 */	
#if 0
static unsigned short GR3_GE7_URAM_U[] = {
GR3_GE7_URAM0_LOC,GR3_GE7_URAM1_LOC,GR3_GE7_URAM2_LOC,GR3_GE7_URAM3_LOC,
GR3_GE7_URAM4_LOC,GR3_GE7_URAM5_LOC,GR3_GE7_URAM6_LOC,GR3_GE7_URAM7_LOC,
};
#endif

/* GE7 Share RAM location */

#define GR3_GE7_SRAM0_LOC	39	/* bit  0-7    */
#define GR3_GE7_SRAM1_LOC	37	/* bit  8-15   */
#define GR3_GE7_SRAM2_LOC	38	/* bit  16-23  */
#define GR3_GE7_SRAM3_LOC	36	/* bit  24-31  */
#if 0
static unsigned short GR3_GE7_SRAM_U[] = {
GR3_GE7_SRAM0_LOC,GR3_GE7_SRAM1_LOC,GR3_GE7_SRAM2_LOC,GR3_GE7_SRAM3_LOC };
#endif

/* HQ Ucode RAM */

#define GR3_HQ2_URAM0_LOC	1	/* bit 0-7      */
#define GR3_HQ2_URAM1_LOC	3	/* bit 8-15     */
#define GR3_HQ2_URAM2_LOC	2	/* bit 15-23    */


/* GR3 Z-Buffer DRAM */

#define	GR3_ZB4_DRAMA_01	33	/* bank A bit 0 - 15 */
#define GR3_ZB4_DRAMA_23	34	/* bank A bit 16 - 23 */

#define	GR3_ZB4_DRAMB_01	19	/* bank B  */
#define GR3_ZB4_DRAMB_23	32	/* bank B  */

#define	GR3_ZB4_DRAMC_01	515	/* bank C  */
#define GR3_ZB4_DRAMC_23	514	/* bank C  */

#define	GR3_ZB4_DRAMD_01	513	/* bank D  */
#define GR3_ZB4_DRAMD_23	512	/* bank D  */

#define	GR3_ZB4_DRAME_01	508	/* bank E  */
#define GR3_ZB4_DRAME_23	509	/* bank E  */

#define	GR3_ZB4_DRAMF_01	21	/* bank F  */
#define GR3_ZB4_DRAMF_23	20	/* bank F  */

#define	GR3_ZB4_DRAMG_01	5	/* bank G  */
#define GR3_ZB4_DRAMG_23	4	/* bank G  */

#define	GR3_ZB4_DRAMH_01	511	/* bank H  */
#define GR3_ZB4_DRAMH_23	510	/* bank H  */

#define	GR3_ZB4_DRAMI_01	501	/* bank I  */
#define GR3_ZB4_DRAMI_23	500	/* bank I  */

#define	GR3_ZB4_DRAMJ_01	503	/* bank J  */
#define GR3_ZB4_DRAMJ_23	502	/* bank J  */

#ifdef GR2_BITP
static unsigned short GR3_ZB4_DRAM_U[] = {
GR3_ZB4_DRAMA_01,GR3_ZB4_DRAMA_23,
GR3_ZB4_DRAMB_01,GR3_ZB4_DRAMB_23,
GR3_ZB4_DRAMC_01,GR3_ZB4_DRAMC_23,
GR3_ZB4_DRAMD_01,GR3_ZB4_DRAMD_23,
GR3_ZB4_DRAME_01,GR3_ZB4_DRAME_23,
GR3_ZB4_DRAMF_01,GR3_ZB4_DRAMF_23,
GR3_ZB4_DRAMG_01,GR3_ZB4_DRAMG_23,
GR3_ZB4_DRAMH_01,GR3_ZB4_DRAMH_23,
GR3_ZB4_DRAMI_01,GR3_ZB4_DRAMI_23,
GR3_ZB4_DRAMJ_01,GR3_ZB4_DRAMJ_23
};
#endif

#ifdef IP22
/* Express support for Indy */

/* GR3 chip designation */ 

#define GR3_HQ2_LOC_INDY	38
#define	GR3_GE7_0_LOC_INDY	15
#define	GR3_GE7_1_LOC_INDY	15
#define	GR3_GE7_2_LOC_INDY	508
#define	GR3_GE7_3_LOC_INDY	508
#define	GR3_GE7_4_LOC_INDY	0 /* N/A */
#define	GR3_GE7_5_LOC_INDY	0 /* N/A */
#define	GR3_GE7_6_LOC_INDY	0 /* N/A */
#define	GR3_GE7_7_LOC_INDY	0 /* N/A */
#define GR3_RE3_LOC_INDY	25	
static unsigned short GR3_GE7_U_INDY[] = {
GR3_GE7_0_LOC_INDY,GR3_GE7_1_LOC_INDY,GR3_GE7_2_LOC_INDY,GR3_GE7_3_LOC_INDY,
GR3_GE7_4_LOC_INDY,GR3_GE7_5_LOC_INDY,GR3_GE7_6_LOC_INDY,GR3_GE7_7_LOC_INDY };

/* GE7 Ucode RAM location */

#define GR3_GE7_URAM0_LOC_INDY 	503  	/* bit  0-15   */	
#define GR3_GE7_URAM1_LOC_INDY 	507	/* bit 16-31   */	
#define GR3_GE7_URAM2_LOC_INDY 	512	/* bit 31-47   */	
#define GR3_GE7_URAM3_LOC_INDY 	513	/* bit 48-63   */	
#define GR3_GE7_URAM4_LOC_INDY 	10	/* bit 64-79   */	
#define GR3_GE7_URAM5_LOC_INDY 	16	/* bit 80-95   */	
#define GR3_GE7_URAM6_LOC_INDY 	22	/* bit 96-111  */	
#define GR3_GE7_URAM7_LOC_INDY 	24	/* bit 112-127 */	
static unsigned short GR3_GE7_URAM_U_INDY[] = {
GR3_GE7_URAM0_LOC_INDY,GR3_GE7_URAM1_LOC_INDY,GR3_GE7_URAM2_LOC_INDY,GR3_GE7_URAM3_LOC_INDY,
GR3_GE7_URAM4_LOC_INDY,GR3_GE7_URAM5_LOC_INDY,GR3_GE7_URAM6_LOC_INDY,GR3_GE7_URAM7_LOC_INDY,
};

/* GE7 Share RAM location */

#define GR3_GE7_SRAM0_LOC_INDY	521	/* bit  0-7    */
#define GR3_GE7_SRAM1_LOC_INDY	524	/* bit  8-15   */
#define GR3_GE7_SRAM2_LOC_INDY	31	/* bit  16-23  */
#define GR3_GE7_SRAM3_LOC_INDY	526	/* bit  24-31  */

static unsigned short GR3_GE7_SRAM_U_INDY[] = {
GR3_GE7_SRAM0_LOC_INDY,GR3_GE7_SRAM1_LOC_INDY,GR3_GE7_SRAM2_LOC_INDY,GR3_GE7_SRAM3_LOC_INDY };

/* HQ Ucode RAM */

#define GR3_HQ2_URAM0_LOC_INDY	520	/* bit 0-7      */
#define GR3_HQ2_URAM1_LOC_INDY	523	/* bit 8-15     */
#define GR3_HQ2_URAM2_LOC_INDY	525	/* bit 15-23    */


/* GR3 Z-Buffer DRAM */

#define	GR3_ZB4_DRAMA_01_INDY	21	/* bank A bit 0 - 15 */
#define GR3_ZB4_DRAMA_23_INDY	511	/* bank A bit 16 - 23 */

#define	GR3_ZB4_DRAMB_01_INDY	14	/* bank B  */
#define GR3_ZB4_DRAMB_23_INDY	506	/* bank B  */

#define	GR3_ZB4_DRAMC_01_INDY	20	/* bank C  */
#define GR3_ZB4_DRAMC_23_INDY	510	/* bank C  */

#define	GR3_ZB4_DRAMD_01_INDY	13	/* bank D  */
#define GR3_ZB4_DRAMD_23_INDY	505	/* bank D  */

#define	GR3_ZB4_DRAME_01_INDY	7	/* bank E  */
#define GR3_ZB4_DRAME_23_INDY	501	/* bank E  */

#define	GR3_ZB4_DRAMF_01_INDY	19	/* bank F  */
#define GR3_ZB4_DRAMF_23_INDY	509	/* bank F  */

#define	GR3_ZB4_DRAMG_01_INDY	12	/* bank G  */
#define GR3_ZB4_DRAMG_23_INDY	504	/* bank G  */

#define	GR3_ZB4_DRAMH_01_INDY	6	/* bank H  */
#define GR3_ZB4_DRAMH_23_INDY	500	/* bank H  */

#define	GR3_ZB4_DRAMI_01_INDY	8	/* bank I  */
#define GR3_ZB4_DRAMI_23_INDY	502	/* bank I  */

#define	GR3_ZB4_DRAMJ_01_INDY	3	/* bank J  */
#define GR3_ZB4_DRAMJ_23_INDY	2	/* bank J  */

static unsigned short GR3_ZB4_DRAM_U_INDY[] = {
GR3_ZB4_DRAMA_01_INDY,GR3_ZB4_DRAMA_23_INDY,
GR3_ZB4_DRAMB_01_INDY,GR3_ZB4_DRAMB_23_INDY,
GR3_ZB4_DRAMC_01_INDY,GR3_ZB4_DRAMC_23_INDY,
GR3_ZB4_DRAMD_01_INDY,GR3_ZB4_DRAMD_23_INDY,
GR3_ZB4_DRAME_01_INDY,GR3_ZB4_DRAME_23_INDY,
GR3_ZB4_DRAMF_01_INDY,GR3_ZB4_DRAMF_23_INDY,
GR3_ZB4_DRAMG_01_INDY,GR3_ZB4_DRAMG_23_INDY,
GR3_ZB4_DRAMH_01_INDY,GR3_ZB4_DRAMH_23_INDY,
GR3_ZB4_DRAMI_01_INDY,GR3_ZB4_DRAMI_23_INDY,
GR3_ZB4_DRAMJ_01_INDY,GR3_ZB4_DRAMJ_23_INDY
};
#endif	/* IP22 */


extern void print_GR3_loc(unsigned int);
