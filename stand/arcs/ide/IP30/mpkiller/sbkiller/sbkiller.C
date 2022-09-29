/*			
 * NAME:	sbkiller.c
 * AUTHOR:	Anoosh Hosseini
 * COPYRIGHT:   Copyright (C) 1990,1991,1992,1993 Mips Computer Systems, All rights Reserved.
 *
 * Comments:
 * - when using Interlocks, number of store and load base registers has to
 *   be the same.
 * -currently we can generate 4 streams for MP, need to modify code for greater
 *   more than that.
 * Now requires stack to be initialized, sp.
 * For MPkiller, r26 contains 
 */

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<CC/iostream.h>
#include <stream.h>
#include<ctype.h>
#include<math.h>
#include<time.h>
#include<string.h>

#include "sbkiller.h"

/* add option for printing memory */
/* add option for settin random seed */

/* here we set how many register are assigned 
 * base registers, random number holders and  load destination
 * registers.
 */


char versionG[] = "$Revision: 1.2 $";
char traceFileName[128];

int             lockPtr = 0;
int             baseAddr64BitMode = FALSE;

struct lock allLocks[MAXLOCKS];

int  addrMode64G = FALSE;

ULL  baseReg64[MAX_BASE_REG];
ULL  baseRegPhysAddr[MAX_BASE_REG];

UINT baseAddrMode[MAX_BASE_REG];

ULL  immdReg64[MAX_IMMD_REG];
ULL  immdFpuReg64[MAX_IMMD_FPU_REG];

int  useSharedLabelAddr = FALSE;
ULL  sharedSN0AddrG    = 0x9400000000800000LL;

ULL  compReg64[MAX_COMP_REG];

UINT loadRegValidG[MAX_BASE_REG];
UINT compRegValidG[MAX_COMP_REG];

ULL  compFpuReg64[MAX_COMP_FPU_REG];
UINT compFpuRegValidG[MAX_COMP_FPU_REG];
UINT fpuDataType[MAX_COMP_FPU_REG];

UINT dbl[MAX_COMP_REG];
UINT mipsMode[MAX_COMP_REG];

UINT mappedExtraVA[MAX_MAP_EXTRA];

int  mpMapTable[MAX_CPU_MP][MAX_CPU_MP];

UINT baseRegG;
UINT baseImmdRegG;
UINT baseCompRegG;

UINT baseFpuRegG;
UINT baseImmdFpuRegG;
UINT baseCompFpuRegG;

UINT baseLdStAddr; 

/* 64 bit virtual base address, either 32 bit cached unmapped address
   or 64 bit mapped address, with the assumption that the TLB handler
   uses a  1-1 mapping 
*/
int  baseAddrSet    = FALSE;
int  baseUncAddrSet = FALSE;
         
ULL  baseLdStAddr64; 
ULL  baseUncLdStAddr64;

int  addrMode64BitsG = FALSE;

UINT addrRangeG;
ULL  MPwordG;
UINT PageOffset;

UINT lockBaseG;
ULL  lockBase64G;

int     burstModeG       = 0;

int   	flushAllRegRateG = 4;
int	cacheWrapRateG   = 4;

int     noCacheOpsG        = 0;
int     sharedLockCounterG = 0; 
int     sharedLockGenRateG = 0;

int	disableNodeRead = FALSE;
int     doDebugBeastG   = FALSE;
int     doFPopG         = FALSE;
int	loopG 	 	= 100;
int	countG	 	= 100;
int	syncCountG	= 20;
int	recycleCountG   = 0;
int     curCpuIdG       = 0;
int     fpRepeatCounterG = 0; 

int     maxCompRegG     = 0;
int     maxImmdRegG     = 0;
int     maxBaseRegG     = 0;

int     maxCompFpuRegG  = 0;
int     maxImmdFpuRegG  = 0;

int	syncCounter 	= 0;
int	numOfCpuG 	= 4;
int	numLocksG 	= 10;
int	lockIncG	= 5;

int	extIntvCountG   = 30;
int	extIntrCountG   = 60;
int	extInvCountG    = 40;
int	extNackCountG   = 40;
int	extRdRdyCountG  = 40;
int	extWrRdyCountG  = 40;
int     lockAddrStepG   = 0;
int     randEventRateG  = 0;

int     doBurstG        = FALSE;
int     burstMinG       = 32;     // min burst count
int     burstMaxG       = 64;    // max burst count
int     burstDeltaG     = 256;   // vary offset by this each time (may not even be same base reg
int     burstRateG      = 100;    //
int     burstLdStDelayG = 16;    // delay between load and store streams

int     speedReadRateG  = 30;
int     doSpeedG        = FALSE;

int     doUseFpuRegG    = TRUE;
int     fpuFRbit1G      = TRUE;  // if true FR bit == 1

int     watchdogTimer    = FALSE;
unsigned long long   watchdogTimeout = 100000UL;    // loop count for watchdog timer
int     writeSN0LED    = FALSE;
int 	byPassHitInvSI  = FALSE;
int	recycleLimitG 	= maxCompRegG -1;
int     doSyncOp        = FALSE;
int	randDelayMultG	= 0;
int     doAtomicG       = FALSE;
int	doMemFileOutputG= FALSE;
int	onHardwareG	= FALSE;
int	doAluG	 	= FALSE;
int	doLlScG		= TRUE;
int	doBlackBirdG	= FALSE;
int	BigEndianG	= TRUE;
int	printMemG	= FALSE;
int	useSyncG	= FALSE;
int	mips2G	 	= FALSE;	/* default Mips3 instruction set */
int	mips4G	 	= FALSE;	
int	noSecondCacheG  = FALSE;	/* default secondary cache */
int	noPrimaryCacheG = FALSE;	/* default primary cache */
int     noInitG         = FALSE;
int	userSetSeed     = FALSE;
int	doMP		= FALSE;
int	cacheWrapG      = TRUE;
int	reverseEndianG  = FALSE;
int	ImappedG	= FALSE;
int	IkernelG	= TRUE;
int	useTLB 		= TRUE;
int	sameMPcodeG	= FALSE;
int	noUncachedG 	= FALSE; 
int	forICE 		= FALSE;
int	VCEmodeG 	= FALSE;
int	noFPUloadsG	= FALSE;
int    	checkPrefSyncG  = FALSE;
int	uncachedAccG    = FALSE;
int	T5ModeG		= FALSE;
int	prefetchG	= FALSE;
int	extIntrG        = FALSE;
int	extIntvG        = FALSE;
int	extInvG		= FALSE;
int	extNackG	= FALSE;
int	userTLBHandlerG = FALSE;
int	lockBaseSetG	= FALSE;
int	rdRdyG		= FALSE;
int	wrRdyG		= FALSE;
int     doMpTrace       = FALSE;
int     doWordDwordOP   = FALSE;

int     extIntvCounterG  = 0; /* dynamic counters */
int	extInvCounterG   = 0;
int	extIntrCounterG  = 0;
int	extNackCounterG  = 0;
int	extRdRdyCounterG = 0;
int	extWrRdyCounterG = 0;

int	minRdRdyDuration = 20;
int	maxRdRdyDuration = 100;
int	minWrRdyDuration = 20;
int	maxWrRdyDuration = 100;


int	doShivaG   = FALSE;
int 	doFetchOps = FALSE;
int	doSN0M    = FALSE;
int 	doSN0N    = FALSE;
int 	doBarrierG = FALSE;

int   ContinueOnLLSCMiscompare = FALSE;
int   QuietLLSCMiscompare = FALSE;
int   NopsBeforeLL = FALSE;
int   NopsAfterSC = FALSE;
int   COP1AfterSC = FALSE;
int   SCBranchLikely = FALSE;

int barrierCounter       = 0;
int numBarriersDone      = 0;
int numBarrierLckToClear = 0;
int barrierDropRate      = 100;
int maxNumBarrier        = 100;  
int numCpuSetByUser	 = 0;

int hwIterationCountG    = 1;

char	mpMapFileName[256];

sharedOp * sharedLocks;
int 	extEventIdG = 0; /* unique external event ID */ 


UINT	MPaddrMask   = 0xffffffe0;
ULL	MPaddrMask64 = 0xffffffffffffffe0LL;

UINT	tmpReg1G;
UINT	tmpReg2G;

UINT	lastLastStoreAdr;
UINT	lastStoreAdr;

char 	templateFile[256];
FILE	* memFile;
FILE    * MPtraceFile;

// declare the sbkiller memory model

Memory memoryModel;

struct cell *topOfMemory = NULL;

#define PREFSYNC_TYPE_PREF	1
#define PREFSYNC_TYPE_SYNC	2
#define PREFSYNC_CMD_PREF	8
#define PREFSYNC_CMD_SYNC	10
#define PREFSYNC_MAX_COUNT	50

UINT	prefSyncVaddrLo[32];
UINT	prefSyncVaddrHi[32];
UINT	prefSyncPaddrLo[32];
UINT	prefSyncPaddrHi[32];

PrefSyncRecord  prefSyncInfoG[PREFSYNC_MAX_COUNT];

int	nextPrefSyncInfoG = 0;

int	nextCheckPrefSyncLabel = 0;
int	nextIssuePrefSyncLabel = 0;



// if we get it in this format, then user specified a
// hex address, and we work from that

Work :: Work ( int op ,ULL addr, ULL dat  ) {

 operation = op;
 type      = ADDR_HEX;
 data	   = dat;
 address   = addr;
 next      = NULL;

 strcpy(addrLabel,"");

}

// this is the label version 

Work :: Work ( int op ,char * label , ULL dat  ) {

 operation = op; 
 type      = ADDR_LABEL;
 data	   = dat;
 address   = 0;
 next      = NULL;
 strcpy(addrLabel,label);

}

//

int Work :: getOp()  {
    return ( operation );
}

//

int Work :: getData() {
    return ( data );
}

//

int  Work :: execute ( ) {
    switch ( operation ) {
       
    case FUNC_LLSC_INC:

   if (NopsBeforeLL) {
	  if ( type == ADDR_HEX )
	    printf( " 	       	DINCLOCK_NOPLL(0x%0llx,0x%llx )\n",address,data);
	  else
	    printf( " 		DINCLOCK_L_NOPLL(%s,0x%llx )\n",addrLabel,data);
   } else if (SCBranchLikely) {
	  if ( type == ADDR_HEX )
	    printf( " 	       	DINCLOCK_SCBRL(0x%0llx,0x%llx )\n",address,data);
	  else
	    printf( " 		DINCLOCK_L_SCBRL(%s,0x%llx )\n",addrLabel,data);
   } else if (NopsAfterSC) {
	  if ( type == ADDR_HEX )
	    printf( " 	       	DINCLOCK_SCNOP(0x%0llx,0x%llx )\n",address,data);
	  else
	    printf( " 		DINCLOCK_L_SCNOP(%s,0x%llx )\n",addrLabel,data);
   } else if (COP1AfterSC) {
	  if ( type == ADDR_HEX )
	    printf( " 	       	DINCLOCK_SCCOP1(0x%0llx,0x%llx )\n",address,data);
	  else
	    printf( " 		DINCLOCK_L_SCCOP1(%s,0x%llx )\n",addrLabel,data);
   } else {
	  if ( type == ADDR_HEX )
	    printf( " 	       	DINCLOCK(0x%0llx,0x%llx )\n",address,data);
	  else
	    printf( " 		DINCLOCK_L(%s,0x%llx )\n",addrLabel,data);
   }

	break;


    case FUNC_FETCH_INC_OP:

	if ( type == ADDR_HEX )
	    printf("  		FUNC_FETCH_INC_OP(0x%0llx,0x%llx )\n",address,data);
	else
	    printf("  		FUNC_FETCH_INC_OP_L(%s,0x%llx )\n",addrLabel,data);
	break;

    case FUNC_FETCH_DEC_OP:

	if ( type == ADDR_HEX )
	    printf("  		FUNC_FETCH_DEC_OP(0x%0llx,0x%llx )\n",address,data);
	else
	    printf("  		FUNC_FETCH_DEC_OP_L(%s,0x%llx )\n",addrLabel,data);
	break;

    case FUNC_OR_OP:

	if ( type == ADDR_HEX )
	    printf( " 	       	FUNC_OR_OP(0x%0llx,0x%llx )\n",address,data);
	else
	    printf( " 		FUNC_OR_OP_L(%s,0x%llx)\n",addrLabel,data);
	break;

    case FUNC_AND_OP:

	if ( type == ADDR_HEX )
	    printf( " 	       	FUNC_AND_OP(0x%0llx,0x%llx )\n",address,data);
	else
	    printf( " 		FUNC_AND_OP_L(%s,0x%llx)\n",addrLabel,data);
	break;

    case FUNC_INC_OP:

	if ( type == ADDR_HEX )
	    printf("  		FUNC_INC_OP(0x%0llx,0x%llx )\n",address,data);
	else
	    printf("  		FUNC_INC_OP_L(%s,0x%llx )\n",addrLabel,data);
	break;

    case FUNC_DEC_OP:

	if ( type == ADDR_HEX )
	    printf("  		FUNC_DEC_OP(0x%0llx,0x%llx )\n",address,data);
	else
	    printf("  		FUNC_DEC_OP_L(%s,0x%llx )\n",addrLabel,data);
	break;

    case FUNC_INIT_ADDRESS_OP:

	if ( type == ADDR_HEX )
	    printf("  		FUNC_INIT_ADDRESS_OP( 0x%0llx,%lld )\n",address,data);
	else
	    printf("  		FUNC_INIT_ADDRESS_OP_L( %s,%lld )\n",addrLabel,data);
	break;

    case INIT_ADDRESS:

	if ( type == ADDR_HEX )
	    printf("  		INIT_ADDRESS( 0x%0llx,%lld )\n",address,data);
	else
	    printf("  		INIT_ADDRESS_L( %s,%lld )\n",addrLabel,data);
	break;      

    case FUNC_BARRIER_FETCH_OP:

	if ( type == ADDR_HEX )
	    printf("  		FUNC_BARRIER_FETCH_OP( 0x%0llx,%lld )\n",address,data);
	else
	    printf("  		FUNC_BARRIER_FETCH_OP( %s,%lld )\n",addrLabel,data);
	break;


    case CHECK_LLSC_INC:
        printf("\n # check shared LLSC lock at addr %0x16llx\n\n",address);
   if ( ContinueOnLLSCMiscompare && !QuietLLSCMiscompare) {
	  printf("		dla	r5,saveReg		\n");
	  printf("		sd		v0, (r5)		\n");
	  printf("		sd		v1, 8(r5)		\n");
	  printf("		sd		a0, 16(r5)		\n");
	  printf("		sd		t1, 24(r5)		\n");
	  printf("		sd		t2, 32(r5)		\n");
	  printf("		sd		t3, 40(r5)		\n");
	  printf("		sd		t4, 48(r5)		\n");
	  printf("		sd		t5, 56(r5)		\n");
	  printf("		sd		t6, 64(r5)		\n");
	  printf("		sd		t7, 72(r5)		\n");
	  printf("		sd		t8, 80(r5)		\n");
	  printf("		sd		t9, 88(r5)		\n");
	  printf("		sd		r6, 96(r5)		\n");
	  printf("		sd		r28, 104(r5)		\n");
	  printf("		sd		r29, 112(r5)		\n");
	  printf("		sd		r1, 120(r5)		\n");
	  printf("		sd		r31, 128(r5)		\n");
   }
	printf("		li	r5,%lld		        \n",data);
	printf("                mult    r6,r5                   \n");
	printf("                mflo    r5                      \n");
	if ( type == ADDR_HEX )
	    printf("                dli      r7,0x%016llx 		\n",address);
	else
	    printf("                dla      r7,%s 		\n",addrLabel);
	printf("		ld	r8,(r7)			\n");
   if ( ContinueOnLLSCMiscompare) {
     if (!QuietLLSCMiscompare) {
	    printf("		beq	r8,r5,1f		\n");
	    printf("		or		a2, r0, r7		\n");
	    printf("		dla	a0,LLSCErrorMsg		\n");
	    printf("		dla	a3,iterationCount		\n");
	    printf("		lw	   a3,(a3)		\n");
	    printf("		jal	printf	\n");
	    printf("		dsubu a1, r5, r8				\n");	
	    printf("1: \n");	
	    printf("		dla	r5,saveReg		\n");
	    printf("		ld		v0, (r5)		\n");
	    printf("		ld		v1, 8(r5)		\n");
	    printf("		ld		a0, 16(r5)		\n");
	    printf("		ld		t1, 24(r5)		\n");
	    printf("		ld		t2, 32(r5)		\n");
	    printf("		ld		t3, 40(r5)		\n");
	    printf("		ld		t4, 48(r5)		\n");
	    printf("		ld		t5, 56(r5)		\n");
	    printf("		ld		t6, 64(r5)		\n");
	    printf("		ld		t7, 72(r5)		\n");
	    printf("		ld		t8, 80(r5)		\n");
	    printf("		ld		t9, 88(r5)		\n");
	    printf("		ld		r6, 96(r5)		\n");
	    printf("		ld		r28, 104(r5)		\n");
	    printf("		ld		r29, 112(r5)		\n");
	    printf("		ld		r1, 120(r5)		\n");
	    printf("		ld		r31, 128(r5)		\n");
	    if ( type == ADDR_HEX )
	      printf("                dli      r7,0x%016llx 		\n",address);
	    else
	      printf("                dla      r7,%s 		\n",addrLabel);
     }
   } else {
	  printf("		bne	r8,r5,fail		\n");
	  printf("		nop				\n");	
   }
	printf("		sd	r0,(r7)			\n");

	break;

    case CHECK_INC_OP:
    case CHECK_FETCH_INC_OP:

        printf("\n # check INC OP/ FETCH_INC OP at addr %016llx\n\n",address);
	printf("		li	r5,%lld		        \n",data);
	printf("                mult    r6,r5                   \n");
	printf("                mflo    r5                      \n");
	if ( type == ADDR_HEX )
	    printf("                dli      r7,0x%016llx 		\n",address);
	else
	    printf("                dla      r7,%s 		\n",addrLabel);
	printf("		ld	r8,(r7)			\n");
	printf("		bne	r8,r5,fail		\n");
	printf("		nop				\n");	
	printf("		sd	r0,(r7)			\n");

	break;
    
    case CHECK_DEC_OP:
    case CHECK_FETCH_DEC_OP:

        printf("\n # check DEC OP/ FETCH_DEC OP at addr %016llx\n\n",address);
	printf("		li	r5,%lld		        \n",data);
	printf("                mult    r6,r5                   \n");
	printf("                mflo    r5                      \n");
	printf("		li	r7,-1		        \n");
	printf("                mult    r7,r5                   \n");
	printf("                mflo    r5                      \n");
	if ( type == ADDR_HEX )
	    printf("                dli      r7,0x%016llx 		\n",address);
	else
	    printf("                dla      r7,%s 		\n",addrLabel);
	printf("		ld	r8,(r7)			\n");
	printf("		bne	r8,r5,fail		\n");
	printf("		nop				\n");	
	printf("		sd	r0,(r7)			\n");

	break;

    case CHECK_OR_OP:

        printf("\n # check OR OP at addr %016lld\n\n",address);
	if ( type == ADDR_HEX ) { 
	    printf("		dli     r1,0x%016llx \n",address);
	    printf("		dli	r2,0x%016llx \n",data);
	    printf("		li      r5,-64  # number of bits in test data\n");
	    printf("		mult    r6,r5 \n");
	    printf("		mflo    r3 \n");
	    printf("		li      r4,%d \n",numOfCpuG);
	    printf("		div     r3,r4 \n");
	    printf("		mflo    r5 \n");
	    printf("		addiu	r5,r5,64 \n");
	    printf("		dsrlv	r2,r2,r5 \n");
	    printf("		ld      r1,0(r1) \n");
	    printf("		bne	r2,r1,fail \n");
	    printf("		nop \n");
	}
	else {
	    printf("		dla     r1,%s\n",addrLabel);
	    printf("		dli	r2,0x%llx\n",data);
	    printf("		li      r5,-64  # number of bits in test data\n");
	    printf("		mult    r6,r5 \n");
	    printf("		mflo    r3 \n");
	    printf("		li      r4,%d \n",numOfCpuG);
	    printf("		div     r3,r4 \n");
	    printf("		mflo    r5 \n");
	    printf("		addiu	r5,r5,64 \n");
	    printf("		dsrlv	r2,r2,r5 \n");
	    printf("		ld      r1,0(r1) \n");
	    printf("		bne	r2,r1,fail \n");
	    printf("		nop \n");
	}
	break;

    case CHECK_AND_OP:

        printf("\n # check AND OP at addr %016llx\n\n",address);
	if ( type == ADDR_HEX ) {
	    printf("		dli     r1,0x%016llx\n",address);
	    printf("		dli	r2,0x%016llx\n",data);
	    printf("		li      r5,64  # number of bits in test data\n");
	    printf("		mult    r6,r5 \n");
	    printf("		mflo    r3 \n");
	    printf("		li      r4,%d \n",numOfCpuG);
	    printf("		div     r3,r4 \n");
	    printf("		mflo    r5 \n");
	    printf("		dsllv	r2,r2,r5 \n");
	    printf("		ld      r1,0(r1) \n");
	    printf("		bne	r2,r1,fail \n");
	    printf("		nop \n");
	}
	else {
	    printf("		dla     r1,%s\n",addrLabel);
	    printf("		dli	r2,0x%llx\n",data);
	    printf("		li      r5,64  # number of bits in test data\n");
	    printf("		mult    r6,r5 \n");
	    printf("		mflo    r3 \n");
	    printf("		li      r4,%d \n",numOfCpuG);
	    printf("		div     r3,r4 \n");
	    printf("		mflo    r5 \n");
	    printf("		dsllv	r2,r2,r5 \n");
	    printf("		ld      r1,0(r1) \n");
	    printf("		bne	r2,r1,fail \n");
	    printf("		nop \n");
	}
	break;


    }
 return (TRUE);
}

void sharedOp :: addToCpuWorkQueue ( int cpu, Work * request ) {
    if ( cpuWrk[cpu] == NULL ) {
	 cpuWrk[cpu] = request;
    }
    else{
	request->next  = cpuWrk[cpu];
	cpuWrk[cpu]    = request;
    }
}

//

void sharedOp :: addToCheckWorkQueue ( Work * request ) {
    if ( checkWrk == NULL ) {
	 checkWrk = request;
    }
    else{
	request->next  = checkWrk;
	checkWrk       = request;
    }
}

//

void sharedOp :: addToInitWorkQueue ( Work * request ) {
    if ( initWrk == NULL ) {
	 initWrk = request;
    }
    else{
	request->next  = initWrk;
	initWrk        = request;
    }
}

//

sharedOp :: sharedOp () {

    numCpu 	= 0;
    iterations  = 0;

    for ( int j = 0 ; j < MAX_NUM_PROCESSORS; j++ )
	cpuWrk[j] = NULL;

    checkWrk = NULL;
    initWrk  = NULL;
}

//

void sharedOp :: shuffle() {

    int i;
    int count=0;

    for ( i = 0 ; i < MAX_NUM_PROCESSORS ; i++ ) {
	if ( cpuWrk[i] != NULL ) {
	    for ( Work * tmpPtr = cpuWrk[i]; tmpPtr != NULL ; tmpPtr=tmpPtr->next )
		count++;
	}
    }


}
//

void sharedOp :: initL ( int operation, char * address , ULL data, int cpuID ) {

    addToCpuWorkQueue( cpuID, new Work( operation, address, data ) );
}

void sharedOp :: initL ( int operation, ULL  address , ULL data, int cpuID ) {

    addToCpuWorkQueue( cpuID, new Work( operation, address, data ) );
}

void sharedOp :: init ( int operation, ULL address , int count,  int maxNumCpu ) {

    numCpu     = maxNumCpu;
    iterations = count;

     if ( operation == FUNC_LLSC_INC )
       addToInitWorkQueue( new Work ( INIT_ADDRESS, address, (ULL) 0) );
    else
      addToInitWorkQueue( new Work ( FUNC_INIT_ADDRESS_OP, address, (ULL) 0) );

    for ( int cpu = 0 ; cpu < numCpu ; cpu++ ) {
	for ( int i = 0 ; i < count ; i++ ) {
	    addToCpuWorkQueue( cpu, new Work( operation, address,(ULL) i ) );
	}
    } 

    addToCheckWorkQueue(  new Work( CHECK_BIT | operation, address, (ULL) count  ) );
}

void sharedOp :: init ( int operation, char * address , int count,  int maxNumCpu ) {

    numCpu     = maxNumCpu;
    iterations = count;

    if ( operation == FUNC_LLSC_INC )
      addToInitWorkQueue( new Work ( INIT_ADDRESS, address, (ULL) 0) );
     else
      addToInitWorkQueue( new Work ( FUNC_INIT_ADDRESS_OP, address, (ULL) 0) );

    for ( int cpu = 0 ; cpu < numCpu ; cpu++ ) {
	for ( int i = 0 ; i < count ; i++ ) {
	    addToCpuWorkQueue( cpu, new Work( operation, address,(ULL) i ) );
	}
    } 

    addToCheckWorkQueue(  new Work( CHECK_BIT | operation, address, (ULL) count  ) );
}

//

Work * sharedOp :: popWorkQueue ( int cpuid  ) {
    if (  cpuWrk[cpuid] == NULL ) return ( NULL );

    Work * tmpWorkPtr = cpuWrk[cpuid];
    cpuWrk[cpuid] = (cpuWrk[cpuid])->next;
    return ( tmpWorkPtr);

}

//

void sharedOp :: prologue() {

    Work * wrkPtr;
    wrkPtr = initWrk;
   
    while ( wrkPtr != NULL ) {
	wrkPtr->execute();
	wrkPtr = wrkPtr->next;
    }

}

//

int sharedOp :: genCode ( int cpuId ) {

    if ( cpuWrk[cpuId] != NULL ) {
	Work * workToDo = popWorkQueue( cpuId );
	return (workToDo->execute() );
    }
    return ( FALSE );
}

//
void sharedOp :: epilogue() {

    Work * wrkPtr = checkWrk;

    while ( wrkPtr ) {
	wrkPtr->execute();
	wrkPtr = wrkPtr->next;
    }
}

//
sharedOp :: ~sharedOp () {
    Work * wrkPtr = checkWrk;
    Work * delPtr;

    while ( wrkPtr ) {
	delPtr = wrkPtr;
	wrkPtr = wrkPtr->next;
	delete delPtr;
    }
    checkWrk = NULL;

    wrkPtr = initWrk;

    while ( wrkPtr ) {
	delPtr = wrkPtr;
	wrkPtr = wrkPtr->next;
	delete delPtr;
    }
    initWrk = NULL;

    for ( int j = 0 ; j < MAX_NUM_PROCESSORS; j++ )
	delete cpuWrk[j];

}

//
void sharedOp :: randomizeWrkQueue() {
  //Work * tmp2;
  // Work * tmp3;

  //int count;

    //for ( tmpWrkPtr = cpuWrk; tmpWrkPtr != NULL; tmpWrkPtr =tmpWrkPtr->next )
    //count++;
    
    //if ( count > 3 ) {
    //for ( i = 0; i < count-1; i++ ) {
    //   tmp2 = cpuWrk;
    //   randInsertPoint = random()%(count-1);
    //   for ( tmp3 = cpuWrk ; randInsertPoint > 0 ; tmp3 = tmp3->next, randInsertPoint-- );
    //   tmp4 = tmp3->next;
	
	    
    //}
    //}
}

ULL physToXkuseg( ULL physAddr ) {
  // do 1->1 mapping, TLB will handle it
  return ( physAddr );
}
ULL physToXksseg( ULL physAddr ) {
  return ( physAddr | 0x4000000000000000LL);
}
ULL physToXkphys_uncached( ULL physAddr ) {
  return ( physAddr | 0x9600000000000000LL);
}
ULL physToXkphys_cached( ULL physAddr ) {
  return ( physAddr | 0xA800000000000000LL);
}
ULL physToCkseg0( ULL physAddr ) {
  return ( physAddr | 0xffffffff80000000LL );
}
ULL physToCkseg1( ULL physAddr ) {
  return ( physAddr | 0xffffffffA0000000LL);
}
ULL physToCkseg( ULL physAddr ) {
  return ( physAddr | 0xffffffffC0000000LL );
}
ULL physToCkseg3( ULL physAddr ) {
return (physAddr  | 0xffffffffE0000000LL );
}

//

Cell :: Cell ( ULL addr, ULL val, UINT prop ) {

  address  = addr;
  value    = val;
  property = prop;

  prev = next = NULL;
}

//

Address :: Address ( ULL vaddr, ULL paddr, UINT property ) {

  VAddr = vaddr;
  PAddr = paddr;
  Property = property;
}

//

Address :: Address () {

  VAddr = PAddr = Property = 0;
}

void Address :: init ( ULL vaddr ) {

 VAddr = vaddr;

 if ( (vaddr & UNMAP_CACHED_32) == UNMAP_CACHED_32 ) {
      PAddr =  VAddr  & UNMAP_CACHED_MASK_32;
      Property = UNMAPPED_ADDR | SPACE32_ADDR | CACHED_ADDR;
 }  
 else if ( ( VAddr & DIRECTMAP_UNC_MASK ) == DIRECTMAP_UNCACHED_A ||
           ( VAddr & DIRECTMAP_UNC_MASK ) == DIRECTMAP_UNCACHED_B ) {

   PAddr = VAddr & UNMAP_CACHED_MASK_32; 
   Property = UNMAPPED_ADDR | SPACE32_ADDR | UNCACHED_ADDR;
 }
 else 
   switch ( VAddr & DIRECTMAP_64_MASK ) {

    case DIRECTMAP_64_8:
    case DIRECTMAP_64_A:
    case DIRECTMAP_64_B:

      PAddr = VAddr &  DIRECTMAP_64_VA_PA;
      Property =  UNMAPPED_ADDR | SPACE64_ADDR | CACHED_ADDR;
      break;

    case DIRECTMAP_64_9:

      PAddr = VAddr &  DIRECTMAP_64_VA_PA;
      Property =  UNMAPPED_ADDR | SPACE64_ADDR | UNCACHED_ADDR ;
      break;

   default: 

     // OK so we give up. Lets use mapping 1-1 between VA and PA
     PAddr = VAddr;
     Property = UNMAPPED_ADDR | SPACE64_ADDR | CACHED_ADDR;
 }
}
  
//

Memory :: Memory () {

    topOfMemory  = NULL;

    bAddrCnt     = 0; 
    memReuseRate = 2;
    cellCount    = 0;


    memHasUncached = FALSE;
    memHasCached   = FALSE;

    for ( int i = 0 ; i < MAX_QUEUE; i++ )
	lastAddrQ[i] = 0;

    queueSize = 0;

// setup cache wraps

    cacheWrapAddr[0] = CACHE_WRAP1;
    cacheWrapAddr[1] = CACHE_WRAP2;
    cacheWrapAddr[2] = CACHE_WRAP3;
    cacheWrapAddr[3] = CACHE_WRAP4;
    cacheWrapAddr[4] = CACHE_WRAP5;
    cacheWrapAddr[5] = CACHE_WRAP6;
    cacheWrapAddr[6] = CACHE_WRAP7;
    cacheWrapAddr[7] = CACHE_WRAP8;

    maxCacheWrapVal = 0;
    for ( i = 0 ; i < NUM_CACHE_WRAP ; i++ ) {
      maxCacheWrapVal = ( cacheWrapAddr[i] > maxCacheWrapVal ) ? cacheWrapAddr[i] : maxCacheWrapVal;
    }

    nodeIDCnt    = 0;
   for ( i = 0 ; i < MAX_NODE_ID ; i++ )
     nodeID[i] = 0;
}

Cell * Memory :: getTopOfMemory() {
    return topOfMemory;
}
// allocated double word for memory system

Cell * Memory :: allocateCell( ULL addr, UINT addrMode ) {

  Cell  * tmpPtr;

  tmpPtr = new Cell(addr & DWORDMASK64,0,addrMode);

  if (tmpPtr == NULL) {
    cerr << "/* Run out of memory */\n";
    exit(EXIT_FAILURE);

  }

  if ( addrMode & UNCACHED_ADDR ) 
    memHasUncached = TRUE;
  else
    memHasCached = TRUE;

  cellCount++;

  return(tmpPtr);
};


// delete all cells allocated for Memory model

void Memory :: deAllocMem() {
	Cell * tmpPtr1 ;
        Cell * tmpPtr2 ;
	tmpPtr1 =  memoryModel.getTopOfMemory();
	while ( tmpPtr1 != NULL ) {
		tmpPtr2 = tmpPtr1;
		tmpPtr1 = tmpPtr1->next;
		delete tmpPtr2;
	}
       
	memHasUncached = FALSE;
	memHasCached   = FALSE;

	cellCount = 0;
	topOfMemory = NULL;
}

