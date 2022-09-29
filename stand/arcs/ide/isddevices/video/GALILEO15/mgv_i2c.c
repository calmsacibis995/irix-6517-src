/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifdef IP30

#include "mgv_diag.h"

/*** Prototype declarations ***/
uchar_t _mgv_inb_I2C(int);
void _mgv_outb_I2C(int, int);
int mgv_i2c_poll(void);
void mgv_i2c_start(void);
void mgv_i2c_stop(void);
void mgv_i2c_stop_noack(void);
void mgv_i2c_noack(void);
int mgv_i2c_write(long);
int mgv_i2c_dummy_read(void);
int mgv_i2c_read(uchar_t *, int);
void mgv_i2c_slave(uchar_t);
void _mgv_i2c_init(void);
int mgv_i2c_send(uchar_t *, int);
int mgv_i2c_rcv(uchar_t *, int, int);
uchar_t mgv_I2C_edh_seq(int, int, int, int);


/*
 * Read a byte in from the i2c bus controller
 */
uchar_t
_mgv_inb_I2C(int reg)
{
    int val;
    int s;

    us_delay(10);
    GAL_W1(mgvbase, GAL_DIR0, reg);
    mgbase->dcbctrl_gal = HQ3_GAL_PROTOCOL_ASYNC;
    GAL_R1(mgvbase, GAL_DIR1, val); 
    mgbase->dcbctrl_gal = HQ3_GAL_PROTOCOL_NOACK;
    msg_printf(DBG," Read from iic_reg 0x%x = 0x%x\n", reg,val);
    return((uchar_t)(val));
}  


/*
 * Write a byte out to the i2c bus controller
 */
void
_mgv_outb_I2C(int reg, int val)
{
    msg_printf(DBG," Write into iic_reg 0x%x = 0x%x\n", reg,val);    
    us_delay(10);
    GAL_W1(mgvbase, GAL_DIR0, reg);
    mgbase->dcbctrl_gal = HQ3_GAL_PROTOCOL_ASYNC;
    GAL_W1(mgvbase, GAL_DIR1, val&0xff); 
    mgbase->dcbctrl_gal = HQ3_GAL_PROTOCOL_NOACK;
    GAL_W1(mgvbase, GAL_DIR0, 0);
    return;
}

/*
 * poll the i2c bus controller, waiting for it to be ready for a new read/write
 */
int mgv_i2c_timeout = 1000;

int
mgv_i2c_poll(void)
{
    register int timeo = mgv_i2c_timeout;

    while (MGV_I2C_BUSY() && timeo)
    {
        us_delay(10);
        timeo--;
    }

    if ( MGV_I2C_BUSY() ) {
	msg_printf(SUM,"Poll timed out\n");
        _mgv_outb_I2C(GAL_IIC_CTL,I2C_STOP);
        return(-1);
    }

    return(0);
}


/*
 * generate a start condition on the i2c bus
 */
void
mgv_i2c_start(void)
{
    msg_printf(DBG,"iic start\n");

    _mgv_outb_I2C(GAL_IIC_CTL,I2C_START);
}    


/*
 * generate a stop condition on the i2c bus.
 * note the call to i2c_poll: a stop condition interrupts a write that
 * is in progress.
 */
void
mgv_i2c_stop(void)
{
    if( mgv_i2c_poll() == 0 ) {
        msg_printf(DBG,"iic stop\n");

        _mgv_outb_I2C(GAL_IIC_CTL,I2C_STOP);
    }
    else
        msg_printf(SUM,"WARN - Poll timed out on stop\n");

}

void
mgv_i2c_stop_noack(void)
{
    if( mgv_i2c_poll() == 0 ) {

        msg_printf(DBG,"iic stop noack\n");

        _mgv_outb_I2C(GAL_IIC_CTL,I2C_STOP_NOACK);
        /* wait for bus to clear */
        while( _mgv_inb_I2C(GAL_IIC_CTL) & 0x01 == 0 )
                ;;
    }
    else
        msg_printf(SUM,"Poll timed out on stop\n");

}


/*
 * Clear the ACK bit in the control register.
 * This must be done prior to reading the last byte
 * so that the slave transmitter knows not to send
 * any more data.
 */
void
mgv_i2c_noack(void)
{
    uchar_t ctl_reg;
    ctl_reg = I2C_NOACK;
    _mgv_outb_I2C(GAL_IIC_CTL,ctl_reg);
}


/*
 * write a single value on the i2c bus
 * note: start condition must be previously asserted.
 */
int
mgv_i2c_write(long data)
{
    if (mgv_i2c_poll())  {
        msg_printf(DBG,"Poll timed out on write\n");
        return(-1);
    }

    msg_printf(DBG,"iic data write starting\n");

    _mgv_outb_I2C(GAL_IIC_DATA,data);
    return(0);
}

/*
 * Read a single value from the i2c bus.
 */
