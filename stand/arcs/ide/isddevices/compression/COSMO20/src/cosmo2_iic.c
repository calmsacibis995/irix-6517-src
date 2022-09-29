/*
 * Low level p8584 IIC controller interface for talk to AVI AVO chips
 * Lifted from compression/kern/driver/jpeg_I2C.c
 */

#include "cosmo2_defs.h"
#include "cosmo2_mcu_def.h"

extern UBYTE checkDataInFifo( UBYTE );

#define STATUS_OK		0
#define STATUS_I2C_TIMEOUT	1
#define I2C_DATA		1
#define I2C_DATA_NP		2
#define I2C_CTL			3

extern void putCMD (UWORD *);
extern UBYTE getCMD (UWORD *);

typedef struct {
    volatile unsigned char jpeg8584_data;
	volatile unsigned char jpeg8584_ctrl_stat;
} IIC_REG;

#if 1
IIC_REG *p8584 = (IIC_REG *) IIC_BASE;
#else
IIC_REG *p8584 ;
#endif

/*
 * Determine if the previous I2C bus operation has terminated.
 * Return an indication if we timed out so that the caller can
 * reset.
 */

void
put_iic_reg ( UBYTE length, long val, unsigned long addr )
{

    unsigned short cmdbuf[10];
    int comlength;

    comlength = 5 + ((length + 1) / 2);
    cmdbuf[0] = 0x55aa;
    cmdbuf[1] = 5 << 8 | comlength;     /* 5 is MCU command queue */
    cmdbuf[2] = COSMO2_CMD_WRBYTES;
    cmdbuf[3] = cmd_seq++ ;

#if 0
    * ((unsigned long *) &cmdbuf[4]) = (unsigned long) addr;
#else
	cmdbuf[4] = (addr & 0xff0000) >> 16 ;
	cmdbuf[5] = (addr & 0xffff)  ;
#endif
	msg_printf(DBG, " cmdbuf[4] 0x%x \n", cmdbuf[4]);
	msg_printf(DBG, " cmdbuf[5] 0x%x \n", cmdbuf[5]);
    cmdbuf[6] = length;
    cmdbuf[7] = (UWORD) ((val & 0xff ) << 8);

	putCMD(cmdbuf);

}

UBYTE 
get_iic_reg(int length, unsigned long long addr)
{

    unsigned short cmdbuf[10];
    UBYTE comlength = 5;
	UBYTE val8 ;

    cmdbuf[0] = 0x55aa;
    cmdbuf[1] = 5 << 8 | comlength;
    cmdbuf[2] = COSMO2_CMD_RDBYTES;
    cmdbuf[3] = cmd_seq++;
    * ((unsigned long *) &cmdbuf[4]) = (unsigned long) addr;
    cmdbuf[6] = length;

	putCMD(cmdbuf);
	/* DelayFor(10000); */
	us_delay(100000);
	getCMD(cmdbuf);
	val8 = ((cmdbuf[7] & 0xff00) >> 8) ;
	msg_printf(DBG, " val8 is %x \n", val8);

	return (val8);
}

static int
get_I2C_status(IIC_REG *i2c)
{
	/* Loop waiting for good I2C status, or timeout. */
#if 0 
	return get_iic_reg(1, (unsigned long long *) 
	    &i2c->jpeg8584_ctrl_stat);
#else
	return get_iic_reg(1, IIC_BASE+1 );
#endif
}

