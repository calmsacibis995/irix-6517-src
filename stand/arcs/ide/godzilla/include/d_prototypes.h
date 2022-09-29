/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident	"ide/godzilla/include/d_prototypes.h:  $Revision: 1.23 $"

/*
 * IDE godzilla function prototypes
 */

#ifndef __IDE_GOD_PROTO_H__
#define __IDE_GOD_PROTO_H__

/* include */
/* XXX nested header files! necessary for Heart_Regs, Bridge_Regs & */
/*  	XbowRegs_ definitions */
#include "d_bridge.h"
#include "d_heart.h"
#include "d_xbow.h"
#include "d_ioc3.h"
#include "d_duart.h"
#include "d_rtc.h"
#include "d_scsiregs.h"

/* in util dir */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
extern	int         report_pass_or_fail( _Test_Info *Test, __uint32_t erorrs);
extern	__uint64_t	pio_reg_mod_32(__uint32_t address, __uint32_t mask, int mode);
extern	__uint64_t	pio_reg_mod_64(__uint32_t address, __uint32_t mask, int mode);
extern	__uint64_t	br_reg_mod_64(__uint32_t address, __uint32_t mask, int mode);
extern	void		_send_bootp_msg(char *msg);
#endif /* C || C++ */

/* in heart dir */
extern	bool_t	hr_intr(void);
extern	bool_t	hr_misc(void);
extern	bool_t	hr_pio_wcr(void);
extern	bool_t	hr_piu_acc(void);
extern	bool_t	hr_regs(__uint32_t argc, char **argv);
extern	bool_t	_hr_regs_read_write(Heart_Regs *regs_ptr);
extern	bool_t	_special_hr_cases(void);
extern	bool_t	hr_read_reg(__uint32_t argc, char **argv);
extern	bool_t	hr_write_reg(__uint32_t argc, char **argv);

/* in bridge dir */
extern	bool_t	br_err(__uint32_t argc, char **argv);
extern	bool_t	br_intr(int argc, char *argv[]);
extern	bool_t	_br_is_valid_vector(__uint64_t intr_vector);
extern	bool_t	_br_reset_br_isr(__uint32_t intr_group);
extern	uchar_t	_br_intr_level(__uint64_t intr_vector);
extern	bool_t	br_pci_master(void);
extern	bool_t	_br_pci_master_core(bool_t io_mem, bridgereg_t pci_add_off,
	bridgereg_t pci_data, unsigned char pci_dev, bridgereg_t *int_status);
extern	__uint32_t	_br_get_SSRAM_size(__uint64_t bridge_control_reg);
extern	bool_t	br_ram(__uint32_t argc, char **argv);
extern	bool_t	br_regs(__uint32_t argc, char **argv);
extern	bool_t	_br_regs_read_write(Bridge_Regs *regs_ptr);
extern	bool_t	_special_br_cases(void);
extern	bool_t	br_read_reg(__uint32_t argc, char **argv);
extern	bool_t	br_write_reg(__uint32_t argc, char **argv);

/* in heart_bridge dir */
extern	bool_t	hb_reset(__uint32_t argc, char **argv);
extern	bool_t	_hb_reset(bool_t res_heart, bool_t res_bridge);
extern	bool_t	hb_badllp(void);
extern	bool_t	_hb_badllp_from_heart(void);
extern	bool_t	_hb_badllp_from_bridge(void);
extern	bool_t	_hb_badllp_setup(void);
extern	bool_t	_hb_print_retry_counters(void);
extern	bool_t	hb_chkout(__uint32_t argc, char **argv);
extern	bool_t	_hb_chkout(bool_t chk_heart, bool_t chk_bridge);
extern	bool_t	hb_flow_ctrl(void);
extern	bool_t	hb_flow_ctrl(void);
extern	bool_t	_hb_test_pipe(char head, heartreg_t head_address_reg,
	heartreg_t credit_counter_reg, heartreg_t cre_cnt_val,
	heartreg_t *fc_time_out, heartreg_t *fc_credit_out);
