/*			- IO6811.H -

   This file #defines the internal register addresses.

   Version: 3.10 [IANR]

*/

#ifndef HC_REG_OFFSET
#define	HC_REG_OFFSET 0x1000		/* Default for 68HC11 */
#endif

		/*======================*/
		/* I/O Port Definitions */
		/*======================*/

		/* I/O port A */
#define PORTA (* (unsigned char *) (HC_REG_OFFSET + 0))

		/* Data Direction Port A ****BLM is this Correct******/
#define DDRA (* (unsigned char *) (HC_REG_OFFSET + 7))

		/* Parallel I/O Control Register */
#define PIOC (* (unsigned char *) (HC_REG_OFFSET + 2))

		/* I/O Port C */
#define PORTC (* (unsigned char *) (HC_REG_OFFSET + 3))

		/* I/O Port B */
#define PORTB (* (unsigned char *) (HC_REG_OFFSET + 4))

		/* Alternate Latched Port C */
#define PORTCL (* (unsigned char *) (HC_REG_OFFSET + 5))

		/* Data Direction Port C */
#define DDRC (* (unsigned char *) (HC_REG_OFFSET + 7))

		/* I/O Port D */
#define PORTD (* (unsigned char *) (HC_REG_OFFSET + 8))

		/* Data Direction Port D */
#define DDRD (* (unsigned char *) (HC_REG_OFFSET + 9))

		/* I/O Port E */
#define PORTE (* (unsigned char *) (HC_REG_OFFSET + 0xA))

		/* Compare Force Register */
#define CFORC (* (unsigned char *) (HC_REG_OFFSET + 0xB))

		/* OC1 Action Mask Register */
#define OC1M (* (unsigned char *) (HC_REG_OFFSET + 0xC))

		/* OC1 Action Data Register */
#define OC1D (* (unsigned char *) (HC_REG_OFFSET + 0xD))

		/* Timer Counter Register */
#define TCNT (* (unsigned int *) (HC_REG_OFFSET + 0xE))

		/* Input Capture Register 1 */
#define TIC1 (* (unsigned int *) (HC_REG_OFFSET + 0x10))

		/* Input Capture Register 2 */
#define TIC2 (* (unsigned int *) (HC_REG_OFFSET + 0x12))

		/* Input Capture Register 3 */
#define TIC3 (* (unsigned int *) (HC_REG_OFFSET + 0x14))

		/* Output Compare Register 1 */
#define TOC1 (* (unsigned int *) (HC_REG_OFFSET + 0x16))

		/* Output Compare Register 2 */
#define TOC2 (* (unsigned int *) (HC_REG_OFFSET + 0x18))

		/* Output Compare Register 3 */
#define TOC3 (* (unsigned int *) (HC_REG_OFFSET + 0x1A))

		/* Output Compare Register 4 */
#define TOC4 (* (unsigned int *) (HC_REG_OFFSET + 0x1C))

		/* Output Compare Register 5 */
#define TOC5 (* (unsigned int *) (HC_REG_OFFSET + 0x1E))

		/* Timer Control Register 1 */
#define TCTL1 (* (unsigned char *) (HC_REG_OFFSET + 0x20))

		/* Timer Control Register 2 */
#define TCTL2 (* (unsigned char *) (HC_REG_OFFSET + 0x21))

		/* Timer Interrupt Mask Register 1 */
#define TMSK1 (* (unsigned char *) (HC_REG_OFFSET + 0x22))

		/* Timer Interrupt Flag Register 1 */
#define TFLG1 (* (unsigned char *) (HC_REG_OFFSET + 0x23))

		/* Timer Interrupt Flag Register 2 */
#define TMSK2 (* (unsigned char *) (HC_REG_OFFSET + 0x24))

		/* Timer Interrupt Flag Register 2 */
#define TFLG2 (* (unsigned char *) (HC_REG_OFFSET + 0x25))

		/* Pulse Accumulator Control Register */
#define PACTL (* (unsigned char *) (HC_REG_OFFSET + 0x26))

		/* Pulse Accumulator Count Register */
#define PACNT (* (unsigned char *) (HC_REG_OFFSET + 0x27))

		/* SPI Control Register */
#define SPCR (* (unsigned char *) (HC_REG_OFFSET + 0x28))

		/* SPI Status Register */
#define SPSR (* (unsigned char *) (HC_REG_OFFSET + 0x29))

		/* SPI Data Register */
#define SPDR (* (unsigned char *) (HC_REG_OFFSET + 0x2A))

		/* SCI Baud Rate Control */
#define BAUD (* (unsigned char *) (HC_REG_OFFSET + 0x2B))

		/* SCI Control Register 1 */
#define SCCR1 (* (unsigned char *) (HC_REG_OFFSET + 0x2C))

		/* SCI Control Register 2 */
#define SCCR2 (* (unsigned char *) (HC_REG_OFFSET + 0x2D))

		/* SCI Status Register */
#define SCSR (* (unsigned char *) (HC_REG_OFFSET + 0x2E))

		/* SCI Data Register */
#define SCDR (* (unsigned char *) (HC_REG_OFFSET + 0x2F))

		/* A/D Control Register */
#define ADCTL (* (unsigned char *) (HC_REG_OFFSET + 0x30))

		/* A/D Result Register 1 */
#define ADR1 (* (unsigned char *) (HC_REG_OFFSET + 0x31))

		/* A/D Result Register 2 */
#define ADR2 (* (unsigned char *) (HC_REG_OFFSET + 0x32))

		/* A/D Result Register 3 */
#define ADR3 (* (unsigned char *) (HC_REG_OFFSET + 0x33))

		/* A/D Result Register 4 */
#define ADR4 (* (unsigned char *) (HC_REG_OFFSET + 0x34))

		/* EEPROM Block Protect Register */
#define BPROT (* (unsigned char *) (HC_REG_OFFSET + 0x35))

		/* System Configuration Options */
#define OPTION (* (unsigned char *) (HC_REG_OFFSET + 0x39))

		/* Arm/reset COP Timer Circuitry */
#define COPRST (* (unsigned char *) (HC_REG_OFFSET + 0x3A))

		/* EEPROM Programming Control Register */
#define PPROG (* (unsigned char *) (HC_REG_OFFSET + 0x3B))

		/* Highest Priority I-bit Interrupt And Misc. */
#define HPRIO (* (unsigned char *) (HC_REG_OFFSET + 0x3C))

		/* RAM -I/O Mapping Register */
#define INIT (* (unsigned char *) (HC_REG_OFFSET + 0x3D))

		/* Factory TEST Control Register */
#define TEST1 (* (unsigned char *) (HC_REG_OFFSET + 0x3E))

		/* COP, ROM & EEPROM Enables */
#define CONFIG (* (unsigned char *) (HC_REG_OFFSET + 0x3F))

