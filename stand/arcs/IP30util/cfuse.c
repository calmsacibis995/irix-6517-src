/* cfuse.c - cfuse program for IP30 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/nic.h>
#include <uif.h>
#include <libsk.h>
#include <libsc.h>
#include <sys/RACER/IP30nvram.h>

#define	TM_PM10		0
#define	TM_PM20		1
#define	TM_IP30		2
#define	TM_GFX		3
#define	TM_FP		4
#define	TM_PCI		5

void wait(void);
extern int reboot_cmd(int argc, char **argv);
extern int      nic_get_eaddr(__uint32_t *, __uint32_t *, char *);


extern char *          cpu_get_nvram_offset(int, int);
extern int		cpu_probe(void);
extern	_setenv(char *name, char *value, int override);

char response[40];
char *netAddr;
int  num_cpus;
int  emc = 0;


#define	Dprintf		if(debug)printf


void
wait(void)
{
	char line[80];
	while (1) {
		printf(" Press <Enter> to continue ");
		gets(line);
	}

}

void
setnetaddr(char *cp)
{
	while (*cp != '\0') *cp++;
	*cp++ = response[0];
	*cp++ = response[1];
	*cp++ = '.';
	*cp++ = response[2];
	*cp++ = response[3];
	*cp = '\0';
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
			/*
			if (is_fullhouse() ) {
				dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
				cp += 2;
			}
			else {
				cp += 2;
				i++;
			}
			*/
			dig[i++] = (HEXVAL(*cp) << 4) + HEXVAL(*(cp+1));
			cp += 2;
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

int
func_setup(void) {
	char testModule[20];
	char *cp;
	int  tm, ans;

	printf("\n=== Setting up for Function Test ===\n\n");

	printf("Setting AutoLoad to No\n");
	setenv_nvram("AutoLoad", "No");

	printf("Setting idebinary to ide.IP30MG\n");
	_setenv("idebinary", "ide.IP30MG", 2);

	do {
	    printf("Enter the test module (pm10, pm20, ip30, gfx, fp, pci): ");
	    gets(testModule);
	    if (!strcasecmp(testModule, "pm10")) { tm = TM_PM10; break ; }
	    else if (!strcasecmp(testModule, "pm20")) { tm = TM_PM20; break ; }
	    else if (!strcasecmp(testModule, "ip30")) { tm = TM_IP30; break ; }
	    else if (!strcasecmp(testModule, "gfx")) { tm = TM_GFX; break ; }
	    else if (!strcasecmp(testModule, "fp")) { tm = TM_FP; break ; }
	    else if (!strcasecmp(testModule, "pci")) { tm = TM_PCI; break ; }
	    else if (!strcasecmp(testModule, "q")) { return(1) ; }
	    else {printf("Invalid test module - type q to quit\n");}
	} while (1);
	printf("Setting testmod to %s\n", testModule);
	_setenv("testmod", testModule, 2);

	switch (tm) {
	case TM_PM10:
	case TM_PM20:
	case TM_IP30:
		printf("Setting cpuvolt to nom\n");
		_setenv("cpuvolt", "nom", 2);
		break;
	case TM_GFX:
		printf("Setting gfxvolt to nom\n");
		_setenv("gfxvolt", "nom", 2);
		break;
	case TM_FP:
	case TM_PCI:
		printf("cfuse not implemented for Frontplane/PCI boards\n");
		break;
	}

	do {
		printf("\nEnter a 4-digit number for the IP address: ");
		gets(response);
	} while(!isdigit(response[0]) || !isdigit(response[1]) || !isdigit(response[2]) || !isdigit(response[3]) || isdigit(response[4]));
	cp = netAddr;

	setnetaddr(cp);

	printf("Setting netaddr to %s\n", netAddr);
	setenv_nvram("netaddr",netAddr);

	if (emc) {
		printf("Setting ef0mode to 100\n");
		_setenv("ef0mode", "100", 2);
	} else {
		printf("Setting ef0mode to 10\n");
		_setenv("ef0mode", "10", 2);
	}

	printf("Setting pdslog to off\n");
	_setenv("pdslog", "off", 2);

	return(0);
}

