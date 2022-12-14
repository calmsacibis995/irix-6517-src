/*
||
|| mp3d.U
||
|| Use : This is a general purpose program that uses the Stanford
|| particle method to examine rarefied flows over objects in a simulated
|| wind tunnel.  Geometry is defined by a cad program. This version
|| uses the ANL macro set to exploit shared memory parallel computer
|| architectures.
||
|| Date : February, 1988.
|| Prog : Jeff McDonald
||        Stanford University.
||
*/

/* Macro for variable declarations */

MAIN_ENV

/* Include files */

#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#define MAIN
#include "common.h"

/* Local variables: */
static char answer[FILENAME_LENGTH],
            *geometry_file_name = "test.geom";

#ifdef LOCKING
#  define MXLOCKS (12+2*MAX_XCELL*24*MAX_ZCELL)
#else
#  define MXLOCKS 10
#endif

extern char *global_alloc(int bytes, char* name);

/*
||  MAIN PROGRAM.  The begining and the end.
*/
int main(argc, argv)
int argc;
char *argv[];
{
  /* ANL macro for initialization */
  MAIN_INITENV(MAX_PROCESSORS,7000000,MXLOCKS)

  fill_mix_table();
  fill_sign_table();
  prepare_multi();

  /* Get user input from command line and initialize */
  get_user_info(argc, argv);

  if (get_geom(geometry_file_name)) {
    init_variables();
    fill_space();
    fill_reservoir();

  } else {
    fflush(stdout);
    perror("ERROR");
    fprintf(stderr, "ERROR: Could not read geometry file (%s).\n",
      geometry_file_name);
    MAIN_END(-1)
    exit(-1);
  }

  /* Main command execution loop */
  answer[0] = 'r';
  answer[1] = '\0';

  Global->take_steps = 0;
  while (answer[0] != 'e') /* Do while not exit command */ {
    do_command(answer);
    printf("Enter Command --> ");
    gets(answer);
  }
  printf("\n");
  MAIN_END
  return 0;
} /* main() */

/*
|| GET USER INFO :  Uses passed command line information
|| to determine molecules, processors, and geometry.
*/
int get_user_info(argc, argv) /* From the command line */
int argc;
char *argv[];
{
  if (argc > 2) {
    sscanf(argv[1], "%d", &Global->num_mol);
    if (Global->num_mol < RES_RATIO*2) {
      fflush(stdout);
      fprintf(stderr, "ERROR: Too few molecules (%d).\n", Global->num_mol);
      MAIN_END(-1)
      exit(-1);
    }
    if (Global->num_mol > MAX_MOL) {
      fflush(stdout);
      fprintf(stderr, "ERROR: Too many molecules (%d).\n", Global->num_mol);
      MAIN_END(-1)
      exit(-1);
    }

    sscanf(argv[2], "%d", &number_of_processors);
    if (number_of_processors < 1) {
      fflush(stdout);
      fprintf(stderr, "ERROR: Too few processors (%d).\n", 
        number_of_processors);
      MAIN_END(-1)
      exit(-1);
    }
    if (number_of_processors > MAX_PROCESSORS) {
      fflush(stdout);
      fprintf(stderr, "ERROR: Too many processors (%d).\n",
        number_of_processors);
      MAIN_END(-1)
      exit(-1);
    }

    if (argc > 3)
      geometry_file_name = argv[3];

  } else {
    fflush(stdout);
    fprintf(stderr, "ERROR: Incorrect argument count (%d).\n\n", argc);
    fprintf(stderr, "Usage: %s <molecules> <processors> [<geometry>]\n",
                          argv[0]);
    MAIN_END(-1)
    exit(-1);
  }
} /* get_user_info() */

