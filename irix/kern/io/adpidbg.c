/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if IP32
#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/scsi.h>
#include <sys/graph.h>

#define SH_SG_DESCRIPTOR void
#include "adphim/osm.h"
#include "adphim/him_scb.h"
#include "adphim/him_equ.h"
#include <sys/adp78.h>


void    idbg_adp_sp_struct(sp_struct *scb_p);
void    idbg_adp_task_struct(adp78_ha_t *ha_p);
void    idbg_adp_data_struct(int adap);
void    idbg_adp_cfp_struct(int adap);
void    idbg_adp_adap_rec(int adap);
void    idbg_xfer_option(int adap);

extern adp78_adap_t **adp78_array;

struct adp_idbg_func
{
	char	*name;
	void	(*func)();
	char	*descrip;
};

struct adp_idbg_func adpidbg_funcs[] = {
    "adp_scb", idbg_adp_sp_struct, "Print adp78 scb structure",
    "adp_task", idbg_adp_task_struct, "Print adp78 SCSI handle structure",
    "adp_data", idbg_adp_data_struct, "Print adp78 adapter data structure",
    "adp_config", idbg_adp_cfp_struct, "Print adp78 adapter Config Structure",
    "adp_adp", idbg_adp_adap_rec, "Print adp78 adapter control info",
    "adp_xopt", idbg_xfer_option, "Print xfer option bytes",
    0, 0, 0
};


void
adpidbg_init(void)
{
	struct adp_idbg_func	*fp;

	for (fp = adpidbg_funcs; fp->name; fp++)
		idbg_addfunc(fp->name, fp->func);
}

void
idbg_adp_sp_struct(sp_struct *scb_p)
{
        int i;

	qprintf("scb at 0x%x segcnt %d (0x%x)\n", scb_p, scb_p->SP_SegCnt,
	       scb_p->SP_SegCnt);

	qprintf("scb_num       = %d\n", scb_p->Sp_scb_num);
	qprintf("Sp_scb_num    = 0x%x\n", scb_p->Sp_scb_num);
	qprintf("Sp_ctrl       = 0x%x\n", scb_p->Sp_ctrl);
	qprintf("SP_ConfigPtr  = 0x%x\n", scb_p->SP_ConfigPtr);
	qprintf("SP_Cmd        = 0x%x\n", scb_p->SP_Cmd);
	qprintf("SP_Stat       = 0x%x\n", scb_p->SP_Stat);

        qprintf("** FLAGS **\n");
	qprintf("NegoInProg    = %x", scb_p->SP_NegoInProg);
	qprintf("  OverlaySns = %x", scb_p->SP_OverlaySns);
	qprintf("  NoNegotiation = %x\n", scb_p->SP_NoNegotiation);
	qprintf("NoUnderrun    = %x", scb_p->SP_NoUnderrun);
	qprintf("  AutoSense = %x\n", scb_p->SP_AutoSense);

	qprintf("RejectMDP     = %x", scb_p->SP_RejectMDP);
	qprintf("  DisEnable = %x", scb_p->SP_DisEnable);
	qprintf("  TagEnable = %x\n", scb_p->SP_TagEnable);
	qprintf("SpecFunc      = %x", scb_p->SP_SpecFunc);
	qprintf("  Discon = %x", scb_p->SP_Discon);
	qprintf("  TagType = %x\n", scb_p->SP_TagType);

	qprintf("Progress      = %x", scb_p->SP_Progress);
	qprintf("  Aborted = %x", scb_p->SP_Aborted);
	qprintf("  Abort = %x\n", scb_p->SP_Abort);
	qprintf("Concurrent    = %x", scb_p->SP_Concurrent);
	qprintf("  Holdoff = %x", scb_p->SP_Holdoff);
        qprintf("\n");

	qprintf("SP_MgrStat    = 0x%x\n", scb_p->SP_MgrStat);
	qprintf("SP_SegCnt     = 0x%x\n", scb_p->SP_SegCnt);
	qprintf("SP_CDBLen     = 0x%x\n", scb_p->SP_CDBLen);
	qprintf("SP_SegPtr     = 0x%x\n", scb_p->SP_SegPtr);
	qprintf("SP_CDBPtr     = 0x%x\n", scb_p->SP_CDBPtr);
	qprintf("SP_TargStat   = 0x%x\n", scb_p->SP_TargStat);
	qprintf("SP_ResCnt     = 0x%x\n", scb_p->SP_ResCnt);
	qprintf("SP_HaStat     = 0x%x\n", scb_p->SP_HaStat);
	qprintf("Sp_SensePtr   = 0x%x\n", scb_p->Sp_SensePtr);
	qprintf("Sp_SenseLen   = 0x%x\n", scb_p->Sp_SenseLen);
	qprintf("Sp_Sensesgp   = 0x%x\n", scb_p->Sp_Sensesgp);
	qprintf("Sp_Sensebuf   = 0x%x\n", scb_p->Sp_Sensebuf);

	qprintf("Sp_CDB        = ");
	for (i=0; i < MAX_CDB_LEN; i++)
	    qprintf("%d ", scb_p->Sp_CDB[i]);
        qprintf("\n");

	qprintf("Sp_ExtMsg     = 0x");
	for (i=0; i < 8; i++)
	  qprintf("%x ", scb_p->Sp_ExtMsg[i]);
        qprintf("\n");

	qprintf("Sp_WideInProg = 0x%x\n", scb_p->Sp_WideInProg);
	qprintf("Sp_dowide     = 0x%x\n", scb_p->Sp_dowide);
	qprintf("Sp_SyncInProg = 0x%x\n", scb_p->Sp_SyncInProg);
	qprintf("ptr to handle = 0x%x\n", scb_p->Sp_OSspecific);

}

