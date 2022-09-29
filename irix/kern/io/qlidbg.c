#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/systm.h>
#include <sys/PCI/pciio.h>
#undef UNLOCK
#include <sys/ql.h>
#include <sys/PCI/PCI_defs.h>

extern pHA_INFO  ql_ha_list;
extern pHA_INFO  ql_ha_info_from_ctlr_get(vertex_hdl_t);

void	idbg_qlinfo(pHA_INFO);
void    idbg_qlregs(pHA_INFO);
#ifdef PIO_TRACE
void    idbg_qlpio(pHA_INFO, __psint_t);
#endif
void	idbg_qlluninfo(vertex_hdl_t);
void	idbg_qlhelp(void);

#define VD	(void (*)())

struct idbg_if {
	char	*name;
	void   (*func)();
	char    *descrip;   
} qlidbg_funcs[] = {
	"qlinfo",       VD idbg_qlinfo,		"\t\tDump QL host adapter status. Usage: qlinfo [<ha addr>]",
	"qlregs",       VD idbg_qlregs,		"\t\tDump QL host adapter registers. Usage: qlregs [<ha addr>]",
#ifdef PIO_TRACE
	"qlpio",        VD idbg_qlpio,          "\t\tDump QL host adapter PIO trace. Usage: qlpio [<ha addr> [<count>]]",
#endif
	"qlluninfo",	VD idbg_qlluninfo,	"\t\tDump QL LUN information. Usage: qlluninfo <lun vhdl>",
	"qlhelp",	VD idbg_qlhelp,		"\t\tPrint help about QL idbg functions",
	0,		0,	                0
};

int qlpio_count = 10;

void 
qlidbg_init(void)
{
	struct idbg_if *p;

	for (p = qlidbg_funcs; p->name; p++)
		idbg_addfunc(p->name, p->func);
}

/*
   ++ kp qlhelp - print descriptions of QL kp funcions.
*/
void 
idbg_qlhelp(void)
{
	struct idbg_if *p;

        for (p = qlidbg_funcs; p->name; p++) {
                  qprintf("%s:%s\n", p->name, p->descrip);
	}
}

/*
   ++ kp qlinfo - print data structures for QL controller. If no
   ++ argument is supplied, print a short list of all controllers.  
*/
void
idbg_qlinfo(pHA_INFO ha)
{
  char path_name[MAXDEVNAME];
  int  rc;
  int  i, j;

  if (ha == (pHA_INFO)(-1LL)) {
    for (ha = ql_ha_list; ha; ha = ha->next_ha) {
      rc = hwgraph_vertex_name_get(ha->ctlr_vhdl, path_name, MAXDEVNAME);
      if (rc != GRAPH_SUCCESS)
	sprintf(path_name, "Invalid ctlr_vhdl %d\n", ha->ctlr_vhdl);
      qprintf("0x%x: %s\n", ha, path_name);
    }
  }
  else {
    qprintf("ctlr_vhdl: %d, pci_vhdl: %d, bridge revnum: %d, ha_flags: 0x%x chip_flags: 0x%x chip_info: 0x%x\n", 
	    ha->ctlr_vhdl, ha->chip_info->pci_vhdl, ha->chip_info->bridge_revnum, ha->host_flags, ha->chip_info->chip_flags, ha->chip_info);

   qprintf("chip_info: 0x%x, device_id: 0x%x, channel_count: %d \n",
		ha->chip_info, ha->chip_info->device_id, ha->chip_info->channel_count); 
#ifdef PIO_TRACE
    qprintf("ha_base: 0x%x, ha_config_base: 0x%x, ql_log_vals: 0x%x\n", 
	    ha->chip_info->ha_base, ha->chip_info->ha_config_base, ha->chip_info->ql_log_vals);
#else
    qprintf("ha_base: 0x%x, ha_config_base: 0x%x\n", 
	    ha->chip_info->ha_base, ha->chip_info->ha_config_base);
#endif
    qprintf("request_in: %d, request_out: %d, response_out: %d, queue_space: %d\n",
	    ha->chip_info->request_in, ha->chip_info->request_out, ha->chip_info->response_out, ha->chip_info->queue_space);
    qprintf("request_ptr: 0x%x, request_base: 0x%x\n",
	    ha->chip_info->request_ptr, ha->chip_info->request_base);
    qprintf("response_ptr: 0x%x, response_base: 0x%x\n",
	    ha->chip_info->response_ptr, ha->chip_info->response_base);
    qprintf("req_forw: 0x%x, req_back: 0x%x\n", 
	    ha->req_forw, ha->req_back);
    qprintf("waitf: 0x%x, waitb: 0x%x\n", 
	    ha->waitf, ha->waitb);
    qprintf("reqi_block: 0x%x\n", ha->reqi_block);
    qprintf("res_lock 0x%x, req_lock 0x%x, mbox_lock 0x%x, waitQ_lock 0x%x, probe_lock 0x%x\n", 
	    &ha->chip_info->res_lock, &ha->chip_info->req_lock, &ha->chip_info->mbox_lock, &ha->waitQ_lock, &ha->chip_info->probe_lock);
    qprintf("mbox_done_sema 0x%x\n", &ha->chip_info->mbox_done_sema);
    qprintf("Outstanding SCSI commands: ql_ncmd: %d, ha_last_intr: %d, lbolt: %d\n", 
	    ha->ql_ncmd, ha->chip_info->ha_last_intr, lbolt/HZ);
    for (i = 0; i < QL_MAXTARG; ++i) {
      qprintf("targ: %d: tlock 0x%x, ql_tcmd: %d, ql_dups: %d, current_timeout: %d, time_base: %d, t_last_intr: %d\n", 
	      i, 
	      &ha->timeout_info[i].per_tgt_tlock,
	      ha->timeout_info[i].ql_tcmd,
	      ha->timeout_info[i].ql_dups,
	      ha->timeout_info[i].current_timeout,
	      ha->timeout_info[i].time_base,
	      ha->timeout_info[i].t_last_intr);
      for (j = 0; j < MAX_REQ_INFO + 1; ++j) {
	if (ha->reqi_block[i][j].req) {
	  qprintf("\t(%d)req: 0x%x, timeout: %d, cmd_age: %d\n", 
		  j, 
		  ha->reqi_block[i][j].req,
		  ha->reqi_block[i][j].timeout,
		  ha->reqi_block[i][j].cmd_age);
	}
      }
    }
  }
}

