#include "sys/gr2hw.h"
#include "ide_msg.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

/* define MFG_USED 1 */

#define TRUE	1
#define FALSE	0 
/*Modified Jeff Weinstein's code*/
#define MPLEX_PALT 	0xc0 /* Set 5:1 multiplexing and using color paletter RAM*/
#define RDMASK_ADDR	0x04 /* Address of read mask register*/
#define PATTERN		0x00
#define RED_ENAB	0x01
#define GREEN_ENAB	0x02
#define BLUE_ENAB	0x04
#define REDONLY 0
#define GREENONLY 1 
#define BLUEONLY 2 

extern struct gr2_hw *base;

void BT457menu(void) {
	msg_printf(VRB,"BT457 DAC cmd#:	ADDR = 0, 	PALETTE = 1, 	CMD2 = 2, OVERLAY = 3\n");
}

/*ARGSUSED*/
void
gr2_init457DAC(int argc, char *argv[])
{
        unsigned char rdred, rdgreen, rdblue;
        int i;

/*
 * Control Register Initialization
 */
        msg_printf(VRB, "Initialize Control registers\n");
        base->reddac.addr = RDMASK_ADDR;
        base->grndac.addr = RDMASK_ADDR;
        base->bluedac.addr = RDMASK_ADDR;

        rdred = base->reddac.addr;
        rdgreen = base->grndac.addr;
        rdblue = base->bluedac.addr;
        if (rdred != RDMASK_ADDR){
                        msg_printf(ERR, "Error in reading addr of red dac:\
expected %#x,  returned  %#08x\n", RDMASK_ADDR, rdred);
	}
        if (rdgreen != RDMASK_ADDR){
                        msg_printf(ERR, "Error in reading the addr of green dac:\
expected %#x,  returned  %#08x\n", RDMASK_ADDR, rdgreen);
	}
        if (rdblue != RDMASK_ADDR){
                        msg_printf(ERR, "Error in reading the addr of blue dac:\
expected %#x,  returned  %#08x\n", RDMASK_ADDR, rdblue);
	}
	/* Initializing read mask register*/
        base->reddac.cmd2 = 0xff;
        base->grndac.cmd2= 0xff;
        base->bluedac.cmd2= 0xff;

	rdred = base->reddac.cmd2;
        rdgreen = base->grndac.cmd2;
        rdblue = base->bluedac.cmd2;

	if (rdred != 0xff){
                        msg_printf(ERR, "Error in reading the red read mask:\
expected 0xff,  returned  %#08x\n", rdred);
        }

        if (rdgreen != 0xff){
                        msg_printf(ERR, "Error in reading the green read mask:\
expected 0xff,  returned  %#08x\n", rdgreen);
        }

        if (rdblue != 0xff){
                        msg_printf(ERR, "Error in reading the blue read mask:\
expected 0xff,  returned  %#08x\n", rdblue);
        }
	
	/* Initializing blink mask register to disable  blinking*/
	base->reddac.addr = 0x05;
        base->grndac.addr = 0x05;
        base->bluedac.addr = 0x05;	

        base->reddac.cmd2 = 0x00;
        base->grndac.cmd2= 0x00;
        base->bluedac.cmd2= 0x00;

	rdred = base->reddac.cmd2;
        rdgreen = base->grndac.cmd2;
        rdblue = base->bluedac.cmd2;

        if (rdred != 0x00){
                        msg_printf(ERR, "Error in reading the red blink mask:\
expected 0x00,  returned  %#08x\n", rdred);
        }

        if (rdgreen != 0x00){
                        msg_printf(ERR, "Error in reading the green blink mask:\
expected 0x00,  returned  %#08x\n", rdgreen);
        }

        if (rdblue != 0x00){
                        msg_printf(ERR, "Error in reading the blue blink mask:\
expected 0x00,  returned  %#08x\n", rdblue);
        }

	

	/* Initializing command register*/
	base->reddac.addr = 0x06;
        base->grndac.addr = 0x06;
        base->bluedac.addr = 0x06;	

        base->reddac.cmd2 = MPLEX_PALT;
        base->grndac.cmd2= MPLEX_PALT;
        base->bluedac.cmd2= MPLEX_PALT;

	rdred = base->reddac.cmd2;
        rdgreen = base->grndac.cmd2;
        rdblue = base->bluedac.cmd2;

	if (rdred != MPLEX_PALT){
        	msg_printf(ERR, "Error in reading the red command reg:\
expected %#x,  returned  %#08x\n", MPLEX_PALT,rdred);
        }
        if (rdgreen != MPLEX_PALT){
        	msg_printf(ERR, "Error in reading the green command reg:\
expected %#x,  returned  %#08x\n", MPLEX_PALT, rdgreen);
        }
        if (rdblue != MPLEX_PALT){
               msg_printf(ERR, "Error in reading the blue command reg:\
expected 0x40,  returned  %#08x\n", MPLEX_PALT, rdblue);
        }

	/* Initializing test register*/

	base->reddac.addr = 0x07;
        base->grndac.addr = 0x07;
        base->bluedac.addr = 0x07;	

        base->reddac.cmd2 = 0;
        base->grndac.cmd2 = 0;
        base->bluedac.cmd2= 0;

	rdred = base->reddac.cmd2;
        rdgreen = base->grndac.cmd2;
        rdblue = base->bluedac.cmd2;

	rdred &= 0x0f;
	rdgreen &= 0x0f;
	rdblue &= 0x0f;

	if (rdred != 0){
                        msg_printf(ERR, "Error in reading the red test register:\
expected 0,  returned  %#x\n",  rdred);
        }
        if (rdgreen != 0){
                        msg_printf(ERR, "Error in reading the green test register:\
expected 0,  returned  %#x\n",rdgreen);
        }
        if (rdblue !=  0){
                        msg_printf(ERR, "Error in reading the blue test register:\
expected 0,  returned  %#x\n",  rdblue);
        }

	/* Initializing Color Palette RAM */
	base->reddac.addr = 0x00;
        base->grndac.addr = 0x00;
        base->bluedac.addr = 0x00;	

	for (i=0; i< 256; i++)
        {
                base->reddac.paltram = i;
                base->grndac.paltram = i;
                base->bluedac.paltram= i;
        }

	base->reddac.addr = 0x00;
        base->grndac.addr = 0x00;
        base->bluedac.addr = 0x00;	


	for (i=0; i< 256; i++)
        {
                rdred = base->reddac.paltram;
                rdgreen = base->grndac.paltram;
                rdblue = base->bluedac.paltram;

        if (rdred != i){
                        msg_printf(ERR, "Error in reading the red Palette RAM at offset %d:\
expected %#x,  returned  %#x\n", i, i, rdred);
        }
        if (rdgreen != i){
                        msg_printf(ERR, "Error in reading the green Palette RAM at offset %d:\
expected %#x,  returned  %#x\n",i, i, rdgreen);
        }
        if (rdblue !=  i){
                        msg_printf(ERR, "Error in reading the blue Palette RAM at offset %d:\
expected %#x,  returned  %#x\n", i, i, rdblue);
        }
        } 
	/* Initializing Overlay Color Palette */
        base->reddac.addr = 0x00;
        base->grndac.addr = 0x00;
        base->bluedac.addr = 0x00;

        for (i=0; i< 4; i++)
        {
                base->reddac.ovrlay  = i;
                base->grndac.ovrlay  = i;
                base->bluedac.ovrlay = i;
        }
	/*verify*/
        base->reddac.addr = 0x00;
        base->grndac.addr = 0x00;
        base->bluedac.addr = 0x00;
	
        for (i=0; i< 4; i++)
        {
                rdred = base->reddac.ovrlay  ;
                rdgreen = base->grndac.ovrlay  ;
                rdblue = base->bluedac.ovrlay ;

        if (rdred != i){
                        msg_printf(ERR, "Error in reading the red overlay  at offset %d:\
expected %#x,  returned  %#x\n", i, i, rdred);
        }
        if (rdgreen != i){
                        msg_printf(ERR, "Error in reading the green overlay at offset %d:\
expected %#x,  returned  %#x\n",i, i, rdgreen);
        }
        if (rdblue !=  i){
                        msg_printf(ERR, "Error in reading the blue overlay at offset %d:\
expected %#x,  returned  %#x\n", i, i, rdblue);
        }
	}
}

