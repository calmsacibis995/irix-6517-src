/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#include "evo_diag.h"
#ifdef IP30
#include "../../../godzilla/include/d_bridge.h"
#include "../../../godzilla/include/d_godzilla.h"
#endif

/*
 * Forward References
 */
__uint32_t 	_evo_reportPassOrFail(evo_test_info *tPtr, __uint32_t errors);
__uint32_t	_evo_outb(__paddr_t addr, unsigned char val);
uchar_t		_evo_inb(__paddr_t addr);
unsigned short	_evo_inhw(__paddr_t addr);
__uint32_t	_evo_outhw(__paddr_t addr, unsigned short  val);
__uint32_t	_evo_outw(__paddr_t addr, __uint32_t val);
__uint32_t	_evo_inw(__paddr_t addr);
__uint32_t	evo_waitForSpace(__uint32_t argc, char **argv);
int		_evo_i2c_poll(void);
void		_evo_i2c_slave(uchar_t);
void		_evo_i2c_stop(void);
int		_evo_i2c_dummy_read(void);
void		_evo_i2c_start(void);
int		_evo_i2c_read(uchar_t *, int);
int		_evo_i2c_write(int);
__uint32_t	_evo_i2c_dev_wr(int addr, uchar_t *data, int len);
uchar_t		_evo_i2c_dev_rd(int addr, uchar_t *data, int len);

unsigned char* 	_evo_PageAlign(unsigned char *pBuf);
int 		check_reference(void);
int 		_evo_i2c_seq_retry_count = 0;

/*
 * Global Variables
 */

/*** For Speed Racer Bridge port ***/
__uint32_t evo_bridge_port;

evo_vgi1_ch_trig_t evo_abort_dma = {
        0LL,                            /* unused0 */
        EVO_VGI1_DMA_ABORT_ON,          /* abort */
        EVO_VGI1_TRIG_DISABLE           /* int_trig */
};
evo_vgi1_ch_trig_t evo_trig_on = {
        0LL,                            /* unused0 */
        EVO_VGI1_DMA_ABORT_OFF,         /* abort */
        EVO_VGI1_TRIG_ENABLE            /* int_trig */
};
evo_vgi1_ch_trig_t evo_trig_off = {
        0LL,                            /* unused0 */
        EVO_VGI1_DMA_ABORT_OFF,         /* abort */
        EVO_VGI1_TRIG_DISABLE           /* int_trig */
};


__uint32_t evo_walk[24] = {
	0x11111111,  0x22222222,  0x33333333,  0x44444444,
	0x55555555,  0x66666666,  0x77777777,  0x88888888,
	0x99999999,  0xaaaaaaaa,  0xbbbbbbbb,  0xcccccccc,
	~0x11111111, ~0x22222222, ~0x33333333, ~0x44444444,
	~0x55555555, ~0x66666666, ~0x77777777, ~0x88888888,
	~0x99999999, ~0xaaaaaaaa, ~0xbbbbbbbb, ~0xcccccccc

};

__uint32_t evo_walk_one[16] = {
	0xFFFE0001, 0xFFFD0002, 0xFFFB0004, 0xFFF70008,
	0xFFEF0010, 0xFFDF0020, 0xFFBF0040, 0xFF7F0080,
	0xFEFF0100, 0xFDFF0200, 0xFBFF0400, 0xF7FF0800,
	0xEFFF1000, 0xDFFF2000, 0xBFFF4000, 0x7FFF8000
};

__uint32_t evo_patrn[6] = {
	0x55550000, 0xAAAA5555, 0x0000AAAA,
	0x55550000, 0xAAAA5555, 0x0000AAAA
};

__uint32_t evo_patrn32[6] = {
	((__uint32_t)0x55550000),
	((__uint32_t)0xAAAA5555),
	((__uint32_t)0x0000AAAA),
	((__uint32_t)0x5555AAAA),
	((__uint32_t)0x55555555),
	((__uint32_t)0xAAAAAAAA),
};


uchar_t evo_patrn8[5] = {
	0x55, 0xAA, 0xCC, 0x33, 0xFF
};

/*
 * This structure matches with the constants defined in evo_diag.h
 * for error codes 
 */
