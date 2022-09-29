#include "cosmo_hw.h"
#include "cosmo_ide.h"
#include "cosmo_7186.h"
#include <libsk.h>

#define GIO_BUS_DELAY	15
#define S7186_DELAY	2000
#define DELAYBUS(n)	us_delay(n)

unsigned i2c_error = 0;
SAAREG SAA7186_register[] = {
/**
** HORIZANTAL CONTROL
**/
SUB_FMT_SEQ,		(RTB_DEFAULT << 7) | 
			(OFx_DEFAULT << 5) | 
			(VPE_DEFAULT << 4) | 
			(LWx_DEFAULT << 2) | 
			FS0_1_DEFAULT,
SUB_PIXLINE_OUT,	XDx_DEFAULT,
SUB_PIXLINE_IN,		XSx_DEFAULT,
SUB_HSTART,		XOx_DEFAULT,
SUB_PIXDEC_FIL,		(HFx_DEFAULT << 5) | 
			(XO8_DEFAULT << 4) | 
			(XS89_DEFAULT << 2) | 
			XD89_DEFAULT ,
/**
** VERTICAL CONTROL
**/
SUB_LINEFIELD_OUT,	YDx_DEFAULT,
SUB_LINEFIELD_IN,	YSx_DEFAULT,
SUB_VSTART,		YOx_DEFAULT,
SUB_AFSV,		(AFS_DEFAULT << 7) | 
			(VPx_DEFAULT << 5) | 
			(YO8x_DEFAULT << 4) | 
			(YS89_DEFAULT << 2) | 
			YD89_DEFAULT ,
/**
** BYPASS CONTROL
**/
SUB_VBYPASS,		VSx_DEFAULT,
SUB_VBYPASS_COUNT,	VCx_DEFAULT,
SUB_SUB_VBYPASS1,	( TCC_DEFAULT << 7)   |/* bit wrong in data book */
			( VS8_DEFAULT << 4)   | 
			( VC8_DEFAULT << 2)   | 
			POE_DEFAULT ,
/**
** CHROMA KEY CONTROL
**/
SUB_VL,			VLx_DEFAULT,
SUB_VU,			VUx_DEFAULT,
SUB_UL,			ULx_DEFAULT,
SUB_UU,			UUx_DEFAULT,
SUB_10bit,		(MCT_DEFAULT << 4) | 
			(QPL_DEFAULT << 3) | 
			(QPP_DEFAULT << 2) | 
			(TTR_DEFAULT << 1) | 
			EFE_DEFAULT ,
};

static int write_I2C (JPEG_8584_REG *i2c, long Value, enum I2C_mode mode);
static int _jpeg_send_I2C(JPEG_8584_REG *i2c, long addr, long subaddr, long data);
static int _jpeg_read_I2C_byte (JPEG_8584_REG *i2c, long addr, long subaddr);

void
cosmo_check_bits(JPEG_8584_REG *i2cv, unsigned reg_to_check, unsigned reg_bits)
{
	char *self = "cosmo_check_bits";
	unsigned i, j;
	long regvalue = 0;

	msg_printf(VRB, "%s: checking 0x%x, these bits 0x%x\n", self, 
					reg_to_check, reg_bits);
	for ( i=0, j=1; i<8; i++ ) {
		if ( reg_bits & j ) {
			/* Select the register */
			write_I2C(i2cv, reg_to_check, I2C_CTL);
			/* write out the bit */
			write_I2C(i2cv, j, I2C_DATA_NP);
			regvalue = (volatile long) i2cv->jpeg8584_data;
			if ( (regvalue&reg_bits) != j ) {
				msg_printf(DBG, "register 0x%x is 0x%x",
					reg_to_check, (regvalue&reg_bits));
				msg_printf(DBG, ", should be 0x%x\n", j);
				i2c_error++;
			}
		}
		j = (j<<1);
	}
}

