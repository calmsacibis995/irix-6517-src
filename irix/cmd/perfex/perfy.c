#include "counts.h"

#include <strings.h>
#include <limits.h>
#include <math.h>

typedef	struct _Events {
  evcnt_t	count;
  double	cost[NCOSTS];
  int		counter;
} Events;

typedef	struct _Statistics {
  char		*name;
  double	value;
} Statistics;

#define	CYCLESEVENT	( 0)
#define	ISSUEDINSTS	( 1)
#define	ISSUEDLOADS	( 2)
#define	ISSUEDSTORES	( 3)
#define	DECODEDBRANCHES	( 6)
#define	L2QWRITEBACKS	( 7)
#define	L1ICACHEMISSES	( 9)
#define	L2ICACHEMISSES	(10)
#define	L2IMISPREDICT	(11)
#define	GRADUATEDINSTS	(15)
#define	CYCLESEVENT2	(16)
#define	GRADUATEDLOADS	(18)
#define	GRADUATEDSTORES	(19)
#define	FLOPS		(21)
#define	L1QWRITEBACKS	(22)
#define	TLBMISSES	(23)
#define	MISSEDBRANCHES	(24)
#define	L1DCACHEMISSES	(25)
#define	L2DCACHEMISSES	(26)
#define	L2DMISPREDICT	(27)

/* only valid for 3.x and higher R10k */
#define CYCLESBUSY      (14)

/* only valid for R12k */
#define MHTPOPCYCLES    ( 4)
#define EXPREFETCHES    (16)
#define PREFETCHDMISS   (17)

/* The default cost of each event in processor cycles (unless the
 * value is negative, in which case its absolute value is the cost
 * in nsec).  The 3 values for each event are:
 *
 *	{ minimum time, typical time, maximum time }
 *
 * The flag indicates whether the table has ever been initialized.
 * At print time the table will be initialized to the the values
 * in the system-wide cost file if the flag is FALSE.
 */
static	char	*CostNames[NCOSTS] = {"Minimum", "Typical", "Maximum"};
static	int	DefaultCounterCostsInitialized = FALSE;
static	int	UserDefinedTable = FALSE;
static	double	*DefaultCounterCosts;

static	double	DefaultCounterCosts0[NCOUNTERS][NCOSTS] = {
  {    0.00,    0.00,    0.00}, /*  0 Cycles.....................................................*/
  {    0.00,    0.00,    0.00}, /*  1 Issued instructions........................................*/
  {    0.00,    0.00,    0.00}, /*  2 Issued loads...............................................*/
  {    0.00,    0.00,    0.00}, /*  3 Issued stores..............................................*/
  {    0.00,    0.00,    0.00}, /*  4 Issued store conditionals..................................*/
  {    0.00,    0.00,    0.00}, /*  5 Failed store conditionals..................................*/
  {    0.00,    0.00,    0.00}, /*  6 Decoded branches...........................................*/
  {    0.00,    0.00,    0.00}, /*  7 Quadwords written back from scache.........................*/
  {    0.00,    0.00,    0.00}, /*  8 Correctable scache data array ECC errors...................*/
  {    0.00,    0.00,    0.00}, /*  9 Primary instruction cache misses...........................*/
  {    0.00,    0.00,    0.00}, /* 10 Secondary instruction cache misses.........................*/
  {    0.00,    0.00,    0.00}, /* 11 Instruction misprediction from scache way prediction table.*/
  {    0.00,    0.00,    0.00}, /* 12 External interventions.....................................*/
  {    0.00,    0.00,    0.00}, /* 13 External invalidations.....................................*/
  {    0.00,    0.00,    0.00}, /* 14 Virtual coherency conditions...............................*/
  {    0.00,    0.00,    0.00}, /* 15 Graduated instructions.....................................*/
  {    0.00,    0.00,    0.00}, /* 16 Cycles.....................................................*/
  {    0.00,    0.00,    0.00}, /* 17 Graduated instructions.....................................*/
  {    0.00,    0.00,    0.00}, /* 18 Graduated loads............................................*/
  {    0.00,    0.00,    0.00}, /* 19 Graduated stores...........................................*/
  {    0.00,    0.00,    0.00}, /* 20 Graduated store conditionals...............................*/
  {    0.00,    0.00,    0.00}, /* 21 Graduated floating point instructions......................*/
  {    0.00,    0.00,    0.00}, /* 22 Quadwords written back from primary data cache.............*/
  {    0.00,    0.00,    0.00}, /* 23 TLB misses.................................................*/
  {    0.00,    0.00,    0.00}, /* 24 Mispredicted branches......................................*/
  {    0.00,    0.00,    0.00}, /* 25 Primary data cache misses..................................*/
  {    0.00,    0.00,    0.00}, /* 26 Secondary data cache misses................................*/
  {    0.00,    0.00,    0.00}, /* 27 Data misprediction from scache way prediction table........*/
  {    0.00,    0.00,    0.00}, /* 28 External intervention hits in scache.......................*/
  {    0.00,    0.00,    0.00}, /* 29 External invalidation hits in scache.......................*/
  {    0.00,    0.00,    0.00}, /* 30 Store/prefetch exclusive to clean block in scache..........*/
  {    0.00,    0.00,    0.00}  /* 31 Store/prefetch exclusive to shared block in scache.........*/
};
static	double	DefaultCounterCosts25[NCOUNTERS][NCOSTS] = {
  {    1.00,    1.00,    1.00}, /*  0 Cycles.....................................................*/
  {    0.00,    0.00,    1.00}, /*  1 Issued instructions........................................*/
  {    1.00,    1.00,    1.00}, /*  2 Issued loads...............................................*/
  {    1.00,    1.00,    1.00}, /*  3 Issued stores..............................................*/
  {    1.00,    1.00,    1.00}, /*  4 Issued store conditionals..................................*/
  {    1.00,    1.00,    1.00}, /*  5 Failed store conditionals..................................*/
  {    1.00,    1.00,    1.00}, /*  6 Decoded branches...........................................*/
  {    7.34,    7.34,   11.33}, /*  7 Quadwords written back from scache.........................*/
  {    0.00,    0.00,    1.00}, /*  8 Correctable scache data array ECC errors...................*/
  {    5.78,   18.06,   18.06}, /*  9 Primary instruction cache misses...........................*/
  {  100.03,  192.12,  200.06}, /* 10 Secondary instruction cache misses.........................*/
  {    0.00,    0.00,    1.00}, /* 11 Instruction misprediction from scache way prediction table.*/
  {    0.00,    0.00,    0.00}, /* 12 External interventions.....................................*/
  {    0.00,    0.00,    0.00}, /* 13 External invalidations.....................................*/
  {    0.00,    0.00,    0.00}, /* 14 Virtual coherency conditions...............................*/
  {    0.00,    0.00,    1.00}, /* 15 Graduated instructions.....................................*/
  {    1.00,    1.00,    1.00}, /* 16 Cycles.....................................................*/
  {    0.00,    0.00,    1.00}, /* 17 Graduated instructions.....................................*/
  {    1.00,    1.00,    1.00}, /* 18 Graduated loads............................................*/
  {    1.00,    1.00,    1.00}, /* 19 Graduated stores...........................................*/
  {    1.00,    1.00,    1.00}, /* 20 Graduated store conditionals...............................*/
  {    0.50,    1.00,   52.00}, /* 21 Graduated floating point instructions......................*/
  {    3.07,    3.85,    4.37}, /* 22 Quadwords written back from primary data cache.............*/
  {   50.95,   50.95,   50.95}, /* 23 TLB misses.................................................*/
  {    0.55,    1.50,    5.63}, /* 24 Mispredicted branches......................................*/
  {    2.89,    9.03,    9.03}, /* 25 Primary data cache misses..................................*/
  {  100.03,  192.12,  200.06}, /* 26 Secondary data cache misses................................*/
  {    0.00,    0.00,    1.00}, /* 27 Data misprediction from scache way prediction table........*/
  {    0.00,    0.00,    0.00}, /* 28 External intervention hits in scache.......................*/
  {    0.00,    0.00,    0.00}, /* 29 External invalidation hits in scache.......................*/
  {    1.00,    1.00,    1.00}, /* 30 Store/prefetch exclusive to clean block in scache..........*/
  {    1.00,    1.00,    1.00}  /* 31 Store/prefetch exclusive to shared block in scache.........*/
};
static	double	DefaultCounterCosts27[NCOUNTERS][NCOSTS] = {
{      1.00,      1.00,      1.00}, /*  0 Cycles......................................................*/
{      0.00,      0.00,      1.00}, /*  1 Issued instructions.........................................*/
{      1.00,      1.00,      1.00}, /*  2 Issued loads................................................*/
{      1.00,      1.00,      1.00}, /*  3 Issued stores...............................................*/
{      1.00,      1.00,      1.00}, /*  4 Issued store conditionals...................................*/
{      1.00,      1.00,      1.00}, /*  5 Failed store conditionals...................................*/
{      1.00,      1.00,      1.00}, /*  6 Decoded branches............................................*/
{      4.23,      6.40,      6.40}, /*  7 Quadwords written back from scache..........................*/
{      0.00,      0.00,      1.00}, /*  8 Correctable scache data array ECC errors....................*/
{      5.63,     18.02,     18.02}, /*  9 Primary instruction cache misses............................*/
{     49.36,     75.50,     84.00}, /*  10 Secondary instruction cache misses......................... */
{      0.00,      0.00,      1.00}, /*  11 Instruction misprediction from scache way prediction table. */
{      0.00,      0.00,      0.00}, /*  12 External interventions..................................... */
{      0.00,      0.00,      0.00}, /*  13 External invalidations..................................... */
{      0.00,      0.00,      0.00}, /*  14 Virtual coherency conditions............................... */
{      0.00,      0.00,      1.00}, /*  15 Graduated instructions..................................... */
{      1.00,      1.00,      1.00}, /*  16 Cycles..................................................... */
{      0.00,      0.00,      1.00}, /*  17 Graduated instructions..................................... */
{      1.00,      1.00,      1.00}, /*  18 Graduated loads............................................ */
{      1.00,      1.00,      1.00}, /*  19 Graduated stores........................................... */
{      1.00,      1.00,      1.00}, /*  20 Graduated store conditionals............................... */
{      0.50,      1.00,     52.00}, /*  21 Graduated floating point instructions...................... */
{      3.14,      3.85,      4.45}, /*  22 Quadwords written back from primary data cache............. */
{     68.09,     68.09,     68.09}, /*  23 TLB misses................................................. */
{      0.64,      1.42,      5.22}, /*  24 Mispredicted branches...................................... */
{      2.82,      9.01,      9.01}, /*  25 Primary data cache misses.................................. */
{     49.36,     75.50,     84.00}, /*  26 Secondary data cache misses................................ */
{      0.00,      0.00,      1.00}, /*  27 Data misprediction from scache way prediction table........ */
{      0.00,      0.00,      0.00}, /*  28 External intervention hits in scache....................... */
{      0.00,      0.00,      0.00}, /*  29 External invalidation hits in scache....................... */
{      1.00,      1.00,      1.00}, /*  30 Store/prefetch exclusive to clean block in scache.......... */
{      1.00,      1.00,      1.00}  /*  31 Store/prefetch exclusive to shared block in scache......... */
};

