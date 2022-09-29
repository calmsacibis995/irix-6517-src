#include <math.h>
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "uif.h"
#include "ide_msg.h"
#include "sys/param.h"
#include "parser.h"
#include "mgv1_ev1_I2C.h"

#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

/*extern struct gr2_hw  *base ;*/
extern  mgras_hw *mgbase;
/*extern struct mgras_hw *base;*/

unsigned sysctlval = 0x68;


/******************************************************************************
*
*  v2ga - This function is taken from a script written by Bob Williams: s_v2ga
*       It makes the appropriate AB and CC calls to set up a video 
*       to graphics channel A .  ev1_init may need to be run before this
*       command. The shell script function vid2gfxa calls gfxset, then
*       ev1_init then v2ga.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro
*  jfg 5/95
******************************************************************************/



void
v2ga()
{
/* 320 -> 960, 0 -> 512 */
AB_W1(mgbase,sysctl,0x1);   /*NTSC, express video disable keyin:  abdir5 1 */

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,0);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,1);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,2);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,3);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,4);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display enable, flicker reduction on*/
AB_W1(mgbase,low_addr,5);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,3);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,6);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /*1x decimation :abind7 0    */
AB_W1(mgbase,low_addr,7);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,10);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,11);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,12);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,13);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,14);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display disable, flicker reduction on*/
AB_W1(mgbase,low_addr,15);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,2);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,16);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /*1x decimation :abind7 0    */
AB_W1(mgbase,low_addr,17);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,20);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,21);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,22);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,23);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,24);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display disable, flicker reduction on*/
AB_W1(mgbase,low_addr,25);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,26);
AB_W1(mgbase,intrnl_reg,0);  

CC_W1 (mgbase, indrct_addreg, 0); /*A:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	

CC_W1 (mgbase, indrct_addreg, 1);    /*first active line 12? :ccind1 1 */
CC_W1 (mgbase, indrct_datareg, 1);	

CC_W1 (mgbase, indrct_addreg, 2); /*B:video to gfx, read- not done yet*/
                                 /*  ccind2 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	 /*changed !*/

CC_W1 (mgbase, indrct_addreg, 3);    /*first active line 12? for B and C */
CC_W1 (mgbase, indrct_datareg, 1);	

CC_W1 (mgbase, indrct_addreg, 4); /*C:video to gfx, read- not done yet*/
                                 /*  ccind2 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	 /*changed !*/

CC_W1 (mgbase, indrct_addreg, 8);   /*window A location left edge / 5: ccind8 0*/
CC_W1 (mgbase, indrct_datareg, 64);   /*320*/  

CC_W1 (mgbase, indrct_addreg, 9);/*window A location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 192);  /*960*/	/*: ccind9 127 0*/

CC_W1 (mgbase, indrct_addreg, 10); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x0e);	

CC_W1 (mgbase, indrct_addreg, 11);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	
                                      /* 512 (top) x 1023 (bot)*/
CC_W1 (mgbase, indrct_addreg, 12);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 255);	

CC_W1 (mgbase, indrct_addreg, 13); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 14);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

/* stuff for window B: ccind18 0x10*/

CC_W1 (mgbase, indrct_addreg, 16); /*window B location left edge / 5:*/
CC_W1 (mgbase, indrct_datareg, 0); /* left 0*/

CC_W1 (mgbase, indrct_addreg, 17);/*window B location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 128);	/*: right 640*/

CC_W1 (mgbase, indrct_addreg, 18); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x14);	

CC_W1 (mgbase, indrct_addreg, 19);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 20);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 255);  	

CC_W1 (mgbase, indrct_addreg, 21); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 22);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

CC_W1 (mgbase, indrct_addreg, 24);  /*window C location left edge / 5: ccind8 0*/
CC_W1 (mgbase, indrct_datareg,128);  /*640*/	

CC_W1 (mgbase, indrct_addreg, 25);/*window A location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 256);	/*: 1280 0*/

CC_W1 (mgbase, indrct_addreg, 26); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x14);	

CC_W1 (mgbase, indrct_addreg, 27);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 28);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 0xe0);	

CC_W1 (mgbase, indrct_addreg, 29); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 30);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

/*CC_W1 (mgbase, indrct_addreg, 18);*//*stuff for window B: ccind18 0x10*/
/*CC_W1 (mgbase, indrct_datareg, 0x10);	*/

