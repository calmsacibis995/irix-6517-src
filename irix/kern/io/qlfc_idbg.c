#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/systm.h>
#include <sys/PCI/pciio.h>
#undef UNLOCK

#define QLFC_IDBG
#include <sys/qlfc.h>
#include <sys/PCI/PCI_defs.h>

QLFC_CONTROLLER *qlfc_ctlrs=0;	/* strong definition for the weak one below */

#pragma weak qlfc_controllers = qlfc_ctlrs

extern uint16_t qLogicDeviceIds[];
extern QLFC_CONTROLLER *  qlfc_controllers;
extern QLFC_CONTROLLER *  qlfc_ctlr_info_from_ctlr_get(vertex_hdl_t);
#pragma weak qLogicDeviceIds
#pragma weak qlfc_ctlr_info_from_ctlr_get

void	idbg_qlfc_info(int);
void    idbg_qlfc_regs(int);
void	idbg_qlfc_help(void);
void	idbg_qlfc_nvram (int);
void	idbg_qlfc_trace (int);
void	idbg_qlfc_ctrace (int);
void	idbg_qlfc_trace_reset (int);
void	idbg_qlfc_stat_ent (int);
void	idbg_qlfc_ctlrs (void);

#define VD	(void (*)())

struct idbg_if {
	char	*name;
	void   (*func)();
	char    *descrip;   
} qlifc_dbg_funcs[] = {
	"qlfc_info",	VD idbg_qlfc_info,		"\t\tDump QL everything known about host adapter. "
								"Usage: qlfc_info [<ha NUMBER>]",

	"qlfc_regs",	VD idbg_qlfc_regs,		"\t\tDump QL host adapter registers. STOPS! RISC! "
								"Usage: qlfc_regs [<ha NUMBER>]",

	"qlfc_trace",	VD idbg_qlfc_trace,		"\t\tDump specified QLFC trace buffer. "
								"Usage: qlfc_trace <ha NUMBER>",

	"qlfc_ctrace",	VD idbg_qlfc_ctrace,		"\t\tDump specified QLFC controller trace buffer. "
								"Usage: qlfc_ctrace <ha NUMBER>",

	"qlfc_ctlrs",	VD idbg_qlfc_ctlrs,		"\t\tList all qLogic FC controllers in system.",

	"qlfc_nvram",	VD idbg_qlfc_nvram,		"\t\tDisplay the nvram data for the controller.",

	"qlfc_t_rst",	VD idbg_qlfc_trace_reset,	"\t\tReset trace buffer pointer for specified controller."
								"Usage: qlfc_trace_reset <ha NUMBER>",

	"qlfc_stent",	VD idbg_qlfc_stat_ent,		"\t\tDump the status entries, last to first.  "
								"Usage: qlfc_stent <ha NUMBER>",

	"qlfc_help",	VD idbg_qlfc_help,		"\t\tPrint help about QL idbg functions",

	0,		0,	                	0
};

void
idbg_qlfc_stat_ent (int number)
{
	
	status_entry	*qptr;
	QLFC_CONTROLLER *ctlr;
	status_entry	*response_ptr;
	REQ_COOKIE	cookie;
	int		i;

	ctlr = qlfc_controllers;
	while (ctlr) {
		if (ctlr->ctlr_number == number) break;
		ctlr = ctlr->next;
	}

	if (ctlr) {
		response_ptr = (status_entry *)ctlr->response_base;
		for (i=0; i<ctlr->ql_response_queue_depth; i++) {

			qptr = response_ptr + i;

			/*  format qptr entry */

			qprintf ("stat_ent[%d]: hdr: flags 0x%x, sys_def_1 0x%x, entry_cnt 0x%x, entry_type 0x%x\n",
				i,
				qptr->hdr.flags,
				qptr->hdr.sys_def_1,
				qptr->hdr.entry_cnt,
				qptr->hdr.entry_type);

			qprintf ("stat_ent[%d]: handle 0x%x, status_flags 0x%x, state_flags 0x%x, comp_status 0x%x\n",
				i, qptr->handle, qptr->status_flags, qptr->state_flags, qptr->completion_status);

			cookie.data = qptr->handle;
			if (cookie.data != 0xeeeeeeee) {
				qprintf ("stat_ent[%d]: handle: cookie.index(-1) %d, cookie.magic 0x%x, cookie.unique %d, cookie.value %d\n",
					i, cookie.field.index-MIN_COOKIE_INDEX, cookie.field.magic, cookie.field.unique, cookie.field.value);
			}

			qprintf ("stat_ent[%d]: fc_scsi_status 0x%x, residual %d, req_sense_length %d, resp_info_length %d\n",
				i, qptr->fc_scsi_status, qptr->residual, qptr->req_sense_length, qptr->resp_info_length);

			qprintf ("stat_ent[%d]: fcp_response_info 0x%x\n",
				i, *(uint64_t *)(&qptr->fcp_response_info[0]));

			qprintf ("stat_ent[%d]: req_sense_data 0x%x 0x%x 0x%x 0x%x\n",
				i,
				*(uint64_t *)(&qptr->req_sense_data[0]),
				*(uint64_t *)(&qptr->req_sense_data[8]),
				*(uint64_t *)(&qptr->req_sense_data[16]),
				*(uint64_t *)(&qptr->req_sense_data[24]));
		}
	}
}


