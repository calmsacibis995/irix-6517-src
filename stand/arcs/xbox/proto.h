#pragma option -l;  // Disable header inclusion in listing
/*
		Silicon Graphics Computer Systems
        Header file for SN0 XBOX System Controller (XMSC)

                        proto.h

        (c) Copyright 1997   Silicon Graphics Computer Systems
		File contains the prototype defintions.

		Brad Morrow
*/

/* main.c */
void main(void);
void hw_init(void);
void init_cond(void);
void sw_init(void);
void LED_int(void);


/* monitor.c */
void monitor(void);
void inputPortA(void);
void inputPortB(void);
void inputPwr(void);
void tasks_env(void);
void tasks_fan(void);
void tasksFanSpeed(void); 
void tasks_fw(void);
void tasks_to(void);

/* sequence.c */
void power_up(void);
void power_dn(void);

/* util.c */
void cancel_timeout(int tcb_entry);
void delay(void);
void extMemWt(char ext_reg);
char extMemRd(void);
void fan_fail(void);
void next_fan(void);
void cntLED(char led, char mode);
void tick(void);
void timeout(char tcb_entry);

/* irq.c */
char i2cIntProc(void);

#pragma option +l;  // Enable header inclusion in listing