/*CC_W1 (mgbase, indrct_addreg, 26);*//*C top bottom blackout ccind26 0x10 */
/*CC_W1 (mgbase, indrct_datareg, 0x10);	*/

CC_W1 (mgbase, indrct_addreg, 32); /*complete current window :ccind32 2*/
CC_W1 (mgbase, indrct_datareg, 0x0A);	

CC_W1 (mgbase, indrct_addreg, 38); /*enable video input port*/
CC_W1 (mgbase, indrct_datareg, 0x20);	

CC_W1 (mgbase, indrct_addreg, 54); /*video input for gfx A,*/
                                 /*video for gfx B  ccind54 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 55); /*video input for gfx C,*/
                                 /*video for expansion 0  ccind55 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 56); /*frame buffer input is video input*/
                            /*video output is frame buffer output ccind56 30*/
CC_W1 (mgbase, indrct_datareg, 0x30);

CC_W1 (mgbase, indrct_addreg, 57); /*forground input and bkgrnd is video: */
                                 /*  ccind57 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 58); /*forground and bkgrnd alpha is video*/ 
                                 /*  ccind58 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 0); /*A:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	

CC_W1 (mgbase, indrct_addreg, 2); /*B:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x80);	 /*changed !*/

CC_W1 (mgbase, sysctl, sysctlval);
/*CC_W1 (mgbase, sysctl, 0x48);*/
                       /*video double buffering, start, video output(7191)*/


	       }

/******************************************************************************
*
*  v2gb - This function is taken from a script written by Bob Williams: s_v2ga
*       It makes the appropriate AB and CC calls to set up a video 
*       to graphics channel B .  ev1_init may need to be run before this
*       command. The shell script function vid2gfxa calls gfxset, then
*       ev1_init then this function.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro
*   Note: Currently this function doesn't work (only paths through A do)
*  jfg 5/95
******************************************************************************/

void
v2gb()
{
/* 320 -> 960, 0 -> 512 */
AB_W1(mgbase,sysctl,0x1);   /*NTSC, express video disable keyin:  abdir5 1 */
AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,0);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,1);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,2);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,3);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,4);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display enable, flicker reduction on*/
AB_W1(mgbase,low_addr,5);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,2);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,6);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /*1x decimation :abind7 0    */
AB_W1(mgbase,low_addr,7);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,20);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,21);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,22);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,23);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,24);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display disable, flicker reduction on*/
AB_W1(mgbase,low_addr,25);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,0);  

/*AB_W1(mgbase,intrnl_reg,1);  */

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,26);
AB_W1(mgbase,intrnl_reg,0);  
AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */

AB_W1(mgbase,low_addr,10);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,11);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,12);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,13);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,14);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display disable, flicker reduction on*/
AB_W1(mgbase,low_addr,15);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,3);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,16);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /*1x decimation :abind7 0    */
AB_W1(mgbase,low_addr,17);
AB_W1(mgbase,intrnl_reg,0);  


CC_W1 (mgbase, indrct_addreg, 0); /*A:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	

CC_W1 (mgbase, indrct_addreg, 1);    /*first active line 12? :ccind1 1 */
CC_W1 (mgbase, indrct_datareg, 1);	

CC_W1 (mgbase, indrct_addreg, 2); /*B:video to gfx, read- not done yet*/
                                 /*  ccind2 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	 /*changed !*/

CC_W1 (mgbase, indrct_addreg, 3);    /*first active line 12? for B and C */
CC_W1 (mgbase, indrct_datareg, 1);	

CC_W1 (mgbase, indrct_addreg, 4); /*C:video to gfx, read- not done yet*/
                                 /*  ccind2 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	 /*changed !*/

CC_W1 (mgbase, indrct_addreg, 8);   /*window A location left edge / 5: ccind8 0*/
CC_W1 (mgbase, indrct_datareg, 0);	
                                       /* 0 - 640*/
CC_W1 (mgbase, indrct_addreg, 9);/*window A location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 128);	/*: ccind9 127 0*/
                                     
CC_W1 (mgbase, indrct_addreg, 10); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x14);	/* 0 (top) - 511 */

CC_W1 (mgbase, indrct_addreg, 11);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	
                                      /* 512 (top) x 1023 (bot)*/
CC_W1 (mgbase, indrct_addreg, 12);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 255);	

CC_W1 (mgbase, indrct_addreg, 13); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 14);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130 );	

/* stuff for window B: ccind18 0x10*/