evo_test_info	evo_err[] = {
	/* Impact Video Board Error Strings and Codes */
	/* Board Specific ASIC independent Tests 0 - 4 */
	{"",						"XXXXX"},  /* 00 */
	{"Impact Video Initialization Test",		"XXXXX"},  /* 01 */
	{"EVO NIC Test",				"XXXXX"},  /* 02 */
	{"",						"XXXXX"},  /* 02 */
	{"",						"XXXXX"},  /* 03 */
	{"",						"XXXXX"},  /* 04 */
	{"",						"XXXXX"},  /* 05 */
	{"",						"XXXXX"},  /* 06 */
	{"",						"XXXXX"},  /* 07 */
	{"",						"XXXXX"},  /* 08 */
	{"",						"XXXXX"},  /* 09 */

	/* VGI1 Error Strings and Codes */
	{"VGI1 Register Pattern Test",			"XXXXX"},  /* 10 */
	{"VGI1 Register Walking Bit Test",		"XXXXX"},  /* 11 */
	{"VGI1 DMA Test",				"XXXXX"},  /* 12 */
	{"VGI1 Interrupt Test",				"XXXXX"},  /* 13 */
	{"",						"XXXXX"},  /* 14 */
	{"",						"XXXXX"},  /* 15 */
	{"",						"XXXXX"},  /* 16 */
	{"",						"XXXXX"},  /* 17 */
	{"",						"XXXXX"},  /* 18 */
	{"",						"XXXXX"},  /* 19 */

        /* CSC Error Strings and Codes */
        {"CSC  Address Bus Test",                       "XXXXX"},  /* 20 */
        {"CSC  Data Bus Test",                          "XXXXX"},  /* 21 */
        {"CSC  Input LUTs Test",                        "XXXXX"},  /* 22 */
        {"CSC  Output LUTs Test",                       "XXXXX"},  /* 23 */
        {"CSC  Coefficient Test",                       "XXXXX"},  /* 24 */
        {"CSC  X2K Lut Test",                           "XXXXX"},  /* 25 */
	{"",						"XXXXX"},  /* 26 */
	{"",						"XXXXX"},  /* 27 */
	{"",						"XXXXX"},  /* 28 */
	{"",						"XXXXX"},  /* 29 */

        /* SCL Error Strings and Codes */
        {"Scaler  Address Bus Test",                    "XXXXX"},  /* 30 */
        {"Scaler  Data Bus Test",                       "XXXXX"},  /* 31 */
        {"",				                "XXXXX"},  /* 32 */
        {"",			                        "XXXXX"},  /* 33 */
        {"",                    		        "XXXXX"},  /* 34 */
        {"",		                                "XXXXX"},  /* 35 */
	{"",						"XXXXX"},  /* 36 */
	{"",						"XXXXX"},  /* 37 */
	{"",						"XXXXX"},  /* 38 */
	{"",						"XXXXX"},  /* 39 */

        /* VOC Error Strings and Codes */
        {"VOC  Address Bus Test",                       "XXXXX"},  /* 40 */
        {"VOC  Data Bus Test",                          "XXXXX"},  /* 41 */
        {"",				                "XXXXX"},  /* 42 */
        {"",			                        "XXXXX"},  /* 43 */
        {"",                    		        "XXXXX"},  /* 44 */
        {"",		                                "XXXXX"},  /* 45 */
	{"",						"XXXXX"},  /* 46 */
	{"",						"XXXXX"},  /* 47 */
	{"",						"XXXXX"},  /* 48 */
	{"",						"XXXXX"},  /* 49 */

        /* FMB Error Strings and Codes */
        {"FMB  Address Bus Test",                       "XXXXX"},  /* 50 */
        {"FMB  Data Bus Test",                          "XXXXX"},  /* 51 */
        {"",				                "XXXXX"},  /* 52 */
        {"",			                        "XXXXX"},  /* 53 */
        {"",                    		        "XXXXX"},  /* 54 */
        {"",		                                "XXXXX"},  /* 55 */
	{"",						"XXXXX"},  /* 56 */
	{"",						"XXXXX"},  /* 57 */
	{"",						"XXXXX"},  /* 58 */
	{"",						"XXXXX"},  /* 59 */

        /* I2C Error Strings and Codes */
        {"I2C  Initialization Test",                    "XXXXX"},  /* 60 */
        {"I2C  Encoder Test",                           "XXXXX"},  /* 61 */
        {"I2C  Decoder Test",		                "XXXXX"},  /* 62 */
        {"",			                        "XXXXX"},  /* 63 */
        {"",                    		        "XXXXX"},  /* 64 */
        {"",		                                "XXXXX"},  /* 65 */
	{"",						"XXXXX"},  /* 66 */
	{"",						"XXXXX"},  /* 67 */
	{"",						"XXXXX"},  /* 68 */

        /* FPGA Error Strings and Codes */
	{"VOC FPGA Test",				"XXXXX"},  /* 70 */
        {"Genesis-A Input FPGA Test",                   "XXXXX"},  /* 71 */
        {"Genesis-A Output FPGA Test",                  "XXXXX"},  /* 72 */
        {"Genesis-B Input FPGA Test",                   "XXXXX"},  /* 73 */
        {"Genesis-B Output FPGA Test",                  "XXXXX"},  /* 74 */
        {"Bit Picker FPGA Test",	                "XXXXX"},  /* 75 */
        {"Reference Generator FPGA Test",               "XXXXX"},  /* 76 */
	{"Square Pixel FPGA Test",			"XXXXX"},  /* 77 */
        {"Frame Reset FPGA Test",	                "XXXXX"},  /* 78 */
	{"",						"XXXXX"},  /* 79 */

};

