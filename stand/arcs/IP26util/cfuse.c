#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <uif.h>
#include <libsk.h>
#include <libsc.h>
#include "nvram.h"

/*
 * the version of nvram_cs_on() and nvram_cs_off() we have here is slightly
 * different than the one used in LIB 
 * THEY ARE NOT INTERCHANGEABLE
 */ 

#define	RESET_AUXSERCLK	*cpu_auxctl &= ~SERCLK
#define	SET_AUXSERCLK	*cpu_auxctl |= SERCLK
#define	RESET_PRE	*cpu_auxctl &= ~NVRAM_PRE
#define	SET_PRE		*cpu_auxctl |= NVRAM_PRE

/* Kludge-o-rama  */

int diagcmds;
int ndiagcmds;


int check_enet(void);
void wait(void);
int get_enet(void);
extern char *          cpu_get_nvram_offset(int, int);

#define nvram_cs_on	_nvram_cs_on
#define nvram_cs_off	_nvram_cs_off

/*
 * enable the serial memory by setting the console chip select
 */
void
nvram_cs_on(void)
{
	volatile unsigned char *cpu_auxctl =
		(unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char cs_on;
	int delaylen = 100;

	cs_on = *cpu_auxctl & ~(SERCLK | CONSOLE_CS);
	*cpu_auxctl &= ~CPU_TO_SER;	/* see data sheet timing diagram */
	RESET_AUXSERCLK;
	*cpu_auxctl = cs_on;
	while (--delaylen)
		;
	*cpu_auxctl = cs_on | CONSOLE_CS;
	SET_AUXSERCLK;
}



/*
 * turn off the chip select
 */
void
nvram_cs_off(void)
{
	volatile unsigned char *cpu_auxctl =
		(unsigned char *)PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char cs_off;

	cs_off = *cpu_auxctl & ~(SERCLK | CONSOLE_CS);
	RESET_AUXSERCLK;
	*cpu_auxctl = cs_off;
	SET_AUXSERCLK;
}

/*
 * IP26 standalone utility program to set the NVRAM protect register
 */
void
Fuse(void)
{
	int 				bits_in_cmd = size_nvram ();
	volatile unsigned char		*cpu_auxctl = (unsigned char *)
						PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char 			health_led_off = 
						*cpu_auxctl & LED_GREEN;
	int				uflag = 0;

	/*
	uflag = argc == 2 && strcmp(argv[1], "-u") == 0;
	*/

	RESET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WEN, 0, bits_in_cmd);
	nvram_cs_off();

	SET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_PREN, 0, bits_in_cmd);
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_PRCLEAR, 0xff, bits_in_cmd);
	nvram_cs_off();
	nvram_hold();

	if (!uflag) {
		nvram_cs_on();
		nvram_cmd(SER_PREN, 0, bits_in_cmd);
		nvram_cs_off();

		nvram_cs_on();
		nvram_cmd(SER_PRWRITE, NVFUSE_START / 2, bits_in_cmd);
		nvram_cs_off();
		nvram_hold();
	}

	RESET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WDS, 0, bits_in_cmd);
	nvram_cs_off();

	*cpu_auxctl = (*cpu_auxctl & ~LED_GREEN) | health_led_off;
	
	get_enet();
	us_delay(5000*1000); /* 5 sec delay */
}


/*
 * IP26 standalone utility program to unset the NVRAM protect register
 * and perform nvram tests
 */
void
Unfuse(void)
{
	int 				bits_in_cmd = size_nvram ();
	volatile unsigned char		*cpu_auxctl = (unsigned char *)
						PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char 			health_led_off = 
						*cpu_auxctl & LED_GREEN;

	RESET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WEN, 0, bits_in_cmd);
	nvram_cs_off();

	SET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_PREN, 0, bits_in_cmd);
	nvram_cs_off();

	nvram_cs_on();
	nvram_cmd(SER_PRCLEAR, 0xff, bits_in_cmd);
	nvram_cs_off();
	nvram_hold();

	RESET_PRE;
	nvram_cs_on();
	nvram_cmd(SER_WDS, 0, bits_in_cmd);
	nvram_cs_off();

	*cpu_auxctl = (*cpu_auxctl & ~LED_GREEN) | health_led_off;
}

