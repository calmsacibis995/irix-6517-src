#include <string.h>
#include <sys/IP32.h>

#include "uif.h"
#include "execlib.h"
#include "plptests.h"
#include "intr.h"

#define NS_PER_TICK    960

Test TestTable[] = {
    { "Reset",    RegisterContentsAfterReset, 
      "Verifying Register Contents After Reset"       , PARALLEL_PORT, 1 },
    { "CntxtA_1", WalkOneThroughContextRegisterA,
      "Walking a 1 through PLP Dma Context Register A", PARALLEL_PORT, 2 },
    { "CntxtA_0", WalkZeroThroughContextRegisterA,
      "Walking a 0 through PLP Dma Context Register A", PARALLEL_PORT, 3 },
    { "CntxtB_1", WalkOneThroughContextRegisterB ,
      "Walking a 1 through PLP Dma Context Register B", PARALLEL_PORT, 4 },
    { "CntxtB_0", WalkZeroThroughContextRegisterB, 
      "Walking a 0 through PLP Dma Context Register B", PARALLEL_PORT, 5 },
    { "PLP_DMA_CNTRL", TestBitsInControlRegister,
      "Testing  bits in PLP DMA Control Register",      PARALLEL_PORT, 6 },
    {      0    ,     0      , 0,        0     , 0 }
};

unsigned long long zzElapsedTime;
unsigned long long zzCurrentTime;
unsigned long long zzTicks;
unsigned long long zztim;
unsigned long long zzcond;
void Delay(unsigned long timeout)
{      

    zzCurrentTime = UST;
    zzTicks = (timeout * 1000) / NS_PER_TICK;
    do {
	busy(1);
	zzElapsedTime = UST - zzCurrentTime;
    } while (zzElapsedTime < zzTicks);
}

static Test *LookupTest(char *runThis)
{
    Test *p;
    p = (Test *) &TestTable[0];
    msg_printf(DBG, "LookupTest: %s\n", runThis);
    for (; p->name != (char *) 0; p++) {
        if (strcmp(runThis, p->name) == 0) {
	    msg_printf(DBG, "LookupTest: Found %s (0x%x)\n", p->name, p);
	    return p;
	}
    }
    msg_printf(DBG, "LookupTest: NOT FOUND!\n", runThis);
    return ((Test *) 0);	
}

int ExecuteTest(char *name)
{
    Test       *t;
    int         result;

   /* 
    * argv[0] contains name of test, lookup
    * name of test in table.
    */
    t = LookupTest(name);
    if (t) {
	    msg_printf(DBG, "ExecuteTest: Running \"%s\"\n", t->name);
	    StartTest(t->helpString,t->functionalUnit,t->testNumber);
	    result = t->callback();
	    msg_printf(DBG, "ExecuteTest: callback returne 0x%x\n", result);
	    EndTest(t->helpString);
    }
    return(result);
}

plp_regtest()
{
    int result = 0, result1 = 0;
    Test *p;

    for ( p = (Test *) &TestTable[0]; p->name != (char *)0; p++ ) {
	result1 = ExecuteTest(p->name);
	if ( result1 ) {
		result += result1;
		msg_printf(ERR, "plp_regtest: \"%s\" FAILED (0x%x)\n",
					p->name, result1);
		result1 = 0;
	}
    }
    if (result) {
    	sum_error("secondary cache data or address");
    } else {
    	okydoky();
    }
}