#if MGV_NOT_INCLUDED
/* The following LUT's are defined in evo_extern.h.
   Change the following if those LUTs value change.
*/
char * csc_luts[] = {
	"YG INPUT LUT",		/* CSC_YG_IN */
	"UB INPUT LUT",		/* CSC_UB_IN */
	"VR INPUT LUT",		/* CSC_VR_IN */
	"XK INPUT LUT",		/* CSC_XK_IN */
	"YG OUTPUT LUT",	/* CSC_YG_OUT */	
	"UB OUTPUT LUT",	/* CSC_UB_OUT */
	"VR OUTPUT LUT",	/* CSC_VR_OUT */
	"ALPHA OUTPUT LUT"	/* CSC_ALPHA_LUT */
	};
#endif

evodiag	evobase_data;
evodiag	*evobase = (evodiag *) &evobase_data;
unsigned char   *evo_pVGIDMADesc;
unsigned char   evo_VGIDMADesc[4096 * 2 ] /*4K Pg. Size*/;

#if MGV_NOT_INCLUDED
evo_vgi1_h_param_t	hpNTSC = {0LL};
evo_vgi1_h_param_t	hpNTSCSQ = {0LL};
evo_vgi1_h_param_t	hpPAL = {0LL};
evo_vgi1_h_param_t	hpPALSQ = {0LL};

evo_vgi1_v_param_t	vpNTSC = {0LL};
evo_vgi1_v_param_t	vpNTSCSQ = {0LL};
evo_vgi1_v_param_t	vpPAL = {0LL};
evo_vgi1_v_param_t	vpPALSQ = {0LL};

/*                  Parameters for each tv standard          */
/*                CCIR_525, SQ_525,   CCIR_625, SQ_625   */
int      xsize[]     = { 720, 640, 720, 768 };
int      ysize[]     = { 486, 486, 576, 768 };


/*			    	allones,allzeroes,alt10, alt01,walk1,walk0 */
/* DO NOT CHANGE INITIAL WALK VALUES: walk pattern generators will limit values */
int test_values[] = { 0xFF,    0x00,    0xAA,  0x55, 0x01, 0xFE, 0x80, 0x40 };

char* asc_bit_pat_p[] = { 
				"ALLONES", "ALLZEROES", "ALT1&0",
				"ALT0&1",  "WALK1",     "WALK0",
				"HEX80",   "HEX40"};

char* asc_vid_pat_p[] = {
				"ALLONES", "ALLZEROES", "ALT1&0",
				"ALT0&1",  "WALK1",     "WALK0",
				"VLINE",   "HLINE",     "BOX",
				"XHBOX" };

char* asc_tv_std_p[] =  {  "CCIR_525", "SQ_525", "CCIR_625", "SQ_625" };