void
idbg_adp_task_struct(adp78_ha_t *ha_p)
{

  qprintf("SCSI handle at loc 0x%x\n\n", ha_p);

  qprintf("ha_id          = 0x%1x\n", ha_p->ha_id);
  qprintf("ha_qflag       = 0x%1x\n", ha_p->ha_qflag);
  qprintf("ha_flags       = 0x%x\n", ha_p->ha_flags);
  qprintf("ha_req         = 0x%x\n", ha_p->ha_req);
  qprintf("ha_scb         = 0x%x\n", ha_p->ha_scb);
  qprintf("ha_ebuf        = 0x%x\n", ha_p->ha_ebuf);
  qprintf("ha_sg          = 0x%x\n", ha_p->ha_sg);
  qprintf("ha_numsg       = 0x%x\n", ha_p->ha_numsg);
  qprintf("ha_lrgsg       = 0x%x\n", ha_p->ha_lrgsg);
  qprintf("ha_lrgsg_numsg = 0x%x\n", ha_p->ha_lrgsg_numsg);
  qprintf("ha_start       = 0x%x\n", ha_p->ha_start);
  qprintf("ha_forw        = 0x%x\n", ha_p->ha_forw);
  qprintf("ha_back        = 0x%x\n", ha_p->ha_back);

}

void
idbg_adp_data_struct(int adap)
{

     adp78_adap_t *adp;
     cfp_struct *cfgp;
     hsp_struct *hsp;
     int i;

     adp = adp78_array[adap];
     cfgp = adp->ad_himcfg;
     hsp = cfgp->CFP_HaDataPtr;


     qprintf("Host Adapter %d Data Structure at %x\n", adap, hsp);

     qprintf("scb_ptr_array     = 0x%x\n", hsp->scb_ptr_array);
     qprintf("qout_ptr_array    = 0x%x\n", hsp->qout_ptr_array);
     qprintf("qout_ptr_all      = 0x%x\n", hsp->qout_ptr_all);

     qprintf("free_lo_scb       = 0x%x\n", hsp->free_lo_scb);
     qprintf("qout_index        = 0x%x\n", hsp->qout_index);

     qprintf("Hsp_MaxNonTagScbs = 0x%x\n", hsp->Hsp_MaxNonTagScbs);
     qprintf("Hsp_MaxTagScbs    = 0x%x\n", hsp->Hsp_MaxTagScbs);
     qprintf("done_cmd          = 0x%x\n", hsp->done_cmd);
     qprintf("free_hi_scb       = 0x%x\n", hsp->free_hi_scb);

     qprintf("hsp_active_ptr");
     for (i=0; i < ADP78_NUMSCBS; i++)
       {
	 if (i % 4 == 0)
	   qprintf("\n      ");
	 qprintf("0x%x  ", hsp->hsp_active_ptr[i]);
       }
     qprintf("\n");

     qprintf("hsp_free_stack");
     for (i=0; i < ADP78_NUMSCBS; i++)
       {
	 if (i % 4 == 0)
	   qprintf("\n      ");
	 qprintf("0x%x  ", hsp->hsp_free_stack[i]);
       }
     qprintf("\n");

     qprintf("hsp_done_stack");
     for (i=0; i < ADP78_NUMSCBS; i++)
       {
	 if (i % 4 == 0)
	   qprintf("\n      ");
	 qprintf("0x%x  ", hsp->hsp_done_stack[i]);
       }
     qprintf("\n");
}

