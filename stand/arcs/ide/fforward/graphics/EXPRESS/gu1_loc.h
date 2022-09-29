/* 
	 GU1 	- 003, 31 dec 1992
*/
   
/* GU1 chip designation */ 

#define GU1_HQ2_LOC	29
#define	GU1_GE7_0_LOC	21
#define	GU1_GE7_1_LOC	21
#define	GU1_GE7_2_LOC	21
#define	GU1_GE7_3_LOC	21
#define	GU1_GE7_4_LOC	16 
#define	GU1_GE7_5_LOC	16 
#define	GU1_GE7_6_LOC	16 
#define	GU1_GE7_7_LOC	16
#if 0
static unsigned short GU1_GE7_U[] = {
GU1_GE7_0_LOC,GU1_GE7_1_LOC,GU1_GE7_2_LOC,GU1_GE7_3_LOC,
GU1_GE7_4_LOC,GU1_GE7_5_LOC,GU1_GE7_6_LOC,GU1_GE7_7_LOC };
#endif

/* GE7 Ucode RAM location */

#define GU1_GE7_URAM0_LOC 	510  	/* bit  0-15   */	
#define GU1_GE7_URAM1_LOC 	17	/* bit 16-31   */	
#define GU1_GE7_URAM2_LOC 	511	/* bit 31-47   */	
#define GU1_GE7_URAM3_LOC 	18	/* bit 48-63   */	
#define GU1_GE7_URAM4_LOC 	515	/* bit 64-79   */	
#define GU1_GE7_URAM5_LOC 	22	/* bit 80-95   */	
#define GU1_GE7_URAM6_LOC 	513	/* bit 96-111  */	
#define GU1_GE7_URAM7_LOC 	25	/* bit 112-127 */	
#if 0
static unsigned short GU1_GE7_URAM_U[] = {
GU1_GE7_URAM0_LOC,GU1_GE7_URAM1_LOC,GU1_GE7_URAM2_LOC,GU1_GE7_URAM3_LOC,
GU1_GE7_URAM4_LOC,GU1_GE7_URAM5_LOC,GU1_GE7_URAM6_LOC,GU1_GE7_URAM7_LOC,
};
#endif

/* GE7 Share RAM location */

#define GU1_GE7_SRAM0_LOC	516	/* bit  0-7    */
#define GU1_GE7_SRAM1_LOC	517	/* bit  8-15   */
#define GU1_GE7_SRAM2_LOC	33	/* bit  16-23  */
#define GU1_GE7_SRAM3_LOC	32	/* bit  24-31  */
#if 0
static unsigned short GU1_GE7_SRAM_U[] = {
GU1_GE7_SRAM0_LOC,GU1_GE7_SRAM1_LOC,GU1_GE7_SRAM2_LOC,GU1_GE7_SRAM3_LOC };
#endif

/* HQ Ucode RAM */

#define GU1_HQ2_URAM0_LOC	34	/* bit 0-7      */
#define GU1_HQ2_URAM1_LOC	35	/* bit 8-15     */
#define GU1_HQ2_URAM2_LOC	518	/* bit 15-23    */


/* GU1 Z-Buffer DRAM */

#define	GU1_ZB4_DRAMA_01	505	/* bank A bit 0 - 15 low */
#define GU1_ZB4_DRAMA_23	8	/* bank A bit 16 - 23 low */

#define	GU1_ZB4_DRAMB_01	506	/* bank B  */
#define GU1_ZB4_DRAMB_23	9	/* bank B  */

#define	GU1_ZB4_DRAMC_01	507	/* bank C  */
#define GU1_ZB4_DRAMC_23	10	/* bank C  */

#define	GU1_ZB4_DRAMD_01	508	/* bank D  */
#define GU1_ZB4_DRAMD_23	11	/* bank D  */

#define	GU1_ZB4_DRAME_01	509	/* bank E  */
#define GU1_ZB4_DRAME_23	12	/* bank E  */

#define	GU1_ZB4_DRAMF_01	500	/* bank F  */
#define GU1_ZB4_DRAMF_23	2	/* bank F  */

#define	GU1_ZB4_DRAMG_01	501	/* bank G  */
#define GU1_ZB4_DRAMG_23	3	/* bank G  */

#define	GU1_ZB4_DRAMH_01	502	/* bank H  */
#define GU1_ZB4_DRAMH_23	4	/* bank H  */

#define	GU1_ZB4_DRAMI_01	503	/* bank I  */
#define GU1_ZB4_DRAMI_23	5	/* bank I  */

#define	GU1_ZB4_DRAMJ_01	504	/* bank J  */
#define GU1_ZB4_DRAMJ_23	6	/* bank J  */

#ifdef GR2_BITP
static unsigned short GU1_ZB4_DRAM_U[] = {
GU1_ZB4_DRAMA_01,GU1_ZB4_DRAMA_23,
GU1_ZB4_DRAMB_01,GU1_ZB4_DRAMB_23,
GU1_ZB4_DRAMC_01,GU1_ZB4_DRAMC_23,
GU1_ZB4_DRAMD_01,GU1_ZB4_DRAMD_23,
GU1_ZB4_DRAME_01,GU1_ZB4_DRAME_23,
GU1_ZB4_DRAMF_01,GU1_ZB4_DRAMF_23,
GU1_ZB4_DRAMG_01,GU1_ZB4_DRAMG_23,
GU1_ZB4_DRAMH_01,GU1_ZB4_DRAMH_23,
GU1_ZB4_DRAMI_01,GU1_ZB4_DRAMI_23,
GU1_ZB4_DRAMJ_01,GU1_ZB4_DRAMJ_23
};
#endif

/*	 GU1 	- 004, 19 Feb 1993 */

#define GU1_004_HQ2_LOC	29

/* GE7 Share RAM location */

#define GU1_004_GE7_SRAM0_LOC	516	/* bit  0-7    */
#define GU1_004_GE7_SRAM1_LOC	517	/* bit  8-15   */
#define GU1_004_GE7_SRAM2_LOC	33	/* bit  16-23  */
#define GU1_004_GE7_SRAM3_LOC	32	/* bit  24-31  */

/* HQ Ucode RAM */

#define GU1_004_HQ2_URAM0_LOC	34	/* bit 0-7      */
#define GU1_004_HQ2_URAM1_LOC	35	/* bit 8-15     */
#define GU1_004_HQ2_URAM2_LOC	518	/* bit 15-23    */

extern void print_GU1_loc(unsigned int);
