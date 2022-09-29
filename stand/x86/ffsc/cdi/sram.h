/*
** =======================================================================
**
**  Header Name     :   sram.h
**
**  Project Name    :   SBC-FFSC
**
**  Module Synopsis :   Header file for sram.c.
**
**  Author          :   Kenneth J. Smith
**
**  Creation Date   :   May 24, 1996
**
**  Compiler Env.   :   GNU C
**
**                    (c) Copyright Lines Unlimited
**                        1996 All Rights Reserved
**
** =======================================================================
*/

#ifndef LIST_H
  #define LIST_H

#define SIGNATURE		0xAA55

#define VGA_ENABLE		0x540
#define VGA_DISABLE		0x548

#define WRITE_STR		0x00

#define PORT_70			0x70
#define PORT_71			0x71

#define SRAM_START		0xA2000
#define PAGE_SELECT		0x760
#define PAGE_0			0x81

#define NOT_INITIALIZED	-3
#define CHECKSUM_FAILED	-2
#define ERR         	-1
#define VALID_READ     	1
#define VALID_WRITE    	1

struct ram_data
  {
    short signature;
    char reserve;
    char loader_flag;
	unsigned int checksum;
  };

typedef struct ram_data sram_data;

#define BUFF_SIZE		    (( 1024 * 8 ) - sizeof( struct ram_data ))

#endif  /* LIST_H */