/*
   ++ kp qlregs - print internal registers for QL controller. If no
   ++ argument is supplied, print a short list of all controllers.  
*/
void
idbg_qlregs(pHA_INFO ha)
{
  char path_name[MAXDEVNAME];
  int  rc;

  if (ha == (pHA_INFO)(-1LL)) {
    for (ha = ql_ha_list; ha; ha = ha->next_ha) {
      rc = hwgraph_vertex_name_get(ha->ctlr_vhdl, path_name, MAXDEVNAME);
      if (rc != GRAPH_SUCCESS)
	sprintf(path_name, "Invalid ctlr_vhdl %d\n", ha->ctlr_vhdl);
      qprintf("0x%x: %s\n", ha, path_name);
    }
  }
  else {
    pISP_REGS isp = (pISP_REGS)ha->chip_info->ha_base;
    u_short bus_id_low, bus_id_high, bus_config0, bus_config1;
    u_short bus_icr = 0;
    u_short bus_isr = 0;
    u_short bus_sema;
    u_short hccr = 0;
    u_short risc_pc;
    u_short mbox0, mbox1, mbox2, mbox3, mbox4, mbox5;

    u_short control_dma_control;
    u_short control_dma_config;
    u_short control_dma_fifo_status;
    u_short control_dma_status;
    u_short control_dma_counter;
    u_short control_dma_address_counter_1;
    u_short control_dma_address_counter_0;
    u_short control_dma_address_counter_3;
    u_short control_dma_address_counter_2;

    u_short data_dma_control;
    u_short data_dma_config;
    u_short data_dma_fifo_status;
    u_short data_dma_status;
    u_short data_dma_xfer_counter_high;
    u_short data_dma_xfer_counter_low;
    u_short data_dma_address_counter_1;
    u_short data_dma_address_counter_0;
    u_short data_dma_address_counter_3;
    u_short data_dma_address_counter_2;

    /* Get copies of the registers.  */
    mbox0 = PCI_INH(&isp->mailbox0);
    mbox1 = PCI_INH(&isp->mailbox1);
    mbox2 = PCI_INH(&isp->mailbox2);
    mbox3 = PCI_INH(&isp->mailbox3);
    mbox4 = PCI_INH(&isp->mailbox4);
    mbox5 = PCI_INH(&isp->mailbox5);

    bus_id_low = PCI_INH(&isp->bus_id_low);
    bus_id_high = PCI_INH(&isp->bus_id_high);
    bus_config0 = PCI_INH(&isp->bus_config0);
    bus_config1 = PCI_INH(&isp->bus_config1);
    bus_icr = PCI_INH(&isp->bus_icr);
    bus_isr = PCI_INH(&isp->bus_isr);
    bus_sema = PCI_INH(&isp->bus_sema);
    hccr = PCI_INH(&isp->hccr);
    PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);
    control_dma_control = PCI_INH(&isp->control_dma_control);
    control_dma_config = PCI_INH(&isp->control_dma_config);
    control_dma_fifo_status = PCI_INH(&isp->control_dma_fifo_status);
    control_dma_status = PCI_INH(&isp->control_dma_status);
    control_dma_counter = PCI_INH(&isp->control_dma_counter);
    control_dma_address_counter_1 = PCI_INH(&isp->control_dma_address_counter_1);
    control_dma_address_counter_0 = PCI_INH(&isp->control_dma_address_counter_0);
    control_dma_address_counter_3 = PCI_INH(&isp->control_dma_address_counter_3);
    control_dma_address_counter_2 = PCI_INH(&isp->control_dma_address_counter_2);

    data_dma_config = PCI_INH(&isp->data_dma_config);
    data_dma_fifo_status = PCI_INH(&isp->data_dma_fifo_status);
    data_dma_status = PCI_INH(&isp->data_dma_status);
    data_dma_control = PCI_INH(&isp->data_dma_control);
    data_dma_xfer_counter_high = PCI_INH(&isp->data_dma_xfer_counter_high);
    data_dma_xfer_counter_low = PCI_INH(&isp->data_dma_xfer_counter_low);
    data_dma_address_counter_1 = PCI_INH(&isp->data_dma_address_counter_1);
    data_dma_address_counter_0 = PCI_INH(&isp->data_dma_address_counter_0);
    data_dma_address_counter_3 = PCI_INH(&isp->data_dma_address_counter_3);
    data_dma_address_counter_2 = PCI_INH(&isp->data_dma_address_counter_1);

    risc_pc = PCI_INH(&isp->risc_pc);

    PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);


    qprintf("mbox0 = %x, mbox1 = %x\n", mbox0, mbox1);
    qprintf("mbox2 = %x, mbox3 = %x\n", mbox2, mbox3);
    qprintf("mbox4 = %x, mbox5 = %x\n", mbox4, mbox5);
    qprintf("BUS_ID_LOW=%x, BUS_ID_HIGH=%x\n",
	    bus_id_low, bus_id_high);
    qprintf("BUS_CONFIG0=%x, BUS_CONFIG1=%x\n",
	    bus_config0, bus_config1);
    qprintf("BUS_ICR=%x, BUS_ISR=%x, BUS_SEMA=%x\n",
	    bus_icr, bus_isr, bus_sema);
    qprintf("HCCR=%x: %x  \n", &isp->hccr, hccr);
    qprintf("risc PC : %x\n", risc_pc);
    qprintf("control_dma_control %x: %x\n",
	    &isp->control_dma_control, control_dma_control);
    qprintf("control_dma_config %x: %x\n",
	    &isp->control_dma_config, control_dma_config);
    qprintf("control_dma_fifo_status %x: %x\n",
	    &isp->control_dma_fifo_status, control_dma_fifo_status);
    qprintf("control_dma_status %x: %x\n",
	    &isp->control_dma_status, control_dma_status);
    qprintf("control_dma_counter %x: %x\n",
	    &isp->control_dma_counter, control_dma_counter);
    qprintf("dump_regs: control_dma_address_counter_1 %x: %x\n",
	    &isp->control_dma_address_counter_1, control_dma_address_counter_1);
    qprintf("dump_regs: control_dma_address_counter_0 %x: %x\n",
	    &isp->control_dma_address_counter_0, control_dma_address_counter_0);
    qprintf("dump_regs: control_dma_address_counter_3 %x: %x\n",
	    &isp->control_dma_address_counter_3, control_dma_address_counter_3);
    qprintf("dump_regs: control_dma_address_counter_0 %x: %x\n",
	    &isp->control_dma_address_counter_2, control_dma_address_counter_2);
    qprintf("data_dma_config %x: %x\n",
	    &isp->data_dma_config, data_dma_config);

    qprintf("data_dma_status %x: %x\n",
	    &isp->data_dma_status, data_dma_status);
    qprintf("data_dma_fifo_status %x: %x\n",
	    &isp->data_dma_fifo_status, data_dma_fifo_status);
    qprintf("data_dma_control %x: %x\n",
	    &isp->data_dma_control, data_dma_control);
    qprintf("data_dma_xfer_counter_high %x: %x\n",
	    &isp->data_dma_xfer_counter_high, data_dma_xfer_counter_high);
    qprintf("data_dma_xfer_counter_low %x: %x\n",
	    &isp->data_dma_xfer_counter_low, data_dma_xfer_counter_low);
    qprintf("dump_regs: data_dma_address_counter_1 %x: %x\n",
	    &isp->data_dma_address_counter_1,
	    data_dma_address_counter_1);
    qprintf("dump_regs: data_dma_address_counter_0 %x: %x\n",
	    &isp->data_dma_address_counter_0,
	    data_dma_address_counter_0);
    qprintf("dump_regs: data_dma_address_counter_3 %x: %x\n",
	    &isp->data_dma_address_counter_3,
	    data_dma_address_counter_3);
    qprintf("dump_regs: data_dma_address_counter_2 %x: %x\n",
	    &isp->data_dma_address_counter_2,
	    data_dma_address_counter_2);
  }
}

