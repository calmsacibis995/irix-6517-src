/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "sys/gr2hw.h"
#include "sys/gr2_if.h"
#include "ide_msg.h"
#include "libsc.h"
#include "uif.h"

#include "diag_glob.h"
#include "mc.out.h"

#include "geucout.h"
#include "hqucout.h"

void gr2_load_ge_ucode(struct gr2_hw *);
void verify_ge_ucode(struct gr2_hw *);
void image_info(void);
void gr2_load_hq_ucode(struct gr2_hw *);
void verify_hq_ucode(struct gr2_hw *);

#define MAX_GE_RECORDS	65536
#define MAX_HQ_RECORDS  32768

extern struct gr2_hw *base; 

mcout_t *header;

unsigned char *code_start;


/*ARGSUSED*/
void
gr2_ucload(int argc, char *argv[])
{
  
    if( (strcmp(argv[1], "hq") != 0) && (strcmp(argv[1], "ge") != 0) )
    {
	msg_printf(ERR, "Unknown processor type specified\n");
	return;
    }


    if( (strcmp(argv[1], "ge" ) == 0 ) )
    {

	header = (mcout_t *)ge_diag_ucode_str.ge_ucode;

	image_info();
        
	code_start = (unsigned char *)&ge_diag_ucode_str.ge_ucode[header->f_codeoff/2];

	gr2_load_ge_ucode(base);

        code_start = (unsigned char *)&ge_diag_ucode_str.ge_ucode[header->f_codeoff/2];

	verify_ge_ucode(base);
    }

    if( (strcmp(argv[1], "hq" ) == 0 ) )
    {
	header = (mcout_t *)hq_diag_ucode_str.hq_ucode;

	image_info();

	code_start = (unsigned char *)&hq_diag_ucode_str.hq_ucode[header->f_codeoff/2];

	gr2_load_hq_ucode(base);

        code_start = (unsigned char *)&hq_diag_ucode_str.hq_ucode[header->f_codeoff/2];

	verify_hq_ucode(base);
    }
}

void
image_info(void)
{ 

    if( (header->f_magic != HQ2_MAGIC) && (header->f_magic != GE7_MAGIC) )
    {
    extern void EnterInteractiveMode(void);
	msg_printf(ERR,"This is not a metastep linker file bad magic number!!!\n");
	EnterInteractiveMode();
    }
    
    msg_printf(DBG,"Header  : %d bytes\n", HEADER_SIZE);
    msg_printf(DBG,"Magic   : %x\n", header->f_magic);
    msg_printf(DBG,"Version : %s", header->version);
    msg_printf(DBG,"Date    : %s", header->date);
    msg_printf(DBG,"Path    : %s\n", header->path);
    msg_printf(DBG,"Length  : %d bytes per microword   =>   %d bit control word\n", 
		header->length, header->length * 8);

}

void
gr2_load_ge_ucode(struct gr2_hw *base)
{
    GE_RECORD *baseptr;    

    int index, i, total;

    index = 0;
    total = 0;

    for(i=0; i < 512; i++)
    {
	base->shram[CONSTMEM + i] = header->data[i];
    }


    while(1)
    {

	baseptr = (GE_RECORD *)code_start;

	if(baseptr->address == 0xffff) break;

	base->hq.gepc = baseptr->address;

	base->ge[0].ram0[0xf8] = (baseptr->ucode[8]  << 16 ) | baseptr->ucode[9];
	base->ge[0].ram0[0xf9] = (baseptr->ucode[6]  << 16 ) | baseptr->ucode[7];
	base->ge[0].ram0[0xfa] = (baseptr->ucode[4]  << 16 ) | baseptr->ucode[5];
	base->ge[0].ram0[0xfb] = (baseptr->ucode[2]  << 16 ) | baseptr->ucode[3];

	base->hq.ge7loaducode = (baseptr->ucode[0] << 16 ) | baseptr->ucode[1];

	code_start += header->length + 6;

	total++;

	if(index++ == 256)
	{
	    msg_printf(DBG, ".");
	    index = 0;
	}
	
    }

    msg_printf(DBG, "\nSize of ucode -> %d\n", total);


}