static int
poll_I2C(char *str, IIC_REG *i2c)
{
	int	timeout = 100;
	int non_expire_count = 0;
	int expire_count = 0;
	int save_status;

	/* Loop waiting for good I2C status, or timeout. */
#if 0
	while ((save_status=get_iic_reg(1, (unsigned long long *) 
	    &i2c->jpeg8584_ctrl_stat)) & IIC_SERIAL_BUSY) {
#else
    while ((save_status=get_iic_reg(1, IIC_BASE+1)) & IIC_SERIAL_BUSY) {
#endif
		msg_printf(DBG, "poll    status=%02x\n", save_status);
		if (--timeout == 0) break;
	}
	        msg_printf(DBG, "poll ok status=%02x\n", save_status);

	/* The serial transfer is no longer busy, or timeout expired. */
	if (timeout == 0) {
		msg_printf(SUM, "timeout status=%02x\n", save_status);
		expire_count++;
		return (STATUS_I2C_TIMEOUT);
	}
	non_expire_count++;

return (STATUS_OK);
}

/*
 * Three types of write on the I2C bus.
 *
 *	I2C_CTL: write to the control register.
 *	I2C_DATA: write to the data register, checking
 *		first if the previous operation ended.
 *		If this check times out, return status
 *		so that the caller can reset.
 *	I2C_DATA_NP: write to the data register, without
 *		checking first if the previous operation
 *		ended.
 */
int
WRITE_I2C (IIC_REG *i2c, long Value, int mode)
{
	char	*self = "WRITE_I2C";
	int	status;
	char * smode;
	switch (mode){
		case I2C_DATA:
		smode="I2C_DATA   ";
		break;
		case I2C_DATA_NP:
		smode="I2C_DATA_NP";
		break;
		case I2C_CTL:
		smode="I2C_CTL    ";
		break;
	}
	us_delay(1000);

msg_printf(DBG, "write_I2c %s %02x ", smode, Value );

	switch (mode) {

	case I2C_DATA:
		if ((status = poll_I2C(self, i2c)) != STATUS_OK) {
msg_printf(SUM, "write_I2c poll fails %x %x\n", Value, mode);
			return (status);
		}
	case I2C_DATA_NP:
#if 0 
		put_iic_reg(1, Value,
			(unsigned long long *) &i2c->jpeg8584_data);
#else
		put_iic_reg(1, Value, (IIC_BASE));
#endif
msg_printf(DBG,  "status=%02x\n",get_I2C_status(i2c));
		break;
	
	case I2C_CTL:
#if 0 
		put_iic_reg(1, Value,
			(unsigned long long *) &i2c->jpeg8584_ctrl_stat);
#else
		put_iic_reg(1, Value, (IIC_BASE+1) );
#endif

msg_printf(DBG,  "status=%02x\n",get_I2C_status(i2c));
		break;
	
	default:
		msg_printf(SUM, "%s: bad mode %d\n",
			self, mode);
		return (-1);
	}

return (STATUS_OK);
}

/*
 * Transmit a -1 terminated array on the I2C bus by writing
 * each word, checking each time that the previous write
 * terminated.   If any write times out, return status so
 * that the caller can recover.
 */
static int
send_I2C_array (IIC_REG *i2c, long *array, char *str)
{
	int	i;
	int	status;

	for (i = 0; array[i] != 0xffffffff; i++) {
		if ((status = WRITE_I2C(i2c, array[i], I2C_DATA)) !=
			STATUS_OK) {
			return (status);
		}
	}

return (STATUS_OK);
}

/*
 * Send a message to an I2C bus slave, by setting up the address
 * and subaddress of the slave, and then transmitting the array.
 * If either the array transmission times out, or fails to 
 * teminate correctly, return bad status.
 */
p8584_send_slave_I2C_msg(long addr, long subaddr, long *data)
{
	int	status;
	IIC_REG  *i2c = p8584;

	/* Set up the address of the I2C slave. */
	WRITE_I2C(i2c, IIC_S1_ESO, I2C_CTL);
	WRITE_I2C(i2c, addr, I2C_DATA_NP);

	/* Set up the subaddress of the I2C slave. */
	WRITE_I2C(i2c, IIC_SEND_START_PLUS_ADDR, I2C_CTL);
	if ((status = WRITE_I2C(i2c, subaddr, I2C_DATA)) != STATUS_OK) {
		return (status);
	}

	/* Send the data to the slave. */
	if ((status = send_I2C_array(i2c, data, "send_slave_I2C_msg ARRAY")) !=
	    STATUS_OK) {
		return (status);
	}
	if ((status = poll_I2C("send_slave_I2C_msg poll", i2c)) != STATUS_OK) {
		return (status);
	}

	/* Set STOP. */
	WRITE_I2C(i2c, IIC_SEND_STOP, I2C_CTL);
return (STATUS_OK);
}

/*
 * Send a message of chars to an I2C bus slave, by setting up the address
 * and subaddress of the slave, and then transmitting the array.
 * If either the array transmission times out, or fails to 
 * teminate correctly, return bad status.
 */
p8584_send_slave_I2C_tbl(IIC_REG *i2c, long addr, unsigned long subaddr, unsigned char *dp,  int n)
{
	int	status;

	/* Set up the address of the I2C slave. */
	WRITE_I2C(i2c, (long)IIC_S1_ESO, I2C_CTL);
	WRITE_I2C(i2c, addr, I2C_DATA_NP);

	/* Set up the subaddress of the I2C slave. */
	/* XXXX - app note polls for BB bit not set before sending start */
	WRITE_I2C(i2c, (long)IIC_SEND_START_PLUS_ADDR, I2C_CTL);
	if ((status = WRITE_I2C(i2c, (long)subaddr, I2C_DATA)) != STATUS_OK) {
		return (status);
	}

	/* Send the data to the slave. */
	while (n--) {
		if ((status = WRITE_I2C(i2c, (long)*dp, I2C_DATA)) !=
			STATUS_OK) {
			return (status);
		}
		dp++;
	}
	if ((status = poll_I2C("send_slave_I2C_msg poll", i2c)) != STATUS_OK) {
		return (status);
	}

	/* Set STOP. */
	WRITE_I2C(i2c, (long)IIC_SEND_STOP, I2C_CTL);

	return (STATUS_OK);
}

/*
 * Send a byte to an I2C bus slave by setting up the address
 * and subaddress of the slave, and then sending the byte,
 * checking for timeout.
 */
int
p8584_send_I2C(IIC_REG *i2c, long data, long addr, long subaddr )
{
	int	status;

	/* Set up the address of the I2C slave. */
	WRITE_I2C(i2c, IIC_S1_ESO, I2C_CTL);
	WRITE_I2C(i2c, addr, I2C_DATA_NP);

	/* Set up the subaddress of the I2C slave. */
	if ((status = 
		WRITE_I2C(i2c, IIC_SEND_START_PLUS_ADDR, I2C_CTL)) 
								!= STATUS_OK) {
		return (status);
	}

	if ((status = WRITE_I2C(i2c, subaddr, I2C_DATA)) != STATUS_OK) {
		return (status);
	}

	/* Send the data to the slave. */
	if ((status = WRITE_I2C(i2c, data, I2C_DATA)) != STATUS_OK) {
		return (status);
	}
	if ((status =
	    poll_I2C("p8584_send_I2C poll", i2c)) != STATUS_OK) {
		return (status);
	}

	/* Set STOP. */
	WRITE_I2C(i2c, IIC_SEND_STOP, I2C_CTL);
return (STATUS_OK);
}

int
p8584_read_I2C_byte (IIC_REG *i2c,
			long addr,
			long subaddr)
{
	char	*self = "p8584_read_I2C_byte";
	UBYTE	regvalue = 0;
	int	status;
	
	WRITE_I2C (i2c, IIC_S1_ESO, I2C_CTL);
	WRITE_I2C (i2c, addr | 0x1, I2C_DATA_NP);
	WRITE_I2C (i2c, IIC_SEND_START_PLUS_ADDR, I2C_CTL);
	/* if valid subaddr, then send that next */
	if ((status = WRITE_I2C(i2c, subaddr, I2C_DATA)) != STATUS_OK)
		return (status);

	WRITE_I2C (i2c, IIC_MASTER_RECV, I2C_CTL);
	if ((poll_I2C (self, i2c)) != STATUS_OK) {
		msg_printf(SUM,  "%s: bad poll\n", self);
	}
	if ((status = poll_I2C(self, i2c)) != STATUS_OK) {
		msg_printf(ERR, "%s: MASTER_RECV poll failed\n", self);
		return (status);
	}
#if 0
	regvalue = get_iic_reg(1, (unsigned long long *)&i2c->jpeg8584_data);
#else
	regvalue = get_iic_reg(1, IIC_BASE);
#endif

	WRITE_I2C (i2c, IIC_SEND_STOP, I2C_CTL);

	return (regvalue&0xff);
}

unsigned long long *
i2cl_regaddr(unsigned long long val)
{
	return ((unsigned long long *) val);
}

/*ARGSUSED1*/
int 
put_i2cl_reg(UBYTE length, __uint32_t val, unsigned long addr)
{
	unsigned char send[2];
	unsigned long slave_addr;
	unsigned long sub_addr;

	/* i2c upper 16 bits is slave address lower 16 is subaddr */
	slave_addr = (long) ((addr >> 16) & 0xffff);

	/* For 7199 - subaddr is always 2, first byte is real subaddr */
	if (slave_addr == DVEG_WRITE_SLAVEADDRESS) {
		sub_addr = 2;
		send[0] = (unsigned char) (addr & 0xff);
		send[1] = (unsigned char) (val & 0xff);
		if (p8584_send_slave_I2C_tbl(p8584, (long)slave_addr, sub_addr, 
		    send, 2) != STATUS_OK) {
			msg_printf(ERR,  "put_i2cl_reg: failed\n");
			return(-1);
		}
	} else {
		sub_addr =  (unsigned long) (addr & 0xff);
		send[0] = (unsigned char) (val & 0xff);
		send[1] = 0;
		if (p8584_send_slave_I2C_tbl(p8584, (long)slave_addr, sub_addr, 
		    send, 1) != STATUS_OK) {
			msg_printf(ERR,  "put_i2cl_reg: failed\n");
			return(-1);
		}
	}
	return (0);
}
		
#if 0
unsigned long long
get_i2cl_reg(int length, unsigned long long *addr)
{
	length = 0;
	
	return (0);
}
#endif

extern char *getenv();
void
i2cl_init()
{
	IIC_REG  *i2cv = p8584;
	char * res;
	int iic_scl_bits;	
	int iic_scl_speed;
	
#if 1
	res=getenv("IIC_SCL");
	if (res==0) res="90";

	atob(res,&iic_scl_speed);
	if (iic_scl_speed > 45) {
		iic_scl_speed=90;
		iic_scl_bits=0;
	}else if (iic_scl_speed > 11) {
		iic_scl_speed=45;
		iic_scl_bits=1;
	} else if (iic_scl_speed > 2) {
		iic_scl_speed=11;
		iic_scl_bits=2;

	} else {
		iic_scl_speed=1;
		iic_scl_bits=3;

	}
#else
		iic_scl_speed = 90;
		iic_scl_bits=0;
#endif

	  msg_printf(VRB, " iic_scl_speed %x \n", iic_scl_speed);

	/* XXXX - app note says don't do this step */
	/* document says write to s0' first, so we will try */
        WRITE_I2C(i2cv, IIC_SLAVE_ADDR, I2C_DATA_NP);

        /* Select own address register  (s0') */
        WRITE_I2C(i2cv, IIC_OWNADDR_REG, I2C_CTL);

        /* Set 8584 I2C slave address. */
        /* slave mode never used on JPEG board now. */
        WRITE_I2C(i2cv, IIC_SLAVE_ADDR, I2C_DATA_NP);

        /* Select clock speed register. */
        WRITE_I2C(i2cv, IIC_CLK_REG, I2C_CTL);

        /* was Set clock speed to SCL==90kHz, prescaler 4.43 MHz. */
        /*WRITE_I2C(i2cv, IIC_CLK_SPEED, I2C_DATA_NP);*/
        /* Set clock speed to SCL==90kHz, prescaler 8.00 MHz. */
	msg_printf(DBG,  "setting i2c clock %x speed=%d\n",
		IIC_CLK_REG_S24|IIC_CLK_REG_S23|iic_scl_bits,iic_scl_speed );
        WRITE_I2C(i2cv, IIC_CLK_REG_S24|IIC_CLK_REG_S23|iic_scl_bits, I2C_DATA_NP);

        /* Select interrupt vector register. */
        /* no interrupts yet on JPEG board. */
        WRITE_I2C(i2cv, IIC_INT_VECT_REG, I2C_CTL);

        /* Set interrupt vector */
        WRITE_I2C(i2cv, IIC_INT_VECT_80XX, I2C_DATA_NP);

	
}

void
avo_slave ()
{

put_i2cl_reg(1, ((DVEG_REG_INPUT_DEFAULT & DVEG_MOD_MASK) | DVEG_MOD_SLAVE), 
			((DVEG_WRITE_SLAVEADDRESS) << 16) | DVEG_REG_INPUT );
put_i2cl_reg(1, 0x30, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_SYNC);
put_i2cl_reg(1, DVEG_REG_GDC_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_GDC );
put_i2cl_reg(1, DVEG_REG_IDEL_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_IDEL );
put_i2cl_reg(1, DVEG_REG_PSO_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_PSO );
put_i2cl_reg(1, DVEG_REG_CCO1_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_CCO1);
put_i2cl_reg(1, DVEG_REG_CCO2_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_CCO2);
put_i2cl_reg(1, DVEG_REG_CHPS_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_CHPS);
put_i2cl_reg(1, DVEG_REG_FSCO_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_FSCO);
put_i2cl_reg(1, DVEG_REG_STD_DEFAULT, ((DVEG_WRITE_SLAVEADDRESS) << 16) |DVEG_REG_STD);

}

/*
These are defined in cosmo2_mcu_def.h

#define  ofc1_write_slaveaddress    ((OFC1_WRITE_SLAVEADDRESS) << 16)
#define  vip_read_slaveaddress      ((VIP_READ_SLAVEADDRESS) << 16)
#define  vip_write_slaveaddress     ((VIP_WRITE_SLAVEADDRESS) << 16)
#define  dveg_write_slaveaddress    ((DVEG_WRITE_SLAVEADDRESS) << 16)

*/

void
avo_genlock()
{
    put_i2cl_reg(1, DVEG_REG_SYNC_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_SYNC));
    put_i2cl_reg(1, DVEG_REG_GDC_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_GDC));
    put_i2cl_reg(1, DVEG_REG_IDEL_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_IDEL));
    put_i2cl_reg(1, DVEG_REG_PSO_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_PSO));
    put_i2cl_reg(1, DVEG_REG_CCO1_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO1));
    put_i2cl_reg(1, DVEG_REG_CCO2_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO2));
    put_i2cl_reg(1, DVEG_REG_CHPS_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CHPS));
    put_i2cl_reg(1, DVEG_REG_FSCO_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_FSCO));
    put_i2cl_reg(1, DVEG_REG_STD_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_STD));
    put_i2cl_reg(1, DVEG_VTBY_BIT | DVEG_FMT_YUV422_BITS | DVEG_SCBW_BIT |
                DVEG_CCIR_BIT, (dveg_write_slaveaddress | DVEG_REG_INPUT));
}