static	double	DefaultCounterCosts28[NCOUNTERS][NCOSTS] = {
  {    1.00,    1.00,    1.00}, /*  0 Cycles.....................................................*/
  {    0.00,    0.00,    1.00}, /*  1 Issued instructions........................................*/
  {    1.00,    1.00,    1.00}, /*  2 Issued loads...............................................*/
  {    1.00,    1.00,    1.00}, /*  3 Issued stores..............................................*/
  {    1.00,    1.00,    1.00}, /*  4 Issued store conditionals..................................*/
  {    1.00,    1.00,    1.00}, /*  5 Failed store conditionals..................................*/
  {    1.00,    1.00,    1.00}, /*  6 Decoded branches...........................................*/
  {   16.64,   18.19,   18.19}, /*  7 Quadwords written back from scache.........................*/
  {    0.00,    0.00,    1.00}, /*  8 Correctable scache data array ECC errors...................*/
  {    5.84,   17.59,   17.59}, /*  9 Primary instruction cache misses...........................*/
  {  156.44,  156.44,  163.13}, /* 10 Secondary instruction cache misses.........................*/
  {    0.00,    0.00,    1.00}, /* 11 Instruction misprediction from scache way prediction table.*/
  {    0.00,    0.00,    0.00}, /* 12 External interventions.....................................*/
  {    0.00,    0.00,    0.00}, /* 13 External invalidations.....................................*/
  {    0.00,    0.00,    0.00}, /* 14 Virtual coherency conditions...............................*/
  {    0.00,    0.00,    1.00}, /* 15 Graduated instructions.....................................*/
  {    1.00,    1.00,    1.00}, /* 16 Cycles.....................................................*/
  {    0.00,    0.00,    1.00}, /* 17 Graduated instructions.....................................*/
  {    1.00,    1.00,    1.00}, /* 18 Graduated loads............................................*/
  {    1.00,    1.00,    1.00}, /* 19 Graduated stores...........................................*/
  {    1.00,    1.00,    1.00}, /* 20 Graduated store conditionals...............................*/
  {    0.50,    1.00,   52.00}, /* 21 Graduated floating point instructions......................*/
  {    3.01,    3.94,    4.39}, /* 22 Quadwords written back from primary data cache.............*/
  {   47.96,   47.96,   47.96}, /* 23 TLB misses.................................................*/
  {    0.55,    1.42,    5.33}, /* 24 Mispredicted branches......................................*/
  {    2.92,    8.80,    8.80}, /* 25 Primary data cache misses..................................*/
  {  156.44,  156.44,  163.13}, /* 26 Secondary data cache misses................................*/
  {    0.00,    0.00,    1.00}, /* 27 Data misprediction from scache way prediction table........*/
  {    0.00,    0.00,    0.00}, /* 28 External intervention hits in scache.......................*/
  {    0.00,    0.00,    0.00}, /* 29 External invalidation hits in scache.......................*/
  {    1.00,    1.00,    1.00}, /* 30 Store/prefetch exclusive to clean block in scache..........*/
  {    1.00,    1.00,    1.00}  /* 31 Store/prefetch exclusive to shared block in scache.........*/
};
static	double	DefaultCounterCosts30[NCOUNTERS][NCOSTS] = {
  {    1.00,    1.00,    1.00}, /*  0 Cycles.....................................................*/
  {    0.00,    0.00,    1.00}, /*  1 Issued instructions........................................*/
  {    1.00,    1.00,    1.00}, /*  2 Issued loads...............................................*/
  {    1.00,    1.00,    1.00}, /*  3 Issued stores..............................................*/
  {    1.00,    1.00,    1.00}, /*  4 Issued store conditionals..................................*/
  {    1.00,    1.00,    1.00}, /*  5 Failed store conditionals..................................*/
  {    1.00,    1.00,    1.00}, /*  6 Decoded branches...........................................*/
  {    3.72,    3.86,    3.86}, /*  7 Quadwords written back from scache.........................*/
  {    0.00,    0.00,    1.00}, /*  8 Correctable scache data array ECC errors...................*/
  {    5.68,   18.02,   18.13}, /*  9 Primary instruction cache misses...........................*/
  {   41.83,   77.31,   81.87}, /* 10 Secondary instruction cache misses.........................*/
  {    0.00,    0.00,    1.00}, /* 11 Instruction misprediction from scache way prediction table.*/
  {    0.00,    0.00,    0.00}, /* 12 External interventions.....................................*/
  {    0.00,    0.00,    0.00}, /* 13 External invalidations.....................................*/
  {    0.00,    0.00,    0.00}, /* 14 Virtual coherency conditions...............................*/
  {    0.00,    0.00,    1.00}, /* 15 Graduated instructions.....................................*/
  {    1.00,    1.00,    1.00}, /* 16 Cycles.....................................................*/
  {    0.00,    0.00,    1.00}, /* 17 Graduated instructions.....................................*/
  {    1.00,    1.00,    1.00}, /* 18 Graduated loads............................................*/
  {    1.00,    1.00,    1.00}, /* 19 Graduated stores...........................................*/
  {    1.00,    1.00,    1.00}, /* 20 Graduated store conditionals...............................*/
  {    0.50,    1.00,   52.00}, /* 21 Graduated floating point instructions......................*/
  {    3.10,    3.85,    4.49}, /* 22 Quadwords written back from primary data cache.............*/
  {   58.08,   58.08,   58.08}, /* 23 TLB misses.................................................*/
  {    0.59,    1.43,    5.37}, /* 24 Mispredicted branches......................................*/
  {    2.84,    9.01,    9.06}, /* 25 Primary data cache misses..................................*/
  {   41.83,   77.22,   81.87}, /* 26 Secondary data cache misses................................*/
  {    0.00,    0.00,    1.00}, /* 27 Data misprediction from scache way prediction table........*/
  {    0.00,    0.00,    0.00}, /* 28 External intervention hits in scache.......................*/
  {    0.00,    0.00,    0.00}, /* 29 External invalidation hits in scache.......................*/
  {    1.00,    1.00,    1.00}, /* 30 Store/prefetch exclusive to clean block in scache..........*/
  {    1.00,    1.00,    1.00}  /* 31 Store/prefetch exclusive to shared block in scache.........*/
};

static  double  DefaultCounterCosts32_FPGA[NCOUNTERS][NCOSTS] = {
{      1.00,      1.00,      1.00}, /*  0 Cycles................................................................... */
{      0.00,      0.00,      1.00}, /*  1 Issued instructions...................................................... */
{      1.00,      1.00,      1.00}, /*  2 Issued loads............................................................. */
{      1.00,      1.00,      1.00}, /*  3 Issued stores............................................................ */
{      1.00,      1.00,      1.00}, /*  4 Issued store conditionals................................................ */
{      1.00,      1.00,      1.00}, /*  5 Failed store conditionals................................................ */
{      1.00,      1.00,      1.00}, /*  6 Decoded branches......................................................... */
{    -19.34,    -32.45,    -43.10}, /*  7 Quadwords written back from scache....................................... */
{      0.00,      0.00,      1.00}, /*  8 Correctable scache data array ECC errors................................. */
{      3.52,     18.96,     19.20}, /*  9 Primary instruction cache misses......................................... */
{   -1126.02,   -1126.02,   -1170.75}, /*  10 Secondary instruction cache misses....................................... */
{      0.00,      0.00,      1.00}, /*  11 Instruction misprediction from scache way prediction table............... */
{      0.00,      0.00,      0.00}, /*  12 External interventions................................................... */
{      0.00,      0.00,      0.00}, /*  13 External invalidations................................................... */
{      0.00,      0.00,      0.00}, /*  14 Virtual coherency conditions............................................. */
{      0.00,      0.00,      1.00}, /*  15 Graduated instructions................................................... */
{      1.00,      1.00,      1.00}, /*  16 Cycles................................................................... */
{      0.00,      0.00,      1.00}, /*  17 Graduated instructions................................................... */
{      1.00,      1.00,      1.00}, /*  18 Graduated loads.......................................................... */
{      1.00,      1.00,      1.00}, /*  19 Graduated stores......................................................... */
{      1.00,      1.00,      1.00}, /*  20 Graduated store conditionals............................................. */
{      0.50,      1.00,     51.22}, /*  21 Graduated floating point instructions.................................... */
{      1.81,      3.17,      3.17}, /*  22 Quadwords written back from primary data cache........................... */
{     40.00,     40.00,     40.00}, /*  23 TLB misses............................................................... */
{      3.88,      7.34,      8.42}, /*  24 Mispredicted branches.................................................... */
{      1.76,      9.48,      9.60}, /*  25 Primary data cache misses................................................ */
{   -1126.02,   -1126.02,   -1170.75}, /*  26 Secondary data cache misses.............................................. */
{      0.00,      0.00,      1.00}, /*  27 Data misprediction from scache way prediction table...................... */
{      0.00,      0.00,      0.00}, /*  28 External intervention hits in scache..................................... */
{      0.00,      0.00,      0.00}, /*  29 External invalidation hits in scache..................................... */
{      1.00,      1.00,      1.00}, /*  30 Store/prefetch exclusive to clean block in scache........................ */
{      1.00,      1.00,      1.00}, /*  31 Store/prefetch exclusive to shared block in scache....................... */
};

static  double  DefaultCounterCosts32_ASIC[NCOUNTERS][NCOSTS] = {
{      1.00,      1.00,      1.00}, /*  0 Cycles................................................................... */
{      0.00,      0.00,      1.00}, /*  1 Issued instructions...................................................... */
{      1.00,      1.00,      1.00}, /*  2 Issued loads............................................................. */
{      1.00,      1.00,      1.00}, /*  3 Issued stores............................................................ */
{      1.00,      1.00,      1.00}, /*  4 Issued store conditionals................................................ */
{      1.00,      1.00,      1.00}, /*  5 Failed store conditionals................................................ */
{      1.00,      1.00,      1.00}, /*  6 Decoded branches......................................................... */
{    -12.27,    -38.04,    -38.04}, /*  7 Quadwords written back from scache....................................... */
{      0.00,      0.00,      1.00}, /*  8 Correctable scache data array ECC errors................................. */
{      4.68,     19.49,     21.54}, /*  9 Primary instruction cache misses......................................... */
{   -734.72,   -745.54,   -775.28}, /*  10 Secondary instruction cache misses....................................... */
{      0.00,      0.00,      1.00}, /*  11 Instruction misprediction from scache way prediction table............... */
{      0.00,      0.00,      0.00}, /*  12 External interventions................................................... */
{      0.00,      0.00,      0.00}, /*  13 External invalidations................................................... */
{      0.00,      0.00,      0.00}, /*  14 Virtual coherency conditions............................................. */
{      0.00,      0.00,      1.00}, /*  15 Graduated instructions................................................... */
{      1.00,      1.00,      1.00}, /*  16 Cycles................................................................... */
{      0.00,      0.00,      1.00}, /*  17 Graduated instructions................................................... */
{      1.00,      1.00,      1.00}, /*  18 Graduated loads.......................................................... */
{      1.00,      1.00,      1.00}, /*  19 Graduated stores......................................................... */
{      1.00,      1.00,      1.00}, /*  20 Graduated store conditionals............................................. */
{      0.50,      1.00,     52.19}, /*  21 Graduated floating point instructions.................................... */
{      1.64,      3.42,      3.42}, /*  22 Quadwords written back from primary data cache........................... */
{     41.95,     41.95,     41.95}, /*  23 TLB misses............................................................... */
{      1.36,      6.38,      9.42}, /*  24 Mispredicted branches.................................................... */
{      2.34,      9.75,     10.77}, /*  25 Primary data cache misses................................................ */
{   -734.72,   -745.54,   -775.28}, /*  26 Secondary data cache misses.............................................. */
{      0.00,      0.00,      1.00}, /*  27 Data misprediction from scache way prediction table...................... */
{      0.00,      0.00,      0.00}, /*  28 External intervention hits in scache..................................... */
{      0.00,      0.00,      0.00}, /*  29 External invalidation hits in scache..................................... */
{      1.00,      1.00,      1.00}, /*  30 Store/prefetch exclusive to clean block in scache........................ */
{      1.00,      1.00,      1.00}, /*  31 Store/prefetch exclusive to shared block in scache....................... */
};