gr2_check457DAC(int argc, char *argv[])
{

	unsigned int cmd, loop, wrval, addr;
	unsigned char rdred, rdgreen, rdblue;
	unsigned char rdred2, rdgreen2, rdblue2; /* For second reading of colors */ 
	int i;
	int error = 0;

/*Address read after a write to color palette */
	unsigned char redaddr, greenaddr, blueaddr; 

	
	if (argc < 4) {
		msg_printf(ERR,"usage: %s cmd# addr value <count> \n", argv[0]);
		BT457menu();
		return -1;
	}

	argv++;
	atohex(*(argv), (int *)&cmd);

	argv++;
	atohex(*(argv), (int *)&addr);
	addr &= 0xff;

	argv++;
	atohex(*(argv), (int *)&wrval);
	wrval &= 0xff;

	if (argc == 5) {
		argv++;
		loop = atoi(*(argv));
	}
	else loop = 1;


	for (i = 0; i < loop; i++) {
	switch (cmd) {
		case GR2_BT_ADDR:
		base->reddac.addr = addr; 
		base->grndac.addr = addr; 
		base->bluedac.addr = addr; 

		rdred = base->reddac.addr;
		rdgreen = base->grndac.addr;
		rdblue = base->bluedac.addr;

                if (rdred != addr)
                {
                	msg_printf(ERR, "Error in reading red addr for loop# %d: \
expected %#x,  returned %#x\n", i, addr, rdred);
			rdred = base->reddac.addr;
                	msg_printf(DBG, "2nd reading of red addr:  %#x\n", rdred );

			error ++;
                }
                if (rdgreen != addr)
                {
                	msg_printf(ERR, "Error in reading green addr for loop# %d: \
expected %#x,  returned %#x\n", i, addr, rdgreen);
			rdgreen = base->grndac.addr;
                	msg_printf(DBG, "2nd reading of greenaddr:  %#x\n", rdgreen);
			error ++;
                }
                if (rdblue!= addr)
                {
                	msg_printf(ERR, "Error in reading blue addr for loop# %d: \
expected %#x,  returned %#x\n", i, addr, rdblue );
			rdblue = base->bluedac.addr;
                	msg_printf(DBG, "2nd reading of blue addr :  %#x\n", rdblue);
			error ++;
                }
		break;

		case GR2_BT_PALETTE_RAM:

		base->reddac.addr = addr; 
		base->grndac.addr = addr; 
		base->bluedac.addr = addr; 

		base->reddac.paltram = wrval; 
		base->grndac.paltram = wrval; 
		base->bluedac.paltram = wrval; 

		/* Read address to check for autoincrementation, before resetting address */
		redaddr = base->reddac.addr;
		greenaddr = base->grndac.addr;
		blueaddr = base->bluedac.addr;

		base->reddac.addr = addr; 
		base->grndac.addr = addr; 
		base->bluedac.addr = addr; 

		rdred = base->reddac.paltram;
		rdgreen = base->grndac.paltram;
		rdblue = base->bluedac.paltram;

                if (rdred != wrval) {
                        msg_printf(ERR, "Error in reading red palette ram for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdred);
                        msg_printf(DBG, "Current red address is %#x\n",  redaddr);

			base->reddac.addr = addr; 
			base->grndac.addr = addr; 
                        base->bluedac.addr = addr;

                        rdred2 = base->reddac.paltram;
                        rdgreen2 = base->grndac.paltram;
                        rdblue2 = base->bluedac.paltram;
                        msg_printf(DBG, "2nd reading for red loop# %d at addr %#x: returned %#x\n", i, addr, rdred2);
			error ++;
                }
                if (rdgreen != wrval) {
                        msg_printf(ERR, "Error in reading green palette ram for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdgreen);
                        msg_printf(DBG, "Current green address is %#x\n",  greenaddr);

			base->reddac.addr = addr; 
			base->grndac.addr = addr; 
                        base->bluedac.addr = addr;

                        rdred2 = base->reddac.paltram;
                        rdgreen2 = base->grndac.paltram;
                        rdblue2 = base->bluedac.paltram;
                        msg_printf(DBG, "2nd reading for green loop# %d at addr %#x: returned %#x\n", i, addr, rdgreen2);
			error ++;

 		}
		if (rdblue != wrval) {
                        msg_printf(ERR, "Error in reading blue palette ram for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdblue);
                        msg_printf(DBG, "Current blue address is %#x\n",  blueaddr);

			base->reddac.addr = addr; 
			base->grndac.addr = addr; 
                        base->bluedac.addr = addr;

                        rdred2 = base->reddac.paltram;
                        rdgreen2 = base->grndac.paltram;
                        rdblue2 = base->bluedac.paltram;
                        msg_printf(DBG, "2nd reading for blue  loop# %d at addr %#x: returned %#x\n", i, addr, rdblue2);
			error ++;
 }
		break;

		case GR2_BT_OVERLAY:
		base->reddac.addr = addr; 
		base->grndac.addr = addr; 
		base->bluedac.addr = addr; 

		/* Need 3 writes to overlay palette */
		base->reddac.ovrlay = wrval;   
		base->grndac.ovrlay = wrval;   
		base->bluedac.ovrlay = wrval;   

		/* Read address to check for autoincrementation, 
		 * before resetting address 
		 */
                redaddr = base->reddac.addr;
                greenaddr = base->grndac.addr;
                blueaddr = base->bluedac.addr;

		base->reddac.addr = addr; 
		base->grndac.addr = addr; 
		base->bluedac.addr = addr; 

		rdred = base->reddac.ovrlay;
		rdgreen = base->grndac.ovrlay;
		rdblue = base->bluedac.ovrlay;


                if (rdred != wrval) {
			/* rdred = base->reddac.addr; */
			redaddr = base->reddac.addr; 
                        msg_printf(ERR, "Error in reading red overlay register for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdred);
                        msg_printf(DBG, "Current address is %#x\n",  redaddr);

			base->reddac.addr = addr; 
			base->grndac.addr = addr;
                	base->bluedac.addr = addr;

			rdred2 = base->reddac.ovrlay ;
			rdgreen2 = base->grndac.ovrlay ;
			rdblue2 = base->bluedac.ovrlay ;

			msg_printf(DBG, "2nd reading for loop# %d at addr %#x: \
returned %#x\n", i,  addr,rdred2);
			error ++;
                }
	
		if (rdgreen != wrval) {
                        /* rdgreen = base->grndac.addr; */
                        greenaddr = base->grndac.addr;
                        msg_printf(ERR, "Error in reading green overlay register for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdgreen);
                        msg_printf(DBG, "Current address is %#x\n",  greenaddr);

                        base->reddac.addr = addr;
                        base->grndac.addr = addr;
                        base->bluedac.addr = addr;

                        rdred2 = base->reddac.ovrlay ;
                        rdgreen2 = base->grndac.ovrlay ;
                        rdblue2 = base->bluedac.ovrlay ;

                        msg_printf(DBG, "2nd reading for loop# %d at addr %#x: \
returned %#x\n", i,  addr, rdgreen2);
			error ++;
                }
		if (rdblue != wrval) {
                        /* rdblue = base->bluedac.addr; */
                        blueaddr  = base->bluedac.addr;
                        msg_printf(ERR, "Error in reading blue overlay register for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdblue );
                        msg_printf(DBG, "Current address is %#x\n",  blueaddr );

                        base->reddac.addr = addr;
                        base->grndac.addr = addr;
                        base->bluedac.addr = addr;

                        rdred2 = base->reddac.ovrlay ;
                        rdgreen2 = base->grndac.ovrlay ;
                        rdblue2 = base->bluedac.ovrlay ;

                        msg_printf(DBG, "2nd reading for loop# %d at addr %#x: \
returned %#x\n", i,  addr, rdblue2);
			error ++;
                }
		break;

		case GR2_BT_CMD2:
		base->reddac.addr = addr; 
		base->grndac.addr = addr; 
		base->bluedac.addr = addr; 

		base->reddac.cmd2 = wrval;
		base->grndac.cmd2 = wrval;
		base->bluedac.cmd2 = wrval;

		rdred = base->reddac.cmd2;
                if (rdred != wrval) {
                        msg_printf(ERR, "Error in reading red command register for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdred);
			error ++;
                }

		rdgreen = base->grndac.cmd2;
                if (rdgreen != wrval) {
                        msg_printf(ERR, "Error in reading green command register for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdgreen);
			error ++;
                }

		rdblue= base->bluedac.cmd2;
                if (rdblue!= wrval) {
                        msg_printf(ERR, "Error in reading blue command register for loop# %d: \
expected %#x,   returned %#x\n",  i, wrval, rdblue);
			error ++;
                }
		break;
	    }  /* end of switch */	
	}
	return error ;
}

/*ARGSUSED*/
int
gr2_wr457DAC(int argc, char *argv[])
{
#ifdef MFG_USED

        int unsigned  cmd, val, loop, i;
	int all, color;
        

        if (argc < 3)
        {
		BT457menu();
                msg_printf(ERR, "usage: %s cmd# value <count> <0=red only, 1=green only, 2=blue only, default is ALL>\n", argv[0]);
                return -1;
        }

        argv++;
        atohex(*(argv), &cmd);
        argv++;
        atohex(*(argv), &val);
	val &= 0xff;

	if (argc >= 4) {
	argv++;
	loop = atoi(*argv); 
	}
	else loop = 1;

	if (argc == 5) {
		all = FALSE;
		argv++;
		color = atoi(*argv); 
	}
	else {
		color = 3;
		all = TRUE;
	}
   for (i=0; i < loop; i++) {
        switch (cmd) {
                case GR2_BT_ADDR:
		if ( (color == REDONLY) | (all))
                	base->reddac.addr = val;
		if ( (color == GREENONLY) | (all))
                	base->grndac.addr = val;
		if ( (color == BLUEONLY) | (all))
                	base->bluedac.addr = val;
                break;

                case GR2_BT_PALETTE_RAM:
                base->reddac.paltram = val;
                base->grndac.paltram = val;
                base->bluedac.paltram = val;
                break;

                case GR2_BT_CMD2:
		if ( (color == REDONLY) | (all))
                	base->reddac.cmd2 = val;
		if ( (color == GREENONLY) | (all))
                	base->grndac.cmd2 = val;
		if ( (color == BLUEONLY) | (all))
                	base->bluedac.cmd2 = val;
                break;

                case GR2_BT_OVERLAY:
                base->reddac.ovrlay = val;
                base->grndac.ovrlay = val;
                base->bluedac.ovrlay = val;
                break;
        }
   }
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif
}

/*ARGSUSED*/
int
gr2_wr457RGB(int argc, char *argv[])
{
#ifdef MFG_USED
        int unsigned  red, green,blue, loop, i, start;
        

        if (argc < 5)
        {
                msg_printf(ERR, "Can also fill a color map range with one color.\n");
		msg_printf(ERR, " usage: %s start_addr(in hex)  r g b  <count> Note: writes will wraparound after addr 0xff\n", argv[0]);
                return -1;
        }

        argv++;
        atohex(*argv, &start);
        argv++;
        atohex(*(argv), &red);
        argv++;
        atohex(*(argv), &green);
        argv++;
        atohex(*(argv), &blue);

        if (argc == 6) {
        argv++;
        loop = atoi(*argv);
        }
	else loop = 1;

	base->reddac.addr =  start & 0xff; /* init colormap */
	base->grndac.addr =  start & 0xff; /* init colormap */
	base->bluedac.addr =  start & 0xff; /* init colormap */

   for (i=0; i < loop; i++) {
	base->reddac.paltram= red & 0xff;
	base->grndac.paltram= green & 0xff;
	base->bluedac.paltram= blue & 0xff;
	}
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif
}

/*ARGSUSED*/
int
gr2_rd457DAC(int argc, char *argv[])
{
#ifdef MFG_USED
        int unsigned  cmd, loop,i,all,color,msg;
	unsigned char rdred, rdgreen, rdblue;
	
        

        if (argc < 2)
        {
		BT457menu();
                msg_printf(ERR, "usage: %s cmd# <count> <0=red, 1=green, 2=blue>\n", argv[0]);
                return -1;
        }

        argv++;
        atohex(*(argv), &cmd);

        if (argc >= 3)
        {
                argv++;
                loop = atoi(*(argv));
		if (loop > 1)
			msg = FALSE;
		else
			msg = TRUE;
        }
	else { /*default*/
        	loop = 1;
		msg = TRUE;
	}
	if (argc >= 4) {
                all = FALSE;
                argv++;
                color = atoi(*argv);
        }
        else {
                color = 3;
                all = TRUE;
        }
	

        for (i=0; i< loop; i++) {
        switch (cmd) {
                case GR2_BT_ADDR:
                	if ( (color == REDONLY) | (all))
			{	
                        rdred = base->reddac.addr;
			if (msg)
                        	msg_printf(DBG, "reading red addr: returned %#x\n", rdred);
			}
                	if ( (color == GREENONLY) | (all))
			{	
                        rdgreen = base->grndac.addr;
			if (msg)
                        	msg_printf(DBG, "reading green addr: returned %#x\n", rdgreen);
			}
                	if ( (color == BLUEONLY) | (all))
			{	
                        rdblue = base->bluedac.addr;
			if (msg)
                        	msg_printf(DBG, "reading blue addr: returned %#x\n", rdblue);
			}
                break;

                case GR2_BT_PALETTE_RAM:
                rdred = base->reddac.paltram;
                rdgreen = base->grndac.paltram;
                rdblue = base->bluedac.paltram;
		if (msg) {
                        msg_printf(DBG, "reading red palette ram: returned %#x\n",  rdred);
                        msg_printf(DBG, "reading green palette ram: returned %#x\n",  rdgreen);
                        msg_printf(DBG, "reading blue palette ram: returned %#x\n",  rdblue);
		}
                break;

                case GR2_BT_CMD2:
		if ( (color == REDONLY) | (all))
                {
                rdred = base->reddac.cmd2 ;
		if (msg)
                	msg_printf(DBG, "reading red control: returned %#x\n",  rdred);
		}

		if ( (color == GREENONLY) | (all))
                {
                rdgreen = base->grndac.cmd2 ;
		if (msg)
                	msg_printf(DBG, "reading green control: returned %#x\n",  rdgreen);
		}

		if ( (color == BLUEONLY) | (all))
                {
                rdblue = base->bluedac.cmd2 ;
		if (msg)
                	msg_printf(DBG, "reading blue control: returned %#x\n",  rdblue);
		}
                break;

                case GR2_BT_OVERLAY:
                rdred = base->reddac.ovrlay ;
                rdgreen = base->grndac.ovrlay ;
                rdblue = base->bluedac.ovrlay ;
		if (msg) {
                msg_printf(DBG, "reading red overlay : returned %#x\n",  rdred);
                msg_printf(DBG, "reading green overlay : returned %#x\n",  rdgreen);
		msg_printf(DBG, "reading blue overlay : returned %#x\n",  rdblue);
		}
                break;
          }
     }
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif /* MFG_USED */
}

static	char * cmd_names [] = { "Address","Pallette RAM","Command 2","Overlay" };
int
gr2_check_bt457(void)
{

#define	MAX_ADDR	0xff
#define	MAX_ADDR_OVRLAY	0x3

	int cmd , addr, value, count  = 1;
	char  *argv[5] ;
	char  buf[20],buf1[20],buf2[20],buf3[20];
	char addr_err[MAX_ADDR]  ;
	int i;
	int err = 0;

	sprintf(buf3,"%d",count);
	argv[4] = buf3; 

	msg_printf(SUM,"Checking bt457 DAC ...\n");
	for ( cmd = 0 ; cmd < 4 ; cmd ++) {
	 gr2_init457DAC(2, argv); /* parameters are dummy - not used*/
	 sprintf(buf,"%d",cmd);
	 argv[1] = buf; 
	 for( addr = 0 ; addr <= ((cmd == 3) ? MAX_ADDR_OVRLAY : MAX_ADDR) ; addr ++) {
	   addr_err[addr] = 0 ;

	   if ( cmd == 2 ) {
	     if ( addr == 7 || addr == 8 || addr ==9 || addr == 0x6b || addr == 0x6c ||
		  addr == 0x6d || addr == 0xcf || addr == 0xd0 || addr == 0xd1 )

		  continue ;
	   }


	   sprintf(buf1,"%d",addr);
	   argv[2] = buf1; 
	   for ( value =0 ; value < 0x100 ; value ++ ){
	      sprintf(buf2,"%d",value);
	      argv[3] = buf2; 
	      msg_printf(DBG,"checking bt457 DAC with cmd %d for addr %#x and value %#x\n",cmd,addr,value);
	      if ( gr2_check457DAC(5,argv) > 0 ) {
		 addr_err[addr] = 1;
		 
	         msg_printf(DBG,"Error - cmd %d for addr %#x and value %#x\n",cmd,addr,value);
		 
	      }
	   }
	 }
	 msg_printf(DBG,"Cmd -> %d(%s test), Failed addresses(if any) follow:\n",cmd,cmd_names[cmd]);
	 for(  i = 0, addr = 0 ; addr <= ((cmd == 3 ) ? MAX_ADDR_OVRLAY : MAX_ADDR) ; addr ++) {
	   if( addr_err[addr] ) {
	         i++;
		 err ++;
	         if (!(i%16))
			msg_printf(DBG,"%#x\n",addr);
		 else
			msg_printf(DBG,"%#x ",addr);

	  }
	 }
	 msg_printf(DBG,"\n");
	}

	return err ;
}