void
avo_combo (UBYTE pal, UBYTE ccir, UBYTE genlock) 
{
    put_i2cl_reg(1, DVEG_REG_SYNC_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_SYNC));
    put_i2cl_reg(1, DVEG_REG_GDC_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_GDC));
    put_i2cl_reg(1, DVEG_REG_IDEL_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_IDEL));
    put_i2cl_reg(1, DVEG_REG_PSO_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_PSO));
    put_i2cl_reg(1, DVEG_REG_CCO1_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO1));
    put_i2cl_reg(1, DVEG_REG_CCO2_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO2));
    put_i2cl_reg(1, DVEG_REG_CHPS_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CHPS));
    put_i2cl_reg(1, DVEG_REG_FSCO_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_FSCO));

   if (ccir) {
		if (pal) {
			put_i2cl_reg(1, DVEG_STD_PAL_CCIR,   (dveg_write_slaveaddress | DVEG_REG_STD));
		} else   {
			put_i2cl_reg(1, DVEG_STD_NTSC_CCIR,   (dveg_write_slaveaddress | DVEG_REG_STD));
		}
	}else   {
		if (pal) {
			put_i2cl_reg(1, DVEG_STD_PAL,   (dveg_write_slaveaddress | DVEG_REG_STD));
		} else   {
		    put_i2cl_reg(1, DVEG_REG_STD_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_STD));
		}
	}

	if (genlock)
    		put_i2cl_reg(1, DVEG_VTBY_BIT | DVEG_FMT_YUV422_BITS | DVEG_SCBW_BIT |
                DVEG_CCIR_BIT, (dveg_write_slaveaddress | DVEG_REG_INPUT));
	else
			put_i2cl_reg(1, DVEG_REG_INPUT_DEFAULT, (dveg_write_slaveaddress | DVEG_REG_INPUT));
}

