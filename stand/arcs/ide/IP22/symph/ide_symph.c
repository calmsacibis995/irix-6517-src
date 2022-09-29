/*
**
**	Diagnostics and IDE for Adobe Pixel Formatter board
*/

#include "uif.h"
#include "saioctl.h"
#include "sys/sbd.h"
#ifdef IP20
#include "sys/IP20.h"
#endif
#ifdef IP22
#include "sys/IP22.h"
#endif

#include "apf_test.h"
#include "sys/apf_diag.h"

extern int badaddr();
unsigned int ide_apf_test_pat[] = {
	0xffffffff,
	0xaaaaaaaa,
	0x55555555,
	0,
	END_OF_PAT
};

volatile unsigned int *ide_probe_apf(char *);
int do_test(int test_id, 
	    volatile unsigned int *addr, 
	    char *tstname,
	    void (* err_print)(volatile pf_diag_status_blk *));

#define TEST_PASSED 1

/* 68302 test ids as per Adobe 68302-Pixel Formatter diagnostic spec */
#define PF_REGS       1
#define PF_ROM_CHKSUM 2
#define PF_ADR_LINE   3
#define PF_SHRD_MEM   4
#define PF_INTRN_RAM  5
#define PF_UNSHRD_MEM 6

/* Error print routines for tests */
void pf_regs_err(volatile pf_diag_status_blk *);
void pf_rom_chksum_err(volatile pf_diag_status_blk *);
void pf_adr_line_err(volatile pf_diag_status_blk *);
void pf_shrd_mem_err(volatile pf_diag_status_blk *);
void pf_intrn_ram_err(volatile pf_diag_status_blk *);
void pf_unshrd_mem_err(volatile pf_diag_status_blk *);

/*********************************
 * 68302 tests internal registers
 *********************************/
pf_regs(int argc, char *argv[])
{
  volatile unsigned int *addr;

  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF 68302 Register Test");
    return FAILED;
  }

  if ( do_test(PF_REGS, addr, argv[0], pf_regs_err) != FAILED) {
    okydoky();
    return 0;
  }  else {
    return FAILED;
  }
}
/**************
 * pf_regs_err
 *************/
void 
pf_regs_err(volatile pf_diag_status_blk *statblk)
{
  sum_error("PF 68302 Register Test");
  msg_printf(ERR,
	     "pf_regs: Failed!  Error Code= 0x%x\n", 
	     (statblk->cmd_stat & ERROR_CODE) >> 8);
  msg_printf(ERR,
	     "Expected= 0x%x, Actual= 0x%x\n", 
	     statblk->p_exp,
	     statblk->p_act);
  return;
}

/****************************
 * 68302 verify ROM checksum
 ****************************/
pf_rom_chksum(int argc, char *argv[])
{
  volatile unsigned int *addr;

  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF 68302 Rom Checksum Test");
    return FAILED;
  }

  if (do_test(PF_ROM_CHKSUM, addr, argv[0], pf_rom_chksum_err) != FAILED) {
    okydoky();
    return 0;
  }  else {
    return FAILED;
  }
}
/*******************
 * pf_rom_chksum_err
 *******************/
void
pf_rom_chksum_err(volatile pf_diag_status_blk *statblk)
{
  sum_error("PF 68302 Rom Checksum Test");
  msg_printf(ERR,
	     "pf_rom_chksum: Failed!  Error Code= 0x%x\n", 
	     (statblk->cmd_stat & ERROR_CODE) >> 8);
  msg_printf(ERR,
	     "Expected= 0x%x, Actual= 0x%x\n", 
	     statblk->p_exp,
	     statblk->p_act);
  return;
}

/*****************************************
 * 68302 tests address lines to shared ram
 *****************************************/
pf_adr_line(int argc, char *argv[])
{
  volatile unsigned int *addr;

  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF 68302 Address Line Test");
    return FAILED;
  }

  if (do_test(PF_ADR_LINE, addr, argv[0], pf_adr_line_err) != FAILED) {
    okydoky();
    return 0;
  }  else {
    return FAILED;
  }
}
/*******************
 * pf_adr_line_err
 *******************/
void
pf_adr_line_err(volatile pf_diag_status_blk *statblk)
{
  sum_error("PF 68302 Address Line Test");
  msg_printf(ERR,
	     "pf_adr_line: Failed!  Error Code= 0x%x\n", 
	     (statblk->cmd_stat & ERROR_CODE) >> 8);
  msg_printf(ERR,
	     "Pass #= 0x%x, Expected= 0x%x, Actual= 0x%x, Addr= 0x%x\n", 
	     statblk->p_misc,
	     statblk->p_exp,
	     statblk->p_act,
	     statblk->p_other);
  return;
}