#define	Dprintf		if(debug)printf

int
check_enet(void)
{
	int nvram_idx;
	char *cp;
	int val1 ;
	char val2;
	int i;
	int debug = 0;
	int errors = 0;

	for ( nvram_idx = NVOFF_ENET; nvram_idx < NVOFF_ENET+6; nvram_idx++) {
		cp = cpu_get_nvram_offset( nvram_idx, 1 );
		val1  = (int)(*cp);
		Dprintf("Orig val :%x\n",val1);
		val2 = (int)(0xff - val1) ;
		Dprintf("Modified val :%x\n",val2);
		cpu_set_nvram_offset( nvram_idx, 1, &val2); /* delay needed after setting ?*/

		for ( i = 0x10000 ; i; i--) 
				;
		cp = cpu_get_nvram_offset( nvram_idx, 1 );
		val2 = (int )(*cp);
		if ( val1 != val2 )  {
			printf("Error in fusing ...%x is modified into %x\n",val1,val2);
			errors ++;
		}

		Dprintf("val1 = %x, val2 = %x \n",val1,val2);

	}
	Dprintf( "\n" );
	return errors;
}

void
wait(void)
{
	char line[80];
	printf(" press <Enter> to continue\n");
	gets(line);

}

int
get_enet(void)
{
	int nvram_idx;
	char *cp;

	printf( "\nEthernet address is ---- " );
	for ( nvram_idx = NVOFF_ENET; nvram_idx < NVOFF_ENET+6; nvram_idx++) {
		cp = cpu_get_nvram_offset( nvram_idx, 1 );
		if ( nvram_idx != NVOFF_ENET+5 )
			printf( "%02x:", (int )(*cp) );
		else
			printf( "%02x", (int )(*cp) );
	
	}
	printf( "\n\n" );
	return( 0 );
}



#define HEXVAL(c) ((('0'<=(c))&&((c)<='9'))? ((c)-'0'):(('A'<=(c))&&((c)<='F'))? \
			((c)-'A'+10)  : ((c)-'a'+10))

int
isxdigit(char ch)
{
  return ((ch >= '0' && ch <= '9')||(ch >= 'a' && ch <= 'f')||(ch >='A' && ch <='F'));
}

int
isdigit(char ch)
{
  return ((ch >= '0' && ch <= '9'));
} 

/*
	Checks for valid eaddr and sets eaddr to the new given value
	Return value: 
		0 for no error
		1 for invalid eaddr
		2 for valid eaddr but not able to set (since it is already fused)
*/
int
PutEnet(char *eaddr)
{
	char dig[6], *cp;
	int i;

	for ( i = 0, cp = eaddr ; *cp; )
		if ( *cp == ':' ) {
			cp++;
			continue;
			}
		else
		if ( !isxdigit(*cp) || !isxdigit(*(cp+1)) )
			return 1;
		else	{
			if ( i >= 6 )
				return 1;
			dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
			cp += 2;
			}
	
	if ( i != 6 )
		return 1;
	for ( i = 0; i < 6; i++ )
		cpu_set_nvram_offset( NVOFF_ENET+i, 1, &dig[i] );

	for ( i = 0x1000000 ; i; i--) 
			; 		/* possible delay */

	/* Retrieve and check whether eaddr has really been changed */

	for ( i = 0; i < 6; i++ ){
		cp = cpu_get_nvram_offset( NVOFF_ENET+i, 1);
		if ((int )(*cp) != dig[i])
			return 2;
	}	
	return( 0 );
}

