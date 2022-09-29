#include "SIM.h"
#include <flash.h>
#include <string.h>

/*
 * FLASH SIMblock
 */
char  SIMflash[flashblock_SIZE+4096];
char* SIMflashblock;
static long SIMflashWE;



/**************
 * initSimFlash
 */
void
initSimFlash(){
  extern FlashROM envFlash;	/* Defined in stubs for now. */

  /* Set to "unprogrammed" state */
  memset(SIMflash,-1,sizeof(SIMflash));

  /* Write an empty SLOADER segment so that we can lookup the ENV faster */
  writeFlashSegment(&envFlash,
		    (FlashSegment*)SIMflashblock,
		    SLOADER_FLASH_SEGMENT,
		    "1.0",
		    (long*)SIMflashblock+100, /* Feed it zeros */
		    1024*8-100);
#ifndef xxx
  /* 
   * Write a zero length segment past the ENV segment area to terminate search
   * for segments early on initialization.
   */
  writeFlashSegment(&envFlash,
		    (FlashSegment*)(SIMflashblock+(9*1024)),
		    "",
		    "",
		    (long*)(SIMflashblock+(10*1024)),
		    0);
#endif
}



/***************
 * SIM_flash_ram
 */
void
SIM_flash_ram(Instr_t inst, Saddr_t eadr, Ctx_t* sc, int ignore){
  if (SIMflashWE || !isStore(inst))
    loadstore(inst, sc,(eadr & flashblock_MASK)+SIMflashblock);
}

/**************
 * SIM_flash_we
 */
void
SIM_flash_we(Instr_t inst, Saddr_t eadr, Ctx_t* sc, int ignore){
  loadstore(inst, sc,&SIMflashWE);
}