int
system_setup(void) {
	char testModule[20], eaddr_from_user[40], eaddr_from_system[40];
	char testStr[80];
	char eaddr[6], ch;
	char *cp, *pEaddr;
	int  i, j, tm, ans, flag, testNumber, count = 0;
	__psunsigned_t	mcr, gpcr_s;

	printf("\n=== Setting up for System Test ===\n\n");

	do {
		pEaddr = eaddr_from_system; /* pointer to a buf */
		printf("Enter the new eaddr: ");
		gets(eaddr_from_user);

		/* read from the system serial number NIC chip */
		mcr = IOC3_PCI_DEVIO_K1PTR + IOC3_MCR;
		gpcr_s = IOC3_PCI_DEVIO_K1PTR + IOC3_GPCR_S;
		if (nic_get_eaddr((__uint32_t *)mcr, (__uint32_t *)gpcr_s, 
			eaddr) != NIC_OK) {
			printf("******* ERROR: Bad/missing eaddr NIC *******\n");
			wait();
		}

		/* convert binary array to a string */
		for (i = 0; i < 6; i++) {
		    for (j = 0; j < 2; j++) {
			if (j) {
				ch = (eaddr[i] >> 0) & 0xf;
		  	} else {
				ch = (eaddr[i] >> 4) & 0xf;
			}
			if ((int)ch >= 0 && (int)ch <= 9) {
				ch = (char) ('0' + ch);
			} else {
				ch = (char) ('a' + (ch - 10));
			}
			*pEaddr = ch;
			pEaddr++;
		    }
		}
		*pEaddr = '\0';

		printf("eaddr from system  : %s\n", eaddr_from_system);
		printf("eaddr from user    : %s\n", eaddr_from_user);

		/* 
		 * check to see if the serial number of the system NIC
		 * matches with what the user types in 
		 */
		if (strcasecmp(eaddr_from_user, eaddr_from_system) == 0) break;
		if (count > 3) {
			printf("******* ERROR: cfuse exiting because of eaddr mismatch *******\n");
			wait();
		} else {
			count++;
		}
	} while(1);

	printf("Setting AutoLoad to Yes\n");
	setenv_nvram("AutoLoad", "Yes");

	num_cpus = cpu_probe();

#ifdef TESTALL
	strcpy(testModule, "pm20-ip30-gfx");
#else
	printf("\nSelect module to be tested:-\n\
		<ENTER> or 0 -> System\n\
		           1 -> Processor Board ONLY\n\
		           2 -> IP30 Board ONLY\n\
		           3 -> Graphics Board ONLY\n\
		           4 -> Processor AND IP30 Boards\n");
	do {
	    printf("Enter the number of your choice: ");
	    gets(testStr);
	    testNumber = atoi(testStr);
	    flag = 0;
	    switch (testNumber) {
	       case 0:
			if (num_cpus == 1) {
				strcpy(testModule, "pm10-ip30-gfx");
			} else {
				strcpy(testModule, "pm20-ip30-gfx");
			}
			break;
	       case 1:
			if (num_cpus == 1) {
				strcpy(testModule, "pm10");
			} else {
				strcpy(testModule, "pm20");
			}
			break;
	       case 2:
			strcpy(testModule, "ip30");
			break;
	       case 3:
			strcpy(testModule, "gfx");
			break;
	       case 4:
			if (num_cpus == 1) {
				strcpy(testModule, "pm10-ip30");
			} else {
				strcpy(testModule, "pm20-ip30");
			}
			break;
	       default:
			printf("Invalid Choice - try again\n");
			flag = 1;
			break;
	    }
	} while (flag);
#endif
	printf("Setting testmod to %s\n", testModule);
	_setenv("testmod", testModule, 2);

	printf("Setting idebinary to ide.IP30MG\n");
	_setenv("idebinary", "ide.IP30MG", 2);

	printf("Setting cpuvolt to nom\n");
	_setenv("cpuvolt", "nom", 2);

	printf("Setting gfxvolt to nom\n");
	_setenv("gfxvolt", "nom", 2);

	do {
		printf("Enter the pallet ID: ");
		gets(response);
	} while(!isdigit(response[0]) || !isdigit(response[1]) || !isdigit(response[2]) || !isdigit(response[3]) || isdigit(response[4]));
	cp = netAddr;

	setnetaddr(cp);

	printf("Setting netaddr to %s\n", netAddr);
	setenv_nvram("netaddr",netAddr);

	printf("Setting ef0mode to 100\n");
	_setenv("ef0mode", "100", 2);

	printf("Setting pdslog to off\n");
	_setenv("pdslog", "off", 2);

	return(0);
}

int
ef_setup(void) {
	/* Put the changes for emc function test here */
	return(0);
}

int
es_setup(void) {
	/* Put the changes for emc system test here */
	return(0);
}

/*
* System Configuration Fuse 
*/
main(argc,argv)
int argc;
char *argv[];
{
	int ans , err = 0;
	char inval[80];

	printf("\nStarting cfuse for OCTANE....\n\n");

	printf("Setting nogfxkbd to 1\n");
	setenv_nvram("nogfxkbd", "1");

	printf("Setting diagmode to mfg\n");
	setenv_nvram("diagmode", "mfg");

	printf("\nFollowing manufacturing sites are allowed:\n");
	printf("    mtv - (Mountain View)\n");
	printf("    emc - (EMC)\n");

        do {
	   printf("\nEnter the manufacturing site: ");
	   gets(inval);
	   if(!strcasecmp(inval, "mtv")) {
		_setenv("config_ip_addr", "10.1.100.2", 2);
		_setenv("final_ip_addr",  "10.1.100.3", 2);
		_setenv("func_ip_addr",   "10.1.100.4", 2);
		_setenv("ra_ip_addr",     "10.1.100.5", 2);
		netAddr = "10.1."; break;
	   }
	   else if(!strcasecmp(inval, "emc")) {
		_setenv("config_ip_addr", "10.1.100.2", 2);
		_setenv("final_ip_addr",  "10.1.100.3", 2);
		_setenv("func_ip_addr",   "10.1.100.4", 2);
		_setenv("ra_ip_addr",     "10.1.100.5", 2);
		netAddr = "10.1."; emc = 1; break;
	   }
	   else if(!strcasecmp(inval, "q")) {
		err = 1; break;
	   }
	   else {
		printf("Invalid process location - type q to quit\n");
	   }
	} while (1);

	printf("\nFollowing process locations are allowed:\n");
	printf("    f - (Function Test)\n");
	printf("    s - (System Test)\n");

        do {
	   printf("\nEnter the process location: ");
	   gets(inval);
	   if(!strcasecmp(inval, "f")) {
		err = func_setup();
		/* Make small changes specifically for EMC if necessary */
		if (emc)
		    ef_setup();
		break;
	   } else if(!strcasecmp(inval, "s")) {
		err = system_setup();
		/* Make small changes specifically for EMC if necessary */
		if (emc)
		    es_setup();
		break;
	   } else if(!strcasecmp(inval, "q")) {
		err = 1; break;
	   } else {
		printf("Invalid process location - type q to quit\n");
	   }
	} while (1);

	if (err) {
		EnterInteractiveMode();
	} else {
		reboot_cmd(1, NULL);
	}

	return(0);
}