/* R12000 counter costs */
static	double	Default12KCounterCosts27[NCOUNTERS][NCOSTS] = {
{      1.00,      1.00,      1.00}, /*  0 Cycles................................................................... */
{      0.00,      0.00,      1.00}, /*  1 Decoded instructions..................................................... */
{      1.00,      1.00,      1.00}, /*  2 Decoded loads............................................................ */
{      1.00,      1.00,      1.00}, /*  3 Decoded stores........................................................... */
{      1.00,      1.00,      1.00}, /*  4 Miss handling table occupancy............................................ */
{      1.00,      1.00,      1.00}, /*  5 Failed store conditionals................................................ */
{      1.00,      1.00,      1.00}, /*  6 Resolved conditional branches............................................ */
{      5.90,      8.49,      8.77}, /*  7 Quadwords written back from scache....................................... */
{      0.00,      0.00,      1.00}, /*  8 Correctable scache data array ECC errors................................. */
{      4.34,     17.01,     17.01}, /*  9 Primary instruction cache misses......................................... */
{     63.03,     99.89,     99.89}, /*  10 Secondary instruction cache misses....................................... */
{      0.00,      0.00,      1.00}, /*  11 Instruction misprediction from scache way prediction table............... */
{      0.00,      0.00,      0.00}, /*  12 External interventions................................................... */
{      0.00,      0.00,      0.00}, /*  13 External invalidations................................................... */
{      1.00,      1.00,      1.00}, /*  14 ALU/FPU progress cycles.................................................. */
{      0.00,      0.00,      1.00}, /*  15 Graduated instructions................................................... */
{      0.00,      0.00,      0.00}, /*  16 Executed prefetch instructions........................................... */
{      0.00,      0.00,      1.00}, /*  17 Prefetch primary data cache misses....................................... */
{      1.00,      1.00,      1.00}, /*  18 Graduated loads.......................................................... */
{      1.00,      1.00,      1.00}, /*  19 Graduated stores......................................................... */
{      1.00,      1.00,      1.00}, /*  20 Graduated store conditionals............................................. */
{      0.50,      1.00,     52.00}, /*  21 Graduated floating point instructions.................................... */
{      3.14,      3.98,      3.98}, /*  22 Quadwords written back from primary data cache........................... */
{     77.78,     77.78,     77.78}, /*  23 TLB misses............................................................... */
{      6.00,      7.28,      8.81}, /*  24 Mispredicted branches.................................................... */
{      2.17,      8.50,      8.50}, /*  25 Primary data cache misses................................................ */
{     63.03,     99.89,     99.89}, /*  26 Secondary data cache misses.............................................. */
{      0.00,      0.00,      1.00}, /*  27 Data misprediction from scache way prediction table...................... */
{      0.00,      0.00,      0.00}, /*  28 State of intervention hits in scache..................................... */
{      0.00,      0.00,      0.00}, /*  29 State of invalidation hits in scache..................................... */
{      1.00,      1.00,      1.00}, /*  30 Store/prefetch exclusive to clean block in scache........................ */
{      1.00,      1.00,      1.00}, /*  31 Store/prefetch exclusive to shared block in scache....................... */
};

static	double	Default12KCounterCosts30[NCOUNTERS][NCOSTS] = {
{      1.00,      1.00,      1.00}, /*  0 Cycles................................................................... */
{      0.00,      0.00,      1.00}, /*  1 Decoded instructions..................................................... */
{      1.00,      1.00,      1.00}, /*  2 Decoded loads............................................................ */
{      1.00,      1.00,      1.00}, /*  3 Decoded stores........................................................... */
{      1.00,      1.00,      1.00}, /*  4 Miss handling table occupancy............................................ */
{      1.00,      1.00,      1.00}, /*  5 Failed store conditionals................................................ */
{      1.00,      1.00,      1.00}, /*  6 Decoded branches......................................................... */
{      5.87,      6.32,      6.32}, /*  7 Quadwords written back from scache....................................... */
{      0.00,      0.00,      1.00}, /*  8 Correctable scache data array ECC errors................................. */
{      6.05,     23.00,     23.00}, /*  9 Primary instruction cache misses......................................... */
{     48.16,     93.22,     93.22}, /*  10 Secondary instruction cache misses....................................... */
{      0.00,      0.00,      1.00}, /*  11 Instruction misprediction from scache way prediction table............... */
{      0.00,      0.00,      0.00}, /*  12 External interventions................................................... */
{      0.00,      0.00,      0.00}, /*  13 External invalidations................................................... */
{      1.00,      1.00,      1.00}, /*  14 ALU/FPU progress cycles.................................................. */
{      0.00,      0.00,      1.00}, /*  15 Graduated instructions................................................... */
{      0.00,      0.00,      0.00}, /*  16 Executed prefetch instructions........................................... */
{      0.00,      0.00,      1.00}, /*  17 Prefetch primary data cache misses....................................... */
{      1.00,      1.00,      1.00}, /*  18 Graduated loads.......................................................... */
{      1.00,      1.00,      1.00}, /*  19 Graduated stores......................................................... */
{      1.00,      1.00,      1.00}, /*  20 Graduated store conditionals............................................. */
{      0.50,      1.00,     52.00}, /*  21 Graduated floating point instructions.................................... */
{      3.33,      3.65,      3.65}, /*  22 Quadwords written back from primary data cache........................... */
{     77.78,     77.78,     77.78}, /*  23 TLB misses............................................................... */
{      6.00,      7.27,      8.54}, /*  24 Mispredicted branches.................................................... */
{      3.03,     11.50,     11.50}, /*  25 Primary data cache misses................................................ */
{     48.16,     93.22,     93.22}, /*  26 Secondary data cache misses.............................................. */
{      0.00,      0.00,      1.00}, /*  27 Data misprediction from scache way prediction table...................... */
{      0.00,      0.00,      0.00}, /*  28 State of intervention hits in scache..................................... */
{      0.00,      0.00,      0.00}, /*  29 State of invalidation hits in scache..................................... */
{      1.00,      1.00,      1.00}, /*  30 Store/prefetch exclusive to clean block in scache........................ */
{      1.00,      1.00,      1.00}, /*  31 Store/prefetch exclusive to shared block in scache....................... */
};

static  double  Default12KCounterCosts32_ASIC[NCOUNTERS][NCOSTS] = {
{      1.00,      1.00,      1.00}, /*  0 Cycles.....................................................*/
{      0.00,      0.00,      1.00}, /*  1 Decoded instructions.......................................*/
{      1.00,      1.00,      1.00}, /*  2 Decoded loads..............................................*/
{      1.00,      1.00,      1.00}, /*  3 Decoded stores.............................................*/
{      1.00,      1.00,      1.00}, /*  4 Miss handling table occupancy..............................*/
{      1.00,      1.00,      1.00}, /*  5 Failed store conditionals..................................*/
{      1.00,      1.00,      1.00}, /*  6 Resolved conditional branches..............................*/
{     -3.35,    -26.37,    -66.56}, /*  7 Quadwords written back from scache.........................*/
{      0.00,      0.00,      1.00}, /*  8 Correctable scache data array ECC errors...................*/
{      5.19,     20.10,     20.10}, /*  9 Primary instruction cache misses...........................*/
{   -752.21,   -804.84,   -804.84}, /*  10 Secondary instruction cache misses........................ */
{      0.00,      0.00,      1.00}, /*  11 Instruction misprediction from scache way prediction table */
{      0.00,      0.00,      0.00}, /*  12 External interventions.................................... */
{      0.00,      0.00,      0.00}, /*  13 External invalidations.................................... */
{      1.00,      1.00,      1.00}, /*  14 ALU/FPU progress cycles................................... */
{      0.00,      0.00,      1.00}, /*  15 Graduated instructions.................................... */
{      0.00,      0.00,      0.00}, /*  16 Executed prefetch instructions............................ */
{      0.00,      0.00,      1.00}, /*  17 Prefetch primary data cache misses........................ */
{      1.00,      1.00,      1.00}, /*  18 Graduated loads........................................... */
{      1.00,      1.00,      1.00}, /*  19 Graduated stores.......................................... */
{      1.00,      1.00,      1.00}, /*  20 Graduated store conditionals.............................. */
{      0.50,      1.00,     51.71}, /*  21 Graduated floating point instructions..................... */
{      2.72,      2.72,      4.23}, /*  22 Quadwords written back from primary data cache............ */
{     50.03,     50.03,     50.03}, /*  23 TLB misses................................................ */
{      5.16,      6.37,      7.29}, /*  24 Mispredicted branches..................................... */
{      2.59,     10.05,     10.05}, /*  25 Primary data cache misses................................. */
{   -752.21,   -804.84,   -804.84}, /*  26 Secondary data cache misses............................... */
{      0.00,      0.00,      1.00}, /*  27 Data misprediction from scache way prediction table....... */
{      0.00,      0.00,      0.00}, /*  28 State of intervention hits in scache...................... */
{      0.00,      0.00,      0.00}, /*  29 State of invalidation hits in scache...................... */
{      1.00,      1.00,      1.00}, /*  30 Store/prefetch exclusive to clean block in scache......... */
{      1.00,      1.00,      1.00}, /*  31 Store/prefetch exclusive to shared block in scache........ */
};

/* This is the cost table that is used by the print routines.
 * The flag indicates whether the table has ever been initialized.
 * At print time the table will be initialized to the default
 * costs if the flag is FALSE.
 */
static	int	CounterCostsInitialized = FALSE;
static	double	CounterCosts[NCOUNTERS][3];

/* This variable is visible to perfex: it needs to fill it in before
 * calls to the PFX_print* routines.
 */
perfy_option_t	perfy_options;

/*
 ******************************************************************************
 */

/*
 * Internal entry points.
 */

static void
TableInitialize(
	perfy_option_t	*options);

static	void
PresentResults(
	perfy_option_t	*options,
	pid_t		pid,
	Events		*tally,
	int		nevents);

#ifdef TIDY
static	void
TidyCosts(
	perfy_option_t	*options,
	Events		*tally,
	int		nevents);
#endif

static	int
CounterCompare(
	Events		*event1,
	Events		*event2);

static	int
CostCompare(
	Events		*event1,
	Events		*event2);

static	int
PerfyLoadTable(
	double	*def,
	double	*working,
	char	*CostFileName,
	int	PrintErrors);

extern	int
InsertCosts(
	double	CostTable[NCOUNTERS][NCOSTS],
	char	*buffer,
	char	*CostFileName,
	int	counter,
	int	line,
	int	PrintErrors);

static	int
GrabCost(
	double	*cost,
	char	*CostFileName,
	int	counter,
	int	line,
	char	*s,
	char	*name,
	int	PrintErrors);

static	int
mhz(void);

static	int
ip(void);

unsigned
cpu_rev_maj(void);

/*
 ******************************************************************************
 *
 * These four entry points are used by perfex to print out results.
 * They are now just jackets around print_table.
 *
 ******************************************************************************
 */

int
PFX_print_counters(

int	event_type0,
evcnt_t	count0,
int	event_type1,
evcnt_t	count1)

{
  int		i;
  counts_t	counts[NCOUNTERS];

  /* this global is filled by the caller.
   * remove any fields from here that are 
   * filled in by the perfex main.
   */
  perfy_options.fpout           = _fpout;
  perfy_options.MultiRunFiles   = _MultiRunFiles;
  perfy_options.MHz             = _MHz;
  perfy_options.cpu_majrev      = _cpurev;
  perfy_options.IP              = _IP;
  perfy_options.range           = _range;
  perfy_options.cpu_species_mix = _cpuspeciesmix;
  
  /* This data structure is required by the print_table interface.
   */
  for (i=0; i<NCOUNTERS; i++) {
    counts[i].active = FALSE;
  }
  counts[event_type0].active = TRUE;
  counts[event_type0].count  = count0;
  counts[event_type1].active = TRUE;
  counts[event_type1].count  = count1;

  /* No process ID in this case.
   */
  print_table(&perfy_options, counts, (pid_t) -1);

  return(0);
}

/*
 ------------------------------------------------------------------------------
 */

#ifndef FILTER

int
PFX_print_counters_all(
hwperf_cntr_t	*cnts)

{
  int		i;
  counts_t	counts[NCOUNTERS];

  /* this global is filled by the caller.
   * remove any fields from here that are 
   * filled in by the perfex main.
   */
  perfy_options.fpout           = _fpout;
  perfy_options.MultiRunFiles   = _MultiRunFiles;
  perfy_options.MHz             = _MHz;
  perfy_options.cpu_majrev      = _cpurev;
  perfy_options.IP              = _IP;
  perfy_options.range           = _range;
  perfy_options.cpu_species_mix = _cpuspeciesmix;

  /* This data structure is required by the print_table interface.
   */
  for (i=0; i<NCOUNTERS; i++) {
    counts[i].active = TRUE;
    counts[i].count  = 16*(cnts->hwp_evctr[i]);
  }

  /* No process ID in this case.
   */
  print_table(&perfy_options, counts, (pid_t) -1);

  return(0);
}

/*
 ------------------------------------------------------------------------------
 */

int
PFX_print_counters_thread(

int	pid,
int	event_type0,
evcnt_t	count0,
int	event_type1,
evcnt_t	count1)