int
mgv_i2c_dummy_read(void)
{
    int errcode =0;
    uchar_t data;

    if (mgv_i2c_poll()) {
        msg_printf(SUM,"Poll timed out on read\n");
        errcode = -1;
    }

    if ( errcode == 0 )
        /* check for slave acknowledge */
        if ( (_mgv_inb_I2C(GAL_IIC_CTL) & 0x08) != 0x00 )
            errcode = -2;

        msg_printf(CE_NOTE,"iic read\n");

    if ( errcode >= 0 )
        data = _mgv_inb_I2C(GAL_IIC_DATA);

    return(errcode);
}

/*
 * Read a single value from the i2c bus.
 */
int
mgv_i2c_read(uchar_t *data, int poll_flag)
{
    int errcode = 0;
    if ( poll_flag ) {
        if (mgv_i2c_poll())  {
            msg_printf(SUM,"Poll timed out on read\n");
            errcode = -1;
        }
    }

        msg_printf(DBG,"iic read\n");

    *data = _mgv_inb_I2C(GAL_IIC_DATA);
    return(errcode);
}


/*
 * an i2c_write that doesn't poll, because the slave address must be set
 * BEFORE a start condition is asserted.
 */
void
mgv_i2c_slave(uchar_t data)
{
        msg_printf(DBG,"send addr 0x%02x\n",data);

    _mgv_outb_I2C(GAL_IIC_CTL,0x40);     /* enable serial out */
    _mgv_outb_I2C(GAL_IIC_DATA,data);    /* send slave addr */
}


/*
 * initialize the i2c bus
 */
void
_mgv_i2c_init(void)
{
    _mgv_outb_I2C(GAL_IIC_CTL,0);        /* address own addr reg */
    _mgv_outb_I2C(GAL_IIC_DATA,I2C_SLAVE_ADDRESS);

    _mgv_outb_I2C(GAL_IIC_CTL,0x20);     /* address clock register */
    _mgv_outb_I2C(GAL_IIC_DATA,0x18);   /* 8Mhz + 90Khz  XXX define */
}


/*
 * send a block of data down the i2c bus. Return 0 on success,
 * or -(reg no) on failure.
 */
int
mgv_i2c_send(uchar_t *data, int len)
{
    register int i;

    for (i = 0; i <= len; i++) {
        if (mgv_i2c_write(data[i]))
            return(-i);
    }
    return(0);
}
/*
 * receive a block of data from the i2c bus.  Return 0
 * on success or -(reg no) on failure, where (reg no) is
 * the number of the register we failed to read.
 */


int
mgv_i2c_rcv(uchar_t *data, int len, int part_ix)
{
    register int i;
    int errcode = 0;
    int poll_flag = 1;

    /* make byte counter 1-based, not 0-based */
    len++;

    /* dummy read, really gives back slave addr */
    if( (errcode = mgv_i2c_dummy_read()) < 0 )
       return (errcode );


    for (i = 0; i <= len; i++)  {
        if ( i == (len-1) ) {
            /* right before reading next to last byte, clear ack bit */
            mgv_i2c_noack();
            poll_flag = 0;
        }
        else {
            poll_flag = 1;
        }
        if ( i == len ) {
            /* on last byte, set stop before reading */
            mgv_i2c_stop();
            poll_flag = 0;
        }
        errcode = mgv_i2c_read(data+i,poll_flag);
        if ( errcode == -1 )
           return ( -i );
    }
    return(0);
}


/*
 * Read/Write register "reg" ( 0-based ).  rw is VID_READ/VID_WRITE.
 * return value read/written.  Note that as a side-effect, all
 * registers up to and including "reg" are read/written to shadow
 * array. This is the way the Genumm 9001 works :(
 *
 * Note: for writing, put value into shadow_edh_iic_write
 * first. 
 */

uchar_t
mgv_I2C_edh_seq(int addr, int reg, int val, int rw)
{
    int errcode = 0;

    if ( mgv_iic_war ) {
        if ( rw == MGV_EDH_WRITE ) {
            shadow_edh_iic_write[reg] = val;
            mgv_i2c_slave(EDH_ADDR);
            mgv_i2c_start();
            errcode = mgv_i2c_send(shadow_edh_iic_write,reg);
            if ( errcode == 0 )
                mgv_i2c_stop();
        }
        else { /* READ */
            /* wait for bus to clear */
            while( _mgv_inb_I2C(GAL_IIC_CTL) & 0x01 == 0 )
                us_delay(10);
            mgv_i2c_slave(EDH_ADDR | 1);
            mgv_i2c_start();
            errcode = mgv_i2c_rcv(shadow_edh_iic_read,reg,val);
        }
    }
    if ( errcode )
        msg_printf(SUM,
        "I2C Transfer failed: reg %d rw %d code %d\n",reg, rw, errcode);
    return(errcode);
}

#endif