/*
||
|| SAVE DENSITY :  Saves complete number density info for
|| later plotting.
||
*/
static int save_density(filename, scale, num_col, cz)
char filename[];
int scale,
    num_col,
    cz;
{
  int i,
      j,
      k,
      ii,
      jj,
      fd,
      i_max,
      j_max,
      icol,
      t1;
  float mach_num,
        max_map,
        slope,
        end1, 
        end2,
        diff,
        fcol;
  char buf[25];
  static float *map[I_MAX];
  static char *cmap[I_MAX];

  fd = open(filename, O_WRONLY | O_CREAT, 0754);
  if (fd < 0) {
    fflush(stdout);
    perror("ERROR");
    fprintf(stderr, "ERROR: Cannot open plot save file (%s).\n", filename);
    fflush(stderr);

  } else {
    sprintf(buf, "%-25d", scale);
    write(fd, buf, 25);
    i_max = (num_xcell-4)*scale;
    sprintf(buf, "%-25d", i_max);
    write(fd, buf, 25);
    j_max = (num_ycell-4)*scale;
    sprintf(buf, "%-25d", j_max);
    write(fd, buf, 25);
    sprintf(buf, "%-25d", Global->num_mol);
    write(fd, buf, 25);
    sprintf(buf, "%-25d", Global->time_step);
    write(fd, buf, 25);
    sprintf(buf, "%-25d", Global->cum_steps);
    write(fd, buf, 25);
    mach_num = fluid_speed/snd_speed;
    sprintf(buf, "%-25f", mach_num);
    write(fd, buf, 25);
    sprintf(buf, "%-25f", fluid_speed);
    write(fd, buf, 25);
    sprintf(buf, "%-25f", Global->free_pop);
    write(fd, buf, 25);

    for (i=0; i<i_max; i++) {
      map[i] = (float *) calloc((unsigned)j_max, sizeof(float));
      if (map[i] <= 0) {
        fflush(stdout);
        fprintf(stderr, "ERROR: Alloc error for plot map.\n");
        fflush(stderr);
        return;
      }
      cmap[i] = (char *) calloc((unsigned)j_max, sizeof(char));
      if (cmap[i] <= 0) {
        fflush(stdout);
        fprintf(stderr, "ERROR: Alloc error for plot cmap.\n");
        fflush(stderr);
        return;
      }
    }

    for (i=0; i<i_max; i++)
      for (j=0; j<j_max; j++)
        map[i][j] = 0.0;

    max_map = 0.0;

    for (i = 2; i < num_xcell-2; ++i) {
      for (j = 2; j < num_ycell-2; ++j) {
        t1  = Cells[j][cz][i].cum_cell_pop;

        ii = (i-2)*scale;
        jj = (j-2)*scale;
        if (Cells[j][cz][i].space < 0) 
          map[ii][jj] = 0.0;
        else
          map[ii][jj] = 1.0*t1;
        if (map[ii][jj] > max_map)
          max_map = map[ii][jj];
      }
    }

    if (scale > 1) {
      slope = 1.0 / scale;
      for (i=0; i<i_max-scale; i+=scale)
        for (j=0; j<j_max; j+=scale) {
          end1 = map[i][j];
          end2 = map[i+scale][j];
          diff = (end2-end1)*slope;
          for (k=1; k<scale; k++)
            map[i+k][j] = map[i][j]+diff*k;
        }
  
      for (i=0; i<i_max; i++)
        for (j=0; j<j_max; j+=scale) {
          end1 = map[i][j];
          end2 = map[i][j+scale];
          diff = (end2-end1)*slope;
          for (k=1; k<scale; k++)
            map[i][j+k] = map[i][j]+diff*k;
        }
    } /* if scale */

    for (i=0; i<i_max; i++)
      for (j=0; j<j_max; j++) {
        fcol = (num_col*map[i][j]/max_map+0.5);
        icol = fcol;
        cmap[i][j] = (char) icol;
      }

    for (i=0; i<i_max; i++)
      write(fd, cmap[i], j_max*sizeof(char));

    close(fd);
    
    for (i=0; i<i_max; i++) {
      cfree((char *)map[i]);
      cfree(cmap[i]);
    }

    printf("\n");
  } /* able to open file */
} /* save_density() */

