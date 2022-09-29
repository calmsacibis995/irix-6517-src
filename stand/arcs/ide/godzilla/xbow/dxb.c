#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <fault.h>
#include <setjmp.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include <sys/xtalk/xbow_info.h>

#include "ide.h"
#include "ide_msg.h"

#include "gparser.h"


#define PATTERN1      0x5555555555555596
#define PATTERN2      0xaaaaaaaaaaaaaa69
#define PATTERN3      0xa5a5a5a5a5a5a596
#define PATTERN_NO5   0x5555555555055555


/* Parser defines */
enum {
  OPT_h = 0,
  OPT_f,
  OPT_r,
  OPT_t,
  OPT_reset,
  OPT_MAX,
  OPT_END = -1
};

#define MAX_OPTIONS OPT_MAX


/* Module variables */
OPTIONS_T options[MAX_OPTIONS];
GPARSER_T parse[] = {
  { "h", 0        , &options[OPT_h].found, &options[OPT_h].buf[0],
      OPTION_BUF_SIZE },
  { "f", 0        , &options[OPT_f].found, &options[OPT_f].buf[0],
      OPTION_BUF_SIZE },
  { "r", GET_TOKEN, &options[OPT_r].found, &options[OPT_r].buf[0],
      OPTION_BUF_SIZE },
  { "t", GET_TOKEN, &options[OPT_t].found, &options[OPT_t].buf[0],
      OPTION_BUF_SIZE },
  { "reset", 0    , &options[OPT_reset].found, &options[OPT_reset].buf[0],
      OPTION_BUF_SIZE },
  { NULL, NULL, NULL, NULL },
};


static int s_OptVerbose=0;
static int s_OptForever=0;
static int s_OptReset=0;

static int s_PortValidArray[] = { 9, 10, 11, 12, 13, -1 };

static __uint64_t s_CntA = 0;  /* Total perf counter A */
static __uint64_t s_CntB = 0;  /* Total perf counter B */

#define MAX_TIME               60
#define DEFAULT_TIME           5

#define DELAY_uSEC             1
#define DELAY_mSEC             (1000 * DELAY_uSEC)
#define DELAY_SEC              (1000 * DELAY_mSEC)
#define nSEC_TICK              80
#define TICKS_TO_TIME( tick )  ( (tick)*(nSEC_TICK) ) 
#define TIME_TO_TICKS( time )  ( (time)/(nSEC_TICK) ) 
#define SIG_DIGITS             100
#define PKT_PER_nSEC           20
#define FAIL_UTIL_PERCENT      90


#define PERF_A                 0
#define PERF_B                 1


typedef struct {
  __uint64_t TotalTime;
  __uint64_t StartTime;
  __uint64_t pTime;

  __uint64_t TotalCnt;
  __uint64_t pCnt;
} PERFCNTR_T;

#define MAX_PORTS              8
#define PORT_TESTING           1
#define PORTINFO(port)         PortInfo[(port) & (MAX_PORTS-1)]

typedef struct {
  int        Status;
  PERFCNTR_T PerfCntr[2];
  __uint64_t RcvErrs; 
  __uint64_t XmtRetries; 
} PORTINFO_T;


typedef struct {
  PORTINFO_T PortInfo[MAX_PORTS];
  PERFCNTR_T PerfCntr[2];
} PROGINFO_T;

PROGINFO_T gProgInfo;



/* Prototypes */
static int Usage( void );

/* External prototypes */
void rot_pattern_write(ulong *address, ulong size, ulong data);


static int
Usage( void )
{
  msg_printf( ERR,
    "\n"
    "Usage:\n"
    "  dxb [-reset] [-t <run time>] [-h] [-f] [-r <report time>]\n"
    "    -f             : Run forever, overrides -t option\n"
    "    -t <run time>  : Run for specific time, in seconds (1-%02d)\n"
    "    -r <time>      : Display report every <time> second\n"
    "    -h             : Print help\n"
    "    -reset         : Reset ports, and exit\n"
    "\n", MAX_TIME );
  return( 0 );
}


static int
xlink_check(xbow_t *xbp, int port, int flag)
{
  xbowreg_t status;

  status = (xbp->xb_link(port).link_status & XB_STAT_LINKALIVE) &&
               (xbp->xb_link(port).link_aux_status & XB_AUX_STAT_PRESENT);

  if( flag ) {
    /* Read the status and clear error bits. */
    flag = xbp->xb_link(port).link_status_clr;
  }

  return( status ? 1 : 0 );
}