CC_W1 (mgbase, indrct_addreg, 24);  /*window C location left edge / 5: ccind8 0*/
CC_W1 (mgbase, indrct_datareg,128);  /*640*/	

CC_W1 (mgbase, indrct_addreg, 25);/*window A location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 256);	/*: 1280 0*/

CC_W1 (mgbase, indrct_addreg, 26); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x14);	

CC_W1 (mgbase, indrct_addreg, 27);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 28);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 0xe0);	

CC_W1 (mgbase, indrct_addreg, 29); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 30);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

/*CC_W1 (mgbase, indrct_addreg, 18);*//*stuff for window B: ccind18 0x10*/
/*CC_W1 (mgbase, indrct_datareg, 0x10);	*/

/*CC_W1 (mgbase, indrct_addreg, 26);*//*C top bottom blackout ccind26 0x10 */
/*CC_W1 (mgbase, indrct_datareg, 0x10);	*/

CC_W1 (mgbase, indrct_addreg, 32); /*complete current window :ccind32 2*/
CC_W1 (mgbase, indrct_datareg, 0x0A);	

CC_W1 (mgbase, indrct_addreg, 16); /*window B location left edge / 5:*/
CC_W1 (mgbase, indrct_datareg,64); /* left 0*/

CC_W1 (mgbase, indrct_addreg, 17);/*window B location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 192);	/*: right 640*/

CC_W1 (mgbase, indrct_addreg, 18); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x0e);	

CC_W1 (mgbase, indrct_addreg, 19);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 20);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 255);	

CC_W1 (mgbase, indrct_addreg, 21); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 22);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	


CC_W1 (mgbase, indrct_addreg, 54); /*video input for gfx A,*/
                                 /*video for gfx B  ccind54 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 55); /*video input for gfx C,*/
                                 /*video for expansion 0  ccind55 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 56); /*frame buffer input is video input*/
                            /*video output is frame buffer output ccind56 30*/
CC_W1 (mgbase, indrct_datareg, 0x30);	

CC_W1 (mgbase, indrct_addreg, 57); /*forground input and bkgrnd is video: */
                                 /*  ccind57 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 58); /*forground and bkgrnd alpha is video*/ 
                                 /*  ccind58 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 0); /*A:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x80);	

CC_W1 (mgbase, indrct_addreg, 2); /*B:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	 /*changed !*/

CC_W1 (mgbase, sysctl, sysctlval);
/*CC_W1 (mgbase, sysctl, 0x48);*/
                       /*video double buffering, start, video output(7191)*/


	       }



/******************************************************************************
*
*  g2vb - This function is taken from a script written by Bob Williams: s_v2ga
*       It makes the appropriate AB and CC calls to set up a video 
*       to graphics channel B .  ev1_init may need to be run before this
*       command. The shell script function vid2gfxa calls gfxset, then
*       ev1_init then this function.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro
*   Note: Currently this function doesn't work (only paths through A do)
*  jfg 5/95
******************************************************************************/


void
g2vb()
{
CC_W1 (mgbase, indrct_addreg, 36); /*first unblanked video output line */
CC_W1 (mgbase, indrct_datareg, 0x01);	

AB_W1(mgbase,sysctl,0x1);  /*NTSC, express video disable keyin:  abdir5 1 */
                          
AB_W1(mgbase,high_addr,0);/*Window Display disabled, flicker reduction off*/
AB_W1(mgbase,low_addr,5);  /*pan disable, no zoom: abind5 0   */
AB_W1(mgbase,intrnl_reg,0);   /*currently not touching A*/

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,10);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,11);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,12);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,13);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,14);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0); /* enable window, deinterlace off*/
AB_W1(mgbase,low_addr,15);
/*AB_W1(mgbase,intrnl_reg,0x83);  */
AB_W1(mgbase,intrnl_reg,0x3);  

AB_W1(mgbase,high_addr,0); /* GFX TO VIDEO */
AB_W1(mgbase,low_addr,16);
AB_W1(mgbase,intrnl_reg,2);  

AB_W1(mgbase,high_addr,0); /* decimate  1 times */
AB_W1(mgbase,low_addr,17);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);/* window c disable,  :  abind 15 0 abind25 0*/
AB_W1(mgbase,low_addr,25);
AB_W1(mgbase,intrnl_reg,0);  