/*
|| DO COMMAND:  Simple Command Interpreter for the particle
|| simulator.
*/
static void do_command(command)
char command[];
{
  int i,
      j,
      k,
      scale,
      cz,
      num_col,
      temp_tot;
  char buf[FILENAME_LENGTH];
  unsigned int start_time,
               end_time;

  switch (command[0]) {
    case 'P' :
    case 'p' :
      printf("Enter Plot Filename          : ");
      gets(answer);
      printf("Enter Scale                  : ");
      gets(buf);
      sscanf(buf, "%d", &scale);
      printf("Enter Number of Colors       : ");
      gets(buf);
      sscanf(buf, "%d", &num_col);
      printf("Enter Z cell                 : ");
      gets(buf);
      sscanf(buf, "%d", &cz);
      save_density(answer, scale, num_col, cz);
      break;
    case 'R' :
    case 'r' :
      printf("\nRun Status\n\n");
      printf("Number of molecules........%ld\n", Global->num_mol);
      printf("Number of steps............%d\n", Global->time_step);
      printf("Number of Avg. Steps.......%d\n", Global->cum_steps);
      printf("Upstream MFP ..............%f\n", upstream_mfp);
      printf("Free Pop ..................%f\n", Global->free_pop);
      printf("Gamma......................%f\n", gama);
      printf("Coll Prob .................%f\n", Global->coll_prob);
      printf("Num collisions ............%d\n", Global->num_collisions);
      printf("Total collisions...........%d\n", Global->total_collisions);
      printf("Num BC ....................%d\n\n", Global->BC);
      break;

    case 'q' :
      temp_tot = 0;
      for (i=2; i<4; i++)
        for (j=2; j<4; j++) {
          printf("Cum cell pop = %d %d %d %d\n",
             Cells[j][2][i].cum_cell_pop, 
             Cells[j][3][i].cum_cell_pop, 
             Cells[j][4][i].cum_cell_pop,
             Cells[j][5][i].cum_cell_pop);
          for (k=2; k<4; k++) 
            temp_tot += Cells[j][k][i].cell_pop;
        }
      printf("\n\n");
      for (i=2; i<4; i++)
        for (j=2; j<4; j++)
          printf("Cell pop = %d %d %d %d\n",
            Cells[j][2][i].cell_pop, 
            Cells[j][3][i].cell_pop, 
            Cells[j][4][i].cell_pop,
            Cells[j][5][i].cell_pop);
      printf("total in system = %d\n", temp_tot);
      break;

    case 'Q' :
      printf("Number in reservoir is %d\n\n", num_res);
      break;

    case '1' :
    case '2' :
    case '3' :
    case '4' :
    case '5' :
    case '6' :
    case '7' :
    case '8' :
    case '9' :
    case '0' :
      sscanf(command, "%d", &Global->take_steps);
      Global->time_step += Global->take_steps;
      Global->cum_steps += Global->take_steps;
      printf("Working towards %d steps.\n", Global->time_step);

      /* NumberOfProcessors-1 slaves to be started */
      for (I = 1; I < number_of_processors; I++)
        CREATE(slave_advance_solution)

      CLOCK(start_time)
      I = 0;

      /* Make master earn his keep */
      master_advance_solution();

      WAIT_FOR_END(number_of_processors - 1)
      CLOCK(end_time)
      fflush(stdout);
      fprintf(stderr, "Elapsed time for advance: %d\n",
                   end_time-start_time);
      break;
  } /* switch */
} /* do_command() */

/*
|| GLOBAL_ALLOC:  Allocates aligned global memory.
*/
char *global_alloc(int bytes, char* name)
{
  char* temp;

  temp = (char*) G_MALLOC(bytes + ALIGNMENT - 1);
  if (temp == (char*)NULL) {
    fflush(stdout);
    fprintf(stderr, 
      "ERROR: unable to allocate global memory for %s. (%d bytes)\n", 
      name, bytes);
    MAIN_END(-1)
    exit(-1);
  }

  return (char *)temp;
} /* global_alloc() */

/*
|| PREPARE MULTI :  Initializes all ANL constructs as well
|| as allocating global memory.
*/
static int prepare_multi() {

  /* set clump factor for move loop */
  for (clump = 1; (clump * sizeof(struct particle)) % ALIGNMENT; clump++);

  Global = (struct GlobalMemory *) 
       global_alloc(sizeof(struct GlobalMemory), "Global");
  Particles = (struct particle *) 
       global_alloc(MAX_MOL*sizeof(struct particle), "Particles");

  /* Initialize locks, barriers and distributed loops */
  BARINIT(Global->end_sync)
  LOCKINIT(Global->output)
  LOCKINIT(Global->collisions_lock)
  LOCKINIT(Global->num_mol_lock)
  LOCKINIT(Global->next_res_lock)
} /* prepare_multi() */
