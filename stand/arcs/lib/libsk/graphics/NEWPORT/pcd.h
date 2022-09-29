/* #defines for Indy Presenter I2C controls */

#define DCB_PCD8584             12

#define PCD8584_DATACRS		0
#define PCD8584_CTRLCRS		1

#define PCD8584_OWNADR 		0x7f

#define PCD8584_CLK3MHZ		0x00
#define PCD8584_CLK4MHZ		0x10
#define PCD8584_CLK6MHZ		0x14
#define PCD8584_CLK8MHZ		0x18
#define PCD8584_CLK12MHZ	0x1C
#define PCD8584_BAUD90KHZ	0x00
#define PCD8584_BAUD45KHZ	0x01
#define PCD8584_BAUD11KHZ	0x02
#define PCD8584_BAUD1KHZ	0x03

#define PCD8584_DATA_WRITE	0x00
#define PCD8584_DATA_READ	0x01

#define PCD8584_CTL_PIN 	0x80
#define PCD8584_CTL_ES0 	0x40
#define PCD8584_CTL_ES1 	0x20
#define PCD8584_CTL_ES2 	0x10
#define PCD8584_CTL_ENI 	0x08
#define PCD8584_CTL_STA 	0x04
#define PCD8584_CTL_STO 	0x02
#define PCD8584_CTL_ACK  	0x01

#define PCD8584_STAT_PIN 	0x80
#define PCD8584_STAT_STS 	0x20
#define PCD8584_STAT_BER 	0x10
#define PCD8584_STAT_LRB 	0x08
#define PCD8584_STAT_AAS 	0x04
#define PCD8584_STAT_LAB 	0x02
#define PCD8584_STAT_BB  	0x01

#define PCD8584_ADRREG 		0
#define PCD8584_CLKREG 		PCD8584_CTL_ES1
#define PCD8584_CSDREG 		PCD8584_CTL_ES0

#define PCD8584_REGTIME		5     /* big enough for about 200ns */
#define PCD8584_XFRTIME		100   /* big enough for about 200us */
#define PCD8584_STOPTIME	10    /* big enough for about 20us */

#define PCD8574_OWNADR		0x70
#define PCD8574_IDSHIFT		5
#define PCD8574_IDMASK		0x07
#define PCD8574_ID		0xe0
#define PCD8574_UP		0x04
#define PCD8574_DOWN		0x00
#define PCD8574_INCNOT		0x02
#define PCD8574_INC		0x00
#define PCD8574_CSNOT		0x01
#define PCD8574_CS		0x00
#define PCD8574_OFF		0x18
#define PCD8574_ON		0x00
#define PCD8574_BIT		0x08

#if 0 /* flat panels not used in corona */
#define PANEL_XGA_SHARP		0x0f
#define PANEL_XGA_NEC		0x0e
#endif
#define PANEL_XGA_MITSUBISHI    0x03

#define PCD8425_OWNADR          0x82
#define PCD8425_VLADR		0x00
#define PCD8425_VRADR		0x01
#define PCD8425_BSADR		0x02
#define PCD8425_TRADR		0x03
#define PCD8425_SWADR		0x08







