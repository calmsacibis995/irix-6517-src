/*************************************************************************/
/*                                                                       */
/*  Copyright (c) 1994 Stanford University                               */
/*                                                                       */
/*  All rights reserved.                                                 */
/*                                                                       */
/*  Permission is given to use, copy, and modify this software for any   */
/*  non-commercial purpose as long as this copyright notice is not       */
/*  removed.  All other uses, including redistribution in whole or in    */
/*  part, are forbidden without prior written permission.                */
/*                                                                       */
/*  This software is provided with absolutely no warranty and no         */
/*  support.                                                             */
/*                                                                       */
/*************************************************************************/

/*************************************************************************/
/*                                                                       */
/*  SPLASH Ocean Code                                                    */
/*                                                                       */
/*  This application studies the role of eddy and boundary currents in   */
/*  influencing large-scale ocean movements.  This implementation uses   */
/*  dynamically allocated four-dimensional arrays for grid data storage. */
/*                                                                       */
/*  Command line options:                                                */
/*                                                                       */
/*     -nN : Simulate NxN ocean.  N must be (power of 2)+2.              */
/*     -pP : P = number of processors.  P must be power of 2.            */
/*     -eE : E = error tolerance for iterative relaxation.               */
/*     -rR : R = distance between grid points in meters.                 */
/*     -tT : T = timestep in seconds.                                    */
/*     -s  : Print timing statistics.                                    */
/*     -o  : Print out relaxation residual values.                       */
/*     -h  : Print out command line options.                             */
/*                                                                       */
/*  Default: OCEAN -n130 -p1 -e1e-7 -r20000.0 -t28800.0                  */
/*                                                                       */
/*  NOTE: This code works under both the FORK and SPROC models.          */
/*                                                                       */
/*************************************************************************/

MAIN_ENV

#define DEFAULT_N      258
#define DEFAULT_P        1
#define DEFAULT_E        1e-7
#define DEFAULT_T    28800.0
#define DEFAULT_R    20000.0
#define UP               0
#define DOWN             1
#define LEFT             2
#define RIGHT            3
#define UPLEFT           4
#define UPRIGHT          5
#define DOWNLEFT         6
#define DOWNRIGHT        7
#define PAGE_SIZE     4096

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include "decs.h"

struct global_struct *global;
struct Global_Private gp[MAX_PROCS];
struct bars_struct *bars;
struct locks_struct *locks;
struct multi_struct *multi;

double ****psi;
double ****psim;
double ***psium;
double ***psilm;
double ***psib;
double ***ga;
double ***gb;
double ****work1;
double ***work2;
double ***work3;
double ****work4;
double ****work5;
double ***work6;
double ****work7;
double ****temparray;
double ***tauz;
double ***oldga;
double ***oldgb;
double *f;
double ****q_multi;
double ****rhs_multi;

void subblock();
void slave();
int log_2(int);
void printerr(char *);

int nprocs = DEFAULT_P;
double h1 = 1000.0;
double h3 = 4000.0;
double h = 5000.0;
double lf = -5.12e11;
double res = DEFAULT_R;
double dtau = DEFAULT_T;
double f0 = 8.3e-5;
double beta = 2.0e-11;
double gpr = 0.02;
int im = DEFAULT_N;
int jm;
double tolerance = DEFAULT_E;
double eig2;
double ysca;
int jmm1;
double pi;
double t0 = 0.5e-4 ;
double outday0 = 1.0;
double outday1 = 2.0;
double outday2 = 2.0;
double outday3 = 2.0;
double factjacob;
double factlap;
int numlev;
int *imx;
int *jmx;
double *lev_res;
double *lev_tol;
double maxwork = 10000.0;

double *i_int_coeff;
double *j_int_coeff;
int xprocs;
int yprocs;
int *xpts_per_proc;
int *ypts_per_proc;
int minlevel;
int do_stats = 0;
int do_output = 0;