char* asc_primary_num[] = {
				"FIRST", "SECOND", "THIRD", "FOURTH",
				"FIFTH", "SIXTH",  "SEVENTH", "EIGHTH",
				"NINTH", "TENTH" };

/********************************************************************/
				
#if 1
/* YUVA arrays for test_allstds patterns */
float   yfgm[1][1];
float   ufgm[1][1];
float   vfgm[1][1];
short   afgm[1][1];
#else
/* YUVA arrays for test_allstds patterns */
float   yfgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
float   ufgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
float   vfgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
short   afgm[MAX_HOR/YDIV][MAX_HOR/XDIV];
#endif


/* Galileo 1.5 */

int tv_std_loc[][16][2] = { 
			{ /* CCIR525 */
			{  0,0},{  0,180}, {  0,360}, {  0,540},
			{122,0},{122,180}, {122,360}, {122,540}, 
			{243,0},{243,180}, {243,360}, {243,540}, 
			{365,0},{365,180}, {365,360}, {365,540}},
			{ /* SQ525 */ 
			{  0,0},{  0,160}, {  0,320}, {  0,480},
			{122,0},{122,160}, {122,320}, {122,480}, 
			{243,0},{243,160}, {243,320}, {243,480}, 
			{365,0},{365,160}, {365,320}, {365,480}},
			{ /* CCIR625 */
			{  0,0},{  0,180}, {  0,360}, {  0,540},
			{144,0},{144,180}, {144,360}, {144,540}, 
			{288,0},{288,180}, {288,360}, {288,540}, 
			{432,0},{432,180}, {432,360}, {432,540}},
			{ /* SQ625 */ 
			{  0,0},{  0,180}, {  0,360}, {  0,540},
			{192,0},{192,180}, {192,360}, {192,540}, 
			{384,0},{384,180}, {384,360}, {384,540}, 
			{576,0},{576,180}, {576,360}, {576,540}} 
			};

int tv_std_length[][2] = {
					/* YLENGTH/YDIV, XLENGTH */
			 	/* CCIR525 */  121, 720,
				/* SQ525   */  121, 640,
				/* CCIR625 */  144, 720,
				/* SQ625   */  144, 768  };

reg_test	batch_test[MAX_REGMEMS];


/* CSC Specific variables */
int csc_inlut, csc_alphalut, csc_coef, csc_x2k, csc_outlut, csc_inlut_value, csc_tolerance;
int csc_clipping, csc_constant_hue, csc_each_read ;
#endif /* MGV_NOT_INCLUDED */

int evo_i2c_timeout = 1000;

/*
 * Global functions
 */
__uint32_t
_evo_reportPassOrFail(evo_test_info *tPtr, __uint32_t errors)
{
    if (!errors) {
	msg_printf(SUM, "IMPV:- %s passed. ---- \n" , tPtr->TestStr);
	return 0;
    } else {
	msg_printf(SUM, "IMPV:- %s failed. ErrCode %s Errors %d\n",
		tPtr->TestStr, tPtr->ErrStr, errors);
	return -1;
    }
}

 
__uint32_t
_evo_outb(__paddr_t addr, unsigned char val)
{
    register volatile uchar_t *ioaddr;

    ioaddr  = (volatile uchar_t *)addr;
    *ioaddr = val;
    return 0;
}

uchar_t
_evo_inb(__paddr_t addr)
{
    register uchar_t val;
    register volatile uchar_t *ioaddr;

    ioaddr = (volatile uchar_t *) addr;
    val  = *ioaddr;
    return (val & 0xff);
}

__uint32_t
_evo_outhw(__paddr_t addr, unsigned short val)
{
    register volatile unsigned short *ioaddr;

    ioaddr  = (volatile unsigned short *)addr;
    *ioaddr = val;
    return 0;
}

unsigned short
_evo_inhw(__paddr_t addr)
{
    register unsigned short val;
    register volatile unsigned short *ioaddr;

    ioaddr = (volatile ushort *) addr;
    val  = *ioaddr;
    return (val & 0xffff);
}

