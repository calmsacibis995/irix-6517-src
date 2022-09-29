/*
 * -------------------------------------------------------------
 * $Revision: 1.1 $
 * $Date: 1997/08/18 20:41:12 $
 * -------------------------------------------------------------
 */

/*
 *  Primary/Secondary cache Instruction/Data
 */
#define PI                  0x00000000 /* Primary Instruction  */
#define PD                  0x00000001 /* Primary Data.        */
#define SI                  0x00000002 /* Secondary Instruc.   */
#define SD                  0x00000003 /* Secondary Data.      */

/*
 *  Cache ops.
 */
#define SDC                 0x00000000 /* Secondary cache clear sequence*/
#define IWI                 0x00000000 /* Index Writeback Invalidate    */
#define ILT                 0x00000004 /* Index Load Tags               */
#define IST                 0x00000008 /* Index Store Tags              */
#define CDE                 0x0000000C /* Create Dirty Exclusive.       */
#define HitI                0x00000010 /* Hit Invalidate.               */
#define HWI                 0x00000014 /* Hit WriteBack Invalidate.     */
#define FILL                0x00000014 /* I-FILL                        */
#define SCPI                0x00000014 /* Secondary cache page invalid  */
#define HWB                 0x00000018 /* Hit WriteBack.                */
#define HSV                 0x0000001C /* Hit Set Virtual.              */

/*
 * R4600 primary cache state.
 */
#define INVALID             0x0
#define SHARED              0x1
#define CLEAN_EC            0x2
#define DIRTY_EC            0x3


/*
 * -------------------------------------------------------------
 *
 * $Log: cacheops.h,v $
 * Revision 1.1  1997/08/18 20:41:12  philw
 * updated file from bonsai/patch2039 tree
 *
 * Revision 1.2  1996/10/31  21:51:07  kuang
 * Bring Bonsai IP32 debugcard up to the level of IP32 debugcard v2.4 on Pulpwood
 *
 * Revision 1.1  1995/11/15  00:41:21  kuang
 * initial checkin
 *
 * Revision 1.1  1995/11/14  22:49:34  kuang
 * Initial revision
 *
 * -------------------------------------------------------------
 */

/* END OF FILE */