CC_W1 (mgbase, indrct_addreg, 10); /*top bottom blackout: ccind10 0x10 */
CC_W1 (mgbase, indrct_datareg, 0x10);  /*CHANGE BACK WHEN A ALSO IS ACTIVE*/

CC_W1 (mgbase, indrct_addreg, 26);/*C top bottom blackout ccind26 0x10 */
CC_W1 (mgbase, indrct_datareg, 0x10);	

CC_W1 (mgbase, indrct_addreg, 16);/*window B location left edge /5 */
                                   /*ccind16 128*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 17);/*window B location right edge /5 */
                                   /*ccind17 255*/
CC_W1 (mgbase, indrct_datareg, 127);	

CC_W1 (mgbase, indrct_addreg, 18); /*top bottom blackout: ccind18 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x04);	


CC_W1 (mgbase, indrct_addreg, 19);    /*top line number  :ccind19 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 20);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 240);	

CC_W1 (mgbase, indrct_addreg, 21); /*phase detector threshold upper :ccind21 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 22);/*phase detector threshold lower:ccind22 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

CC_W1 (mgbase, indrct_addreg, 3);/*B first line ccind3 10*/
CC_W1 (mgbase, indrct_datareg, 0x10);	

CC_W1 (mgbase, indrct_addreg, 32); /*complete current window :ccind32 2*/
CC_W1 (mgbase, indrct_datareg, 0xA);	

/*CC_W1 (mgbase, indrct_addreg, 36);*/ /*first unblanked video output line */
/*CC_W1 (mgbase, indrct_datareg, 0x01);	*/

CC_W1 (mgbase, indrct_addreg, 54); /*video input for gfx A,*/
                                 /*video for gfx B  ccind54 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 55); /*video input for gfx C,*/
                                 /*video for expansion 0  ccind55 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

/*CC_W1 (mgbase, indrct_addreg, 56);*/ /*frame buffer input is video input*/
                            /*video output is gfx channel B pixel data: */
                            /*ccind56 30*/
CC_W1 (mgbase, indrct_addreg, 56); /*video out is gfx b*/
CC_W1 (mgbase, indrct_datareg, 0x50);	

CC_W1 (mgbase, indrct_addreg, 57); /*forground input and bkgrnd is video: */
                                 /*  ccind57 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 58); /*forground and bkgrnd alpha is video*/ 
                                 /*  ccind58 0*/
CC_W1 (mgbase, indrct_datareg, 0);	


CC_W1 (mgbase, indrct_addreg, 2); /*B:gfx to video , read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x80);	

CC_W1 (mgbase, indrct_addreg, 0); /*B:gfx to video , read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x80);	

/*CC_W1 (mgbase, sysctl, 0x48);*/
CC_W1 (mgbase, sysctl, sysctlval); /* also set TBC input function enable*/
                       /*video double buffering, start, video output(7191)*/
	       }


/******************************************************************************
*
*  v2ga - This function is taken from a script written by Bob Williams: s_v2ga
*       It makes the appropriate AB and CC calls to set up a video 
*       to graphics channel A .  ev1_init may need to be run before this
*       command. The shell script function vid2gfxa calls gfxset, then
*       ev1_init then this function.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro
*   
*  jfg 5/95
******************************************************************************/

void
g2va()
{
CC_W1 (mgbase, indrct_addreg, 36); /*first unblanked video output line */
CC_W1 (mgbase, indrct_datareg, 0x01);	

AB_W1(mgbase,sysctl,0x1);   /*NTSC, express video disable keyin:  abdir5 1 */

AB_W1(mgbase,high_addr,0);/*Window Display disabled, flicker reduction off*/
AB_W1(mgbase,low_addr,5);  /*pan disable, no zoom: abind5 0   */
AB_W1(mgbase,intrnl_reg,3);  

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,0);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,1);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,2);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,3);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,4);
/*AB_W1(mgbase,intrnl_reg,0x20);  */
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0); /* enable window, deinterlace off*/
AB_W1(mgbase,low_addr,5);
/*AB_W1(mgbase,intrnl_reg,0x83);  */
AB_W1(mgbase,intrnl_reg,0x3); 

AB_W1(mgbase,high_addr,0); /* GFX TO VIDEO */
AB_W1(mgbase,low_addr,6);
AB_W1(mgbase,intrnl_reg,2);  

AB_W1(mgbase,high_addr,0); /* decimate  1 times */
AB_W1(mgbase,low_addr,7);
     AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);/* window c disable,  :  abind 15 0 abind25 0*/