__uint32_t
_evo_outw(__paddr_t addr, __uint32_t val)
{
    register volatile unsigned int *ioaddr;

    ioaddr  = (volatile unsigned int *)addr;
    *ioaddr = val;
    return 0;
}

__uint32_t
_evo_inw(__paddr_t addr)
{
    register __uint32_t val;
    register volatile __uint32_t *ioaddr;

    ioaddr = (volatile unsigned int *) addr;
    val  = *ioaddr;
    return (val);
}


int
_evo_i2c_poll()
{
  register int timeo = evo_i2c_timeout;

    /*Begin poll condition*/
    /* poll the i2c bus controller, waiting for it to be ready for a new read/write */
    us_delay(100);
    while( (_evo_inb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL)) & I2C_PIN_STAT) && timeo)
    {
        us_delay(2000);
        timeo--;
    }
    if ( _evo_inb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL))  & I2C_PIN_STAT) {
        _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL),I2C_CMD_STOP);
        return(-1);
    }
    /*End poll condition*/
    return(0);
}

void 
_evo_i2c_slave(uchar_t addr)
{
	us_delay(100);
	 _evo_outb(evobase->i2c+(__paddr_t)EVO_I2C_DATA,addr);    /* send slave addr */
	 msg_printf(DBG, "exiting slave address send\n");
}

void 
_evo_i2c_start()
{
	msg_printf(DBG, "Entering start condition\n");
	us_delay(3000);
	_evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL),I2C_CMD_START);
	msg_printf(DBG,"End start condition\n");
}

int
_evo_i2c_write(int data)
{
	int errcode = 0;
        us_delay(100);	
    	if (_evo_i2c_poll())  {
       		 msg_printf(ERR,"Poll timed out on write: data = 0x%x\n",data);
       		 return(-1);
    	}

    	msg_printf(DBG,"iic write\n");
        _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_DATA),(data & 0xff)); 
	return(0);
}

void
_evo_i2c_stop()
{
    /* poll the i2c bus controller, waiting for it to be ready for stop signal */
    us_delay(100);
    if( _evo_i2c_poll() == 0 ) {
            msg_printf(DBG,"iic stop\n");
    	    _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL),I2C_CMD_STOP);
    }
    else
        msg_printf(ERR,"Poll timed out on stop\n");
}

__uint32_t
_evo_i2c_dev_wr(int addr, uchar_t *data, int len)
{
    register int i;
    uchar_t val;
    __uint32_t errcode = 0;
    
    msg_printf(DBG,"Entering I2C device write\n");

    /**************Begin start condition******************************/
    /* wait for bus to clear */
    msg_printf(DBG,"Waiting for bus to clear\n");
    while( _evo_inb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL)) & I2C_NBB_STAT == 0){
        	us_delay(100);
    }
    msg_printf(DBG,"Bus clear\n");

    /******************** send slave address and start condition */
    _evo_i2c_slave(addr);

    /************************ send start condition*/
    _evo_i2c_start();
    msg_printf(DBG,"End start condition\n");
    /***************End start condition******************************/

    /*************Begin sending data******************************/
    for (i = 0; i < len; i++) {
	msg_printf(DBG, "Sending data: i is %d data[i] is 0x%x\n",i,data[i]); 
	us_delay(100);
    	errcode = _evo_i2c_write(data[i]);
	if (errcode == -1) return (-i+1);
    }
    /***************End sending data******************************/
 
    /*************Begin stop condition******************************/
    _evo_i2c_stop();
    /*************End stop condition******************************/

    /* all done */
    return(0);
}

int
_evo_i2c_dummy_read()
{
	int errcode=0;
	uchar_t tmp;

    	if (_evo_i2c_poll()) {
       	 	msg_printf(DBG,"Poll timed out on dummy read\n");
       	 	errcode = -1;
   	 }

    	if ( errcode == 0 ){
        	/* check for slave acknowledge */
		if ( (_evo_inb(evobase->i2c+(__paddr_t)EVO_I2C_CONTROL) & I2C_LRB_STAT) != 0x00 ){
            		errcode = -2;
		}
	}

    	msg_printf(DBG,"iic dummy read\n");
    	if ( errcode >= 0 ){
        	tmp = _evo_inb((evobase->i2c+(__paddr_t)EVO_I2C_DATA));
	}
    	return(errcode);
}