cosmo_I2C_reg(int argc, char **argv)
{
	char *self = "cosmo_I2C_reg";
	volatile long  *i2cv;
	JPEG_8584_REG *i2c;
	unsigned i, status;
	long	regvalue = 0;
	int addr = SAA7186_I2C_ADDR_W;
	char sav_err_subaddr[SUBADDRESS];

	if ( cosmo_probe_for_it() == -1 ) {
		return(-1);
	}
	cosmo_board_reset();
	cosmo_dma_reset(ALL_DMA_RESETS);
	i2cv = (long *)cosmo_regs.jpeg_8584_SAA7;
	i2c = (JPEG_8584_REG *)i2cv;
	msg_printf(INFO, "%s: Initializing 8584\n", self);
	msg_printf(VRB, "%s: i2cv==0x%x\n", self, i2cv);

	/* document says write to s0' first, so we will try */
	write_I2C(i2c, JPEG_8584_SLAVE_ADDR, I2C_DATA_NP);

	/* Check out the bits in the OWNADDR register */
	cosmo_check_bits(i2c,JPEG_8584_OWNADDR_REG,0x7f);

	/* Select own address register  (s0') */
	write_I2C(i2c, JPEG_8584_OWNADDR_REG, I2C_CTL);

	/* Set 8584 I2C slave address. */
	/* slave mode never used on JPEG board now. */
	write_I2C(i2c, JPEG_8584_SLAVE_ADDR, I2C_DATA_NP);
	regvalue = (volatile long) i2c->jpeg8584_data;
	if ( (regvalue&0xff) != JPEG_8584_SLAVE_ADDR ) {
		msg_printf(DBG, "OWNADDR_REG  0x%x, should be 0x%x\n",
				(regvalue&0xff), JPEG_8584_SLAVE_ADDR);
		i2c_error++;
	} else {
		msg_printf(DBG, "OWNADDR_REG (0x%x) OK\n",
					JPEG_8584_SLAVE_ADDR);
	}

	/* Check out the bits in the CLK register */
	cosmo_check_bits(i2c,JPEG_8584_CLK_REG,0x1f);

	/* Select clock speed register. */
	write_I2C(i2c, JPEG_8584_CLK_REG, I2C_CTL);

	/* Set clock speed to SCL==90kHz, prescaler 4.43 MHz. */
	write_I2C(i2c, JPEG_8584_CLK_SPEED, I2C_DATA_NP);
	regvalue = (volatile long) i2c->jpeg8584_data;
	if ( (regvalue&0x1f) != JPEG_8584_CLK_SPEED ) {
		msg_printf(DBG, "CLK_REG 0x%x, should be 0x%x\n",
				(regvalue&0x1f), JPEG_8584_CLK_SPEED);
		i2c_error++;
	} else {
		msg_printf(DBG, "CLK_REG (0x%x) OK\n",
					JPEG_8584_CLK_SPEED);
	}

	/* Check out the bits in the INT_VECT register */
	cosmo_check_bits(i2c,JPEG_8584_INT_VECT_REG,0xff);

	/* Select interrupt vector register. */
	/* no interrupts yet on JPEG board. */
	write_I2C(i2c, JPEG_8584_INT_VECT_REG, I2C_CTL);

	/* Set interrupt vector */
	write_I2C(i2c, JPEG_8584_INT_VECT_80XX, I2C_DATA_NP);
	regvalue = (volatile long) i2c->jpeg8584_data;
	if ( (regvalue&0xff) != JPEG_8584_INT_VECT_80XX ) {
		msg_printf(DBG, "INT_VECT_REG 0x%x, should be 0x%x\n",
				(regvalue&0xff), JPEG_8584_INT_VECT_80XX);
		i2c_error++;
	} else {
		msg_printf(DBG, "INT_VECT_REG (0x%x) OK\n",
					JPEG_8584_INT_VECT_80XX);
	}

	msg_printf(INFO, "%s: Initializing 7186 Scaler\n", self, i2c);
	for(i=0;i<SUBADDRESS;i++) {
		DELAYBUS(S7186_DELAY);
		if ( _jpeg_send_I2C(i2c, addr,SAA7186_register[i].subaddr,
			SAA7186_register[i].data) != STATUS_OK ) {
				sav_err_subaddr[i2c_error] = 
					SAA7186_register[i].subaddr;
				msg_printf(VRB, "%s: SUBADDRESS==0x%x",
						self, i);
				msg_printf(VRB, " failed write over I2C bus\n");
				i2c_error++;
		}
	}
	DELAYBUS(S7186_DELAY);
	status = _jpeg_read_I2C_byte(i2c,addr,0);
	msg_printf(DBG, "Status byte from 7186 == 0x%x\n", status&0xff);
	if ( i2c_error ) {
		msg_printf(SUM, "%s: test failed\n", self);
		return(-1);
	} else {
		msg_printf(SUM, "%s: test passed\n", self);
		return(0);
	}
}
/*
 * Determine if the previous I2C bus operation has terminated.
 * Return an indication if we timed out so that the caller can
 * reset.
 */