/****************************
 * 68302 tests shared memory
 ****************************/
pf_shrd_mem(int argc, char *argv[])
{
  volatile unsigned int *addr;

  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF 68302 Shared Memory Test");
    return FAILED;
  }

  if (do_test(PF_SHRD_MEM, addr, argv[0], pf_shrd_mem_err) != FAILED) {
    okydoky();
    return 0;
  }  else {
    return FAILED;
  }
}
/*******************
 * pf_shrd_mem_err
 *******************/
void
pf_shrd_mem_err(volatile pf_diag_status_blk *statblk)
{
  sum_error("PF 68302 Shared Memory Test");
  msg_printf(ERR,
	     "pf_shrd_mem: Failed!  Error Code= 0x%x\n", 
	     (statblk->cmd_stat & ERROR_CODE) >> 8);
  msg_printf(ERR,
	     "Addr= 0x%x, Expected= %x, Actual= %x\n", 
	     statblk->p_misc,
	     statblk->p_exp,
	     statblk->p_act);
  return;
}

/****************************
 * 68302 internal RAM tests *
 ****************************/
pf_intrn_ram(int argc, char *argv[])
{
  volatile unsigned int *addr;

  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF 68302 Internal Ram Test");
    return FAILED;
  }

  if (do_test(PF_INTRN_RAM, addr, argv[0], pf_intrn_ram_err) != FAILED) {
    okydoky();
    return 0;
  }  else {
    return FAILED;
  }
}
/*******************
 * pf_intrn_ram_err
 *******************/
void
pf_intrn_ram_err(volatile pf_diag_status_blk *statblk)
{
  sum_error("PF 68302 Internal Ram Test");
  msg_printf(ERR,
	     "pf_intrn_ram: Failed!  Error Code= 0x%x\n", 
	     (statblk->cmd_stat & ERROR_CODE) >> 8);
  msg_printf(ERR,
	     "Addr= 0x%x, Expected= %x, Actual= %x\n", 
	     statblk->p_misc,
	     statblk->p_exp,
	     statblk->p_act);
  return;
}
/****************************
 * 68302 tests unshared memory
 ****************************/
pf_unshrd_mem(int argc, char *argv[])
{
  volatile unsigned int *addr;

  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF 68302 Unshared Memory Test");
    return FAILED;
  }

  /* unshared memory is only on revision level 2 boards */
  if (GET_PRODUCT_REVISION(*addr) != 2) {
    return 0;
  }

  if (do_test(PF_UNSHRD_MEM, addr, argv[0], pf_unshrd_mem_err) != FAILED) {
    okydoky();
    return 0;
  }  else {
    return FAILED;
  }
}
/*******************
 * pf_unshrd_mem_err
 *******************/
void
pf_unshrd_mem_err(volatile pf_diag_status_blk *statblk)
{
  sum_error("PF 68302 Unshared Memory Test");
  msg_printf(ERR,
	     "pf_unshrd_mem: Failed!  Error Code= 0x%x\n", 
	     (statblk->cmd_stat & ERROR_CODE) >> 8);
  msg_printf(ERR,
	     "Addr= 0x%x, Expected= %x, Actual= %x\n", 
	     statblk->p_misc,
	     statblk->p_exp,
	     statblk->p_act);
  return;
}

/*********************
 * Host (SGI) ram test
 *********************/
pf_hst_ram(int argc, char *argv[])
{
  register int i;
  volatile unsigned int *addr;
  volatile pf_diag_status_blk *statblk;

  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF Host Shared Ram Test");
    return 1;
  }

  statblk = (volatile pf_diag_status_blk *)(addr +
                PF_DIAG_STATUS_OFFSET/sizeof(int));

  /* find a place in sram that is safe for us to clobber */
  addr = (volatile unsigned int *)&statblk->p_other;

  for (i = 0; ide_apf_test_pat[i] != END_OF_PAT; i++) {
    *addr = ide_apf_test_pat[i];
    wbflush();
    if (*addr != ide_apf_test_pat[i]) {
      msg_printf(ERR,
		 "%s: miscompare, exp= 0x%x, act= 0x%x\n", 
		 *addr, ide_apf_test_pat[i]);
      /* FAILED */
      sum_error("PF Host Shared Ram Test");
      return 1;
    }
  }

  okydoky();
  return 0;
}

/************************************
 * Look for the Pixel Formatter board 
 ************************************/   