#ifdef PIO_TRACE
/*
   ++ kp qlpio - dump PIO trace, if it exists
   ++ argument is supplied, print a short list of all controllers.  
*/
void
idbg_qlpio(pHA_INFO ha, __psint_t count)
{
	char path_name[MAXDEVNAME];
	int  rc;

	if (ha == (pHA_INFO)(-1LL)) {
		for (ha = ql_ha_list; ha; ha = ha->next_ha) {
			rc = hwgraph_vertex_name_get(ha->ctlr_vhdl, path_name, MAXDEVNAME);
			if (rc != GRAPH_SUCCESS)
				sprintf(path_name, "Invalid ctlr_vhdl %d\n", ha->ctlr_vhdl);
			qprintf("0x%x: %s\n", ha, path_name);
		}
	}
	else {
		uint	last_index = ha->ql_log_vals[0];
		uint	i, j = 0;
		ushort	dir, reg, val;
		char	regname[10];
		
		qprintf("ha = 0x%x, count = 0x%x\n", ha, count);
#if 1
		count = qlpio_count;
#endif
		if ((count > QL_LOG_CNT) || (count == 0))
			count = QL_LOG_CNT;
		i = (last_index - 1 - count + QL_LOG_CNT - 1) % (QL_LOG_CNT - 1) + 1;
		for (j = 0; j < count; ++j) {
			dir = (ha->ql_log_vals[i] & 0x80000000) >> 31;
			reg = (ha->ql_log_vals[i] & 0x7FFF0000) >> 16;
			val = ha->ql_log_vals[i] & 0x0000FFFF;
			switch(reg) {
			case 0x0070: sprintf(regname, "MAILBOX1"); break;
			case 0x0072: sprintf(regname, "MAILBOX0"); break;
			case 0x0074: sprintf(regname, "MAILBOX3"); break;
			case 0x0076: sprintf(regname, "MAILBOX2"); break;
			case 0x0078: sprintf(regname, "MAILBOX5"); break;
			case 0x007A: sprintf(regname, "MAILBOX4"); break;
			case 0x007C: sprintf(regname, "MAILBOX7"); break;
			case 0x007E: sprintf(regname, "MAILBOX6"); break;

			case 0x0008: sprintf(regname, "ISR"); break;
			case 0x000A: sprintf(regname, "ICR"); break;
			case 0x000E: sprintf(regname, "BUSSEMA"); break;
			case 0x00C2: sprintf(regname, "HCCR"); break;
			default: sprintf(regname, "0x%x", reg);
			}
			qprintf("[%4d] %s 0x%4x (%9s): 0x%4x\n", j - count, (dir ? "W":"R"), reg, regname, val);
			if (i++ == QL_LOG_CNT)
				i = 1;
		}
	}
}
#endif