{
  int		i;
  counts_t	counts[NCOUNTERS];

  /* this global is filled by the caller.
   * remove any fields from here that are 
   * filled in by the perfex main.
   */
  perfy_options.fpout           = _fpout;
  perfy_options.MultiRunFiles   = _MultiRunFiles;
  perfy_options.MHz             = _MHz;
  perfy_options.cpu_majrev      = _cpurev;
  perfy_options.IP              = _IP;
  perfy_options.range           = _range;
  perfy_options.cpu_species_mix = _cpuspeciesmix;

  /* This data structure is required by the print_table interface.
   */
  for (i=0; i<NCOUNTERS; i++) {
    counts[i].active = FALSE;
  }
  counts[event_type0].active = TRUE;
  counts[event_type0].count  = count0;
  counts[event_type1].active = TRUE;
  counts[event_type1].count  = count1;

  print_table(&perfy_options, counts, (pid_t) pid);

  return(0);
}

/*
 ------------------------------------------------------------------------------
 */

int
PFX_print_counters_thread_all(

int		pid,
hwperf_cntr_t	*cnts)

{
  int		i;
  counts_t	counts[NCOUNTERS];

  /* this global is filled by the caller.
   * remove any fields from here that are 
   * filled in by the perfex main.
   */
  perfy_options.fpout           = _fpout;
  perfy_options.MultiRunFiles   = _MultiRunFiles;
  perfy_options.MHz             = _MHz;
  perfy_options.cpu_majrev      = _cpurev;
  perfy_options.IP              = _IP;
  perfy_options.range           = _range;
  perfy_options.cpu_species_mix = _cpuspeciesmix;

  /* This data structure is required by the print_table interface.
   */
  for (i=0; i<NCOUNTERS; i++) {
    counts[i].active = TRUE;
    counts[i].count  = 16*(cnts->hwp_evctr[i]);
  }

  print_table(&perfy_options, counts, (pid_t) pid);

  return(0);
}

#endif

/*
 ******************************************************************************
 */

static void
TableInitialize(

perfy_option_t	*options)

{
  static int	last_IP = _IP;

  if (options == NULL) {
    perfy_options.fpout           = _fpout;
    perfy_options.MultiRunFiles   = _MultiRunFiles;
    perfy_options.MHz             = _MHz;
    perfy_options.cpu_majrev      = _cpurev;
    perfy_options.IP              = _IP;
    perfy_options.range           = _range;
    perfy_options.cpu_species_mix = _cpuspeciesmix;
    options                     = &perfy_options;
  }
    
  /* This routine is called by all functions which must have access to
   * the cost table. It can be called before the IP number is set, so
   * we must attempt to determine the IP number in order to initialize
   * the default cost table. However, if used by the filter, one could
   * reset the IP number ofter the first call to this routine (e.g., if
   * one first loads a cost table, then sets the IP number), so we need
   * to check if the IP number has changed. If so, the default cost table
   * needs to be re-initialized. The working table won't be changed,
   * though, unless another cost table is loaded.
   */
  if ((options->IP != last_IP) || (options->IP < 0)) {
    if (options->IP < 0) {
      options->IP = ip();
      options->cpu_majrev = cpu_rev_maj();
      options->cpu_species_mix = system_cpu_mix();
      if (options->IP < 0) options->IP = 0;
    }
    last_IP = options->IP;
    DefaultCounterCostsInitialized = FALSE;
  }

  /* bind correct event definitions */
  if( ! EventDesc ) {
    switch (options->cpu_species_mix) {
    case CPUSPECIES_PURE_R10000:
      EventDesc = &R10000EventDesc[0];
      if(options->cpu_majrev >= 3) {
	EventDesc[14] = "ALU/FPU forward progress cycles";
      }
      break;
    case CPUSPECIES_PURE_R12000:
      EventDesc = &R12000EventDesc[0];
      break;
    case CPUSPECIES_MIXED_R10000_R12000:
      EventDesc = &CommonEventDesc[0];
      break;
    default:
      EventDesc = &R10000EventDesc[0];
      break;
    }
  }
  
  /* bind the correct table to DefaultCounterCosts */
  if (!DefaultCounterCostsInitialized) {
    /* it always makes sense to initialize with 0 costs 
     * in case a path through the switchyard below doesn't 
     * hit the initialization
     */
    DefaultCounterCosts = &DefaultCounterCosts0[0][0];

    switch(options->cpu_species_mix) {
    case CPUSPECIES_PURE_R10000:
      {
	switch (options->IP) {
	case 0:
	  DefaultCounterCosts = &DefaultCounterCosts0[0][0];
	  break;
	case 25:
	  DefaultCounterCosts = &DefaultCounterCosts25[0][0];
	  break;
	case 27:
	  DefaultCounterCosts = &DefaultCounterCosts27[0][0];
	  break;
	case 28:
	  DefaultCounterCosts = &DefaultCounterCosts28[0][0];
	  break;
	case 30:
	  DefaultCounterCosts = &DefaultCounterCosts30[0][0];
	  break;
	case 32:
	  if (mhz() <= 180) {
	    /* Juice FPGA */
	    DefaultCounterCosts = &DefaultCounterCosts32_FPGA[0][0];
	  }
	  else {
	    /* Juice ASIC */
	    DefaultCounterCosts = &DefaultCounterCosts32_ASIC[0][0];
	  }
	  break;
	default:
#ifdef FILTER
	  fprintf(output_stream,
		  "No default cost table for IP %d\n"
		  "Rerun using the -ip <IP> flag to set the IP number manually.\n",
		  options->IP);
	  exit(1);
#else
	  DefaultCounterCosts = &DefaultCounterCosts0[0][0];
	  fprintf(output_stream,
		  "Warning: No default cost table for IP %d, using default costs of zero.\n",
		options->IP);
	  options->IP = 0;
#endif
	  break;


	}

	/* may need to apply a patch to the DefaultCounterCosts table 
	 * due to cpu revisions
	 */

	switch(options->cpu_majrev) {
	case 3:
	  {
	    double *overlay = DefaultCounterCosts;
	    overlay += 14*3;   /* rewrite the 14th row */
	    *overlay++ = 1.0;
	    *overlay++ = 1.0;
	    *overlay   = 1.0;
	  }
	  break;
	default:
	  break;
	}
      }
    break;   /* cpu_species_mix */
    case CPUSPECIES_PURE_R12000:
      {
	switch (options->IP) {
	case 0:
	  DefaultCounterCosts = &DefaultCounterCosts0[0][0];
	  break;
	case 27:
	  DefaultCounterCosts = &Default12KCounterCosts27[0][0];
	  break;

	case 30:
	  DefaultCounterCosts = &Default12KCounterCosts30[0][0];
	  break;

	case 32:
	  DefaultCounterCosts = &Default12KCounterCosts32_ASIC[0][0];
	  break;
	default:
#ifdef FILTER
	  fprintf(output_stream,
		  "No default cost table for IP %d\n"
		  "Rerun using the -ip <IP> flag to set the IP number manually.\n",
		  options->IP);
	  exit(1);
#else
	  DefaultCounterCosts = &DefaultCounterCosts0[0][0];
	  fprintf(output_stream,
		  "Warning: No default cost table for IP %d, using default costs of zero.\n",
		  options->IP);
	  options->IP = 0;
#endif
	  break;
	}
      } /* cpu_species_mix */
    break;
    case CPUSPECIES_MIXED_R10000_R12000:
      {
	/* not supported except with user-loaded cost table */
	DefaultCounterCosts = &DefaultCounterCosts0[0][0];
      }
    break; /* cpu_species_mix */
    default:
      {
	/* unrecognized cpu species */
	DefaultCounterCosts = &DefaultCounterCosts0[0][0];
      } 
    break;
    } /* end cpu_species_mix switch */
    
    /* No printing of errors is used when initializing the default
     * cost table to the system-wide values since, if an error occurs,
     * the hard-coded values in this file will be used.
     */
    (void) PerfyLoadTable((double *) NULL, DefaultCounterCosts,
			  SYSTEM_TABLE,FALSE);
    DefaultCounterCostsInitialized = TRUE;
  }

  if (!CounterCostsInitialized) {
    bcopy(DefaultCounterCosts,CounterCosts,
	  sizeof(CounterCosts));
    CounterCostsInitialized = TRUE;
  }
}

/*
 ******************************************************************************
 */

#define	PID_LEN			(10)
#define	PID_FORMAT		"pid %*d: "
#define	ID_LEN			(10 + 19 + PID_LEN)
#define	ID_FORMAT		"Costs for pid %*d     "
#define	EVENT_FORMAT		"%*d"
#define EVENT_HEADING		"Event Counter Name"
#define	COUNT_LEN		(12)
#define	COUNT_FORMAT		"%*lld"
#define COUNT_HEADING		"Counter Value"
#if (1)
#define	TIME_LEN		(12)
#define	TIME_FORMAT		"%*.6f"
#define	TIME_HEADING		"Time (sec)"
#else
#define	TIME_LEN		(8)
#define	TIME_FORMAT		"%*.3f"
#define	TIME_HEADING		"Time (s)"
#endif
#define	COST_HEADING		"Cost"

/* Variables used for the printouts.
 */
#define	PRINT_DEFS(PERFY,RANGE)						\
									\
  int	j;								\
  int	ntimes = (PERFY) ? ((RANGE) ? NCOSTS : 1) : 0;			\
  char	PrintBuffer[PID_LEN+6 + EVENT_LEN+1 + MAX_EVENT_DESC_LEN +	\
		    COUNT_LEN+1 + 3*(TIME_LEN+1) + 1];			\
  int	pid_len, event_len, event_desc_len, count_len, time_len;	\
  int	line_len;							\
  char	thread[PID_LEN+6 + 1], id[ID_LEN], *str;			\
  char	pad[EVENT_LEN+1 + MAX_EVENT_DESC_LEN + COUNT_LEN+1 + 1];	\
  int	order[NCOSTS] = {TYPICAL, MINIMUM, MAXIMUM};


/* The standard field lengths for each line of a printout:
 *
 *	                 Event Counter Name  Cou...  Tim... Tim...  Tim...
 *	=================================================================
 *	pid <###>: <###> <--name--><--...--> <count> <time> <time> <time>
 *	    \-v-/  \-v-/ \--------v--------/ \--v--/ \--v-/
 *	      |      |            |             |       |
 *	   PID_LEN   |   MAX_EVENT_DESC_LEN     |   TIME_LEN
 *	         EVENT_LEN                  COUNT_LEN
 */
#define	SET_FIELD_LENGTHS(OPTIONS,COST)					\
  									\
  if ((OPTIONS == NULL) || ((OPTIONS != NULL) && OPTIONS->perfy) || (pid <= 0)) {	\
    thread[0]    = '\0';						\
    pid_len      = 0;							\
  } else {								\
    sprintf(thread, PID_FORMAT, PID_LEN, pid);				\
    pid_len      = strlen(thread);					\
  }									\
  event_len      = EVENT_LEN+1;						\
  event_desc_len = MAX_EVENT_DESC_LEN + ((COST) ? COUNT_LEN+1 : 0);	\
  count_len      = ((COST) ? 0: COUNT_LEN+1);				\
  time_len       = ntimes*(TIME_LEN+1);					\
  line_len       = pid_len + event_len + event_desc_len +		\
                   count_len + time_len;


#define	CLOCK_LINE							\
									\
  {									\
    char	*clock = "Based on XXXXXXX MHz IPXXXXXXXXXX";		\
    int		length = line_len - (ntimes-1)*(TIME_LEN+1);		\
									\
    if (!UserDefinedTable) {						\
      sprintf(clock, "Based on %d MHz IP%d", options->MHz, options->IP);\
    } else {								\
      sprintf(clock, "Based on %d MHz Clock Rate", options->MHz);	\
    }									\
    str = (length >= strlen(clock)) ? clock : "";			\
    fprintf(fpout, "\n%*s", length, str);				\
  }

#define	IP_LINE								\
									\
  {									\
    char	*ipline = "Costs for IPXXXXXXXXXX processor";		\
    int		length  = line_len - (ntimes-1)*(TIME_LEN+1);		\
									\
    if (options->IP >= 0) {						\
      sprintf(ipline, "Costs for IP%d processor", options->IP);		\
      str = (length >= strlen(ipline)) ? ipline : "";			\
      fprintf(fpout, "\n%*s", length, str);				\
    }									\
  }