void
avo_ccir_pal()
{
    put_i2cl_reg(1, DVEG_REG_SYNC_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_SYNC));
    put_i2cl_reg(1, DVEG_REG_GDC_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_GDC));
    put_i2cl_reg(1, DVEG_REG_IDEL_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_IDEL));
    put_i2cl_reg(1, DVEG_REG_PSO_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_PSO));
    put_i2cl_reg(1, DVEG_REG_CCO1_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO1));
    put_i2cl_reg(1, DVEG_REG_CCO2_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO2));
    put_i2cl_reg(1, DVEG_REG_CHPS_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CHPS));
    put_i2cl_reg(1, DVEG_REG_FSCO_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_FSCO));
    put_i2cl_reg(1, DVEG_STD_PAL_CCIR,   (dveg_write_slaveaddress | DVEG_REG_STD));
    put_i2cl_reg(1, DVEG_REG_INPUT_DEFAULT, (dveg_write_slaveaddress | DVEG_REG_INPUT));


}

void
avo_ccir()
{
    put_i2cl_reg(1, DVEG_REG_SYNC_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_SYNC));
    put_i2cl_reg(1, DVEG_REG_GDC_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_GDC));
    put_i2cl_reg(1, DVEG_REG_IDEL_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_IDEL));
    put_i2cl_reg(1, DVEG_REG_PSO_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_PSO));
    put_i2cl_reg(1, DVEG_REG_CCO1_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO1));
    put_i2cl_reg(1, DVEG_REG_CCO2_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO2));
    put_i2cl_reg(1, DVEG_REG_CHPS_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CHPS));
    put_i2cl_reg(1, DVEG_REG_FSCO_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_FSCO));
    put_i2cl_reg(1, DVEG_STD_NTSC_CCIR,   (dveg_write_slaveaddress | DVEG_REG_STD));
    put_i2cl_reg(1, DVEG_REG_INPUT_DEFAULT, (dveg_write_slaveaddress | DVEG_REG_INPUT));


}