int
ReadEnet(char *eaddr)
{
	char dig[6];
	char  *cp;
	int i;

	for ( i = 0, cp = eaddr ; *cp; ) {
		if ( *cp == ':' ) {
			cp++;
			continue;
		}
		else
		if ( !isxdigit(*cp) || !isxdigit(*(cp+1)) )
			return 1;
		else	{
			if ( i >= 6 ) {
				return 1;
			}
			if (is_fullhouse() ) {
				dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
				cp += 2;
			}
			else {
				cp += 2;
				i++;
			}
		}
	}

	if ( i != 6 ) {
		return 1;
	}

	for ( i = 0; i < 6; i++ ) {
		cpu_set_nvram_offset( NVOFF_ENET+i, 1, &dig[i] );
	}

	for ( i = 0x2000000 ; i; i--) 
			; 		/* possible delay */

	/* Retrieve and check whether eaddr has really been changed */

	for ( i = 0; i < 6; i++ ) {
	    cp = cpu_get_nvram_offset( NVOFF_ENET+i, 1);
	    if ((int )(*cp) != dig[i]) {
		return 2;
	    }
	}	
	
	return( 0 );
}

/* Get pallet ID in the form of wxyzL(wxyz are digits and L is a letter)
the netaddr will be 66.1.wx.yz */

void
get_pallet_id(void)
{
	char response[80];
	char *cp, *netaddr = {"66.1."};
	do {
		printf("Enter the pallet ID:");
		gets(response);
	} while(!isdigit(response[0]) || !isdigit(response[1]) || !isdigit(response[2]) || !isdigit(response[3]) || isdigit(response[4]));
	cp = netaddr;
	while (*cp != '\0') *cp++; 
	*cp++ = response[0];
	*cp++ = response[1];
	*cp++ = '.';
	*cp++ = response[2];
	*cp++ = response[3];
	*cp = '\0';
	setenv_nvram("netaddr",netaddr); 
}

/*
* System Configuration Fuse 
*/

main(argc,argv)
int argc;
char *argv[];
{
	int ans ;
	char response[80];

	printf("Starting cfuse....\n");
	{ /* add some delay after the printf() */
	    int i;
	    for(i=0; i<0x100000; i++);
	}

	/* Set some variables required by mfg for the ASRS */
	printf("\nSetting variable 'nogfxkbd' to 1\n");
	setenv_nvram("nogfxkbd","1");
	{ /* add some delay */
	    int i;
	    for(i=0; i<0x100000; i++);
	}

	printf("Setting variable 'diagmode' to M2\n");
	setenv_nvram("diagmode","M2");
	{ /* add some delay */
	    int i;
	    for(i=0; i<0x100000; i++);
	}

	printf("Setting variable 'AutoLoad' to Yes\n");
	setenv_nvram("AutoLoad", "Yes");
	{ /* add some delay */
	    int i;
	    for(i=0; i<0x100000; i++);
	}

	/* Check if nvram already burn, if yes issue warning and unfuse */

	if (nvram_is_protected()) {
		printf("Warning:  PCA is already fused. \n");
		get_enet();
		printf ("Do you want to change the eaddr?[y/n]");
		gets(response);
		if (response[0] == 'N' || response[0] == 'n') {
			get_pallet_id();
			EnterInteractiveMode();
		}
		Unfuse();
	}

	do {
		printf("Enter the new eaddr:");
		gets(response);
		ans = ReadEnet(response);
	} while(ans != 0 && ans != 2);

	printf("Setting variable 'eaddr' to %s\n", response);
	setenv_nvram("eaddr", response);

	if ( ans != 2 ) { /* if not already fused */
		Fuse();
	}

	/* Check if fuse OK? */
	if (!nvram_is_protected()) {
		printf("Fuse failed, nvram eaddr is not protected\n");
		wait();
		return(1);
	}

	/* 
	  Get pallet ID in the form of wxyz where:
	       * wxyz are digits 
	       * L is a letter
	   The netaddr will be 66.1.wx.yz 
	*/
	get_pallet_id();

/*	 Tests don't work now, so leave them out */
/*
	*Reportlevel = 4;
	if (eeprom_address()) wait();
	if (eeprom_data()) wait();
	if (eeprom_id()) wait();
	if (eeprom_pins()) wait();
*/
	EnterInteractiveMode();

	return(0);
}

/* 
 * KLUDGE Alert- These two parameters are referenced by files in the
 * libide.a library. So we need to define them here, although they are
 * not used.
 */
int ide_startup;
int ide_version;
