#ident	"IP12diags/hpc3/chg_volt.c:  $Revision: 1.5 $"

/*
 * chg_volt.c - Set +5V supply to -5%, nominal, or +5%
 */

#include "libsc.h"
#include "uif.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/param.h"
#include "sys/sbd.h"

int chg_volt_ip24(int);
int chg_volt_ip22(int);
char *usage_chgvolt = "Usage:chg_volt [0-2] (0=>4.75V, 1=>5.00V, 2=>5.25V)\n";
/*
 * Command to set +5V supply to 4.75V (-5%), 5.00V (nominal), or 5.25V (+5%)
 * Note, changed the function to be a wrapper that calls either the fullhouse
 * or Guinness change voltage function (R.Frias).
 */
int
chg_volt(int argc,char *argv[])
{
    int val = -1;
    int ret_code ;


    if ( argc != 2 ) {
	printf(usage_chgvolt);
	return (1);
    }

    atob(argv[1],&val);

#ifdef IP22
    /*
     * some day we may need to specifically check for different
     * types of machines.
     */
    if( !is_fullhouse() ) 
	/* Must be a guinness machine */
	ret_code= chg_volt_ip24( val);
    else
#endif
	/* This is a fullhouse machine */
	ret_code= chg_volt_ip22( val);

    return (ret_code);
}

/*
 * Change voltage command for IP22. Just rearranged code here to become
 * a function so we could add Guinness stuff. 
 */
int
chg_volt_ip22( int voltage_level)
{
    extern unsigned int _hpc3_write2;

    /* Save current contents of HPC3_WRITE2 register */
    _hpc3_write2 = *(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE2);

    /* Set bits appropriately */
    switch (voltage_level) {
	case 0:		/* set to 4.75 Volts */
		/* Clear MARGIN_HI bit, set MARGIN_LO bit */
		_hpc3_write2 &= ~(MARGIN_HI | MARGIN_LO);
		_hpc3_write2 |= MARGIN_LO;
		break;
	case 1:		/* set to 5.00 Volts */
		/* Clear MARGIN_HI & MARGIN_LO bits */
		_hpc3_write2 &= ~(MARGIN_HI | MARGIN_LO);
		break;
	case 2:		/* set to 5.25 Volts */
		/* Clear MARGIN_LO bit, set MARGIN_HI bit */
		_hpc3_write2 &= ~(MARGIN_HI | MARGIN_LO);
		_hpc3_write2 |= MARGIN_HI;
		break;
	default:
		printf(usage_chgvolt);
		return (1);
    }

    /* Now set HPC3_WRITE2 Register */
    *(volatile unsigned int *)PHYS_TO_K1(HPC3_WRITE2) = _hpc3_write2;
   
    return(0);
}

#ifdef IP22

/* Defines for the parallel port bits that control the voltage 
 * Note, this is in the control register which is used to drive a special
 * board designed to margin the supplies based on these two bits.
 */
#define MARGIN_LO_IP24	PAR_CTRL_INT
#define MARGIN_HI_IP24	PAR_CTRL_POLARITY

/*
 * Change voltage command for Guinness. Uses the parallel port bits
 * 1 and 2 of the control port register.
 *
 * BIT  2  1
 *      ----
 *	0  0	5.0  volts
 *	0  1	5.25 volts
 *	1  0	4.75 volts
 *	1  1    5.0  volts
 */
int
chg_volt_ip24( int voltage_level)
{
    u_int par_ctrl_reg;

    msg_printf(DBG, "chg_volt_ip24 %x\n", voltage_level);

    /* Save current contents of parallel port control register */
    par_ctrl_reg = *(volatile unsigned int *)PHYS_TO_K1(0x1fbd9804);

    /* Set bits appropriately */
    switch (voltage_level) {
	case 0:		/* set to 4.75 Volts */
		/* Clear MARGIN_HI bit, set MARGIN_LO bit */
		par_ctrl_reg &= ~(MARGIN_HI_IP24 | MARGIN_LO_IP24);
		par_ctrl_reg |= MARGIN_LO_IP24;
		msg_printf(DBG, "set to 4.75 volts %x\n", par_ctrl_reg);
		break;
	case 1:		/* set to 5.00 Volts */
		/* Clear MARGIN_HI & MARGIN_LO bits */
		par_ctrl_reg &= ~(MARGIN_HI_IP24 | MARGIN_LO_IP24);
		msg_printf(DBG, "set to 5.0 volts %x\n", par_ctrl_reg);
		break;
	case 2:		/* set to 5.25 Volts */
		/* Clear MARGIN_LO bit, set MARGIN_HI bit */
		par_ctrl_reg &= ~(MARGIN_HI_IP24 | MARGIN_LO_IP24);
		par_ctrl_reg |= MARGIN_HI_IP24;
		msg_printf(DBG, "set to 5.25 volts %x\n", par_ctrl_reg);
		break;
	default:
		printf(usage_chgvolt);
		return (1);
    }

    /* Now set Parallel Control Register */
    msg_printf(DBG, "par_ctrl_reg %x\n", par_ctrl_reg);
    *(volatile unsigned int *)PHYS_TO_K1(0x1fbd9804) = par_ctrl_reg;

    return(0);
}
#endif /* IP22 */