void
idbg_adp_cfp_struct(int adap)
{

  adp78_adap_t *adp;
  cfp_struct *cfp_p;
  int i;
  UBYTE byte;

  adp = adp78_array[adap];
  cfp_p = adp->ad_himcfg;

  qprintf("Host Adapter %d Configuration Structure Controller at 0x%x\n", adap, cfp_p);

  qprintf("CFP_AdapterId     = 0x%x\n", cfp_p->CFP_AdapterId);
  qprintf("CFP_BusNumber     = 0x%1x\n", cfp_p->CFP_BusNumber);
  qprintf("CFP_DeviceNumber  = 0x%1x\n", cfp_p->CFP_DeviceNumber);

  qprintf("CFP_base_addr     = 0x%x\n", cfp_p->CFP_base_addr);

  qprintf("CFP_Adapter       = 0x%x\n", cfp_p->CFP_Adapter);

  qprintf("ScsiTimeOut       = 0x%1x\n", cfp_p->CFP_ScsiTimeOut);
  qprintf("ScsiParity        = 0x%1x\n", cfp_p->CFP_ScsiParity);
  qprintf("ResetBus          = 0x%1x\n", cfp_p->CFP_ResetBus);
  qprintf("InitNeeded        = 0x%1x\n", cfp_p->CFP_InitNeeded);
  qprintf("MultiTaskLun      = 0x%1x\n", cfp_p->CFP_MultiTaskLun);
  qprintf("DriverIdle        = 0x%1x\n", cfp_p->CFP_DriverIdle);
  qprintf("SuppressNego      = 0x%1x\n", cfp_p->CFP_SuppressNego);
  qprintf("EnableFast20      = 0x%1x\n", cfp_p->CFP_EnableFast20);

  qprintf("CFP_AccessMode    = 0x%x\n", cfp_p->CFP_AccessMode);
  qprintf("CFP_BusRelease    = 0x%x\n", cfp_p->CFP_BusRelease);
  qprintf("CFP_ScsiId        = 0x%x\n", cfp_p->CFP_ScsiId);
  qprintf("CFP_MaxTargets    = 0x%x\n", cfp_p->CFP_MaxTargets);

  qprintf("CFP_ScsiOption");
  for (i=0; i < 16; i++)
    {
      if (i % 4 == 0)
	qprintf("\n      ");
      qprintf("0x%x  ", cfp_p->Cf_ScsiOption[i].byte);
    }
  qprintf("\n");

  qprintf("AllowDscnt\n    ");
  for (i=0; i < 4; i++)
    {
      byte = (UBYTE) (cfp_p->CFP_AllowDscnt >> (3-i)*8) & 0xff;
      qprintf("0x%x  ", byte);;
    }
  qprintf("\n");

  qprintf("CFP_AllowSyncMask = 0x%x\n", cfp_p->CFP_AllowSyncMask);

  qprintf("CFP_HaDataPtr     = 0x%x\n", cfp_p->CFP_HaDataPtr);
  qprintf("CFP_HaDataPhy     = 0x%x\n", cfp_p->CFP_HaDataPhy);
  qprintf("CFP_MaxNonTagScbs = 0x%x\n", cfp_p->CFP_MaxNonTagScbs);
  qprintf("CFP_MaxTagScbs    = 0x%x\n", cfp_p->CFP_MaxTagScbs);
  qprintf("CFP_NumberScbs    = 0x%x\n", cfp_p->CFP_NumberScbs);

}

