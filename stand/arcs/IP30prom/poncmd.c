#ident	"IP30prom/poncmd.c:  $Revision: 1.8 $"

/*
 * poncmd.c -- Mfg DBGPROM pon command code
 */

#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <sys/immu.h>
#include <parser.h>
#include <stringlist.h>
#include <setjmp.h>
#include <gfxgui.h>
#include <menu.h>
#include <libsc.h>
#include <libsk.h>

#include <uif.h>
#include <pon.h>

#if defined(MFG_DBGPROM) && defined(MFG_MAKE)

#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_enet.h"
#include "d_scsi.h"
#include "d_rad.h"
#include "d_rad_util.h"
#include <sys/ns16550.h>

bool_t    pci_sio(__uint32_t argc, char **argv);
bool_t    scsi_regs(__uint32_t argc, char **argv);


int       RadCSpaceTest(int argc, char** argv);
int       RadRamTest(int argc, char** argv);
int       RadStatusTest(int argc, char** argv);




static int poncmd_enet( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index );
static int poncmd_scsi( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index );
static int poncmd_generic( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index );
static int poncmd_duart( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index );
static int poncmd_pci_sio( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index );
static int poncmd_rad( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index );

static int poncmd_Usage( int argc, char *argv[], int index );
static int poncmd_AllUsage( int argc, char *argv[] );

typedef struct {
  char *TestName;
  int  (*Fp)( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index );
  int  TestIndex;
} PON_Cmd_T;


/* The enums and the PON_TestTable need to be in sync */
enum {
  PON_INDEX_SCSI,
  PON_INDEX_NIC,
  PON_INDEX_ENET,
  PON_INDEX_DUART,
  PON_INDEX_PCI_SIO,
  PON_INDEX_RAD,
  PON_INDEX_LAST
};


static PON_Cmd_T PON_TestTable[] = {
  { "scsi"      , poncmd_scsi          , PON_INDEX_SCSI      },
  { "nic"       , poncmd_generic       , PON_INDEX_NIC       },
  { "enet"      , poncmd_enet          , PON_INDEX_ENET      },
  { "duart"     , poncmd_duart         , PON_INDEX_DUART     },
  { "pci_sio"   , poncmd_pci_sio       , PON_INDEX_PCI_SIO   },
  { "rad"       , poncmd_rad           , PON_INDEX_RAD       },

  { NULL        , NULL                 , 0                   },
};



static int
poncmd_rad( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index )
{
  struct string_list av;
  int ac;
  char cmdbuf[256];
  char *pfstr;
  int retcode = 0;
  
  sprintf( cmdbuf, "RadCspaceTest" );
  ac = _argvize( cmdbuf, &av );

  /* Test Ram Configuration registers */
  printf( "RAD Test - Started\n" );
  retcode |= RadCSpaceTest( ac, av.strptrs );

  /* Test Rad Ram */
  sprintf( cmdbuf, "RadRamTest" );
  ac = _argvize( cmdbuf, &av );
  retcode |= RadRamTest( ac, av.strptrs );

  /* Test Status DMA Transfers */
  sprintf( cmdbuf, "RadStatusTest" );
  ac = _argvize( cmdbuf, &av );
  retcode |= RadStatusTest( ac, av.strptrs );

  pfstr = (retcode == 0) ? "PASSED" : "FAILED"; 
  printf( "RAD Test - %s\n", pfstr );

  return( 0 );
}



static int
poncmd_pci_sio( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index )
{
  char *pfstr;
  struct string_list av;
  int ac;
  char cmdbuf[256];
  int port, baud;


  if( argc < 5 ) return( 1 );
 
  /* Syntax
   *
   * pon pci_sio <port> <baud> <string>
   */

  port = atoi( argv[2] );

  /* Valid port values = 0,1 */
  if( (unsigned int)port > 1 ) return( 1 );

  /* TJS - 01/24/1997
   *   Can't figure out what is wrong with testing serial port 0
   *   with serial loopback - so it is not currently not supported.
   */
  if( port == 0 ) {
    printf( "Serial port 0 - currently not supported\n" );
    return( 0 );
  }

  baud = atoi( argv[3] );
  
  /* Valid baud values */
  if( (unsigned int)baud > DIVISOR_TO_BAUD(0x0001, SER_CLK_SPEED(SER_PREDIVISOR)) ) return( 1 );
  if( (unsigned int)baud < DIVISOR_TO_BAUD(0xFFFF, SER_CLK_SPEED(SER_PREDIVISOR)) ) return( 1 );

  /* Check the strlen of arg[4]. */
  if( strlen( argv[4] ) > 128 ) return( 1 );

  sprintf( cmdbuf, "pci_sio -p %d -b %5d -t %s", port, baud, argv[4] );
  printf( "PCI SIO Test - Started: Port %d, Baud = %5d, Data = '%s'\n",
      port, baud, argv[4] );
  ac = _argvize( cmdbuf, &av );
  pfstr = (pci_sio( ac, av.strptrs ) == 0) ? "PASSED" : "FAILED"; 
  printf( "DUART Test - %s\n", pfstr );

  return( 0 );
}



static int
poncmd_duart( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index )
{
  char *pfstr;

  printf( "DUART Test - Started\n" );
  pfstr = (duart_regs( 0, NULL ) == 0) ? "PASSED" : "FAILED"; 
    
  printf( "DUART Test - %s\n", pfstr );
  return( 0 );
}