int
_evo_i2c_read(uchar_t *data, int poll_flag)
{
	int errcode = 0;

    	if ( poll_flag ) {
        	if (_evo_i2c_poll()){
            		msg_printf(ERR,"Poll timed out on read\n");
            		errcode = -1;
        	}
    	}

    	msg_printf(DBG,"iic read\n");
    	*data = _evo_inb((evobase->i2c+(__paddr_t)EVO_I2C_DATA));
	return(errcode);
}

uchar_t
_evo_i2c_dev_rd(int addr, uchar_t *data, int len)
{
    uchar_t tmp;
    register int i;
    int errcode = 0;
    int poll_flag = 1;

    msg_printf(DBG,"Entering I2C device write\n");
    /**************Begin start condition******************************/
    /* wait for bus to clear */
    msg_printf(DBG,"Waiting for bus to clear\n");
    while( _evo_inb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL)) & I2C_NBB_STAT == 0){
        us_delay(100);
    }
    msg_printf(DBG,"Bus clear\n");

    /* send slave address and start condition */
    _evo_i2c_slave(addr);
    _evo_i2c_start();

    /**************End start condition******************************/

    /**************Start dummy read condition******************************/
    /* dummy read, really gives back slave addr */
    errcode = _evo_i2c_dummy_read();

    /**************End dummy read condition******************************/

    for (i = 0; i < len; i++)  {

        /* need to poll the last two bytes */
        if (i >= (len - 2)) {
                if (_evo_i2c_poll())  {
                        msg_printf(ERR,"Poll timed out on rcv\n");
                        return(-(i + 1));
                }
                poll_flag = 0;
        } 
	else {
		poll_flag = 1;
	}

        /* right before reading next to last byte, clear ack bit */
        if ((len == 1) || (i == (len-2))){
    		 _evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL),I2C_CMD_NOACK);
	}

        /* on last byte, set stop before reading */
        if (i == (len - 1)){
    		if( _evo_i2c_poll() == 0 ) {
            		msg_printf(DBG,"iic stop\n");
    	    		_evo_outb((evobase->i2c+(__paddr_t)EVO_I2C_CONTROL),I2C_CMD_STOP);
    		}
    		else {
        		msg_printf(DBG,"Poll timed out on stop\n");
		}
	}

    	/**************Start read condition******************************/
        /* read the data */
	errcode = _evo_i2c_read(data+i, poll_flag);
        if ( errcode == -1 )
           return ( -(i + 1) );
     }
    	/**************End  read condition******************************/
    	return(0);
}

unsigned char*
_evo_PageAlign(unsigned char *pBuf)
{
    __paddr_t		tmp;
    unsigned char 	*retPtr = pBuf;

    msg_printf(SUM, "pageSize 0x%x; pageMask 0x%x\n",
	evobase->pageSize, evobase->pageMask);
    if (((__paddr_t)pBuf) & (evobase->pageSize - 1)) {
	msg_printf(SUM, "About to align the buffer\n");
    	tmp = ((__paddr_t)pBuf) + evobase->pageSize;
    	tmp &= evobase->pageMask;
	retPtr = (unsigned char *) tmp;
	msg_printf(SUM, "retPtr 0x%x\n", retPtr);
    } 
    return (retPtr);
}


void _evo_delay_dots( unsigned char seconds, unsigned char newline )
{
unsigned char i;
	if( newline )
		msg_printf(JUST_DOIT,"\n");
	for(i=0; i < seconds; i++)
	{
		delay( 1000 );
		msg_printf(JUST_DOIT,".");
	}
}

void _evo_delay_countdown( unsigned char seconds, unsigned char newline, unsigned char overlay )
{
unsigned char i;
	if( newline )
		msg_printf(JUST_DOIT,"\n");
	for(i= seconds; i > 0; i--)
	{
		delay( 1000 );
		msg_printf(JUST_DOIT,"%s%3d", overlay ? "\b\b\b": 
				(((seconds-i)% 25) ? "  ": " \n"), i );
	}
}

