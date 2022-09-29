#pragma option -l;  // Disable header inclusion in listing
/*
		Silicon Graphics Computer Systems
        Header file for Lego Entry Level System Controller

                        proto.h

        (c) Copyright 1995   Silicon Graphics Computer Systems
		File contains the prototype defintions.

		Brad Morrow
*/

/* main.c */
void main(void);
void hw_init(void);
void init_cond(void);
void midplane_clk_init(void);
void nvram_init(void);
void sw_init(void);
void LED_int(void);


/* monitor.c */
void monitor(void);
void sendI2cToken(void);
void inputPortA(void);
void inputPortB(void);
void inputIn2(void);
void tasks_env(void);
void tasks_SlaveElsc(void);
void tasks_switch(void);
void tasks_fan(void);
void tasksFanSpeed(void); 
void tasks_fw(void);
void tasks_to(void);
void dip_sw(void);

/* sequence.c */
void power_up(void);
void power_dn(void);
void issueHbtRestart(void);
void issueNMI(void);
void issueReset(void);
void executeNMI(void);
void executeReset(void);

/* util.c */
void cancel_timeout(int tcb_entry);
void delay(void);
void dspControl(char cword);
void dspMsg(char far *cptr);
long rdTextChar(registerx msb, registerw lsb);
void dspChar(char ch_num, char ch_val);
void dspRamMsg(char near * sptr);
void extMemWt(char ext_data);
char extMemRd(void);
void getPOKBId(void);
void fan_fail(void);
void next_fan(void);
void speedoLED(char color, char mode);
void tick(void);
void timeout(char tcb_entry);
void hub_msg(char type, char msg);
void load_cnt_t_msg(void);
void load_CR_NL(void);


/* i2c.c */
void i2c_init(void);
char i2cAccess(void);
char i2c_master_xmit_recv(void);
char i2cToken(char);
void i2c_1_usec(void);
void i2c_slave(void);
char nvramAccess(void);
char readS0(void);
char readS1(void);
char waitForPin(void);
void writeS0(char reg_val);
void writeS1(char reg_val);


/* async.c */
void async_init(void);
#ifdef INDIERCT_Q
void flush(int near *qp);
void addq(int near *qp, char c);
unsigned char chkq(int near *qp);
unsigned char remq(int near *qp);

#else
void flush(int queue);
void addq(int queue, char c);
unsigned char chkq(int queue);
unsigned char remq(int queue);
#endif

/* irq.c */
char i2cIntProc(void);

/* cmd.c */
void sci_cmd(void);
void dsp_usage(char cmd);
void v_margin(char v_source, char setting);
void processCmd(void);
void getCmdNum(void);
void parseCmd(void);
unsigned char getNextChar(int	cut_space);
void purgeXtraChars(void);
void sendHubInt(char *acp_int);
void sendHubRsp(void);
void sendAcpRsp(void);
char getParam(void);
char putParam(char near * ptr);
char convertXtoA(char c, int nibble);
char elscToElsc(char far *cptr);
		
#pragma option +l;  // Enable header inclusion in listing
