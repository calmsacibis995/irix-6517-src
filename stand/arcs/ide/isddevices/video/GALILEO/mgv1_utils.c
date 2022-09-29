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

extern  mgras_hw *mgbase;

extern unsigned sysctlval;


/******************************************************************************
*
*  cc/ab set/get ind/dir - These are commands that allow for simple
*       peek and poke in the IDE environment.  There is no register 
*       shadowing so all registers are actually written and read.
*       If the register being read is not readable, the result is
*       undefined. 
*  jfg 5/95
******************************************************************************/


void ccsetind(int argc, char **argv)
{
unsigned int reg;
unsigned int val;

if (argc < 3){
  msg_printf(SUM, " Please provide both register and hex value \n");
  return ;}
reg = atoi(argv[1]);
atohex(argv[2],(int *)&val);
CC_W1 (mgbase, indrct_addreg, reg);    
CC_W1 (mgbase, indrct_datareg, val);	
CC_W1 (mgbase, sysctl, sysctlval);
msg_printf(DBG, "Wrote CC indirect %d 0x%x \n", reg, val);
}

void ccsetdir(int argc, char **argv)
{
unsigned int reg;
unsigned int val;

if (argc < 3){
  msg_printf(SUM, " Please provide both register and hex value \n");
 return;}

reg = atoi(argv[1]);
atohex(argv[2],(int *)&val);
switch(reg){
 case 0: CC_W1 (mgbase, sysctl, val);   
         sysctlval = val;
         break;
 case 1: CC_W1 (mgbase, indrct_addreg, val);   
         break;
 case 2: CC_W1 (mgbase, indrct_datareg, val);   
         break;
 case 3: CC_W1 (mgbase, fbctl, val);   
         break;
 case 4: CC_W1 (mgbase, fbsel, val);   
         break;
 case 5: CC_W1 (mgbase, fbdata, val);   
         break;
 case 6: CC_W1 (mgbase, I2C_bus, val);   
         break;
 case 7: CC_W1 (mgbase, I2C_data, val);   
         break;
 default: msg_printf(SUM, " Register > 7, Try again \n");
          return;
 }
CC_W1 (mgbase, sysctl, sysctlval);
msg_printf(DBG, "Wrote CC direct %d 0x%x \n", reg, val);
}

void ccgetdir(int argc, char **argv)
{
unsigned int reg;
unsigned int val;
if (argc < 2){
  msg_printf(SUM, " Please provide register \n");
 return;}

reg = atoi(argv[1]);


switch(reg){
 case 0: val = CC_R1 (mgbase, sysctl);   
         break;
 case 1: val = CC_R1 (mgbase, indrct_addreg);   
         break;
 case 2: val = CC_R1 (mgbase, indrct_datareg);   
         break;
 case 3: val = CC_R1 (mgbase, fbctl);   
         break;
 case 4: val = CC_R1 (mgbase, fbsel);   
         break;
 case 5: val = CC_R1 (mgbase, fbdata);   
         break;
 case 6: val = CC_R1 (mgbase, I2C_bus);   
         break;
 case 7: val = CC_R1 (mgbase, I2C_data);   
         break;
 default: msg_printf(SUM, " Register > 7, Try again \n");
          return;
 }
CC_W1 (mgbase, sysctl, sysctlval);
msg_printf(DBG, "CC direct %d = 0x%x \n", reg, val);
}

void abgetdir(int argc, char **argv)
{
unsigned int reg;
unsigned int val;

if (argc < 2){
  msg_printf(SUM, " Please provide register \n");
 return;}

reg = atoi(argv[1]);


switch(reg){
 case 0: val = AB_R1 (mgbase, dRam);   
         break;
 case 1: val = AB_R1 (mgbase, intrnl_reg);   
         break;
 case 2: val = AB_R1 (mgbase, tst_reg);   
         break;
 case 3: val = AB_R1 (mgbase, low_addr);   
         break;
 case 4: val = AB_R1 (mgbase, high_addr);   
         break;
 case 5: val = AB_R1 (mgbase, sysctl);   
         break;
 case 6: val = AB_R1 (mgbase, rgb_sel);   
         break;
 default: msg_printf(SUM, " Register > 6, Try again \n");
          return;
 }
msg_printf(DBG, "AB direct %d = 0x%x \n", reg, val);
CC_W1 (mgbase, sysctl, sysctlval);
}

void absetdir(int argc, char **argv)
{
unsigned int reg;
unsigned int val;

if (argc < 3){
  msg_printf(SUM, " Please provide both register and hex value \n");
  return;}

reg = atoi(argv[1]);
atobu(argv[2],&val);
switch(reg){
 case 0: AB_W1 (mgbase, dRam, val);   
         break;
 case 1: AB_W1 (mgbase, intrnl_reg, val);   
         break;
 case 2: AB_W1 (mgbase, tst_reg, val);   
         break;
 case 3: AB_W1 (mgbase, low_addr, val);   
         break;
 case 4: AB_W1 (mgbase, high_addr, val);   
         break;
 case 5: AB_W1 (mgbase, sysctl, val);   
         break;
 case 6: AB_W1 (mgbase, rgb_sel, val);   
         break;
 default: msg_printf(SUM, " Register > 6, Try again \n");
          return;
 }
CC_W1 (mgbase, sysctl, sysctlval);
msg_printf(DBG, "Wrote AB direct %d 0x%x \n", reg, val);
}

void absetind(int argc, char **argv)
{
unsigned int reg;
unsigned int val;

if (argc < 3){
  msg_printf(SUM, " Please provide both register and hex value \n");
 return;}

reg = atoi(argv[1]);

atobu(argv[2],&val);
AB_W1 (mgbase, high_addr,0);    
AB_W1 (mgbase, low_addr, reg);	
AB_W1 (mgbase, intrnl_reg,val);	
CC_W1 (mgbase, sysctl, sysctlval);
msg_printf(DBG, "Wrote AB indirect  %d 0x%x \n", reg, val);
}

void ccgetind(int argc, char **argv)
{
unsigned int reg;
unsigned int val;
if (argc < 2){
  msg_printf(SUM, " Please provide register \n");
 return;}

reg = atoi(argv[1]);
CC_W1 (mgbase, indrct_addreg, reg);   
val = CC_R1 (mgbase, indrct_datareg);   
msg_printf(DBG, "CC indirect %d = 0x%x \n", reg, val);
}

void abgetind(int argc, char **argv)
{
unsigned int reg;
unsigned int val;
if (argc < 2){
  msg_printf(SUM, " Please provide register \n");
 return;}

reg = atoi(argv[1]);
AB_W1 (mgbase, high_addr,0);    
AB_W1 (mgbase, low_addr, reg);	
val = AB_R1 (mgbase, intrnl_reg);	
msg_printf(DBG, "AB indirect %d = 0x%x \n", reg, val);
}









