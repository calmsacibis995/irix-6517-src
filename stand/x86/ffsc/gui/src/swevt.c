#include <vxworks.h>
#include <stdio.h>
#include <iolib.h>
#include <string.h>
#include <stdlib.h>

#include "metawndo.h"
#include "guiconst.h"
#include "guiextrn.h"
#include "swtst.h"

#include "ffsc.h"

extern int swbits;

int getSwEvent( ) {

  mwEvent swevt = {0,0,0,0,0,0,0,0,0,{0,0}};

  switch (swbits) {
  case SW1:
    if (ffscDebug.Flags & FDBF_SWITCH) {
      ffscMsg("Got SW1");
    }
    swevt.eventChar = 0x0d;	/* Carriage Return */
    swevt.eventScan = 0;
    break;
  case SW2:
    if (ffscDebug.Flags & FDBF_SWITCH) {
      ffscMsg("Got SW2");
    }
    swevt.eventChar = 0;
    swevt.eventScan = 80;	/* Down arrow */
    break;
  case SW3:
    if (ffscDebug.Flags & FDBF_SWITCH) {
      ffscMsg("Got SW3");
    }
    swevt.eventChar = 0;
    swevt.eventScan = 77; 	/* Right arrow */
    break;
  case SW4:
    if (ffscDebug.Flags & FDBF_SWITCH) {
      ffscMsg("Got SW4");
    }
    swevt.eventChar = 0;
    swevt.eventScan = 72;  	/* Up arrow */
    break;
  case SW5:
    if (ffscDebug.Flags & FDBF_SWITCH) {
      ffscMsg("Got SW5");
    }
    swevt.eventChar = 0;
    swevt.eventScan = 75; 	/* Left arrow */
    break;
  case SW6:
    if (ffscDebug.Flags & FDBF_SWITCH) {
      ffscMsg("Got SW6");
    }
    swevt.eventChar = 0x1b;	   /* Esc (cancel) */
    swevt.eventScan = AWAKEMU; /* Enter menu system */
    break;
  default:
    return -1;
    break;
  }
  swevt.eventType = evntKEYDN;
  if (!StoreEvent(&swevt)) {
    ffscMsg("Couldn't store event");
    return -1;
  }
  return 0;
}