/**********************************************************************
*
*       Function: _evo_check_vin_status(unsigned char ch_sel, tv_std)
*
*
*       Return: 0 - channel input active & genlocked for tv_std
*                  -1 - error from no input, no genlock, etc.
*
*
***********************************************************************/
int _evo_check_vin_status(unsigned char ch_sel, unsigned char tv_std)
{
unsigned char reg_val, i, first_time = TRUE;

        msg_printf(JUST_DOIT,"\nCHECKING FOR VIDEO INPUT STATUS");
        /* check for video input status */
        for(i = 0; i < 10; i++)
        {
                if(ch_sel==ONE)
		{
			/*XXX Needs revisitingi, redefine macro below for EVO*/
                        /*GAL_IND_R1(evobase, EVO_VIN_CH1_STATUS, reg_val);*/
		}
                else
			/*XXX Needs revisitingi, redefine macro below for EVO*/
                        /*GAL_IND_R1(evobase, EVO_VIN_CH2_STATUS, reg_val);*/

                if( (reg_val & 0x10) == 0)
                                break;
		else if (first_time == TRUE)
		{
			msg_printf(JUST_DOIT,"...PLEASE CONNECT CABLE(S)");
			first_time = FALSE;
		}
		delay( 1000 );
		msg_printf(JUST_DOIT,".");
        }

        /* display failed result messages */
        if(reg_val & 0x10)
        {
               	msg_printf(SUM,"\nCONNECT THE VIDEO SOURCE TO CHANNEL %s AND REPEAT THIS TEST\n", (ch_sel==ONE) ? "ONE":"TWO");
               	return(-1);
        }
        else if( (reg_val & 0x03) != tv_std )
        {
                msg_printf(SUM,"\nSET THE VIDEO STANDARD TO %s AND REPEAT THIS TEST\n", asc_tv_std_p[ tv_std ]);
                return(-1);
        }
        else if(reg_val & 0x10)
        {
                msg_printf(SUM,"\nAUTOPHASE NOT LOCKED,CHECK GENLOCK STATUS, AND REPEAT THIS TEST\n");
                return(-1);
        }
#if 0
        else if(reg_val & 0x20)
        {
                msg_printf(JUST_DOIT,"\nCHANNEL %s INPUT OUT OF RANGE.\
                        CHECK SETTINGS AND REPEAT THIS TEST\n",\
                        (ch_sel==ONE) ? "ONE":"TWO");
                return(-1);
        }
#endif

return(0);
}
/**************************************************************************
*
*	Function: _evo_setup_VGIDMA(unsigned char channel, unsigned char tv_std,
*				unsigned char vidFmt, unsigned char* pBuf,
*				unsigned char op, unsigned int vout_freeze)
*
*	Returns: None.
*
***************************************************************************/
void _evo_setup_VGI1DMA(unsigned char channel, unsigned char tv_std,
			unsigned char vidFmt, unsigned char fields,
			unsigned char* pBuf, 
			unsigned char op, unsigned int vout_freeze)
{
	evobase->chan = channel;
	evobase->tvStd = tv_std;
	evobase->vidFmt = vidFmt;
	evobase->nFields = fields;
	evobase->pVGIDMABuf = (uchar_t *)pBuf;
	evobase->VGIDMAOp = op;
	evobase->vout_freeze = vout_freeze;
}


__uint32_t
evo_waitForSpace(__uint32_t argc, char **argv)
{
    __uint32_t secs, i;

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
                case 's':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &secs);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &secs);
			}
                        break;
        }
        argc--; argv++;
    }

    msg_printf(SUM, "\n");
    for (i = 0; i < secs; i++) {
	msg_printf(SUM, ".");
	if ((pause(2, " ", " ")) != 2) return (0);
	delay (1000);
    }
    msg_printf(SUM, "\n");
    return(-1);

}

/*** Following structures are for bridge initialization ***/

/*
 * DMA Dirmap attributes for Command (desc) channel
 */
#define DEVICE_DMA_ATTRS_CMD                    ( \
        /* BRIDGE_DEV_VIRTUAL_EN */ 0           | \
        BRIDGE_DEV_RT                     | \
        /* BRIDGE_DEV_PREF */ 0                           | \
        /* BRIDGE_DEV_PRECISE */ 0                      | \
        BRIDGE_DEV_COH                          | \
        /* BRIDGE_DEV_BARRIER */ 0              | \
        BRIDGE_DEV_GBR                          )

