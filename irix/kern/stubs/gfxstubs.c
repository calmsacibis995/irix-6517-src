int gfx_exit(){return 0;}
int gfx_disp(){return 1;}
void gfx_earlyinit(){}
int tp_init(){return 0;}
int tp_dogui(){return 0;}
void tpcons_write(){}
int gfx_fault(){return 1;}
void rrmSuspend(){}
void rrmResume(){}
void gfx_shcheck(){}
void gfx_shdup(){}
void gfx_shalloc(){}
void gfx_shfree(){}
void shmiq_sproc(){}

unsigned short kbdstate;
char kbdcntrlstate;

/* pc keyboard systems */
void initkbdtype(){}

/* Frame Scheduler */
/* ARGSUSED */
int gfx_frs_valid_pipenum(int pipenum){return (0);}
/* ARGSUSED */
void gfx_frs_install(void* intrgroup, int pipenum){}
/* ARGSUSED */
void gfx_frs_uninstall(int pipenum){}