void
avo_pal()
{
    put_i2cl_reg(1, DVEG_REG_SYNC_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_SYNC));
    put_i2cl_reg(1, DVEG_REG_GDC_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_GDC));
    put_i2cl_reg(1, DVEG_REG_IDEL_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_IDEL));
    put_i2cl_reg(1, DVEG_REG_PSO_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_PSO));
    put_i2cl_reg(1, DVEG_REG_CCO1_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO1));
    put_i2cl_reg(1, DVEG_REG_CCO2_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO2));
    put_i2cl_reg(1, DVEG_REG_CHPS_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CHPS));
    put_i2cl_reg(1, DVEG_REG_FSCO_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_FSCO));
    put_i2cl_reg(1, DVEG_STD_PAL,   (dveg_write_slaveaddress | DVEG_REG_STD));
    put_i2cl_reg(1, DVEG_REG_INPUT_DEFAULT, (dveg_write_slaveaddress | DVEG_REG_INPUT));


}


void
avo ()
{
	msg_printf(VRB, " AVO init \n");

#if 1
	put_i2cl_reg(1, DVEG_REG_SYNC_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_SYNC));
	put_i2cl_reg(1, DVEG_REG_GDC_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_GDC));
	put_i2cl_reg(1, DVEG_REG_IDEL_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_IDEL));
	put_i2cl_reg(1, DVEG_REG_PSO_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_PSO));
	put_i2cl_reg(1, DVEG_REG_CCO1_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO1));
	put_i2cl_reg(1, DVEG_REG_CCO2_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CCO2));
	put_i2cl_reg(1, DVEG_REG_CHPS_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_CHPS));
	put_i2cl_reg(1, DVEG_REG_FSCO_DEFAULT,  (dveg_write_slaveaddress | DVEG_REG_FSCO));
	put_i2cl_reg(1, DVEG_REG_STD_DEFAULT,   (dveg_write_slaveaddress | DVEG_REG_STD));
	put_i2cl_reg(1, DVEG_REG_INPUT_DEFAULT, (dveg_write_slaveaddress | DVEG_REG_INPUT));

#else
	msg_printf(SUM, " dveg_write_slaveaddress %x \n", DVEG_WRITE_SLAVEADDRESS);
	p8584_send_I2C (p8584, DVEG_REG_SYNC_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_SYNC);
	p8584_send_I2C (p8584,DVEG_REG_GDC_DEFAULT , DVEG_WRITE_SLAVEADDRESS, DVEG_REG_GDC);
	p8584_send_I2C (p8584,DVEG_REG_IDEL_DEFAULT,DVEG_WRITE_SLAVEADDRESS,DVEG_REG_IDEL);
	p8584_send_I2C (p8584, DVEG_REG_PSO_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_PSO);
	p8584_send_I2C (p8584, DVEG_REG_CCO1_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_CCO1);
	p8584_send_I2C (p8584, DVEG_REG_CCO2_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_CCO2);
	p8584_send_I2C (p8584, DVEG_REG_CHPS_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_CHPS);
	p8584_send_I2C (p8584, DVEG_REG_FSCO_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_FSCO);
	p8584_send_I2C (p8584, DVEG_REG_STD_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_STD);
	p8584_send_I2C (p8584, DVEG_REG_INPUT_DEFAULT, DVEG_WRITE_SLAVEADDRESS, DVEG_REG_INPUT);