void
idbg_qlfc_nvram (int number)
{
	QLFC_CONTROLLER *ctlr;
	ctlr = qlfc_controllers;
	while (ctlr) {
		if (ctlr->ctlr_number == number) break;
		ctlr = ctlr->next;
	}

	if (ctlr) {
		qprintf ("id:                %c%c%c%c \n",
			ctlr->nvram_buf.nvram_data.id[0],
			ctlr->nvram_buf.nvram_data.id[1],
			ctlr->nvram_buf.nvram_data.id[2],
			ctlr->nvram_buf.nvram_data.id[3]
			);

		qprintf ("version            0x%x\n", ctlr->nvram_buf.nvram_data.version);
		qprintf ("maxFrameSize       0x%x\n", ctlr->nvram_buf.nvram_data.fw_params.maxFrameSize);
		qprintf ("maxIOCBperPort     0x%x\n", ctlr->nvram_buf.nvram_data.fw_params.maxIOCBperPort);
		qprintf ("executionThrottle  0x%x\n", ctlr->nvram_buf.nvram_data.fw_params.executionThrottle);
		qprintf ("retryCount         0x%x\n", ctlr->nvram_buf.nvram_data.fw_params.retryCount);
		qprintf ("retryDelay         0x%x\n", ctlr->nvram_buf.nvram_data.fw_params.retryDelay);
		qprintf ("portName           0x%x %x %x %x %x %x %x %x\n", 
			ctlr->nvram_buf.nvram_data.fw_params.portName[0],
			ctlr->nvram_buf.nvram_data.fw_params.portName[1],
			ctlr->nvram_buf.nvram_data.fw_params.portName[2],
			ctlr->nvram_buf.nvram_data.fw_params.portName[3],
			ctlr->nvram_buf.nvram_data.fw_params.portName[4],
			ctlr->nvram_buf.nvram_data.fw_params.portName[5],
			ctlr->nvram_buf.nvram_data.fw_params.portName[6],
			ctlr->nvram_buf.nvram_data.fw_params.portName[7]
			);

		qprintf ("inquiry_data       0x%x\n", ctlr->nvram_buf.nvram_data.fw_params.inquiry_data);
		qprintf ("loginTimeout       0x%x\n", ctlr->nvram_buf.nvram_data.fw_params.loginTimeout);
		qprintf ("nodeName           0x%x %x %x %x %x %x %x %x\n", 
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[0],
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[1],
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[2],
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[3],
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[4],
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[5],
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[6],
			ctlr->nvram_buf.nvram_data.fw_params.nodeName[7]
			);



	}
}

void
idbg_qlfc_trace_reset (int number)
{
	QLFC_CONTROLLER *ctlr;

	ctlr = qlfc_controllers;
	while (ctlr) {
		if (ctlr->ctlr_number == number) break;
		ctlr = ctlr->next;
	}

	if (ctlr) {
		ctlr->trace_index = 0;
		ctlr->first_trace_entry->format = NULL;
		ctlr->last_trace_entry->format = NULL;
	}
}

void
idbg_qlfc_ctlrs ()
{
	QLFC_CONTROLLER *ctlr;

	for (ctlr = qlfc_controllers; ctlr != NULL ; ctlr = ctlr->next) {

		qprintf ("ctlr 0x%x: type %x, number %d, which %d\n",
			ctlr, qLogicDeviceIds[ctlr->ctlr_type_index], ctlr->ctlr_number, ctlr->ctlr_which);
	}
}

