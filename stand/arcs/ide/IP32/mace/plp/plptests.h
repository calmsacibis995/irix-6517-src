#if !defined(__PLP_TESTS__)
#define __PLP_TESTS__

#define PARALLEL_PORT     1

extern int WalkOneThroughContextRegisterA();
extern int WalkZeroThroughContextRegisterA();
extern int WalkOneThroughContextRegisterB();
extern int WalkZeroThroughContextRegisterB();
extern int TestBitsInControlRegister();
extern int RegisterContentsAfterReset();


#ifdef notdef
Test TestTable[] = {
    { "Reset",    RegisterContentsAfterReset, 
      "Verifying Register Contents After Reset"       , PARALLEL_PORT, 1 },
    { "CntxtA_1", WalkOneThroughContextRegisterA,
      "Walking a 1 through PLP Dma Context Register A", PARALLEL_PORT, 2 },
    { "CntxtB_0", WalkZeroThroughContextRegisterA,
      "Walking a 0 through PLP Dma Context Register A", PARALLEL_PORT, 3 },
    { "CntxtA_1", WalkOneThroughContextRegisterB ,
      "Walking a 1 through PLP Dma Context Register B", PARALLEL_PORT, 4 },
    { "CntxtB_0", WalkZeroThroughContextRegisterB, 
      "Walking a 0 through PLP Dma Context Register B", PARALLEL_PORT, 5 },
    { "PLP_DMA_CNTRL", TestBitsInControlRegister,
      "Testing  bits in PLP DMA Control Register",      PARALLEL_PORT, 6 },
    {      0    ,     0      , 0,        0     , 0 }
};
#endif /* notdef */

#if !defined(FALSE)
#define FALSE 0
#endif /* FALSE */

#if !defined(TRUE)
#define TRUE 1
#endif /* TRUE */

#endif






























