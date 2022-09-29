/*			- INT6811.H -

   This file defines the interrupt vector addresses of the 68HC11
   and appropriate function names that can be used with the interrupts.
   It is assumed that the segment INTVEC is located at address 0xFFD6.

   Version: 3.10 [IANR]

*/

#pragma language=extended

#define	INTVEC_START	 0		/* Default for 68HC11 (must be matched
					   to the value used at link-time)  */


		/*=======================*/
		/* Interrupt Definitions */
		/*=======================*/

		/* SCI Serial Communication Interface */
interrupt [INTVEC_START +  0] void SCI_interrupt(void);

		/* SPI Serial Transfer Complete */
interrupt [INTVEC_START +  2] void SPI_interrupt(void);

		/* Pulse Accumulator Input Edge */
interrupt [INTVEC_START +  4] void PAIE_interrupt(void);

		/* Pulse Accumulator Overflow */
interrupt [INTVEC_START +  6] void PAO_interrupt(void);

		/* Timer Overflow */
interrupt [INTVEC_START +  8] void TO_interrupt(void);

		/* Timer Output Compare 5 */
interrupt [INTVEC_START + 10] void TOC5_interrupt(void);

		/* Timer Output Compare 4 */
interrupt [INTVEC_START + 12] void TOC4_interrupt(void);

		/* Timer Output Compare 3 */
interrupt [INTVEC_START + 14] void TOC3_interrupt(void);

		/* Timer Output Compare 2 */
interrupt [INTVEC_START + 16] void TOC2_interrupt(void);

		/* Timer Output Compare 1 */
interrupt [INTVEC_START + 18] void TOC1_interrupt(void);

		/* Timer Input Compare 3 */
interrupt [INTVEC_START + 20] void TIC3_interrupt(void);

		/* Timer Input Compare 2 */
interrupt [INTVEC_START + 22] void TIC2_interrupt(void);

		/* Timer Input Compare 1 */
interrupt [INTVEC_START + 24] void TIC1_interrupt(void);

		/* Real Time Interrupt */
interrupt [INTVEC_START + 26] void RTI_interrupt(void);

		/* Interrupt ReQuest */
interrupt [INTVEC_START + 28] void IRQ_interrupt(void);

		/* eXtended Interrupt ReQuest */
interrupt [INTVEC_START + 30] void XIRQ_interrupt(void);

		/* SoftWare Interrupt */
interrupt [INTVEC_START + 32] void SWI_interrupt(void);

		/* Illegal Opcode Trap */
interrupt [INTVEC_START + 34] void IOT_interrupt(void);

		/* COP Failure */
interrupt [INTVEC_START + 36] void NOCOP_interrupt(void);

		/* COP Monitor Failure */
interrupt [INTVEC_START + 38] void CME_interrupt(void);
