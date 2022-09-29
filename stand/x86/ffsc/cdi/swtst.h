/*
** =======================================================================
**
**  Header Name     :   swtst.h
**
**  Description     :   Constant declarations
**
**  Author          :   Kenneth J. Smith, fixed for vxWorks
**						by Jeff Becker, SGI
**
**  Creation Date   :   July 2, 1996
**
**  Compiler Env.   :   Borland C++
**
**                    (c) Copyright Lines Unlimited
**                        1996 All Rights Reserved
**
** =======================================================================
*/

#define		SWITCH_INT		5
#define		SWITCH_SVEC		37


#define		SW1				0x3e
#define		SW2				0x3d
#define		SW3				0x3b
#define		SW4				0x37
#define		SW5				0x2f
#define		SW6				0x1f

#define		SWITCH_MASK		0x003f
#define		SWITCH_PORT		0x2D0
#define		NUM_SWITCHES	6

#define		IER				0x01
#define		MCR				0x04

#define		INT_ENABLE 		0x08

#define		ICR_8259 		0x20
#define		IMR_8259 		0x21