AB_W1(mgbase,low_addr,25);
AB_W1(mgbase,intrnl_reg,0);  

CC_W1 (mgbase, indrct_addreg, 8); /*window A location*/
CC_W1 (mgbase, indrct_datareg, 0x0);	

CC_W1 (mgbase, indrct_addreg, 9); /*window A location*/
CC_W1 (mgbase, indrct_datareg, 127);	

CC_W1 (mgbase, indrct_addreg, 10); /*top bottom blackout: ccind10 0x10 */
CC_W1 (mgbase, indrct_datareg, 0x4);	

CC_W1 (mgbase, indrct_addreg, 11); /*window A top line number */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 12); /*window A bottom line number */
CC_W1 (mgbase, indrct_datareg, 240);	

CC_W1 (mgbase, indrct_addreg, 13); /*phase detector threshold*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 14);
CC_W1 (mgbase, indrct_datareg, 130);	

CC_W1 (mgbase, indrct_addreg, 18);/*C top bottom blackout ccind26 0x10 */
CC_W1 (mgbase, indrct_datareg, 0x10);	

CC_W1 (mgbase, indrct_addreg, 26);/*C top bottom blackout ccind26 0x10 */
CC_W1 (mgbase, indrct_datareg, 0x10);	

CC_W1 (mgbase, indrct_addreg, 1);
CC_W1 (mgbase, indrct_datareg, 0x10);	

CC_W1 (mgbase, indrct_addreg, 32);
CC_W1 (mgbase, indrct_datareg, 0xA);	

CC_W1 (mgbase, indrct_addreg, 54); /*video input for gfx A,*/
                                 /*video for gfx B  ccind54 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 55); /*video input for gfx C,*/
                                 /*video for expansion 0  ccind55 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

/*CC_W1 (mgbase, indrct_addreg, 56);*/ /*frame buffer input is video input*/
                            /*video output is gfx channel B pixel data: */
                            /*ccind56 30*/
CC_W1 (mgbase, indrct_addreg, 56); /*video out is gfx a*/
CC_W1 (mgbase, indrct_datareg, 0x40);	
CC_W1 (mgbase, indrct_addreg, 57); /*forground input and bkgrnd is video: */
                                 /*  ccind57 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 58); /*forground and bkgrnd alpha is video*/ 
                                 /*  ccind58 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 2); /*B:gfx to video , read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x80);	

CC_W1 (mgbase, indrct_addreg, 0); /*B:gfx to video , read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x80);	

/*CC_W1 (mgbase, sysctl, 0x48);*/
CC_W1 (mgbase, sysctl, sysctlval); /* also set TBC input function enable*/
                       /*video double buffering, start, video output(7191)*/
	       }


/******************************************************************************
*
*  v2gc - This function is taken from a script written by Bob Williams: s_v2ga
*       It makes the appropriate AB and CC calls to set up a video 
*       to graphics channel B .  ev1_init may need to be run before this
*       command. The shell script function vid2gfxa calls gfxset, then
*       ev1_init then this function.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro
*   Note: Currently this function doesn't work (only paths through A do)
*  jfg 5/95
******************************************************************************/
void
v2gc()
{
/* 320 -> 960, 0 -> 512 */
AB_W1(mgbase,sysctl,0x1);   /*NTSC, express video disable keyin:  abdir5 1 */

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,0);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,1);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,2);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,3);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,4);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display enable, flicker reduction on*/
AB_W1(mgbase,low_addr,5);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,6);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /*1x decimation :abind7 0    */
AB_W1(mgbase,low_addr,7);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,10);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,11);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,12);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,13);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,14);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display disable, flicker reduction on*/
AB_W1(mgbase,low_addr,15);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,2);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,16);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);  /*1x decimation :abind7 0    */
AB_W1(mgbase,low_addr,17);
AB_W1(mgbase,intrnl_reg,0);  

AB_W1(mgbase,high_addr,0);          /* panning x offset  : abind0 0 ,abind1 0  */
AB_W1(mgbase,low_addr,20);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,21);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0);       /* panning y offset  : abind2 0, abind3 0  */
AB_W1(mgbase,low_addr,22);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); 
AB_W1(mgbase,low_addr,23);
AB_W1(mgbase,intrnl_reg,0);

AB_W1(mgbase,high_addr,0); /* XMOD  abind4 0x20*/
AB_W1(mgbase,low_addr,24);
AB_W1(mgbase,intrnl_reg,0x20);  

