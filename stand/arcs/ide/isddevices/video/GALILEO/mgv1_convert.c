#include <math.h>
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "uif.h"
#include "sys/param.h"
#include "parser.h"

#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

extern mgras_hw *mgbase;

/******************************************************************************
*
*  convert - The function makes an alpha blender background come from a
*            flat field.  This field is either specified as R G B
*            and then CONVERTed to YUV or entered in YUV. 
*            The YUV value is converted internally in the AB1 to RGB
*            before being sent to the AB1s for display.  
*
*            The original intention of this test was to 
*            test the specific connector
*            wires between the video and graphics board.  If R G B
*            is entered with only 1 bit on, converted to YUV, converted
*            back to RGB (hopefully the same number) the display will
*            either be black or the given color, providing that the wire
*            is properly connected.  The RGB->YUV conversion is exactly
*            inverse of the YUV->RGB routine inside the CC1
*
*            This test was eventually abandon since it was too tedious
*            to test in this manner but this code remains for possible
*            future use.
*  jfg 5/95
******************************************************************************/


void convert(int argc, char **argv)
{

#if 0
int red,green,blue;
int y,u,v;
int i,i2,i3 ;
unsigned int ored,ogreen,oblue;

if ((argc > 4)&&(strncmp(argv[4],"d",1) == 0)){
red = atoi(argv[1]);    /*if a "d" is put after*/
green = atoi(argv[2]);  /*rgb input parameters, then decimal*/
blue = atoi(argv[3]);}
else {         /*else is hex*/
atohex(argv[1],&red);
atohex(argv[2],&green);
atohex(argv[3],&blue);}




/*in case are inputting y u v */
if (((argc > 4)&&(strncmp(argv[4],"y",1) == 0))||
   ((argc > 5)&&(strncmp(argv[5],"y",1) == 0))){
y = red;
u = green;
v =  blue;
ored = ( ((1192 * (y - 16))/128) + ((1634 * (v - 128))/128) + 4) / 8;
oblue = ( ((1192 * (y - 16))/128) + ((2066 * (u - 128))/128) + 4) / 8;
ogreen = ( ((1192 * (y - 16))/128) - ((832 * (v - 128))/128) - ((401 * (u-128))/128) + 4) / 8;

if ((argc > 4)&&(strncmp(argv[4],"d",1) == 0)){
printf("output in decimal y %d u %d v %d \n",y,u,v);
printf("output in decimal red %d green %d blue %d \n",ored,ogreen,oblue);}
else{
printf("output in hex y 0x%x u 0x%x v 0x%x \n",y,u,v);
printf("output in hex red 0x%x green 0x%x blue 0x%x \n",ored,ogreen,oblue);}
}
else {
y = (( ((263 * red )/128) + ((516*green)/128) + ((100*blue)/128) + 4) /8) + 16;
u = (( ((298*(blue - green))/128) + ((152*(blue-red))/128) + 2) / 8 ) + 128;
v = (( ((377* (red - green))/128) + ((73 * (red - blue))/128) + 2) / 8) + 128;
ored = ( ((1192 * (y - 16))/128) + ((1634 * (v - 128))/128) + 4) / 8;
oblue = ( ((1192 * (y - 16))/128) + ((2066 * (u - 128))/128) + 4) / 8;
ogreen = ( ((1192 * (y - 16))/128) - ((832 * (v - 128))/128) - ((401 * (u-128))/128) + 4) / 8;
if ((argc > 4)&&(strncmp(argv[4],"d",1) == 0)){
printf("input in decimal red %d green %d blue %d \n",red,green,blue);
printf("output in decimal y %d u %d v %d \n",y,u,v);
printf("output in decimal red %d green %d blue %d \n",ored,ogreen,oblue);}
else{
printf("input in hex red 0x%x green 0x%x blue 0x%x \n",red,green,blue);
printf("output in hex y 0x%x u 0x%x v 0x%x \n",y,u,v);
printf("output in hex red 0x%x green 0x%x blue 0x%x \n",ored,ogreen,oblue);}
}
/*now set up the CC1*/

v2ga();

CC_W1 (mgbase, indrct_addreg, 50);    /*set alpha blender background */
                                    /*to be from flat field*/
CC_W1 (mgbase, indrct_datareg, 0x01);	

CC_W1 (mgbase, indrct_addreg, 51);    /*Y input value */
CC_W1 (mgbase, indrct_datareg, y);	

CC_W1 (mgbase, indrct_addreg, 52);    /*U input value */
CC_W1 (mgbase, indrct_datareg, u);	

CC_W1 (mgbase, indrct_addreg, 53);    /*V input value */
CC_W1 (mgbase, indrct_datareg, v);	

CC_W1 (mgbase, indrct_addreg, 54);    /*make graphics channel A alpha blender */
                                    /*pixel output */
CC_W1 (mgbase, indrct_datareg, 0x07);	

CC_W1 (mgbase, indrct_addreg, 58);    /*make background everywhere, foreground*/
                                    /*nowhere*/
CC_W1 (mgbase, indrct_datareg, 0x00);	

CC_W1 (mgbase, indrct_addreg, 59);    /*make background everywhere, foreground*/
                                    /*nowhere*/
CC_W1 (mgbase, indrct_datareg, 0x01);	

CC_W1 (mgbase, indrct_addreg, 79);    /*disable alpha blender shadow scaling */

CC_W1 (mgbase, indrct_datareg, 0x01);	

#endif

}  /*end of convert*/











