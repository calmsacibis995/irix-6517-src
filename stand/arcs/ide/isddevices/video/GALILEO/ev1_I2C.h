#define I2C_NOT_IDLE 0x1
#define I2C_ERR	(1 << 7)
#define I2C_NOT_ACK (1 << 5)
#define I2C_XFER_BUSY (1 << 4)

#define I2C_FORCE_IDLE  (0 << 0)

#define I2C_WRITE       (0 << 1)
#define I2C_READ        (1 << 1)

#define I2C_RELEASE     (0 << 2)
#define I2C_HOLD        (1 << 2)

#define I2C_64          (0 << 3)        /* NTSC */
#define I2C_128         (1 << 3)        /* PAL */

#define I2C_OK          (0 << 7)
#define I2C_ERR         (1 << 7)


#define MAX_IDLE_WAIT 	2000
#define MAX_ACK_WAIT  	10000
#define MAX_XFER_WAIT 	2000

#define VID_READ 0
#define VID_WRITE 1

#define DENC_ADDR 0xB0
#define DMSD_ADDR 0x8A
#define CSC_ADDR 0xE0
#define DAC_ADDR 0x48    
#define D1_ADDR         0x8E 

#define BUS_CONV_ADDR      0x40