void
idbg_qlfc_ctrace (int number)
{
	QLFC_CONTROLLER *ctlr;
	dump_entry_t *start, *entry;
	int i;

	ctlr = qlfc_controllers;
	while (ctlr) {
		if (ctlr->ctlr_number == number) break;
		ctlr = ctlr->next;
	}

	qprintf ("idbg_qlfc_ctrace: enter, ctlr[%d]=0x%x\n",number,ctlr);

	if (ctlr && ctlr->dump_buffer) {
		start = &ctlr->dump_buffer[ctlr->dump_index];
		if (start == ctlr->dump_buffer) entry = ctlr->dump_last_entry;
		else entry = start-1;

		for (entry=&ctlr->dump_buffer[0],i=0;i<ctlr->dump_index;i++,entry++) {
			if (!entry->format) break;
			qprintf (entry->format,
				entry->arg0,
				entry->arg1,
				entry->arg2,
				entry->arg3,
				entry->arg4,
				entry->arg5,
				entry->arg6,
				entry->arg7,
				entry->arg8
				);
		}
	}
	else {
		qprintf ("trace entry for controller number %d not defined.\n",number);
	}
}

void
idbg_qlfc_trace (int number)
{
	QLFC_CONTROLLER *ctlr;
	trace_entry_t *start, *entry;
	int second, usec;
	char fmt[256];

	ctlr = qlfc_controllers;
	while (ctlr) {
		if (ctlr->ctlr_number == number) break;
		ctlr = ctlr->next;
	}

	qprintf ("idbg_qlfc_trace: enter, ctlr[%d]=0x%x\n",number,ctlr);

	if (ctlr && ctlr->first_trace_entry) {
		start = &ctlr->first_trace_entry[ctlr->trace_index];
		if (start == ctlr->first_trace_entry) entry = ctlr->last_trace_entry;
		else entry = start-1;

		while (entry != start) {
			if (!entry->format) break;
			second = entry->timestamp.tv_sec;
			usec = entry->timestamp.tv_nsec / 1000;

			if (usec < 10)
				qprintf ("%d.00000%d: ",second,usec);
			else if (usec < 100)
				qprintf ("%d.0000%d: ", second,usec);
			else if (usec < 1000)
				qprintf ("%d.000%d: ",  second,usec);
			else if (usec < 10000)
				qprintf ("%d.00%d: ",   second,usec);
			else if (usec < 100000)
				qprintf ("%d.0%d: ",    second,usec);
			else
				qprintf ("%d.%d: ",     second,usec);
			
			strcpy (fmt, "ctlr %d (%x): ");
			strcat (fmt, entry->format);

			qprintf (fmt,ctlr->ctlr_number,qLogicDeviceIds[ctlr->ctlr_type_index],
				entry->arg0,entry->arg1,entry->arg2,entry->arg3,entry->arg4,entry->arg5);

			if (entry == ctlr->first_trace_entry) entry = ctlr->last_trace_entry;
			else entry--;
		}
	}
	else {
		qprintf ("trace entry for controller number %d not defined.\n",number);
	}
}


void 
qlfc_idbg_init(void)
{
	struct idbg_if *p;

	for (p = qlifc_dbg_funcs; p->name; p++)
		idbg_addfunc(p->name, p->func);
}

/*
   ++ kp qlfc_help - print descriptions of QL kp funcions.
*/
void 
idbg_qlfc_help(void)
{
	struct idbg_if *p;

        for (p = qlifc_dbg_funcs; p->name; p++) {
                  qprintf("%s:%s\n", p->name, p->descrip);
	}

	idbg_qlfc_ctlrs ();
}

void
idbg_qlfc_dump_scsi_chain (char *prefix, scsi_request_t *sr)
{
	SR_SPARE	*spare = (SR_SPARE *)&sr->sr_spare;
	int		req=0;

	while (sr) {
		qprintf ("%s [%d] sr_ctlr %d, sr_target %d, sr_lun %d, sr_tag 0x%x\n",
			prefix, req, sr->sr_ctlr, sr->sr_target, sr->sr_lun, sr->sr_tag);

		qprintf ("%s [%d] sr_lun_vhdl %d, sr_command 0x%x, sr_cmdlen %d, sr_flags 0x%x\n",
			prefix, req, sr->sr_lun_vhdl, sr->sr_command, sr->sr_cmdlen, sr->sr_flags);

		qprintf ("%s [%d] sr_timeout %d, sr_buffer 0x%x, sr_sense 0x%x, sr_buflen %d, sr_senselen %d\n",
			prefix, req, sr->sr_timeout, sr->sr_buffer, sr->sr_sense, sr->sr_buflen, sr->sr_senselen);

		qprintf ("%s [%d] sr_notify 0x%x, sr_bp 0x%x, sr_dev_vhdl %d, sr_dev 0x%x\n",
			prefix, req, sr->sr_notify, sr->sr_bp, sr->sr_dev_vhdl, sr->sr_dev);

		qprintf ("%s [%d] sr_ha 0x%x, sr_spare 0x%x, sr_status 0x%x, sr_scsi_status 0x%x\n",
			prefix, req, sr->sr_ha, sr->sr_spare, sr->sr_status, sr->sr_scsi_status);

		qprintf ("%s [%d] sr_ha_flags 0x%x, sr_sensegotten %d, sr_resid %d\n",
			prefix, req, sr->sr_ha_flags, sr->sr_sensegotten, sr->sr_resid);

		qprintf ("%s [%d] spare.field.timeout %d, spare.field.cookie.field.tm_index %d, "
			 "spare.field.cookie.field.magic 0x%x, spare.field.cookie.field.unique %d "
			 "spare.field.cookie.field.value 0x%x\n",
			prefix, req, spare->field.timeout, spare->field.cookie.field.index, 
			spare->field.cookie.field.magic, spare->field.cookie.field.unique,
			spare->field.cookie.field.value);

		sr = sr->sr_ha;
		req++;
	}
}