void
verify_ge_ucode(struct gr2_hw *base)
{
    GE_RECORD *baseptr;    

    int i, error, sherror;

    error = 0;
    sherror = 0;

    for(i=0; i < 512; i++)
    {
	if( base->shram[CONSTMEM + i] != header->data[i])
	{
	    msg_printf(DBG,"ERROR **** Shram Location -> %x     expected -> %x     actual -> %x\n",
			    CONSTMEM + i, header->data[i], base->shram[CONSTMEM + i]);
	    sherror++;
	}
    }


    while(1)
    {

	baseptr = (GE_RECORD *)code_start;

	if(baseptr->address == 0xffff) break;

	base->hq.gepc = baseptr->address;

	if( base->hq.ge7loaducode != ((baseptr->ucode[0] << 16 ) | baseptr->ucode[1])) error++;

	if( base->ge[0].ram0[0xf8] != ((baseptr->ucode[8]  << 16 ) | baseptr->ucode[9])) error++;
	if( base->ge[0].ram0[0xf9] != ((baseptr->ucode[6]  << 16 ) | baseptr->ucode[7])) error++;
	if( base->ge[0].ram0[0xfa] != ((baseptr->ucode[4]  << 16 ) | baseptr->ucode[5])) error++;
	if( base->ge[0].ram0[0xfb] != ((baseptr->ucode[2]  << 16 ) | baseptr->ucode[3])) error++;


	code_start += header->length + 6;

    }


    if(error)
    {
	msg_printf(DBG, "%d errors were detected in ge_ucode\n", error);	
    }
    else
    {
	msg_printf(DBG, "Download was successful no errors detected.....\n");	
    }


    if(sherror)
    {
	msg_printf(DBG, "%d errors were detected in shram constants\n", sherror);	
	base->shram[TP_PROBE_ID] = 0;
    }
    else
    {
	msg_printf(DBG, "Download was successful no errors detected in shram constants.....\n");	
	base->shram[TP_PROBE_ID] = UCODE_DIAG;
    }

}

void
gr2_load_hq_ucode(struct gr2_hw *base)
{
    HQ_RECORD *baseptr; 

    int index, i, total;
    
    index = 0;
    total = 0;

    for(i=0; i < 16; i++)
    {
	base->hq.attrjmp[i] = header->data[i];
    }


    while(1)
    {

	baseptr = (HQ_RECORD *)code_start;

	if(baseptr->address == 0xffff) break;

	base->hqucode[baseptr->address] = (baseptr->ucode[0] << 16) | baseptr->ucode[1];
	
	code_start += header->length + 6;

	total++;

	if(index++ == 256)
	{
	    msg_printf(DBG, ".");
	    index = 0;
	}
	
    }

    msg_printf(DBG, "\nSize of ucode -> %d\n", total);

}

void
verify_hq_ucode(struct gr2_hw *base)
{
    HQ_RECORD *baseptr; 

    int error;
    
    error = 0;

    while(1)
    {

	baseptr = (HQ_RECORD *)code_start;

	if(baseptr->address == 0xffff) break;

	if( ( (baseptr->ucode[0] << 16) | baseptr->ucode[1] ) != base->hqucode[baseptr->address])
	{

	    msg_printf(DBG, "ERROR ****  Address -> %x    expected -> %x    actual ->  %x\n", 
		    baseptr->address,
		    base->hqucode[baseptr->address],
		    ( ( baseptr->ucode[0] << 16) | baseptr->ucode[1]) );
	    
	    error++;
	}

	code_start += header->length + 6;
    }

    if(error)
    {
	msg_printf(DBG, "%d errors were detected in hq_ucode\n", error);	
	base->shram[TP_PROBE_ID] = 0;
    }
    else
    {
	msg_printf(DBG, "Download was successful no errors detected.....\n");	
	base->shram[TP_PROBE_ID] = UCODE_DIAG;
    }

}