static int
poncmd_generic( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index )
{
  char *Testname=argv[1];

  switch( index ) {
    case PON_INDEX_NIC:
      pon_nic();
      break;

    default:
      return( 1 );
  }

  return( 0 );
}



static int
poncmd_enet( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index )
{
  int lbtype;
  int speed;

  /* Syntax
   * pon enet <loopback: ext|int> <speed: 10|100>
   *  0   1        2                   3
   */
  if( argc < 4 ) return( 1 );

  if( (strcmp( argv[2], "ext" ) != 0) && 
      (strcmp( argv[2], "int" ) != 0) ) {
    return( 1 );   
  }

  speed = atoi( argv[3] );
  if( (speed != 10) && (speed != 100) ) {
    return( 1 );
  }

  lbtype = (strcmp( argv[2], "ext" ) == 0) ? 
      EXTERNAL_LOOP : IOC3_LOOP;

  lbtype = (lbtype == IOC3_LOOP ) ? IOC3_LOOP :
      ((speed == 10) ? EXTERNAL_LOOP_10 : EXTERNAL_LOOP_100);

  if( lbtype == IOC3_LOOP ) {
    printf( "Ethernet:  Internal Loopback\n" );
  } else {
    printf( "Ethernet:  External, Speed: %3d Mbs\n", speed );
  }

  printf( "Loopback type = %d\n", lbtype );
  enet_loop( lbtype ); 

  return( 0 );
}


#define DIAG_QL_TEST_REGS                   1

static int
poncmd_scsi( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp, int index )
{
  char *pfstr;
  struct string_list av;
  int ac;
  char cmdbuf[256];

  int controller;
  int test=0;

  /* Syntax
   * pon scsi <controller> [regs]
   *  0   1        2             3
   */
  if( argc < 3 ) return( 1 );

  if( argc > 3 ) {

    /* Check valid arguments. */
    test += (strcmp( "regs", argv[3] ) == 0) * DIAG_QL_TEST_REGS;

    if( !test ) return( 1 );
  }

  controller = atoi( argv[2] );

  /* Type check arguments */
  if( controller > 1 ) {
    return( 1 );
  }

  printf( "SCSI Test - Started\n" );

  if( !test ) {

    /* No specific test specified, then do all test. */
    sprintf( cmdbuf, "ARGNAME %d", controller );
    ac = _argvize( cmdbuf, &av );
    pfstr = (scsi_regs( ac, av.strptrs ) == 0) ? "PASSED" : "FAILED"; 

  } else {
    sprintf( cmdbuf, "ARGNAME %d", controller );
    ac = _argvize( cmdbuf, &av );

    switch( test ) {
      case DIAG_QL_TEST_REGS:
        pfstr = (scsi_regs( ac, av.strptrs ) == 0) ? "PASSED" : "FAILED"; 
        break;
    }
  }
  
  printf( "SCSI Test - %s\n", pfstr );

  return( 0 );
}



static int
poncmd_AllUsage( int argc, char *argv[] )
{
  int index;

  for( index=0; PON_TestTable[index].TestName; ++index ) {
    poncmd_Usage( argc, argv, index );
  }

  return( 0 );
}



static int
poncmd_Usage( int argc, char *argv[], int index )
{
  printf( "pon " );

  switch( index ) {
    case PON_INDEX_SCSI:
        printf( "scsi <controller: 0|1> [regs]" );
      break;

    case PON_INDEX_NIC:
        printf( "nic" );
      break;

    case PON_INDEX_ENET:
        printf( "enet <type: ext|int> <speed: 10|100>" );
      break;

    case PON_INDEX_DUART:
        printf( "duart" );
      break;

    case PON_INDEX_PCI_SIO:
        printf( "pci_sio <port: 0|1> <baud: %d-%d> <pattern>",
            (int)DIVISOR_TO_BAUD(0xFFFF, SER_CLK_SPEED(SER_PREDIVISOR)),
            (int)DIVISOR_TO_BAUD(0x0001, SER_CLK_SPEED(SER_PREDIVISOR)) );
      break;

    case PON_INDEX_RAD:
        printf( "rad" );
      break;

    default:
        printf( "poncmd_Usage: ERROR - Index out of range, index = %d", index );
      break;
  }

  printf( "\n" );
  return( 0 );
}


int
poncmd( int argc, char *argv[], char *envp[], struct cmd_table *cmdtblp )
{
  int index;
  int retcode = 0; /* Return code */

  if( argc < 2 ) {
    /* Print usage of 'pon' */
    poncmd_AllUsage( argc, argv );
    return( 0 );
  }
 
  /* Scan PON test table for test name */
  for( index=0; PON_TestTable[index].TestName; ++index ) {

    if( strcmp( argv[1], PON_TestTable[index].TestName ) == 0 ) {

      if( PON_TestTable[index].Fp ) {

        /* Call test table function pointer */
        retcode = (*PON_TestTable[index].Fp)( argc, argv, envp, cmdtblp, 
            PON_TestTable[index].TestIndex );

        /* Print individual usage */
        if( retcode ) poncmd_Usage( argc, argv, index );
      }
      break; /* No need to continue loop */
    }
  }

  if( index >= PON_INDEX_LAST ) {
    /* Command not valid */
    poncmd_AllUsage( argc, argv );
  }

  return( 0 );
}
#endif
