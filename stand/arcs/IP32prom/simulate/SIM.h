#ifndef _SIM_H_
#define _SIM_H_
#include <signal.h>
#include <sys/inst.h>

typedef union mips_instruction Instr_t;
typedef struct sigcontext Ctx_t;
typedef __uint64_t Saddr_t;
typedef void(*simFunc)(Instr_t, Saddr_t, struct sigcontext*,int);


/*
 * MEMBLOCK is a large block of memory to which references to k0 and k1
 * memory are redirected.
 */
#define MEMBLOCK_SIZE (1024*1024*2)
#define MEMBLOCK_MASK (MEMBLOCK_SIZE-1)
extern char SIMblock[MEMBLOCK_SIZE+4096];
extern char* SIMmemblock;		/* Pointer to pseudo ram */


/*
 * FLASH 
 */
#define flashblock_SIZE (1024*512)
#define flashblock_MASK (flashblock_SIZE-1)
extern char  SIMflash[flashblock_SIZE+4096];
extern char* SIMflashblock;

int  isStore(Instr_t);
void loadstore(Instr_t,Ctx_t* sc, void* addr);
int loadstoreSize(Instr_t);
void advancePC(Instr_t,Ctx_t*);
Saddr_t getEffectiveAddress(Instr_t,Ctx_t*);

void SIM_k0_k1(Instr_t,Saddr_t,Ctx_t* sc, int);
void SIM_mace_serial(Instr_t,Saddr_t,Ctx_t* sc, int);
void SIM_led(Instr_t,Saddr_t,Ctx_t* sc, int);
void SIM_flash_ram(Instr_t,Saddr_t,Ctx_t* sc, int);
void SIM_flash_we(Instr_t,Saddr_t,Ctx_t* sc, int);
void SIM_message(const char*);
void SIM_rtc(Instr_t,Saddr_t,Ctx_t* sc, int);
void SIM_mac110(Instr_t,Saddr_t,Ctx_t* sc, int);

void SIM_assert(char*, char*, int);
#define sim_assert(e) ((e)?((void)0):SIM_assert(#e, __FILE__, __LINE__))

#define UNIMPLEMENTED 0
#define IMPOSSIBLE 0
#define NORETURN 0

#define SERIAL0 0x08
#define SERIAL1 0x00
#define SERIAL2 0xaa
#define SERIAL3 0xbb
#define SERIAL4 0xcc
#define SERIAL5 0xdd
#endif