#define CPUTYPE_LINE                                                    \
  {                                                                     \
    char        *typeline = "MIPS XXXXXXXXXXXXXX CPU";                  \
    int         length = line_len - (ntimes-1)*(TIME_LEN+1);            \
                                                                        \
    switch(options->cpu_species_mix) {                                  \
       case CPUSPECIES_PURE_R10000:                                     \
	 sprintf(typeline,"MIPS R10000 CPU");                           \
         break;                                                         \
       case CPUSPECIES_PURE_R12000:                                     \
	 sprintf(typeline,"MIPS R12000 CPU");                           \
         break;                                                         \
       case CPUSPECIES_MIXED_R10000_R12000:                             \
	 sprintf(typeline,"MIPS R10000/R12000 CPUS");                   \
         break;                                                         \
       default:                                                         \
	 sprintf(typeline,"Unknown CPU");                               \
	 break;                                                         \
     }                                                                  \
     str = (length >= strlen(typeline)) ? typeline : "";                \
     fprintf(fpout, "\n%*s", length, str);                              \
  }                                                                     \

     
#define	CPUREV_LINE							\
									\
  {									\
    char	*revline = "CPU revision X.x";	\
    int		length  = line_len - (ntimes-1)*(TIME_LEN+1);		\
									\
    if (options->cpu_majrev > 0) {					\
      sprintf(revline, "CPU revision %u.x ", options->cpu_majrev); \
      str = (length >= strlen(revline)) ? revline : "";			\
      fprintf(fpout, "\n%*s", length, str);				\
    }									\
  }


#define	HORIZONTAL_RULE							\
									\
  for (j=0; j<line_len; j++) {						\
    PrintBuffer[j] = '=';						\
  }									\
  PrintBuffer[j] = '\0';						\
  fprintf(fpout, "%s\n", PrintBuffer);					\
  fflush(fpout);

#define	PRINT_HEADER(COST)						\
									\
  fprintf(fpout, "\n");							\
  if (pid <= 0) {							\
    id[0]  = '\0';							\
  } else {								\
    sprintf(id, ID_FORMAT, PID_LEN, pid);				\
  }									\
  sprintf(PrintBuffer, "%*s%*s%*s%*s", pid_len, "", event_len, "",	\
	  event_desc_len, id, count_len, "");				\
  for (j=0; j<ntimes; j++) {						\
    sprintf(PrintBuffer + strlen(PrintBuffer), " %*s",			\
	    TIME_LEN, CostNames[order[j]]);				\
  }									\
  fprintf(fpout, "%s\n", PrintBuffer);					\
  fflush(fpout);							\
									\
  sprintf(PrintBuffer, "%*s%*s", pid_len, "", event_len, "");		\
  str = (event_desc_len >= strlen(EVENT_HEADING)) ? EVENT_HEADING : "";	\
  sprintf(PrintBuffer + strlen(PrintBuffer), "%*s", -event_desc_len, str); \
  str = (count_len >= strlen(COUNT_HEADING)) ? COUNT_HEADING : "";	\
  sprintf(PrintBuffer + strlen(PrintBuffer), "%*s", count_len, str);	\
  str = (COST) ? COST_HEADING : TIME_HEADING;				\
  str = (TIME_LEN >= strlen(str)) ? str : "";				\
  for (j=0; j<ntimes; j++) {						\
    sprintf(PrintBuffer + strlen(PrintBuffer), " %*s",			\
	    TIME_LEN, str);						\
  }									\
  fprintf(fpout, "%s\n", PrintBuffer);					\
  fflush(fpout);							\
									\
  HORIZONTAL_RULE;


#define	FILL_PAD							\
									\
  for (j=0; j<event_desc_len; j++) {					\
    pad[j] = '.';							\
  }									\
  pad[j] = '\0';
  

/* This is the form that each statistic line takes. Only the name
 * and statistic value change.
 */
#define	STAT_PRINT(NAME,STAT)						\
									\
      PrintBuffer[0] = '\0';						\
      strcat(PrintBuffer, thread);					\
      strcat(PrintBuffer, NAME);					\
      strcat(PrintBuffer, &pad[strlen(NAME)]);				\
      sprintf(PrintBuffer + strlen(PrintBuffer), " " TIME_FORMAT,	\
	      TIME_LEN,	STAT);						\
      fprintf(fpout, "%s\n", PrintBuffer);				\
      fflush(fpout);

/*
 ******************************************************************************
 */

void
dump_table(

perfy_option_t	*options,
double		*pTable,
FILE		*fpout)

{
  int	i;
  char	*name;
  double *Table, t;
  pid_t	pid    = -1;
  PRINT_DEFS(TRUE,TRUE);

  if (options == NULL) {
    perfy_options.fpout           = _fpout;
    perfy_options.MultiRunFiles   = _MultiRunFiles;
    perfy_options.MHz             = _MHz;
    perfy_options.cpu_majrev      = _cpurev;
    perfy_options.IP              = _IP;
    perfy_options.range           = _range;
    perfy_options.cpu_species_mix = _cpuspeciesmix;
    options                     = &perfy_options;
  }
    
  if (pTable == NULL) {

    /* Make sure that the default and working cost tables have
     * been initialized. The default cost table is intialized
     * from the system-wide cost table file if it exists; otherwise
     * values hard coded into this file are the defaults. Then, the
     * working cost table is intialized from the default table. Two
     * tables are maintained so a user can always revert to the
     * default by supplying an empty cost table file as input to
     * load_table().
     */
    TableInitialize(options);

    Table = &CounterCosts[0][0];

  } else {

    Table = pTable;

  }

  SET_FIELD_LENGTHS(options,TRUE);
  IP_LINE;
  CPUTYPE_LINE;
  if(options->cpu_species_mix == CPUSPECIES_PURE_R10000) {
    CPUREV_LINE;
  }
  PRINT_HEADER(TRUE);
  FILL_PAD;
 
  for (i=0; i<NCOUNTERS; i++) {
    PrintBuffer[0] = '\0';
    name = EventDesc[i];
    strcat(PrintBuffer, thread);
    sprintf(PrintBuffer + strlen(PrintBuffer), "%*d ", EVENT_LEN, i);
    strcat(PrintBuffer, name);
    strcat(PrintBuffer, &pad[strlen(name)]);
    for (j=0; j<ntimes; j++) {
      t = Table[i*NCOSTS + order[j]];
      if (t < 0.0) {
	sprintf(PrintBuffer + strlen(PrintBuffer), " %*.2f %-4s",
		TIME_LEN-1-4, -t, "nsec");
      } else {
	sprintf(PrintBuffer + strlen(PrintBuffer), " %*.2f %-4s",
		TIME_LEN-1-4,  t, "clks");
      }
    }
    fprintf(fpout, "%s\n", PrintBuffer);
    fflush(fpout);
  }
}

/*
 ******************************************************************************
 */

int
load_table(

perfy_option_t	*options,
char		*CostFileName)

{
  int	retval;

  if (options == NULL) {
    perfy_options.fpout           = _fpout;
    perfy_options.MultiRunFiles   = _MultiRunFiles;
    perfy_options.MHz             = _MHz;
    perfy_options.cpu_majrev      = _cpurev;
    perfy_options.IP              = _IP;
    perfy_options.range           = _range;
    perfy_options.cpu_species_mix = _cpuspeciesmix;
    options                     = &perfy_options;
  }
    
  /* Make sure that the default and working cost tables have
   * been initialized. The default cost table is intialized
   * from the system-wide cost table file if it exists; otherwise
   * values hard coded into this file are the defaults. Then, the
   * working cost table is intialized from the default table. Two
   * tables are maintained so a user can always revert to the
   * default by supplying an empty cost table file as input to
   * load_table().
   */
  TableInitialize(options);

  /* This call to PerfyLoadTable will print error messages. In addition,
   * it will fill in the working cost table (CounterCosts) from the
   * specified file (CostFileName), but if an error occurs, values from
   * the default cost table (DefaultCounterCosts) will be used. (Also,
   * any values not specified in the file will be drawn from the default
   * table).
   */
  retval = PerfyLoadTable((double *) DefaultCounterCosts,
			  (double *) CounterCosts, CostFileName, TRUE);

  if (retval == 0) {
    UserDefinedTable = TRUE;
  }

  return (retval);
}

/*
 ******************************************************************************
 */

extern void
print_table(

perfy_option_t	*options,
counts_t	*counts,
pid_t		pid)

{
  int		i, counter, nevents;
  int		CounterIndex[NCOUNTERS];
  Events	tally[NCOUNTERS];
  FILE		*fpout = options->fpout;

#ifdef _BDB
  printf("\n");
  printf("print_table: pid = %d\n", pid);
  for (i=0; i<NCOUNTERS; i++) {
    printf("print_table: counts[%2d] = (%5s,%21lld)\n",
	   i, (counts[i].active) ? "TRUE" : "FALSE", counts[i].count);
  }
#endif

  if (options == NULL) {
    perfy_options.fpout           = _fpout;
    perfy_options.MultiRunFiles   = _MultiRunFiles;
    perfy_options.MHz             = _MHz;
    perfy_options.cpu_majrev      = _cpurev;
    perfy_options.IP              = _IP;
    perfy_options.range           = _range;
    perfy_options.cpu_species_mix = _cpuspeciesmix;
    options                     = &perfy_options;
  }
    
  /* Make sure that the default and working cost tables have
   * been initialized. The default cost table is intialized
   * from the system-wide cost table file if it exists; otherwise
   * values hard coded into this file are the defaults. Then, the
   * working cost table is intialized from the default table. Two
   * tables are maintained so a user can always revert to the
   * default by supplying and empty cost table file as input to
   * load_table().
   */
  TableInitialize(options);

  /* The clock speed of the machine perfy is running on is used
   * if MHz <= 0. Otherwise, the supplied value is taken as the
   * speed.
   */
  if (options->MHz <= 0) options->MHz = mhz();
  if (options->MHz <  0) {
    fprintf(output_stream,"Unable to determine clock rate.\n"
#ifdef FILTER
                   "Rerun using the -mhz <MHz> flag to set this manually.\n"
#endif
	    );
    exit(1);
  }

  /* The IP # of the machine perfy is running on is used if IP < 0.
   * Otherwise, the supplied value is taken as the IP #.
   */
  if (options->IP < 0) options->IP = ip();
  if (options->IP < 0) {
    fprintf(output_stream,"Unable to determine IP #.\n"
#ifdef FILTER
                   "Rerun using the -ip <IP#> flag to set this manually.\n"
#endif
	    );
    exit(1);
  }

  /* The cpu major revision number of the machine perfy is running on 
   * is used if cpu_majrev = 0, otherwise the supplied value is 
   * taken as the rev #.
   */
  if (options->cpu_majrev <= 0) options->cpu_majrev = cpu_rev_maj();
  if (options->cpu_majrev <= 0) {
    fprintf(output_stream,"Unable to determine cpu version #.\n"
#ifdef FILTER
                   "Rerun using the -cpurev <revision#> flag to set this manually.\n"
#endif
	    );
    exit(1);
  }
  
  /* Initialize the index array to a value indicating that there is no
   * information yet for each counter. As we read a counter's value, its
   * slot in the table is maked with it's position in the tally array.
   */
  for (i=0; i<NCOUNTERS; i++) {
    CounterIndex[i] = -1;
  }

  for (nevents=0, counter=0; counter<NCOUNTERS; counter++) {

    /* If there is no count for this counter, go on to check the next one.
     */
    if (!counts[counter].active) continue;

    /* This means that we already have a value for the counter, so we
     * skip this line of input.
     */
    if (CounterIndex[counter] >= 0) continue;

#ifdef _BDB
    printf("print_table: filling in tally[%2d] with counter %2d\n",
	   nevents, counter);
#endif

    /* Fill in the counter index so that we know this counter's value
     * has been seen, and then tally up the costs for the counter.
     */
    CounterIndex[counter]        = nevents;
    tally[nevents].counter       = counter;
    tally[nevents].count         = counts[counter].count;
    tally[nevents].cost[MINIMUM] = tally[nevents].count *
                                   COST(CounterCosts[counter][MINIMUM]);
    tally[nevents].cost[TYPICAL] = tally[nevents].count *
                                   COST(CounterCosts[counter][TYPICAL]);
    tally[nevents].cost[MAXIMUM] = tally[nevents].count *
                                   COST(CounterCosts[counter][MAXIMUM]);

    nevents++;

  }

  if (nevents > 0) {
    PresentResults(options,pid,tally,nevents);
  }

  return;
}

/*
 ******************************************************************************
 */

static void
PresentResults(

perfy_option_t	*options,
pid_t		pid,
Events		*tally,
int		nevents)