AB_W1(mgbase,high_addr,0);/*Window Display disable, flicker reduction on*/
AB_W1(mgbase,low_addr,25);  /*pan diable, no zoom: abind5 3    */
AB_W1(mgbase,intrnl_reg,0x03);  

AB_W1(mgbase,high_addr,0);  /* freeze disable : abind6 0    */
AB_W1(mgbase,low_addr,26);
AB_W1(mgbase,intrnl_reg,0);  

CC_W1 (mgbase, indrct_addreg, 0); /*A:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	

CC_W1 (mgbase, indrct_addreg, 1);    /*first active line 12? :ccind1 1 */
CC_W1 (mgbase, indrct_datareg, 1);	

CC_W1 (mgbase, indrct_addreg, 2); /*B:video to gfx, read- not done yet*/
                                 /*  ccind2 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	 /*changed !*/

CC_W1 (mgbase, indrct_addreg, 3);    /*first active line 12? for B and C */
CC_W1 (mgbase, indrct_datareg, 1);	

CC_W1 (mgbase, indrct_addreg, 4); /*C:video to gfx, read- not done yet*/
                                 /*  ccind2 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	 /*changed !*/

CC_W1 (mgbase, indrct_addreg, 8);   /*window A location left edge / 5: ccind8 0*/
CC_W1 (mgbase, indrct_datareg, 128);   /*640*/  

CC_W1 (mgbase, indrct_addreg, 9);/*window A location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 256);  /*1280*/	/*: ccind9 127 0*/

CC_W1 (mgbase, indrct_addreg, 10); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x14);	

CC_W1 (mgbase, indrct_addreg, 11);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	
                                     /*  0 -> 480 (top,bot); 640->1280 (l->r)*/
CC_W1 (mgbase, indrct_addreg, 12);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 0xe0);	

CC_W1 (mgbase, indrct_addreg, 13); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 14);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

/* stuff for window B: ccind18 0x10*/

CC_W1 (mgbase, indrct_addreg, 16); /*window B location left edge / 5:*/
CC_W1 (mgbase, indrct_datareg, 0); /* left 0*/

CC_W1 (mgbase, indrct_addreg, 17);/*window B location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 128);	/*: right 640*/

CC_W1 (mgbase, indrct_addreg, 18); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x14);	

CC_W1 (mgbase, indrct_addreg, 19);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 20);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 0xe0);  	

CC_W1 (mgbase, indrct_addreg, 21); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 22);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

CC_W1 (mgbase, indrct_addreg, 24);  /*window C location left edge / 5: ccind8 0*/
CC_W1 (mgbase, indrct_datareg,64);  /*320*/	

CC_W1 (mgbase, indrct_addreg, 25);/*window A location right edge / 5*/
CC_W1 (mgbase, indrct_datareg, 192);	/* 960 */

CC_W1 (mgbase, indrct_addreg, 26); /*top bottom blackout: ccind10 0x0e */
CC_W1 (mgbase, indrct_datareg, 0x0e);	
                                         /*    512 x  1023*/
CC_W1 (mgbase, indrct_addreg, 27);    /*top line number  :ccind11 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 28);    /*bottom  line number  :ccind12 0 */
CC_W1 (mgbase, indrct_datareg, 0x255);	

CC_W1 (mgbase, indrct_addreg, 29); /*phase detector threshold upper :ccind13 0 */
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 30);/*phase detector threshold lower:ccind14 130 */
CC_W1 (mgbase, indrct_datareg, 130);	

/*CC_W1 (mgbase, indrct_addreg, 18);*//*stuff for window B: ccind18 0x10*/
/*CC_W1 (mgbase, indrct_datareg, 0x10);	*/

/*CC_W1 (mgbase, indrct_addreg, 26);*//*C top bottom blackout ccind26 0x10 */
/*CC_W1 (mgbase, indrct_datareg, 0x10);	*/

CC_W1 (mgbase, indrct_addreg, 32); /*complete current window :ccind32 2*/
CC_W1 (mgbase, indrct_datareg, 0x0A);	

CC_W1 (mgbase, indrct_addreg, 54); /*video input for gfx A,*/
                                 /*video for gfx B  ccind54 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 55); /*video input for gfx C,*/
                                 /*video for expansion 0  ccind55 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 56); /*frame buffer input is video input*/
                            /*video output is frame buffer output ccind56 30*/