#if NUMA
static void numa_init(void);
#endif /* NUMA */

void 
main(int argc, char *argv[])
{
   int i;
   int j;
   int k;
   double work_multi;
   int my_num;
   int x_part;
   int y_part;
   int d_size;
   int itemp;
   int jtemp;
   double procsqrt;
   FILE *fileptr;
   int iindex;
   int temp = 0;
   char c;
   double min_total;
   double max_total;
   double avg_total;
   double min_multi;
   double max_multi;
   double avg_multi;
   double min_frac;
   double max_frac;
   double avg_frac;
   int ch;
   extern char *optarg;
   unsigned int computeend;
   unsigned int start;

   CLOCK(start)

   while ((ch = getopt(argc, argv, "n:p:e:r:t:soh")) != -1) {
     switch(ch) {
     case 'n': im = atoi(optarg);
               if (log_2(im-2) == -1) {
                 printerr("Grid must be ((power of 2)+2) in each dimension\n");
                 exit(-1);
               }
               break;
     case 'p': nprocs = atoi(optarg);
               if (nprocs < 1) {
                 printerr("P must be >= 1\n");
                 exit(-1);
               }
               if (log_2(nprocs) == -1) {
                 printerr("P must be a power of 2\n");
                 exit(-1);
               }
               break;
     case 'e': tolerance = atof(optarg); break;
     case 'r': res = atof(optarg); break;
     case 't': dtau = atof(optarg); break;
     case 's': do_stats = !do_stats; break;
     case 'o': do_output = !do_output; break;
     case 'h': printf("Usage: OCEAN <options>\n\n");
               printf("options:\n");
               printf("  -nN : Simulate NxN ocean.  N must be (power of 2)+2.\n");
               printf("  -pP : P = number of processors.  P must be power of 2.\n");
               printf("  -eE : E = error tolerance for iterative relaxation.\n");
               printf("  -rR : R = distance between grid points in meters.\n");
               printf("  -tT : T = timestep in seconds.\n");
               printf("  -s  : Print timing statistics.\n");
               printf("  -o  : Print out relaxation residual values.\n");
               printf("  -h  : Print out command line options.\n\n");
               printf("Default: OCEAN -n%1d -p%1d -e%1g -r%1g -t%1g\n",
                       DEFAULT_N,DEFAULT_P,DEFAULT_E,DEFAULT_R,DEFAULT_T);
               exit(0);
               break;
     }
   }

   MAIN_INITENV(,60000000) 

   jm = im;
   printf("\n");
   printf("Ocean simulation with W-cycle multigrid solver\n");
   printf("    Processors                         : %1d\n",nprocs);
   printf("    Grid size                          : %1d x %1d\n",im,jm);
   printf("    Grid resolution (meters)           : %0.2f\n",res);
   printf("    Time between relaxations (seconds) : %0.0f\n",dtau);
   printf("    Error tolerance                    : %0.7g\n",tolerance);
   printf("\n");

   xprocs = 0;
   yprocs = 0;
   procsqrt = sqrt((double) nprocs);
   j = (int) procsqrt;
   while ((xprocs == 0) && (j > 0)) {
     k = nprocs / j;
     if (k * j == nprocs) {
       if (k > j) {
         xprocs = j;
         yprocs = k;
       } else {
         xprocs = k;
         yprocs = j;
       }
     }
     j--;
   }
   if (xprocs == 0) {
     printerr("Could not find factors for subblocking\n");
     exit(-1);
   }  

   minlevel = 0;
   itemp = 1;
   jtemp = 1;
   numlev = 0;
   minlevel = 0;
   while (itemp < (im-2)) {
     itemp = itemp*2;
     jtemp = jtemp*2;
     if ((itemp/yprocs > 1) && (jtemp/xprocs > 1)) {
       numlev++;
     }
   }  
   
   if (numlev == 0) {
     printerr("Must have at least 2 grid points per processor in each dimension\n");
     exit(-1);
   }

   global = (struct global_struct *) G_MALLOC(sizeof(struct global_struct));

#if NUMA
   numa_init();
#endif /* NUMA */
   
   imx = (int *) G_MALLOC(numlev*sizeof(int));
   jmx = (int *) G_MALLOC(numlev*sizeof(int));
   lev_res = (double *) G_MALLOC(numlev*sizeof(double));
   lev_tol = (double *) G_MALLOC(numlev*sizeof(double));
   i_int_coeff = (double *) G_MALLOC(numlev*sizeof(double));
   j_int_coeff = (double *) G_MALLOC(numlev*sizeof(double));
   xpts_per_proc = (int *) G_MALLOC(numlev*sizeof(int));
   ypts_per_proc = (int *) G_MALLOC(numlev*sizeof(int));

   imx[numlev-1] = im;
   jmx[numlev-1] = jm;
   lev_res[numlev-1] = res;
   lev_tol[numlev-1] = tolerance;

   for (i=numlev-2;i>=0;i--) {
     imx[i] = ((imx[i+1] - 2) / 2) + 2;
     jmx[i] = ((jmx[i+1] - 2) / 2) + 2;
     lev_res[i] = lev_res[i+1] * 2;
   }

   for (i=0;i<numlev;i++) {
     xpts_per_proc[i] = (jmx[i]-2) / xprocs;
     ypts_per_proc[i] = (imx[i]-2) / yprocs;
   }  
   for (i=numlev-1;i>=0;i--) {
     if ((xpts_per_proc[i] < 2) || (ypts_per_proc[i] < 2)) {
       minlevel = i+1;
       break;
     }
   }    
 
   for (i=0;i<numlev;i++) {
     temp += imx[i];
   }
   temp = 0;
   j = 0;
   for (k=0;k<numlev;k++) {
     for (i=0;i<imx[k];i++) {
       j++;
       temp += jmx[k];
     }
   }

   d_size = nprocs*sizeof(double ***);
   psi = (double ****) G_MALLOC(d_size);
   psim = (double ****) G_MALLOC(d_size);
   work1 = (double ****) G_MALLOC(d_size);
   work4 = (double ****) G_MALLOC(d_size);
   work5 = (double ****) G_MALLOC(d_size);
   work7 = (double ****) G_MALLOC(d_size);
   temparray = (double ****) G_MALLOC(d_size);

   d_size = 2*sizeof(double **);
   for (i = 0; i < nprocs; i++) {
	   psi[i] = (double ***) N_MALLOC(d_size, gp[i].ap);
	   psim[i] = (double ***) N_MALLOC(d_size, gp[i].ap);
	   work1[i] = (double ***) N_MALLOC(d_size, gp[i].ap);
	   work4[i] = (double ***) N_MALLOC(d_size, gp[i].ap);
	   work5[i] = (double ***) N_MALLOC(d_size, gp[i].ap);
	   work7[i] = (double ***) N_MALLOC(d_size, gp[i].ap);
	   temparray[i] = (double ***) N_MALLOC(d_size, gp[i].ap);
   }

   d_size = nprocs*sizeof(double **);
   psium = (double ***) G_MALLOC(d_size);
   psilm = (double ***) G_MALLOC(d_size);
   psib = (double ***) G_MALLOC(d_size);
   ga = (double ***) G_MALLOC(d_size);
   gb = (double ***) G_MALLOC(d_size);
   work2 = (double ***) G_MALLOC(d_size);
   work3 = (double ***) G_MALLOC(d_size);
   work6 = (double ***) G_MALLOC(d_size);
   tauz = (double ***) G_MALLOC(d_size);
   oldga = (double ***) G_MALLOC(d_size);
   oldgb = (double ***) G_MALLOC(d_size);

   for (i = 0; i < nprocs; i++) {
	   gp[i].rel_num_x = (int *) N_MALLOC(numlev*sizeof(int), 
						  gp[i].ap);
	   gp[i].rel_num_y = (int *) N_MALLOC(numlev*sizeof(int), 
						  gp[i].ap);
	   gp[i].eist = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].ejst = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].oist = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].ojst = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].rlist = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].rljst = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].rlien = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].rljen = (int *) N_MALLOC(numlev*sizeof(int), gp[i].ap);
	   gp[i].multi_time = 0;
	   gp[i].total_time = 0;
   }

   subblock();

   x_part = (jm - 2)/xprocs + 2;
   y_part = (im - 2)/yprocs + 2;

   d_size = x_part*y_part*sizeof(double) + y_part*sizeof(double *);

   for (i = 0; i < nprocs; i++) {
	   psi[i][0] = (double **) N_MALLOC(d_size, gp[i].ap);
	   psi[i][1] = (double **) N_MALLOC(d_size, gp[i].ap);
	   psim[i][0] = (double **) N_MALLOC(d_size, gp[i].ap);
	   psim[i][1] = (double **) N_MALLOC(d_size, gp[i].ap);
	   psium[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   psilm[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   psib[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   ga[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   gb[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work1[i][0] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work1[i][1] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work2[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work3[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work4[i][0] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work4[i][1] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work5[i][0] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work5[i][1] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work6[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work7[i][0] = (double **) N_MALLOC(d_size, gp[i].ap);
	   work7[i][1] = (double **) N_MALLOC(d_size, gp[i].ap);
	   temparray[i][0] = (double **) N_MALLOC(d_size, gp[i].ap);
	   temparray[i][1] = (double **) N_MALLOC(d_size, gp[i].ap);
	   tauz[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   oldga[i] = (double **) N_MALLOC(d_size, gp[i].ap);
	   oldgb[i] = (double **) N_MALLOC(d_size, gp[i].ap);
   }

   f = (double *) G_MALLOC(im*sizeof(double));

   multi = (struct multi_struct *) G_MALLOC(sizeof(struct multi_struct));

   d_size = numlev*sizeof(double **);
   if (numlev%2 == 1) {         /* To make sure that the actual data 
                                   starts double word aligned, add an extra
                                   pointer */
     d_size += sizeof(double **);
   }
   for (i=0;i<numlev;i++) {
     d_size += ((imx[i]-2)/yprocs+2)*((jmx[i]-2)/xprocs+2)*sizeof(double)+
              ((imx[i]-2)/yprocs+2)*sizeof(double *);
   }

   d_size *= nprocs;

   if (nprocs % 2 == 1) {         
	   /* 
	    * To make sure that the actual data 
	    * starts double word aligned, add an extra
	    * pointer 
	    */
	   d_size += sizeof(double ***);
   }

   d_size += nprocs * sizeof(double ***);

   q_multi = (double ****) G_MALLOC(d_size); 
   rhs_multi = (double ****) G_MALLOC(d_size);

   locks = (struct locks_struct *) G_MALLOC(sizeof(struct locks_struct));
   bars = (struct bars_struct *) G_MALLOC(sizeof(struct bars_struct));

   LOCKINIT(locks->idlock)
   LOCKINIT(locks->psiailock)
   LOCKINIT(locks->psibilock)
   LOCKINIT(locks->donelock)
   LOCKINIT(locks->error_lock)
   LOCKINIT(locks->bar_lock)

   BARINIT(bars->iteration)
   BARINIT(bars->gsudn)
   BARINIT(bars->p_setup) 
   BARINIT(bars->p_redph) 
   BARINIT(bars->p_soln) 
   BARINIT(bars->p_subph) 
   BARINIT(bars->sl_prini)
   BARINIT(bars->sl_psini)
   BARINIT(bars->sl_onetime)
   BARINIT(bars->sl_phase_1)
   BARINIT(bars->sl_phase_2)
   BARINIT(bars->sl_phase_3)
   BARINIT(bars->sl_phase_4)
   BARINIT(bars->sl_phase_5)
   BARINIT(bars->sl_phase_6)
   BARINIT(bars->sl_phase_7)
   BARINIT(bars->sl_phase_8)
   BARINIT(bars->sl_phase_9)
   BARINIT(bars->sl_phase_10)
   BARINIT(bars->error_barrier)

   link_all();

   multi->err_multi = 0.0;
   i_int_coeff[0] = 0.0;
   j_int_coeff[0] = 0.0;
   for (i=0;i<numlev;i++) {
     i_int_coeff[i] = 1.0/(imx[i]-1);
     j_int_coeff[i] = 1.0/(jmx[i]-1);
   }

/* 
 * Initialize constants and variables
 * id is a global shared variable that has fetch-and-add operations
 * performed on it by processes to obtain their pids.   
 */

   global->id = 0;
   global->psibi = 0.0;
   pi = atan(1.0);
   pi = 4.*pi;

   factjacob = -1./(12.*res*res);
   factlap = 1./(res*res);
   eig2 = -h*f0*f0/(h1*h3*gpr);

   jmm1 = jm-1 ;
   ysca = ((double) jmm1)*res ;

   im = (imx[numlev-1]-2)/yprocs + 2;
   jm = (jmx[numlev-1]-2)/xprocs + 2;

   for (i = 1; i < nprocs; i++) {
	   CREATE(slave)  
   }

   if (do_output) {
     printf("                       MULTIGRID OUTPUTS\n");
   }

   slave();
   WAIT_FOR_END(nprocs-1)
   CLOCK(computeend)

   printf("\n");
   printf("                       PROCESS STATISTICS\n");
   printf("                  Total          Multigrid         Multigrid\n");
   printf(" Proc             Time             Time            Fraction\n");
   printf("    0   %15.0f    %15.0f        %10.3f\n",
          gp[0].total_time,gp[0].multi_time,
          gp[0].multi_time/gp[0].total_time);

   if (do_stats) {
     min_total = max_total = avg_total = gp[0].total_time;
     min_multi = max_multi = avg_multi = gp[0].multi_time;
     min_frac = max_frac = avg_frac = gp[0].multi_time/gp[0].total_time;
     for (i=1;i<nprocs;i++) {
       if (gp[i].total_time > max_total) {
         max_total = gp[i].total_time;
       }
       if (gp[i].total_time < min_total) {
         min_total = gp[i].total_time;
       }
       if (gp[i].multi_time > max_multi) {
         max_multi = gp[i].multi_time;
       }
       if (gp[i].multi_time < min_multi) {
         min_multi = gp[i].multi_time;
       }
       if (gp[i].multi_time/gp[i].total_time > max_frac) {
         max_frac = gp[i].multi_time/gp[i].total_time;
       }
       if (gp[i].multi_time/gp[i].total_time < min_frac) {
         min_frac = gp[i].multi_time/gp[i].total_time;
       }
       avg_total += gp[i].total_time;
       avg_multi += gp[i].multi_time;
       avg_frac += gp[i].multi_time/gp[i].total_time;
     }
     avg_total = avg_total / nprocs;
     avg_multi = avg_multi / nprocs;
     avg_frac = avg_frac / nprocs;
     for (i=1;i<nprocs;i++) {
       printf("  %3d   %15.0f    %15.0f        %10.3f\n",
	      i,gp[i].total_time,gp[i].multi_time,
	      gp[i].multi_time/gp[i].total_time);
     }
     printf("  Avg   %15.0f    %15.0f        %10.3f\n",
            avg_total,avg_multi,avg_frac);
     printf("  Min   %15.0f    %15.0f        %10.3f\n",
            min_total,min_multi,min_frac);
     printf("  Max   %15.0f    %15.0f        %10.3f\n",
            max_total,max_multi,max_frac);
   }
   printf("\n");

   global->starttime = start;
   printf("                       TIMING INFORMATION\n");
   printf("Start time                        : %16d\n",
           global->starttime);
   printf("Initialization finish time        : %16d\n",
           global->trackstart);
   printf("Overall finish time               : %16d\n",
           computeend);
   printf("Total time with initialization    : %16d\n",
           computeend-global->starttime);
   printf("Total time without initialization : %16d\n",
           computeend-global->trackstart);
   printf("    (excludes first timestep)\n");
   printf("\n");

   MAIN_END
}

#if NUMA

/*
 * Globals: 
 *   nprocs -- initialized beforehand;
 *   Gloabl -- global data (storage is existent beforehand)
 *   gp -- thread private data (storage is existent beforehand)
 */

static void
numa_init(void)
{
        int n_total_mlds;
        int n_full_mlds;
        int n_empty_mlds;
        int i;
        pmo_handle_t mld;
        pmo_handle_t mldlist[(MAX_PROCS + 1)/2];
        pmo_handle_t mldset;

        /*
         * We know we have two cpus per node, so we need
         * to create only one mld for every two processes.
         */
        n_full_mlds = (nprocs + 1) / 2;

        /*
         * We want to ask for a hypercube.
         * For that, the number of mlds has
         * to be a power of two. So we are
         * padding with empty mlds.
         */

        n_total_mlds = 1;
        while (n_total_mlds < n_full_mlds) {
                n_total_mlds <<= 1;
        }

        n_empty_mlds = n_total_mlds - n_full_mlds;

        printf("About to create %d mlds, full: %d, empty: %d\n",
               n_total_mlds, n_full_mlds, n_empty_mlds);

        /*
         * Now we create the full mlds
         */

        for (i = 0; i < n_full_mlds; i++) {
                mld = mld_create(0, MLD_DEFAULTSIZE);
                if (mld < 0) {
                        perror("mld_create");
                        exit(1);
                }
                printf("Created MLD <%d> for process <%d>\n", mld, i);
                gp[2*i].mld = mld;
                gp[2*i+1].mld = mld;
                mldlist[i] = mld;
        }

        /*
         * Now we create the padding mlds (empty)
         */
        
        for (i = n_full_mlds; i < n_total_mlds; i++) {
                mld = mld_create(0, 0);
                if (mld < 0) {
                        perror("mld_create");
                        exit(1);
                }
                mldlist[i] = mld;
        }

        /*
         * Now group the mlds in a set and place them.
         */

        mldset = mldset_create(mldlist, n_total_mlds);
        if (mldset < 0) {
                perror("mldset_create");
                exit(1);
        }

        if (mldset_place(mldset, TOPOLOGY_FREE, 0, 0, RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        global->mldset = mldset;
        
        /*
         * We will be using a FIXED allocation policy via numa_malloc
         * (N_MALLOC within the ANL macros).
         */

	/*
	 * We need initialize one arena for one full mld
	 */
	for (i = 0; i < n_full_mlds; i++) {
		gp[2 * i].ap = numa_acreate(mldlist[i],
					    ARENA_SIZE_DEFAULT,
					    PAGE_SIZE_DEFAULT);
		assert(gp[2 * i].ap != NULL);
		gp[2 * i + 1].ap = gp[2 * i].ap;
	}

        /*
         * The actual binding between processes and mlds is  done
         * at the beginning of each slave function.
         */
}

#endif /* NUMA */

int log_2(int number)
{
	int cumulative = 1;
	int out = 0;
	int done = 0;

	while ((cumulative < number) && (!done) && (out < 50)) {
		if (cumulative == number) {
			done = 1;
		} else {
			cumulative = cumulative * 2;
			out ++;
		}
	}

	if (cumulative == number) {
		return(out);
	} else {
		return(-1);
	}
}

void printerr(char *s)
{
	fprintf(stderr,"ERROR: %s\n",s);
}













