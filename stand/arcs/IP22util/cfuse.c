#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>


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
int wait(void);
int get_enet(void);
int set_date_for_mfg(void);
extern char *          cpu_get_nvram_offset(int, int);
/* extern int use_dallas; */
int size_nvram (void);
void	nvram_cs_on(void);
void	nvram_cs_off(void);
void 	nvram_cmd(unsigned int cmd, unsigned int reg, \
		register int bit_in_command);
int 	nvram_hold(void);
int 	nvram_is_protected(void);
int 	cpu_nv_lock(int);
int 	get_enet(void);
void 	us_delay(uint);
int 	cpu_set_nvram_offset(int, int, char *);
int 	printf(const char *, ...);
int 	isxdigit(char ch);
int 	isdigit(char ch);
char 	*gets(char *);
int 	setenv_nvram(char *, char *);
int 	ip24cfuse(int argc, char *argv[]);
int 	get_pallet_id(void);
int 	Unfuse(void);
int 	ReadEnet(char *eaddr);
int 	gPutEnet(char *eaddr);



/*
 * IP22 standalone utility program to set the NVRAM protect register
 */

Fuse(void)
{
	int 				bits_in_cmd = size_nvram ();
	volatile unsigned char		*cpu_auxctl = (unsigned char *)
						PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char 			health_led_off = 
						*cpu_auxctl & LED_GREEN;
	int				uflag = 0;
	int i, j ;

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
 * IP22 standalone utility program to unset the NVRAM protect register
   and perform nvram tests
 */
int
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

int
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
PutEnet( eaddr)
char *eaddr;
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
ReadEnet( eaddr)
char *eaddr;
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
	if (is_fullhouse() ) {
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
	}
	
	return( 0 );
}

/* Get pallet ID in the form of wxyzL(wxyz are digits and L is a letter)
the netaddr will be 66.1.wx.yz */
int
get_pallet_id(void)
{
	char response[80],ch;
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
	char response[80],ch;
/*	extern int Reportlevel ; */
	char *__s1, *__s2;


	printf("Starting cfuse....\n");
	printf("Setting the date to 01/01/90....\n");
	set_date_for_mfg();
	{ /* add some delay after the printf() */
	    int i;
	    for(i=0; i<0x100000; i++);
	}

	/* If guinness machine do new stuff */
	if( !is_fullhouse() ) {
		ip24cfuse(argc, argv);
	}
	else {
	    /* Assume Fullhouse (Indigo2) machines */
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
/*	Tests don't work now, so leave them out */
/*
		Reportlevel = 3;
		if (eeprom_address()) wait();
		if (eeprom_data()) wait();
		if (eeprom_id()) wait();
		if (eeprom_pins()) wait();
*/
	}
	EnterInteractiveMode();
}

/* 
 * KLUDGE Alert- These two parameters are referenced by files in the
 * libide.a library. So we need to define them here, although they are
 * not used.
 */
int ide_startup;
int ide_version;


/* T H I S   S T U F F   I S   F O R   G U I N N E S S   M A C H I N E S */

int
gPutEnet(char *eaddr)
{
	char  *cp;
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
			cp += 2;
			i++;
			}
	
	return( 0 );
}
struct	nvrm {
	char	*var_name;	/* NVRAM variable name */
	char	*var_value;	/* Value to set it to  */
	};

/* To set an NVRAM variable with CFUSE just add it to this table */
struct	nvrm	nvrm_setup[] = {
	"nogfxkbd", "1",
	"rebound", "y",
#ifndef FUNC
	"AutoLoad", "y",
	"diagmode", "M2",
	"Autopower", "y",
#else

	"diagmode", "v",
	"pagecolor", "0",
	"AutoLoad", "n",
#endif /* FUNC */
	0, 0,
	};

	

/*		i p 2 4 c f u s e ( )
 * ip24cfuse() Set the NVRAM variables: nogfx, diagmode, rebound, pagecolor
 *	netaddr, and eaddr. The Makefile builds two versions one for the
 *	functional area and one for the systems test area. 
 * 
 */
