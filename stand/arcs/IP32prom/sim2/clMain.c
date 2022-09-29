#include "sim.h"

extern void fwStart(int,int);
extern void start(void);

/*
 * Simulation "main". 
 */ 
main(int argc, char** argv){
  int simMode = clSimInit(argc,argv);
  switch (simMode){
  case simExit:
    return 0;
  case simSloader:
    start();
    break;
  case simFW:
    fwStart(0,0);
    break;
  }
}



/*************
 * _getsystype
 *------------
 *
 * HACK: The shared memory code calls _getsystype which calls the configuration
 *       routines to figure out what type of processor we are dealing with.
 *       Well, the firmware implementation of the config routines overlays
 *       the IRIX implementation but doesn't work right so we "hard wire"
 *       _getsystype here so the shared memory code will work properly.
 */
#define US_R4K_UP 2

int
_getsystype(void){
  return US_R4K_UP;
}