{
  int		i,r10k,r12k,mixed;
  char		*name;
  int		CounterIndex[NCOUNTERS], print, nstats;
  Statistics	stats[MAX_STATS];
  double	L1Load, L1Block, L2Block;
  FILE		*fpout = options->fpout;
  double	cycle  = PCYCLE;
  PRINT_DEFS(options->perfy,options->range);

#ifdef ID
  fprintf(fpout,"*** Running /hosts/steinmetz/d0/work/T5/counters/perfex-perfy/Product ***\n");
#endif

#ifdef _BDB
  printf("\n");
  printf("PresentResults: nevents = %d\n", nevents);
  for (i=0; i<nevents; i++) {
    printf("PresentResults: tally[%2d] = (%2d,%21lld,%8.3f,%8.3f,%8.3f)\n",
	   i, tally[i].counter, tally[i].count,
	   tally[i].cost[MINIMUM], tally[i].cost[TYPICAL],
	   tally[i].cost[MAXIMUM]);
  }
#endif

  /* Tidy up the "typical" costs by making sure that if a
   * cycle count has been included, no cost exceeds the
   * cost of the cycles event.
   */
#ifdef TIDY
  if (options->perfy) {
    TidyCosts(options,tally,nevents);
  }
#endif

  SET_FIELD_LENGTHS(options,FALSE);

  if (options->perfy) {

    /* Sort the events by cost, then print them out by name.
     */
    qsort(tally, (size_t) nevents, (size_t) sizeof(Events),
	  (int (*)(const void *, const void*)) CostCompare);

    if(options->cpu_species_mix != CPUSPECIES_MIXED_R10000_R12000) {
      CLOCK_LINE;
    }
    CPUTYPE_LINE;
    if(options->cpu_species_mix == CPUSPECIES_PURE_R10000) {
      CPUREV_LINE;
    }
    PRINT_HEADER(FALSE);
  }

  FILL_PAD;

  for (i=0; i<nevents; i++) {
    name = EventDesc[tally[i].counter];
    PrintBuffer[0] = '\0';
    strcat(PrintBuffer, thread);
    sprintf(PrintBuffer + strlen(PrintBuffer), EVENT_FORMAT " ",
	    EVENT_LEN, tally[i].counter);
    strcat(PrintBuffer, name);
    strcat(PrintBuffer, &pad[strlen(name)]);
    sprintf(PrintBuffer + strlen(PrintBuffer), " " COUNT_FORMAT, 
	    COUNT_LEN, tally[i].count);
    if (options->perfy) {
      for (j=0; j<ntimes; j++) {
	sprintf(PrintBuffer + strlen(PrintBuffer), " " TIME_FORMAT, 
		TIME_LEN,  tally[i].cost[order[j]]*cycle);
      }
    }
    fprintf(fpout, "%s\n", PrintBuffer);
    fflush(fpout);
  }

  /*
   * Print out some useful statistics.
   */

  if (options->perfy) {

    /* Mark all the counters as empty, then update those that
     * actually have values. Thus, we can use CounterIndex to
     * check if a valid counter value does not exist, and if
     * so, we skip any statistics depending on it.
     */
    for (i=0; i<NCOUNTERS; i++) {
      CounterIndex[i] = -1;
    }
    for (i=0; i<nevents; i++) {
      CounterIndex[tally[i].counter] = i;
    }

    event_desc_len = event_len + event_desc_len + count_len;
    time_len       = TIME_LEN+1;
    line_len       = pid_len + event_desc_len + time_len;

    FILL_PAD;

    /*
     * All the statistics, one after the other. The general form is:
     *
     *	1) Do some tests to make sure the counts required to calculate
     *     the statistic are available;
     *	2) Set name to the name of the statistic;
     *	3) Set stat to the value of the statistic;
     *	4) Print out the statistic line.
     *
     * For the cache reuse statistics, we also need to determine whether
     * cache line or data element reuse has been requested. If it is the
     * latter, the size in bytes of the data transfers between different
     * levels of the memory hierarchy must be used.
     */
    
    nstats = 0;

    /* event 16 is cycles only on r10k so we build a logical to use 
     * in the test clauses.  The statistics calculations themselves 
     * will be safe as long as the count of event 16 on r12k 
     * (executed prefetches) does not exceed the cycle count. This is 
     * admittedly complex, but the statistics must depend on the details 
     * of what event is what, and which are available.  We attempt to 
     * make the testing phase as transparent as possible.  
     */
    r10k = ((options->cpu_species_mix) == CPUSPECIES_PURE_R10000);
    r12k = ((options->cpu_species_mix) == CPUSPECIES_PURE_R12000);
    mixed = ((options->cpu_species_mix) == CPUSPECIES_MIXED_R10000_R12000);

    /* Graduated instructions/cycle
     */
    print = ((CounterIndex[GRADUATEDINSTS] >= 0) &&
	     ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) && 
	     (MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Graduated instructions/cycle";
      stats[nstats].value = (double) tally[CounterIndex[GRADUATEDINSTS]].count /
                            (double) MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count);
      nstats++;
    }

    /* Graduated floating point instructions/cycle
     */
    print = ((CounterIndex[FLOPS] >= 0) &&
	     ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) && 
	     (MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Graduated floating point instructions/cycle";
      stats[nstats].value = (double) tally[CounterIndex[FLOPS]].count /
	                    (double) MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count);
      nstats++;
    }

    /* Graduated loads & stores/cycle
     */
    print = ((CounterIndex[GRADUATEDLOADS] >= 0) &&
	     (CounterIndex[GRADUATEDSTORES] >= 0) &&
	     ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) && 
	     (MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Graduated loads & stores/cycle";
      stats[nstats].value = (double) (tally[CounterIndex[GRADUATEDLOADS]].count +
				      tally[CounterIndex[GRADUATEDSTORES]].count) /
			    (double) MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count);
      nstats++;
    }

    /* Graduated loads & stores/floating point instruction
     */
    print = ((CounterIndex[GRADUATEDLOADS] >= 0) &&
	     (CounterIndex[GRADUATEDSTORES] >= 0) &&
	     (CounterIndex[FLOPS] >= 0) && 
	     (tally[CounterIndex[FLOPS]].count > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Graduated loads & stores/floating point instruction";
      stats[nstats].value = (double) (tally[CounterIndex[GRADUATEDLOADS]].count +
				      tally[CounterIndex[GRADUATEDSTORES]].count) /
			    (double) tally[CounterIndex[FLOPS]].count;
      nstats++;
    }

    /* Mispredicted branches/Decoded branches
     */
    print = ((CounterIndex[MISSEDBRANCHES] >= 0) &&
	     (CounterIndex[DECODEDBRANCHES] >= 0) && 
	     (tally[CounterIndex[DECODEDBRANCHES]].count > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      switch( options->cpu_species_mix ) {
      case CPUSPECIES_PURE_R10000:
	stats[nstats].name  = "Mispredicted branches/Decoded branches";
	break;
      case CPUSPECIES_PURE_R12000:
	stats[nstats].name  = "Mispredicted branches/Resolved conditional branches";
	break;
      default:
	stats[nstats].name  = "Mispredicted branches/Decoded branches";
	break;
      }
      stats[nstats].value = (double) tally[CounterIndex[MISSEDBRANCHES]].count /
	                    (double) tally[CounterIndex[DECODEDBRANCHES]].count;
      nstats++;
    }

    /* Graduated loads/Issued loads
     */
    print = (!mixed &&
	     (CounterIndex[ISSUEDLOADS] >= 0) &&
	     (CounterIndex[GRADUATEDLOADS] >= 0) && 
	     (tally[CounterIndex[GRADUATEDLOADS]].count > 0));
    if (print) {
      evcnt_t prefetches = 0;
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      switch( options->cpu_species_mix ) {
      case CPUSPECIES_PURE_R10000:
	stats[nstats].name  = "Graduated loads/Issued loads";
	break;
      case CPUSPECIES_PURE_R12000:
	if ( CounterIndex[EXPREFETCHES] >= 0 ) {
	  prefetches = tally[CounterIndex[EXPREFETCHES]].count;
	  stats[nstats].name  = "Graduated loads /Decoded loads ( and prefetches )";
	} else {
	  stats[nstats].name  = "Graduated loads/Decoded loads";
	}
	break;
      default:
	stats[nstats].name  = "Graduated loads/Issued loads";
	break;
      }
      stats[nstats].value = (double) tally[CounterIndex[GRADUATEDLOADS]].count /
	                    ((double) prefetches + 
                            (double) tally[CounterIndex[ISSUEDLOADS]].count);
      nstats++;
    }

    /* Graduated stores/Issued stores
     */

    print = (!mixed &&
	     (CounterIndex[ISSUEDSTORES] >= 0) &&
	     (CounterIndex[GRADUATEDSTORES] >= 0) && 
	     (tally[CounterIndex[GRADUATEDSTORES]].count > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      switch( options->cpu_species_mix ) {
      case CPUSPECIES_PURE_R10000:
	stats[nstats].name  = "Graduated stores/Issued stores";
	break;
      case CPUSPECIES_PURE_R12000:
	stats[nstats].name  = "Graduated stores/Decoded stores";
	break;
      default:
	stats[nstats].name  = "Graduated stores/Decoded stores";
	break;
      }

      stats[nstats].value = (double) tally[CounterIndex[GRADUATEDSTORES]].count /
	                    (double) tally[CounterIndex[ISSUEDSTORES]].count;
      nstats++;
    }

    /* Data mispredict/Data scache hits
     */
    print = ((CounterIndex[L2DMISPREDICT] >= 0) &&
	     (CounterIndex[L1DCACHEMISSES] >= 0) &&
	     (CounterIndex[L2DCACHEMISSES] >= 0) && 
	     ((tally[CounterIndex[L1DCACHEMISSES]].count -
	       tally[CounterIndex[L2DCACHEMISSES]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Data mispredict/Data scache hits";
      stats[nstats].value = (double) tally[CounterIndex[L2DMISPREDICT]].count /
	                    (double) (tally[CounterIndex[L1DCACHEMISSES]].count -
				      tally[CounterIndex[L2DCACHEMISSES]].count);
      nstats++;
    }

    /* Instruction mispredict/Instruction scache hits
     */
    print = ((CounterIndex[L2IMISPREDICT] >= 0) &&
	     (CounterIndex[L1ICACHEMISSES] >= 0) &&
	     (CounterIndex[L2ICACHEMISSES] >= 0) && 
	     ((tally[CounterIndex[L1ICACHEMISSES]].count -
	       tally[CounterIndex[L2ICACHEMISSES]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Instruction mispredict/Instruction scache hits";
      stats[nstats].value = (double) tally[CounterIndex[L2IMISPREDICT]].count /
	                    (double) (tally[CounterIndex[L1ICACHEMISSES]].count -
				      tally[CounterIndex[L2ICACHEMISSES]].count);
      nstats++;
    }

    /* L1 Cache (Line) Reuse
     */
    print = ((CounterIndex[L1DCACHEMISSES] >= 0) &&
	     (CounterIndex[L2DCACHEMISSES] >= 0) &&
	     (CounterIndex[GRADUATEDLOADS] >= 0) &&
	     (CounterIndex[GRADUATEDSTORES] >= 0) &&
	     (tally[CounterIndex[L1DCACHEMISSES]].count > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      if (options->reuse == 0) {
	stats[nstats].name = "L1 Cache Line Reuse";
	L1Load             = 1.0;
	L1Block            = 1.0;
      } else {
	stats[nstats].name = "L1 Data Reuse";
	L1Load             = options->reuse;
	L1Block            = L1BLOCKSIZE;
      }
      stats[nstats].value = ((tally[CounterIndex[GRADUATEDLOADS]].count +
			      tally[CounterIndex[GRADUATEDSTORES]].count) * L1Load) /
			    (tally[CounterIndex[L1DCACHEMISSES]].count * L1Block) - 1.0;
      nstats++;
    }

    /* L2 Cache (Line) Reuse
     */
    print = ((CounterIndex[L1DCACHEMISSES] >= 0) &&
	     (CounterIndex[L2DCACHEMISSES] >= 0) &&
	     (tally[CounterIndex[L2DCACHEMISSES]].count > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      if (options->reuse == 0) {
	stats[nstats].name = "L2 Cache Line Reuse";
	L1Block            = 1.0;
	L2Block            = 1.0;
      } else {
	stats[nstats].name = "L2 Data Reuse";
	L1Block            = L1BLOCKSIZE;
	L2Block            = L2BLOCKSIZE;
      }
      stats[nstats].value = (tally[CounterIndex[L1DCACHEMISSES]].count * L1Block) /
                            (tally[CounterIndex[L2DCACHEMISSES]].count * L2Block) - 1.0;
      nstats++;
    }

    /* L1 Data Cache Hit Rate
     */
    print = ((CounterIndex[L1DCACHEMISSES] >= 0) &&
	     (CounterIndex[GRADUATEDLOADS] >= 0) &&
	     (CounterIndex[GRADUATEDSTORES] >= 0) &&
	     ((tally[CounterIndex[GRADUATEDLOADS]].count +
	       tally[CounterIndex[GRADUATEDSTORES]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "L1 Data Cache Hit Rate";
      stats[nstats].value = 1.0 - ((double) tally[CounterIndex[L1DCACHEMISSES]].count /
				   (double) (tally[CounterIndex[GRADUATEDLOADS]].count +
					     tally[CounterIndex[GRADUATEDSTORES]].count));
      nstats++;
    }

    /* L2 Data Cache Hit Rate
     */
    print = ((CounterIndex[L1DCACHEMISSES] >= 0) &&
	     (CounterIndex[L2DCACHEMISSES] >= 0) &&
	     (tally[CounterIndex[L1DCACHEMISSES]].count > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "L2 Data Cache Hit Rate";
      stats[nstats].value = 1.0 - ((double) tally[CounterIndex[L2DCACHEMISSES]].count /
				   (double) tally[CounterIndex[L1DCACHEMISSES]].count);
      nstats++;
    }

    /* Time accessing memory/Total time
     */
    print = ((CounterIndex[L1DCACHEMISSES] >= 0) &&
	     (CounterIndex[L2DCACHEMISSES] >= 0) &&
	     (CounterIndex[GRADUATEDLOADS] >= 0) &&
	     (CounterIndex[GRADUATEDSTORES] >= 0) &&
	     (CounterIndex[TLBMISSES] >= 0) &&
	     ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) && 
	     (MAX(tally[CounterIndex[CYCLESEVENT]].cost[TYPICAL],tally[CounterIndex[CYCLESEVENT2]].cost[TYPICAL]) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Time accessing memory/Total time";
      stats[nstats].value = (tally[CounterIndex[GRADUATEDLOADS]].count *
			     COST(CounterCosts[GRADUATEDLOADS][TYPICAL]) +
			     tally[CounterIndex[GRADUATEDSTORES]].count *
			     COST(CounterCosts[GRADUATEDSTORES][TYPICAL]) +
			     (tally[CounterIndex[L1DCACHEMISSES]].count) *
			     COST(CounterCosts[L1DCACHEMISSES][TYPICAL]) +
			     tally[CounterIndex[L2DCACHEMISSES]].count *
			     COST(CounterCosts[L2DCACHEMISSES][TYPICAL]) +
			     tally[CounterIndex[TLBMISSES]].count *
			     COST(CounterCosts[TLBMISSES][TYPICAL])) /
			    MAX(tally[CounterIndex[CYCLESEVENT]].cost[TYPICAL],tally[CounterIndex[CYCLESEVENT2]].cost[TYPICAL]);
      nstats++;
    }

    /* Memory wait time is available for 3.x R10k */
    if(options->cpu_majrev >= 3 ) {
      print = ( ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) &&
              (CounterIndex[CYCLESBUSY] >= 0) &&
              (MAX(tally[CounterIndex[CYCLESEVENT]].cost[TYPICAL],tally[CounterIndex[CYCLESEVENT2]].cost[TYPICAL]) > 0) );
      if(print) {
	if (nstats >= MAX_STATS) {
	  fprintf(output_stream,"Overflowed stats[] array\n");
	  exit(1);
	}
	stats[nstats].name  = "Time not making progress (probably waiting on memory) / Total time";      
	stats[nstats].value = 
           ((double) ((MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) ) -  tally[CounterIndex[CYCLESBUSY]].count) ) / 
	  ((double) (MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count)));
	nstats++;
      }
    }

    /* L1--L2 bandwidth used.
     */
    print = (!mixed &&
	     (CounterIndex[L1DCACHEMISSES] >= 0) &&
	     (CounterIndex[L1QWRITEBACKS] >= 0) &&
	     ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) && 
    	     (MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) > 0));;
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "L1--L2 bandwidth used (MB/s, average per process)";
      stats[nstats].value = (((double) tally[CounterIndex[L1DCACHEMISSES]].count * L1BLOCKSIZE) +
			     ((double) tally[CounterIndex[L1QWRITEBACKS]].count * QUADWORD)) /
	                    ((double) MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) /
			     (double) options->MHz);
      nstats++;
    }

    /* Memory bandwidth used.
     */
    print = (!mixed &&
	     (CounterIndex[L2DCACHEMISSES] >= 0) &&
	     (CounterIndex[L2QWRITEBACKS] >= 0) &&
	     ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) &&
    	     (MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Memory bandwidth used (MB/s, average per process)";
      stats[nstats].value = (((double) tally[CounterIndex[L2DCACHEMISSES]].count * L2BLOCKSIZE) +
			     ((double) tally[CounterIndex[L2QWRITEBACKS]].count * QUADWORD)) /
	                    ((double) MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) /
			     (double) options->MHz);
      nstats++;
    }

    /* MFLOP Rate (per process)
     */
    print = (!mixed &&
	     (CounterIndex[FLOPS] >= 0) &&
	     ((CounterIndex[CYCLESEVENT] >= 0) || (r10k && CounterIndex[CYCLESEVENT2] >= 0)) && 
	     (MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) > 0));
    if (print) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "MFLOPS (average per process)";
      stats[nstats].value = (double) tally[CounterIndex[FLOPS]].count /
	                    ((double) MAX(tally[CounterIndex[CYCLESEVENT]].count,tally[CounterIndex[CYCLESEVENT2]].count) /
			     (double) options->MHz);
      nstats++;
    }

    /* Average number of outstanding misses
     */

    print = ( r12k &&
	      (CounterIndex[MHTPOPCYCLES] >= 0) &&
	      (CounterIndex[CYCLESEVENT] >= 0));
    
    if (print ) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Cache misses in flight per cycle (average)";
      stats[nstats].value = (double) tally[CounterIndex[MHTPOPCYCLES]].count /
	((double) tally[CounterIndex[CYCLESEVENT]].count);
      
      nstats++;
    }
	
    /* Prefetch cache miss rate 
     */

    print = ( r12k &&
	      (CounterIndex[EXPREFETCHES] >= 0) &&
	      (CounterIndex[PREFETCHDMISS] >= 0));
    
    if (print ) {
      if (nstats >= MAX_STATS) {
	fprintf(output_stream,"Overflowed stats[] array\n");
	exit(1);
      }
      stats[nstats].name  = "Prefetch cache miss rate";
      stats[nstats].value = (double) tally[CounterIndex[PREFETCHDMISS]].count /
	((double) tally[CounterIndex[EXPREFETCHES]].count);
      
      nstats++;
    }

    if (nstats > 0) {

      /* Print a banner line & horizontal rule start the statistics section.
       */
      fprintf(fpout, "\nStatistics\n");
      HORIZONTAL_RULE;
    
      for (i=0; i<nstats; i++) {
	STAT_PRINT(stats[i].name,stats[i].value);
      }

    }

  }
}

/*
 ******************************************************************************
 */

#ifdef TIDY
static void
TidyCosts(

perfy_option_t	*options,
Events		*tally,
int		nevents)

{
  int		i;
  double	MaxCost;

  /* Sort the events by counter number.
   */
  qsort(tally, (size_t) nevents, (size_t) sizeof(Events),
	(int (*)(const void *, const void*)) CounterCompare);

  if (tally[CYCLESEVENT].counter == CYCLESEVENT) {
    MaxCost = tally[CYCLESEVENT].cost[TYPICAL];
    for (i=0; i<nevents; i++) {
      if (tally[i].cost[TYPICAL] > MaxCost) tally[i].cost[TYPICAL] = MaxCost;
    }
  }
}
#endif

/*
 ******************************************************************************
 */

static int
CounterCompare(

Events	*event1,
Events	*event2)

{
    return (event1->counter - event2->counter);
}

/*
 ******************************************************************************
 */

static int
CostCompare(

Events	*event1,
Events	*event2)

{
  if ((event1->counter == CYCLESEVENT)  || (event2->counter == CYCLESEVENT) ||
      (event1->counter == CYCLESEVENT2) || (event2->counter == CYCLESEVENT2)) {
    if (((event1->counter == CYCLESEVENT)  && (event2->counter == CYCLESEVENT2)) ||
        ((event1->counter == CYCLESEVENT2) && (event2->counter == CYCLESEVENT))) {
      return ((event1->cost[TYPICAL] < event2->cost[TYPICAL]) ? 1 : -1);
    } else if (event1->counter == CYCLESEVENT2) {
      return (-1);
    } else if (event2->counter == CYCLESEVENT2) {
      return (1);
    } else {
      return (event1->counter - event2->counter);
    }
  } else if (event1->cost[TYPICAL] < event2->cost[TYPICAL]) {
    return (1);
  } else if (event1->cost[TYPICAL] > event2->cost[TYPICAL]) {
    return (-1);
  } else {
    return (event1->counter - event2->counter);
  }
}

/*
 ******************************************************************************
 */

static int
PerfyLoadTable(

double	*def,
double	*working,
char	*CostFileName,
int	PrintErrors)

{
  pid_t		pid = -1;	/* needed for SET_FIELD_LENGTHS macro */
  FILE		*fpin;
  int		line, counter;
  char		buffer[BUFLEN], *s;
  double	scratch[NCOUNTERS][NCOSTS];
  perfy_option_t *options;
  PRINT_DEFS(TRUE,TRUE);

  /* If a default cost table has been provided, copy it into the working
   * table so that the working table has valid values whether or not the
   * cost file can be successfully read.
   */
  if (def != NULL) {
    bcopy(def,working,sizeof(scratch));
  }

  /* Try to open the cost file.
   */
#ifdef ALLOW_STDIN_FOR_COST_TABLE
  /*
   * When perfy is used as a filter, there is no problem reading the
   * user-supplied cost table from standard input. But when integrated
   * into perfex, stdin goes to the program being profiled, NOT to
   * perfex, so the cost table would not get where it is supposed to go.
   * Thus, we don't allow standard input as the file to read the cost
   * table from in this case.
   */
  if (strcmp(STANDARDINPUT,CostFileName) == 0) {
    fpin = stdin;
  } else
#endif
  fpin = fopen(CostFileName,"r");
  if (fpin == NULL) {
    if (PrintErrors) {
      fprintf(output_stream,"Unable to open cost table file \"%s\".\nWill use default cost table.\n", CostFileName);
    }
    return (1);
  }

  options = NULL;
  SET_FIELD_LENGTHS(options,TRUE);

  /* Copy the working table into the scratch table. As the cost file is
   * read, the scratch table will be updated. If no errors occurs, the
   * scratch table will overwrite the working table. If errors do occur,
   * we return using the initial values of the working table.
   */
  bcopy(working,scratch,sizeof(scratch));

  /* Grab one line at a time from the cost file and see if we can
   * parse a counter name and its three costs.
   */
  for (line=1; fgets(buffer,BUFLEN,fpin) != NULL; line++) {

    if (strchr(buffer,'\n') == NULL) {
      if (PrintErrors) {
	fprintf(output_stream,"Length of line %d in cost file \"%s\" exceeds limit (%d).\nWill use default cost table.",
		line, CostFileName, BUFLEN-1);
      }
      return (1);
    }

    /* Try to match the input file line to one of the counter outputs.
     * WILL NEED TO CONSIDER MAKING THIS MORE ROBUST & FLEXIBLE.
     */
    for (counter=0; counter<NCOUNTERS; counter++) {
      if ((s = strstr(buffer,EventDesc[counter])) != NULL) break;
    }

    /* If the line matched none of the counters, we skip it. If it matched
     * one of the redundant counters, distinguish which counter it really
     * is from the counter number which should be at the beginning of the
     * line. If nothing precedes the counter description, then an out of
     * data cost table is being used (one with no event numbers), and this
     * is an error.
     */
    switch (counter) {
    case CYCLES0:
    case CYCLES1:
    case GRADUATEDINSTS0:
    case GRADUATEDINSTS1:
      if (s-buffer > EVENT_LEN) {
	counter = atoi(s - (EVENT_LEN+1));
      } else {
	if (PrintErrors) {
	  fprintf(output_stream, "No event number for counter \"%s\"\n",
		  EventDesc[counter]);
	  fprintf(output_stream, "Error occurred at line %d of cost file \"%s\"\n",
		  line, CostFileName);
	}
	return (1);
      }

      break;
    case NCOUNTERS:
      continue;
    }

    /*
     * Locate the costs for this counter. Here we assume that the -ht
     * output has been used as a template so that the first MAX_EVENT_DESC_LEN+EVENTSIZE
     * characters in the line only contain the counter name, not any of its
     * costs.
     */

    if (strlen(buffer) < (line_len - time_len)) {
      if (PrintErrors) {
	fprintf(output_stream, "No cost values for counter \"%s\"\n",
		EventDesc[counter]);
	fprintf(output_stream, "Error occurred at line %d of cost file \"%s\"\n",
		line, CostFileName);
      }
      return (1);
    }

    if (InsertCosts(scratch, buffer+line_len-time_len, CostFileName,
		counter, line, PrintErrors)) {
      return (1);
    }

  }

  fclose(fpin);

  bcopy(scratch,working,sizeof(scratch));

  return (0);
}

/*
 ******************************************************************************
 */

int
InsertCosts(

double	CostTable[NCOUNTERS][NCOSTS],
char	*buffer,
char	*CostFileName,
int	counter,
int	line,
int	PrintErrors)

{
  double	cost[NCOSTS];
  PRINT_DEFS(TRUE,TRUE);

  for (j=0; j<NCOSTS; j++) {
    if (GrabCost(&cost[j], CostFileName, counter, line,
		 (j == 0) ? buffer : NULL,
		 CostNames[order[j]],PrintErrors)) {
      return (1);
    }
  }

  for (j=0; j<NCOSTS; j++) {
    CostTable[counter][order[j]] = cost[j];
  }

  return (0);
}

/*
 ******************************************************************************
 */

static int
GrabCost(

double	*cost,
char	*CostFileName,
int	counter,
int	line,
char	*s,
char	*name,
int	PrintErrors)

{
  double	value;
  char		*unit;

  s = strtok(s," \t\n");
  if (s == NULL) {
    if (PrintErrors) {
      fprintf(output_stream, "No %s cost for counter \"%s\"\n",
	      name, EventDesc[counter]);
      fprintf(output_stream, "Error occurred at line %d of cost file \"%s\"\n",
	      line, CostFileName);
    }
    return (1);
  }
  value = atof(s);
  if (value < 0.0) {
    if (PrintErrors) {
      fprintf(output_stream, "Invalid %s cost (%s) for counter \"%s\"\n",
	      name, s, EventDesc[counter]);
      fprintf(output_stream, "Error occurred at line %d of cost file \"%s\"\n",
	      line, CostFileName);
    }
    return (1);
  }
  unit = strtok(NULL," \t\n");
  if (unit == NULL) {
    if (PrintErrors) {
      fprintf(output_stream, "No units for %s cost for counter \"%s\"\n",
	      name, EventDesc[counter]);
      fprintf(output_stream, "Error occurred at line %d of cost file \"%s\"\n",
	      line, CostFileName);
    }
    return (1);
  }
  if (strcasecmp(unit,"nsec") == 0) {	/* anything besides "nsec" is considered clks */
    value = -value;
  }

  *cost = value;
  return (0);
}

/*
 ******************************************************************************
 * For machine hardware configuration.
 ******************************************************************************
 */

#include <invent.h>

/* Make sure all the boards are defined since no single <invent.h>
 * seems to include them all.
 */
#ifndef INV_IP4BOARD
# define INV_IP4BOARD	(2)
#endif
#ifndef INV_IP5BOARD
# define INV_IP5BOARD	(3)
#endif
#ifndef INV_IP6BOARD
# define INV_IP6BOARD	(4)
#endif
#ifndef INV_IP7BOARD
# define INV_IP7BOARD	(5)
#endif
#ifndef INV_IP9BOARD
# define INV_IP9BOARD	(6)
#endif
#ifndef INV_IP12BOARD
# define INV_IP12BOARD	(7)
#endif
#ifndef INV_IP17BOARD
# define INV_IP17BOARD	(8)
#endif
#ifndef INV_IP15BOARD
# define INV_IP15BOARD	(9)
#endif
#ifndef INV_IP20BOARD
# define INV_IP20BOARD	(10)
#endif
#ifndef INV_IP19BOARD
# define INV_IP19BOARD	(11)
#endif
#ifndef INV_IP22BOARD
# define INV_IP22BOARD	(12)
#endif
#ifndef INV_IP21BOARD
# define INV_IP21BOARD	(13)
#endif
#ifndef INV_IP26BOARD
# define INV_IP26BOARD	(14)
#endif
#ifndef INV_IP25BOARD
# define INV_IP25BOARD	(15)
#endif
#ifndef INV_IP30BOARD
# define INV_IP30BOARD	(16)
#endif
#ifndef INV_IP28BOARD
# define INV_IP28BOARD	(17)
#endif
#ifndef INV_IP32BOARD
# define INV_IP32BOARD	(18)
#endif
#ifndef INV_IP27BOARD
# define INV_IP27BOARD	(19)
#endif

#ifdef FILTER
#define	SETINVENT	setinvent
#define	GETINVENT	getinvent
#define	ENDINVENT	endinvent()
#define	FIND(P,CLASS,TYPE)						\
									\
  if (SETINVENT()) return (-1);						\
  while (((P = GETINVENT()) != NULL) &&					\
         ((P->inv_class     != (CLASS)) ||				\
          (P->inv_type      != (TYPE))));				\
  if (P == NULL) {							\
    ENDINVENT;								\
    return (-1);							\
  }
#else
#define	SETINVENT	setinvent_r
#define	GETINVENT	getinvent_r
#define	ENDINVENT	endinvent_r(p_invent_state)
#define	P_INVENT_STATE	p_invent_state
#define	FIND(P,CLASS,TYPE)						\
									\
  if (SETINVENT(&P_INVENT_STATE)) return (-1);				\
  while (((P = GETINVENT(P_INVENT_STATE)) != NULL) &&			\
         ((P->inv_class     != (CLASS)) ||				\
          (P->inv_type      != (TYPE))));				\
  if (P == NULL) {							\
    ENDINVENT;								\
    return (-1);							\
  }
#endif
  
/*
 ******************************************************************************
 * Returns IP number.
 ******************************************************************************
 */

int
ip(void)

{
  static int	first = TRUE;
  static int	ipnum;

  if (first) {

    inventory_t		*p_inventory;
#ifndef FILTER
    inv_state_t		*P_INVENT_STATE = NULL;
#endif

    FIND(p_inventory,INV_PROCESSOR,INV_CPUBOARD);
    switch (p_inventory->inv_state) {
    case INV_IP4BOARD:
      ipnum = 4;
      break;
    case INV_IP5BOARD:
      ipnum = 5;
      break;
    case INV_IP6BOARD:
      ipnum = 6;
      break;
    case INV_IP7BOARD:
      ipnum = 7;
      break;
    case INV_IP9BOARD:
      ipnum = 9;
      break;
    case INV_IP12BOARD:
      ipnum = 12;
      break;
    case INV_IP15BOARD:
      ipnum = 15;
      break;
    case INV_IP17BOARD:
      ipnum = 17;
      break;
    case INV_IP19BOARD:
      ipnum = 19;
      break;
    case INV_IP20BOARD:
      ipnum = 20;
      break;
    case INV_IP21BOARD:
      ipnum = 21;
      break;
    case INV_IP22BOARD:
      ipnum = 22;
      break;
    case INV_IP25BOARD:
      ipnum = 25;
      break;
    case INV_IP26BOARD:
      ipnum = 26;
      break;
    case INV_IP27BOARD:
      ipnum = 27;
      break;
    case INV_IP28BOARD:
      ipnum = 28;
      break;
    case INV_IP30BOARD:
      ipnum = 30;
      break;
    case INV_IP32BOARD:
      ipnum = 32;
      break;
    default:
      ipnum = -1;
      break;
    }

    ENDINVENT;
    first = FALSE;
  }

  return (ipnum);
}

/*
 ******************************************************************************
 * Returns the speed of the processor in MHz.  C entry point.
 ******************************************************************************
 */

static int
mhz(void)

{
  static int	first = TRUE;
  static int	MHz;

  if (first) {

    inventory_t		*p_inventory;
#ifndef FILTER
    inv_state_t		*P_INVENT_STATE = NULL;
#endif

    FIND(p_inventory,INV_PROCESSOR,INV_CPUBOARD);
    MHz = p_inventory->inv_controller;
    if (MHz <= 0) MHz = -1;

    ENDINVENT;
    first = FALSE;
  }

  return (MHz);
}

/*
 * coprocessor revision identifiers (see hinv.c)
 */

union rev_id {
	unsigned int	ri_uint;
	struct {
#ifdef MIPSEB
		unsigned int	Ri_fill:16,
				Ri_imp:8,		/* implementation id */
				Ri_majrev:4,		/* major revision */
				Ri_minrev:4;		/* minor revision */
#endif
#ifdef MIPSEL
		unsigned int	Ri_minrev:4,		/* minor revision */
				Ri_majrev:4,		/* major revision */
				Ri_imp:8,		/* implementation id */
				Ri_fill:16;
#endif
	} Ri;
};

#define	ri_imp		Ri.Ri_imp
#define	ri_majrev	Ri.Ri_majrev
#define	ri_minrev	Ri.Ri_minrev

/*
 ******************************************************************************
 * Returns the cpu revision number.  
 * This is a rev_id union consisting of major and minor parts.
 * C entry point
 ******************************************************************************
 */

static unsigned
cpurev(void)
{
  static int	first = TRUE;
  static unsigned rev;

  if (first) {

    inventory_t		*p_inventory;
#ifndef FILTER
    inv_state_t		*P_INVENT_STATE = NULL;
#endif

    FIND(p_inventory,INV_PROCESSOR,INV_CPUCHIP);
    rev = p_inventory->inv_state;

    ENDINVENT;
    first = FALSE;
  }
  return (rev);
}

/* returns the unsigned integer major revision number */
unsigned
cpu_rev_maj(void)
{
  static int	first = TRUE;
  static unsigned rev;
  union rev_id revid;

  if (first) {

    inventory_t		*p_inventory;
#ifndef FILTER
    inv_state_t		*P_INVENT_STATE = NULL;
#endif

    FIND(p_inventory,INV_PROCESSOR,INV_CPUCHIP);
    rev = p_inventory->inv_state;

    ENDINVENT;
    first = FALSE;
  }
  revid.ri_uint = rev;
  return (revid.ri_majrev);
}

/* *******************************************
 * int system_cpu_mix(void)
 * returns an interger code defined in counts.h indicating 
 * which qualitative types of cpu are present.  Currently 
 * limited to pure R10k or R12k, or a mix of the two. 
 * ********************************************
 * Arguments: none
 * Return values:  (nonpositive indicates error)
 *           0: no recognized cputypes present
 *          -1: sysinfo call failed
 *           positive: code for CPUSPECIES_* types defined in counts.h
 *                
 */

#include <sys/systeminfo.h>

int
system_cpu_mix(void) 
{
  int r10k = 0;
  int r12k = 0;
  char *token;
  char buf[MAXCPU * 16];

  if (sysinfo(_MIPS_SI_PROCESSORS,buf,sizeof(buf)) == -1) {
    /*
     * not much else we could do if the system call fails
     */
    return -1;       
  }
  
  for (token = strtok(buf," ");  1 ; token = strtok(NULL," ") ) {
    if (token == NULL)
      break;
    if (!strcmp(token,"R10000"))
      r10k++;
    if (!strcmp(token,"R12000"))
      r12k++;
  }

  if (r10k && r12k)
    return CPUSPECIES_MIXED_R10000_R12000;
  if (r10k && ! r12k)
    return CPUSPECIES_PURE_R10000;
  if (! r10k && r12k)
    return CPUSPECIES_PURE_R12000;

  /* no recognized cpu types */
    return 0;
}