void
idbg_adp_adap_rec(int adap)
{

  adp78_adap_t *adp;
  int i, j;

  adp = adp78_array[adap];

  qprintf("Adapter Structure for Controller %d at 0x%x\n", adap, adp);

  qprintf("ad_ctlrid      = 0x%x\n", adp->ad_ctlrid);
  qprintf("ad_scsiid      = 0x%x\n", adp->ad_scsiid);
  qprintf("ad_rev         = 0x%x\n", adp->ad_rev);
  qprintf("ad_pcibusnum   = 0x%x\n", adp->ad_pcibusnum);
  qprintf("ad_pcidevnum   = 0x%x\n", adp->ad_pcidevnum);
  qprintf("ad_vertex      = 0x%x\n", adp->ad_vertex);

  qprintf("ad_waithead    = 0x%x\n", adp->ad_waithead);
  qprintf("ad_waittail    = 0x%x\n", adp->ad_waittail);
  qprintf("ad_freeha      = 0x%x\n", adp->ad_freeha);
  qprintf("ad_busyha      = 0x%x\n", adp->ad_busyha);
  qprintf("ad_doneha      = 0x%x\n", adp->ad_doneha);

  qprintf("ad_himcfg      = 0x%x\n", adp->ad_himcfg);

  qprintf("ad_tagcmds     = ");
  for (i=0; i < ADP78_MAXTARG; i++)
    {
      qprintf("%x  ", adp->ad_tagcmds[i]);
    }
  qprintf("\n");

  qprintf("ad_alloc       = ");
  for (i=0; i < ADP78_MAXTARG; i++)
    {
      qprintf("%x  ", adp->ad_alloc[i][0]);
    }
  qprintf("\n");

  qprintf("devices with callback = \n");
  for (i = 0; i < ADP78_MAXTARG; i++)
    for (j = 0; j < ADP78_MAXLUN; j++)
      {
	if (adp->adp_sense_callback[i][j] != NULL)
	  qprintf("target/lun = %d/%d; call_back_adr = 0x%x\n",
		  i, j, adp->adp_sense_callback[i][j]);
      }

  qprintf("negotiation params =");
  for (i=0; i < ADP78_MAXTARG; i++)
    {
      qprintf("%x  ", adp->ad_negotiate[i].all_params);
    }
  qprintf("\n");

  qprintf("ad_syncperiod  = ");
  for (i=0; i < ADP78_MAXTARG; i++)
    {
      qprintf("%x  ", adp->ad_negotiate[i].params.period);
    }
  qprintf("\n");

  qprintf("ad_syncoffset  = ");
  for (i=0; i < ADP78_MAXTARG; i++)
    {
      qprintf("%x  ", adp->ad_negotiate[i].params.offset);
    }
  qprintf("\n");

  qprintf("ad_width       = ");
  for (i=0; i < ADP78_MAXTARG; i++)
    {
      qprintf("%x  ", adp->ad_negotiate[i].params.width);
    }
  qprintf("\n");

  qprintf("ad_info:");
  for (i=0; i < ADP78_MAXTARG; i++)
    {
      if (i % 4 == 0)
	qprintf("\n      ");
      qprintf("0x%x  ", adp->ad_info[i][0]);
    }
  qprintf("\n");

  qprintf("ad_strayintr   = 0x%x\n", adp->ad_strayintr);

  qprintf("ha queue lock  = 0x%x %x\n", adp->ad_haq_lk, adp->ad_haq_ospl);
  qprintf("req lock       = 0x%x %x\n", adp->ad_req_lk, adp->ad_req_ospl);
  qprintf("adap lock      = 0x%x %x\n", adp->ad_adap_lk, adp->ad_adap_ospl);
  qprintf("him lock       = 0x%x\n", adp->ad_him_lk);
  qprintf("probe lock     = 0x%x\n", adp->ad_probe_lk);
  qprintf("abort lock     = 0x%x\n", adp->ad_abort_lk);

  qprintf("ad_flags       = 0x%x\n", adp->ad_flags);
  qprintf("ad_busymap     = 0x%x\n", adp->ad_busymap);
  qprintf("ad_memwaitmap  = 0x%x\n", adp->ad_memwaitmap);
  qprintf("ad_tagholdmap  = 0x%x\n", adp->ad_tagholdmap);
  qprintf("ad_resettime   = 0x%x\n", adp->ad_resettime);
  qprintf("ad_baseaddr    = 0x%x\n", adp->ad_baseaddr);
  qprintf("ad_cfgaddr     = 0x%x\n", adp->ad_cfgaddr);
  qprintf("ad_numha       = 0x%x\n", adp->ad_numha);
  qprintf("ad_normsg_numsg= 0x%x\n", adp->ad_normsg_numsg);
  qprintf("ad_lrgsg       = 0x%x\n", adp->ad_lrgsg);
  qprintf("ad_lrgsg_numsg = 0x%x\n", adp->ad_lrgsg_numsg);
  qprintf("ad_lrgsg_age   = 0x%x\n", adp->ad_lrgsg_age);
  qprintf("ad_stats       = 0x%x\n", adp->ad_stats);
  qprintf("ad_cmdbuf      = 0x%x\n", adp->ad_cmdbuf);
  qprintf("ad_cmdbufcur   = 0x%x\n", adp->ad_cmdbufcur);
  qprintf("ad_cmdbuflen   = 0x%x\n", adp->ad_cmdbuflen);
}

void
idbg_xfer_option (int adap)
{
  adp78_adap_t *adp;
  u_char targ;
  u_char xfer_option;
  u_char byte;
  AIC_7870 *base;
 
  adp = adp78_array[adap];
  base = adp->ad_himcfg->CFP_Base;

  qprintf("Transfer Option for Controller %d at 0x%x\n", adap, adp);

  for (targ = 0; targ < ADP78_MAXTARG; targ++)
    {
      xfer_option = OSM_CONV_BYTEPTR(XFER_OPTION) + targ;
      xfer_option = OSM_CONV_BYTEPTR(xfer_option);
      byte = INBYTE(AIC7870[xfer_option]);
      qprintf("  %x", byte);
    }
  printf("\n");

}
#endif /* IP32 */
