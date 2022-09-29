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
#include "oobmsg.h"

extern int getSwEvent(void);
extern int doGraphCmd(unsigned char *);

static unsigned char Rbuf[1024];

int waitReq()                        /* wait on/handle requests on DispReqFD */
{
  char Cmd[80];
  char Rsp[80];
  int n, cmdAddr;
  extern int redraw;

  initdisplay();
  Rsp[0] = 'O';
  Rsp[1] = 'K';
  Rsp[2] = ' ';
  sprintf(Rsp+3, "%d", (unsigned) Rbuf);
  Rbuf[0] = (unsigned char) OBP_CHAR;
  while (1) {
    if ((n = read(DispReqFD, Cmd, 80)) == ERROR) {
      ffscMsg("Error reading display request pipe");
      exit(-1);
    }
    if (Cmd[0] == 'i')                  /* switch interrupt */
      getSwEvent();
    else if ((Cmd[0] == 'g') &&         /* graphics command */
	     (Cmd[1] == ' ')) {
      Cmd[n] = '\0';
      cmdAddr = atoi(Cmd+2);
      Rbuf[1] = doGraphCmd((unsigned char *) cmdAddr);
      Rbuf[2] = 0;                      /* anticipate response length < 256 */
      Rbuf[3] = 0;                      /* for now */
      write(D2RReqFD, Rsp, strlen(Rsp));
    }
	 else if (Cmd[0] == 'r') {
		/* reset the screen from bargraph to backimage, called from watch dog timer */
      redraw = 1;
      drawBackImage();
	 }
    else 
      ffscMsg("Display got unrecognized command %s from pipe", Cmd);
  }
}