#endif
}

void
avi_pal()
{

put_i2cl_reg (1, OFC1_REG_IDEL_DEFAULT, (ofc1_write_slaveaddress | OFC1_REG_IDEL));
put_i2cl_reg (1, 0x28, (ofc1_write_slaveaddress | OFC1_REG_HSYB));
put_i2cl_reg (1, 0xf8, (ofc1_write_slaveaddress | OFC1_REG_HSYS));
put_i2cl_reg (1, 0xdf, (ofc1_write_slaveaddress | OFC1_REG_HCLB));
put_i2cl_reg (1, 0xad, (ofc1_write_slaveaddress | OFC1_REG_HCLS));

put_i2cl_reg (1, OFC1_REG_HPHI_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_HPHI);
put_i2cl_reg (1, OFC1_REG_LUMA_COMPOSITE_DEFAULT|0x2, ofc1_write_slaveaddress |
OFC1_REG_LUMA);
put_i2cl_reg (1, OFC1_REG_HUEC_DEFAULT, ofc1_write_slaveaddress |  OFC1_REG_HUEC);
put_i2cl_reg (1, OFC1_REG_CKTQ_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CKTQ);
put_i2cl_reg (1, OFC1_REG_CKTS_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CKTS);

put_i2cl_reg (1, OFC1_REG_PLSE_GAL_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_PLSE);
put_i2cl_reg (1, OFC1_REG_SESE_GAL_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_SESE);
put_i2cl_reg (1, OFC1_REG_GAIN_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAIN);
put_i2cl_reg (1, OFC1_REG_STDC_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_STDC);
put_i2cl_reg (1, OFC1_REG_IOCK_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_IOCK);

put_i2cl_reg (1, OFC1_REG_CTL3_GAL_DEFAULT|0x10, ofc1_write_slaveaddress | OFC1_REG_CTL3);
put_i2cl_reg (1, OFC1_REG_CTL4_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CTL4);
put_i2cl_reg (1, OFC1_REG_CHCV_PAL_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CHCV);
put_i2cl_reg (1, OFC1_REG_SATN_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_SATN);
put_i2cl_reg (1, OFC1_REG_CONT_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CONT);

put_i2cl_reg (1, 0x28, ofc1_write_slaveaddress |  OFC1_REG_HS6B);
put_i2cl_reg (1, 0x2,  ofc1_write_slaveaddress | OFC1_REG_HS6S );
put_i2cl_reg (1,  0xf0, ofc1_write_slaveaddress | OFC1_REG_HC6B );
put_i2cl_reg (1,  0xc8, OFC1_REG_HC6S | ofc1_write_slaveaddress );
put_i2cl_reg (1,OFC1_REG_HP6I_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_HP6I );

put_i2cl_reg (1, OFC1_REG_BRIG_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_BRIG);
put_i2cl_reg (1, OFC1_ACT1_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_ACT1);
put_i2cl_reg (1, OFC1_ACT2_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_ACT2 );
put_i2cl_reg (1, OFC1_MIX1_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX1);
put_i2cl_reg (1, OFC1_REG_CLx1_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL21);

put_i2cl_reg (1, OFC1_REG_CLx2_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL22);
put_i2cl_reg (1, OFC1_REG_CLx1_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL31);
put_i2cl_reg (1, OFC1_REG_CLx2_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL32);
put_i2cl_reg (1, OFC1_REG_GAIx_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAI2);
put_i2cl_reg (1, OFC1_REG_WIPE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_WIPE);
put_i2cl_reg (1,2, ofc1_write_slaveaddress | OFC1_REG_SBOT);
put_i2cl_reg (1, OFC1_REG_GAIx_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAI3);
put_i2cl_reg (1,OFC1_REG_GAIx_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAI4);
put_i2cl_reg (1, OFC1_MIX2_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX2);
put_i2cl_reg (1,0x1, ofc1_write_slaveaddress | OFC1_REG_IVAL);

put_i2cl_reg (1, OFC1_REG_VBPS_50HZ_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_VBPS);
put_i2cl_reg (1, OFC1_REG_VBPR_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_VBPR);
put_i2cl_reg (1, OFC1_REG_ADCG_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_ADCG);

put_i2cl_reg (1, OFC1_REG_MIX3_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX3);
put_i2cl_reg (1, 0x2, ofc1_write_slaveaddress | OFC1_REG_WVAL);
put_i2cl_reg (1, OFC1_REG_MIX4_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX4);
put_i2cl_reg (1, OFC1_REG_GUDL_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GUDL );


}