int
xbDisplayLinkRegs( xbow_t *xbp, int widget )
{

  msg_printf( VRB, "  Link registers (widget %02d)\n", widget );
  msg_printf( VRB, "    Control       = 0x%08X\n", 
      (__uint64_t)xbp->xb_link(widget).link_control );
  msg_printf( VRB, "    Arb Upper     = 0x%08X\n", 
      (__uint64_t)xbp->xb_link(widget).link_arb_upper );
  msg_printf( VRB, "    Arb Lower     = 0x%08X\n", 
      (__uint64_t)xbp->xb_link(widget).link_arb_lower );
  msg_printf( VRB, "    Status        = 0x%08X\n", 
      (__uint64_t)xbp->xb_link(widget).link_status );
  msg_printf( VRB, "    Aux Status    = 0x%08X\n", 
      (__uint64_t)xbp->xb_link(widget).link_aux_status );

  return( 0 );
}


int IsValidPort( int port )
{
  int i;

  for( i=0; s_PortValidArray[i] != -1; ++i ) {
    if( port == s_PortValidArray[i] ) return( 1 );
  }

  return( 0 );
}


static int
xbSetLinkSelect( xbow_t *xbp, int csel, int port )
{
	xbow_perfcount_t pc;
  
  
  pc.xb_perf.link_select = port;

  if( csel == PERF_A ) {

    /* Performance counter A */
    xbp->xb_perf_ctr_a = pc.xb_counter_val;

  } else {

    /* Performance counter B */
    xbp->xb_perf_ctr_b = pc.xb_counter_val;

  } 
  
  return( 0 );
}


static int
xbSetLinkCredits( xbow_t *xbp, int port, int credits )
{
	xbow_linkctrl_t ctl;

  ctl.xbl_ctrlword = xbp->xb_link(port).link_control;
  ctl.xb_linkcontrol.llp_credit = credits;

  xbp->xb_link(port).link_control = ctl.xbl_ctrlword;

  return( 0 );
}


static int
xbSetLinkPerfMode( xbow_t *xbp, int port, int mode )
{
	xbow_linkctrl_t ctl;

  ctl.xbl_ctrlword = xbp->xb_link(port).link_control;
  ctl.xb_linkcontrol.perf_mode = mode;

  xbp->xb_link(port).link_control = ctl.xbl_ctrlword;

  return( 0 );
}


int
reportPerf( xbow_t *xbp, PROGINFO_T *PIp )
{
  int port;
  int failed=0;
  int numfailed=0;
  __uint64_t time, ticks, count;
  __uint64_t util;

  msg_printf( INFO, "Xtown test Results\n" );

  /* Display format */

  /*
   * Port   Total Ticks        Total Count        Rcv Errors         Xmt Retries
   *  xx    aaaabbbbccccdddd   aaaabbbbccccdddd   aaaabbbbccccdddd   aaaabbbbccccdddd
   */


  msg_printf( INFO, "  Port   Status       Pass/Fail   %% Util     Time (ns)   Count      Rcv Errs             Xmt Retry\n" );
  msg_printf( INFO, "  ----   ----------   ---------   --------   ---------   --------   ------------------   ------------------\n" );

  for( port=8; port<(MAX_PORTS+8); ++port ) {

    if( !IsValidPort(port) ) continue;

    msg_printf( INFO, "   %02d :  %s", port, 
        (PIp->PORTINFO(port).Status == PORT_TESTING) ? "Testing      " : "-\n" );
     
    if( PIp->PORTINFO(port).Status != PORT_TESTING ) continue;

    count = PIp->PORTINFO(port).PerfCntr[PERF_A].TotalCnt;
    time  = TICKS_TO_TIME( PIp->PORTINFO(port).PerfCntr[PERF_A].TotalTime );

    util  = (PKT_PER_nSEC * count * 100 * SIG_DIGITS) / time;

    failed= (util < (FAIL_UTIL_PERCENT * SIG_DIGITS)) ||
                PIp->PORTINFO(port).RcvErrs || 
                PIp->PORTINFO(port).XmtRetries;

    numfailed += failed;
    
    /* Display results */
    msg_printf( INFO, "%s   ", 
        (!failed) ? "PASSED   " : "FAILED   " );
    msg_printf( INFO, "%02llu.%02llu      ", (util / SIG_DIGITS), (util % SIG_DIGITS) );
    msg_printf( INFO, "%09llu   ", time );
    msg_printf( INFO, "%08llu   ", count );

    msg_printf( INFO, "0x%016llX   ",
        (__uint64_t)PIp->PORTINFO(port).RcvErrs );
    msg_printf( INFO, "0x%016llX\n",
        (__uint64_t)PIp->PORTINFO(port).XmtRetries );

    xbDisplayLinkRegs( xbp, port );
  }

  msg_printf( INFO, "\n" );
  return( numfailed );
}