CC_W1 (mgbase, indrct_datareg, 0x00);   /*was 0x30*/	

CC_W1 (mgbase, indrct_addreg, 57); /*forground input and bkgrnd is video: */
                                   /*ccind57 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 58); /*forground and bkgrnd alpha is video*/ 
                                 /*  ccind58 0*/
CC_W1 (mgbase, indrct_datareg, 0);	

CC_W1 (mgbase, indrct_addreg, 0); /*A:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x81);	

CC_W1 (mgbase, indrct_addreg, 2); /*B:video to gfx, read- not done yet*/
                                 /*  ccind0 0x81*/
CC_W1 (mgbase, indrct_datareg, 0x80);	 /*changed !*/

CC_W1 (mgbase, sysctl, sysctlval);
/*CC_W1 (mgbase, sysctl, 0x48);*/
                       /*video double buffering, start, video output(7191)*/


	       }

/******************************************************************************
*
*  i2ga - This function uses v2ga to make an indycam connection to graphics
*       It makes the appropriate AB and CC calls to set up a video 
*       to graphics channel A .  ev1_init may need to be run before this
*       command. The shell script function vid2gfxa calls gfxset, then
*       ev1_init then this function.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro

*  jfg 5/95
******************************************************************************/

void
i2ga()   /*testing indycam input*/
{
struct ev1_info *info, ev1info;
info = &ev1info;
bzero((void *)info, sizeof(struct ev1_info));

v2ga();
CC_W1 (mgbase, indrct_addreg, 38); /*enable expansion port 1*/
CC_W1 (mgbase, indrct_datareg, 0x80);	

CC_W1 (mgbase, indrct_addreg, 54); /*gfx channel A and B from expansion port 1*/
CC_W1 (mgbase, indrct_datareg, 0x22);	

CC_W1 (mgbase, indrct_addreg, 56);/*video out from buffer, buffer from e-port 1*/
CC_W1 (mgbase, indrct_datareg, 0x32);	

mgv1_i2c_initalize(INDYCAM_ADDR,mgbase,info);
CC_W1 (mgbase, sysctl, sysctlval);
}


/******************************************************************************
*
*  ci2cio - Using loopback cable 2 designed by Bob Cannataro, this function
*       takes cosmo input from the video board (specifically the unidirect
*       digital in) loops it to bidirection cio, and reads it in.
*       This test has limitations in that the bidirectional input
*       does not have the current circuitary to lock the digital
*       input source (i.e. indy cam), but it doesn verify that
*       the path is connected.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro

*  jfg 5/95
******************************************************************************/


void       /*loop cable 2 */
ci2cio()  
{                                  /*of cio */
struct ev1_info *info, ev1info;
info = &ev1info;
bzero((void *)info, sizeof(struct ev1_info));

v2ga();
                              
CC_W1 (mgbase, indrct_addreg, 37); /*normal input, not cosmo on unidirect*/
CC_W1 (mgbase, indrct_datareg, 0x11);	/*from cosmo on bidirect */

CC_W1 (mgbase, indrct_addreg, 38); /*disable expansion port 1*/
CC_W1 (mgbase, indrct_datareg, 0x40);	/*enable expansion port 0 */

CC_W1 (mgbase, indrct_addreg, 54); /*gfx channel A and B from expansion port 0*/
CC_W1 (mgbase, indrct_datareg, 0x11);	

CC_W1 (mgbase, indrct_addreg, 56);/*video out from buffer, buffer from e-port 0*/
CC_W1 (mgbase, indrct_datareg, 0x31);	

mgv1_i2c_initalize(INDYCAM_ADDR,mgbase,info);

sysctlval =  0x68;
CC_W1 (mgbase, sysctl, sysctlval);
}


/******************************************************************************
*
*  cio2ci - Using loopback cable 1 designed by Bob Cannataro, this function
*      takes directcosmo input from the video board (specifically the unidirect
*       digital in) loops it to bidirection cio, and reads it in.
*       This test has limitations in that the bidirectional input
*       does not have the current circuitary to lock the digital
*       input source (i.e. indy cam), but it doesn verify that
*       the path is connected.
*       Possible inputs are "s" svideo (2nd svideo connector on ABOB), 
*       "g" graphics, "d" for digital (indycam) or composite in (default).
*        There may be problems with getting a timing lock when using live
*       video in. 
*       (Composite in uses 1st composite input  on ABOB)
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro
*
*  jfg 5/95
******************************************************************************/