// search for a double work location, lower 3 bits are masked off before search

Cell * Memory :: locateCell( ULL addr) {
    ULL	dwordAddr;
    Cell * tmpPtr;

    if ( topOfMemory == NULL )
	return(NULL);
    else
	{
	    // calculate double word address, removing lower byte selects
	    dwordAddr = addr & DWORDMASK64;

	    // otherwise look for address 
	    for ( tmpPtr = topOfMemory ; tmpPtr !=  NULL ; tmpPtr = tmpPtr->next ) {
		//ok we found the word 
		if ( dwordAddr == tmpPtr->address )
		    return(tmpPtr);
		}
		return(NULL);
	}
}

//
// this routine assumes we have never placed this cell before.
Cell * Memory :: placeCell ( ULL addr, UINT addrMode ) {

    ULL	dwordAddr;
    Cell * cellPtr;
    Cell * currentPtr;

    if ( doAtomicG == 0 ) printf("\n/*inserting Address: %08x */\n", addr);
    // if memory database empty 
    if ( topOfMemory == NULL ) {
	topOfMemory       = allocateCell(addr,addrMode);
	topOfMemory->prev = topOfMemory;
	topOfMemory->next = NULL;
	return(topOfMemory);
    } // else if there is only 1 element in database 
    else if ( topOfMemory->prev == topOfMemory && topOfMemory->next == NULL ) {
	// remove lower 3 bits  
	dwordAddr = addr & DWORDMASK64;

	if ( dwordAddr <=  topOfMemory->address ) {
	    // then this cell needs to be first 
	    cellPtr           = allocateCell(addr,addrMode);
	    cellPtr->next     = topOfMemory;
	    topOfMemory->prev = cellPtr;
	    topOfMemory       = cellPtr;
	    cellPtr->prev     = topOfMemory;
	    return(cellPtr);
	} else
	    {   
		cellPtr           = allocateCell(addr,addrMode);
		cellPtr->prev     = topOfMemory;
		topOfMemory->next = cellPtr;
		return(cellPtr);
		}
    } // else general case 
    else
	{
	    // calculate double word address 
	    dwordAddr = addr & DWORDMASK64;

	    // otherwise look for address 
	    for ( currentPtr = topOfMemory ; currentPtr->next !=  NULL ; currentPtr = currentPtr->next ) {
		// ok we found the word 
		if ( dwordAddr <= currentPtr->address ) {
		    if ( currentPtr == topOfMemory ) {
			cellPtr = allocateCell(addr,addrMode);
			cellPtr->next = topOfMemory;
			cellPtr->prev = cellPtr;
			topOfMemory->prev = cellPtr;
			topOfMemory = cellPtr ;
			return(topOfMemory);
		    } else
			{
			    cellPtr = allocateCell(addr,addrMode);
			    currentPtr->prev->next = cellPtr;
			    cellPtr->prev = currentPtr->prev;
			    currentPtr->prev = cellPtr;
			    cellPtr->next = currentPtr;
			    return(cellPtr);
			}
		}
	    }
	    // this address is largest of bunch 
	    if ( dwordAddr <=  currentPtr->address ) {
	      cellPtr = allocateCell(addr,addrMode);
	      currentPtr->prev->next = cellPtr;
	      cellPtr->prev = currentPtr->prev;
	      currentPtr->prev = cellPtr;
	      cellPtr->next = currentPtr;
	      return(cellPtr);
	    } else
	      {
		cellPtr = allocateCell(addr,addrMode);
		currentPtr->next = cellPtr;
		cellPtr->prev = currentPtr;
		return(cellPtr);
	      }
	}
}

//

void Memory :: printMemory() {
    register Cell * currentPtr;

    printf("\n\n/* Mem dump:\n");
    for ( currentPtr = memoryModel.getTopOfMemory() ; currentPtr !=  NULL ; currentPtr = currentPtr->next )
	printf("Address: %016llx Value:%016llx \n", currentPtr->address, currentPtr->value);
    printf("*/\n");
}

//

void Memory :: addNodeID ( ULL nodeid ) {
 
  nodeID[nodeIDCnt++] = nodeid;

}

//

void Memory :: addBaseAddr( ULL addr ) {

  baseLdStAddrG[bAddrCnt++].init(addr);

}

//

void Memory :: addUncBaseAddr( ULL addr ) {

  baseUncLdStAddrG[bUncAddrCnt++].init(addr);
 
}

//

void  Memory :: computeAllBases() {

  // we need a place to compute all the net effect of the command line
  // options relating to base addresses (cached / uncached / sn0 mode )

  // if user has specified uncached base addr, then we dont have to do
  // anything, else we set the bases relative to the cached segements
  // if # uncached base addr < # cached base adder, we use our standard
  // algorithm to set the uncached addresses which are missing.

  // zero our cache wrap addresses also, just incase we want to 
  // use the registers.

  if ( cacheWrapG == FALSE ) {
    for ( int j = 0 ; j < NUM_CACHE_WRAP ; j++ ) {
      cacheWrapAddr[j] = 0;
    }
  }

  if ( bUncAddrCnt == 0 ) {
    for ( int i = 0 ; i < bAddrCnt ; i++ ) {
      baseUncLdStAddrG[i].PAddr    = ((baseLdStAddrG[i].PAddr & 0x00ffffffffffffffLL) + addrRange + MAX_SCACHE_SIZE );
      baseUncLdStAddrG[i].Property = (baseLdStAddrG[i].Property & (~CACHE_MASK)) |  UNCACHED_ADDR;
    }
    bUncAddrCnt = bAddrCnt;
  }  
  else if ( bUncAddrCnt < bAddrCnt ) {
    for (int j = bUncAddrCnt; j <  bAddrCnt ; j++ ) {

     baseUncLdStAddrG[j].PAddr     = baseUncLdStAddrG[bUncAddrCnt-1].PAddr;
     baseUncLdStAddrG[j].Property  = baseUncLdStAddrG[bUncAddrCnt-1].Property;
    }
    bUncAddrCnt =  bAddrCnt;
  }

  // now we take in account SN0 node ID issues
  // if user gave a node ID count.

  // nodeID[] are all 0 by default so no effect on base adddresses
  if ( nodeIDCnt == 0 ) 
    nodeIDCnt = bAddrCnt;
  else if ( nodeIDCnt < bAddrCnt ) {
       bAddrCnt = bUncAddrCnt = nodeIDCnt;
  }

  if ( nodeIDCnt > bAddrCnt ) {
    for (int j = bAddrCnt; j <  nodeIDCnt ; j++ ) {

      baseUncLdStAddrG[j].VAddr    = baseUncLdStAddrG[bUncAddrCnt-1].VAddr; 
      baseUncLdStAddrG[j].PAddr    = baseUncLdStAddrG[bUncAddrCnt-1].PAddr; 
      baseUncLdStAddrG[j].Property = baseUncLdStAddrG[bUncAddrCnt-1].Property; 

      baseLdStAddrG[j].VAddr     = baseLdStAddrG[bAddrCnt-1].VAddr;  
      baseLdStAddrG[j].PAddr     = baseLdStAddrG[bAddrCnt-1].PAddr;  
      baseLdStAddrG[j].Property  = baseLdStAddrG[bAddrCnt-1].Property;  
    }

    bAddrCnt = bUncAddrCnt = nodeIDCnt;
  }

   // now adjust base addresses based on node ID's
  
  if ( doSN0M == TRUE ) {
       //  bAddrCnt = bUncAddrCnt = nodeIDCnt
    for ( int i = 0 ; i < bAddrCnt ; i++ ) {
     baseLdStAddrG[i].PAddr    = ( baseLdStAddrG[i].PAddr    & 0xffffff007fffffffLL ) | ( nodeID[i] << 32 );
     baseUncLdStAddrG[i].PAddr = ( baseUncLdStAddrG[i].PAddr & 0xffffff007fffffffLL ) | ( nodeID[i] << 32 );
    }
 }
 else if (doSN0N == TRUE ) {
    for ( int i = 0 ; i < bAddrCnt ; i++ ) {
     baseLdStAddrG[i].PAddr    = ( baseLdStAddrG[i].PAddr    & 0xffffff00ffffffffLL ) | ( nodeID[i] << 31 );
     baseUncLdStAddrG[i].PAddr = ( baseUncLdStAddrG[i].PAddr & 0xffffff00ffffffffLL ) | ( nodeID[i] << 31 );
    }
 }

}
//
// checks for overlap between uncached and cached areas

int checkForOverlap () {
 return(0);
}

//

ULL Memory :: mapPhysAddr ( ULL physAddr, UINT hint ) {

  if ( (hint & MAPPED_ADDR_MASK) == MAPPED_ADDR ) {

	    return( physToXkuseg( physAddr ));
  }
  else if ( (hint & SPACE_ADDR_MASK) == SPACE32_ADDR ) {

    if ( (hint & CACHE_MASK) == UNCACHED_ADDR ) 
      return ( physToCkseg1( physAddr ));
    else
      return ( physToCkseg0( physAddr ));
  }
  else {
    if ( (hint & CACHE_MASK) == UNCACHED_ADDR )
      return ( physToXkphys_uncached( physAddr ));
    else
      return( physToXkphys_cached( physAddr )); 
  }

}

void  Memory :: usedAddr ( ULL physAddr ) {

    for ( int i =  MAX_QUEUE-1 ; i > 0 ; i-- ) 
	lastAddrQ[i+1] = lastAddrQ[i];
    lastAddrQ[0] = physAddr;
    if ( queueSize < MAX_QUEUE ) queueSize++;
}

// 

void Memory :: setCacheWrapAddr( int index, ULL offset ) {

    cacheWrapAddr[index] = offset;
    maxCacheWrapVal = ( cacheWrapAddr[index] >  maxCacheWrapVal ) ? cacheWrapAddr[index] : maxCacheWrapVal;
}

//

ULL Memory :: pickPhysAddr( int cpuID, uint&  hint ) {

    int i;
    int index = 0;
    int addrSelected = FALSE;

    ULL selectedAddr = 0xdeadbeafdeadbeaf;

    // if we are supposed to reuse memeory
    // before we use recycled memory, we have to make sure that
    // a locate of the type we want exists. Otherwise we have
    // to allocation a new location.

    if ( topOfMemory != NULL && (hint & USED_MEM)  && 
	 (( !(hint & UNCACHED_ADDR) && (memHasCached == TRUE)) || 
            ((hint & UNCACHED_ADDR) && (memHasUncached == TRUE))) ) {

      Cell * currentPtr; 
      UINT searchHint = hint & CACHE_MASK ;
      int matchedIndex = 0;

           // if user requested uncached, we better give it
           // allocate enough cell pointers for result of search

      Cell ** matchedCells = ( Cell **) malloc ( (cellCount + 1 ) * sizeof(Cell*) );

      for (currentPtr = topOfMemory, i = 0 ; i < cellCount ; i++, currentPtr = currentPtr->next )
	if ( (currentPtr->property & CACHE_MASK) == searchHint ) 
	  matchedCells[matchedIndex++] = currentPtr;

           // if only one entry found, return it
      if ( matchedIndex  == 1 ) 
	currentPtr = matchedCells[0];
      else // otherwise randomly pick one
	currentPtr = matchedCells[random()%matchedIndex];

      selectedAddr = currentPtr->address;
      hint =  (hint & (~SPACE_ADDR_MASK)) |  (currentPtr->property & SPACE_ADDR_MASK);
      free(matchedCells);
      return (selectedAddr);
    }

       // otherwise select new address from range

    else { 

      // first compute index of base loadstore address

      if ( doSN0M == FALSE && doSN0N == FALSE ) {

	// check to see if multiple base addresses are being used

	if ( bAddrCnt > 1 )
	  index = random()%bAddrCnt;
 	else 
	  index = 0;
      }
      else { // sn0 mode

	while ( addrSelected == FALSE ) {
	    if ( bAddrCnt <= 1 ) { // if only one base address, just use it
		index = 0;
		addrSelected = TRUE;
	    }
	    else {
		int colIndex = random()%bAddrCnt;
	        if ( mpMapTable[cpuID][colIndex] == 1 ) {
		    index = colIndex;
		    addrSelected = TRUE;
		}
		else if ( mpMapTable[cpuID][colIndex] > 1 ) {
		    if ( (random()%(mpMapTable[cpuID][colIndex])) == 0 ) {
			index = colIndex;
			addrSelected = TRUE;
		    }
		}
	    }
	}
      } // sn0 mode

      // OK now we have selected a base address, lets get the 
      // physical address and also pass on its attributes
         // first get the base right
      if ( hint & UNCACHED_ADDR ) {
	selectedAddr = baseUncLdStAddrG[index].PAddr;
	hint = (hint & (~SPACE_ADDR_MASK)) | (baseUncLdStAddrG[index].Property & SPACE_ADDR_MASK);
      }
      else {
	selectedAddr = baseLdStAddrG[index].PAddr;
	hint = (hint & (~SPACE_ADDR_MASK)) | (baseLdStAddrG[index].Property & SPACE_ADDR_MASK);
      }
    

      // now handle cache wrap (we allow it for uncached also)

      if ( hint & CACHE_WRAP )
	selectedAddr += cacheWrapAddr[random()%NUM_CACHE_WRAP];

      if ( hint & SEL_OFFSET ) 
	selectedAddr += (ULL)(random()% (addrRange + 1));

      return (selectedAddr);
    } // end of new memory vs recycled

    // so that compiler does not nag

    return (selectedAddr);
}

void Memory :: setSN0Mode( int numCpus, int shiftAmt ) {

// this assumes we have already loaded  baseLdSt array 
// auto fills baseLdSt phys array   

  for ( int i = 1 ; i < (numCpus/2) ; i++ ) {

    baseLdStAddrG[i].PAddr    = baseLdStAddrG[0].PAddr + ((ULL) i << shiftAmt);
    baseLdStAddrG[i].VAddr    = baseLdStAddrG[0].VAddr;
    baseLdStAddrG[i].Property = baseLdStAddrG[0].Property;

  }

  bAddrCnt = numCpus/2;
}

void Memory :: setAddrRange( UINT range ) {
    addrRange = range;
    if ( addrRange > MAX_RANGE ) addrRange =  MAX_RANGE ;
    if ( doMP == TRUE) {
	if ( addrRange < MIN_MP_RANGE ) addrRange = MIN_MP_RANGE;
	else if (addrRange < MIN_RANGE) addrRange = MIN_RANGE;
    }

}

UINT Memory :: getAddrRange() {
  return ( addrRange);
}

void Memory :: zeroTouchedMem () {

 int i,j,k;
 int duplicate;
  // this code should only be executed by Master CPU 

/*
  for ( k = 0 ; k < bAddrCnt ; k++ ) {
	  
    duplicate = FALSE;

    for ( i = 0 ; i < k ; i++ ) {
      if ( doSN0M == TRUE ) {
	if ( (baseLdStAddrG[k].VAddr & ~SN0_M_MASK) == (baseLdStAddrG[i].VAddr & ~SN0_M_MASK) ) 
	  duplicate = TRUE;
      }
      else if ( doSN0N == TRUE ) { 
	if ( (baseLdStAddrG[k].VAddr & ~SN0_N_MASK) == (baseLdStAddrG[i].VAddr & ~SN0_N_MASK) ) 
	  duplicate = TRUE;
      }
      else if ( (baseLdStAddrG[k].VAddr == baseLdStAddrG[i].VAddr) && (i != k) ) 
	duplicate = TRUE;
    }

    if ( duplicate == TRUE ) continue;

    // normal read write location

    printf("\n\t # clear out normal areas\n\n");
    printf("\n\tdli r2,0x%016llx \t # start addr\n",(baseLdStAddrG[k].VAddr));

    printf("\tdaddiu r3,r2,0x%x \t # max value\n",addrRange+16); // 16 added due to offset addition
    printf("\tli r4,8 \t\t # inc rate\n");
    printf("1:\n");
    printf("\tsd  r0,0(r2) \t # zero out location\n");
    printf("\tblt r2,r3,1b\n");
    printf("\tdaddu r2,r2,r4\n");
    printf("\n\t ###### clear out cache wrap areas\n\n");

    // cache wrap locations

    if ( cacheWrapG == TRUE ) 
    for ( j = 0 ; j < NUM_CACHE_WRAP ; j++ ) {

      printf("\n\tdli r2,0x%016llx \t # start addr\n",(baseLdStAddrG[k].VAddr)+ cacheWrapAddr[j]);
  
      printf("\tdaddiu r3,r2,0x%x \t\t # max value\n",addrRange+16);
      printf("\tli r4,8 \t\t # inc rate\n");
      printf("1:\n");
      printf("\tsd  r0,0(r2) \t\t # zero out location\n");
      printf("\tblt r2,r3,1b\n");
      printf("\tdaddu r2,r2,r4\n");

    }
 }

 for ( k = 0 ; k < bUncAddrCnt ; k++ ) {

    duplicate = FALSE;

	  for ( i = 0 ; i < k ; i++ ) {
	    if ( doSN0M == TRUE ) {
	      if ( (baseUncLdStAddrG[k].PAddr & ~SN0_M_MASK) == (baseUncLdStAddrG[i].PAddr & ~SN0_M_MASK)) 
		duplicate = TRUE;
	    }
	    else if ( doSN0N == TRUE ) { 
	      if ( (baseUncLdStAddrG[k].PAddr & ~SN0_N_MASK) == (baseUncLdStAddrG[i].PAddr & ~SN0_N_MASK)) 
		duplicate = TRUE;
	    }
	    else if ( baseUncLdStAddrG[k].PAddr == baseUncLdStAddrG[i].PAddr ) 
		duplicate = TRUE;
	  }

	  if ( duplicate == TRUE ) continue;

    printf("\n\t ##### clear out uncached  areas\n\n");
      if ( doSN0N == TRUE )
	printf("\n\tdli r2,0x%016llx \t # start addr\n",mapPhysAddr((baseUncLdStAddrG[k].PAddr & ~SN0_N_MASK),SPACE64_ADDR|UNCACHED_ADDR));
      else if( doSN0M == TRUE ) 	      
	printf("\b\tdli r2,0x%016llx \t # start addr\n",mapPhysAddr((baseUncLdStAddrG[k].PAddr & ~SN0_M_MASK),SPACE64_ADDR|UNCACHED_ADDR));
	    else
	      printf("\tdli r2,0x%016llx \t # start addr\n",mapPhysAddr((baseUncLdStAddrG[k].PAddr ),SPACE32_ADDR|UNCACHED_ADDR));
    

    printf("\tdaddiu r3,r2,0x%x \t # max value\n",addrRange+16);
    printf("\tli r4,8 \t\t # inc rate\n");
    printf("1:\n");
    printf("\tsd  r0,0(r2) \t\t # zero out location\n");
    printf("\tblt r2,r3,1b\n");
    printf("\tdaddu r2,r2,r4\n");
    
    // cache wrap locations 

    if ( cacheWrapG == TRUE )
    for ( j = 0 ; j < NUM_CACHE_WRAP ; j++ ) {

      if ( doSN0N == TRUE )
	printf("\n\tdli r2,0x%016llx \t # start addr\n",mapPhysAddr((baseUncLdStAddrG[k].PAddr & ~SN0_N_MASK)+ cacheWrapAddr[j],SPACE64_ADDR|UNCACHED_ADDR));
      else if( doSN0M == TRUE ) 	      
	printf("\n\tdli r2,0x%016llx \t # start addr\n",mapPhysAddr((baseUncLdStAddrG[k].PAddr & ~SN0_M_MASK)+ cacheWrapAddr[j],SPACE64_ADDR|UNCACHED_ADDR));
      else
	printf("\n\tdli r2,0x%016llx \t # start addr\n",mapPhysAddr((baseUncLdStAddrG[k].PAddr )+ cacheWrapAddr[j],SPACE32_ADDR|UNCACHED_ADDR));

      printf("\tdaddiu r3,r2,0x%x \t\t # max value\n",addrRange+16);
      printf("\tli r4,8 \t\t # inc rate\n");
      printf("1:\n");
      printf("\tsd  r0,0(r2) \t\t # zero out location\n");
      printf("\tblt r2,r3,1b\n");
      printf("\tdaddu r2,r2,r4\n");

    }
  }

*/

 // and now SN0 fetch and op areas

 if ( doFetchOps == TRUE ) {
   printf("\n\t # clear out fetchOP areas\n\n");
   printf("\tdli  r2,0x%016llx \n",sharedSN0AddrG);

   for ( i = 0 ; i < numLocksG*8 ; i++ ) {
     printf("\tsd r0,0x%lx(r2) \n",i * 64 );
   }
 }


 printf("\n\t # clear out shared lock  areas\n\n");
 if ( doMP == TRUE ) {
   printf("\tdli  r2,0x%016llx \n",lockBase64G);
   for ( i = 0 ; i < numLocksG ; i++ )
     printf("\tsd r0,0x%lx(r2) \n",i * lockAddrStepG);
 }

 // clear barrier area

 if ( doBarrierG == TRUE ) {

   printf("	dla      r2,barrierLock\n");
   printf("	li      r3,0x%08x \n",(maxNumBarrier + 2) *8);
   printf("	addu    r3,r3,0x20              # cpuID 1 only\n");
   printf("	andi    r3,r3,0xfff8            # cpuID 1 only\n");
   printf("	daddu    r3,r2,r3 # upper limit of zeroing memory \n");
   printf("1:	sd      r0,0(r2)                # cpuID 1 only\n");
   printf("	daddu    r2,r2,8                # cpuID 1 only \n");
   printf("	bne     r2,r3,1b                # cpuID 1 only\n");
   printf("   nop \n");

 } // barrier

}

//
void Memory :: dumpMemUsageFile () {

    int i,j,k;
    int duplicate = FALSE;
	memFile=fopen(MEM_INIT_FILE,"w");


	for ( k = 0 ; k < bAddrCnt ; k++ ) {

          duplicate = FALSE;

	  for ( i = 0 ; i < k ; i++ ) {
	    if ( doSN0M == TRUE ) {
	      if ( (baseLdStAddrG[k].PAddr & ~SN0_M_MASK) == (baseLdStAddrG[i].PAddr & ~SN0_M_MASK) ) 
		duplicate = TRUE;
	    }
	    else if ( doSN0N == TRUE ) { 
	      if ( (baseLdStAddrG[k].PAddr & ~SN0_N_MASK) == (baseLdStAddrG[i].PAddr & ~SN0_N_MASK) ) 
		duplicate = TRUE;
	    }
	    else if ( baseLdStAddrG[k].PAddr == baseLdStAddrG[i].PAddr ) 
		duplicate = TRUE;
	  }

	  if ( duplicate == TRUE ) continue;

	    /* normal read write location */
	  for ( i = 0 ; i <= addrRange ; i = i + 8 ) {
	    if ( doSN0N == TRUE )
	      fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseLdStAddrG[k].PAddr & ~SN0_N_MASK) + i );
	    else if( doSN0M == TRUE ) 
	      fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseLdStAddrG[k].PAddr & ~SN0_M_MASK) + i );
	    else
	      fprintf(memFile,"0x%016llx 0x0000000000000000 \n",baseLdStAddrG[k].PAddr  + i );
	  }
	
	    /* cache wrap locations */
	    for ( j = 0 ; j < NUM_CACHE_WRAP ; j++ )
	      for ( i = 0 ; i <= addrRange ; i+=8 ) {

		if ( doSN0N == TRUE ) 
		    fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseLdStAddrG[k].PAddr & ~SN0_N_MASK) + cacheWrapAddr[j] + i);
		else if( doSN0M == TRUE )
		    fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseLdStAddrG[k].PAddr & ~SN0_M_MASK) + cacheWrapAddr[j] + i);
		else
		    fprintf(memFile,"0x%016llx 0x0000000000000000 \n",baseLdStAddrG[k].PAddr + cacheWrapAddr[j] + i);
	      }
	   
 // and now SN0 fetch and op areas
	    if ( doFetchOps == TRUE ) {

	      for ( i = 0 ; i < numLocksG*8 ; i++ ) {
		fprintf(memFile,"0x%016llx 0x0000000000000000 \n", (sharedSN0AddrG + (ULL) (i * 64)) & (ULL) CACHED64_TO_PHYSICAL);		
	      }
	    }
	    if ( doMP == TRUE ) {
		for ( i = 0 ; i < numLocksG ; i++ )
		    fprintf(memFile,"0x%016llx 0x0000000000000000 \n", (lockBase64G + (ULL) (i * lockAddrStepG)) & (ULL) CACHED_TO_PHYSICAL);
	    }

	    if ( doBurstG == TRUE ) {


	      ULL burstMaxOffset =  (4* MAX_SCACHE_SIZE) +  2*(burstMaxG * burstDeltaG);
	      ULL burstMinOffset = memoryModel.maxCacheWrapVal + (2 * memoryModel.addrRange) + (2* MAX_SCACHE_SIZE);


	      for ( i = 0 ; i <= burstMaxOffset  ; i = i + 8 ) {

		if ( doSN0N == TRUE )
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseLdStAddrG[k].PAddr & ~SN0_N_MASK) + burstMinOffset + i );
		else if( doSN0M == TRUE ) 
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseLdStAddrG[k].PAddr & ~SN0_M_MASK) + burstMinOffset + i );
		else
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",baseLdStAddrG[k].PAddr  + burstMinOffset + i );
	      }
	    }
	}

	for ( k = 0 ; k < bUncAddrCnt ; k ++ ) {

          duplicate = FALSE;

	  for ( i = 0 ; i < k ; i++ ) {
	    if ( doSN0M == TRUE ) {
	      if ( (baseUncLdStAddrG[k].PAddr & ~SN0_M_MASK) == (baseUncLdStAddrG[i].PAddr & ~SN0_M_MASK)) 
		duplicate = TRUE;
	    }
	    else if ( doSN0N == TRUE ) { 
	      if ( (baseUncLdStAddrG[k].PAddr & ~SN0_N_MASK) == (baseUncLdStAddrG[i].PAddr & ~SN0_N_MASK)) 
		duplicate = TRUE;
	    }
	    else if ( baseUncLdStAddrG[k].PAddr == baseUncLdStAddrG[i].PAddr ) 
		duplicate = TRUE;
	  }

	  if ( duplicate == TRUE ) continue;

	  /* normal read write location */
	  for ( i = 0 ; i <= addrRange ; i = i + 8 ) {
	    if ( doSN0N == TRUE ) 
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~(SN0_N_MASK | UNCACH64_TO_PHYS_MASK) ) + i );
	    else if( doSN0M == TRUE )
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~(SN0_M_MASK | UNCACH64_TO_PHYS_MASK) ) + i );
	      else
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~UNCACH64_TO_PHYS_MASK) + i );
	  }
	    /* cache wrap locations */
	    for ( j = 0 ; j < NUM_CACHE_WRAP ; j++ )
	      for ( i = 0 ; i <= addrRange ; i+=8 ) {
		if ( doSN0N == TRUE ) 
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~(SN0_N_MASK | UNCACH64_TO_PHYS_MASK)) + cacheWrapAddr[j] + i);
		else if( doSN0M == TRUE )
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~(SN0_M_MASK | UNCACH64_TO_PHYS_MASK)) + cacheWrapAddr[j] + i);
		else
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~UNCACH64_TO_PHYS_MASK) + cacheWrapAddr[j] + i);
	      }

	    if ( doBurstG == TRUE ) {

	      for ( i = 0 ; i <= (burstMaxG * burstDeltaG) ; i = i + 8 ) {

		ULL burstOffset = memoryModel.maxCacheWrapVal + (2 * memoryModel.addrRange) + (4* MAX_SCACHE_SIZE) +  (burstMaxG * burstDeltaG);

		if ( doSN0N == TRUE )
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~(SN0_N_MASK | UNCACH64_TO_PHYS_MASK)) + burstOffset + i );
		else if ( doSN0M == TRUE ) 
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~(SN0_M_MASK | UNCACH64_TO_PHYS_MASK)) + burstOffset + i );
		else
		  fprintf(memFile,"0x%016llx 0x0000000000000000 \n",(baseUncLdStAddrG[k].PAddr & ~UNCACH64_TO_PHYS_MASK) + burstOffset + i );
	      }
	    }
	}
	fclose(memFile);
}
int parseMpTable( char * mpFileNameG ) {

FILE * mpFileHandle;
char   mpLine[256];
char * mpLinePtr;

int cpuIndex = 0;
int memIndex = 0;
int i,j;

	if ( *mpFileNameG != NULL ) {
	    if ( (mpFileHandle = fopen(mpFileNameG,"r")) == NULL ) {
		printf("/* FILE ERROR: could not open %s */ \n",mpFileNameG);
	    }
	    printf("/* MP mapping table \n");
	    while ( (mpLinePtr = fgets(mpLine,256,mpFileHandle)) != NULL ) {
		printf("%s",mpLinePtr);

		while ( *mpLinePtr != NULL && memIndex < MAX_CPU_MP && cpuIndex < MAX_CPU_MP ) {
		    if ( isspace(*mpLinePtr) ) mpLinePtr++;
		    else if ( *mpLinePtr >= '0' && *mpLinePtr <= '9' ) {
			mpMapTable[cpuIndex][memIndex++] = (*mpLinePtr)- '0';
			mpLinePtr++;
		    }
		    else {
			printf("ERROR: syntax error in mpMap file \n");
			exit(EXIT_FAILURE);
		    }
		}
		cpuIndex++;
		memIndex = 0;
	    }
	    printf("Table is : \n");
	    for ( i = 0; i < MAX_CPU_MP ; i++ ) {
		for ( j = 0 ; j < MAX_CPU_MP ; j++ ) 
		    printf(" %d",mpMapTable[i][j]);

		printf("\n");
	    }
	    printf("*/ \n");
	}
 return ( 0 );
}
char *
generateCheckPrefSyncLabel()
{
    static char	buf[128];

    sprintf(buf, "CheckPrefSyncLabel_%d", nextCheckPrefSyncLabel--);
    return buf;
}

char *
generateIssuePrefSyncLabel()
{
    static char	buf[128];

    sprintf(buf, "IssuePrefSyncLabel_%d", nextIssuePrefSyncLabel++);
    return buf;
}
    
void
logPrefetch(UINT vaddrHi, UINT vaddrLo, UINT paddrHi, UINT paddrLo, int hint)
{
    if (checkPrefSyncG != TRUE)
	return;

    if (nextPrefSyncInfoG >= PREFSYNC_MAX_COUNT)  {
	fprintf(stderr, "**** Fatal:  Too many prefetches/syncs logged\n");
	exit(EXIT_FAILURE);
    }

    prefSyncInfoG[nextPrefSyncInfoG].type = PREFSYNC_TYPE_PREF;
    prefSyncInfoG[nextPrefSyncInfoG].vaddrHi = vaddrHi;
    prefSyncInfoG[nextPrefSyncInfoG].vaddrLo = vaddrLo;
    prefSyncInfoG[nextPrefSyncInfoG].paddrHi = paddrHi;
    prefSyncInfoG[nextPrefSyncInfoG].paddrLo = paddrLo;
    prefSyncInfoG[nextPrefSyncInfoG].hint = hint;

    nextPrefSyncInfoG++;
}

void
logSync()
{
    if (checkPrefSyncG != TRUE)
	return;

    if (nextPrefSyncInfoG >= PREFSYNC_MAX_COUNT)  {
	fprintf(stderr, "**** Fatal:  Too many prefetches/syncs logged\n");
	exit(EXIT_FAILURE);
    }

    prefSyncInfoG[nextPrefSyncInfoG].type = PREFSYNC_TYPE_SYNC;
    prefSyncInfoG[nextPrefSyncInfoG].vaddrHi = 0;
    prefSyncInfoG[nextPrefSyncInfoG].vaddrLo = 0;
    prefSyncInfoG[nextPrefSyncInfoG].paddrHi = 0;
    prefSyncInfoG[nextPrefSyncInfoG].paddrLo = 0;
    prefSyncInfoG[nextPrefSyncInfoG].hint = 0;

    nextPrefSyncInfoG++;
}

void
saveRegs()
{
    printf("\t dmtc1	r16, fp10\n");
    printf("\t dmtc1	r17, fp11\n");
    printf("\t dmtc1	r18, fp12\n");
    printf("\t dmtc1	r19, fp13\n");
}

void
restoreRegs()
{
    printf("\t dmfc1	r16, fp10\n");
    printf("\t dmfc1	r17, fp11\n");
    printf("\t dmfc1	r18, fp12\n");
    printf("\t dmfc1	r19, fp13\n");
}