int
ip24cfuse(argc,argv)
int argc;
char *argv[];
{

	int ans ;
	char response[80],ch;
/*	extern int Reportlevel ; */
	u_int tmp, char_count, flag;
	struct nvrm *ptr;

	/* Generic ethernet address. Last two digits will be modified
	 * based on the station number
	 */
	char generic_eaddr[] = { "08:00:69:06:d1:00"};


	/* 
	 * Net address required by the ASRS
	 */
	char *cp, *netaddr = {"66.1."};

	/*
	 * Used in lib/libsk/ml/IP12nvram to determine whether this is
	 * a Guinness machine.
	 */
	/* use_dallas =!is_fullhouse(); */

	for( ptr= nvrm_setup; ptr->var_name != 0; ptr++) { 
		printf("   Setting variable %s to %s, return code %x\n",
			ptr->var_name, ptr->var_value, 
			setenv_nvram(ptr->var_name, ptr->var_value) ); 
		{ /* add some delay */
		    int i;
		    for(i=0; i<0x100000; i++);
		}
	}
		
	/* See if the user wants to skip setting the eaddr */
	get_enet();
#ifndef FUNC
	do {
		printf("\n   Enter the new eaddr (xx:xx:xx:xx:xx:xx)\n");
		printf("   Where 'x' is a hex value: ");
		gets(response);
		ans = gPutEnet(response);
	} while(ans != 0 && ans != 2);

	/*
	 * Unlock NVRAM soft lock bit so we can reset the eaddr
	 */
	cpu_nv_lock(0);

	printf("\n   Setting variable 'eaddr' to %s, return code %x\n",
	 	response, setenv_nvram("eaddr", response) );

	/*
	 * Lock NVRAM soft lock bit.
	 */
	cpu_nv_lock(1);

	/* Get pallet ID in the form of wxyzL(wxyz are digits and L is a letter)
	the netaddr will be 66.1.wx.yz */
	do {
		printf("\n   Enter the 4 digits of the pallet ID: ");
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

#else
	/* At the functional area create the eaddr and netaddr  based on the 
	 * station number entered.
	 */
	do {
		do {
			flag= 0;
			/* number between 1-99 decimal */
			printf("\n   Enter Station number: ");

			gets(response);
			cp = response;
			char_count= 0;
			while( *cp++ != '\0') char_count++; 
			if( char_count > 2) {
				flag = 1;
				printf("\n**** ERROR: Too many digits entered, number must be between 1-99.\n\n");
			}
	    	} while( flag) ; 

		/* Netaddr and eaddr require two digits */
		if( response[1] == '\0') {
			response[1] = response[0];
			response[0] = '0';
		}

		/* Make sure the input is a number */
		if (!isdigit(response[0]) || !isdigit( response[1]) ) {
			flag= 1;
			printf("\n*** ERROR: Must be a decimal number %s\n\n", response);
		}
                else {
			/* convert to bcd and check if bewteen 1 and 99*/
			tmp = (response[0]  - '0') << 4;
			tmp |= (response[1] - '0');
		}
		if ( tmp <= 0 || tmp > 0x99) {
			flag= 1;
			printf("\n*** ERROR: Must be a number bewtween 1-99\n\n");
		}
	} while( flag);

	cp = netaddr;
	while (*cp != '\0') *cp++; 

	*cp++ = '4';
	*cp++ = '.';
	*cp++ = response[0];
	*cp++ = response[1];
	*cp = '\0';
	printf("netaddr %s\n", netaddr);

	/* Over write the last two digits of the eaddr string with the
	 * station number
	 */
	generic_eaddr[15] = response[0];
	generic_eaddr[16] = response[1];

	/*
	 * Unlock NVRAM soft lock bit so we can reset the eaddr
	 */
	cpu_nv_lock(0);

	/* Now set the eaddr */
	printf("\n   Setting variable 'eaddr' to %s, return code %x\n",
	 	generic_eaddr, setenv_nvram("eaddr", generic_eaddr) );

	/*
	 * Lock NVRAM soft lock bit.
	 */
	cpu_nv_lock(1);

#endif /* FUNC*/

	/* Set the network address */
	printf("   Setting variable 'netaddr' to %s, return code %x\n",
		 netaddr, setenv_nvram("netaddr", netaddr) ); 

/*	Reportlevel = 3; */
	EnterInteractiveMode();
}

