#include <assert.h>
#include <flash.h>
#include <fcntl.h>
#include "sim.h"
#include "simSvr.h"
#include <stdio.h>
#include <string.h>
#include <sys/cpu.h>
#include <unistd.h>

#define SIM_FLASH_SIZE (1024*512)
#define SIM_FLASH_MASK (SIM_FLASH_SIZE-1)

static void loadFlash(void);
static void flashWrite(long* ptr, long data);
static void flashFlush(void);
static void flashEnable(int);

typedef struct{
  char         flash[SIM_FLASH_SIZE];
  FlashROM     flashROM;	/* Flash object */
  SimAddrMap_t map;		/* Write map */
  SimAddrMap_t map2;		/* Read map */
  long long    writeEnable;
  SimAddrMap_t map3;		/* Map for writeEnable */
} Flash_t;

static Flash_t* flash;

/* Stubs to satisify the fault logic in the included flash.o & flashwrite.o */
int gen_pda_tab;
int cpuid(void){return 0;}

/**************
 * simFlashInit
 */
void
simFlashInit(void){
  flash = (Flash_t*)usmemalign(4096,sizeof(Flash_t),arena);
  assert(flash);
  memset(flash,0,sizeof(Flash_t));

  /*
   * Set up flash write map.  This is a straightforward client side
   * map with flash->writeEnable as the ignoreIfZero control.
   */
  flash->map.base   = flash->flash;
  flash->map.stride = 1;
  flash->map.mask   = SIM_FLASH_MASK;
  flash->map.shift  = 0;
  flash->map.ignoreIfZero = (long*)&flash->writeEnable;

  /*
   * Set up flash read map.  This is the same as the write map excepth
   * there is no ignoreIfZero control.
   */
  flash->map2.base   = flash->flash;
  flash->map2.stride = 1;
  flash->map2.mask   = SIM_FLASH_MASK;
  flash->map2.shift  = 0;
  flash->map2.ignoreIfZero = 0;

  /* Set up flash write enable map. */
  flash->map3.base   = (char*)&flash->writeEnable;
  flash->map3.stride =  0;
  flash->map3.mask   =  0;
  flash->map3.shift  =  0;
  flash->map3.ignoreIfZero = 0;

  /* Set up flashROM structure */
  flash->flashROM.base = (FlashSegment*)&flash->flash;
  flash->flashROM.limit = (FlashSegment*)(flash->flash+FLASH_SIZE);
  flash->flashROM.pageSize = FLASH_PAGE_SIZE;
  flash->flashROM.flashSize = FLASH_SIZE;
  flash->flashROM.write = flashWrite;
  flash->flashROM.writeFlush = flashFlush;
  flash->flashROM.writeEnable = flashEnable;
  
  /*
   * Establish read and write service for FLASH RAM
   * Note that all accesses are processed on the client side.  (We don't
   *    emulate the FLASH "magic handshakes".)
   */
  simRegisterAddrMatch(~SIM_FLASH_MASK, 			/* READ */
		       TO_PHYSICAL(FLASH_BASE),
		       SIM_loadStore,
		       0,
		       &flash->map2);

  simRegisterAddrMatch(~SIM_FLASH_MASK, 			/* WRITE */
		       TO_PHYSICAL(FLASH_BASE) | ADDR_MATCH_STORE,
		       SIM_loadStore,
		       0,
		       &flash->map); 

  /*
   * Establish read/write service for FLASH write enable
   * Note that this also is handled on the client side.  (We only require
   *    the WE word be non-zero, we don't check the bits.)
   */
  simRegisterAddrMatch(-1 & (~ADDR_MATCH_STORE),
		       TO_PHYSICAL(ISA_FLASH_NIC_REG),
		       SIM_loadStore,
		       0,
		       &flash->map3);


  /* Construct the initial contents of the FLASH */
  loadFlash();
}


#define SLOADER_LENGTH (16*1024)
#define ENV_LENGTH     (8*1024)
#define POST1_LENGTH   (8*1024)
#define FW_LENGTH      (8*1024)

#define ROUND          (FLASH_PAGE_SIZE-1)

/***********
 * loadFlash	Load initial contents of flash
 */
static void
loadFlash(void){
  int t;
  char* fb = flash->flash;
  int fd;

  /* Attempt to load flash from flash.image and ../images/flash.image */
  
  fd = open("flash.image",O_RDONLY);
  if (fd<0){
    fd = open("../images/flash.image",O_RDONLY);
    if (fd<0){
      fprintf(stderr,
	      "--Could not open flash.image or ../images/flash.image\n"
	      "  Constructing dummy flash contents\n");
      
      /*
       * Create valid but empty segment for "sloader".
       * Create valid but empty segment for "env".
       * Create valid but empty segment for "post1".
       * Create valid but empty segment for "fw".
       *
       * Note: The segment length passed to writeFlashSegment does not count
       *       the header, only the body length.  To insure that these segments
       *       will properly align, we reduce the body length by a length just
       *       less than the flash page size. 
       */
      t = writeFlashSegment(&flash->flashROM,
			    (FlashSegment*)fb,
			    SLOADER_FLASH_SEGMENT,
			    "V1.0",
			    (long*)(fb+64*1024), /* zeros... */
			    SLOADER_LENGTH-ROUND);
      assert(t==0);
      
      t = writeFlashSegment(&flash->flashROM,
			    (FlashSegment*)(fb+SLOADER_LENGTH),
			    ENV_FLASH_SEGMENT,
			    ENV_FLASH_VSN,
			    (long*)(fb+64*1024), /* zeros... */
			    ENV_LENGTH-ROUND);
      assert(t==0);
      
      t = writeFlashSegment(&flash->flashROM,
			    (FlashSegment*)(fb+SLOADER_LENGTH+ENV_LENGTH),
			    "post1",
			    "V1.0",
			    (long*)(fb+64*1024), /* zeros... */
			    POST1_LENGTH-ROUND);
      assert(t==0);
      
      t = writeFlashSegment(&flash->flashROM,
			    (FlashSegment*)(fb+SLOADER_LENGTH+
					    ENV_LENGTH+POST1_LENGTH),
			    FW_SEGMENT,
			    "V1.0",
			    (long*)(fb+64*1024), /* zeros... */
			    FW_LENGTH-ROUND);
      assert(t==0);
      
      t = writeFlashSegment(&flash->flashROM,
			    (FlashSegment*)(fb+SLOADER_LENGTH+
					    ENV_LENGTH+POST1_LENGTH+FW_LENGTH),
			    "",
			    "",
			    (long*)(fb+64*1024), /* zeros... */
			    0);
      return;
    }
  }
  t = read(fd,fb,SIM_FLASH_SIZE);
  close(fd);
}


/************
 * flashWrite	Write a long to FLASH
 */
static void
flashWrite(long* ptr, long data){
  char* dst = (char*)ptr;
  char* src = (char*)&data;
  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
}


/************
 * flashFlush	Flush write to flash
 */
static void
flashFlush(void){
}

/*************
 * flashEnable	Enable writes to the flash
 */
static void
flashEnable(int e){
}