volatile unsigned int * ide_probe_apf(char *name)
{
  volatile unsigned int *addr;

  /* look in slot 0 */
  addr = (volatile unsigned int *)PHYS_TO_K1(GIO_SLOT_0_ADDRESS);
  if (!badaddr(addr, sizeof(int))) {
    /* read product ID */
    if ((*addr & PRODUCT_ID_MASK) == PF_PRODUCT_ID) {
      return addr;
    }
  }

  /* look in slot 1 */
  addr = (volatile unsigned int *)PHYS_TO_K1(GIO_SLOT_1_ADDRESS);
  if (!badaddr(addr, sizeof(int))) {
    /* read product ID */
    if ((*addr & PRODUCT_ID_MASK) == PF_PRODUCT_ID) {
      return addr;
    }
  }

  /* if we get here, the BOARD was not found */
  msg_printf(ERR, "%s: probe failed, pf board not found\n", name);
  return NULL;

} 

/**********************************************************
 * do_test
 *          Runs diagnostics by communicating
 *          with the 68302 through the diag status
 *          block. 
 ************************************************************/
do_test(int testid, 
	volatile unsigned int *addr, 
	char *name, 
	void (*err_print)(volatile pf_diag_status_blk *))
{
  volatile pf_diag_status_blk *statblk;

  statblk = (volatile pf_diag_status_blk *)(addr +
					    PF_DIAG_STATUS_OFFSET/sizeof(int));
  
  /*
   * make sure 68302 is idle and waiting for next test.
   * If the 68302 is not idle, it is likely it failed a power-on
   * diagnostic and did not boot the diag server (this is a feature).  
   * If this is the case, do not over write the status block.  It 
   * contains the error information.
   */
  if ((statblk->cmd_stat & TEST_ID) != IDLE) {
    msg_printf(ERR,
       "%s: 68302 not IDLE. Test not started because 68302 is busy or hung\n",
               name);
    return FAILED;
  }
    
  /* start the 68302 by writing test id */
  statblk->cmd_stat = testid;
  
  /* delay 1 second to allow 68302 to finish test */
  us_delay(1 * 1000000);

  /* if 68302 hasn't finished */
  if ((statblk->cmd_stat & ERROR_CODE) == 0 ) {
    /* if we have a heart beat, try one more second before giving up */	
    if (statblk->cmd_stat & HEART_BEAT) {

      us_delay(1 * 1000000);
      
      if ((statblk->cmd_stat & ERROR_CODE) != 0 ) {
	goto completed;
      }
    }
    
    /* Still no response, so time out */
    msg_printf(ERR,
	       "%s: Timed Out waiting for 68302 to complete diagnostic\n",
               name);
    return FAILED;
  }


completed:
  if ( ((statblk->cmd_stat & ERROR_CODE) >> 8) != TEST_PASSED) {
    /* test failed.  Call error printing routine if one was supplied */
    if (err_print == NULL) {
      msg_printf(ERR,
		 "%s: Test failed!  68302 error code= 0x%x\n", 
		 name, 
		 ((statblk->cmd_stat & ERROR_CODE) >> 8));
    } else {
      (*err_print)(statblk);
    }
    return FAILED;
  } 
  else
    return 0;

}


/*******************************************************
 * pf_dump_statblk:
 *                 print out values in diag status block
 *******************************************************/
pf_dump_blk(int argc, char *argv[])
{
  volatile unsigned int *addr;
  volatile pf_diag_status_blk *statblk;

  /* find pf board */
  addr = ide_probe_apf(argv[0]);
  if (addr == NULL) {
    /* probe prints out error message.  just return here */
    sum_error("PF Dump Status Block");
    return 1;
  }

  statblk = (volatile pf_diag_status_blk *)(addr +
					    PF_DIAG_STATUS_OFFSET/sizeof(int));

  msg_printf(JUST_DOIT,
	     "Test ID= %x, Error Code= %x, Num Params= %x \n",
	     (statblk->cmd_stat & TEST_ID), 
	     (statblk->cmd_stat & ERROR_CODE) >> 8,
	     (statblk->cmd_stat & PARAM_CNT) >> 16); 
  msg_printf(JUST_DOIT,
	     "Parameter (misc)= %x\n", statblk->p_misc);
  msg_printf(JUST_DOIT,
	     "Parameter (expected)= %x\n", statblk->p_exp);
  msg_printf(JUST_DOIT,
	     "Parameter (actual)= %x\n", statblk->p_act);
  msg_printf(JUST_DOIT,
	     "Parameter (other)= %x\n", statblk->p_other);

  return 0;

}

/*********************************
 * is_symphony
 *             Looks for PSfontdisk env variable to
 *             decide if this is a SYMPHONY box and needs
 *             to run PF diagnostics
 **********************************************************/
is_symphony(int argc, char *argv[])
{

	if ( getenv("PSfontdisk") ) {
		/* found SYMPHONY specific environment variable */
		return 1;
	} else {
		return 0;
	}
}