/*
   ++ kp qlluninfo - print QL lun information. Argument is LUN VHDL.
*/
void 
idbg_qlluninfo(vertex_hdl_t lun_vhdl)
{
  ql_local_info_t *qli = SLI_INFO(scsi_lun_info_get(lun_vhdl));
  pHA_INFO         ha = ql_ha_info_from_ctlr_get(SLI_CTLR_VHDL(scsi_lun_info_get(lun_vhdl)));
  char             path_name[MAXDEVNAME];
  int              rc;
  scsi_request_t  *req;

  rc = hwgraph_vertex_name_get(lun_vhdl, path_name, MAXDEVNAME);
  qprintf("ha 0x%x, lun_vhdl: %d, lun_name %s\n",
	  ha, lun_vhdl, (rc == GRAPH_SUCCESS ? path_name : "Invalid vhdl"));
  qprintf("open_mutex: 0x%x, lun_mutex: 0x%x\n",
	  &qli->qli_open_mutex, &qli->qli_lun_mutex);
  qprintf("dev_flags: 0x%x\n", qli->qli_dev_flags);
  qprintf("ref_count: %d, cmd_rcnt: %d, cmd_iwcnt: %d, cmd_awcnt: %d\n",
	  qli->qli_ref_count, qli->qli_cmd_rcnt, qli->qli_cmd_iwcnt, qli->qli_cmd_awcnt);
  qprintf("Abort waiting requests: ");
  for (req = qli->qli_awaitf; req; req = req->sr_ha)
    qprintf("0x%x ", req);
  qprintf("\n");
  qprintf("Init waiting requests: ");
  for (req = qli->qli_iwaitf; req; req = req->sr_ha)
    qprintf("0x%x ", req);
  qprintf("\n");
}