void   /*loop cable 1 */
cio2ci(int argc, char **argv)     /*testing cosmo input, looping to cio, and testing output direction*/
{                                  /*of cio */
struct ev1_info *info, ev1info;
info = &ev1info;
bzero((void *)info, sizeof(struct ev1_info));

if ((argc > 1)&&(strncmp(argv[1],"s",1) == 0)){
msg_printf(DBG, "svideo input \n");
sv2ga();}
else if ((argc > 1)&&(strncmp(argv[1],"g",1) == 0)){
msg_printf(DBG, "graphics input \n");
g2va();
}
else
{v2ga();

CC_W1 (mgbase, indrct_addreg, 38); /*enable expansion port 1 input*/
CC_W1 (mgbase, indrct_datareg, 0xc0);	/*disable expansion port 0 input */

CC_W1 (mgbase, indrct_addreg, 54);/*gfx channel A and B from expansion port 0*/
CC_W1 (mgbase, indrct_datareg, 0x22);	

CC_W1 (mgbase, indrct_addreg, 55); /*expansion 0 from frame buffer */
CC_W1 (mgbase, indrct_datareg, 0x30);	

CC_W1 (mgbase, indrct_addreg, 56);/*video out from expansion 1 */
                                 /*buffer from videoinput */
CC_W1 (mgbase, indrct_datareg, 0x20);	
}

CC_W1 (mgbase, indrct_addreg, 37); /*unidirect from cosmo*/
CC_W1 (mgbase, indrct_datareg, 0x08);	/*to cosmo on bidirect */


if ((argc > 1)&&(strncmp(argv[1],"g",1) == 0)){
msg_printf(DBG, "graphics input \n");

CC_W1 (mgbase, indrct_addreg, 38); /*enable expansion port 1 input*/
CC_W1 (mgbase, indrct_datareg, 0x80);	/*disable expansion port 0 input */
                                         /*disable video input*/
CC_W1 (mgbase, indrct_addreg, 55); /*expansion 0 from gfx a */
CC_W1 (mgbase, indrct_datareg, 0x40);	

CC_W1 (mgbase, indrct_addreg, 56);/*video out from buffer */
                                 /*buffer from expansion port 1 */
CC_W1 (mgbase, indrct_datareg, 0x32);	
/*unslave();*/
}
else{
msg_printf(DBG, "SLAVE \n");
slave();}

if ((argc > 2)&&(strncmp(argv[2],"d",1) == 0)){
CC_W1 (mgbase, indrct_addreg, 37); /*unidirect from cosmo*/
CC_W1 (mgbase, indrct_datareg, 0x00);	/*to cosmo on bidirect */

}
/*mgv1_i2c_initalize(INDYCAM_ADDR,mgbase,info);*/
sysctlval =  0x4c;
CC_W1 (mgbase, sysctl, sysctlval);
}




/******************************************************************************
*
*  dloop - Using the digital loopback cable (actually it is just a terminator
*       with internal cabling looped back), this function
*       takes graphics and send it out to the bidirectional digital
*       output and reads it in to the unidirectional digital input
*       which is sent out to video out.  This is only a visual verification.
*       A more precise verification can be done by adapting Judy Ting's
*       unix loopback diags.
*       This routine could certainly be more efficiently written with
*       an indirect AB1/CC1 register macro
*
*  jfg 5/95
******************************************************************************/


void dloop()
{ 
struct ev1_info *info, ev1info;
info = &ev1info;
bzero((void *)info, sizeof(struct ev1_info));

g2va();

CC_W1 (mgbase, indrct_addreg, 38); /*enable expansion port 1 input*/
CC_W1 (mgbase, indrct_datareg, 0x80);	/*disable expansion port 0 input */
                                         /*disable video input*/
CC_W1 (mgbase, indrct_addreg, 55); /*expansion 0 from gfx a */
CC_W1 (mgbase, indrct_datareg, 0x40);	


CC_W1 (mgbase, indrct_addreg, 56);/*video out from buffer */
                                 /*buffer from expansion port 1 */
CC_W1 (mgbase, indrct_datareg, 0x32);	

CC_W1 (mgbase, indrct_addreg, 37); /*unidirect from cosmo*/
CC_W1 (mgbase, indrct_datareg, 0x00);	/*to cosmo on bidirect */

sysctlval =  0x4c;
CC_W1 (mgbase, sysctl, sysctlval);
}