void
generateSyncCheckCode(int idx)
{
    printf("%s:\n", generateCheckPrefSyncLabel());
    printf("\t dmtc1	r17, fp11\n");
    printf("\t dli	r17, 0x9000000018FFFFF0	# TBUS event stack pop address\n");
    printf("\t ld	r17, 0(r17)		# Get top of TBUS event stack\n");
    printf("\t dsrl32	r17, r17, (56 - 32)\n");
    printf("\t and	r17, r17, 0xF\n");
    printf("\t beq	r17, 0xA, 200f\n");
    printf("\t nop\n");
    printf("\t halt(FAIL)\n");
    printf("200:\n");
    printf("\t dmfc1	r17, fp11\n");
}

void
computePhysicalAddress(int idx)
/* Walks the page table and puts the low 32 bits of the physical address	*/
/* corresponding to the virtual address in prefSyncInfoG[idx] into r16.		*/
/* Assumes each PTE is 8 bytes with the lower 20 bits of the pfn in the upper	*/
/* 20 bits of the second word of the PTE.					*/
/* Assumes a page size of 4KB.							*/
{
    if (prefSyncInfoG[idx].vaddrHi & 0x80000000)  {
	/* If VA < 0, then it is in unmapped space and so PA = VA */
	printf("\t dli	r16, 0x%08x\n",
	       prefSyncInfoG[idx].vaddrLo);
	printf("j	200f\n");
    }
    else  {
	printf("\t li		r17, 0x%08x		# VA[11:0]\n",
	       prefSyncInfoG[idx].vaddrLo & 0xFFF);
	printf("\t li		r18, 0x%08x		# VPN * (PTE size)\n",
	       (prefSyncInfoG[idx].vaddrLo >> 9) & 0x7FFFF8);
	/* Fetch the pfn from the page table */
	printf("\t dmfc0	r19, C0_UBASE\n");
	printf("\t dadd		r19, r19, r18\n");
	printf("\t lwu		r19, 4(r19)\n");
	printf("\t and		r19, r19, 0xFFFFF000\n");
	printf("\t or		r16, r17, r19\n");
    }
    printf("200:\n");
}

void
generatePrefetchCheckCode(int idx)
/* NOTE:  This code will not work for physical addresses which exceed 32 bits */
{
    UINT	tbusHi, tbusLo;
    UINT	tbusHiMask;
    UINT	pAddrHi, virtSyn, state, set, match, coh, hint, command;

    pAddrHi = 0;
    virtSyn = (prefSyncInfoG[idx].vaddrLo & 0xF000) >> 12;
    state = 0;
    set = 0;
    match = 0;
    coh = 0;
    hint = prefSyncInfoG[idx].hint & 0x7;
    command = 8;

    tbusLo = prefSyncInfoG[idx].paddrLo;
    tbusHi = (command << 24) |
	     (hint    << 21) |
	     (coh     << 18) |
	     (match   << 16) |
	     (set     << 14) |
	     (state   << 12) |
	     (virtSyn <<  8) |
	     (pAddrHi <<  0) ;

    /* This mask will ignore all fields in TBUS[63:32] except command,	*/
    /* hint, and virtSyn.						*/
    tbusHiMask = 0x0FE00F00;

    printf("%s:\n", generateCheckPrefSyncLabel());

    saveRegs();
    
    /* Computes PA into r16 */
    computePhysicalAddress(idx);

    printf("\t dli	r17, 0x9000000018FFFFF0	# TBUS event stack pop address\n");
    printf("\t dli	r18, 0xEEEEEEEEEEEEEEEE	# \"misc bus collision\" value\n");
    printf("\t ld	r17, 0(r17)		# Get top of TBUS event stack\n");
    printf("\t beq	r17, r18, 200f    	# If (misc bus collision) break\n");
    printf("\t dsll32	r19, r17, (32 - 32)\n");
    printf("\t dsrl32	r19, r19, (32 - 32)\n");
    printf("\t bne	r16, r19, 100f\n");
    printf("\t dli	r18, 0x%08x		# TBUS[63:32] mask\n",
	   tbusHiMask);
    printf("\t dsrl32	r17, r17, (32 - 32)\n");
    printf("\t and	r17, r17, r18\n");
    printf("\t dli	r18, 0x%08x		# Expected TBUS[63:32]\n",
	   tbusHi);
    printf("\t beq	r17, r18, 200f\n");
    printf("\t nop\n");
    printf("100:\n");
    printf("\t halt(FAIL)\n");
    printf("200:\n");

    restoreRegs();
}

void
checkPrefetchAndSync()
{
    int	i;

    if (checkPrefSyncG != TRUE || doBlackBirdG != TRUE)
	return;

    printf("/* Expect %d prefetches/syncs on the stack */\n", nextPrefSyncInfoG);

    nextCheckPrefSyncLabel = nextIssuePrefSyncLabel - 1;

    for (i = nextPrefSyncInfoG - 1; i >= 0; i--)  {
	if (prefSyncInfoG[i].type == PREFSYNC_TYPE_SYNC)  {
	    printf("\t/*\n");
	    printf("\t * Checking for SYNC:\n");
	    printf("\t */\n");
	    generateSyncCheckCode(i);
	}
	else  {
	    printf("\t/*\n");
	    printf("\t * Checking for PREFETCH:\n");
	    printf("\t *\tvaddr = 0x%08x %08x\n",
		   prefSyncInfoG[i].vaddrHi, prefSyncInfoG[i].vaddrLo);
	    printf("\t *\tpaddr = 0x%08x %08x\n",
		   prefSyncInfoG[i].paddrHi, prefSyncInfoG[i].paddrLo);
	    printf("\t *\thint = %d\n", prefSyncInfoG[i].hint);
	    printf("\t */\n");
	    generatePrefetchCheckCode(i);
	}
    }

    nextPrefSyncInfoG = 0;
}

/************/
struct cell *allocateCell(UINT addr)
{
	struct cell *tmpPtr;

	tmpPtr = (struct cell *) malloc(sizeof(struct cell ));
	if (tmpPtr == NULL) {
		printf("/* Run out of memory */\n");
		exit(EXIT_FAILURE);
	}

	tmpPtr->address = addr;
	tmpPtr->hi      = 0;
	tmpPtr->lo      = 0;
	tmpPtr->next	 = NULL;
	tmpPtr->prev	 = NULL;
	return(tmpPtr);
}


/************/
void deallocateMem()
{
	struct cell *tmpPtr1 ;
	struct cell *tmpPtr2 ;
	for ( tmpPtr1 = topOfMemory ; tmpPtr1 !=  NULL ; tmpPtr1 = tmpPtr1->next) {
		tmpPtr2 = tmpPtr1;
		free(tmpPtr2);
	}
	topOfMemory = NULL;
}


/************/
struct cell *locateCell(UINT addr)
{
	UINT	dwordAddr;
	struct cell *tmpPtr;

	if ( topOfMemory == NULL )
		return(NULL);
	else
	 {
		/* calculate double word address */
		dwordAddr = addr & DWORDMASK;

		/* otherwise look for address */
		for ( tmpPtr = topOfMemory ; tmpPtr !=  NULL ; tmpPtr = tmpPtr->next ) {
			/* ok we found the word */
			if ( dwordAddr == ( tmpPtr->address & DWORDMASK ) )
				return(tmpPtr);
		}
		return(NULL);
	}
}



/************/
void printCell( struct cell *cellPtr)
{
	printf("Address: %08x Value:%08x%08x \n", cellPtr->address, cellPtr->hi, cellPtr->lo);
}


/************/
void printMemory()
{
	struct cell *currentPtr;

	printf("\n\n/* Mem dump:\n");
	for ( currentPtr = topOfMemory ; currentPtr !=  NULL ; currentPtr = currentPtr->next )
		printCell(currentPtr);
	printf("*/\n");

}


// perform SB operation given physical address and data

Cell * sb( ULL  addr, ULL value, UINT addrMode )
{
	Cell * tmpPtr;
	UINT    offset;
	Cell * newCell = NULL;

	offset   = addr & 0x7ll;
	ULL llvalue =  value;

	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr =memoryModel.placeCell(addr,addrMode);
	}
	if ( BigEndianG == TRUE ) {
		switch (offset) {
		case 7:
			tmpPtr->value = (tmpPtr->value & 0xffffffffffffff00) | (llvalue & 0x0ff);
			break;
		case 6:
			tmpPtr->value = (tmpPtr->value & 0xffffffffffff00ff) |  ((llvalue & 0x0ff) << 8);
			break;
		case 5:
			tmpPtr->value = (tmpPtr->value & 0xffffffffff00ffff) |  ((llvalue & 0x0ff) << 16);
			break;
		case 4:
			tmpPtr->value = (tmpPtr->value & 0xffffffff00ffffff) |  ((llvalue & 0x0ff) << 24);
			break;
		case 3:
			tmpPtr->value = (tmpPtr->value & 0xffffff00ffffffff) |  ((llvalue & 0x0ff) << 32);
			break;
		case 2:
			tmpPtr->value = (tmpPtr->value & 0xffff00ffffffffff) |  ((llvalue & 0x0ff) << 40);
			break;
		case 1:
			tmpPtr->value = (tmpPtr->value & 0xff00ffffffffffff) |  ((llvalue & 0x0ff) << 48);
			break;
		case 0:
			tmpPtr->value = (tmpPtr->value & 0x00ffffffffffffff) |  ((llvalue & 0x0ff) << 56);
			break;
		}

	} else
	 {

		switch (offset) {
		case 0:
			tmpPtr->value = (tmpPtr->value & 0xffffffffffffff00) | (llvalue & 0x0ff);
			break;
		case 1:
			tmpPtr->value = (tmpPtr->value & 0xffffffffffff00ff) | ((llvalue & 0x0ff) << 8);
			break;
		case 2:
			tmpPtr->value = (tmpPtr->value & 0xffffffffff00ffff) | ((llvalue & 0x0ff) << 16);
			break;
		case 3:
			tmpPtr->value = (tmpPtr->value & 0xffffffff00ffffff) | ((llvalue & 0x0ff) << 24);
			break;
		case 4:
			tmpPtr->value = (tmpPtr->value & 0xffffff00ffffffff) | ((llvalue & 0x0ff) << 32);
			break;
		case 5:
			tmpPtr->value = (tmpPtr->value & 0xffff00ffffffffff) | ((llvalue & 0x0ff) << 40);
			break;
		case 6:
			tmpPtr->value = (tmpPtr->value & 0xff00ffffffffffff) | ((llvalue & 0x0ff) << 48);
			break;
		case 7:
			tmpPtr->value = (tmpPtr->value & 0x00ffffffffffffff) | ((llvalue & 0x0ff) << 56);
			break;
		}
	}
	if ( doAtomicG == 0 )	    printf("\t\t\t\t\t#sb  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"sb  %016lx %016llx %016llx\n",value,addr,tmpPtr->value);
	
	return ( newCell );
}

/************/
Cell * sh( ULL addr, ULL value, UINT addrMode )
{
	Cell          * tmpPtr;
	UINT	offset;

	Cell *  newCell = NULL;

	ULL llvalue = value;
	offset      = addr & 0x7;

	newCell = tmpPtr = memoryModel.locateCell(addr);

	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	}

	if ( BigEndianG == TRUE ) {

		switch (offset) {

		case 7:
		case 6:
			tmpPtr->value = (tmpPtr->value & 0xffffffffffff0000) | (llvalue & 0x0ffff);
			break;
		case 5:
		case 4:
			tmpPtr->value = (tmpPtr->value & 0xffffffff0000ffff) | ((llvalue & 0x0ffff) << 16);
			break;
		case 3:
		case 2:
			tmpPtr->value = (tmpPtr->value & 0xffff0000ffffffff) | ((llvalue & 0x0ffff) << 32);
			break;
		case 1:
		case 0:
			tmpPtr->value = (tmpPtr->value & 0x0000ffffffffffff) | ((llvalue & 0x0ffff) << 48);
			break;
		}
	} else
	 {

		switch (offset) {

		case 0:
		case 1:
			tmpPtr->value = (tmpPtr->value & 0xffffffffffff0000) | (llvalue & 0x0ffff);
			break;
		case 2:
		case 3:
			tmpPtr->value = (tmpPtr->value & 0xffffffff0000ffff) | ((llvalue & 0x0ffff) << 16);
			break;
		case 4:
		case 5:
			tmpPtr->value = (tmpPtr->value & 0xffff0000ffffffff) | ((llvalue & 0x0ffff) << 32);
			break;
		case 6:
		case 7:
			tmpPtr->value = (tmpPtr->value & 0x0000ffffffffffff) | ((llvalue & 0x0ffff) << 48);
			break;
		}
	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#sh  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"sh  %016lx %016llx %016llx\n",value,addr,tmpPtr->value);

	return ( newCell );
}


/************/
Cell * sw( ULL addr, ULL  value, UINT addrMode)

{
	Cell * tmpPtr;
	UINT   offset;

	Cell * newCell = NULL;

	offset = addr & 0x7;
	ULL llvalue = value;
	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL )
		tmpPtr = memoryModel.placeCell(addr,addrMode);

	if ( BigEndianG == TRUE ) {

		switch (offset) {

		case 7:
		case 6:
		case 5:
		case 4:
			tmpPtr->value = (tmpPtr->value & 0xffffffff00000000) | (llvalue & 0x00000000ffffffff);
			break;
		case 3:
		case 2:
		case 1:
		case 0:
			tmpPtr->value = (tmpPtr->value & 0x00000000ffffffff) | ((llvalue & 0x00000000ffffffff) << 32);
			break;
		}
	} else	 {

		switch (offset) {

		case 0:
		case 1:
		case 2:
		case 3:
			tmpPtr->value = (tmpPtr->value & 0xffffffff00000000) | (llvalue & 0x00000000ffffffff);
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			tmpPtr->value = (tmpPtr->value & 0x00000000ffffffff) | ((llvalue & 0x00000000ffffffff) << 32);
			break;
		}
	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#sw  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"sw  %016lx %016llx %016llx\n",value,addr,tmpPtr->value);

	return ( newCell );
}

//

Cell * swc1( ULL addr, ULL  value, UINT addrMode)

{
	Cell * tmpPtr;
	UINT   offset;
	
        Cell * newCell = NULL;

	offset = addr & 0x7;
	ULL llvalue = value;
	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	}

	if ( BigEndianG == TRUE ) {

		switch (offset) {

		case 7:
		case 6:
		case 5:
		case 4:
			tmpPtr->value = (tmpPtr->value & 0xffffffff00000000) | (llvalue & 0x00000000ffffffff);
			break;
		case 3:
		case 2:
		case 1:
		case 0:
			tmpPtr->value = (tmpPtr->value & 0x00000000ffffffff) | ((llvalue & 0x00000000ffffffff) << 32);
			break;
		}
	} else	 {

		switch (offset) {

		case 0:
		case 1:
		case 2:
		case 3:
			tmpPtr->value = (tmpPtr->value & 0xffffffff00000000) | (llvalue & 0x00000000ffffffff);
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			tmpPtr->value = (tmpPtr->value & 0x00000000ffffffff) | ((llvalue & 0x00000000ffffffff) << 32);
			break;
		}
	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#swc1  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"swc1  %016lx %016llx %016llx\n",value,addr,tmpPtr->value);
	return ( newCell );
	
}

//

Cell * swl(ULL  addr, ULL value, UINT addrMode)

{
	Cell  * tmpPtr;
	UINT	offset;

	Cell * newCell = NULL;

	offset      = addr & 0x7;
	ULL llvalue = value & 0x00000000ffffffff;

	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	}

	if ( BigEndianG == TRUE ) {

		switch (offset) {

		case 7:
			tmpPtr->value = (llvalue >> 24) | (0xffffffffffffff00 & tmpPtr->value);
			break;
		case 6:
			tmpPtr->value = (llvalue >> 16) | (0xffffffffffff0000 & tmpPtr->value);
			break;
		case 5:
			tmpPtr->value = (llvalue >> 8)  | (0xffffffffff000000 & tmpPtr->value);
			break;
		case 4:
			tmpPtr->value = llvalue         | (0xffffffff00000000 & tmpPtr->value);
			break;
		case 3:
			tmpPtr->value = ((llvalue >> 24) << 32)  | (0xffffff00ffffffff & tmpPtr->value);
			break;
		case 2:
			tmpPtr->value = ((llvalue >> 16) << 32)  | (0xffff0000ffffffff & tmpPtr->value);
			break;
		case 1:
			tmpPtr->value = ((llvalue >> 8)  << 32)  | (0xff000000ffffffff & tmpPtr->value);
			break;
		case 0:
			tmpPtr->value = ((llvalue) << 32)        | (0x00000000ffffffff & tmpPtr->value);
			break;
		}
	} else
	 {
		switch (offset) {

		case 0:
			tmpPtr->value = (llvalue >> 24) | (0xffffffffffffff00 & tmpPtr->value);
			break;
		case 1:
			tmpPtr->value = (llvalue >> 16) | (0xffffffffffff0000 & tmpPtr->value);
			break;
		case 2:
			tmpPtr->value = (llvalue >> 8)  | (0xffffffffff000000 & tmpPtr->value);
			break;
		case 3:
			tmpPtr->value = llvalue         | (0xffffffff00000000 & tmpPtr->value);
			break;
		case 4:
			tmpPtr->value = ((llvalue >> 24) << 32)  | (0xffffff00ffffffff & tmpPtr->value);
			break;
		case 5:
			tmpPtr->value = ((llvalue >> 16) << 32)  | (0xffff0000ffffffff & tmpPtr->value);
			break;
		case 6:
			tmpPtr->value = ((llvalue >> 8)  << 32)  | (0xff000000ffffffff & tmpPtr->value);
			break;
		case 7:
			tmpPtr->value = ((llvalue) << 32)        | (0x00000000ffffffff & tmpPtr->value);
			break;
		}
	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#swl  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"swl %016lx %016llx %016llx\n",value,addr,tmpPtr->value);
	
	return ( newCell );
}


// 

Cell * swr(ULL addr, ULL value, UINT addrMode)
{
	Cell * tmpPtr;
	UINT   offset;
	
	Cell * newCell = NULL;

	offset = addr & 0x7;
	ULL llvalue = value & 0x00000000ffffffff;
	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	}

	if ( BigEndianG == TRUE ) {
		switch (offset) {

		case 7:
			tmpPtr->value = llvalue         | (0xffffffff00000000 & tmpPtr->value);
			break;
		case 6:
			tmpPtr->value = ((llvalue << 8)  & 0x00000000ffffff00) | (0xffffffff000000ff & tmpPtr->value);
			break;
		case 5:
			tmpPtr->value = ((llvalue << 16) & 0x00000000ffff0000) | (0xffffffff0000ffff & tmpPtr->value);
			break;
		case 4:
			tmpPtr->value = ((llvalue << 24) & 0x00000000ff000000) | (0xffffffff00ffffff & tmpPtr->value);
			break;
		case 3:
			tmpPtr->value =  (llvalue << 32)                       | (0x00000000ffffffff & tmpPtr->value);
			break;
		case 2:
			tmpPtr->value = ((llvalue << 40) & 0xffffff0000000000) | (0x000000ffffffffff & tmpPtr->value);
			break;
		case 1:
			tmpPtr->value = ((llvalue << 48) & 0xffff000000000000) | (0x0000ffffffffffff & tmpPtr->value);
			break;
		case 0:
			tmpPtr->value = ((llvalue << 56) & 0xff00000000000000) | (0x00ffffffffffffff & tmpPtr->value);
			break;
		}
	} else
	 {

		switch (offset) {

		case 0:
			tmpPtr->value = llvalue         | (0xffffffff00000000 & tmpPtr->value);
			break;
		case 1:
			tmpPtr->value = ((llvalue << 8)  & 0x00000000ffffff00) | (0xffffffff000000ff & tmpPtr->value);
			break;
		case 2:
			tmpPtr->value = ((llvalue << 16) & 0x00000000ffff0000) | (0xffffffff0000ffff & tmpPtr->value);
			break;
		case 3:
			tmpPtr->value = ((llvalue << 24) & 0x00000000ff000000) | (0xffffffff00ffffff & tmpPtr->value);
			break;
		case 4:
			tmpPtr->value =  (llvalue << 32)                       | (0x00000000ffffffff & tmpPtr->value);
			break;
		case 5:
			tmpPtr->value = ((llvalue << 40) & 0xffffff0000000000) | (0x000000ffffffffff & tmpPtr->value);
			break;
		case 6:
			tmpPtr->value = ((llvalue << 48) & 0xffff000000000000) | (0x0000ffffffffffff & tmpPtr->value);
			break;
		case 7:
			tmpPtr->value = ((llvalue << 56) & 0xff00000000000000) | (0x00ffffffffffffff & tmpPtr->value);
			break;

		}
	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#swr  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"swr %016lx %016llx %016llx\n",value,addr,tmpPtr->value);

	return ( newCell );
}

//

Cell *  sdl( ULL addr, ULL value, UINT addrMode )
{
	Cell  * tmpPtr;
	uint	offset;
	Cell *  newCell = NULL;

	offset = addr & 0x7;

	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	}

	if ( BigEndianG == TRUE ) {

		switch (offset) {

		case 7:
			tmpPtr->value = (value >> 56) | (0xffffffffffffff00 & tmpPtr->value);
			break;
		case 6:
			tmpPtr->value = (value >> 48) | (0xffffffffffff0000 & tmpPtr->value);
			break;
		case 5:
			tmpPtr->value = (value >> 40) | (0xffffffffff000000 & tmpPtr->value);
			break;
		case 4:
			tmpPtr->value = (value >> 32) | (0xffffffff00000000 & tmpPtr->value);
			break;
		case 3:
			tmpPtr->value = (value >> 24) | (0xffffff0000000000 & tmpPtr->value);
			break;
		case 2:
			tmpPtr->value = (value >> 16) | (0xffff000000000000 & tmpPtr->value);
			break;
		case 1:
			tmpPtr->value = (value >> 8)  | (0xff00000000000000 & tmpPtr->value);
			break;
		case 0:
			tmpPtr->value = value;
			break;
		}
	} else
	 {
		switch (offset) {

		case 0:
			tmpPtr->value = (value >> 56) | (0xffffffffffffff00 & tmpPtr->value);
			break;
		case 1:
			tmpPtr->value = (value >> 48) | (0xffffffffffff0000 & tmpPtr->value);
			break;
		case 2:
			tmpPtr->value = (value >> 40) | (0xffffffffff000000 & tmpPtr->value);
			break;
		case 3:
			tmpPtr->value = (value >> 32) | (0xffffffff00000000 & tmpPtr->value);
			break;
		case 4:
			tmpPtr->value = (value >> 24) | (0xffffff0000000000 & tmpPtr->value);
			break;
		case 5:
			tmpPtr->value = (value >> 16) | (0xffff000000000000 & tmpPtr->value);
			break;
		case 6:
			tmpPtr->value = (value >> 8)  | (0xff00000000000000 & tmpPtr->value);
			break;
		case 7:
			tmpPtr->value = value;
			break;
		}
	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#sdl  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"sdl %016llx %016llx %016llx\n",value,addr,tmpPtr->value);

	return ( newCell );
}

//

Cell *  sdr( ULL addr, ULL value, UINT addrMode)
{
    Cell * tmpPtr;
    UINT   offset;
    Cell * newCell = NULL;

    offset = addr & 0x7;

    newCell = tmpPtr = memoryModel.locateCell(addr);
    if ( tmpPtr == NULL ) {
	tmpPtr = memoryModel.placeCell(addr, addrMode);
    }

	if ( BigEndianG == TRUE ) {

		switch (offset) {

		case 7:
		    tmpPtr->value = value;
		    break;
		case 6:
		    tmpPtr->value = (value << 8)  | (0x00000000000000ff & tmpPtr->value);
		    break;
		case 5:
		    tmpPtr->value = (value << 16) | (0x000000000000ffff & tmpPtr->value);
		    break;
		case 4:
		    tmpPtr->value = (value << 24) | (0x0000000000ffffff & tmpPtr->value);
		    break;
		case 3:
		    tmpPtr->value = (value << 32) | (0x00000000ffffffff & tmpPtr->value);
		    break;
		case 2:
		    tmpPtr->value = (value << 40) | (0x000000ffffffffff & tmpPtr->value);
		    break;
		case 1:
		    tmpPtr->value = (value << 48) | (0x0000ffffffffffff & tmpPtr->value);
		    break;
		case 0:
		    tmpPtr->value = (value << 56) | (0x00ffffffffffffff & tmpPtr->value);
		    break;
		}
	} else
	 {
		switch (offset) {
		case 0:
		    tmpPtr->value = value;
		    break;
		case 1:
		    tmpPtr->value = (value << 8)  | (0x00000000000000ff & tmpPtr->value);
		    break;
		case 2:
		    tmpPtr->value = (value << 16) | (0x000000000000ffff & tmpPtr->value);
		    break;
		case 3:
		    tmpPtr->value = (value << 24) | (0x0000000000ffffff & tmpPtr->value);
		    break;
		case 4:
		    tmpPtr->value = (value << 32) | (0x00000000ffffffff & tmpPtr->value);
		    break;
		case 5:
		    tmpPtr->value = (value << 40) | (0x000000ffffffffff & tmpPtr->value);
		    break;
		case 6:
		    tmpPtr->value = (value << 48) | (0x0000ffffffffffff & tmpPtr->value);
		    break;
		case 7:
		    tmpPtr->value = (value << 56) | (0x00ffffffffffffff & tmpPtr->value);
		    break;
		}
	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#sdr  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"sdr %016llx %016llx %016llx\n",value,addr,tmpPtr->value);

	return ( newCell );
}
/************/
Cell *  sd( ULL addr, ULL value, UINT addrMode )
{
	Cell  * tmpPtr;
        Cell  * newCell = NULL;
	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	}
	tmpPtr->value = value ;

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#sd  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"sd  %016llx %016llx %016llx\n",value,addr,tmpPtr->value);

       return ( newCell );
}
/************/
Cell * sdc1( ULL addr, ULL value, UINT addrMode )
{
	Cell  * tmpPtr;
	Cell * newCell = NULL;

	newCell = tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	}
	tmpPtr->value = value ;

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#sdc1  %016llx result %016llx\n",addr,tmpPtr->value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"sdc1  %016llx %016llx %016llx\n",value,addr,tmpPtr->value);

        return ( newCell );
}
/************/
Cell *lb(ULL addr, ULL * value, UINT addrMode)
{
	Cell  * tmpPtr;
	UINT	offset;

	ULL prevValue = *value;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
		tmpPtr = memoryModel.placeCell(addr,addrMode);
		*value = 0;
		return(NULL);
	}
	/* if a negative number then sign extend */


	offset = addr & 0x7;

	if ( BigEndianG == TRUE ) {

		switch (offset) {

		case 7:
			*value = tmpPtr->value  & 0x00000000000000ff;
			break;
		case 6:
			*value = (tmpPtr->value & 0x000000000000ff00) >> 8;
			break;
		case 5:
			*value = (tmpPtr->value & 0x0000000000ff0000) >> 16;
			break;
		case 4:
			*value = (tmpPtr->value & 0x00000000ff000000) >> 24;
			break;
		case 3:
			*value = (tmpPtr->value & 0x000000ff00000000) >> 32;
			break;
		case 2:
			*value = (tmpPtr->value & 0x0000ff0000000000) >> 40;
			break;
		case 1:
			*value = (tmpPtr->value & 0x00ff000000000000) >> 48;
			break;
		case 0:
			*value = (tmpPtr->value & 0xff00000000000000) >> 56;
			break;
		}
	} else
	 {

		switch (offset) {

		case 0:
			*value = tmpPtr->value  & 0x00000000000000ff;
			break;
		case 1:
			*value = (tmpPtr->value & 0x000000000000ff00) >> 8;
			break;
		case 2:
			*value = (tmpPtr->value & 0x0000000000ff0000) >> 16;
			break;
		case 3:
			*value = (tmpPtr->value & 0x00000000ff000000) >> 24;
			break;
		case 4:
			*value = (tmpPtr->value & 0x000000ff00000000) >> 32;
			break;
		case 5:
			*value = (tmpPtr->value & 0x0000ff0000000000) >> 40;
			break;
		case 6:
			*value = (tmpPtr->value & 0x00ff000000000000) >> 48;
			break;
		case 7:
			*value = (tmpPtr->value & 0xff00000000000000) >> 56;
			break;
		}
	}

	// sign extend
	if ( *value & 0x080 )
	    *value = *value | 0xffffffffffffff00;
	else
	    *value = *value & 0x00000000000000ff;

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#lb  %016llx result %016llx\n",addr, *value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"lb  %016llx %016llx %016llx\n",prevValue, addr, *value);
	return(tmpPtr);
}


/************/
Cell * lh( ULL addr, ULL * value, int extend, UINT addrMode)
{
	Cell  * tmpPtr;
	UINT	offset;

	ULL prevValue = *value;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
		tmpPtr = memoryModel.placeCell(addr,addrMode);
		*value = 0;
		return(NULL);
	}
	/* if a negative number then sign extend */


	offset = addr & 0x7;

	if ( BigEndianG == TRUE ) {
		switch (offset) {

		case 7:
		case 6:
			*value = tmpPtr->value & 0x000000000000ffff;
			break;

		case 5:
		case 4:
			*value = (tmpPtr->value & 0x00000000ffff0000) >> 16;
			break;

		case 3:
		case 2:
			*value = (tmpPtr->value & 0x0000ffff00000000) >> 32;
			break;

		case 1:
		case 0:
			*value = (tmpPtr->value & 0xffff000000000000) >> 48;
			break;

		}
	} else
	 {

		switch (offset) {

		case 0:
		case 1:
			*value = tmpPtr->value & 0x000000000000ffff;
			break;

		case 2:
		case 3:
			*value = (tmpPtr->value & 0x00000000ffff0000) >> 16;
			break;

		case 4:
		case 5:
			*value = (tmpPtr->value & 0x0000ffff00000000) >> 32;
			break;

		case 6:
		case 7:
			*value = (tmpPtr->value & 0xffff000000000000) >> 48;
			break;
		}
	}

	// sign extend 
	if ( extend == TRUE && (*value & 0x08000) ) 
	    *value = *value | 0xffffffffffff0000;
	else
	    *value = *value & 0x000000000000ffff; 

	if ( extend == TRUE ) {
		if ( doAtomicG == 0 ) printf("\t\t\t\t\t#lh  %016llx result %016llx\n",addr, *value);
		if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"lh  %016llx %016llx %016llx\n",prevValue,addr,*value);
	}
	else {
		if ( doAtomicG == 0 ) printf("\t\t\t\t\t#lhu %016llx result %016llx\n",addr, *value);
		if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"lhu %016llx %016llx %016llx\n", prevValue, addr, *value);
	}	
	return(tmpPtr);
}


/************/
Cell  * lw(ULL addr, ULL * value, UINT addrMode)
{
	Cell  * tmpPtr;
	UINT	offset;

	ULL prevValue = *value;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
		tmpPtr = memoryModel.placeCell(addr,addrMode);
		*value = 0 ;
		return(NULL);
	}
	/* if a negative number then sign extend */


	offset = addr & 0x7;

	if ( BigEndianG == TRUE ) {
		switch (offset) {

		case 7:
		case 6:
		case 5:
		case 4:

			*value = tmpPtr->value & 0x00000000ffffffff;
			break;
		case 3:
		case 2:
		case 1:
		case 0:

			*value = (tmpPtr->value >> 32) & 0x00000000ffffffff;
			break;
		}
	} else
	 {

		switch (offset) {

		case 0:
		case 1:
		case 2:
		case 3:
			*value = tmpPtr->value & 0x00000000ffffffff;
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			*value = (tmpPtr->value >> 32) & 0x00000000ffffffff;
			break;
		}
	}
	/* sign extend */
	if ( *value & 0x0000000080000000 ) 
	    *value = *value | 0xffffffff00000000;
	else
	    *value = *value & 0x00000000ffffffff; 

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#lw  %016llx result %016llx\n",addr, *value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"lw  %016llx %016llx %016llx\n",prevValue, addr, *value);
	return(tmpPtr);
}


