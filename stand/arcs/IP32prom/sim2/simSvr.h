#ifndef _simSvr_h_
#define _simSvr_h_



#define SIM_SERVICE_SIZE 50
extern void (*simService[SIM_SERVICE_SIZE])(void); 
extern int simServiceFree;


void simRegisterAddrMatch(__uint64_t    mask,
			  __uint64_t    target,
			  int           matchCode,
			  int           targetCode,
			  SimAddrMap_t* map);
void simRegisterCop0Match(int mask, int target, int matchCode, int targetCode);
int simRegisterService(void (*)(void) );


/*
 * Kind of ugly but for now, all service initialization routines
 * are declared here.
 */
#define SIM_LIST {\
		    simMaceSerialInit,\
		    simK0K1Init,\
		    simFlashInit,\
		    simRtcInit,\
		    simCop0Init,\
		    simMaceEtherInit,\
		    simCrimeInit,\
		    simUSTInit,\
		    simPCIInit,\
		    0 }

extern void simK0K1Init(void);
extern void simCop0Init(void);
extern void simRtcInit(void);
extern void simMaceSerialInit(void);
extern void simFlashInit(void);
extern void simMaceEtherInit(void);
extern void simCrimeInit(void);
extern void simUSTInit(void);
extern void simPCIInit(void);
#endif