void
idbg_qlfc_info (int number)
{
	int i;
	target_map_t		*tm;
	qlfc_local_targ_info_t	*qti;
	qlfc_local_lun_info_t	*qli;
	scsi_target_info_t	*sti=NULL;
	nvram_risc_param	*fwp;
	uint64_t		*port_name;
	char			path_name[MAXDEVNAME];
	int			rc;

	QLFC_CONTROLLER *ctlr;

	ctlr = qlfc_controllers;
	while (ctlr) {
		if (ctlr->ctlr_number == number) break;
		ctlr = ctlr->next;
	}

	if (!ctlr) {
		for (ctlr = qlfc_controllers; ctlr; ctlr = ctlr->next) {
			rc = hwgraph_vertex_name_get(ctlr->ctlr_vhdl, path_name, MAXDEVNAME);
			if (rc != GRAPH_SUCCESS) {
				sprintf(path_name, "Invalid ctlr_vhdl %d\n", ctlr->ctlr_vhdl);
			}
			qprintf ("ctlr 0x%x: type %x, number %d, which %d, %s\n",
				ctlr, qLogicDeviceIds[ctlr->ctlr_type_index], ctlr->ctlr_number, ctlr->ctlr_which, path_name);
		}
		return;
	}


	qprintf	("DUMPING EVERYTHING KNOWN ABOUT THIS CONTROLLER.  YOU ASKED FOR IT!\n\n");

	qprintf ("next: 0x%x, ctlr_type_index %d, ctlr_number %d, ctlr_which %d\n",
		ctlr->next, ctlr->ctlr_type_index, ctlr->ctlr_number, ctlr->ctlr_which);

	qprintf ("base_address 0x%x, ctlr_vhdl %d, pci_vhdl %d\n",
		ctlr->base_address, ctlr->ctlr_vhdl, ctlr->pci_vhdl);

	qprintf ("quiesce_in_progress_id 0x%x, quiesce_id 0x%x, quiesce_time %d\n",
		ctlr->quiesce_in_progress_id, ctlr->quiesce_id, ctlr->quiesce_time);

#if defined(SN)||defined(IP30)
	qprintf ("flags 0x%x, revision %d, bridge_revnum %d\n", 
		ctlr->flags, ctlr->revision, ctlr->bridge_revnum);
#endif

	qprintf ("request_in 0x%x, request_out 0x%x, response_in 0x%x, response_out 0x%x, queue_space %d\n",
		ctlr->request_in, ctlr->request_out, ctlr->response_in, ctlr->response_out, ctlr->queue_space);

	qprintf ("request_base 0x%x, response_base 0x%x, request_ptr 0x%x, response_ptr 0x%x\n",
		ctlr->request_base, ctlr->response_base, ctlr->request_ptr, ctlr->response_ptr);

	qprintf ("request_misc 0x%x, response_misc 0x%x, request_size %d, response_size %d\n",
		ctlr->request_misc, ctlr->response_misc, ctlr->request_size, ctlr->response_size);

	qprintf ("request_dmaptr 0x%x, response_dmaptr 0x%x, request_misc_dmaptr 0x%x, response_misc_dmaptr 0x%x\n",
		ctlr->request_dmaptr, ctlr->response_dmaptr, ctlr->request_misc_dmaptr, ctlr->response_misc_dmaptr);

	qprintf ("ql_request_queue_depth %d, ql_response_queue_depth %d\n",
		ctlr->ql_request_queue_depth, ctlr->ql_response_queue_depth);

	qprintf ("alen_p 0x%x\n",ctlr->alen_p);

	qprintf ("trace_index %d, first_trace_entry 0x%x, last_trace_entry 0x%x, trace_enabled %d, trace_code %d\n",
		ctlr->trace_index, ctlr->first_trace_entry, ctlr->last_trace_entry, ctlr->trace_enabled, ctlr->trace_code);

	qprintf ("&res_lock 0x%x, res_lock.m_bits 0x%x, res_lock.m_queue 0x%x\n",
		&ctlr->res_lock, ctlr->res_lock.m_bits, ctlr->res_lock.m_queue);

	qprintf ("&req_lock 0x%x, req_lock.m_bits 0x%x, req_lock.m_queue 0x%x\n",
		&ctlr->req_lock, ctlr->req_lock.m_bits, ctlr->req_lock.m_queue);

	qprintf ("&mbox_lock 0x%x, mbox_lock.m_bits 0x%x, mbox_lock.m_queue 0x%x\n",
		&ctlr->mbox_lock, ctlr->mbox_lock.m_bits, ctlr->mbox_lock.m_queue);

	qprintf ("&waitQ_lock 0x%x, waitQ_lock.m_bits 0x%x, waitQ_lock.m_queue 0x%x\n",
		&ctlr->waitQ_lock, ctlr->waitQ_lock.m_bits, ctlr->waitQ_lock.m_queue);

	qprintf ("&probe_lock 0x%x, probe_lock.m_bits 0x%x, probe_lock.m_queue 0x%x\n",
		&ctlr->probe_lock, ctlr->probe_lock.m_bits, ctlr->probe_lock.m_queue);

	qprintf ("&mbox_done_sema 0x%x, mbox_done_sema.s_queue 0x%x, mbox_done_sema.s_un.s_st.count %d, mbox_done_sema.s_un.s_st.flags 0x%x\n",
		&ctlr->mbox_done_sema, ctlr->mbox_done_sema.s_queue, ctlr->mbox_done_sema.s_un.s_st.count, ctlr->mbox_done_sema.s_un.s_st.flags);

	qprintf ("&demon_lock 0x%x, demon_lock.m_bits 0x%x, demon_lock.m_queue 0x%x\n",
		&ctlr->demon_lock, ctlr->demon_lock.m_bits, ctlr->demon_lock.m_queue);

	qprintf ("&demon_sema 0x%x, demon_sema.s_queue 0x%x, demon_sema.s_un.s_st.count %d, demon_sema.s_un.s_st.flags 0x%x\n",
		&ctlr->demon_sema, ctlr->demon_sema.s_queue, ctlr->demon_sema.s_un.s_st.count, ctlr->demon_sema.s_un.s_st.flags);

	qprintf ("demon_msg_count %d, demon_msg_in %d, demon_msg_out %d, demon_flags 0x%x, demon_timeout %d, demon_pdbc_count %d\n",
		ctlr->demon_msg_count, ctlr->demon_msg_in, ctlr->demon_msg_out, 
		ctlr->demon_flags, ctlr->demon_timeout, ctlr->demon_pdbc_count);

	qprintf ("demon_scn_count %d\n",
		ctlr->demon_scn_count);

	for (i=0; i<DEMON_MAX_EVENTS; i++) {
		if (ctlr->demon_msg_queue[i].msg != 0 || ctlr->demon_msg_queue[i].arg != 0) {
			qprintf ("demon_msg_queue[%d].msg %d, arg 0x%x", i, ctlr->demon_msg_queue[i].msg, ctlr->demon_msg_queue[i].arg);
			if (i == ctlr->demon_msg_in)		qprintf ("          -- demon_msg_in\n");
			else if (i == ctlr->demon_msg_out)	qprintf ("          -- demon_msg_out\n");
			else					qprintf ("\n");
		}
	}
	
	qprintf ("&reset_sema 0x%x, reset_sema.s_queue 0x%x, reset_sema.s_un.s_st.count %d, reset_sema.s_un.s_st.flags 0x%x\n",
		&ctlr->reset_sema, ctlr->reset_sema.s_queue, ctlr->reset_sema.s_un.s_st.count, ctlr->reset_sema.s_un.s_st.flags);

	qprintf ("&probe_sema 0x%x, probe_sema.s_queue 0x%x, probe_sema.s_un.s_st.count %d, probe_sema.s_un.s_st.flags 0x%x\n",
		&ctlr->probe_sema, ctlr->probe_sema.s_queue, ctlr->probe_sema.s_un.s_st.count, ctlr->probe_sema.s_un.s_st.flags);

	qprintf ("ql_ncmd %d, ha_last_intr 0x%x, drain_count %d, drain_timeout %d, mbox_timeout_info %d, lip_resets %d\n",
		ctlr->ql_ncmd, ctlr->ha_last_intr, ctlr->drain_count, ctlr->drain_timeout, ctlr->mbox_timeout_info,
		ctlr->lip_resets);

	qprintf ("ql_awol_count %d\n",ctlr->ql_awol_count);

	for (i=0; i<MBOX_REGS; i++) qprintf ("mailbox[%d] 0x%x\n",i,ctlr->mailbox[i]);
	for (i=0; i<MBOX_REGS; i++) qprintf ("mailbox_out[%d] 0x%x\n",i,ctlr->mailbox_out[i]);

	qprintf ("\nTARGET_MAPS\n");
	for (i=0; i<QL_MAX_LOOP_IDS; i++) {
		tm = &ctlr->target_map[i];
		
		if (!tm->tm_qti && !tm->tm_port_id && !tm->tm_node_name && !tm->tm_port_name) continue;

		qprintf ("target_map[%d].tm_port_id 0x%x, tm_flags 0x%x, tm_node_name 0x%llx, tm_port_name 0x%llx, tm_qti 0x%x\n",
			i,tm->tm_port_id, tm->tm_flags, tm->tm_node_name, tm->tm_port_name, tm->tm_qti);
		qprintf ("target_map[%d].tm_mutex 0x%x, tm_mutex.m_bits 0x%x, tm_mutex.m_queue 0x%x\n",
			i, &tm->tm_mutex, tm->tm_mutex.m_bits, tm->tm_mutex.m_queue);

		if (qti=tm->tm_qti) {	/* yes, assignment */
			qprintf ("   qti target %d: recovery_step %d, &target_mutex 0x%x, target_mutex.m_bits 0x%x, target_mutex.m_queue 0x%x\n",
				qti->target, qti->recovery_step, &qti->target_mutex, qti->target_mutex.m_bits, qti->target_mutex.m_queue);
			qprintf ("   qti target %d: req_active 0x%x, req_last 0x%x, req_count %d, unique_cookie %d\n",
				qti->target, qti->req_active, qti->req_last, qti->req_count, qti->unique_cookie);
			if (qti->req_active) idbg_qlfc_dump_scsi_chain ("\t\t req_active: ",qti->req_active);
			qprintf ("   qti target %d: targ_vertex %d, local_lun_info 0x%x, last_local_lun_info 0x%x\n",
				qti->target, qti->targ_vertex, qti->local_lun_info, qti->last_local_lun_info);
			qprintf ("   qti target %d: awol_timeout %d, awol_retries %d\n",qti->target, qti->awol_timeout, qti->awol_retries);
			qprintf ("   qti target %d: awol_giveup_retries %d\n",qti->target, qti->awol_giveup_retries);
			if (qli=qti->local_lun_info) while (qli) {
				qprintf ("     qli[%d]: &qli_open_mutex 0x%x, qli_open_mutex.m_bits 0x%x, qli_open_mutex.m_queue 0x%x\n",
					qli->qli_lun, &qli->qli_open_mutex, qli->qli_open_mutex.m_bits, qli->qli_open_mutex.m_queue);
				qprintf ("     qli[%d]: &qli_lun_mutex 0x%x, qli_lun_mutex.m_bits 0x%x, qli_lun_mutex.m_queue 0x%x\n",
					qli->qli_lun, &qli->qli_lun_mutex, qli->qli_lun_mutex.m_bits, qli->qli_lun_mutex.m_queue);
				qprintf ("     qli[%d]: qli_awaitf 0x%x, qli_awaitb 0x%x, qli_iwaitf 0x%x, qli_iwaitb 0x%x\n",
					qli->qli_lun, qli->qli_awaitf, qli->qli_awaitb, qli->qli_iwaitf, qli->qli_iwaitb);
				qprintf ("     qli[%d]: qli_cmd_awcnt %d, qli_cmd_iwcnt %d, qli_ref_count %d, qli_cmd_rcnt %d\n",
					qli->qli_lun, qli->qli_cmd_awcnt, qli->qli_cmd_iwcnt, qli->qli_ref_count, qli->qli_cmd_rcnt);
				qprintf ("     qli[%d]: qli_tinfo 0x%x, qli_sense_callback 0x%x, next 0x%x\n",
					qli->qli_lun, qli->qli_tinfo, qli->qli_sense_callback, qli->next);
				if (sti=qli->qli_tinfo) {	/* yes, assignment */
					qprintf ("     qli[%d]: qli_tinfo.si_inq 0x%x, qli_tinfo.si_sense 0x%x\n",
						qli->qli_lun, sti->si_inq, sti->si_sense);
					qprintf ("     qli[%d]: qli_tinfo.si_maxq %d, si_qdepth %d, si_qlimit %d, si_ha_status 0x%x\n",
						qli->qli_lun, sti->si_maxq, sti->si_qdepth, sti->si_qlimit, sti->si_ha_status);
				}
				qprintf ("     qli[%d]: qli_dev_flags 0x%x\n",
					qli->qli_lun, qli->qli_dev_flags);
				if (qli->qli_awaitf) idbg_qlfc_dump_scsi_chain ("       awaitf: ",qli->qli_awaitf);
				if (qli->qli_iwaitf) idbg_qlfc_dump_scsi_chain ("       iwaitf: ",qli->qli_iwaitf);
				qli = qli->next;
			}
			qprintf ("\n");
		}
	}

	fwp = &ctlr->ql_defaults.fw_params;
	port_name = (uint64_t *)&fwp->portName;
	qprintf ("ql_defaults.fw_params.version %d, maxFrameSize %d, maxIOCBperPort %d, executionThrottle %d\n",
		fwp->version, fwp->maxFrameSize, fwp->maxIOCBperPort, fwp->executionThrottle);
	qprintf ("ql_defaults.fw_params.retryCount %d, retryDelay %d, portName 0x%x, hardID %d, loginTimeout %d\n",
		fwp->retryCount, fwp->retryDelay, *port_name, fwp->hardID, fwp->loginTimeout);
	qprintf ("ql_defaults.fw_params.enableTGTDVCtype %d, enableADISC %d, disableInitator %d, enableTargetMode %d\n",
		fwp->fwOption0.bits.enableTGTDVCtype, fwp->fwOption0.bits.enableADISC, 
		fwp->fwOption0.bits.disableInitator, fwp->fwOption0.bits.enableTargetMode);
	qprintf ("ql_defaults.fw_params.enableFastposting %d, enableFullDuplex %d, enableFairness %d, enableHardLoopID %d\n",
		fwp->fwOption0.bits.enableFastposting, fwp->fwOption0.bits.enableFullDuplex, 
		fwp->fwOption0.bits.enableFairness, fwp->fwOption0.bits.enableHardLoopID);
	qprintf ("ql_defaults.fw_params.fullLoginOnLIP %d, descendIDSearch %d, disbaleInitialLIP %d, enablePDBChange %d\n",
		fwp->fwOption1.bits.fullLoginOnLIP, fwp->fwOption1.bits.descendIDSearch, 
		fwp->fwOption1.bits.disbaleInitialLIP, fwp->fwOption1.bits.enablePDBChange);
		
	/*
	** dump scsi request chains LAST
	*/

	qprintf ("req_forw 0x%x, req_back 0x%x, waitf 0x%x, waitb 0x%x, compl_forw 0x%x, compl_back 0x%x\n",
		ctlr->req_forw, ctlr->req_back, ctlr->waitf, ctlr->waitb, ctlr->compl_forw, ctlr->compl_back);

	if (ctlr->req_forw)	idbg_qlfc_dump_scsi_chain ("req_forw  : ",ctlr->req_forw);
	if (ctlr->waitf)	idbg_qlfc_dump_scsi_chain ("waitf     : ",ctlr->waitf);
	if (ctlr->compl_forw)	idbg_qlfc_dump_scsi_chain ("compl_forw: ",ctlr->compl_forw);

}