/*************/
Cell  *lwl(ULL addr, ULL * value, UINT addrMode, int *newCell )
{
	Cell  * tmpPtr;
	UINT	offset;

	ULL prevValue = *value;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
		tmpPtr = memoryModel.placeCell(addr,addrMode);
		*newCell = TRUE;
	}
	/* if a negative number then sign extend */


	offset = addr & 0x7;

	if ( BigEndianG == TRUE ) {
		switch (offset) {

		case 7:
			*value = ((tmpPtr->value & 0x00000000000000ff) << 24) | (*value & 0xffffffff00ffffff);
			break;
		case 6:
			*value = ((tmpPtr->value & 0x000000000000ffff) << 16) | (*value & 0xffffffff0000ffff);
			break;
		case 5:
			*value = ((tmpPtr->value & 0x0000000000ffffff) << 8 ) | (*value & 0xffffffff000000ff);
			break;
		case 4:
			*value = ((tmpPtr->value & 0x00000000ffffffff)     ) | (*value & 0xffffffff00000000);
			break;
		case 3:
			*value = ((tmpPtr->value & 0x000000ff00000000) >> 8 ) | (*value & 0xffffffff00ffffff);
			break;
		case 2:
			*value = ((tmpPtr->value & 0x0000ffff00000000) >> 16 )| (*value & 0xffffffff0000ffff);
			break;
		case 1:
			*value = ((tmpPtr->value & 0x00ffffff00000000) >> 24 )| (*value & 0xffffffff000000ff);
			break;
		case 0:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 32 )| (*value & 0xffffffff00000000);
			break;
		}
	} else
	 {
		switch (offset) {

		case 0:
			*value = ((tmpPtr->value & 0x00000000000000ff) << 24) | (*value & 0xffffffff00ffffff);
			break;
		case 1:
			*value = ((tmpPtr->value & 0x000000000000ffff) << 16) | (*value & 0xffffffff0000ffff);
			break;
		case 2:
			*value = ((tmpPtr->value & 0x0000000000ffffff) << 8 ) | (*value & 0xffffffff000000ff);
			break;
		case 3:
			*value = ((tmpPtr->value & 0x0000000000ffffff)      ) | (*value & 0xffffffff00000000);
			break;
		case 4:
			*value = ((tmpPtr->value & 0x000000ff00000000) >> 8 ) | (*value & 0xffffffff00ffffff);
			break;
		case 5:
			*value = ((tmpPtr->value & 0x0000ffff00000000) >> 16 )| (*value & 0xffffffff0000ffff);
			break;
		case 6:
			*value = ((tmpPtr->value & 0x00ffffff00000000) >> 24 )| (*value & 0xffffffff000000ff);
			break;
		case 7:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 32 )| (*value & 0xffffffff00000000);
			break;
		}
	}

// take care of 32 bit sign extension

	if ( *value & 0x0000000080000000 )
	     *value |=0xffffffff00000000;
	else
	     *value &=0x00000000ffffffff;

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#lwl %016llx result %016llx\n",addr, *value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"lwl %016llx %016llx %016llx\n",prevValue, addr, *value);
	return(tmpPtr);
}


/*************/
Cell  *lwr(ULL addr, ULL * value, UINT addrMode, int * newCell )

{
	Cell  * tmpPtr;
	UINT	offset;

	ULL prevValue = *value;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
	  tmpPtr = memoryModel.placeCell(addr,addrMode);
	  *newCell = TRUE;
	}
	/* if a negative number then sign extend */


	offset = addr & 0x7;

	if ( BigEndianG == TRUE ) {
		switch (offset) {

		case 7:
			*value = (tmpPtr->value &  0x00000000ffffffff) ;
			break;
		case 6:
			*value = ((tmpPtr->value & 0x00000000ffffffff) >> 8)  | ( *value & 0x00000000ff000000);
			break;
		case 5:
			*value = ((tmpPtr->value & 0x00000000ffffffff) >> 16) | ( *value & 0x00000000ffff0000);
			break;
		case 4:
			*value = ((tmpPtr->value & 0x00000000ffffffff) >> 24) | ( *value & 0x00000000ffffff00);
			break;
		case 3:
			*value = (tmpPtr->value &  0xffffffff00000000) >> 32 ;
			break;
		case 2:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 40) | ( *value & 0x00000000ff000000);
			break;
		case 1:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 48) | ( *value & 0x00000000ffff0000);
			break;
		case 0:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 56) | ( *value & 0x00000000ffffff00);
			break;
		}
	} else
	 {
		switch (offset) {

		case 0:
			*value = (tmpPtr->value & 0x00000000ffffffff) ;
			break;
		case 1:
			*value = ((tmpPtr->value & 0x00000000ffffffff) >> 8)  | ( *value & 0x00000000ff000000);
			break;
		case 2:
			*value = ((tmpPtr->value & 0x00000000ffffffff) >> 16) | ( *value & 0x00000000ffff0000);
			break;
		case 3:
			*value = ((tmpPtr->value & 0x00000000ffffffff) >> 24) | ( *value & 0x00000000ffffff00);
			break;
		case 4:
			*value = (tmpPtr->value &  0xffffffff00000000) >> 32 ;
			break;
		case 5:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 40) | ( *value & 0x00000000ff000000);
			break;
		case 6:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 48) | ( *value & 0x00000000ffff0000);
			break;
		case 7:
			*value = ((tmpPtr->value & 0xffffffff00000000) >> 56) | ( *value & 0x00000000ffffff00);
			break;
		}

	}
// take care of 32 bit sign extension

	if ( *value & 0x0000000080000000 )
	     *value |=0xffffffff00000000;
	else
	     *value &=0x00000000ffffffff;

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#lwr %016llx result %016llx\n",addr, *value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"lwr %016llx %016llx %016llx\n",prevValue, addr, *value);

	return(tmpPtr);
}


/************/
Cell  *ld(ULL addr, ULL * value, UINT addrMode )

{
	Cell * tmpPtr;
        ULL prevValue = *value;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
		tmpPtr = memoryModel.placeCell(addr,addrMode);
		*value = 0;
		return(NULL);
	}

	*value = tmpPtr->value;

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#ld  %016llx result %016llx\n",addr, *value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"ld  %016llx %016llx %016llx\n",prevValue,addr, *value);
	return(tmpPtr);
}


/************/
Cell  *ldl(ULL addr, ULL * value, UINT addrMode, int * newCell )
{
	Cell  * tmpPtr;
	UINT	offset;
	ULL prevValue = *value;

	offset = addr & 0x7;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
		tmpPtr = memoryModel.placeCell(addr,addrMode);
		*newCell = TRUE;
	}

	if ( BigEndianG == TRUE ) {
		switch ( offset ) {

		case 0:
			*value = tmpPtr->value;
			break;
		case 1:
			*value = (tmpPtr->value << 8 ) | (* value & 0x00000000000000ff);
			break;
		case 2:
			*value = (tmpPtr->value << 16 ) | (* value & 0x000000000000ffff);
			break;
		case 3:
			*value = (tmpPtr->value << 24 ) | (* value & 0x0000000000ffffff);
			break;
		case 4:
			*value = (tmpPtr->value << 32 ) | (* value & 0x00000000ffffffff);
			break;
		case 5:
			*value = (tmpPtr->value << 40 ) | (* value & 0x000000ffffffffff);
			break;
		case 6:
			*value = (tmpPtr->value << 48 ) | (* value & 0x0000ffffffffffff);
			break;
		case 7:
			*value = (tmpPtr->value << 56 ) | (* value & 0x00ffffffffffffff);
			break;
		}
	}
	else
	{
		switch ( offset ) {

		case 7:
			*value = tmpPtr->value;
			break;
		case 6:
			*value = (tmpPtr->value << 8 ) | (* value & 0x00000000000000ff);
			break;
		case 5:
			*value = (tmpPtr->value << 16 ) | (* value & 0x000000000000ffff);
			break;
		case 4:
			*value = (tmpPtr->value << 24 ) | (* value & 0x0000000000ffffff);
			break;
		case 3:
			*value = (tmpPtr->value << 32 ) | (* value & 0x00000000ffffffff);
			break;
		case 2:
			*value = (tmpPtr->value << 40 ) | (* value & 0x000000ffffffffff);
			break;
		case 1:
			*value = (tmpPtr->value << 48 ) | (* value & 0x0000ffffffffffff);
			break;
		case 0:
			*value = (tmpPtr->value << 56 ) | (* value & 0x00ffffffffffffff);
			break;
		}
	}

	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#ldl %016llx result %016llx\n",addr, *value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"ldl %016llx %016llx %016llx\n",prevValue, addr, *value);
	return(tmpPtr);











}

Cell *ldr(ULL addr, ULL * value, UINT addrMode, int * newCell )
{
	Cell *  tmpPtr;
	UINT	offset;
	ULL prevValue = *value;

	offset = addr & 0x7;

	tmpPtr = memoryModel.locateCell(addr);
	if ( tmpPtr == NULL ) {
		tmpPtr = memoryModel.placeCell(addr,addrMode);
		*newCell = TRUE;
	}
	if ( BigEndianG == TRUE ) {
		switch ( offset ) {

		case 0:
			*value = (tmpPtr->value >> 56  ) | (*value & 0xffffffffffffff00);
			break;
		case 1:
			*value = (tmpPtr->value >> 48  ) | (*value & 0xffffffffffff0000);
			break;
		case 2:
			*value = (tmpPtr->value >> 40  ) | (*value & 0xffffffffff000000);
			break;
		case 3:
			*value = (tmpPtr->value >> 32  ) | (*value & 0xffffffff00000000);
			break;
		case 4:
			*value = (tmpPtr->value >> 24  ) | (*value & 0xffffff0000000000);
			break;
		case 5:
			*value = (tmpPtr->value >> 16  ) | (*value & 0xffff000000000000);
			break;
		case 6:
			*value = (tmpPtr->value >>  8  ) | (*value & 0xff00000000000000);
			break;
		case 7:
			*value = (tmpPtr->value);
			break;
		}
	}
	else
	{
		switch ( offset ) {

		case 7:
			*value = (tmpPtr->value >> 56  ) | (* value & 0xffffffffffffff00);
			break;
		case 6:
			*value = (tmpPtr->value >> 48  ) | (* value & 0xffffffffffff0000);
			break;
		case 5:
			*value = (tmpPtr->value >> 40  ) | (* value & 0xffffffffff000000);
			break;
		case 4:
			*value = (tmpPtr->value >> 32  ) | (* value & 0xffffffff00000000);
			break;
		case 3:
			*value = (tmpPtr->value >> 24  ) | (* value & 0xffffff0000000000);
			break;
		case 2:
			*value = (tmpPtr->value >> 16  ) | (* value & 0xffff000000000000);
			break;
		case 1:
			*value = (tmpPtr->value >>  8  ) | (* value & 0xff00000000000000);
			break;
		case 0:
			*value = (tmpPtr->value);
			break;
		}


	}
	if ( doAtomicG == 0 ) printf("\t\t\t\t\t#ldr %016llx result %016llx\n",addr, *value);
	if ( doMpTrace == TRUE ) fprintf(MPtraceFile,"ldr %016llx %016llx %016llx\n",prevValue, addr, *value);
	return(tmpPtr);
}


/************/
Cell  *findRandAddr()
{
	int	i;
	int	pickedCell;
	Cell * tmpPtr;
	UINT	ranValue;

	if ( memoryModel.getTopOfMemory() == NULL )
		return(NULL);
	ranValue = random();

       // if cellCount is 0, then return first entry , else pick one 

       tmpPtr = memoryModel.getTopOfMemory();

       if ( memoryModel.cellCount > 1 ) {
	 pickedCell = ranValue % (memoryModel.cellCount);
	 for ( i = 0 ; i < pickedCell ; i++)
	   tmpPtr = tmpPtr->next;
       }
       return(tmpPtr);
}


/************/
/* fill up constant registers with new values. */
void fillImmdReg()
{
	int	i;
	UINT	ranValue;

	if ( doAtomicG == 0 ) printf("	/* load immd registers */\n");
	for (  i = 0 ; i < maxImmdRegG ; i++) {

	  immdReg64[i]  = ((ULL) random()) & 0x00000000ffffffffLL;
	  immdReg64[i]  =  immdReg64[i] << 32ll;
	  immdReg64[i] |= (ULL) ((ULL) random()) & 0x00000000ffffffffLL;

		printf("\t dli	r%-2d,\"%016llx\"\n", baseImmdRegG + i, immdReg64[i]);
	}

	if ( doUseFpuRegG  == TRUE ) 
	for ( i = 0 ; i < maxImmdFpuRegG ; i++ ) {

	  immdFpuReg64[i]  = ((ULL) random()) & 0x00000000ffffffffLL;
	  immdFpuReg64[i]  =  immdFpuReg64[i] << 32ll;
	  immdFpuReg64[i] |= (ULL) ((ULL) random()) & 0x00000000ffffffffLL;

		printf("\t dli	r%-2d,\"%016llx\"\n",TMP_REG_2, immdFpuReg64[i]);
		printf("\t dmtc1 r%-2d,fp%-2d\n",TMP_REG_2,baseImmdFpuRegG + i);
	}

	if ( doAtomicG ) printf("#ATOMIC#\n");

}


/**************************/
int findSameType(int codeType)
{
	register int i;
	int result     = 0;
	int matchCount = 0;
	int matchedOffset[MAX_COMP_REG];
	for ( i = 0; i < maxCompRegG ; i++ ) 
		if ( (compRegValidG[i] == TRUE) && (mipsMode[i] == codeType) ) {
			matchedOffset[matchCount++] = i;
		}

	if ( matchCount > 0 ) {
		if ( matchCount == 1 ) return ( matchedOffset[0] ); 
		else 
			return(	matchedOffset[random()%matchCount] );
	}
	else return ( -1 );
}

void compareCpuReg() {

register int i;
int tmpReg1G = 30;

  for ( i = 0 ; i < maxCompRegG ; i++) {
    printf("	dli	r%-2d,\"%016llx\"\n", tmpReg1G, compReg64[i]);
    printf("     	bne	r%-2d,r%-2d,1f\n", tmpReg1G, baseCompRegG + i);
    compRegValidG[i] = FALSE;
  }
  printf("	nop\n");
  printf("	beq r0,r0,2f\n");
  printf("	nop\n");

  if ( onHardwareG == TRUE ) {
    printf("1:	dla	r2,iterationCount	\n");
    printf("	lw	r28,0(r2)		\n");
  }

  printf("1:	halt(FAIL)\n");
  printf("2:\n");
}

// routine to compare floating point registers with reference
void compareFpuReg() {

  printf("/* fpu reg comp: */\n");

  int tmpReg1G = TMP_REG_1;
  int tmpReg2G = TMP_REG_2;

  for ( int i = 0 ; i < maxCompFpuRegG ; i++) {

    if ( fpuDataType[i] == DATA64 ) {
      
      printf("\tdmfc1      r%-2d,fp%-2d\n",tmpReg1G,baseCompFpuRegG+i);
      printf("\tdli	r%-2d,\"%016llx\"\n", tmpReg2G, compFpuReg64[i]);
		if ( onHardwareG == TRUE ) {
		  printf("\tbnel	r%-2d,r%-2d,1f\n", tmpReg1G, tmpReg2G);
		  printf("\tli	r29,%-2d\n", baseCompFpuRegG + i + 32);
		} else {
        printf("\tbne	r%-2d,r%-2d,1f\n",tmpReg1G,tmpReg2G);
      }

      compFpuRegValidG[i] = FALSE;
    }
    else { 

      printf("\tmfc1      r%-2d,fp%-2d\n",tmpReg1G,baseCompFpuRegG+i);
      printf("\tdli	r%-2d,\"%016llx\"\n", tmpReg2G, compFpuReg64[i]);
		if ( onHardwareG == TRUE ) {
		  printf("\tbnel	r%-2d,r%-2d,1f\n", tmpReg1G, tmpReg2G);
		  printf("\tli	r29,%-2d\n", baseCompFpuRegG + i + 32);
		} else {
        printf("\tbne	r%-2d,r%-2d,1f\n",tmpReg1G,tmpReg2G);
      }
      compFpuRegValidG[i] = FALSE;
    }

  }

  printf("	nop\n");
  printf("	beq r0,r0,2f\n");
  printf("	nop\n");
  
  if ( onHardwareG == TRUE ) {
    printf("1:	dla	r2,iterationCount	\n");
    printf("	lw	r28,0(r2)		\n");
  }
  printf("1:	halt(FAIL)\n");
  printf("2:\n");

}

// routine to see if we should issue a CPU instruction or FPU.
// used to delay self check as much as possible

int checkDestRegStatus() 
{
 register int i;

  for ( i = 0 ; i < maxCompRegG ; i++)
    if ( compRegValidG[i] == FALSE ) 
      break;

  if ( i != maxCompRegG ) 
    return (CPU_REG_AVAILABLE);

  for ( i = 0 ; i < maxCompFpuRegG ; i++)
    if ( compFpuRegValidG[i] == FALSE ) 
      break;

  // keep it simple either we 
  if ( i != maxCompFpuRegG )
	return(FPU_REG_AVAILABLE);
  
  // force register compare
  compareCpuReg();
  compareFpuReg();

  return(CPU_REG_AVAILABLE);
}

/**************************/
int	allocCompReg(int newReg,int codeType)
{
	register int i;
	int regOffset;
	
	for ( i = 0 ; i < maxCompRegG ; i++)
		if ( compRegValidG[i] == FALSE ) 
			break;

	if ( newReg == REUSE_REG && i != 0 && dbl[i-1] == FALSE && mipsMode[i-1] == codeType )
		return(i - 1 + baseCompRegG);

	  /* change for T5. */
	if ( i != maxCompRegG ) {

		recycleCountG++;
		compRegValidG[i] = TRUE;
		mipsMode[i]	 = codeType;

		if ( codeType == MIPSII ) {
				dbl[i]= FALSE;
				compReg64[i] = 0x12345678;
				printf("	li	r%-2d,0x%08x\n",baseCompRegG+i,0x12345678);
		}
		else
			dbl[i]= TRUE;

		return(i + baseCompRegG);

	} else if ( (findSameType(codeType) != -1 ) && ((recycleCountG < recycleLimitG ) || (recycleLimitG == 0)) ) {
		recycleCountG++;
		regOffset = findSameType(codeType);
		return ( baseCompRegG + regOffset );
	}
	else if ( recycleLimitG == 0 ) {
	          i = random()%maxCompRegG;
		  mipsMode[i]	   = codeType;
	          compRegValidG[i] = TRUE;
		  dbl[i]           = FALSE;
		  /* sign extent destination register if we may have 64 bit data in registers */
		  if ( mips2G == FALSE && codeType == MIPSII )  {
		      if ( compReg64[i] & 0x080000000 ) 
			  compReg64[i] |= 0xffffffff00000000;
		     printf("	dsll	r%-2d,32\n",baseCompRegG+i);
		     printf("	dsra	r%-2d,32\n",baseCompRegG+i);
	     	  } 

	}
	else
	 {
		/* reset recycle count */
		recycleCountG = 0;
		printf("/* reg comp: */\n");
		checkPrefetchAndSync();
		tmpReg1G = 30;
		for ( i = 0 ; i < maxCompRegG ; i++) {
		    printf("	dli	r%-2d,\"%016llx\"\n", tmpReg1G, compReg64[i]);
		    printf("	xor	r%-2d,r%-2d\n", tmpReg1G, baseCompRegG + i);
		    if ( onHardwareG == TRUE ) {
		      printf("     	bnel	r%-2d,r0,1f\n", tmpReg1G);
		      printf("     	li	r29,%-2d\n", baseCompRegG + i);
		    } else {
		      printf("     	bne	r%-2d,r0,1f\n", tmpReg1G);
		    }
		    compRegValidG[i] = FALSE;
		}
		printf("	nop\n");
		printf("	beq r0,r0,2f\n");
		printf("	nop\n");
		if ( onHardwareG == TRUE ) {
		    printf("1:	dla	r2,iterationCount	\n");
		    printf("	lw	r28,0(r2)		\n");
		}
		printf("1:	halt(FAIL)\n");
		printf("2:\n");

		compRegValidG[0] = TRUE;
		dbl[0]           = TRUE; 
		mipsMode[0]	 = codeType;

		if ( codeType == MIPSII ) {
				dbl[0]= FALSE;
				compReg64[0] = 0x12345678;
				printf("	li	r%-2d,0x%08x\n",baseCompRegG+0,0x12345678);
		}
		else
			dbl[0]= TRUE;

		return(0 + baseCompRegG);

	}
	if ( doAtomicG ) printf("#ATOMIC#\n");
}
/************/
/* load up an expected value to compare later, if full then dump values */
int	allocCompRegFilD(ULL value,int codeType)
{
    int	i;
    int regOffset;


    for ( i = 0 ; i < maxCompRegG ; i++)
	if ( compRegValidG[i] == FALSE ) 
	    break;

	if ( i != maxCompRegG ) {
		recycleCountG++;
		compReg64[i]     = value;
		compRegValidG[i] = TRUE;
		dbl[i]           = TRUE;
		mipsMode[i]      = codeType;

		return(i + baseCompRegG);

	} else if ( (findSameType(codeType) != -1 ) && ((recycleCountG < recycleLimitG ) || (recycleLimitG == 0)) ) {
		recycleCountG++;
		regOffset = findSameType(codeType);
		compReg64[regOffset] = value;
		return ( baseCompRegG + regOffset);
	}
	else if ( recycleLimitG == 0 ) {
	          i = random()%maxCompRegG;
		  mipsMode[i]	   = codeType;
	          compRegValidG[i] = TRUE;
		  dbl[i]           = FALSE;
		  // sign extent destination register if we may have 64 bit data in registers 
		  if ( mips2G == FALSE && codeType == MIPSII )  {
		     compReg64[i] |= ( compReg64[i] & 0x080000000 ) ? 0xffffffff00000000 : 0x0;
		     printf("	dsll	r%-2d,32\n",baseCompRegG+i);
		     printf("	dsra	r%-2d,32\n",baseCompRegG+i);
	     	  } 
	} else {
		recycleCountG = 0;
		printf("/* reg comp: */\n");
		checkPrefetchAndSync();
		tmpReg1G = 30;
		for ( i = 0 ; i < maxCompRegG ; i++) {
		    printf("	dli	r%-2d,\"%016llx\"\n", tmpReg1G, compReg64[i]);
		    printf("	xor	r%-2d,r%-2d\n", tmpReg1G, baseCompRegG + i);
		    if ( onHardwareG == TRUE ) {
		      printf("     	bnel	r%-2d,r0,1f\n", tmpReg1G);
		      printf("     	li	r29,%-2d\n", baseCompRegG + i);
		    } else {
		      printf("     	bne	r%-2d,r0,1f\n", tmpReg1G);
		    }
		    compRegValidG[i] = FALSE;
		}

		printf("	nop\n");
		printf("	beq r0,r0,2f\n");
		printf("	nop\n");

		if ( onHardwareG == TRUE ) {
			printf("1:	dla	r2,iterationCount	\n");
			printf("	lw	r28,0(r2)		\n");
		}	
		printf("1:	halt(FAIL)\n");

		printf("2:\n");

		compReg64[0]  = value;

		compRegValidG[0] = TRUE;
		dbl[0]           = TRUE;
		mipsMode[0]        = codeType;
		return(0 + baseCompRegG);

	}
	if ( doAtomicG ) printf("#ATOMIC#\n");

	// this is dummy return , we have already returned.
return(0);
}

/************/
int	allocCompRegFil(ULL value, int codeType)
{
	int	i;
	int     regOffset;

	// find an empty register
	for ( i = 0 ; i < maxCompRegG ; i++)
		if ( compRegValidG[i] == FALSE ) 
			break;
	if ( i != maxCompRegG ) {
		recycleCountG++;

		compReg64[i]       = value;
		compRegValidG[i]   = TRUE;
		dbl[i]             = FALSE; // leave this in for now
		mipsMode[i]	   = codeType;

		return(i + baseCompRegG);
 
	} else if ( (findSameType(codeType) != -1 ) && ((recycleCountG < recycleLimitG ) || (recycleLimitG == 0)) ) {
		recycleCountG++;
		regOffset = findSameType(codeType);
		compReg64[regOffset] = value;
		return ( baseCompRegG + regOffset);
	}
	else if ( recycleLimitG == 0 ) {
	          i = random()%maxCompRegG;
		  mipsMode[i]	   = codeType;
	          compRegValidG[i] = TRUE;
		  dbl[i]           = FALSE;
		  // sign extent destination register, it may have had 64 bit data in the register
		  if ( mips2G == FALSE && codeType == MIPSII )  {
		     compReg64[i] |= ( compReg64[i] & 0x080000000 ) ? 0xffffffff00000000 : 0x0;
		     printf("	dsll	r%-2d,32\n",baseCompRegG+i);
		     printf("	dsra	r%-2d,32\n",baseCompRegG+i);
	     	  } 
	} else {
	    // if all else fails, we have to dump registers
		recycleCountG = 0;
		printf("/* reg comp: */\n");
		checkPrefetchAndSync();
		tmpReg1G = 30;
		for ( i = 0 ; i < maxCompRegG ; i++) {
		    printf("	dli	r%-2d,\"%016llx\"\n", tmpReg1G, compReg64[i]);
		    printf("	xor	r%-2d,r%-2d\n", tmpReg1G, baseCompRegG + i);
		    if ( onHardwareG == TRUE ) {
		      printf("     	bnel	r%-2d,r0,1f\n", tmpReg1G);
		      printf("     	li	r29,%-2d\n", baseCompRegG + i);
		    } else {
		      printf("     	bne	r%-2d,r0,1f\n", tmpReg1G);
          	    }
		    compRegValidG[i] = FALSE;
		}

		printf("	nop\n");
		printf("	beq r0,r0,2f\n");
		printf("	nop\n");

		if ( onHardwareG == TRUE ) {
			printf("1:	dla	r2,iterationCount	\n");
			printf("	lw	r28,0(r2)		\n");
	        }
		printf("1:	halt(FAIL)\n");
		printf("2:\n");

		compReg64[0]     = value;
		compRegValidG[0] = TRUE;
		dbl[0]           = FALSE;
		mipsMode[0]	 = codeType;
		return(0 + baseCompRegG);

	}
	if ( doAtomicG ) printf("#ATOMIC#\n");

 return(0);
}


int	allocCompFpuRegFil(ULL value, int dataType)
{
	int	i;

	// find an empty register
	for ( i = 0 ; i < maxCompFpuRegG ; i++)
		if ( compFpuRegValidG[i] == FALSE ) 
			break;

	// keep it simple either we 
	if ( i != maxCompFpuRegG ) {

		compFpuReg64[i]      = value;
		compFpuRegValidG[i]  = TRUE;
		fpuDataType[i]	     = dataType;

		return(i + baseCompFpuRegG);
 

	} else {
	    // if all else fails, we have to dump registers
		recycleCountG = 0;
		printf("/* fpu reg comp: */\n");

		tmpReg1G = TMP_REG_1;
		tmpReg2G = TMP_REG_2;

		for ( i = 0 ; i < maxCompFpuRegG ; i++) {

		  if ( fpuDataType[i] == DATA64 ) {

		    printf("\tdmfc1      r%-2d,fp%-2d\n",tmpReg1G,baseCompFpuRegG+i);
		    printf("\tdli	r%-2d,\"%016llx\"\n", tmpReg2G, compFpuReg64[i]);
		    if ( onHardwareG == TRUE ) {
		      printf("\tbnel	r%-2d,r%-2d,1f\n", tmpReg1G, tmpReg2G);
		      printf("\tli	r29,%-2d\n", baseCompFpuRegG + i + 32);
		    } else {
		      printf("\tbne	r%-2d,r%-2d,1f\n",tmpReg1G,tmpReg2G);
          }
		    compFpuRegValidG[i] = FALSE;
		  }
		  else { 

		    printf("\tmfc1      r%-2d,fp%-2d\n",tmpReg1G,baseCompFpuRegG+i);
		    printf("\tdli	r%-2d,\"%016llx\"\n", tmpReg2G, compFpuReg64[i]);
		    if ( onHardwareG == TRUE ) {
		      printf("\tbnel	r%-2d,r%-2d,1f\n", tmpReg1G, tmpReg2G);
		      printf("\tli	r29,%-2d\n", baseCompFpuRegG + i + 32);
		    } else {
		      printf("\tbne	r%-2d,r%-2d,1f\n",tmpReg1G,tmpReg2G);
          }
		    compFpuRegValidG[i] = FALSE;
		  }

		}

		printf("	nop\n");
		printf("	beq r0,r0,2f\n");
		printf("	nop\n");

		if ( onHardwareG == TRUE ) {
			printf("1:	dla	r2,iterationCount	\n");
			printf("	lw	r28,0(r2)		\n");
	        }
		printf("1:	halt(FAIL)\n");
		printf("2:\n");

		compFpuReg64[0]     = value;
		compFpuRegValidG[0] = TRUE;
		fpuDataType[0]	    = dataType;
		return(0 + baseCompFpuRegG);

	}
	if ( doAtomicG ) printf("#ATOMIC#\n");

}
//

void allocStoreReg()
{

  int	i;
  int	selectedBaseIndex;

  UINT	addressMode = 0;

  UINT	storeAddress;
  UINT	tempRand;
  int	offsetBase;
  int 	randomOverride = FALSE;

  struct cell *tmpPtr;

     printf("\t/* base registers */\n");

    if ( (random()% cacheWrapRateG ) == 0 ) 
	randomOverride = TRUE;
    	
    for ( i = 0 ; i < maxBaseRegG ; i++) { 	/* generate double word address within the range specified*/

      addressMode = 0;

      if ( random()%2 ) { // cached
	addressMode |= CACHED_ADDR;
	if ( randomOverride == TRUE )
          addressMode = UNMAPPED_ADDR | CACHED_ADDR | CACHE_WRAP;
      }
      else // uncached
	  addressMode |=  UNCACHED_ADDR;

      if ( addressMode & UNCACHED_ADDR ) 
	addressMode |= UNMAPPED_ADDR;
      else
	addressMode |= ( random()%2 ) ? UNMAPPED_ADDR : MAPPED_ADDR;

      addressMode |= ( random()%cacheWrapRateG ) ? CACHE_NO_WRAP : CACHE_WRAP;


	// set how often to recycle used memory

	if (!(random()% memoryModel.memReuseRate) )
	      addressMode |= USED_MEM;

	// if sn0 mode, use 64 bit address space

	//if ( doSN0M == TRUE || doSN0N == TRUE )
	//addressMode |= SPACE64_ADDR;
	//else
	//addressMode |= SPACE32_ADDR;

	// first do sanity checking to make sure we are allowed to do the selected
 	// address mode , if not, then adjust addressMode.


	if ( useTLB == FALSE ) 
	  addressMode &= ~MAPPED_ADDR_MASK;
	
	if ( noUncachedG == TRUE ) 
	  addressMode &= ~CACHE_MASK;

	if (cacheWrapG == FALSE ) 
	  addressMode &= ~CACHE_WRAP;
		
        if ( reverseEndianG == TRUE || IkernelG == FALSE ) 
	  addressMode |= MAPPED_ADDR;
        
	// for all of these we wish to pick offset

	addressMode |= SEL_OFFSET;

	ULL basePhysAddr     = memoryModel.pickPhysAddr( curCpuIdG  , addressMode );
	ULL baseVirtualAddr  = memoryModel.mapPhysAddr( basePhysAddr, addressMode );

	if ( doMP == TRUE ) {
	  baseReg64[i]       = ( baseVirtualAddr & MPaddrMask64 ) | MPwordG;
	  baseRegPhysAddr[i] = ( basePhysAddr    & MPaddrMask64 ) | MPwordG;
	}
	else {
	  baseReg64[i]       =  baseVirtualAddr & 0xfffffffffffffff8LL;
	  baseRegPhysAddr[i] =  basePhysAddr    & 0xfffffffffffffff8LL;
	}

	printf("	dli	r%-2d,\"%016llx\"  /* physical address %016llx */\n",
	       baseRegG + i, baseReg64[i],baseRegPhysAddr[i]);


	baseAddrMode[i] = addressMode;

  } /* end of for loop */

   if ( doAtomicG ) printf("#ATOMIC#\n");
}



/************/
void dumpRegs()
{
	int	i;

	printf("\n\n /*Register Dump:\n");
	for ( i = 0 ; i < maxBaseRegG ; i++)
	    printf("STORE REG: R%2d VALUE: 0x%016llx\n", i + baseRegG, baseReg64[i]);
	    /*printf("STORE REG: R%2d VALUE: 0x%08x\n", i + baseRegG, baseRegLo[i]);*/
	for ( i = 0 ; i < maxBaseRegG ; i++)
		if (loadRegValidG[i] == TRUE )
			printf(" LOAD REG: R%2d VALUE: 0x%016llxx 0x%016llx \n", 
			i + baseRegG,baseReg64[i], baseRegPhysAddr[i]);
	for ( i = 0 ; i < maxCompRegG ; i++)
		if (compRegValidG[i] == TRUE )
				printf(" COMP REG: R%2d VALUE: 0x%016llx 0x%016llx\n", 
				i + baseCompRegG, compReg64[i], baseRegPhysAddr[i]);
	printf("*/\n");

   if ( doAtomicG ) printf("#ATOMIC#\n");
}


