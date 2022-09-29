#include <sys/types.h>
#include <sys/IP22.h>
#include <sys/sbd.h>

#define	RESET_PRE	*cpu_auxctl &= ~NVRAM_PRE
#define	SET_PRE		*cpu_auxctl |= NVRAM_PRE

int check_enet(void);
int wait(void);
int get_enet(void);
extern char *          cpu_get_nvram_offset(int, int);
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
void    EnterInteractiveMode(void);
int 	Unfuse(void);
int 	ReadEnet(char *eaddr);
int 	gPutEnet(char *eaddr);

int
ip24unfuse(void)
{
	cpu_nv_lock(0);
	EnterInteractiveMode();
}

/*
 * IP22 standalone utility program to unset the NVRAM protect register
 */
main(int argc, char **argv)
{
	int 				bits_in_cmd = size_nvram ();
	volatile unsigned char		*cpu_auxctl = (unsigned char *)
						PHYS_TO_K1(CPU_AUX_CONTROL);
	unsigned char 			health_led_off = 
						*cpu_auxctl & LED_GREEN;

	/* if guinness do new stuff */
	if ( !is_fullhouse() ) 
		ip24unfuse();

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
	EnterInteractiveMode(); /* Go back to prom monitor mode */
}