extern	bool_t	hb_rest_regs(void);
extern	void	_hr_save_regs(heartreg_t *p_regs, heartreg_t *p_save);
extern	void	_hr_rest_regs(heartreg_t *p_regs, heartreg_t *p_save);
extern	void	_br_save_regs(bridgereg_t *p_regs, bridgereg_t *p_save);
extern	void	_br_rest_regs(bridgereg_t *p_regs, bridgereg_t *p_save);
extern	bool_t	hb_status(__uint32_t argc, char **argv);
extern	bool_t	_hb_status(bool_t heart_stat, bool_t bridge_stat);
extern	bool_t	_hb_wait_for_widgeterr(void);
extern	void	_disp_heart_cause(void);	/* really a heart function */
extern	void	_hb_disable_proc_intr(void);
extern	bool_t	_hb_poll_proc_cause(uchar_t interrupt_level);

/* in xbow dir */
extern	bool_t	x_reset(void);
extern	bool_t	x_acc(void);
extern	bool_t	hx_badllp(void);
extern	bool_t	_hx_badllp_from_heart(void);
extern	bool_t	_hx_badllp_from_bridge(void);
extern	bool_t	_hx_print_retry_counters(void);
extern	bool_t	_hx_badllp_setup(void);
extern	bool_t	hx_badllp(void);
extern	bool_t	xr_regs(void);
extern	bool_t	_xr_regs_read_write(Xbow_Regs *regs_ptr);
extern	bool_t	_special_xr_cases(void);
extern	bool_t	_hx_badllp_from_xbow(void);
extern	bool_t	_hx_badllp_from_heart(void);
extern	bool_t	x_read_reg(__uint32_t argc, char **argv);
extern	bool_t	x_write_reg(__uint32_t argc, char **argv);

/* in h_x_b dir */
extern	bool_t	hxb_reset(void);
extern	void	_xr_save_regs(xbowreg_t *p_regs, xbowreg_t *p_save);
extern	void	_xr_rest_regs(xbowreg_t *p_regs, xbowreg_t *p_save);
extern	bool_t	hxb_chkout(__uint32_t argc, char **argv);
extern	bool_t	_hxb_chkout(bool_t chk_heart, bool_t chk_xbow, bool_t chk_bridge);
extern	bool_t	hxb_status(void);

extern	bool_t	ioc3_regs(__uint32_t argc, char **argv);
extern  bool_t  _ioc3_regs_check_default(ioc3_Registers *regs_ptr);
extern	bool_t	_ioc3_regs_read_write(ioc3_Registers *regs_ptr);
extern	bool_t	_special_ioc3_cases(void);
extern	bool_t	ioc3_read_reg(__uint32_t argc, char **argv);
extern	bool_t	ioc3_write_reg(__uint32_t argc, char **argv);

extern	bool_t	duart_regs(__uint32_t argc, char **argv);
extern  bool_t  _duart_regs_check_default(duart_Registers *regs_ptr);
extern	bool_t	_duart_regs_read_write(duart_Registers *regs_ptr);
extern	bool_t	_special_duart_cases(void);
extern	bool_t	duart_read_reg(__uint32_t argc, char **argv);
extern	bool_t	duart_write_reg(__uint32_t argc, char **argv);

extern void initParallalPort(void);
extern bool_t testFillFIFO(void);
extern bool_t testDataFIFO(void);
extern bool_t testDataLinesFIFO(void);

bool_t pci_rtc(__uint32_t argc, char **argv);
bool_t rtcTickTock(void);
bool_t rtcRAM(void);
void rtcWrite (char reg, char value);
char rtcRead (char reg);
extern	bool_t	rtc_regs(int argc, char **argv);
extern  bool_t  _rtc_regs_check_default(rtc_Registers *regs_ptr);
extern	bool_t	_rtc_regs_read_write(rtc_Registers *regs_ptr);
bool_t rtcBattery(void);

extern	bool_t	scsi_regs(__uint32_t argc, char **argv);
extern  bool_t  _scsi_regs_check_default(scsi_Registers *regs_ptr);
extern	bool_t	_scsi_regs_read_write(scsi_Registers *regs_ptr);
extern	bool_t	_special_scsi_cases(void);

extern int RadCSpaceTest(int argc, char** argv);
extern int RadRamTest(int argc, char** argv);
extern int RadStatusTest(int argc, char** argv);
extern int RunAllRadTests(int argc, char** argv);
extern int RunAllCardRadTests ();

extern int RunAllShoeBoxTests(int argc, char** argv);


extern _Test_Info  CPU_Test_err[];


#endif	/* __IDE_GOD_PROTO__ */
