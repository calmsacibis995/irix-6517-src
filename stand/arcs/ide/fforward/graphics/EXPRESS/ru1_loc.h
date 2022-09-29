/* 
	 RU1 	-  16 oct 1992
*/
   
#define RU1_RE3_EVEN		20
#define RU1_RE3_ODD		16

/* RU1 Z-Buffer DRAM */

#define	RU1_ZB4_DRAMA_01	505	/* bank A bit 0 - 15 low */
#define RU1_ZB4_DRAMA_23	8	/* bank A bit 16 - 23 low */

#define	RU1_ZB4_DRAMB_01	506	/* bank B  */
#define RU1_ZB4_DRAMB_23	9	/* bank B  */

#define	RU1_ZB4_DRAMC_01	507	/* bank C  */
#define RU1_ZB4_DRAMC_23	10	/* bank C  */

#define	RU1_ZB4_DRAMD_01	508	/* bank D  */
#define RU1_ZB4_DRAMD_23	11	/* bank D  */

#define	RU1_ZB4_DRAME_01	509	/* bank E  */
#define RU1_ZB4_DRAME_23	12	/* bank E  */

#define	RU1_ZB4_DRAMF_01	500	/* bank F  */
#define RU1_ZB4_DRAMF_23	2	/* bank F  */

#define	RU1_ZB4_DRAMG_01	501	/* bank G  */
#define RU1_ZB4_DRAMG_23	3	/* bank G  */

#define	RU1_ZB4_DRAMH_01	502	/* bank H  */
#define RU1_ZB4_DRAMH_23	4	/* bank H  */

#define	RU1_ZB4_DRAMI_01	503	/* bank I  */
#define RU1_ZB4_DRAMI_23	5	/* bank I  */

#define	RU1_ZB4_DRAMJ_01	504	/* bank J  */
#define RU1_ZB4_DRAMJ_23	6	/* bank J  */

#ifdef GR2_BITP
static unsigned short RU1_ZB4_DRAM_U[] = {
RU1_ZB4_DRAMA_01,RU1_ZB4_DRAMA_23,
RU1_ZB4_DRAMB_01,RU1_ZB4_DRAMB_23,
RU1_ZB4_DRAMC_01,RU1_ZB4_DRAMC_23,
RU1_ZB4_DRAMD_01,RU1_ZB4_DRAMD_23,
RU1_ZB4_DRAME_01,RU1_ZB4_DRAME_23,
RU1_ZB4_DRAMF_01,RU1_ZB4_DRAMF_23,
RU1_ZB4_DRAMG_01,RU1_ZB4_DRAMG_23,
RU1_ZB4_DRAMH_01,RU1_ZB4_DRAMH_23,
RU1_ZB4_DRAMI_01,RU1_ZB4_DRAMI_23,
RU1_ZB4_DRAMJ_01,RU1_ZB4_DRAMJ_23
};
#endif

/* RU1 Frame Buffer RAM */

#define	RU1_VRAM0_A	28	/* Bank 0 bit 0 - 7 */
#define	RU1_VRAM1_A	32	/* Bank 1 bit 0 - 7 */
#define	RU1_VRAM2_A 	29	/* Bank 2 bit 0 - 7 */
#define	RU1_VRAM3_A	31	/* Bank 3 bit 0 - 7 */
#define	RU1_VRAM4_A	30	/* Bank 4 bit 0 - 7 */

#define	RU1_VRAM0_B	519	/* Bank 0 bit 8 - 15 */
#define	RU1_VRAM1_B	523	/* Bank 1 bit 8 - 15 */
#define	RU1_VRAM2_B 	520	/* Bank 2 bit 8 - 15 */
#define	RU1_VRAM3_B	522	/* Bank 3 bit 8 - 15 */
#define	RU1_VRAM4_B	521	/* Bank 4 bit 8 - 15 */

#define	RU1_VRAM0_C	22	/* Bank 0 bit 16 - 23 */
#define	RU1_VRAM1_C	26	/* Bank 1 bit 16 - 23 */
#define	RU1_VRAM2_C 	23	/* Bank 2 bit 16 - 23 */
#define	RU1_VRAM3_C	25	/* Bank 3 bit 16 - 23 */
#define	RU1_VRAM4_C	24	/* Bank 4 bit 16 - 23 */

#ifdef GR2_BITP
static unsigned short RU1_VRAM_U[] = {
RU1_VRAM0_A,RU1_VRAM1_A,RU1_VRAM2_A,RU1_VRAM3_A,RU1_VRAM4_A,
RU1_VRAM0_B,RU1_VRAM1_B,RU1_VRAM2_B,RU1_VRAM3_B,RU1_VRAM4_B,
RU1_VRAM0_C,RU1_VRAM1_C,RU1_VRAM2_C,RU1_VRAM3_C,RU1_VRAM4_C
};
#endif

/* CID planes */

#define RU1_CID_VRAM0 	513	/* Bank 0 bit 0 - 3 */
#define RU1_CID_VRAM1 	517	/* Bank 1 bit 0 - 3 */
#define RU1_CID_VRAM2 	514	/* Bank 2 bit 0 - 3 */
#define RU1_CID_VRAM3 	516	/* Bank 3 bit 0 - 3 */
#define RU1_CID_VRAM4 	515	/* Bank 4 bit 0 - 3 */
#ifdef GR2_BITP
static unsigned short RU1_CID_VRAM_U[] = {
RU1_CID_VRAM0,RU1_CID_VRAM1,RU1_CID_VRAM2,RU1_CID_VRAM3,RU1_CID_VRAM4 };
#endif

/* auxiliary planes */

#define RU1_AUX_VRAM0 	513	/* Bank 0 bit 0 - 3 */
#define RU1_AUX_VRAM1 	517	/* Bank 1 bit 0 - 3 */
#define RU1_AUX_VRAM2 	514	/* Bank 2 bit 0 - 3 */
#define RU1_AUX_VRAM3 	516	/* Bank 3 bit 0 - 3 */
#define RU1_AUX_VRAM4 	515	/* Bank 4 bit 0 - 3 */
#ifdef GR2_BITP
static unsigned short RU1_AUX_VRAM_U[] = {
RU1_AUX_VRAM0,RU1_AUX_VRAM1,RU1_AUX_VRAM2,RU1_AUX_VRAM3,RU1_AUX_VRAM4 };
#endif