int
calcPerf( xbow_t *xbp, PROGINFO_T *PIp, __uint64_t tdelay )
{
  xbowreg_t status, auxstatus;
	volatile heartreg_t htick;
	xbow_perfcount_t xperfa, xperfb;
  __uint64_t count;
  int port;
  __uint64_t pCntA, CntA;
	volatile heartreg_t stime;


  for( port=8; port<(MAX_PORTS+8); ++port ) {

    if( PIp->PORTINFO(port).Status != PORT_TESTING ) continue;


    /* Disable perfmode */
    xbSetLinkPerfMode( xbp, port, XBOW_MONITOR_NONE );

    /* Set control */
    xbSetLinkSelect( xbp, PERF_A, port ); 

    /* Give delay */
    DELAY( 1 * DELAY_uSEC );

    /* Save previous counters */
    pCntA = xbp->xb_perf_ctr_a & XBOW_COUNTER_MASK;

    /* Timestamp */
    stime = PIp->PORTINFO(port).PerfCntr[PERF_A].StartTime = HEART_PIU_K1PTR->h_count;

    /* Start Monitoring */
    xbSetLinkPerfMode( xbp, port, XBOW_MONITOR_DEST_LINK );

    DELAY( tdelay );

    CntA = xbp->xb_perf_ctr_a & XBOW_COUNTER_MASK;
    htick = HEART_PIU_K1PTR->h_count;

    count = (CntA >= pCntA) ? (CntA - pCntA) : 
                ((XBOW_COUNTER_MASK + 1 - pCntA) + CntA);

    /* Do totals */
    PIp->PORTINFO(port).PerfCntr[PERF_A].TotalCnt  = count;

    PIp->PORTINFO(port).PerfCntr[PERF_A].TotalTime = htick - stime;
    PIp->PORTINFO(port).PerfCntr[PERF_A].pTime = htick;

    /* Check error rates */
    status    = xbp->xb_link(port).link_status_clr;
    auxstatus = xbp->xb_link(port).link_aux_status;
    PIp->PORTINFO(port).RcvErrs    = (status & XB_STAT_RCV_CNT_OFLOW_ERR) ?
        (__uint64_t)-1 : (PIp->PORTINFO(port).RcvErrs + ((auxstatus & 0xFF000000) >> 24));
    PIp->PORTINFO(port).XmtRetries = (status & XB_STAT_XMT_CNT_OFLOW_ERR) ?
        (__uint64_t)-1 : (PIp->PORTINFO(port).XmtRetries + ((auxstatus & 0xFF000000) >> 24));
  }

  return( 0 );
}