void lb_inst() {

 Cell *tmpPtr;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL	loadValue  = 0;

  if ( fpRepeatCounterG  > 0 ) 
    fpRepeatCounterG--;

  Cell * tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;

  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0-7 are valid */
  offset = random();

  if ( doMP == TRUE )
    offset = offset % 4;
  else
    offset = offset % 8;
 
  found=lb(baseRegPhysAddr[regSel] + offset, &loadValue,baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompRegFil(loadValue,MIPSII);
  printf("  	lb  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}
void lh_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;

  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,2,4,6 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = (offset % 4) & 0xfffffffe;
  else
    offset = (offset % 8) & 0xfffffffe;

  found=lh(baseRegPhysAddr[regSel] +  offset, &loadValue,TRUE,baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompRegFil(loadValue,MIPSII);
  printf("  	lh  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void lhu_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( fpRepeatCounterG > 0 )
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL )
    return;

  regSel = random();
  regSel = regSel % maxBaseRegG;


  /* randomly select the offset 0,2,4,6 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = (offset % 4) & 0xfffffffe;
  else
			offset = (offset % 8) & 0xfffffffe;
  
  found=lh(baseRegPhysAddr[regSel] +  offset, &loadValue,FALSE,baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompRegFil(loadValue,MIPSII);
  printf("        lhu      r%-2d,0x%-x(r%-2d)\n", tmpReg2G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void lw_inst() {


 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0, 4 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;

  found=lw(baseRegPhysAddr[regSel] +  offset, &loadValue,baseAddrMode[regSel]);
  if ( found == NULL ) 
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompRegFil(loadValue,MIPSII);
	
  printf("  	lw  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G,  offset, baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void lwc1_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel,randReg;
 UINT offset;
 ULL  loadValue  = 0;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;

  regSel = random();
  regSel = regSel % maxBaseRegG;
  
  /* randomly select the offset 0, 4 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;

  found=lw(baseRegPhysAddr[regSel] +  offset, &loadValue,baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompFpuRegFil(loadValue,DATA32);
	
  printf("  	lwc1  	fp%-2d,0x%-x(r%-2d)\n",tmpReg2G ,  offset, baseRegG + regSel);
  
  if ( doAtomicG ) printf("#ATOMIC#\n");
}
void lwxc1_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel,randReg;
 UINT offset;
 ULL  loadValue  = 0;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;

  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0, 4 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;

  found=lw(baseRegPhysAddr[regSel] +  offset, &loadValue,baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompFpuRegFil(loadValue,DATA32);
	
  printf("  	ori	r28,r0,0x%-x\n",  offset);
  printf("  	lwxc1  	fp%-2d,r28(r%-2d)\n",tmpReg2G , baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void lwu_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( fpRepeatCounterG > 0 )
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL )
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;


  /* randomly select the offset 0, 4 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;

  found=lw(baseRegPhysAddr[regSel] + offset, &loadValue,baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  // we know lw routine may sign extended, we want it not to!!
  loadValue &= 0x00000000ffffffff;
  tmpReg2G = allocCompRegFilD(loadValue,MIPSIII);
  printf("        lwu      r%-2d,0x%-x(r%-2d)\n", tmpReg2G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void lwl_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr(); 
  if (tmpPtr1 == NULL ) 
    return;

  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0-7 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = (offset % 4);
  else
    offset = (offset % 8);

  /* grab a register */

  tmpReg2G = allocCompReg(REUSE_REG,MIPSII);

  /* merge data with current register if needed */
  int newCell = FALSE; 
  found=lwl(baseRegPhysAddr[regSel] +  offset, &compReg64[tmpReg2G-baseCompRegG],baseAddrMode[regSel], &newCell);
  if ( newCell == TRUE )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  dbl[tmpReg2G-baseCompRegG]      = FALSE;
  mipsMode[tmpReg2G-baseCompRegG] = MIPSII;

  printf("  	lwl  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void lwr_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr(); 
  if (tmpPtr1 == NULL ) 
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0-7 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = (offset % 4);
  else
    offset = (offset % 8);

  /* grab a register */

  tmpReg2G = allocCompReg(REUSE_REG,MIPSII);

  /* merge data with current register if needed */
  int newCell = FALSE;
  found=lwr(baseRegPhysAddr[regSel] + offset, &compReg64[tmpReg2G-baseCompRegG],baseAddrMode[regSel],&newCell);
  if ( newCell == TRUE ) 
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  dbl[tmpReg2G-baseCompRegG] = FALSE;
  mipsMode[tmpReg2G-baseCompRegG] = MIPSII;

  printf("  	lwr  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void ld_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
  if ( mips2G == TRUE ) 
    return;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4 are valid */
  offset = 0;

  loadValue = 0;

  found=ld(baseRegPhysAddr[regSel] + offset, &loadValue, baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompRegFilD(loadValue,MIPSIII);
  
  printf("  	ld  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G, offset, baseRegG + regSel);


  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void ldc1_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
  if ( mips2G == TRUE ) 
    return;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4 are valid */
  offset = 0;
  loadValue = 0;

  found=ld(baseRegPhysAddr[regSel] + offset, &loadValue, baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompFpuRegFil(loadValue,DATA64);

  printf("  	ldc1  	fp%-2d,0x%-x(r%-2d)\n", tmpReg2G, offset, baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void lld_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
  if ( doLlScG == FALSE ) return;
  if ( mips2G == TRUE ) return;

  if ( fpRepeatCounterG > 0 )
                        fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL )
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;

  if ( ( baseAddrMode[regSel] & CACHE_MASK) == UNCACHED_ADDR ) 
    return;

  /* randomly select the offset 0,4 are valid */
  offset = 0;

  loadValue = 0;

  found=ld(baseRegPhysAddr[regSel] + offset, &loadValue, baseAddrMode[regSel]);
  if ( found == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  tmpReg2G = allocCompRegFilD(loadValue,MIPSIII);
  if (NopsBeforeLL) {
    int i;
    for (i = 0; i < 32; i++) printf("  nop\n");
  }
  printf("        lld      r%-2d,0x%-x(r%-2d)\n", tmpReg2G, offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void ldl_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
  if ( mips2G == TRUE ) 
    return;
  
  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;
  
  /* randomly select the offset 0-7 are valid */
  offset = random();
  offset = (offset % 8);

  /* grab a register */
  
  tmpReg2G = allocCompReg(REUSE_REG,MIPSIII);

  /* merge data with current register if needed */
   
  int newCell = FALSE;
  found=ldl(baseRegPhysAddr[regSel] + offset, &compReg64[tmpReg2G-baseCompRegG], baseAddrMode[regSel], &newCell);
  if ( newCell == TRUE )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	ldl  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G, offset, baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void ldr_inst() {
   Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
  if ( mips2G == TRUE ) 
    return;
  
  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  tmpPtr1 = findRandAddr();
  if (tmpPtr1 == NULL ) 
    return;
  regSel = random();
  regSel = regSel % maxBaseRegG;
  
  /* randomly select the offset 0-7 are valid */
  offset = random();
  offset = (offset % 8);

  /* grab a register */

  tmpReg2G = allocCompReg(REUSE_REG,MIPSIII);

  /* merge data with current register if needed */
  int  newCell = FALSE;
  found=ldr(baseRegPhysAddr[regSel] + offset, &compReg64[tmpReg2G-baseCompRegG], baseAddrMode[regSel], &newCell);
  if ( newCell == TRUE )
      printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	ldr  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G, offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}


void hitWriInvD_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( noPrimaryCacheG == TRUE ) 
    return;

  if ( noCacheOpsG == TRUE ) 
    return;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  regSel = (random()) % maxBaseRegG;

  if ( ( baseAddrMode[regSel] & CACHE_MASK ) == UNCACHED_ADDR ) 
    return;

  /* randomly select the offset 0,4 are valid */
  offset = ((random()) % 8) & 0xfffffffc;

  if ( forICE == FALSE ) {
    printf("	nop\n");
    printf("	nop\n");
  }	
  printf("  	cache Hit_Writeback_Inv_D,0x%-x(r%-2d)\n",  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void hitWriInvSD_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( noCacheOpsG == TRUE ) 
    return;

  if ( noSecondCacheG == TRUE ) 
    return;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* reduce the chances of getting this one */
  regSel = random();
  regSel = regSel % 8;
  if ( regSel != 0 ) return;

  regSel = random();
  regSel = regSel % maxBaseRegG;

  if ( ( baseAddrMode[regSel] & CACHE_MASK) == UNCACHED_ADDR ) 
    return;

  offset = 0;
  if ( (random()%flushAllRegRateG)  == 0 ) {
    for ( int i = 0 ; i < maxBaseRegG ; i++ )
      printf("  	cache Hit_Writeback_Inv_SD,0x%-x(r%-2d)\n",  offset, baseRegG + i);
  }
  else    
				printf("  	cache Hit_Writeback_Inv_SD,0x%-x(r%-2d)\n",  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void indexWriInvD_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( noPrimaryCacheG == TRUE ) 
    return;

  if ( noCacheOpsG == TRUE ) 
    return;


  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  regSel = random();
  regSel = regSel % maxBaseRegG;

  if ( ( baseAddrMode[regSel] & CACHE_MASK) == UNCACHED_ADDR ) 
    return;

  /* randomly select the offset 0,4 are valid */
  offset = random();
  offset = (offset % 8) & 0xfffffffc;


  if ( forICE == FALSE ) {
    printf("	nop\n");
    printf("	nop\n");
  }
  printf("  	cache Index_Writeback_Inv_D,0x%-x(r%-2d)\n", offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}


void indexWriInvSD_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;

  if ( noCacheOpsG == TRUE ) 
    return;

  if ( noSecondCacheG == TRUE ) 
    return;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* reduce the chances of getting this one */
  regSel = random();
  regSel = regSel % 8;
  if ( regSel != 0 ) 
    return;

  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4 are valid */
  offset = random();
  offset = (offset % 8) & 0xfffffffc;

  if ( ( baseAddrMode[regSel] & CACHE_MASK) == UNCACHED_ADDR ) 
    return;

  offset = 0;

  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void sb_inst(){
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL     storeValue;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;
  
  /* randomly select the offset 0-7 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = offset % 4;
  else
    offset = offset % 8;
  
  randReg    = random();
  tmpReg1G   = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  
  Cell * newCell = sb(baseRegPhysAddr[regSel] + offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
  printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	sb  	r%-2d,0x%-x(r%-2d)\n", tmpReg1G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}
void sh_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL     storeValue;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;
  
  /* randomly select the offset 0,2,4,6 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = (offset % 4) & 0xfffffffe;
  else
    offset = (offset % 8) & 0xfffffffe;

  randReg    = random();
  tmpReg1G   = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];

  Cell * newCell = sh(baseRegPhysAddr[regSel] +  offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	sh  	r%-2d,0x%-x(r%-2d)\n", tmpReg1G,  offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}
void sw_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 UINT randReg;

 ULL  loadValue  = 0;
 ULL  storeValue;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;

  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell = sw(baseRegPhysAddr[regSel] +  offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	sw  	r%-2d,0x%-x(r%-2d)\n", tmpReg1G,  offset, baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");
}

///////////

void swc1_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL     storeValue;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;

  randReg    = random();
  tmpReg1G   = baseImmdFpuRegG + (randReg % maxImmdFpuRegG);
  storeValue = immdFpuReg64[randReg%maxImmdFpuRegG];

  Cell * newCell = swc1(baseRegPhysAddr[regSel] +  offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL ) 
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	swc1  	fp%-2d,0x%-x(r%-2d)\n", tmpReg1G,  offset, baseRegG + regSel);

   if ( doAtomicG ) printf("#ATOMIC#\n");
}

void swxc1_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 UINT randReg;

 ULL  loadValue  = 0;
 ULL  storeValue;


 if ( mips4G != TRUE ) return;

 // randomly select the base register
  regSel = random();
  regSel = regSel % maxBaseRegG;

  // randomly select the offset 0,4are valid 
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;

  randReg    = random();
  tmpReg1G   = baseImmdFpuRegG + (randReg % maxImmdFpuRegG);
  storeValue = immdFpuReg64[randReg%maxImmdFpuRegG];

  Cell * newCell =  swc1(baseRegPhysAddr[regSel] +  offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("        ori   r28,r0,0x%-x\n",offset);
  printf("  	swxc1  	fp%-2d,r28(r%-2d)\n",tmpReg1G, baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");
}

//

void sc_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL     storeValue;

  if ( doLlScG == FALSE ) return;
  if ( fpRepeatCounterG > 0 )
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  if ( ( baseAddrMode[regSel] & CACHE_MASK) == UNCACHED_ADDR ) 
    return;

  /* randomly select the offset 0,4are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = 0;
  else
    offset = (offset % 8) & 0xfffffffc;
  
  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell = sw(baseRegPhysAddr[regSel] +  offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("/* %x %016llxx */ \n", baseRegPhysAddr[regSel],baseReg64[regSel]);
  printf("	or	r28,r%-2d,r0\n",tmpReg1G);
  printf("1:\n");
  if (NopsBeforeLL) {
    int i;
    for (i = 0; i < 32; i++) printf("  nop\n");
  }
  printf("        ll      r0,0x%-x(r%-2d)\n",  offset, baseRegG + regSel);
  printf("        sc      r%-2d,0x%-x(r%-2d)\n", tmpReg1G,  offset, baseRegG + regSel);
  if (NopsAfterSC) {
    int i;
/*
    printf("	bne 	r0,r%-2d,1f\n",tmpReg1G);
*/
    for (i = 0; i < 32; i++) printf("  nop\n");
  }
  if (COP1AfterSC) {
    printf("   cfc1 r0,C1_SR    \n");
  }
  if (SCBranchLikely) {
    printf("	beql	r0,r%-2d,1b\n",tmpReg1G);
    printf("	or	r%-2d,r28,r0\n",tmpReg1G);
    printf("	or	r%-2d,r28,r0\n",tmpReg1G);
  } else {
    printf("	beq 	r0,r%-2d,1b\n",tmpReg1G);
    printf("	or	r%-2d,r28,r0\n",tmpReg1G);
  }
  printf("1:\n");
  if ( doAtomicG ) printf("#ATOMIC#\n");
  
}

void swl_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL     storeValue;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = (offset % 4);
  else
    offset = (offset % 8);

  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell = swl(baseRegPhysAddr[regSel] +  offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	swl 	r%-2d,0x%-x(r%-2d)\n", tmpReg1G, offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void swr_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL     storeValue;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,7 are valid */
  offset = random();
  if ( doMP == TRUE )
    offset = (offset % 4);
  else
    offset = (offset % 8);

  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell =swr(baseRegPhysAddr[regSel] + offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	swr 	r%-2d,0x%-x(r%-2d)\n", tmpReg1G, offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void sdl_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL   storeValue;

  if ( mips2G == TRUE ) 
    return;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4are valid */
  offset = (random()) % 8;

  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell = sdl(baseRegPhysAddr[regSel] + offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL ) 
     printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("        sdl      r%-2d,0x%-x(r%-2d)\n", tmpReg1G, offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}
void sdr_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL  storeValue;

  if ( mips2G == TRUE ) 
    return;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  /* randomly select the offset 0,4are valid */
  offset = (random()) % 8;

  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell = sdr(baseRegPhysAddr[regSel] + offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL ) 
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("        sdr      r%-2d,0x%-x(r%-2d)\n", tmpReg1G, offset, baseRegG + regSel);
  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void sd_inst() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL  storeValue;

  if ( mips2G == TRUE ) 
    return;

  if ( fpRepeatCounterG > 0 ) 
    fpRepeatCounterG--;

  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;

  offset = 0;
  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell = sd(baseRegPhysAddr[regSel]+ offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	sd  	r%-2d,0x%-x(r%-2d)\n", tmpReg1G, offset, baseRegG + regSel);
}


void burst_sd_inst( UINT virtualOffset, UINT inc ) {

 Cell *tmpPtr1;
 Cell *found;
 
 // we are using first base register for burst mode
 UINT regSel    = 0;
 UINT randReg   = 0;
 ULL  loadValue = 0;
 ULL  storeValue= 0;;

 UINT fakeOffset = 0;

  if ( mips2G == TRUE ) 
    return;

  if ( checkDestRegStatus() == CPU_REG_AVAILABLE ) {  
    randReg = random();
    tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
    storeValue = immdReg64[randReg%maxImmdRegG];
    Cell * newCell = sd(baseRegPhysAddr[regSel]+ virtualOffset, storeValue, baseAddrMode[regSel]);
    printf("\t sd  	r%-2d,0x%-x(r%-2d)\n", tmpReg1G, fakeOffset, baseRegG + regSel);
    printf("\t daddiu  r%-2d,r%-2d,%d \n", baseRegG + regSel, baseRegG + regSel,inc);
  }
  else {
    randReg    = random();
    tmpReg1G   = baseImmdFpuRegG + (randReg % maxImmdFpuRegG);
    storeValue = immdFpuReg64[randReg%maxImmdFpuRegG];
    Cell * newCell = sdc1(baseRegPhysAddr[regSel] + virtualOffset, storeValue, baseAddrMode[regSel]);
    printf("\t sdc1  	fp%-2d,0x%-x(r%-2d)\n", tmpReg1G, fakeOffset, baseRegG + regSel);
    printf("\t daddiu  r%-2d,r%-2d,%d \n", baseRegG + regSel, baseRegG + regSel,inc);

  }
  if ( doAtomicG ) printf("#ATOMIC#\n");
}

void burst_sd_loop_inst( UINT count, UINT constInc ) {

 Cell *tmpPtr1;
 Cell *found;

 
 // we are using first base register for burst mode
 UINT regSel    = 0; // we are using first base register for this
 ULL  loadValue = 0;
 UINT randReg;

 ULL  simStoreValue;
 ULL  simStoreAddr;

 ULL  prevStoreValue;

  if ( mips2G == TRUE ) 
    return;

  UINT offset = 0;

  // select random base register
  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);

  // update internal model
  prevStoreValue = simStoreValue = immdReg64[randReg%maxImmdRegG];
  simStoreAddr  = baseRegPhysAddr[regSel];

  Cell * newCell = sd(simStoreAddr,simStoreValue, baseAddrMode[regSel]);

  // first one done

  // now loop, we dont need to zero out values because we are doing sd
  // which makes sure previous value for prev. run has been deleted.
  for ( int i = 1 ; i < count ; i++ ) {
    simStoreValue += constInc;
    simStoreAddr  += burstDeltaG;
    newCell = sd(simStoreAddr, simStoreValue, baseAddrMode[regSel]);
  }

  printf("/* burst loop */ \n");
  printf("\t dli    r%-2d,%d \n",TMP_REG_1,count-1);
  printf("3: \n");
  printf("\t sd  	r%-2d,0x%-x(r%-2d)\n", tmpReg1G, offset, baseRegG + regSel);
  printf("\t daddiu  r%-2d,r%-2d,%d \n",baseRegG + regSel, baseRegG + regSel,burstDeltaG);
  printf("\t daddiu  r%-2d,r%-2d,%d \n",tmpReg1G,tmpReg1G,constInc);
  printf("\t bne     r%-2d,r%-2d,3b \n",TMP_REG_1,0);
  printf("\t daddiu  r%-2d,r%-2d,%d \n",TMP_REG_1, TMP_REG_1,-1);

  immdReg64[randReg%maxImmdRegG] =  prevStoreValue;

  printf("/* restore immd value */ \n");
  printf("\t dli	r%-2d,\"%016llx\" \n",
	tmpReg1G,prevStoreValue);

   printf("/* restore reg to prior to looping */ \n");
   printf("	dli	r%-2d,\"%016llx\"  /* physical address %016llx */\n",
	  baseRegG + regSel, baseReg64[regSel],baseRegPhysAddr[regSel]);

  if ( doAtomicG ) printf("#ATOMIC#\n");
}

/////

void burst_prefetch() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL  storeValue;
 int hint;

 // only called by burst mode feature, T5 or greater
 //if (  mips4G == FALSE ) return; 

  printf("      /* label 90 reserved for burst prefetch */ \n");  
  printf("\t dla	r28,90f \n");

  hint = 0;

  switch ( random() % 3 ) {
  case 0:
    printf("\t pfetch	%d,r28(r0)\n",hint);
    if ( doAtomicG ) printf("#ATOMIC#\n");
    return;
  case 1:
    printf("\t pfetch	%d,r0(r28)\n",hint);
    if ( doAtomicG ) printf("#ATOMIC#\n");
    return;
  case 2:
    printf("\t pref	%d,0(r28)\n",hint);
    if ( doAtomicG ) printf("#ATOMIC#\n");
    return;
  }
}

//
// we are going to cheat here, pass offset internal memory model
// to keep track of where we are, but use inc to add to the base.
// this allows us to get out of the 16Hex digit limit.

void burst_ld_inst( UINT virtualOffset, UINT inc ) {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel     = 1;
 ULL  loadValue  = 0;
 UINT fakeOffset = 0;

  if ( mips2G == TRUE ) 
    return;


  found=ld(baseRegPhysAddr[regSel] + virtualOffset, &loadValue, baseAddrMode[regSel]);

  if ( found == NULL )
    printf("\t sd  	r0,0x%-x(r%-2d)\n", fakeOffset&0xfff8, baseRegG + regSel);

  if ( checkDestRegStatus() == CPU_REG_AVAILABLE ) {
    tmpReg2G = allocCompRegFilD(loadValue,MIPSIII);
    printf("\t ld  	r%-2d,0x%-x(r%-2d)\n", tmpReg2G, fakeOffset, baseRegG + regSel);
    printf("\t daddiu  r%-2d,r%-2d,%d \n", baseRegG + regSel, baseRegG + regSel,inc);
  }
  else {
    tmpReg2G = allocCompFpuRegFil(loadValue,DATA64);
    printf("\t ldc1  	fp%-2d,0x%-x(r%-2d)\n", tmpReg2G, fakeOffset, baseRegG + regSel);
    printf("\t daddiu  r%-2d,r%-2d,%d \n", baseRegG + regSel, baseRegG + regSel,inc);
  }

  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void burstCacheOP ( UINT count, UINT constInc ) {

 Cell *tmpPtr1;
 Cell *found;

 
 // we are using first base register for burst mode
 UINT regSel    = 0; // we are using first base register for this
 ULL  loadValue = 0;
 UINT randReg;

 ULL  simStoreValue;
 ULL  simStoreAddr;

 ULL  prevStoreValue;

  if ( mips2G == TRUE ) 
    return;

  UINT offset = 0;

  // select random base register
  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);

  // update internal model
  simStoreAddr  = baseRegPhysAddr[regSel];
  printf("\t dli    r%-2d,%d \n",TMP_REG_1,count-1);
  printf("3: \n");
  printf("\t cache Hit_Writeback_Inv_D,0x%-x(r%-2d)\n", offset, baseRegG + regSel);
  printf("\t daddiu  r%-2d,r%-2d,%d \n",baseRegG + regSel, baseRegG + regSel,burstDeltaG);
  printf("\t bne     r%-2d,r%-2d,3b \n",TMP_REG_1,0);
  printf("\t daddiu  r%-2d,r%-2d,%d \n",TMP_REG_1, TMP_REG_1,-1);

  printf("\t /* restore value after looping */\n");
  printf("\t dli	r%-2d,\"%016llx\"  /* physical address %016llx */\n",
	baseRegG + regSel, baseReg64[regSel],baseRegPhysAddr[regSel]);

  if ( doAtomicG ) printf("#ATOMIC#\n");
}
//

void doBurst() {

 UINT addrMode;

 if ( (random()%burstRateG) != 0 ) return;

 ULL  prevBase0PAddr = baseRegPhysAddr[0];
 ULL  prevBase0Addr  = baseReg64[0];
 UINT prevAddr0Mode  = baseAddrMode[0];

 ULL  prevBase1PAddr = baseRegPhysAddr[1];
 ULL  prevBase1Addr  = baseReg64[1];
 UINT prevAddr1Mode  = baseAddrMode[1];

 if ( random()%2 ) {
   printf("/* Burst Mode Cached */ \n");   
   printf("90: \n");
   addrMode = CACHED_ADDR;
 }
 else {
   printf("/* Burst Mode Uncached */ \n");
   printf("90: \n");
   addrMode = UNCACHED_ADDR;
 }
   // get some base address to work with
   ULL basePAddr = memoryModel.pickPhysAddr( curCpuIdG , addrMode);

   if ( (addrMode & CACHE_MASK) == UNCACHED_ADDR )
     basePAddr += memoryModel.maxCacheWrapVal + (2 * memoryModel.addrRange) + (2* MAX_SCACHE_SIZE);
   else 
     basePAddr += memoryModel.maxCacheWrapVal + (2 * memoryModel.addrRange) + (4* MAX_SCACHE_SIZE) +  (burstMaxG * burstDeltaG);

   ULL baseVAddr = memoryModel.mapPhysAddr( basePAddr,addrMode );


 // place in entry 0

 if ( doMP == TRUE ) {
   baseReg64[0]       = ( baseVAddr & MPaddrMask64 ) | MPwordG;
   baseRegPhysAddr[0] = ( basePAddr & MPaddrMask64 ) | MPwordG;
   baseReg64[1]       = ( baseVAddr & MPaddrMask64 ) | MPwordG;
   baseRegPhysAddr[1] = ( basePAddr & MPaddrMask64 ) | MPwordG;

 }
 else {
   baseReg64[0]       =  baseVAddr & 0xfffffffffffffff8LL;
   baseRegPhysAddr[0] =  basePAddr & 0xfffffffffffffff8LL;
   baseReg64[1]       =  baseVAddr & 0xfffffffffffffff8LL;
   baseRegPhysAddr[1] =  basePAddr & 0xfffffffffffffff8LL;

 }

 printf("\t dli	r%-2d,\"%016llx\"  /* physical address %016llx */\n",
	baseRegG + 0, baseReg64[0],baseRegPhysAddr[0]);

 baseAddrMode[0] = addrMode;

 printf("\t dli	r%-2d,\"%016llx\"  /* physical address %016llx */\n",
	baseRegG + 1, baseReg64[1],baseRegPhysAddr[1]);

 baseAddrMode[1] = addrMode;
 
 int burstMin;
 // make sure there is gap
 if ( burstMaxG < ( burstMinG + 2 )) burstMin = burstMinG+2;
 int Burst = burstMin + random()%(burstMaxG - burstMin );

 int storeBurst = burstMaxG;
 int loadBurst  = burstMaxG;

 int ldToStDelay = burstLdStDelayG;

 int storeIndex = 0;
 int loadIndex  = 0;
 int storeOffset;
 int loadOffset;


 int storeBurstMode;

 // either we pick randomly or we use what we were passed.

 if ( burstModeG == RAND_SEL_BURST ) {
   storeBurstMode = ( random()%2 ) ? FLAT_BURST : LOOP_BURST;
 }
 else 
   storeBurstMode = burstModeG;


 if ( random()%2 ) burstCacheOP(storeBurst, burstDeltaG);

 while ( storeBurst > 0 || loadBurst > 0 ) {

   if ( storeBurst > 0 ) {
     if ( storeBurstMode == FLAT_BURST ) {
       storeOffset = storeIndex * burstDeltaG;
       burst_sd_inst(storeOffset,burstDeltaG);   
       storeBurst--;
       storeIndex++;
     }
     else {
       burst_sd_loop_inst(storeBurst, burstDeltaG);
       if ( random()%2 ) burstCacheOP(storeBurst, burstDeltaG);
       storeBurst = 0;
     }
   }

   if ( ldToStDelay ) ldToStDelay--;
   else if ( loadBurst > 0 ) {
	   loadOffset = loadIndex * burstDeltaG;
	   burst_ld_inst(loadOffset,burstDeltaG);
	   loadBurst--;
	   loadIndex++;
   }
 }
 burst_prefetch();

 // restore first base register
 baseReg64[0]       = prevBase0Addr;
 baseRegPhysAddr[0] = prevBase0PAddr;
 baseReg64[1]       = prevBase1Addr;
 baseRegPhysAddr[1] = prevBase1PAddr;
 baseAddrMode[0]    = prevAddr0Mode;
 baseAddrMode[1]    = prevAddr1Mode;

 printf("/* restore base registers after burst mode */ \n");

 printf("	dli	r%-2d,\"%016llx\"  /* physical address %016llx */\n",
	baseRegG + 0, baseReg64[0],baseRegPhysAddr[0]);

 printf("	dli	r%-2d,\"%016llx\"  /* physical address %016llx */\n",
	baseRegG + 1, baseReg64[1],baseRegPhysAddr[1]);

}

//
void sdc1_inst() {

Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 UINT randReg;

 ULL  loadValue  = 0;
 ULL  storeValue;

  if ( mips2G == TRUE ) 
    return;

  // randomly select the base register

  regSel = random();
  regSel = regSel % maxBaseRegG;

  // select data to store out

  offset     = 0;
  randReg    = random();
  tmpReg1G   = baseImmdFpuRegG + (randReg % maxImmdFpuRegG);
  storeValue = immdFpuReg64[randReg%maxImmdFpuRegG];

  Cell * newCell = sdc1(baseRegPhysAddr[regSel] + offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL ) 
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("  	sdc1  	fp%-2d,0x%-x(r%-2d)\n", tmpReg1G, offset, baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void sdxc1_inst() {

Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 UINT randReg;

 ULL  loadValue  = 0;
 ULL  storeValue;

  if ( mips2G == TRUE ) 
    return;

  // randomly select the base register

  regSel = random();
  regSel = regSel % maxBaseRegG;

  // select data to store out

  offset     = 0;
  randReg    = random();
  tmpReg1G   = baseImmdFpuRegG + (randReg % maxImmdFpuRegG);
  storeValue = immdFpuReg64[randReg%maxImmdFpuRegG];

  Cell * newCell = sdc1(baseRegPhysAddr[regSel] + offset, storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL ) 
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("        ori   r28,r0,0x%-x\n",offset);
  printf("  	sdxc1  	fp%-2d,r28(r%-2d)\n", tmpReg1G, baseRegG + regSel);

  if ( doAtomicG ) printf("#ATOMIC#\n");

}

//

void scd_inst() {

 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL  storeValue;

  if ( doLlScG == FALSE ) return;
  if ( mips2G == TRUE ) return;
  
  if ( fpRepeatCounterG > 0 )
    fpRepeatCounterG--;
  
  /* randomly select the base register */
  regSel = random();
  regSel = regSel % maxBaseRegG;
  
  if ( ( baseAddrMode[regSel] & CACHE_MASK) == UNCACHED_ADDR ) 
    return;
  
  offset = 0;
  randReg = random();
  tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
  storeValue = immdReg64[randReg%maxImmdRegG];
  Cell * newCell = sd(baseRegPhysAddr[regSel], storeValue, baseAddrMode[regSel]);
  if ( newCell == NULL )
    printf("  	sd  	r0,0x%-x(r%-2d)\n", offset&0xfff8, baseRegG + regSel);

  printf("	or	r28,r%-2d,r0\n",tmpReg1G);
  printf("1:\n");
  if (NopsBeforeLL) {
    int i;
    for (i = 0; i < 32; i++) printf("  nop\n");
  }
  printf("        lld      r0,0x%-x(r%-2d)\n",  offset, baseRegG + regSel);
  printf("        scd      r%-2d,0x%-x(r%-2d)\n", tmpReg1G,  offset, baseRegG + regSel);
  if (NopsAfterSC) {
    int i;
/*
    printf("	bne 	 r0,r%-2d,1f\n",tmpReg1G);
*/
    for (i = 0; i < 32; i++) printf("  nop\n");
  }
  if (COP1AfterSC) {
    printf("   cfc1 r0,C1_SR       \n");
  }
  if (SCBranchLikely) {
    printf("	beql	 r0,r%-2d,1b\n",tmpReg1G);
    printf("	or	r%-2d,r28,r0\n",tmpReg1G);
    printf("	or	r%-2d,r28,r0\n",tmpReg1G);
  } else {
    printf("	beq 	 r0,r%-2d,1b\n",tmpReg1G);
    printf("	or	r%-2d,r28,r0\n",tmpReg1G);
  }
  printf("1:\n");
  if ( doAtomicG ) printf("#ATOMIC#\n");

}

void bb_prefetch() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL  storeValue;
 int	hint;
 UINT	prefVaddrLo, prefVaddrHi;
 UINT	prefPaddrLo, prefPaddrHi;
 if ( doBlackBirdG != TRUE ) return;

		  /* disabled case 2,3 for now
		   * need to put in t5 option.
		   */
		switch ( random() % 2 ) {

		case 0:
                	regSel = random() % maxBaseRegG;
			offset = (offset % 8) & 0xfffffffc;
			hint   = random() % 32;
			prefVaddrLo = offset + prefSyncVaddrLo[baseRegG + regSel];
			prefVaddrHi = prefSyncVaddrHi[baseRegG + regSel];
			if (prefVaddrLo < prefSyncVaddrLo[baseRegG + regSel])
			    prefVaddrHi++;
			prefPaddrLo = offset + prefSyncPaddrLo[baseRegG + regSel];
			prefPaddrHi = prefSyncPaddrHi[baseRegG + regSel];
			if (prefPaddrLo < prefSyncPaddrLo[baseRegG + regSel])
			    prefPaddrHi++;
			logPrefetch(prefVaddrHi, prefVaddrLo, prefPaddrHi, prefPaddrLo,
				    hint);
			printf("/*      pref    %d ,0x%-x,(r%-2d)     */\n",
			       hint,offset,baseRegG + regSel);
			printf("/* pref vaddr = 0x%08x %08x   paddr = 0x%08x %08x*/\n",
			       prefVaddrHi, prefVaddrLo, prefPaddrHi, prefPaddrLo);
			printf("%s:\n", generateIssuePrefSyncLabel());
			printf("        PREF(%d ,0x%-x,%-2d)\n",
			       	hint, offset, baseRegG + regSel);
 			if ( doAtomicG ) printf("#ATOMIC#\n");
			return;
		case 1:
                	regSel = random() % maxBaseRegG;
			offset = (offset % 8) & 0xfffffffc;
			hint   = random() % 32;
                        hint   = random() % 32;
                        prefVaddrLo = offset + prefSyncVaddrLo[baseRegG + regSel];
                        prefVaddrHi = prefSyncVaddrHi[baseRegG + regSel];
                        if (prefVaddrLo < prefSyncVaddrLo[baseRegG + regSel])
                            prefVaddrHi++;
                        prefPaddrLo = offset + prefSyncPaddrLo[baseRegG + regSel];
                        prefPaddrHi = prefSyncPaddrHi[baseRegG + regSel];
                        if (prefPaddrLo < prefSyncPaddrLo[baseRegG + regSel])
                            prefPaddrHi++;
                        logPrefetch(prefVaddrHi, prefVaddrLo, prefPaddrHi, prefPaddrLo,
                                    hint);
                        printf("/*      pfetch    %d ,r0,(r%-2d)     */\n",
                               hint, baseRegG + regSel);
                        printf("/* pfetch vaddr = 0x%08x %08x   paddr = 0x%08x %08x*/\n",
                               prefVaddrHi, prefVaddrLo, prefPaddrHi, prefPaddrLo);
                        printf("%s:\n", generateIssuePrefSyncLabel());
                        printf("        PFETCH(%d ,0 ,%-2d)\n", hint, baseRegG + regSel);
 			if ( doAtomicG ) printf("#ATOMIC#\n");
			return;

		case 2:
                	regSel = random() % maxBaseRegG;
			offset = (offset % 8) & 0xfffffffc;
			hint   = random() % 32;
			printf("        PREF(%d ,0x%-x,r%-2d)\n",hint,offset,baseRegG + regSel);
 			if ( doAtomicG ) printf("#ATOMIC#\n");
			return;


		case 3:
                	regSel = random() % maxBaseRegG;
			offset = (offset % 8) & 0xfffffffc;
			hint   = random() % 32;
			printf("        PFETCH(%d ,r0 ,r%-2d)\n",hint,baseRegG + regSel);
 			if ( doAtomicG ) printf("#ATOMIC#\n");
			return;
		}
}


void t5_prefetch() {
 Cell *tmpPtr1;
 Cell *found;

 UINT regSel;
 UINT offset;
 ULL  loadValue  = 0;
 UINT randReg;
 ULL  storeValue;
 int hint;

  if ( prefetchG == FALSE || mips4G == FALSE ) return; 
 
  if ( (random() % 2 ) == 0  ) {
    /* Data prefetch */
    uint hint = CACHED_ADDR | USED_MEM;  
    // hint may be modified by this call will addictional properties
    ULL physAddr     = memoryModel.pickPhysAddr( curCpuIdG, hint );
    ULL virtualAddr  = memoryModel.mapPhysAddr( physAddr, hint );

    printf("	dli	r28,0x%16llx \n",virtualAddr);
  }
  else
    {
      /* Instruction prefetch */
      printf("	dla	r28,99f \n");
    }

  hint = random() % 2;	

  switch ( random() % 3 ) {
  case 0:
    printf("	pfetch	%d,r28(r0)\n",hint);
    if ( doAtomicG ) printf("#ATOMIC#\n");
    return;
  case 1:
    printf("	pfetch	%d,r0(r28)\n",hint);
    if ( doAtomicG ) printf("#ATOMIC#\n");
    return;
  case 2:
    printf("	pref	%d,0(r28)\n",hint);
    if ( doAtomicG ) printf("#ATOMIC#\n");
    return;
  }
}


void speedReadSync () {

  if ( (random()%speedReadRateG) == 0 ) {

    ULL basePhysSyncAddr = 0x000ff00080;
    printf(" /* speedracer read sync */ \n");
    printf("\tdli r28,0x%016llx \n",memoryModel.mapPhysAddr(basePhysSyncAddr + (random()%6)*8, UNCACHED_ADDR | SPACE32_ADDR) );
    printf("\tld  r28,0x0(r28) \n");

  }
}

/************/
void doInstruction()
{
	int i;
	UINT	randVal;
	UINT	instSel;
	UINT	regSel;
	UINT	offset;

	ULL     storeValue;
	ULL	loadValue  = 0;

	UINT	randReg;
        int	hint;
	UINT	prefVaddrLo, prefVaddrHi;
	UINT	prefPaddrLo, prefPaddrHi;
	UINT 	address;

	Cell  *tmpPtr1;
	Cell  *tmpPtr2;

	Cell  *found;
	randVal    = random();
	instSel    = randVal % NUM_OF_INSTRUCTIONS;

	if ( doDebugBeastG == TRUE ) {

	  switch ( randVal% 2 ) {
	  case 0:
	    
	    ld_inst();
	  break;

	  case 1:
	    sd_inst();
	    break;
	  }
	}
	else
	switch (instSel) {



	case LB:

	  if ( doWordDwordOP == TRUE ) break;

	  lb_inst();
	  break;

	case LH:
	  if ( doWordDwordOP == TRUE ) break;

	  lh_inst();
	  break;

        case LHU:
	  if ( doWordDwordOP == TRUE ) break;

	  lhu_inst();
	  break;

	case LW:
	 
	  lw_inst();
	  break;

	case LWC1:
	 
	  lwc1_inst();
	  break;

	case LWXC1:
	 
	  lwxc1_inst();
	  break;

        case LWU:

	  lwu_inst();
	  break;


	case LWL:
	  if ( doWordDwordOP == TRUE ) break;
	  lwl_inst();
	  break;

	case LWR:
	  if ( doWordDwordOP == TRUE ) break;
	  lwr_inst();
	  break;

	  
	case HIT_WRI_INV_D:

	  hitWriInvD_inst();
	  break;

	case HIT_WRI_INV_SD:

	  hitWriInvSD_inst();
	  break;

	case INDEX_WRI_INV_D:

	  indexWriInvD_inst();
	  break;

	case INDEX_WRI_INV_SD:

	  indexWriInvSD_inst();
	  break;

	case SB:
	  if ( doWordDwordOP == TRUE ) break;
	  sb_inst();
	  break;

	case SH:
	  if ( doWordDwordOP == TRUE ) break;
	  sh_inst();
	  break;
	case SW:

	  sw_inst();
	  break;

	case SWC1:

	  swc1_inst();
	  break;

	case SDC1:

	  sdc1_inst();
	  break;

	case SWXC1:

	  swxc1_inst();
	  break;

	case SDXC1:

	  sdxc1_inst();
	  break;

	case SC:

	  sc_inst();
	  break;

	case SWL:
	  if ( doWordDwordOP == TRUE ) break;
	  swl_inst();
	  break;

	case SWR:
	  if ( doWordDwordOP == TRUE ) break;
	  swr_inst();
	  break;

	case SDL:
	  if ( doWordDwordOP == TRUE ) break;
	  sdl_inst();
	  break;
	  
	case SDR:
	  if ( doWordDwordOP == TRUE ) break;
	  sdr_inst();
	  break;

	case LD:

	  ld_inst();
	  break;

	case LDC1:

	  ldc1_inst();
	  break;

	case LLD:

	  lld_inst();
	  break;

	case LDL:
	  if ( doWordDwordOP == TRUE ) break;
	 ldl_inst();
	  break;

	case LDR:
	  if ( doWordDwordOP == TRUE ) break;
	  ldr_inst();
	  break;

	case SD:

	  sd_inst();
	  break;

	case SCD :
	
	  scd_inst();
	  break;

	case SHIFT_SLIP:

	  if ( doAluG == TRUE ) {
	    printf("	sll	r0,r26,r26\n");
	    if ( fpRepeatCounterG > 0 ) 
	      fpRepeatCounterG--;
	    if ( doAtomicG ) printf("#ATOMIC#\n");
	  }
	  break;

	case SIGN_EXTEND_STALL:

	  if ( doAluG == TRUE ) {
	    printf("	addu	r0,SIGN_EXTEND_REG,SIGN_EXTEND_REG\n");
	    if ( fpRepeatCounterG > 0 ) 
	      fpRepeatCounterG--;
	    if ( doAtomicG ) printf("#ATOMIC#\n");
	  }
	  break;

	case ALU_OP:

	  if  ( doAluG == FALSE ) break; 
	  
	  if ( random() % 2 )
	    printf("	dmultu	r%d,r%d \n",(random()%31)+1,(random()%31)+1);	
	  else
	    printf("	ddivu	r0,r%d,r%d \n",(random()%31)+1,(random()%31)+1);	

	  if ( doAtomicG ) printf("#ATOMIC#\n");
	  break;


	case BB_PREFETCH:

	  bb_prefetch();
	  break;

	case 	T5_PREFETCH:

	  t5_prefetch();
	  break;

	case PREFETCH_LABEL:

		printf("99: \n");
		break;

	case HIT_INV_I: 

                if ( noPrimaryCacheG == TRUE ) break;
                if ( noCacheOpsG == TRUE ) break;
		if ( IkernelG == FALSE ) break;

		if ( (random() % 2 ) == 0  ) 
		    printf("	dla	r28,99f \n");
		else
		    printf("	dla	r28,99b \n");

		printf("	cache Hit_Invalidate_I 0(r28)\n");
 		if ( doAtomicG ) printf("#ATOMIC#\n");
		break;

	case HIT_INV_SI:  
	
	  if ( byPassHitInvSI == TRUE ) {

               if ( noPrimaryCacheG == TRUE ) break;
                if ( noCacheOpsG == TRUE ) break;
                if ( IkernelG == FALSE ) break;

                if ( (random() % 2 ) == 0  )
                    printf("    dla     r28,99f \n");
                else
                    printf("    dla     r28,99b \n");

                printf("        cache Hit_Invalidate_I 0(r28)\n");
                if ( doAtomicG ) printf("#ATOMIC#\n");
                break;
	  }
	  else { 
                if ( noCacheOpsG == TRUE ) break;
		if ( IkernelG == FALSE ) break;

		if ( (random() % 2 ) == 0  ) 
		    printf("	dla	r28,99f \n");
		else
		    printf("	dla	r28,99b \n");

		printf("	cache Hit_Invalidate_S 0(r28)\n");
 		if ( doAtomicG ) printf("#ATOMIC#\n");
		break;
	}

	case INDEX_INV_I: 

                if ( noPrimaryCacheG == TRUE ) break;
                if ( noCacheOpsG == TRUE ) break;
		if ( IkernelG == FALSE ) break;

		if ( (random() % 2 ) == 0  ) 
		    printf("	dla	r28,99f \n");
		else
		    printf("	dla	r28,99b \n");

		printf("	cache Index_Invalidate_I 0(r28)\n");
 		if ( doAtomicG ) printf("#ATOMIC#\n");
		break;


	case FILL_I: 

                if ( noPrimaryCacheG == TRUE ) break;
                if ( noCacheOpsG == TRUE ) break;
		if ( IkernelG == FALSE ) break;

		if ( (random() % 2 ) == 0  ) 
		    printf("	dla	r28,99f \n");
		else
		    printf("	dla	r28,99b \n");

		printf("	cache Fill_I 0(r28)\n");
 		if ( doAtomicG ) printf("#ATOMIC#\n");
		break;



	}
}
/***********************************************************/
void doSync()
{
        if ( syncCounter > syncCountG ) {
                if ( (doBlackBirdG == TRUE) ) {
                        logSync();
                        printf("%s:\n", generateIssuePrefSyncLabel());
                        printf("\tsync\n");
                        syncCounter = 0;
                }
                else {
                        printf("\tsync\n");
                        syncCounter = 0;
                }
 		 if ( doAtomicG ) printf("#ATOMIC#\n");
        }
        else
                syncCounter++;

}



/***********************************************************/
void doExtIntervention()
{
  UINT address;
  Cell *tmpPtr1;

   if ( extIntvG == FALSE ) return;

   extIntvCounterG++;

   if ( extIntvCounterG > extIntvCountG ) {

       	extEventIdG++;
       	tmpPtr1 = findRandAddr();

        if ( tmpPtr1 == NULL )
             address = memoryModel.getAddrRange() & ~UNMAPPED_CACHE_VA ;
        else
             address = tmpPtr1->address;

        switch ( random() % 2 ) {

        case 0:
           printf("/* intervention */ \n");
           printf (".intex_%d:\n",extEventIdG);
           printf (".addr_%d_%x:\n",extEventIdG,address);
           printf (".target_%d_1:\n",extEventIdG);
           printf (".done_%d:\n",extEventIdG);
           printf ("label%d:\n",extEventIdG);
           break;

        case 1:

           printf("/* intervention */ \n");
           printf (".intsh_%d:\n",extEventIdG);
           printf (".addr_%d_%x:\n",extEventIdG,address);
           printf (".target_%d_1:\n",extEventIdG);
           printf (".done_%d:\n",extEventIdG);
           printf ("label%d:\n",extEventIdG);

        }

        if ( doAtomicG ) printf("#ATOMIC#\n");
	extIntvCounterG = 0;
   }
}
/***********************************************************/
void doInvalidates()
{

  int regSel,randReg;
  int offset;

  if ( extInvG == FALSE ) return;

  extInvCounterG++;

  if ( extInvCounterG > extInvCountG ) {

   //if ( random()%2  ) {
   if ( 0  ) {
       extEventIdG++;
       printf ("/* invalidate */ \n");
       printf (".eivd_%d:\n",extEventIdG);
       printf (".laddr_%d_label%d:\n",extEventIdG,extEventIdG+1);
       printf (".target_%d_1:\n",extEventIdG);
       printf (".done_%d:\n",extEventIdG); 
       printf ("label%d:\n",extEventIdG);
       if ( doAtomicG ) printf("#ATOMIC#\n");
       extInvCounterG = 0;
   } 
   else {

        /* randomly select the base register */
	extEventIdG++; 
        regSel = random();
        regSel = regSel % maxBaseRegG;

	if ( ( baseAddrMode[regSel] & CACHE_MASK) == UNCACHED_ADDR ) 
	  return;

        offset = 0;
        randReg = random();
        tmpReg1G = baseImmdRegG + (randReg % maxImmdRegG);
        printf("        ld	r28,0x%-x(r%-2d)\n", offset, baseRegG + regSel);
        printf("        cache   Hit_Writeback_Inv_SD  0x%-x(r%-2d)\n",offset, baseRegG + regSel);
        printf("        li	r1,-1\n");
        printf("        xor	r28,r1,r28\n");
        printf("        or	r1,r0,r28\n");
        printf("        sd	r28,0(r%-2d)\n", baseRegG + regSel);
        printf("        dmfc0	r0,C0_SR\n");
       	printf ("/* invalidate */ \n");
       	printf (".eivd_%d:\n",extEventIdG);
	printf (".addr_%d_%llx:\n",extEventIdG,baseRegPhysAddr[regSel]);	
       	printf (".target_%d_1:\n",extEventIdG);
       	printf (".done_%d:\n",extEventIdG);
       	printf ("label%d:\n",extEventIdG);
        printf("        dmfc0	r0,C0_SR\n");
        printf("1: \n"); 
        printf("        ld	r28,0(r%-2d)\n",baseRegG + regSel);
        printf("	beq	r28,r1,1b \n"); 
        printf("	nop \n"); 
        if ( doAtomicG ) printf("#ATOMIC#\n");

        extInvCounterG = 0;
   }
  }
}
/***********************************************************/
void doExtInterrupts()
{
  int intNumber;

  if ( extIntrG == FALSE ) return;

  extIntrCounterG++;

  if ( extIntrCounterG > extIntrCountG ) {
       extEventIdG++;
       intNumber = 1 << (random()%4); 
       printf ("/* interrupt */ \n");
       printf (".int_%d:\n",extEventIdG);
       printf (".data_%d_%02x:\n",extEventIdG,intNumber);
       printf (".mask_%d_%02x:\n",extEventIdG,intNumber);
       printf (".dly_%d_128:\n",extEventIdG);
       printf (".target_%d_1:\n",extEventIdG);
       printf (".done_%d:\n",extEventIdG); 
       printf ("label%d:\n",extEventIdG);
       if ( doAtomicG ) printf("#ATOMIC#\n");
       extIntrCounterG = 0;
  }
}
/***********************************************************/
void doNacks()
{
  UINT address;
  Cell *tmpPtr1;

   if ( extNackG == FALSE ) return;

   extNackCounterG++;

   if ( extNackCounterG > extNackCountG ) {

        extEventIdG++;
        tmpPtr1 = findRandAddr();

        if ( tmpPtr1 == NULL )
             address = memoryModel.getAddrRange() & ~UNMAPPED_CACHE_VA ;
        else
             address = tmpPtr1->address;

        printf("/* nak */ \n");
        printf (".respstate_%d:\n",extEventIdG);
        printf (".addr_%d_%x:\n",extEventIdG,address);
        printf (".data_%d_2:\n",extEventIdG);
	if ( random()%2 )
        	printf (".persist_%d_%d:\n",extEventIdG,(random()%6)+1);
        printf (".done_%d:\n",extEventIdG);
        printf ("label%d:\n",extEventIdG);

        if ( doAtomicG ) printf("#ATOMIC#\n");
        extNackCounterG = 0;
   }
}
/***********************************************************/
void doRdRdy()
{
  int delay;
	
  if ( rdRdyG == FALSE ) return; 
  
  extRdRdyCounterG++;
  if ( extRdRdyCounterG > extRdRdyCountG ) {
        extEventIdG++;
        delay = minRdRdyDuration + random()%(maxRdRdyDuration-minRdRdyDuration);
        printf("/* rdRdy */ \n");
        printf (".rdrdy_%d:\n",extEventIdG);
        printf (".dly_%d_0:\n",extEventIdG);
        printf (".data_%d_8%07x:\n",extEventIdG,delay);
        printf (".done_%d:\n",extEventIdG);
        printf ("label%d:\n",extEventIdG);

        if ( doAtomicG ) printf("#ATOMIC#\n");
        extRdRdyCounterG = 0;
  }
}
/***********************************************************/
void doWrRdy()
{
  int delay;
  if ( wrRdyG == FALSE ) return; 

  extWrRdyCounterG++;

  if ( extWrRdyCounterG > extWrRdyCountG ) {
        extEventIdG++;
        delay = minWrRdyDuration + random()%(maxWrRdyDuration-minWrRdyDuration);
        printf("/* wrRdy */ \n");
        printf (".wrrdy_%d:\n",extEventIdG);
        printf (".dly_%d_0:\n",extEventIdG);
        printf (".data_%d_8%07x:\n",extEventIdG,delay);
        printf (".done_%d:\n",extEventIdG);
        printf ("label%d:\n",extEventIdG);

        if ( doAtomicG ) printf("#ATOMIC#\n");
        extWrRdyCounterG = 0;
  }
}
/***********************************************************/
void genLLSC( int cpuid ) 
{

    if ( doMP == FALSE ) return;
    if ( mips2G  ==  TRUE ) return;

    if ( ((sharedLockCounterG++) % sharedLockGenRateG) == 0 ) {
	sharedLocks->genCode(cpuid);

	if ( doAtomicG ) printf("#ATOMIC#\n");

	if ( lockPtr < (numLocksG-1) )
	    lockPtr++;
	else
	    lockPtr = 0;
    }
    
    if ( (doBarrierG == TRUE) && (numBarriersDone < maxNumBarrier) ) {
	if ( barrierCounter > barrierDropRate ) {
	    numBarriersDone++;
	    if ( random() % 2 ) {
		printf("	# Barrier Count %d CPU ID %d\n",numBarriersDone,cpuid);
      if (NopsBeforeLL) {
		  printf("	BARRIER_NOPLL(barrierLock,%d,AllocCPULock) \n",numBarriersDone*8);
      } else if (SCBranchLikely) {
		  printf("	BARRIER_SCBRL(barrierLock,%d,AllocCPULock) \n",numBarriersDone*8);
      } else if (NopsAfterSC) {
		  printf("	BARRIER_SCNOP(barrierLock,%d,AllocCPULock) \n",numBarriersDone*8);
      } else if (COP1AfterSC) {
		  printf("	BARRIER_SCCOP1(barrierLock,%d,AllocCPULock) \n",numBarriersDone*8);
      } else {
		  printf("	BARRIER(barrierLock,%d,AllocCPULock) \n",numBarriersDone*8);
      }
		barrierCounter = 0;
	    }
	    else {
		printf("	FUNC_BARRIER_FETCH_OP(sn0BarrierLock+%d,AllocCPULock) \n",numBarriersDone*64);
		barrierCounter = 0;
	    }

	}
	else barrierCounter++;
    }
}
/***********************************************************/
void doFP()
{
	UINT	randVal;

   if ( (doFPopG == FALSE) || (fpRepeatCounterG != 0)) 
		return;

   fpRepeatCounterG = 8;
   if ( doBlackBirdG  == FALSE )  {
	randVal = (random()) % 5;
	switch ( randVal ) {
	case 0:
		printf("	add.s   fp4,fp0,fp2 \n");
		break;

	case 1:
		printf("	mul.s   fp4,fp6,fp8 \n");
		break;

	case 2:

		printf("	div.s   fp4,fp10,fp12 \n");
		break;

	case 3:

		printf("	div.d   fp4,fp14,fp16 \n");
		break;

	case 4:
		printf("	mfc1    r0,fp4 \n");
		break;

	}
       if ( doAtomicG ) printf("#ATOMIC#\n");
   }
   else {
        randVal = (random()) % 3;
	printf("        FP_MAC_%d\n", randVal);
        if ( doAtomicG ) printf("#ATOMIC#\n");
   }

}

void  doPrologue()
{

  int i;

  // we set the number of CPU we think are


     
  if ( randEventRateG > 0 ) {

      printf("	li	r8,EAREGSET # load EA base register\n");
      printf("/* data area begin and end space */\n");
/* todo need to clear all base areas */
      printf("	dli	r2,0x%x\n" ,baseLdStAddr & CACHED_TO_PHYSICAL);
      printf("	dli	r3,0x%x\n",(baseLdStAddr & CACHED_TO_PHYSICAL) + memoryModel.getAddrRange());
      
      printf("	dli	r7,0x%x\n",CACHED_TO_PHYSICAL);
      printf("/* events on instruction text area */\n");
      printf("	dla	r4,sbkillerBegin\n");
      printf("	dla	r5,sbkillerEnd\n");

      printf("	and	r4,r7,r4\n");
      printf("	and	r5,r7,r5\n");
      
      printf("	dsll	r3,r3,32\n");
      printf("	or	r3,r3,r2\n");

      printf("	dsll	r5,r5,32\n");
      printf("	or	r5,r5,r4\n");

      printf("	sd	r3,SYSAD(r8)\n");
      printf("	sd	r5,SYSAD+8(r8)\n");
      printf("	li	r9,0x%x\n",randEventRateG);
      printf("	dsll	r9,r9,32\n");
      printf("	or	r9,r9,RNDMEVENTCMD \n");
      printf("	sd	r9,CMD(r8)\n");
  }

  if ( doBlackBirdG  == FALSE && noInitG == FALSE ) {

	if ( reverseEndianG == FALSE ) {
		printf("	mfc0	r2,C0_SR\n");

		if ( IkernelG == TRUE  && ImappedG == FALSE )
			printf("	li	r1,SR_CU0 | SR_CU1 \n");

		else if ( IkernelG == TRUE && ImappedG == TRUE )
			printf("	li	r1,SR_CU0 | SR_CU1 | SR_EXL\n");

		else {  /* user mode */
		   if ( forICE == TRUE || doBlackBirdG == TRUE )
			printf("	li	r1,SR_CU0 | SR_CU1 | SR_UX | SR_USER | SR_EXL \n");
		   else
			printf("	li	r1,SR_CU0 | SR_CU1 | SR_UX | SR_KSUUser | SR_EXL \n");
		}
		printf("	or	r2,r1					\n");

		if ( fpuFRbit1G == TRUE ) {
			printf("	li	r1,SR_FR                        \n");		  
			printf("	or	r2,r1				\n");
		}

		printf("	mtc0	r2,C0_SR				\n");
		if ( T5ModeG == FALSE )
		printf("	nop;nop;nop;nop;nop;nop				\n");


		printf("	li	r3,FP_RN				\n");
		printf("	ctc1	r3,C1_SR				\n");
		if ( T5ModeG == FALSE )
		printf("	nop;nop;nop;nop					\n");

	} // if reverse endian

	if ( ImappedG == TRUE ) {

	   if ( reverseEndianG == FALSE ) {

	     if ( userTLBHandlerG == FALSE ) { 

		 printf("	MAP_VA_TO_VAP(firstMappedLocation,r2,r3,r4)	\n"); 	
	         printf("	mtc0    r2,C0_EPC				\n");	

		 if ( T5ModeG == FALSE )
		 printf("	nop;nop;nop;nop;nop				\n");

	         printf("	eret						\n\n");
	         printf("firstMappedLocation:					\n");
	     }
	   }
	} /* end of ImappedG == TRUE */	

        /*
	printf("	dla      r3,fpAddData				\n");
	printf("	l.s     fp0,0(r3)				\n");
	printf("	l.s     fp2,4(r3)				\n");
	printf("	dla      r3,fpMpyData				\n");
	printf("	l.s     fp6,0(r3)				\n");
	printf("	l.s     fp8,4(r3)				\n");
	printf("	dla      r3,fpDivSData				\n");
	printf("	l.s     fp10,0(r3)				\n");
	printf("	l.s     fp12,4(r3)				\n");
	printf("	dla      r3,fpDivDData				\n");
	printf("	l.d     fp14,0(r3)				\n");
	printf("	l.d     fp16,8(r3)				\n");
	printf("\n");
	*/

  } // backbird == false


  if ( doMP == TRUE ) {

	  printf("\n");
          printf("	dla	r2,numberOfCPU 	# dynamic runtime var to hold number of cpu's\n");
	  printf("	dla	r3,AllocCPULock # another LL/SC incremented CPU counter\n");
	  printf("1:\n");
     if (NopsBeforeLL) {
       int i;
       for (i = 0; i < 32; i++) printf("  nop\n");
     }
	  printf("	ll	r6,0(r3)        # attempt to lock address\n");
	  printf("	addi	r6,r6,1         # increment lock\n");
	  printf("	or	r7,r6,r0        # save value\n");
	  printf("	sc	r6,0(r3)        # attempt to save value\n");
     if (NopsAfterSC) {
       int i;
/*
	    printf("	bne	r6,0,1f         # retry if failed\n");
*/
       for (i = 0; i < 32; i++) printf("  nop\n");
     }
     if (COP1AfterSC) {
       printf("   cfc1 r0,C1_SR       \n");
     }
     if (SCBranchLikely) {
	    printf("	beql	r6,0,1b         # retry if failed\n");
     } else {
	    printf("	beq	r6,0,1b         # retry if failed\n");
     }
	  printf("	nop\n");
	  printf("1:\n");
	 
	  // if the user has set number of CPU's then lets use that number not computer it
	  if ( numCpuSetByUser == 0 ) {

	      printf("1:\n");
         if (NopsBeforeLL) {
           int i;
           for (i = 0; i < 32; i++) printf("  nop\n");
         }
	      printf("	ll	r4, 0(r2)       # lock num cpu variable\n");
	      printf("	slt	r4, r7, r4\n");
	      printf("	bne	r4, r0, 2f	# no need to update if less than current value\n");
	      printf("	move	r1, r7\n");
	      printf("	sc	r1, 0(r2)\n");
         if (NopsAfterSC) {
           int i;
/*
	        printf("	bneqz	r1, 2f\n");
*/
           for (i = 0; i < 32; i++) printf("  nop\n");
         }
         if (COP1AfterSC) {
           printf("   cfc1 r0,C1_SR       \n");
         }
         if (SCBranchLikely) {
	        printf("	beqzl	r1, 1b\n");
         } else {
	        printf("	beqz	r1, 1b\n");
         }
	      printf("	nop\n");
	      printf("2:\n");
	  }

 	  printf("	dla      r5,SlaveLock	\n");
	  printf("	li	r6,1		# just to not confuse people, r6 already one due to sc\n");
	  printf("	bne	r7, r6, 100f    # r6=1, only cpuID=1 does this section,\n");
	  printf("	nop                     # all others go wait at label 100 for SlaveLock to be set to 0\n");
	  printf("\n");
	  printf("   # note in RTL runs , SlaveLock has to be set to a.\n");
	  printf("   # none zero value in the data area, If we loop as in.\n");
	  printf("   # the case of hardware, SlaveLock is automatically incremented.\n");
	  printf("   # to numberofCPU's -1, a non-zero value if cpus > 1.\n");

	  // clear out memory locations we touched in preparation for looping back.

          if ( onHardwareG == TRUE ) {
	    memoryModel.zeroTouchedMem();
	  }



	  sharedLocks->prologue();
    
	  printf("\n		dla	r3,AllocCPULock        # This section makes sure all       \n");	
	  printf("		dla	r2,numberOfCPU         # cpus have received and ID         \n");
	  printf("1:		lw	r10,0(r3)              # otherwise we wait                 \n");
	  printf("		lw	r11,0(r2)                                                  \n");
	  printf("		bne	r10,r11,1b                                                 \n");
	  printf("		nop                                                                \n");
	  printf("		sw	r0, 0(r5)              # now master cpu releases other cpus\n");
	  printf("\n");
	  printf("100:\n");
	  printf("		lw	r4, 0(r5)              # slaves will wait here,until       \n");
	  printf("		bne	r4, r0, 100b           # the are all released              \n");
	  printf("		nop\n");

	if (doBlackBirdG) 
	   printf("		daddu	sp,sp, -8                                                 \n");
	else
	   printf("		dla	sp,stack # give each CPU a private state section          \n");
	printf("		   \n");

	// only if we have given NodeID's will be use NodeID information
        // other wise just go by LLSC info.

	  if ( memoryModel.nodeIDCnt > 0 && (doSN0M == TRUE || doSN0N == TRUE) && disableNodeRead == TRUE ) {

	      printf("		# read NODE/CPU ID from hardware if on SN0                       \n");
	      printf(" \n");
	      printf("          dli	r2,0x9200000001000020 # CPU ID address                    \n");
	      printf("          ld      r2,0(r2)	      #                                   \n");
	      printf("          dli	r8,0x9200000001600000 # CPU ID address                    \n");
	      printf("          ld	r8,0(r8) 	      # read ID                           \n");
	      printf("          li	r3,0x0000ff00	      # mask for NODE ID                  \n");
	      printf("          and	r8,r3,r8	      # extract                           \n");
	      printf("          dsrl	r8,r8,8-1	      # shift 7 bits right                \n");
	      printf("          or	r8,r8,r2	      # create one NODE/CPU ID            \n");
              
	      for ( int l = 0 ; l < memoryModel.nodeIDCnt ; l++ ) {

	      printf(" \n"); 
	      printf("          li      r9,0x%llx             # node %llx cpu 0 \n",memoryModel.nodeID[l] << 1,memoryModel.nodeID[l]);
	      printf("          beq     r8,r9,1f    \n");
	      printf("          ori     r7,r0,%d    \n",l*2+1);
	      printf("          li      r9,0x%llx             # node %llx cpu 1  \n",(memoryModel.nodeID[l] << 1) + 1, memoryModel.nodeID[l]);
	      printf("          beq     r8,r9,1f    \n");
	      printf("          ori     r7,r0,%d    \n",l*2+2);
	      }
	      printf("          halt(FAIL)          \n");
	      printf("1: \n");

	  }



// FIX if we want to have mapped I space

	  for ( i = 0 ; i < numOfCpuG ; i++ ) {
	      printf("		li	r2,%d           # r7 hold cpuID, based on which we jump to\n",i+1);
	      printf("		bne	r7,r2,1f        # different sections\n");
	      printf("		nop	                # \n");
	      printf("	   	sd	r7, 0(sp)       # CPU ID now on top of stack \n");
	      printf("		j	begin%d\n",i);
	      printf("	   	dsubu	sp,sp,16        # adjust top of stack\n\n");
	      printf("1:\n");
	      printf("		dsubu	sp,sp,%d\n",MP_STACK_SIZE);
	  }

	if ( onHardwareG == TRUE ) {
	   printf("	dla	r2,iterationCount	\n");
	   printf("	lw	r28,0(r2)		\n");
	}
	   printf("	halt(FAIL)\n");
  } // if do mp
  else {
          if ( onHardwareG == TRUE ) {
	    memoryModel.zeroTouchedMem();
	  }

  }
    /* used for prefetch address computation */
    printf("99: \n");
    printf("90: \n");
    printf("                li      SIGN_EXTEND_REG,0x8fffffff          /* for alu sign extention */\n");
}


/**********/
void	doEpilogue()
{
 int i;
	/* End of test code */

    /* used for prefetch address computation */
 printf("99: \n"); // normal prefetch
 printf("90: \n"); // burst prefetch
 printf ("label%d:\n",++extEventIdG);

 if ( doMP == TRUE ) {

	/* all cpus execpt cpu 0, get stuck, here, basically, have passed,
	 * cpu 0, continues to check the results of the incrementers.
	 */

	printf("/* barrier to wait for all CPU's to finish work */\n");

	printf("endCode:        dla      r18,AllocCPULock\n");
	printf("                li      r7,1 			\n");
	printf("2:\n");
   if (NopsBeforeLL) {
     int i;
     for (i = 0; i < 32; i++) printf("  nop\n");
   }
	printf("                ll      r19,0(r18)              \n");
	printf("                sub     r19,r19,r7              \n");
	printf("                sc      r19,0(r18)              \n");
   if (NopsAfterSC) {
     int i;
/*
	  printf("                bne     r19,r0,2f               \n");
*/
     for (i = 0; i < 32; i++) printf("  nop\n");
   }
   if (COP1AfterSC) {
     printf("   cfc1 r0,C1_SR       \n");
   }
   if (SCBranchLikely) {
	  printf("                beql    r19,r0,2b               \n");
   } else {
	  printf("                beq     r19,r0,2b               \n");
   }
  	printf("                nop 				\n");
	printf("2:\n");

	printf("/* at the same time increment NumCPU counter to \n");
	printf("   maintain the number of CPU's since we are decrementing */\n");
	printf("                dla      r18,NumCPU    	        \n");
	printf("2:\n");
   if (NopsBeforeLL) {
     int i;
     for (i = 0; i < 32; i++) printf("  nop\n");
   }
	printf("                ll      r19,0(r18)               \n");
	printf("                add     r19,r19,1               \n");
	printf("                sc      r19,0(r18)              \n");
   if (NopsAfterSC) {
     int i;
/*
	  printf("                bne     r19,r0,2f               \n");
*/
     for (i = 0; i < 32; i++) printf("  nop\n");
   }
   if (COP1AfterSC) {
     printf("   cfc1 r0,C1_SR       \n");
   }
   if (SCBranchLikely) {
	  printf("                beql    r19,r0,2b               \n");
   } else {
	  printf("                beq     r19,r0,2b               \n");
   }
	printf("                nop 				\n");
	printf("2:\n");
	printf("                dla      r20,numberOfCPU		\n");
	printf("                lw      r21,0(r20)              \n"); 
   if (watchdogTimer == TRUE ) {
	  printf("                dli      r5,%llu            \n", watchdogTimeout); 
   }
	printf("3:              lw      r19,0(r18)              \n");
   if (watchdogTimer == TRUE ) {
	  printf("                bnez      r5,4f              \n"); 
	  printf("                dsubu     r5,r5,1              \n"); 
	  printf("                halt(HUNG)              \n"); 
	  printf("4:              bne     r19,r21,3b		\n");
   } else {
	  printf("                bne     r19,r21,3b		\n");
   }
	printf("/* if CPU is logical ID = 1, perform final lock sum checking */\n");
	printf("/* else goto wait for clean up and loop	      */\n");
        printf("                ld      r2, 16(sp) #CPU ID       \n");
        printf("                li      r7,1                    \n");
        printf("                bne     r2, r7,waitForCleanup   \n");
	printf("                nop 				\n");
	printf("		dla	r2,numberOfCPU		\n");
	printf("                lw	r6,(r2)			\n");

   sharedLocks->epilogue();

     if ( onHardwareG == TRUE ) {

       // decrement loop counter

       printf("                   # hardware looping code                   \n\n");
       printf("		        dla	r2,iterationCount	              \n");
       printf("		        lw	r3,0(r2)		              \n");
       if ( writeSN0LED == TRUE ) {
       printf("		        or	r5,r3,r3		\n");
       }
       printf("		        addiu	r3,r3,-1	# dec counter 	      \n");
       printf("	        	sw	r3,0(r2)	# put back            \n");
       printf("	        	bnez	r3, 2f          # if not finished prep for loop\n");
       printf("		        nop		                              \n");
       
       // get here if we have finished looping

       printf("/* SlaveLock set to Zero at the top of the program, here       \n"); 
       printf("   master cpu waits for all the slave processors to get caught \n"); 
       printf("   in the waitForCleanup: location, once they are all there we \n");
       printf("   turn them loose and continue */                             \n"); 
 
       // wait for all slaves to get to slaveLock, then let them go 
       // and finish program
       printf("	 	       dla	r2,SlaveLock\n"); 
       printf("	    	       sub	r21,1		# r21=numberOfCPU-1   \n");
       printf("1:              lw	r3,0(r2)	# wait for slaves to get to # cpus-1\n");
       printf("		       bne	r3,r21,1b                              \n");
       printf("		       nop                                             \n");
       printf("		       dla      r2,NumCPU    	# now turn slaves loose\n");
       printf("		       sw	r0,0(r2)	#                      \n");
       printf("		       j	EndOfPass		               \n");
       printf("		       nop				               \n");

       // Get here if we have not finished loop,but first wait for cpus
       printf("2:	       nop			# master CPU (#1) comes here to cleanup\n");
       printf("/* wait for other CPU's to all arrive at waitForCleanup label*/\n");
       printf("/* thus wait for SLAVE_LOCK to be NUM_CPU - 1                */\n");

       printf("        	       dla      r2,numberOfCPU		\n");
       printf("        	       lw      r3,0(r2)		        \n");
       printf("                subu    r4,r3,1			\n");
       printf("\n");

       // atomically release slave cpu's

       printf("                  #wait for slave CPUs to all get to waitForCleapup\n");
       printf("                dla      r2,SlaveLock	        \n");
       printf("1:              lw      r3,0(r2)		        \n");
       printf("                bne     r3,r4,1b    		\n");
       printf("                or      r0,r0,r0		        \n");
       printf("			# zero out cpu counters         \n");
       printf("		       dla	r2,AllocCPULock	        \n");	
       printf("		       sw	r0,0(r2)		\n");
       printf("                dla      r2,NumCPU    	        \n");
       printf("	               sw	r0,0(r2)		\n");

       printf("                 #do random delay                \n");
       printf("	 	       mfc0	r2,C0_COUNT		\n");
       printf("		       andi	r2,r2,0xf		\n");
       printf("		       li	r3,%d \n",randDelayMultG);
       printf("		       mult	r2,r3			\n");
       printf("		       mflo	r4			\n");
       printf("3:	       nop				\n");
       printf("		       bne    r4,r0,3b			\n");
       printf("		       addiu	r4,r4,-1		\n");
       printf("		       nop				\n");
       
       if ( writeSN0LED == TRUE ) {

       printf("#define LED0_PTR 0x9200000001220050UL \n");
       printf("#define LED1_PTR 0x9200000001220058UL \n");
       printf("               dli   r6, LED0_PTR \n");
       printf("               sd    r5, 0(r6) \n");
       printf("               srl   r5,r5,8 \n");
       printf("               dli   r6, LED1_PTR \n");
       printf("               sd    r5, 0(r6) \n");

       }

       printf("		       j 	hwLoopBackPoint	# jump to begin of program\n");	
       printf("		       nop				\n");
     }
     else {
       printf("		      j	EndOfPass               \n");
       printf("		      nop			\n");
     }
     // waitForCleanup of looping MP code

     if ( onHardwareG == TRUE ) {
       printf("waitForCleanup:	\n/* come here if we are not Master */\n");

       printf("/* now inc lock so that Master knows each slave */\n");
       printf("/* got here 					*/\n");
       printf("                dla      r2,SlaveLock	\n");
       printf("1:\n");
       if (NopsBeforeLL) {
         int i;
         for (i = 0; i < 32; i++) printf("  nop\n");
       }
       printf("                ll      r3,0(r2)		\n");
       printf("                addu    r3,r3,1			\n");
       printf("                sc      r3,0(r2)		\n");
       if (NopsAfterSC) {
         int i;
/*
         printf("                bne     r3,r0,1f		\n");
*/
         for (i = 0; i < 32; i++) printf("  nop\n");
       }
       if (COP1AfterSC) {
         printf("   cfc1 r0,C1_SR       \n");
       }
       if (SCBranchLikely) {
         printf("                beql    r3,r0,1b		\n");
       } else {
         printf("                beq     r3,r0,1b		\n");
       }
       printf("                nop				\n");
       printf("1:\n");

       printf("/* zero out compare register */\n");
       for ( i = 0; i < maxCompRegG ; i++ )
    	   printf("        	or      r%-2d,r0,r0\n", baseCompRegG + i);	
       printf("\n");


      if ( doSN0N == TRUE || doSN0M == TRUE ) {
       printf("                dla      r8,NumCPU 	# wait for master to turn us loose\n");
       printf("                dli	r2,0x9200000001000020# CPU ID address\n");
       printf("                li	r3,100		        \n");	
       printf("2:              bgtz     r3,1f                   \n");
       printf("                addiu    r3,r3,-1                \n");
       printf("                ld	r4,0(r2)	        \n");
       printf("                li	r3,100		        \n");	
       printf("1:              lw	r9,0(r8)		\n");
       printf("                bne	r9,r0,2b		\n");
       printf("                nop				\n");

      }
      else {
       printf("                dla      r8,NumCPU 	# wait for master to turn us loose\n");
       printf("6:	       lw	r9,0(r8)		\n");
       printf("		       bne	r9,r0,6b		\n");
       printf("		       nop				\n");

      }
       printf("/* If this is last iteration just pass the test and be done with it. */\n");
       printf("		       dla	r2,iterationCount	\n");
       printf("		       lw	r3,0(r2)		\n");
       if ( writeSN0LED == TRUE ) {
       printf("		       or	r5,r3,r3		\n");
       }
       printf("		       bne	r0,r3,2f		\n");
       printf("		       nop				\n");
       printf("		       j	EndOfPass		\n");
       printf("		       nop				\n");
       printf("/* do random delay 			      */\n");
       printf("2:	       mfc0	r2,C0_COUNT		\n");
       printf("		       andi	r2,r2,0xf		\n");
       printf("		       li	r3,%d \n",randDelayMultG);
       printf("		       mult	r2,r3			\n");
       printf("		       mflo	r4			\n");
       printf("3:	       nop				\n");
       printf("		       bne    r4,r0,3b			\n");
       printf("		       addiu	r4,r4,-1		\n");
       printf("		       nop				\n");

       if ( writeSN0LED == TRUE ) {

       printf("#define LED0_PTR 0x9200000001220050UL \n");
       printf("#define LED1_PTR 0x9200000001220058UL \n");
       printf("               dli   r6, LED0_PTR \n");
       printf("               sd    r5, 0(r6) \n");
       printf("               srl   r5,r5,8 \n");
       printf("               dli   r6, LED1_PTR \n");
       printf("               sd    r5, 0(r6) \n");

       }

       printf("/*	       ok	lets go!  	      */\n");
       printf("		       j	hwLoopBackPoint  	\n");	
       printf("		       nop				\n");
     }

     // waitForCleanup of No-looping but SN0
    else if ( doSN0N == TRUE || doSN0M == TRUE ) {

       printf("waitForCleanup:\n");
       printf(" /* come here if we are not Master */\n");
       printf("                dli	  r2,0x9200000001000020# CPU ID address\n");
       printf("2:              ori	  r3,r0,100		\n");	
       printf("1:              nop                                 \n");
       printf("                bne        r0,r3,1b       # endless loop\n");
       printf("                addiu      r3,r3,-1                 \n");
       printf("		       ld	  r3,0(r2)	              # \n");
       printf("                beq        r0,r0,2b       # endless loop\n");	    
       printf("                nop                             \n");

    }
     // generic MP waitForCleanup code
    else {
       printf("waitForCleanup:\n");
       printf(" /* come here if we are not Master */\n");
       printf("1:              nop                             \n");
       printf("                beq       r0,r0,1b       # endless loop\n");
       printf("                nop                             \n");
    }  
 }// if MP

 else {
   // UNIPROCESSOR!!
    if ( onHardwareG == TRUE ) {

       printf("                   # hardware looping code       \n\n");
       printf("		       dla	r2,iterationCount	\n");
       printf("		       lw	r3,0(r2)		\n");
       if ( writeSN0LED == TRUE ) {
       printf("		       or	r5,r3,r3		\n");
       }
       printf("		       addiu	r3,r3,-1		\n");
       printf("	               sw	r3,0(r2)		\n");
       printf("	               bnez	r3, 2f                  \n");
       printf("		       nop		                \n");
       printf("		       j	EndOfPass               \n");
       printf("		       nop			        \n");
       printf("2:                                               \n");

       if ( writeSN0LED == TRUE ) {

       printf("#define LED0_PTR 0x9200000001220050UL \n");
       printf("#define LED1_PTR 0x9200000001220058UL \n");
       printf("               dli   r6, LED0_PTR \n");
       printf("               sd    r5, 0(r6) \n");
       printf("               srl   r5,r5,8 \n");
       printf("               dli   r6, LED1_PTR \n");
       printf("               sd    r5, 0(r6) \n");

       }

       // zero out compare
       printf("/* zero out compare register */\n");
       for ( i = 0; i < maxCompRegG ; i++ )
    	    printf("        	or      r%-2d,r0,r0\n", baseCompRegG + i);
       printf("                j hwLoopBackPoint                \n");
       printf("                nop                              \n");
    }
    else {
       printf("		       j	EndOfPass               \n");
       printf("		       nop			        \n");
    }
 }
       printf("EndOfPass:					\n");
       printf("		halt(PASS)			        \n");
       printf("		nop				        \n");
 
 if ( doMP == TRUE ) {

       printf("fail:	\n");

   if ( onHardwareG == TRUE ) {

       printf("	dla	r2,iterationCount	\n");
       printf("	lw	r28,0(r2)		\n");
   }
       printf("	halt(FAIL)\n");
 }




 printf("sbkillerEnd:\n");

 printf("		.data 	  \n");

 printf("numberOfCPU:	          \n");
 printf("		.align 6  \n");
 printf("		.word  %d \n",numCpuSetByUser);
 printf("hwLoopCount:  # hw loop \n");
 printf("               .word  %d \n",hwIterationCountG);
 printf("iterationCount: # tmp counter for hw loop\n");
 printf("               .word 0x0 \n");


 printf("		.text     \n\n");
}

void printUsage()

{
 printf("sbkiller %s usage: \n",versionG);
 printf("\n");
 printf(" ----- Flags enabling features -----\n");
 printf("\n");
 printf(" -doAluOps 		add in Sign extend stalls and var shifts ALU operations\n");
 printf(" -doAtomic 		insert comments marking instruction boundaries \n");
 printf(" -doBurst              enables burst Mode mixed linear and looping burst \n");
 printf(" -doLinearBurst	burst mode,linear burst\n");
 printf(" -doLoopBurst	        burst mode,looping burst\n");
 printf(" -doChkPrefSync       	Blackbird prefetch testing\n");
 printf(" -doDebugBeast       	special debug option for beast, effects varies \n");
 printf(" -doDlaFetchOp 	Use dla for FETCH op addresses (for fake SN0 system)\n");
 printf(" -doDumpMemState 	turn on sbkiller memory dump\n");
 printf(" -doFpuOp 		insert FP operations \n");
 printf(" -doHardware 		code destined for Hardware (Everest) \n");
 printf(" -doSameMPStream 	creates same code sequence for all MP code streams\n");
 printf(" -doLittleEndian 	Little endian code generation\n");
 printf(" -doMemFile 		create mem.init file in current directory\n");
 printf(" -doMips2 		Mips 2 instruction set \n");
 printf(" -doMips4 		Mips 4 instruction set (T5 option)\n");
 printf(" -doMP 			generate MP code copcodes including ll/sc\n");
 printf(" -doPrefetch 		turn on use of prefetch instructions\n");
 printf(" -doRevEndian 		generate code for reverse endian mode\n");
 printf(" -doShiva 		adjust base address mask for shiva\n");
 printf(" -doFetchOps 		test fetch and op features for SN0\n");


 printf(" -doSN0M 		adjust functionality for SN0 M mode\n");
 printf(" -doSN0N 		adjust functionality for SN0 N mode\n");
 printf(" -doMpTrace 		special option for Beast verification\n");
 printf(" -doSpeedRacer         enable options for speed racer testing \n");
 printf(" -doSyncOp 		insert Sync instructions\n");
 printf(" -doTFP 		generate code for Blackbird/TFP \n");
 printf(" -doICE 		additional features for ICE chip\n");
 printf(" -doT5 			T5 mode \n");
 printf(" -doTextMapped 		Instruction stream should be mapped \n");
 printf(" -doUncaedAcl 	turn on T5 uncached accelerated mode \n");
 printf(" -doUserMode 		Instruction stream should be executed in user mode\n");
 printf("			for use with brkiller\n");
 printf(" -doUserTBLHandler  	user will provide tlb handler, but not use indexes 1-10\n");
 printf(" -doVCE 	       	test VCE \n");
 printf(" -doWordDwordOp       	Only use word or double word instructions \n");
 printf(" -doFaultTolLLSC    	Continue if LLSC data miscompares \n");
 printf(" -doQuietFaults    	Dont complain if LLSC data miscompares \n");
 printf(" -doNopsAfterSC   	   Inserts 32 nops after any SC \n");
 printf(" -doCOP1AfterSC   	   Inserts mfc0 after any SC \n");
 printf(" -doNopsBeforeLL   	   Inserts 32 nops before any LL \n");
 printf(" -doSCBranchLikely  	   Makes all branches following SC branch likely\n");
 printf("\n");
 printf(" ----- Flags diabling features -----\n");
 printf("\n");
 printf(" -noCacheOps 		dont generate any cacheops \n");
 printf(" -noCacheWrap 		turn off cache wrap functionality \n");
 printf(" -noFPUloads  		turn off generation of lxc1/sxc1 instructions \n");
 printf(" -noHitInvSI  		turn off hitInvalidate SI cache ops (for sn0 debug) \n");
 printf(" -noInit               turn off programming of CP0 registers \n");
 printf(" -noLLSC 		turn off generation of ll/lld/sc/scd instructions \n");
 printf(" -noPCacheOp  		no primary cache ops usage to be -P\n");
 printf(" -noScacheOps 		no secondary cache ops \n");
 printf(" -noTLB			no mapped address for ld/st \n");
 printf(" -noUncached 		no uncached accesses \n");

 printf("\n");
 printf(" ----- -a attribute options ----- \n");
 printf("\n");
 printf(" use -a attribute1=xxx,attribute2=yyy etc.. (note commas between attributes)\n");
 printf("\n");
 printf(" baseLdStAddr=0xhhhh  base address from which to do load store from (def: 0x8a002000 \n");
 printf(" baseUncLdStAddr=0xhhhh if set the base for uncached load store (def: baseCache+range+margin \n");
 printf(" baseRegRefRate=dd, 	how often to refresh base registers (def: 100 )\n");
 printf(" burstMin=dd           minimum number of burst store cycles (def: 5 )\n");
 printf(" burstMax=dd           Maximum number of burst store cycles(def:64 ) \n");
 printf(" burstDelta=XX         hex offset increment on each store (def: 256 )\n");
 printf(" burstLdStGap=dd       number of stores to be done before load burst begins (def: 16 )\n");
 printf(" BurstRate=d           Inversely proportional to how often to do burst mode (def: 100 )\n");
 printf(" hwDelayMult=dd,	Mult factor controlling random delay before loop back on hardware system (def: )\n");
 printf(" hwLoop=dd             how many times to loop over whole code (mainly for hardware (def: 1)\n");
 printf(" iterations=dd,  	how many instructions to create (approx) (def: 100)\n");
 printf(" MpNumShareLocks=dd, 	share dd locks between processors(def: 10 )\n");
 printf(" MpLockInc=n, 		each cpu increment every locks n times (def: 5) \n");
 printf(" MpLockBaseAddr=0xh,	address for shared locks used in MP mode (def: \n");
 printf(" MpNumCpu=dd,		generate MP code for dd CPU's. valid values are 4 8 16 32 (def: 4 ) \n");
 printf(" MpLockAddrStep=dd,	MP shared lock address spacing, (def: 128 )\n");
 printf(" MpCheckNumCpu=dd,	total number of CPU's provided at compile time, not computed \n");
 printf(" MpLockGenRate=n,      MP shared lock usage rate. Shared locks are\n");
 printf("			generated aprox every 'n'th instruction(def: 10 )\n");
 printf(" nodeID=0xnnn,         Node ID's for sn0,multiple can be specified \n");
 printf(" recycleLimit=n,	if n is 0, no self checking,if > number of\n");
 printf("			comp reg,reuse comp regs  (def: 0)\n\n");
 printf(" range=0xhhhh, 	range of address above the base to perform I/O on (def: 0x100\n");
 printf(" randomSeed=dd,		not be set unless for debug\n");
 printf(" syncCount=dd,		sync instruction repeated every n instructions  (def: 20 ) \n");
 printf(" syncRdRate=dd,        rate to read SpeedRacer sync registers, greater num, means less chance (def: 30\n");
 printf(" watchdogTimeout=dd,        loopsize for SN0 watchdog timer (def: 100000\n");
 printf("\n");
 printf(" -m <fileName>         specifies the SN0 MP cpu -> base Reg mapping file\n");
 printf("\n");
 printf(" ----- -w options control cache wrap addressing ----- \n");
 printf("  usage: \n");
 printf("        -w  wrap1=0xnnn,wrap2=0xmmm 	cache wrap sizes wrap1 -> wrap8 supported\n");
 printf("  	     note wrapRate=n (def: 4) is inversely proportional to n, we generate cache \n");
 printf("       	wrapped Rate n=0 means always) \n"); 
 printf("\n");
 printf(" ----- -x options control the T5 Systems interface external events ----- \n");
 printf("	   randRate controls external random event rate\n");
 printf("  usage:\n");
 printf(" 	 -x interrupt=nnn,intervention=nnn,invalidate=nnn,nack=nnn,\n");
 printf("	    rdRdy=nnn,wrRdy=nnn,randRate=nnn,\n");
 printf("    	    rdRdyLen=nnn,wrRdyLen=nnn (max duration, default 100) \n");
 printf(" -------------------- \n");



 exit(EXIT_FAILURE);
}
/**********/
UINT setSeed(userSetSeed,seed)
int userSetSeed;
unsigned seed;
{
  long		currentTime;
 if ( userSetSeed == FALSE ) {
	time(&currentTime);
	srandom(currentTime);
	printf("/* random seed %d */\n", currentTime);
 	return(currentTime);
	
	
 } 
 else {
	srandom(seed);
	printf("/* random seed %d */\n", seed);
 	return(seed);
 }
}

/**********/
void clearRegisters()
{
 int i;
	/* clear compare registered since they have not been initialized.*/
	for ( i = 0 ; i < maxCompRegG ; i++ )
	  printf("	or	r%d,r0,r0\n",baseCompRegG+i);
	for ( i = 0 ; i < maxCompFpuRegG ; i++ ) 
	  printf("	dmtc1	r0,fp%d\n",baseCompFpuRegG+i);	 
	  

}

void loadHwLoopCount () {

  if ( onHardwareG == TRUE ) {
    printf("\t # load loop count from reference to tmp counter\n");
    printf("	dla	r2,hwLoopCount          # reference hw loop count\n");
    printf("	lw	r3,0(r2)		# \n");
    printf("	dla	r2,iterationCount	# tmp counter used for \n");
    printf("	sw	r3,0(r2)		# this run. (we can hit reset and reuse code)\n");
  }
}

/**********/
main (int argc, char **argv)
{
	int		result;
	int		c, i, j, k;
	int	        errflg = 0; 
	unsigned	seed = 0;
	int		randVal;
	int		coherency;

	int		tmpRdRdyDelay=0;
	int		tmpWrRdyDelay=0;

	extern char	*optarg;
	extern int	optind, opterr;

	char *options, *value;
	FILE * tmpFileHandle;
	char * resCharPtr;
	char tmpStr[256];

	/* global defaults */
	strcpy(mpMapFileName,"");
	
        sharedLocks        = new sharedOp;

	baseLdStAddr       = DEF_BASE_LD_ST_32;
	baseLdStAddr64     = DEF_BASE_LD_ST_64;
	addrRangeG    	   = DEF_ADDR_RANGE;



	randEventRateG     = 0;
	lockAddrStepG      = 128;
	sharedLockCounterG = 0;
	sharedLockGenRateG = 10;

	baseAddrSet     = FALSE;

	baseRegG        = FIRST_REG_AVAIL;
        baseFpuRegG     = 0;

	maxBaseRegG     = MAX_BASE_REG;
	maxImmdRegG     = MAX_IMMD_REG;
	maxCompRegG     = MAX_COMP_REG;

	maxImmdFpuRegG  = MAX_IMMD_FPU_REG;
	maxCompFpuRegG  = MAX_COMP_FPU_REG;

	recycleLimitG 	   = maxCompRegG -1;

	memoryModel.setAddrRange( addrRangeG );

    
	char *attrOpts[] = {
	    #define BASE_LD_ST_ADDR	0
	    "baseLdStAddr",
	    #define RANGE		1
	    "range",
	    #define ITERATIONS 		2
	    "iterations",
	    #define BASE_REG_REFRESH	3
	    "baseRegRefRate",
	    #define COUNT_MULT		4
	    "hwDelayMult",
	    #define SYNC_COUNT		5
	    "syncCount",
	    #define MP_NUM_CPUS		6
	    "MpNumCpu",
	    #define RAND_SEED		7
	    "randomSeed",
	    #define MP_NUM_LOCKS	8
	    "MpNumShareLocks",
	    #define MP_LOCK_INC		9
	    "MpLockInc",
	    #define MP_LOCK_BASE_ADDR	10
	    "MpLockBaseAddr",
	    #define MP_LOCK_ADDR_STEP	11
	    "MpLockAddrStep",
	    #define MP_LOCK_GEN_RATE    12
	    "MpLockGenRate",
	    #define MP_CHECK_CPUS	13
	    "MpCheckNumCpu",
	    #define RECYCLE_LIMIT	14
	    "recycleLimit",
	    #define BASE_UNC_LD_ST_ADDR	15
	    "baseUncLdStAddr",
            #define NODE_ID             16
            "nodeID",
            #define HW_INTERATION_COUNT 17
            "hwLoop",
            #define SPEED_RD_SYNC_RATE  18
            "syncRdRate",
            #define BURST_MIN           19
            "burstMin",
            #define BURST_MAX           20
            "burstMax",
            #define BURST_DELTA         21
            "burstDelta",
            #define BURST_RATE          22
            "burstRate",
            #define BURST_LDST_GAP      23
            "burstLdStGap",
            #define WATCHDOG_TIMEOUT    24
            "watchdogTimeout",

	    NULL,
	};
	char *extOpts[] = {

		#define EXT_INTR  0
			"interrupt",
		#define EXT_INV	  1
			"invalidate",
		#define EXT_INTV  2
			"intervention",
		#define EXT_NACK  3
			"nack",
		#define EXT_RDRDY_DELAY 4
			"rdRdyLen",
		#define EXT_WRRDY_DELAY 5
			"wrRdyLen",
		#define EXT_RDRDY 6
			"rdRdy",
		#define EXT_WRRDY 7
			"wrRdy",
	        #define EXT_RAND_RATE 8
			"randRate",
			NULL};
			
	char *wrapOpts[] = {
		#define WRAP1_SIZE 0
			"wrap1",
		#define WRAP2_SIZE 1
			"wrap2",
		#define WRAP3_SIZE 2
			"wrap3",
		#define WRAP4_SIZE 3
			"wrap4",
		#define WRAP5_SIZE 4
			"wrap5",
		#define WRAP6_SIZE 5
			"wrap6",
		#define WRAP7_SIZE 6
			"wrap7",
		#define WRAP8_SIZE 7
			"wrap8",
		#define WRAP_RATE  8
			"wrapRate",
			NULL};

	



	printf("/* Command line used for this code stream:\n");

	if ( argc == 1 ) {
	    printUsage();
	    exit(EXIT_SUCCESS);
	}
	for ( i = 1 ; i < argc ; i++ )
		printf("%s ",argv[i]);

	printf("\n*/\n");
	 
	int optionSet = FALSE;
	while (( c = getopt(argc, argv, "a:d:m:n:w:x:T:")) != -1 ) {

	  optionSet = TRUE;

		switch (c) {

		case 'a':
			options = optarg;

			while ( (*options != '\0') ) {
			      switch ( getsubopt(&options,attrOpts,&value)) { 

			      case NODE_ID:
				if ( value != NULL ) {
				  ULL nodeID;
				  sscanf(value, "%llx", &nodeID);
				  memoryModel.addNodeID(nodeID);
				}
				break;

				case BASE_LD_ST_ADDR:

					if ( value != NULL ) {
					    if ( strlen(value) > 10 ) {
					       sscanf(value, "%llx", &baseLdStAddr64);
					       memoryModel.addBaseAddr(baseLdStAddr64); 
					       baseAddrSet = TRUE;
					       baseAddr64BitMode = TRUE;
					   }	
					   else { /* 32 bit value and we need to extract data */
					       sscanf(value, "%lx", &baseLdStAddr);
					       baseAddrSet = TRUE;
					       baseLdStAddr64 = (long long ) baseLdStAddr;
					       if ( baseLdStAddr64 & SIGNBIT_OF_32_IN_64 ) {
						   baseLdStAddr64 |= 0xffffffff00000000LL;
						   memoryModel.addBaseAddr(baseLdStAddr64);
						   baseAddr64BitMode = FALSE;
					       }
					       else { // just add it for 1->1 va->pa mapping
						 memoryModel.addBaseAddr(baseLdStAddr64);
						 baseAddr64BitMode = TRUE;
					       }
					   }

					   printf("/* BASE_LD_ST_ADDR set */ \n"); 
					}
					break;

				case BASE_UNC_LD_ST_ADDR:

					if ( value != NULL ) {
					       sscanf(value, "%llx", &baseUncLdStAddr64);
					       memoryModel.addUncBaseAddr(baseUncLdStAddr64); 
					       baseUncAddrSet = TRUE;
					       printf("/* BASE_UNC_LD_ST_ADDR set */ \n"); 
					   }

					break;

				case RANGE:
					if ( value != NULL ) {
					   sscanf(value, "%x", &addrRangeG);
					   memoryModel.setAddrRange( addrRangeG );
					   printf("/* RANGE set */ \n"); 	
					}
					break;
				case ITERATIONS:
					if ( value != NULL ) {
					   sscanf(value, "%d", &countG);	
					   printf("/* Instruction count set */ \n"); 	
					}
					break;
			      case HW_INTERATION_COUNT:
					if ( value != NULL ) {
					   sscanf(value, "%d", &hwIterationCountG);
					   onHardwareG = TRUE;
					   printf("/* HW looping set */ \n"); 	
					}
					break;
				
				case BASE_REG_REFRESH:
					if ( value != NULL ) {
					   sscanf(value, "%d", &loopG);
					   printf("/*  base register refresh rate set */ \n"); 		
					}
					break;
				case COUNT_MULT:
					if ( value != NULL ) {
					   sscanf(value, "%d", &randDelayMultG);	
					   printf("/*  hardware rand delay multiply factor count set */ \n"); 	
					}
					break;

				case SYNC_COUNT:
					if ( value != NULL ) {
					   sscanf(value, "%d", &syncCountG);	
					   printf("/*  sync count set */ \n"); 	
					}
					break;
				case SPEED_RD_SYNC_RATE:
					if ( value != NULL ) {
					   sscanf(value, "%d", &speedReadRateG);	
					   if ( speedReadRateG < 1 ) speedReadRateG = 1;
					   printf("/*  speedRacer sync Read rate %d */ \n",speedReadRateG); 	
					}
					break;
				case MP_NUM_CPUS:
					if ( value != NULL ) {
					   sscanf(value, "%d", &numOfCpuG);	
					   printf("/*  Max number of MP CPU's allowed set */ \n"); 	
					}
					break;
				case MP_CHECK_CPUS:
					if ( value != NULL ) {
					   sscanf(value, "%d", & numCpuSetByUser);	
					   printf("/*  number of runtime CPU's set */ \n"); 	
					}
					break;
				case RAND_SEED:
					if ( value != NULL ) {
					   sscanf(value, "%d", &seed);	
					   userSetSeed = TRUE;
					   printf("/*  random seed set */ \n"); 	
					}
					break;
				case MP_NUM_LOCKS:
					if ( value != NULL ) {
					   sscanf(value, "%d", &numLocksG);	
					   printf("/*numlocks %d*/ \n",numLocksG);
					}
					break;
				case MP_LOCK_INC:
					if ( value != NULL ) {
					   sscanf(value, "%d", &lockIncG);	
					   printf("/*  MP lock increment set */ \n"); 	
					}
					break;
				case RECYCLE_LIMIT:
					if ( value != NULL ) {
					   sscanf(value, "%d", &recycleLimitG);	
					   printf("/*recycle limit %d*/ \n",recycleLimitG);
					}
					break;
				case MP_LOCK_BASE_ADDR:
					if ( value != NULL ) {
					   sscanf(value, "%llx", &lockBase64G);	

					   if ( lockBase64G & 0x80000000 )
					       lockBase64G = 0xffffffff00000000 | (ULL) lockBase64G;

					   lockBaseSetG = TRUE;
					   printf("/*  lock base address set */ \n"); 	
					}
					break;
				case MP_LOCK_GEN_RATE:
					if ( value != NULL ) {
					   sscanf(value, "%d", &sharedLockGenRateG);
					   if ( sharedLockGenRateG <= 0 ) sharedLockGenRateG = 1;	
					   printf("/*  MP lock usage set */ \n"); 	
					}
					break;
				case MP_LOCK_ADDR_STEP:
					if ( value != NULL ) {
					   sscanf(value, "%d", &lockAddrStepG);
					   if ( lockAddrStepG < 8 ) lockAddrStepG = 8;
					   lockAddrStepG &= 0xfffffff8;
					   printf("/*  MP lock address step set */ \n"); 	
					}
					break;
				case BURST_MIN:
					if ( value != NULL ) {
					   sscanf(value, "%d", &burstMinG);
					   if ( burstMinG < 1 ) burstMinG = 1;
					   doBurstG = TRUE;
					   printf("/*  BURST_MIN %d */ \n",burstMinG); 	
					}
					break;
				case BURST_MAX:
					if ( value != NULL ) {
					   sscanf(value, "%d", &burstMaxG);
					   doBurstG = TRUE;
					   printf("/*  BURST_MAX %d */ \n",burstMaxG); 	
					}
					break;
				case BURST_DELTA:
					if ( value != NULL ) {
					   sscanf(value, "%x", &burstDeltaG);
					   if ( burstDeltaG < 8 ) burstDeltaG = 8;
					   // make aligned
					   burstDeltaG &= 0xfffffff8;
					   doBurstG = TRUE;
					   printf("/*  BURST_DELTA %d */ \n",burstDeltaG); 
					}
					break;
			         case BURST_RATE:
					if ( value != NULL ) {
					   sscanf(value, "%d", &burstRateG);
					   if ( burstRateG < 10 ) burstRateG = 10;
					   printf("/*  BURST_RATE %d */ \n",burstMaxG); 
					   doBurstG = TRUE;
					}
					break;

			         case BURST_LDST_GAP:
					if ( value != NULL ) {
					   sscanf(value, "%d", &burstLdStDelayG);
					   if ( burstLdStDelayG < 1 ) burstRateG = 1;
					   printf("/*  BURST_LD_ST_GAP %d */ \n",burstLdStDelayG); 
					   doBurstG = TRUE;
					}
					break;

			         case WATCHDOG_TIMEOUT:
					if ( value != NULL ) {
					   sscanf(value, "%llu", &watchdogTimeout);
					   printf("/*  WATCHDOG_TIMEOUT %llu */ \n", watchdogTimeout); 
					}
					break;

				default :
					errflg++;
			        }
			}
			break;

                
		case 'd':
		    if ( (strcmp(optarg,"oDebugBeast")) == 0 ) {
				doDebugBeastG = 1;
				printf("/* doDebugBeastG == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oBurst")) == 0 ) {
				doBurstG = TRUE;
				burstModeG = RAND_SEL_BURST;
				printf("/* BurstMode == linear and loop  burst */\n");
		    }
		    else if ( (strcmp(optarg,"oSN0Lab")) == 0 ) {

				writeSN0LED = TRUE;
				disableNodeRead = TRUE;
            watchdogTimer = TRUE;
				printf("/* SN0 Lab options (disableNodeRead, write LED's) */\n");
		    }

		    else if ( (strcmp(optarg,"oLinearBurst")) == 0 ) {
				doBurstG = TRUE;
				burstModeG = FLAT_BURST;
				printf("/* BurstMode == linear  burst mode */\n");
		    }
		    else if ( (strcmp(optarg,"oLoopBurst")) == 0 ) {
				doBurstG = TRUE;
				burstModeG = LOOP_BURST;
				printf("/* BurstMode == loop burst mode */\n");
		    }
		    else if ( (strcmp(optarg,"oAtomic")) == 0 ) {
				doAtomicG = 1;
				printf("/* doAtomic == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oShiva")) == 0 ) {
				doShivaG = TRUE;
            watchdogTimer = TRUE;
				printf("/* doShiva == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oSpeedRacer")) == 0 ) {
				doSpeedG = TRUE;
            watchdogTimer = TRUE;
				printf("/* doSpeedRacer == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oFetchOps")) == 0 ) {
				doFetchOps = TRUE;
				doMP = TRUE;
				printf("/* doFetchOps == doMP == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oSN0M")) == 0 ) {
				doSN0M = TRUE;
				doMP = TRUE;
				printf("/* doSN0M == doMP == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oSN0N")) == 0 ) {
				doSN0N = TRUE;
				doMP = TRUE;
				printf("/* doSN0N == doMP == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oDlaFetchOp")) == 0 ) {
				useSharedLabelAddr = TRUE;
				doFetchOps = TRUE;
				doMP = TRUE;
				printf("/* doFetchOps == doMP == useSharedLabelAddr == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oSameMPStream")) == 0 ) {
				sameMPcodeG = TRUE;
				printf("/* sameMPStream == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oMips2")) == 0 ){
				mips2G = TRUE;
				printf("/* Mips2 == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oMips4")) == 0 ){
				mips4G = TRUE;
				printf("/* Mips4 == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oAluOps")) == 0 ){
				doAluG = TRUE;
				printf("/* ALUOPS == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oTFP")) == 0 ){
				doBlackBirdG = TRUE;
				printf("/* TFP == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oUncachedAcl")) == 0 ){
				uncachedAccG = TRUE;
				printf("/* Uncached accelerate == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oFpuOp")) == 0 ){	
		      //doFPopG = TRUE;
				printf("/* FPUOPS is now disabled */\n");	
		    }
		    else if ( (strcmp(optarg,"oHardware")) == 0 ) {
				onHardwareG = TRUE;
				printf("/* onHardware == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oDumpMemState")) == 0 ){
				printMemG = TRUE;
				printf("/* dumpMemState == TRUE */\n");	
		    }
		    else if ( (strcmp(optarg,"oSyncOp")) == 0 ) {
				 useSyncG = TRUE;
				printf("/* doSyncOp == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oVCE")) == 0 ) {
				 VCEmodeG = TRUE;     
				 printf("/* doVCE == TRUE */\n");
		    }
		    else if ( (strcmp(optarg,"oRevEndian")) == 0 ) {
			reverseEndianG= TRUE;
			ImappedG      = TRUE;
			IkernelG      = FALSE;
			printf("/* doRevEndian == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oMP")) == 0 ) {
			doMP    = TRUE;
			doLlScG = TRUE;
			printf("/* doMP == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oT5")) == 0 ) {
				T5ModeG = TRUE;
				mips4G = TRUE;
				 printf("/* doT5 == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oTextMapped")) == 0 ) {
				  ImappedG = TRUE;
				 printf("/* doTextMapped == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oUserTBLHandler")) == 0 ) {
				   userTLBHandlerG = TRUE; 
				 printf("/* doUserTLB == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oChkPrefSync")) == 0 ) {
				  checkPrefSyncG = TRUE; 
				 printf("/* checkPrefSync == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oICE")) == 0 ) {
				  forICE = TRUE;
				 printf("/* ICE  == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oPrefetch")) == 0 ) {
				   prefetchG = TRUE;
				 printf("/* prefetch == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oMpTrace")) == 0 ) {
				   doMpTrace = TRUE;
				 printf("/* doMpTrace == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oLittleEndian")) == 0 ) {
				   BigEndianG = FALSE;   
				 printf("/* littleEndian == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oUserMode")) == 0 ){
				   IkernelG = FALSE; 
				   ImappedG = TRUE;  
				 printf("/* userMode == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oMemFile")) == 0 ){
				   doMemFileOutputG = TRUE;  
				 printf("/* MemFile == TRUE */\n"); 
		    }
		    else if ( (strcmp(optarg,"oBarrier")) == 0 ){
				   doBarrierG = TRUE;  
				 printf("/* Barrier == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oWordDwordOp")) == 0 ){
				   doWordDwordOP = TRUE;  
				 printf("/* Word/DWord ops only == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oFaultTolLLSC")) == 0 ){
				   ContinueOnLLSCMiscompare = TRUE;  
				 printf("/* Contine on LL/SC data miscompare == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oQuietFaults")) == 0 ){
				   QuietLLSCMiscompare = TRUE;  
				 printf("/* Silent LL/SC data miscompare == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oNopsAfterSC")) == 0 ){
				   NopsAfterSC = TRUE;  
				 printf("/* 32 nops inserted after all SC == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oCOP1AfterSC")) == 0 ){
				   COP1AfterSC = TRUE;  
				 printf("/* 32 nops inserted after all SC == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oSCBranchLikely")) == 0 ){
				   SCBranchLikely = TRUE;  
				 printf("/* branch likely after all SC == TRUE */\n"); 
		    }	
		    else if ( (strcmp(optarg,"oNopsBeforeLL")) == 0 ){
				   NopsBeforeLL = TRUE;  
				 printf("/* 32 nops inserted in front of all LL == TRUE */\n"); 
		    }	
		    else
			errflg++;
			break;

		case 'm':

		    strcpy(mpMapFileName,optarg);

		    break;


		case 'n':

		    if ( (strcmp(optarg,"oLLSC")) == 0 ) {	
			        printf("/*  == FALSE */\n"); 
				doLlScG = FALSE ;
		    }
		    else if ( (strcmp(optarg,"oInit")) == 0 ) {
			        printf("/*  noInit == TRUE  */\n"); 
				noInitG = TRUE;
		    }
		    else if ( (strcmp(optarg,"oHitInvSI")) == 0 ) {
			        printf("/*  noHitInvSI == TRUE  */\n"); 
				byPassHitInvSI = TRUE;
		    }
		    else if ( (strcmp(optarg,"oFPUloads")) == 0 ) {
			        printf("/*  == FALSE */\n"); 
				noFPUloadsG= TRUE;
		    }
		    else if ( (strcmp(optarg,"oCacheWrap")) == 0 ) {
			        printf("/*  == FALSE */\n"); 
				cacheWrapG = FALSE;
		    }
		    else if ( (strcmp(optarg,"oPCacheOp")) == 0 ) {
			        printf("/*  == FALSE */\n"); 
				noPrimaryCacheG = TRUE;
		    }
		    else if ( (strcmp(optarg,"oUncached")) == 0 ) {
			        printf("/*  == FALSE */\n"); 
				 noUncachedG = TRUE;
		    }
		    else if ( (strcmp(optarg,"oSCacheOps")) == 0 ) {
			        printf("/*  == FALSE */\n"); 
				  noSecondCacheG = TRUE;
		    }
		    else if ( (strcmp(optarg,"oCacheOps")) == 0 ) {
			        printf("/*  == FALSE */\n"); 
				  noCacheOpsG  = TRUE;
		    }
		    else if ( (strcmp(optarg,"oTLB")) == 0 ) {
			        printf("/*  == FALSE */\n"); 
				  useTLB  = FALSE;
		    }
		    else 
			errflg++;
			break;


		case 'w':
			options = optarg;
			UINT cacheWrapSize;

			while ( (*options != '\0') ) {
			      switch ( getsubopt(&options,wrapOpts,&value)) { 

				case WRAP1_SIZE:
					if ( value != NULL ) {
					   sscanf(value, "%x", &cacheWrapSize);	
					   if ( cacheWrapSize >= CACHE_WRAP_MIN )
					       memoryModel.setCacheWrapAddr(0,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP2_SIZE:
					if ( value != NULL ) {
					   sscanf(value, "%x", &cacheWrapSize);	
					   if ( cacheWrapSize >= CACHE_WRAP_MIN )
					       memoryModel.setCacheWrapAddr(1,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP3_SIZE:
					if ( value != NULL ) {
					    sscanf(value, "%x", &cacheWrapSize);	
					    if ( cacheWrapSize >= CACHE_WRAP_MIN )
						memoryModel.setCacheWrapAddr(2,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP4_SIZE:
					if ( value != NULL ) {
					    sscanf(value, "%x", &cacheWrapSize);	
					    if ( cacheWrapSize >= CACHE_WRAP_MIN )
						memoryModel.setCacheWrapAddr(3,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP5_SIZE:
					if ( value != NULL ) {
					    sscanf(value, "%x", &cacheWrapSize);	
					    if ( cacheWrapSize >= CACHE_WRAP_MIN )
						memoryModel.setCacheWrapAddr(4,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP6_SIZE:
					if ( value != NULL ) {
					    sscanf(value, "%x", &cacheWrapSize);	
					    if ( cacheWrapSize >= CACHE_WRAP_MIN )
						memoryModel.setCacheWrapAddr(5,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP7_SIZE:
					if ( value != NULL ) {
					    sscanf(value, "%x", &cacheWrapSize);	
					    if ( cacheWrapSize >= CACHE_WRAP_MIN )
						memoryModel.setCacheWrapAddr(6,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP8_SIZE:
					if ( value != NULL ) {
					    sscanf(value, "%x", &cacheWrapSize);	
					    if ( cacheWrapSize >= CACHE_WRAP_MIN )
						memoryModel.setCacheWrapAddr(7,cacheWrapSize & ADDR_FUNC_MASK);
					}
					break;
				case WRAP_RATE:
					if ( value != NULL ) {
					    sscanf(value, "%d", &cacheWrapRateG);	
					    if ( cacheWrapRateG < 1 ) cacheWrapRateG = 1;
					}
					break;

				default :
					errflg++;
			        }
			}
			break;

		case 'x':

			options = optarg;	
			while ( (*options != '\0') ) {
			      switch ( getsubopt(&options,extOpts,&value)) { 
				case EXT_INTR:	
		        	   if ( value != NULL ) {
                                           sscanf(value, "%d", &extIntrCountG);
			    		   extIntrG = TRUE;
				   }
				   break;
				case EXT_INV:	
		        	   if ( value != NULL ) {
					sscanf(value, "%d", &extInvCountG);
					extInvG = TRUE;
				   }
			  	   break;
				case EXT_INTV:	
		        	   if ( value != NULL ) {
					sscanf(value, "%d", &extIntvCountG);
					extIntvG = TRUE;
				   }
				   break;
				case EXT_NACK:	
		        	   if ( value != NULL ) {
					sscanf(value, "%d", &extNackCountG);
					extNackG = TRUE;
				   }
				   break;
				case EXT_RDRDY:	
		        	   if ( value != NULL ) {
					sscanf(value, "%d", &extRdRdyCountG);
					rdRdyG = TRUE;
				   }
			           break;
				case EXT_WRRDY:	
		        	   if ( value != NULL ) {
					sscanf(value, "%d", &extWrRdyCountG);
					wrRdyG = TRUE;
				   }
			           break;
				case EXT_RDRDY_DELAY:	
		        	   if ( value != NULL ) {
					sscanf(value, "%d", &tmpRdRdyDelay);
					if ( tmpRdRdyDelay > minRdRdyDuration ) maxRdRdyDuration = tmpRdRdyDelay;
					rdRdyG = TRUE;
				   }
				   break;
				case EXT_WRRDY_DELAY:	
		        	   if ( value != NULL ) {
					sscanf(value, "%d", &tmpWrRdyDelay);
					if ( tmpWrRdyDelay > minWrRdyDuration ) maxWrRdyDuration = tmpWrRdyDelay;
					wrRdyG = TRUE;
				   }
				   break;

				case EXT_RAND_RATE:
					if ( value != NULL ) {
					    sscanf(value, "%d", &randEventRateG);
					    if ( randEventRateG == 0 ) randEventRateG = 1 ;
						}
					break;

				default :
					errflg++;
			      }
				
			}
			break;

		case 'T':
		    strcpy(templateFile,optarg);
			break;

	        default:
		case '?':
			errflg++;
		}
	}

	if ( errflg || ( optionSet == FALSE) ) printUsage();

	if ( baseAddr64BitMode == FALSE && (doSN0M == TRUE || doSN0N == TRUE ) ) {
	  printf("32 bit addressing used with SN0 Mode, use 64 bit addressing.\n");
	  exit(EXIT_FAILURE);
	}

	// adjust min/max difference incase we have problem

	if ( burstMaxG < ( burstMinG + 2 )) burstMaxG = burstMinG + 10;

	if ( (burstDeltaG < 256) && doMP == TRUE ) {
	  printf("BurstDelta < 256 while in MP mode, increase delta for correct operation\n");
	  exit (EXIT_FAILURE);
	}
	   // if base address is not set

	if ( baseAddrSet == FALSE )
	    memoryModel.addBaseAddr(0xffffffff8a002000);


	//if ( (burstDeltaG * burstMaxG) > 0x7ff0 ) {
	//printf("BurstDelta size is greater than max offset allowed, decrease burstDelta \n");
	//exit(EXIT_FAILURE);
	//}
	
	if (( (UINT) (lockAddrStepG * lockAddrStepG) + LOCK_BASE_SW) > (UINT) 0xa0000000 ) {
	    printf("number of locks* lock step + base will cause address overlow into uncached space!!\n");
	    exit(EXIT_FAILURE);
	}

	if ( doSN0M == TRUE && doSN0N == TRUE ) {
	  fprintf(stderr,"Both doSN0M and doSN0N mode selected\n");
	  exit(EXIT_FAILURE);
	}
 
	  // make sure we generate enough code

	if ( (doSN0M | doSN0N) == TRUE ) 
	  while ( (memoryModel.nodeIDCnt*2 > numOfCpuG ) && (numOfCpuG < 33) ) 
	    numOfCpuG *= 2;
	  
	  
	  // setup default SN0 MP map table of equal probablity

	for ( i = 0; i < MAX_CPU_MP ; i++ )
	    for ( j = 0; j < MAX_CPU_MP; j++ )
		mpMapTable[i][j] = 1;

	  // if user specified file, then read it

	if ( mpMapFileName != NULL )
	    result = parseMpTable( mpMapFileName );

	  // load up uncached base addresses
 
	memoryModel.computeAllBases();
	/*
        if ( memoryModel.nodeIDCnt > 1 && doSN0M == FALSE && doSN0N == FALSE ) {
	  fprintf(stderr,"NodeID's set but neither doSN0M or doSN0N mode selected\n");
	  exit(EXIT_FAILURE);
	}
	*/
	  // forces seed to be set once, and then from here on any time
	  // we set seed, it gives are the same number sequence back.

	seed=setSeed(userSetSeed,seed);


	// T5 event counters

	extInvCounterG     = random()%extInvCountG;
	extIntvCounterG    = random()%extIntvCountG;
	extIntrCounterG    = random()%extIntrCountG;
	extNackCounterG    = random()%extNackCountG;

	if ( doAtomicG == TRUE ) {

		maxBaseRegG = MAX_ATOMIC_BASE_REG;
		maxImmdRegG = MAX_ATOMIC_IMMD_REG;
		maxCompRegG = MAX_ATOMIC_COMP_REG;
	}

	PageOffset = baseLdStAddr & 0x0fff;

	if ( lockBaseSetG == FALSE ) {

	    if ( onHardwareG == TRUE ) {
		lockBaseG = LOCK_BASE_HW;
		lockBase64G = 0xffffffff00000000 | (ULL) LOCK_BASE_HW;
	    }
	    else {
		lockBaseG = LOCK_BASE_SW;
		lockBase64G = 0xffffffff00000000 | (ULL) LOCK_BASE_SW;
	    }
	}


	if ( *templateFile != NULL ) {
	    tmpFileHandle = fopen(templateFile,"r");
	    if ( tmpFileHandle == NULL ) {
	        printf("Could not open MP template file \n");
	       exit (EXIT_FAILURE);
	    }
	    resCharPtr = fgets(tmpStr,256,tmpFileHandle);
	    int rowIndex= 0;
	    int colIndex= 0;
	    while ( (strncmp(TEMPLATE_MARKER,tmpStr,strlen(TEMPLATE_MARKER)) != 0) && resCharPtr != NULL ) {
		printf("%s",tmpStr);
		resCharPtr = fgets(tmpStr,256,tmpFileHandle);
	    }
	}

	if ( doMP== TRUE ) {
		if (  numLocksG > MAXLOCKS ) {
			printf( "maximum lock count exceeded! \n");
			exit(EXIT_FAILURE);
		}

		// init code for standard LL/SC

		for (i = 0 ; i < numLocksG ; i++ ) { 
		    sharedLocks->init(FUNC_LLSC_INC, lockBase64G + (ULL) (i * lockAddrStepG),lockIncG,numOfCpuG );
		}
		if ( doFetchOps == TRUE ) {

		    // FETCH INC (LD)

		    for (i = 0 ; i < numLocksG ; i++ ) { 
			char tmpStr[256];
			if ( useSharedLabelAddr == TRUE ) {
			  sprintf(tmpStr,"sharedSN0Addr+%d",i*64);
			  sharedLocks->init(FUNC_FETCH_INC_OP,tmpStr,lockIncG,numOfCpuG );
			}
			else {
			  sharedLocks->init(FUNC_FETCH_INC_OP,sharedSN0AddrG+(i*64),lockIncG,numOfCpuG );		  
			}
		    }

		    // FETCH DEC (LD)

		    for (i = numLocksG ; i < (2*numLocksG) ; i++ ) { 
			char tmpStr[256];
			if ( useSharedLabelAddr == TRUE ) {
			  sprintf(tmpStr,"sharedSN0Addr+%d",i*64);
			  sharedLocks->init(FUNC_FETCH_DEC_OP,tmpStr,lockIncG,numOfCpuG );
			}
			else {
			  sharedLocks->init(FUNC_FETCH_DEC_OP,sharedSN0AddrG+(i*64),lockIncG,numOfCpuG );
			}
		    }

		    ULL bit = 0x01LL;
		    char tmpStr[256];

		    // OR_OP (LD)

		    for ( k = (2*numLocksG) ; k < (3*numLocksG) ; k++ ) {

			bit = 0x01LL;
			for ( j = 0 ; j < numOfCpuG ; j++ ) {
			    for ( i = 0 ; i < 64/numOfCpuG ; i++ ) {
			      if ( useSharedLabelAddr == TRUE ) {
				sprintf(tmpStr,"sharedSN0Addr+%d",k*64);
				sharedLocks->initL(FUNC_OR_OP,tmpStr , bit, j );
			      }
			      else {
				sharedLocks->initL(FUNC_OR_OP,sharedSN0AddrG+(k*64), bit, j );				
			      } 
				bit = bit << 1;
			    }
			}
			if( useSharedLabelAddr == TRUE ) { 
			  sharedLocks->addToCheckWorkQueue(  new Work( CHECK_BIT | FUNC_OR_OP ,tmpStr , (ULL) 0xffffffffffffffffLL  ));
			  sharedLocks->addToInitWorkQueue( new Work ( FUNC_INIT_ADDRESS_OP,tmpStr , (ULL) 0x0 ) );
			}
			else {
			  sharedLocks->addToCheckWorkQueue(  new Work( CHECK_BIT | FUNC_OR_OP, sharedSN0AddrG+(k*64), (ULL) 0xffffffffffffffffLL  ));
			  sharedLocks->addToInitWorkQueue( new Work ( FUNC_INIT_ADDRESS_OP,sharedSN0AddrG+(k*64), (ULL) 0x0 ) );
			}
		    }

		    // AND_OP (SD)

		    for ( k = (3*numLocksG) ; k < (4*numLocksG) ; k++ ) {
			bit = 0x01LL;
			for ( j = 0 ; j < numOfCpuG ; j++ ) {
			    for ( i = 0 ; i < 64/numOfCpuG ; i++ ) {
			      if ( useSharedLabelAddr == TRUE ) {
				sprintf(tmpStr,"sharedSN0Addr+%d",k*64);
				sharedLocks->initL(FUNC_AND_OP,tmpStr , ~bit, j );
			      } 
			      else {
				sharedLocks->initL(FUNC_AND_OP,sharedSN0AddrG+(k*64) , ~bit, j );
			      }
				bit = bit << 1;
			    }
			}
			if( useSharedLabelAddr == TRUE ) { 
				sharedLocks->addToInitWorkQueue( new Work ( FUNC_INIT_ADDRESS_OP,tmpStr , (ULL) 0xffffffffffffffff) );
				sharedLocks->addToCheckWorkQueue(  new Work( CHECK_BIT | FUNC_AND_OP ,tmpStr , (ULL) 0xffffffffffffffff  ) );
			}
			else {
				sharedLocks->addToInitWorkQueue( new Work ( FUNC_INIT_ADDRESS_OP,sharedSN0AddrG+(k*64), (ULL) 0xffffffffffffffff) );
				sharedLocks->addToCheckWorkQueue(  new Work( CHECK_BIT | FUNC_AND_OP ,sharedSN0AddrG+(k*64), (ULL) 0xffffffffffffffff ) );
 			}
		    }

		    // INC_OP (SD)

		    for ( k = (4*numLocksG) ; k < (5*numLocksG) ; k++ ) {
			char tmpStr[256];
			if( useSharedLabelAddr == TRUE ) { 
				sprintf(tmpStr,"sharedSN0Addr+%d",k*64);
				sharedLocks->init(FUNC_INC_OP,tmpStr,lockIncG,numOfCpuG );
			}
			else {
				sharedLocks->init(FUNC_INC_OP,sharedSN0AddrG+(k*64),lockIncG,numOfCpuG );
			}

		    }

		    // DEC_OP (SD)
		    for ( k = (5*numLocksG) ; k < (6*numLocksG) ; k++ ) {
			char tmpStr[256];
			if( useSharedLabelAddr == TRUE ) { 
				sprintf(tmpStr,"sharedSN0Addr+%d",k*64);
				sharedLocks->init(FUNC_DEC_OP,tmpStr,lockIncG,numOfCpuG );
			}
			else {
				sharedLocks->init(FUNC_INC_OP,sharedSN0AddrG+(k*64),lockIncG,numOfCpuG );
			}
		    }


		}
	} 



	switch ( numOfCpuG ) {

	case 8:
		MPaddrMask64 = 0xffffffffffffffc0LL;
		MPaddrMask   = 0xffffffc0;
		break;

	case 16:
		MPaddrMask64 = 0xffffffffffffff80LL;
		MPaddrMask   = 0xffffff80;
		break;

        case 32:
		MPaddrMask64 = 0xffffffffffffff00LL;
		MPaddrMask   = 0xffffff00;
		break;
		
	default:
		numOfCpuG = 4;
		MPaddrMask64 = 0xffffffffffffffe0LL;
		MPaddrMask   = 0xffffffe0;
		break;
	}

	loadHwLoopCount();
	printf("hwLoopBackPoint: \n");

        doPrologue();

	if ( doMemFileOutputG == TRUE ) 
	    memoryModel.dumpMemUsageFile();


	printf("sbkillerBegin:\n");


	for ( curCpuIdG = 0 ; curCpuIdG < numOfCpuG  ; curCpuIdG++) {
		MPwordG = curCpuIdG * 8;

		if ( doMpTrace == TRUE ) {
		   fclose(MPtraceFile);
		   sprintf(traceFileName,"TRACE%d",curCpuIdG);
		   MPtraceFile = fopen(traceFileName,"w");
		   if ( traceFileName == NULL ) {
		     printf("");
		     exit(EXIT_FAILURE);
		   }
		}
		if ( sameMPcodeG == TRUE ) srandom(seed);
		if (doBlackBirdG == FALSE) baseLdStAddr = baseLdStAddr & 0xfffff000;

		 printf("/* addr %08x range %08x count %d */\n", baseLdStAddr, memoryModel.getAddrRange(), countG);
                 printf("begin%1d:\n", curCpuIdG);
		 if ( (onHardwareG == TRUE) && (doBlackBirdG == FALSE) )
                        printf("                INIT_CPU(CPU%d)         \n",curCpuIdG);
		 
		 printf("\n");


			baseImmdRegG  = baseRegG      + maxBaseRegG;
			baseCompRegG  = baseImmdRegG  + maxImmdRegG;

			baseImmdFpuRegG = baseFpuRegG;
			baseCompFpuRegG = baseImmdFpuRegG  + maxImmdFpuRegG;
			
			clearRegisters();	
			topOfMemory = NULL;
			allocStoreReg();
			fillImmdReg();

			for ( i = 0 ; i < countG; i++) {
			    for ( j = 0 ; j < loopG ; j++)  {
					doInstruction();

					if ( useSyncG == TRUE ) doSync();
					if ( doSpeedG == TRUE ) speedReadSync();
					if ( doBurstG == TRUE ) doBurst();
					doFP();
					genLLSC(curCpuIdG);
					doExtIntervention();
					doExtInterrupts();
					doInvalidates();
					doNacks();
					doRdRdy();
					doWrRdy();
					randVal = random();
					randVal = randVal % SLIPSTALLS;

					if ( randVal == 0 && doAluG == TRUE )
						printf("	sll	r0,r2,r2 /* any shift would do */\n");
					else if ( randVal == 1 && doAluG == TRUE )
						printf("	addu	r0,r31,r31\n");
			    }

			    allocStoreReg();
			    fillImmdReg();
			    if (printMemG == TRUE ) memoryModel.printMemory();
			}

			/* if sbkiller mode, only loop onces */
			if ( doMP != TRUE ) break;

			else {
	

			  while ( sharedLocks->genCode(curCpuIdG));

			  if ( lockPtr < (numLocksG-1) )
				lockPtr++;
			    else
				lockPtr = 0;
			
			    printf("	j 	endCode\n");
			    printf("	nop\n");
			}	
			lockPtr         = 0;
		        barrierCounter  = 0;
		        numBarriersDone = 0;

			// clean out simulation memory

			memoryModel.deAllocMem();

			/* dealloc destination registers, between different
			 * code streams 
			 */
			for ( i = 0 ; i < maxCompRegG ; i++) {
				compRegValidG[i] = FALSE;
				dbl[i]           = TRUE;
				mipsMode[i]	 = MIPSII;
				compReg64[i]     = 0;

			}
			for ( i = 0; i < maxCompFpuRegG ; i++ ) {
			  compFpuRegValidG[i] = FALSE;
			  compFpuReg64[i]     = 0;
			}

	}

	if ( doMpTrace == TRUE ) fclose(MPtraceFile);
	 
	doEpilogue();
	if ( *templateFile != NULL ) {
	    resCharPtr = fgets(tmpStr,256,tmpFileHandle);
	    while ( (strncmp(TEMPLATE_MARKER,tmpStr,strlen(TEMPLATE_MARKER)) != 0) && resCharPtr != NULL ) {
		printf("%s",tmpStr);
		resCharPtr = fgets(tmpStr,256,tmpFileHandle);
	    }
	    fclose(tmpFileHandle);
	}
	delete sharedLocks;
exit(EXIT_SUCCESS);
}