void
avi()
{

	msg_printf(VRB, " AVI init \n");
#if 1
put_i2cl_reg (1, OFC1_REG_IDEL_DEFAULT, (ofc1_write_slaveaddress | OFC1_REG_IDEL));
put_i2cl_reg (1, 0x20, (ofc1_write_slaveaddress | OFC1_REG_HSYB));
put_i2cl_reg (1, 0x0, (ofc1_write_slaveaddress | OFC1_REG_HSYS));
put_i2cl_reg (1, 0xdf, (ofc1_write_slaveaddress | OFC1_REG_HCLB));
put_i2cl_reg (1, 0xad, (ofc1_write_slaveaddress | OFC1_REG_HCLS));

put_i2cl_reg (1, OFC1_REG_HPHI_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_HPHI);
put_i2cl_reg (1, OFC1_REG_LUMA_COMPOSITE_DEFAULT|0x2, ofc1_write_slaveaddress | OFC1_REG_LUMA);
put_i2cl_reg (1, OFC1_REG_HUEC_DEFAULT, ofc1_write_slaveaddress |  OFC1_REG_HUEC);
put_i2cl_reg (1, OFC1_REG_CKTQ_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CKTQ);
put_i2cl_reg (1, OFC1_REG_CKTS_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CKTS);

put_i2cl_reg (1, OFC1_REG_PLSE_GAL_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_PLSE);
put_i2cl_reg (1, OFC1_REG_SESE_GAL_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_SESE);
put_i2cl_reg (1, OFC1_REG_GAIN_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAIN);
put_i2cl_reg (1, OFC1_REG_STDC_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_STDC);
put_i2cl_reg (1, OFC1_REG_IOCK_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_IOCK);

put_i2cl_reg (1, OFC1_REG_CTL3_GAL_DEFAULT|0x10, ofc1_write_slaveaddress | OFC1_REG_CTL3);  
put_i2cl_reg (1, OFC1_REG_CTL4_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CTL4);
put_i2cl_reg (1, OFC1_REG_CHCV_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CHCV);
put_i2cl_reg (1, OFC1_REG_SATN_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_SATN);
put_i2cl_reg (1, OFC1_REG_CONT_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CONT);

put_i2cl_reg (1, 0x28, ofc1_write_slaveaddress |  OFC1_REG_HS6B);
put_i2cl_reg (1, 0x2,  ofc1_write_slaveaddress | OFC1_REG_HS6S );
put_i2cl_reg (1,  0xf0, ofc1_write_slaveaddress | OFC1_REG_HC6B );
put_i2cl_reg (1,  0xc8, OFC1_REG_HC6S | ofc1_write_slaveaddress );
put_i2cl_reg (1,OFC1_REG_HP6I_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_HP6I );

put_i2cl_reg (1, OFC1_REG_BRIG_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_BRIG); 
put_i2cl_reg (1, OFC1_ACT1_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_ACT1);
put_i2cl_reg (1, OFC1_ACT2_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_ACT2 );
put_i2cl_reg (1, OFC1_MIX1_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX1);
put_i2cl_reg (1, OFC1_REG_CLx1_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL21);

put_i2cl_reg (1, OFC1_REG_CLx2_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL22);
put_i2cl_reg (1, OFC1_REG_CLx1_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL31);
put_i2cl_reg (1, OFC1_REG_CLx2_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_CL32);
put_i2cl_reg (1, OFC1_REG_GAIx_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAI2);
put_i2cl_reg (1, OFC1_REG_WIPE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_WIPE);
put_i2cl_reg (1,2, ofc1_write_slaveaddress | OFC1_REG_SBOT);
put_i2cl_reg (1, OFC1_REG_GAIx_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAI3);
put_i2cl_reg (1,OFC1_REG_GAIx_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GAI4);
put_i2cl_reg (1, OFC1_MIX2_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX2);
put_i2cl_reg (1,0x1, ofc1_write_slaveaddress | OFC1_REG_IVAL);

put_i2cl_reg (1, OFC1_REG_VBPS_60HZ_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_VBPS);
put_i2cl_reg (1, OFC1_REG_VBPR_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_VBPR);
put_i2cl_reg (1, OFC1_REG_ADCG_COMPOSITE_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_ADCG);

put_i2cl_reg (1, OFC1_REG_MIX3_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX3);
put_i2cl_reg (1, 0x2, ofc1_write_slaveaddress | OFC1_REG_WVAL);
put_i2cl_reg (1, OFC1_REG_MIX4_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_MIX4);
put_i2cl_reg (1, OFC1_REG_GUDL_DEFAULT, ofc1_write_slaveaddress | OFC1_REG_GUDL);

#else

p8584_send_I2C (p8584, OFC1_REG_IDEL_DEFAULT, (OFC1_WRITE_SLAVEADDRESS),  OFC1_REG_IDEL);
p8584_send_I2C (p8584, 0x20, (OFC1_WRITE_SLAVEADDRESS ), OFC1_REG_HSYB);
p8584_send_I2C (p8584, 0x0, (OFC1_WRITE_SLAVEADDRESS ), OFC1_REG_HSYS);
p8584_send_I2C (p8584, 0xdf, (OFC1_WRITE_SLAVEADDRESS ), OFC1_REG_HCLB);
p8584_send_I2C (p8584, 0xad, (OFC1_WRITE_SLAVEADDRESS ), OFC1_REG_HCLS);

p8584_send_I2C (p8584, OFC1_REG_HPHI_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_HPHI);
p8584_send_I2C (p8584, OFC1_REG_LUMA_COMPOSITE_DEFAULT|0x2, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_LUMA);
p8584_send_I2C (p8584, OFC1_REG_HUEC_DEFAULT, OFC1_WRITE_SLAVEADDRESS  ,  OFC1_REG_HUEC);
p8584_send_I2C (p8584, OFC1_REG_CKTQ_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CKTQ);
p8584_send_I2C (p8584, OFC1_REG_CKTS_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CKTS);

   
p8584_send_I2C (p8584, OFC1_REG_PLSE_GAL_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_PLSE);
p8584_send_I2C (p8584, OFC1_REG_SESE_GAL_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_SESE);
p8584_send_I2C (p8584, OFC1_REG_GAIN_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_GAIN);
p8584_send_I2C (p8584, OFC1_REG_STDC_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_STDC);
p8584_send_I2C (p8584, OFC1_REG_IOCK_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_IOCK);

p8584_send_I2C (p8584, OFC1_REG_CTL3_GAL_DEFAULT|0x10, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CTL3);
p8584_send_I2C (p8584, OFC1_REG_CTL4_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CTL4);
p8584_send_I2C (p8584, OFC1_REG_CHCV_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CHCV);
p8584_send_I2C (p8584, OFC1_REG_SATN_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_SATN);
p8584_send_I2C (p8584, OFC1_REG_CONT_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CONT);

p8584_send_I2C (p8584, 0x28, OFC1_WRITE_SLAVEADDRESS  ,  OFC1_REG_HS6B);
p8584_send_I2C (p8584, 0x2,  OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_HS6S );
p8584_send_I2C (p8584,  0xf0, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_HC6B );
p8584_send_I2C (p8584,  0xc8, OFC1_REG_HC6S  , OFC1_WRITE_SLAVEADDRESS );
p8584_send_I2C (p8584,OFC1_REG_HP6I_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_HP6I );

p8584_send_I2C (p8584, OFC1_REG_BRIG_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_BRIG);
p8584_send_I2C (p8584, OFC1_ACT1_COMPOSITE_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_ACT1);
p8584_send_I2C (p8584, OFC1_ACT2_COMPOSITE_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_ACT2 );
p8584_send_I2C (p8584, OFC1_MIX1_COMPOSITE_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_MIX1);
p8584_send_I2C (p8584, OFC1_REG_CLx1_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CL21);

p8584_send_I2C (p8584, OFC1_REG_CLx2_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CL22);
p8584_send_I2C (p8584, OFC1_REG_CLx1_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CL31);
p8584_send_I2C (p8584, OFC1_REG_CLx2_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_CL32);
p8584_send_I2C (p8584, OFC1_REG_GAIx_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_GAI2);
p8584_send_I2C (p8584, OFC1_REG_WIPE_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_WIPE);
p8584_send_I2C (p8584,2, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_SBOT);
p8584_send_I2C (p8584, OFC1_REG_GAIx_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_GAI3);
p8584_send_I2C (p8584,OFC1_REG_GAIx_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_GAI4);
p8584_send_I2C (p8584, OFC1_MIX2_COMPOSITE_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_MIX2);
p8584_send_I2C (p8584,0x1, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_IVAL);

p8584_send_I2C (p8584, OFC1_REG_VBPS_60HZ_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_VBPS);
p8584_send_I2C (p8584, OFC1_REG_VBPR_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_VBPR);
p8584_send_I2C (p8584, OFC1_REG_ADCG_COMPOSITE_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_ADCG);

p8584_send_I2C (p8584, OFC1_REG_MIX3_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_MIX3);
p8584_send_I2C (p8584, 0x2, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_WVAL);
p8584_send_I2C (p8584, OFC1_REG_MIX4_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_MIX4);
p8584_send_I2C (p8584, OFC1_REG_GUDL_DEFAULT, OFC1_WRITE_SLAVEADDRESS  , OFC1_REG_GUDL);

#endif

}

