#ifndef _SCI_INCLUDED
#include <async.h>
#endif

#ifndef _EXEC_INCLUDED
#include <exec.h>
#endif

/************ main.c *************/
non_banked void main(void);
non_banked void boot(void);


/************ exec.c *************/
non_banked void execinit(void);
non_banked monitor short check(short semnum);
non_banked char create(void (*text)(), unsigned short stksiz, char pri);
non_banked char dupproc(char ipid);
non_banked monitor void forfit();
non_banked char getmypid();
non_banked monitor void remove(char pid);
non_banked unsigned short receive(char semnum);
non_banked monitor void send(short semnum, unsigned long msg);
non_banked void start(char pid);
non_banked monitor void stop(char pid);
non_banked monitor void timeout(short semnum, unsigned short msg, unsigned short tval);
non_banked monitor short cancel_timeout(short semnum,unsigned long msg);

/************ executil.c *************/
non_banked void rti_init(void);
non_banked void tick(void);
non_banked void procinit(void);

/************ execsubs.c *************/
non_banked void dispatch();
non_banked char *dlink(QLINK *qp);
non_banked char *dqueue(QUEUE *qp);
non_banked void link(QLINK *qp, QLINK *ep);
non_banked void queue(QUEUE *qp, QLINK *ep);
non_banked void stk_chk(void);


/************ exec_asm.s07 *************/
non_banked char *getsp(void);
non_banked void sys_reset(void);
non_banked void enable_xirq(void);
non_banked void runproc(unsigned short , unsigned short );
non_banked void delay(long to_value);


/************ dsp_proc.c *************/
non_banked void display(void);
non_banked void dsp_menu(char cmd);
non_banked void dsp_boot(void);
non_banked void dsp_db_switch(char cmd);
non_banked void dsp_log(char cmd);
non_banked void dsp_ver(char scrn_fmt);
non_banked void dsp_perf(void);
non_banked void dsp_nmi(char cmd);
non_banked void dsp_sclr(char cmd);
void mn_bpnmi(void);
void mn_boot(void);
void mn_cpu_sel(void);
void mn_db_switch(void);
void mn_event(void);
void mn_perf(void);
void mn_reset(void);
void mn_temp(void);
void mn_ver(void);
void mn_volts(void);

/************ cpuproc.c *********/
non_banked void cpu_proc(void);
non_banked void  boot_arb(void);
void cpu_cmd(char class, char type);

/************ sys_mon.c *************/
non_banked void blower_init(void);
non_banked void sys_mon(void);
non_banked void pok_chk(void);

/************ sequence.c *************/
non_banked void sequencer(void);
non_banked monitor void shutdown(unsigned char type);
non_banked int powerup(void);

/************ util.c *************/
non_banked char dsp_event(char event);
non_banked void init_port(void);
non_banked void log_event(char event, char dsp_flag);
non_banked char log_read(char *msg_buf, char e_num, char req_count, char clear);
non_banked void rtc_wr(char *abuf, char count);
non_banked void rtc_rd(char *abuf, char count);
non_banked void get_check(char *buff, char seq);
non_banked void get_debug_sw(char *buff, char seq);
non_banked void get_env_data(char *buff, char seq);
non_banked void get_log(char *buff, char seq);
non_banked void get_lcd(char *buff, char seq);
non_banked void get_ser_no(char *buff, char seq);
non_banked void get_time(char *buff, char seq);
non_banked void set_bt_msg(char *buff);
non_banked void set_check(void);
non_banked void set_debug_sw(char *buff);
non_banked void set_cpu_hist(char *buff);
non_banked void set_proc_stat(char *buff);
non_banked void set_ser_no(char *buff);


/************ display.c *************/
non_banked void fpprint(char *msg, char init_flg);
non_banked void fpinit(void);
non_banked void fpinit_cy(void);
non_banked void fpclear_cy(void);
non_banked void fpinit_hd(void);
non_banked void hd_graph(char *msg_str, char count);
non_banked void cy_print(char *msg_str);
non_banked char cyreg_rd(char reg_no);
non_banked void cy_str(char *msg_str);
non_banked void hd_print(char *msg_str, char init_flg);
non_banked void hd_cmd(char cmd);
non_banked void hd_char(char lcd_char);
non_banked void wait_bf(void);
non_banked void status_fmt(char *title, char new_screen, char scroll_flag, unsigned short screen_timeout);
non_banked void screen_sw(char new_screen, unsigned short timeout_value);


/************ async.c *************/
non_banked void async_init(char mode);
non_banked void flush(struct cbufh *qhdr);
non_banked void addq(struct cbufh *qp, char c);
non_banked unsigned char remq(struct cbufh *qp);
non_banked char getcmd(struct cbufh *qp, char *buff);
non_banked char putcmd(struct cbufh *qp, char *buff);