/*
 * DMA Dirmap attributes for Data channel
 */
#define DEVICE_DMA_ATTRS_DATA                   ( \
        /* BRIDGE_DEV_VIRTUAL_EN */ 0           | \
        BRIDGE_DEV_RT                     | \
        BRIDGE_DEV_PREF                            | \
        /* BRIDGE_DEV_PRECISE */ 0                      | \
        BRIDGE_DEV_COH                          | \
        /* BRIDGE_DEV_BARRIER */ 0              | \
        BRIDGE_DEV_GBR                          )

/* 3 bridge slots in hipri-ring for Max PIO priority over DMAs */
#define GIOBR_ARB_RING_BRIDGE   (BRIDGE_ARB_REQ_WAIT_TICK(2) | \
                                 BRIDGE_ARB_REQ_WAIT_EN(0xFF) | \
                                 BRIDGE_ARB_HPRI_RING_B0 )

struct giobr_bridge_config_s evo_srv_bridge_config =
{
    GIOBR_ARB_FLAGS | GIOBR_DEV_ATTRS | GIOBR_RRB_MAP | GIOBR_INTR_DEV,
    GIOBR_ARB_RING_BRIDGE,
    /* even devices, 0,2,4,6        odd devices 1,3,5,7 */
     {  DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_DATA,
        DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_CMD,
        0,                          0,
        0,                          0},
     {  7,                              7,
        1,                              1,
        0,                              0,
        0,                              0},
    /* {0,0,0,0,4,0,6,0} */  /* for interrupts from cc1 & vgi1  */
    {0,0,0,0,4,0,0,0} /* no interrupts from cc1 */
};


/*
 * BRIDGE ASIC configuration for two VGI1 case
 */

struct giobr_bridge_config_s evo_srv_bridge_config_2vgi1 =
{
    GIOBR_ARB_FLAGS | GIOBR_DEV_ATTRS | GIOBR_RRB_MAP | GIOBR_INTR_DEV,
    GIOBR_ARB_RING_BRIDGE,
    /* even devices, 0,2,4,6        odd devices 1,3,5,7 */
     {  DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_DATA,
        DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_CMD,
        DEVICE_DMA_ATTRS_DATA,      DEVICE_DMA_ATTRS_DATA,
        DEVICE_DMA_ATTRS_CMD,       DEVICE_DMA_ATTRS_CMD},
     {  3,                              3,
        1,                              1,
        3,                              3,
        1,                              1},
    {7,7,7,7,2,3,6,7}
};


/*
 *    evo_br_rrb_alloc: allocate some additional Request-Response 
 *	Buffers (RRB) for the specified slot. Returns 1 if there 
 *	were insufficient free RRBs to satisfy the request, or 0
 *	if the request was fulfilled.
 *
 *      Note that if a request can be partially filled, it will be, 
 *	even if we return failure.
 *
 *      IMPT. NOTE: again we avoid iterating across all the RRBs; 
 *	instead, we form up a word containing one bit for each free RRB,\
 *	 then peel the bits off from the low end.
 */


 __uint32_t
evo_br_rrb_alloc(gioio_slot_t slot, int more)
{
    int                     oops = 0;
    volatile bridgereg_t   *regp;
    bridgereg_t             reg, tmp, bit;

    if (slot & 1) {
      PIO_REG_RD_64(BRIDGE_ODD_RESP,0xffffffff,reg);
    }
    else {
      PIO_REG_RD_64(BRIDGE_EVEN_RESP,0xffffffff,reg);
    }

    tmp = (0x88888888 & ~reg) >> 3;
    while (more-- > 0) {
        bit = LSBIT(tmp);
        if (!bit) {
            oops = 1;
            break;
        }
        tmp &= ~bit;
        reg = ((reg & ~(bit * 15)) | (bit * (8 + slot / 2)));
    }
    if (slot & 1) {
      PIO_REG_WR_64(BRIDGE_ODD_RESP,0xffffffff,reg);
    }
    else {
      PIO_REG_WR_64(BRIDGE_EVEN_RESP,0xffffffff,reg);
    }

    return oops;
}