void
read_iic ( )
{
}

__uint32_t
cos2_gpsw ( )
{
    __uint32_t iter = 0xf, flag = 1;
    UBYTE val8;


	val8 = mcu_READ(DDRE);
	val8 = val8 & (~0x20);
	mcu_WRITE(DDRE, val8);

	val8 = mcu_READ(PEPAR);
	val8 = val8 & (~0x20);
	mcu_WRITE(PEPAR, val8);

	msg_printf(SUM, " GPSW Test in Progress \n");
    put_i2cl_reg (1, 0, (ofc1_write_slaveaddress | OFC1_REG_MIX3) );

    while ((iter--) && (flag)) {

	DelayFor(1000);
	msg_printf(VRB, "iteration at %x \n", iter);

	put_i2cl_reg(1, 1, (ofc1_write_slaveaddress | OFC1_REG_IOCK) );
	us_delay(10000);
	val8 = mcu_READ(PORTE);
	if ((val8 & 0x20) != 0x20) {
	msg_printf(SUM, " Failed on Set : PORTE %x iteration %x \n", val8, iter);
	flag = 0;
	}

	put_i2cl_reg(1, 0, (ofc1_write_slaveaddress | OFC1_REG_IOCK) );
	val8 = mcu_READ(PORTE);
	if ((val8 & 0x20) != 0) {
	   msg_printf(SUM, " Failed on clear : PORTE %x iteration %x\n", val8, iter);
	   flag = 0;
        }
   }

	if (flag == 1)
	msg_printf(SUM, " GPSW Test Passed \n" );
	else {
	msg_printf(SUM, " GPSW Test Failed \n");
	G_errors++ ;
	return (0);
	}


	return(1);
}