/*
   ++ kp qlregs - print internal registers for QL controller. If no
   ++ argument is supplied, print a short list of all controllers.  
*/
void
idbg_qlfc_regs(int number)
{
	int  rc;
	QLFC_CONTROLLER *ctlr;

	ctlr = qlfc_controllers;
	while (ctlr) {
		if (ctlr->ctlr_number == number) break;
		ctlr = ctlr->next;
	}

  if (ctlr) {
    CONTROLLER_REGS * isp = (CONTROLLER_REGS *)ctlr->base_address;
	uint16_t bus_iscr = 0;
    uint16_t bus_icr = 0;
    uint16_t bus_isr = 0;
    uint16_t bus_sema;
    uint16_t hccr = 0;
    uint16_t mbox0, mbox1, mbox2, mbox3, mbox4, mbox5;

    /* Get copies of the registers.  */
    mbox0 = PCI_INH(&isp->mailbox0);
    mbox1 = PCI_INH(&isp->mailbox1);
    mbox2 = PCI_INH(&isp->mailbox2);
    mbox3 = PCI_INH(&isp->mailbox3);
    mbox4 = PCI_INH(&isp->mailbox4);
    mbox5 = PCI_INH(&isp->mailbox5);

	bus_iscr = PCI_INH(&isp->icsr);
    bus_icr = PCI_INH(&isp->icr);
    bus_isr = PCI_INH(&isp->isr);
    bus_sema = PCI_INH(&isp->bus_sema);
    hccr = PCI_INH(&isp->hccr);


    qprintf("mbox0 = %x, mbox1 = %x\n", mbox0, mbox1);
    qprintf("mbox2 = %x, mbox3 = %x\n", mbox2, mbox3);
    qprintf("mbox4 = %x, mbox5 = %x\n", mbox4, mbox5);
    qprintf("BUS_ISCR=%x BUS_ICR=%x, BUS_ISR=%x, BUS_SEMA=%x\n",
	    bus_iscr, bus_icr, bus_isr, bus_sema);
    qprintf("HCCR=%x: %x  \n", &isp->hccr, hccr);

	PCI_OUTH(&isp->hccr, HCCR_CMD_PAUSE);
	flushbus();
	for (rc = 10000; ; rc-- ) {
		DELAY(100);
		if (PCI_INH(&isp->hccr) & HCCR_PAUSE)
			break;
	}
	qprintf ("R20= %x\n", (uint64_t)PCI_INH(&isp->cmd_dma));
	qprintf ("R40= %x\n", (uint64_t)PCI_INH(&isp->xmit_dma));
	qprintf ("R60= %x\n", (uint64_t)PCI_INH(&isp->recv_dma));
	qprintf ("ACC= %x\n", (uint64_t)PCI_INH(&isp->r80));
	qprintf ("R1= %x\n", (uint64_t)PCI_INH(&isp->r82));
	qprintf ("R2= %x\n", (uint64_t)PCI_INH(&isp->r84));
	qprintf ("R3= %x\n", (uint64_t)PCI_INH(&isp->r86));
	qprintf ("R4= %x\n", (uint64_t)PCI_INH(&isp->r88));
	qprintf ("R5= %x\n", (uint64_t)PCI_INH(&isp->r8a));
	qprintf ("R6= %x\n", (uint64_t)PCI_INH(&isp->r8c));
	qprintf ("R7= %x\n", (uint64_t)PCI_INH(&isp->r8e));
	qprintf ("R8= %x\n", (uint64_t)PCI_INH(&isp->r90));
	qprintf ("R9= %x\n", (uint64_t)PCI_INH(&isp->r92));
	qprintf ("R10= %x\n", (uint64_t)PCI_INH(&isp->r94));
	qprintf ("R11= %x\n", (uint64_t)PCI_INH(&isp->r96));
	qprintf ("R12= %x\n", (uint64_t)PCI_INH(&isp->r98));
	qprintf ("R13= %x\n", (uint64_t)PCI_INH(&isp->r9a));
	qprintf ("R14= %x\n", (uint64_t)PCI_INH(&isp->r9c));
	qprintf ("R15= %x\n", (uint64_t)PCI_INH(&isp->r9e));

	qprintf ("reg offset a0 = %x\n", (uint64_t)PCI_INH(&isp->ra0));
	qprintf ("reg offset a2 = %x\n", (uint64_t)PCI_INH(&isp->ra2));
	qprintf ("reg offset a4 = %x\n", (uint64_t)PCI_INH(&isp->pcr));
	qprintf ("reg offset a6 = %x\n", (uint64_t)PCI_INH(&isp->ra6));
	qprintf ("reg offset a8 = %x\n", (uint64_t)PCI_INH(&isp->ra8));
	qprintf ("reg offset aa = %x\n", (uint64_t)PCI_INH(&isp->raa));
	qprintf ("reg offset ac = %x\n", (uint64_t)PCI_INH(&isp->rac));

	qprintf ("reg offset b0 = %x\n", (uint64_t)PCI_INH(&isp->mctr));
	qprintf ("reg offset b2 = %x\n", (uint64_t)PCI_INH(&isp->rb2));

	qprintf ("reg offset cc = %x\n", (uint64_t)PCI_INH(&isp->rcc));
	qprintf ("reg offset ce = %x\n", (uint64_t)PCI_INH(&isp->rce));

	qprintf ("reg offset b8 = %x\n", (uint64_t)PCI_INH(&isp->rb8));
	qprintf ("reg offset ba = %x\n", (uint64_t)PCI_INH(&isp->rba));
	qprintf ("reg offset bc = %x\n", (uint64_t)PCI_INH(&isp->rbc));
	qprintf ("reg offset be = %x\n", (uint64_t)PCI_INH(&isp->rbe));

	PCI_OUTH(&isp->hccr, HCCR_CMD_RELEASE);
  }
}