int
dxb(int argc, char **argv, char **envp)
{
  /*
   * Notes:
   *
   *   On the frontplane we will be testing all Xtown cards
   *   which will have a loopback cable plugged into them, thus
   *   sending a packet forever.
   *
   *   Valid ports are 9,10,11,12,13.
   *
   *   Algorithm:
   *     1.  Scan ports (9-13) for a Xtown card:  this card is assumed
   *         to be a card with a Aux status of XB_AUX_STAT_PRESENT, the
   *         XB_STAT_LINKALIVE will be active.
   *
   *     2.  Change the credits to a larver value than the default value.
   *
   *     3.  Write uncached accelerated writes to maximize signal noise.
   *
   *     4.  In the calculation loop, monitor one widget at a time and record
   *         results.
   */
  int Nargs=0;
  char argbuf[256];
  char *strp=NULL, *endp;
  int  port;
	volatile heartreg_t htick;
	__uint64_t dTicks, dReportTicks;
  __uint64_t updateTime = (1000 * DELAY_uSEC);
  __uint64_t ReportTime = (2    * DELAY_SEC);
  __uint64_t ReportTick;
  __uint64_t EndTick;
  __uint64_t runTime    = DEFAULT_TIME * DELAY_SEC;
  xbow_t *xbowp = XBOW_K1PTR;

  /*
   * Initialize ProgInfo
   */
  memset( &gProgInfo, 0, sizeof(gProgInfo) );

  memset( &options[0], 0, sizeof(options) );

  gParser( argc, argv, &Nargs, parse, &argbuf[0], sizeof(argbuf) );

  if( options[OPT_h].found ) {
    Usage();
    return( 0 );
  }

  s_OptForever    = options[OPT_f].found;
  s_OptReset      = options[OPT_reset].found;



  if( options[OPT_t].found ) {
    strp = _strtok( &options[OPT_t].buf[0], " \t\n" );
    if( !strp ) {
      msg_printf( ERR, "Error - Missing time arg.\n" );
      Usage();
      return( 1 );
    }

    runTime = strtoull( &options[OPT_t].buf[0], &endp, 0 );
    if( *endp != '\0' ) {
      msg_printf( ERR, "Error - Cannot convert time arg to number.\n" );
      Usage();
      return( 1 );
    }

    if( !runTime || (runTime > MAX_TIME) ) {
      msg_printf( ERR, "Error - Time invalid.  1-02%d.\n", MAX_TIME );
      Usage();
      return( 1 );
    }

    msg_printf( VRB, "Run    Time (sec)  = %lld\n", runTime );
    runTime *= DELAY_SEC;
  }
  runTime = ( s_OptForever ) ? -1 : runTime;
  msg_printf( VRB, "Run    Time (usec) = %lld\n", runTime );


  if( options[OPT_r].found ) {
    strp = _strtok( &options[OPT_r].buf[0], " \t\n" );
    if( !strp ) {
      msg_printf( ERR, "Error - Missing time arg.\n" );
      Usage();
      return( 1 );
    }

    ReportTime = strtoull( &options[OPT_r].buf[0], &endp, 0 );
    if( *endp != '\0' ) {
      msg_printf( ERR, "Error - Cannot convert time arg to number.\n" );
      Usage();
      return( 1 );
    }

    if( !ReportTime ) {
      msg_printf( ERR, "Error - Time invalid.\n" );
      Usage();
      return( 1 );
    }

    msg_printf( VRB, "Report Time (sec)  = %lld\n", ReportTime );
    ReportTime *= DELAY_SEC;
    msg_printf( VRB, "Report Time (usec) = %lld\n", ReportTime );
  }


#ifdef NOT_NOW
  gProgInfo.PerfCntr[PERF_A].pCnt = xbowp->xb_perf_ctr_a & XBOW_COUNTER_MASK; 
  gProgInfo.PerfCntr[PERF_B].pCnt = xbowp->xb_perf_ctr_b & XBOW_COUNTER_MASK; 


  gProgInfo.PerfCntr[PERF_A].pTime = 
    gProgInfo.PerfCntr[PERF_A].StartTime = 
    gProgInfo.PerfCntr[PERF_B].pTime =
    gProgInfo.PerfCntr[PERF_B].StartTime = htick; 
#endif

  msg_printf( VRB, "Resetting ports :\n" );
  for( port=8; port<(MAX_PORTS+8); ++port ) {

    msg_printf( VRB, "  %02d .. ", port );

    if( xlink_check( xbowp, port, 1 ) && IsValidPort( port ) ) {

      msg_printf( VRB, "Reset\n" );
      xbowp->xb_link(port).link_reset = 0;
      DELAY( 10 * DELAY_mSEC );
      continue;
    }

    msg_printf( VRB, "\n" );
  }
  if( s_OptReset ) return( 0 );

  /* Start write loops */
  msg_printf( VRB, "Checking  ports :\n" );
  for( port=8; port<(MAX_PORTS+8); ++port ) {

    msg_printf( VRB, "  %02d .. ", port );
    if( xlink_check( xbowp, port, 1 ) && IsValidPort( port ) ) {

      msg_printf( VRB, "Testing\n" );
      xbSetLinkCredits( xbowp, port, 5 );
      gProgInfo.PORTINFO(port).Status = PORT_TESTING;
      rot_pattern_write((unsigned long *)(MAIN_WIDGET(port) | 0xBA00000000000000),1,PATTERN1);
      rot_pattern_write((unsigned long *)(MAIN_WIDGET(port) | 0xBA00000000000000),1,PATTERN2);
      continue;
    }

    msg_printf( VRB, "Skipped\n" );
  }
  msg_printf( VRB, "\n" );

  DELAY( 1 * DELAY_SEC );

	htick        = HEART_PIU_K1PTR->h_count;
  EndTick      = htick + TIME_TO_TICKS( runTime    * 1000 );
  ReportTick   = htick + TIME_TO_TICKS( ReportTime * 1000 );
  dReportTicks = TIME_TO_TICKS( ReportTime * 1000 );

  while( htick < EndTick ) {

    calcPerf( xbowp, &gProgInfo, updateTime );

    if( htick >= ReportTick ) {

      /* No need to continue if failed test */
      if( reportPerf( xbowp, &gProgInfo ) ) break;
      ReportTick = dReportTicks + htick;
    }

	  htick = HEART_PIU_K1PTR->h_count;
  } 

  reportPerf( xbowp, &gProgInfo );

  return( 0 );  
}
