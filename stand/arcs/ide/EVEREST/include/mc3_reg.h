/*******************************************************************
 * 
 * mc3_reg.h
 *
 * This file contains the virtual addresses  (in k1seg) for everest 
 * registers visible to the processor.
 *
 * Note: this file should track the everest/design/includes/reg_defs.h
 *       file for the latest register numbers.
 *
 * $Revision: 1.2 $
 *
 ********************************************************************/

/* used by cachemem, mem_ecc */
#define READ 0
#define WRITE 1

#define get_cc_conf(r) \
                dold(dold(EV_SPNUM) << 9 | (r) << 3 | EV_CONFIGREG_BASE)
/* used in map_addr calls */
#define CACHED 1
#define UNCACHED 0
#define DIRECT_MAP 0x20000000
