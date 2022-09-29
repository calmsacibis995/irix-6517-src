/***********************************************************************\
*	File:	serial.c						*
*									*
*	Contains basic support code for the system serial number.	*
*	To improve the integrity of the serial number storage, we	*
*	now store it in both the system controller and the IO4 PROM	*
*	and compare the two.						*
*									*
\***********************************************************************/

#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/nvram.h>
#include <sys/EVEREST/evconfig.h>
#include <libsc.h>
#include <libsk.h>

extern int sysctlr_getserial(char *serial);
extern int sysctlr_setserial(char *serial);
extern int nvram_getserial(char *serial);
extern int nvram_setserial(char *serial);
extern int read_serial(char*);
extern void write_serial(char*);
extern int valid_serial(char*);
 
/*
 * serial_cmd()
 *      Allows the user to display and alter the system serial
 *      number.  Eventually, this will only work if the serial
 *      number hasn't been written already.
 */

int
serial_cmd(int argc, char **argv)
{
    char serial[SERIALNUMSIZE];
    char serial2[12];
    int valid;

    valid = !read_serial(serial);

    if (argc == 1) {
	printf("Serial number is %s\n",
	       (valid ? serial : "not set."));
    }
    else if (argc == 2) {

	/* Don't allow user to change serial number unless it
	 * appears that the serial number is invalid or
	 * the magic switch combination is set.
	 */
#if !defined(DEBUG)
	if (valid) {
	    printf("The serial number has already been set and"
		   " may not be changed.\n\n");
	    return 0;
	}
#endif
	if (strlen(argv[1]) >= 10) {
	    printf("Serial number must be less than 10 chars.\a\n");
	    return 0;
	}

	if (!valid_serial(argv[1])) {
	    printf("'%s' is not a valid serial number. Aborting...\a\n",
		   argv[1]);
	    return 0;
	}

	printf("Please re-enter the serial number for confirmation: ");
	fgets(serial2, 11, 0);

	if (strcmp(argv[1], serial2)) {
	    printf("Serial numbers do not match!!! Aborting...\a\n");
	} else {
	    write_serial(argv[1]);

	    if (read_serial(serial) == -1) {
		printf("Could not retrieve serial number!!!\n");
		return 0;
	    }

	    if (strcmp(serial, argv[1])) {
		printf("ERROR: Could not set serial number!!\a\n");
		return 0;
	    }
	}
    } else {
	return 1;
    }

    return 0;
}


int
erase_serial_cmd()
{
    return(sysctlr_setserial("\t\t"));
}

int erase_nvram_cmd()
{
    return(nvram_setserial("\t\t"));
}