static int
poll_I2C (char *str, JPEG_8584_REG *i2c)
{
	char	*self = "poll_I2C()";
	int	count;
	int	timeout = JPEG_I2C_TIMEOUT;
	static int non_expire_count;
	static int expire_count;
	long	reg;

	/* Loop waiting for good I2C status, or timeout. */
	while ((reg = (volatile long)i2c->jpeg8584_ctrl_stat) &
	    JPEG_8584_SERIAL_BUSY) {
		if (--timeout == 0) {
			msg_printf(VRB, "%s: 8584 BUSY timeout (reg==0x%x",
							self, reg);
			msg_printf(VRB, " at 0x%x)\n",
				(int)&i2c->jpeg8584_ctrl_stat);
			break;
		}
		DELAYBUS (GIO_BUS_DELAY);
	}

	/* The serial transfer is no longer busy, or timeout expired. */
	if (timeout == 0) {
		expire_count++;
		return (STATUS_I2C_TIMEOUT);
	}
#ifdef notdef
	msg_printf(VRB, "%s: 8584 BUSY did not timeout (reg==0x%x",
					self, reg);
	msg_printf(VRB, " at 0x%x)\n",
		(int)&i2c->jpeg8584_ctrl_stat);
#endif /* notdef */
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
static int
write_I2C (JPEG_8584_REG *i2c, long Value, enum I2C_mode mode)
{
	char	*self = "write_I2C";
	int	status;

	switch (mode) {

	case I2C_DATA:
		if ((status = poll_I2C(self, i2c)) != STATUS_OK) {
			msg_printf(VRB, "%s: previous poll failed on I2C_DATA\n",
						self);
			return (status);
		}
	case I2C_DATA_NP:
		DELAYBUS (GIO_BUS_DELAY);
		i2c->jpeg8584_data = Value;
		DELAYBUS (GIO_BUS_DELAY);
		break;
	
	case I2C_CTL:
		DELAYBUS (GIO_BUS_DELAY);
		i2c->jpeg8584_ctrl_stat = Value;
		DELAYBUS (GIO_BUS_DELAY);
		break;
	
	default:
		msg_printf(ERR, "%s: bad mode %d\n", self, mode);
		return (-1);
	}

	return (STATUS_OK);
}

/*
 * Send a byte to an I2C bus slave by setting up the address
 * and subaddress of the slave, and then sending the byte,
 * checking for timeout.
 */
static int
_jpeg_send_I2C(JPEG_8584_REG *i2c, long addr, long subaddr, long data)
{
	char	*self = "_jpeg_send_I2C";
	int	status;

	/* Set up the address of the I2C slave. */
	write_I2C(i2c, JPEG_8584_S1_ESO, I2C_CTL);
	write_I2C(i2c, JPEG_I2C_WRITE(addr), I2C_DATA_NP);

	/* Set up the subaddress of the I2C slave. */
	if ((status = 
		write_I2C(i2c, JPEG_8584_SEND_START_PLUS_ADDR, I2C_CTL)) 
								!= STATUS_OK) {
		msg_printf(VRB, "%s: write to 8584 START_PLUS_ADDR failed\n",
							self);
		return (status);
	}
	DELAYBUS (GIO_BUS_DELAY);

	if ((status = write_I2C(i2c, subaddr, I2C_DATA)) != STATUS_OK) {
		msg_printf(VRB, "%s: write to 8584 for SUBADDR 0x%x failed\n",
							self, subaddr);
		return (status);
	}

	DELAYBUS (GIO_BUS_DELAY);
	/* Send the data to the slave. */
	if ((status = write_I2C(i2c, data, I2C_DATA)) != STATUS_OK) {
		msg_printf(VRB, "%s: write to 8584 SUBADDR 0x%x failed\n",
							self, subaddr);
		return (status);
	}
	if ((status =
	    poll_I2C("_jpeg_send_I2C poll", i2c)) != STATUS_OK) {
		msg_printf(VRB, "%s: Poll for I2C complete failed\n",
							self);
		return (status);
	}

	/* Set STOP. */
	write_I2C(i2c, JPEG_8584_SEND_STOP, I2C_CTL);
	return (STATUS_OK);
}

static int
_jpeg_read_I2C_byte (JPEG_8584_REG *i2c,
			long addr,
			long subaddr)
{
	char	*self = "_jpeg_read_I2C_byte";
	long	regvalue = 0;
	int	status;
	
	msg_printf(DBG, "%s: (0x%x) addr 0x%x, subaddr 0x%x\n",
					self, i2c, addr, subaddr);
	write_I2C (i2c, JPEG_8584_S1_ESO, I2C_CTL);
	write_I2C (i2c, addr | 0x1, I2C_DATA_NP);
	write_I2C (i2c, JPEG_8584_SEND_START_PLUS_ADDR, I2C_CTL);
	/* if valid subaddr, then send that next */
	if ( subaddr <= JPEG_I2C_MAX_SUBADDR ) {
		if ((status = write_I2C(i2c, subaddr, I2C_DATA)) != 
							STATUS_OK) {
			return (status);
		}
	}
	write_I2C (i2c, JPEG_8485_MASTER_RECV, I2C_CTL);
	if ((poll_I2C (self, i2c)) != STATUS_OK) {
		msg_printf(ERR, "%s: bad poll\n", self);
	}
	if ((status = poll_I2C(self, i2c)) != STATUS_OK) {
		msg_printf(ERR, "%s: MASTER_RECV poll failed\n", self);
		return (status);
	}
	DELAYBUS (600);
	regvalue = (volatile long) i2c->jpeg8584_data;
	DELAYBUS (20000);
	write_I2C (i2c, JPEG_8584_SEND_STOP, I2C_CTL);
	msg_printf(DBG, "reg value 0x%x\n", regvalue);

	return (regvalue&0xff);
}

/*
 * Init the pcd8584 chip, own address, clock speed,  and interrupt vector.
 */
int pcd8584_inited = 0;
int
cosmo_init_pcd8584(JPEG_8584_REG *i2c, int firstonly)
{
	char	*self = "jpeg_init_pcd8584";

	if ( firstonly && pcd8584_inited ) {
		return(STATUS_OK);
	}
	msg_printf(DBG, "%s: 0x%x, 0x%x\n", self, i2c, firstonly);
	/* clear I2C reset in dma control register */
	cosmo_dma_reset(IIC_RST_N);

	/* document says write to s0' first, so we will try */
	write_I2C(i2c, JPEG_8584_SLAVE_ADDR, I2C_DATA_NP);

	/* Select own address register  (s0') */
	write_I2C(i2c, JPEG_8584_OWNADDR_REG, I2C_CTL);

	/* Set 8584 I2C slave address. */
	/* slave mode never used on JPEG board now. */
	write_I2C(i2c, JPEG_8584_SLAVE_ADDR, I2C_DATA_NP);

	/* Select clock speed register. */
	write_I2C(i2c, JPEG_8584_CLK_REG, I2C_CTL);

	/* Set clock speed to SCL==90kHz, prescaler 4.43 MHz. */
	write_I2C(i2c, JPEG_8584_CLK_SPEED, I2C_DATA_NP);

	/* Select interrupt vector register. */
	/* no interrupts yet on JPEG board. */
	write_I2C(i2c, JPEG_8584_INT_VECT_REG, I2C_CTL);

	/* Set interrupt vector */
	write_I2C(i2c, JPEG_8584_INT_VECT_80XX, I2C_DATA_NP);

	pcd8584_inited++;
	return (STATUS_OK);
}
/*
 * Init the SAA7186 scaler chip
 */
unsigned s7186_delaybus_value = 0x20;
unsigned s7186init = 0;
int
cosmo_init_7186(void)
{
	char *self = "cosmo_init_7186";
	int     i, addr = SAA7186_I2C_ADDR_W;
	JPEG_8584_REG *i2c = (JPEG_8584_REG *)cosmo_regs.jpeg_8584_SAA7;
	unsigned error = 0;

	/* initialize the pcd8584 first */
	if ( cosmo_init_pcd8584(i2c,0) != STATUS_OK ) {
		return(STATUS_ERR);
	}
	/*
	 * load default registers first
	 */
	msg_printf(DBG, "%s: registers: ", self);
	for(i=0;i<SUBADDRESS;i++) {
		us_delay(s7186_delaybus_value);
		if ( _jpeg_send_I2C(i2c, addr,SAA7186_register[i].subaddr,
		    SAA7186_register[i].data) != STATUS_OK ) {
			error++;
		}
	}
	if ( error ) {
		msg_printf(ERR, "%s: 7186 init failed (0x%x)\n",
					self, error);
		return(STATUS_NOTOK);
	} else {
		return (STATUS_OK);
	}
}
void
cosmo_7186_set_rgbmode(int rgbmode)
{

	int     i, addr = SAA7186_I2C_ADDR_W;
	JPEG_8584_REG *i2c = (JPEG_8584_REG *)cosmo_regs.jpeg_8584_SAA7;
	unsigned status, error = 0;
	int fs0_1;

	if(rgbmode) fs0_1 = 2;
	else fs0_1 = 1;

	i = (RTB_DEFAULT << 7) | (OFx_DEFAULT << 5) | (VPE_DEFAULT << 4) |
	    (LWx_DEFAULT << 2) | fs0_1;
	DELAYBUS(s7186_delaybus_value);
	if (_jpeg_send_I2C(i2c, addr, SUB_FMT_SEQ, i) != STATUS_OK )
		error++;
	DELAYBUS(s7186_delaybus_value);
}
