/*
||
|| setup.U
||
|| This file contains general purpose routines for
|| user program calls mostly for initial setup of the
|| system and for problem definition.
||
*/

EXTERN_ENV

#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif

#define MAIN extern
#include "common.h"

/*
|| INIT VARIABLES :   Initializes some state variables. Sound speed
|| and number of degrees of freedom are passed to this routine as they
|| must be defined before entry (i.e. its only a reminder).  Returns
|| gamma.  Routine also does initial clear on cumulative population
|| counts.  Step by step population counts are done during each step
|| just before collision pairing (in reset()).
*/
void init_variables() {
  int i,
      j,
      k,
      open_cells;
  float start_prob;
  struct acell *ACell;

  snd_speed = 0.02;
  upstream_mfp = 1.0;
  fluid_speed  = 0.1;
  gama = (DF + 2.0) / DF;
  thermal_factor = snd_speed * sqrt(1.0 / (2.0*gama));
  Global->BC = 0;
  Global->time_step = 0;
  Global->cum_steps = 0;
  Global->num_collisions = 0;
  Global->total_collisions = 0;
  Global->exit_num_flow = 0;
  Global->tot_flt_input = 0.0;
  Global->tot_input = 0;

  /* Calculate free stream density */
  open_cells = 0;
  for (j = 0; j < num_ycell; j++)
    for (k = 0; k < num_zcell; k++)
      for (i = 0; i < num_xcell; i++)
        if (Cells[j][k][i].space >= 0)
          open_cells++;

  Global->free_pop = ((float) Global->num_mol)/((float) open_cells);
  Global->num_flow = Global->free_pop 
                   * (num_ycell - 4.0)
                   * (num_zcell-4.0) 
                   * fluid_speed;

  /* Init reservoir variables */
  Global->next_res = 0;
  Global->res_offset = 1;

  num_res = Global->num_mol / RES_RATIO;
  num_res -= (num_res % 2); /* make it even */

  /* Set up collision probability for select routine */
  Global->coll_prob = sqrt(8.0/(M_PI*gama))*snd_speed 
                    / (upstream_mfp*Global->free_pop);

  Global->cum_steps = 0;
  start_prob = Global->coll_prob * Global->free_pop;

  /* Initialize sample data arrays */
  for (j = 0; j < num_ycell; j++)
    for (k = 0; k < num_zcell; k++)
      for (i = 0; i < num_xcell; i++) {
        ACell = &Cells[j][k][i];
        ACell->cum_cell_pop = 0;
        ACell->cell_pop = 0;
        ACell->avg_prob = start_prob;
        ACell->U = 0.0;
        ACell->V = 0.0;
        ACell->W = 0.0;
        ACell->Usqr = 0.0;
        ACell->Vsqr = 0.0;
        ACell->Wsqr = 0.0;
      }
} /* init_variables() */

/*
|| INIT BORDERS :  Set up the edges of space for the different
|| boundary conditions defined in bc.c.  Allows for distinct
|| boundary conditions for each edge and each corner.  Also set
|| up the reservoir box which has the same y and z dimensions 
|| as the general space but has only 4 cells in the x dimension.
|| This means that there is only a depth of one cell at yz plane
|| boundries but this should not present any problems as thermal
|| speeds << 1.0 .
*/
static void init_borders() {
  int i,
      j,
      k;
  float sqrt2 = sqrt(2.0);
  struct bcond *BCond;

  /* top and bottom borders, x-z plane */
  BCond = &Bconds[1];
  BCond->bc_A = 0.0;
  BCond->bc_B = 1.0;
  BCond->bc_C = 0.0;
  BCond->bc_D = -2.0;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  BCond = &Bconds[2];
  BCond->bc_A = 0.0;
  BCond->bc_B = -1.0;
  BCond->bc_C = 0.0;
  BCond->bc_D = num_ycell - 2.0;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  for (k = 2; k < num_zcell-2; k++) {
    for (i = 2; i < num_xcell-2; i++) {
      Cells[0][k][i].space = -1;
      Cells[1][k][i].space = -1;
      Cells[num_ycell-1][k][i].space = -2;
      Cells[num_ycell-2][k][i].space = -2;
    }
  }

  /* walls, x-y plane */
  BCond = &Bconds[3];
  BCond->bc_A = 0.0;
  BCond->bc_B = 0.0;
  BCond->bc_C = 1.0;
  BCond->bc_D = -2.0;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  BCond = &Bconds[4];
  BCond->bc_A = 0.0;
  BCond->bc_B = 0.0;
  BCond->bc_C = -1.0;
  BCond->bc_D = num_zcell - 2.0;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  for (j = 2; j < num_ycell-2; j++) {
    for (i = 2; i < num_xcell-2; i++) {
      Cells[j][0][i].space = -3;
      Cells[j][1][i].space = -3;
      Cells[j][num_zcell-1][i].space = -4;
      Cells[j][num_zcell-2][i].space = -4;
    }
  }

  /* corners between walls and top and bottom */
  BCond = &Bconds[5];
  BCond->bc_A = 0.0;
  BCond->bc_B = 1.0/sqrt2;
  BCond->bc_C = 1.0/sqrt2;
  BCond->bc_D = -2.0*sqrt2;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  BCond = &Bconds[6];
  BCond->bc_A = 0.0;
  BCond->bc_B = -1.0/sqrt2;
  BCond->bc_C = 1.0/sqrt2;
  BCond->bc_D = num_ycell/sqrt2 - 2.0*sqrt2;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  BCond = &Bconds[7];
  BCond->bc_A = 0.0;
  BCond->bc_B = 1.0/sqrt2;
  BCond->bc_C = -1.0/sqrt2;
  BCond->bc_D = num_zcell/sqrt2 - 2.0*sqrt2;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  BCond = &Bconds[8];
  BCond->bc_A = 0.0;
  BCond->bc_B = -1.0/sqrt2;
  BCond->bc_C = -1.0/sqrt2;
  BCond->bc_D = (num_ycell + num_zcell)/sqrt2 - 2.0*sqrt2;
  BCond->bc_temp = 0.0;
  BCond->bc_coef = 1.0;

  for (i = 2; i < num_xcell-2; i++) {
    Cells[0][0][i].space = -5;
    Cells[1][0][i].space = -5;
    Cells[1][1][i].space = -5;
    Cells[0][1][i].space = -5;
    Cells[num_ycell-1][0][i].space = -6;
    Cells[num_ycell-2][0][i].space = -6;
    Cells[num_ycell-1][1][i].space = -6;
    Cells[num_ycell-2][1][i].space = -6;
    Cells[0][num_zcell-1][i].space = -7;
    Cells[1][num_zcell-2][i].space = -7;
    Cells[1][num_zcell-1][i].space = -7;
    Cells[0][num_zcell-2][i].space = -7;
    Cells[num_ycell-1][num_zcell-1][i].space = -8;
    Cells[num_ycell-1][num_zcell-2][i].space = -8;
    Cells[num_ycell-2][num_zcell-1][i].space = -8;
    Cells[num_ycell-2][num_zcell-2][i].space = -8;
  }

  /* entrance and exit borders - conditions not boundarys */
  for (j = 0; j < num_ycell; j++)
    for (k = 0; k < num_zcell; k++) {
      Cells[j][k][0].space = -(2+COND_OFFSET);
      Cells[j][k][1].space = -(2+COND_OFFSET);
      Cells[j][k][num_xcell-1].space = -(1+COND_OFFSET);
      Cells[j][k][num_xcell-2].space = -(1+COND_OFFSET);
    }
} /* init_borders() */

/*
||
|| GET GEOM :  Reads the specified geometry input file and calculates
|| required parameters such as BC cell area ratios.
||
*/
int get_geom(geom_filename)
char geom_filename[];
{
  int bc_cnt,
      i,
      cx,
      cy,
      cz;
  FILE *fd;
  struct bcond *BCond;

  fd = fopen(geom_filename, "r");
  if (fd != NULL) {

    fscanf(fd, "%d", &num_xcell);
    if (num_xcell < 1) {
      fflush(stdout);
      fprintf(stderr, "ERROR: x dimension too small in geometry. (%d) (%s)\n",
             num_xcell, geom_filename);
      MAIN_END(-1)
      exit(-1);

    } else if (num_xcell > MAX_XCELL) {
      fflush(stdout);
      fprintf(stderr, "ERROR: x dimension too large in geometry. (%d) (%s)\n",
             num_xcell, geom_filename);
      MAIN_END(-1)
      exit(-1);
    }

    fscanf(fd, "%d", &num_ycell);
    if (num_ycell < 5) {
      fflush(stdout);
      fprintf(stderr, "ERROR: y dimension too small in geometry. (%d) (%s)\n",
             num_ycell, geom_filename);
      MAIN_END(-1)
      exit(-1);
    }

    fscanf(fd, "%d", &num_zcell);
    if (num_zcell < 5) {
      fflush(stdout);
      fprintf(stderr, "ERROR: z dimension too small in geometry. (%d) (%s)\n",
             num_zcell, geom_filename);
      MAIN_END(-1)
      exit(-1);

    } else if (num_zcell > MAX_ZCELL) {
      fflush(stdout);
      fprintf(stderr, "ERROR: z dimension too large in geometry. (%d) (%s)\n",
             num_zcell, geom_filename);
      MAIN_END(-1)
      exit(-1);
    }

    Cells = (plane *) global_alloc(num_ycell*sizeof(plane), "Cells");
#ifdef LOCKING
    for (cy = 0; cy < num_ycell; cy++)
      for (cz = 0; cz < MAX_ZCELL; cz++)
        for (cx = 0; cx < MAX_XCELL; cx++) {
          LOCKINIT(Cells[cy][cz][cx].sample_lock)
          LOCKINIT(Cells[cy][cz][cx].space_lock)
        }
#endif

    space_lengthm4 = (float) (num_xcell-4);
    space_heightm4 = (float) (num_ycell-4);
    space_widthm4  = (float) (num_zcell-4);

    fscanf(fd, "%d", &bc_cnt);
    bc_cnt += 9;   /* for the 8 edges of the active space, plus [0] not used */

    Bconds = (struct bcond *) calloc(bc_cnt, sizeof(struct bcond));
    if (Bconds <= 0) {
      fflush(stdout);
      fprintf(stderr, "ERROR: Alloc error for boundary conditions.\n");
      MAIN_END(-1)
      exit(-1);
    }
    init_borders();

    for (bc_count = 9; bc_count < bc_cnt; bc_count++) {
      fscanf(fd, "%d %d %d", &cx, &cy, &cz);
      Cells[cy][cz][cx].space = -bc_count;
      BCond = &Bconds[bc_count];
      fscanf(fd, "%f %f %f %f %f %f", 
          &BCond->bc_A,
          &BCond->bc_B,
          &BCond->bc_C, 
          &BCond->bc_D,
          &BCond->bc_temp,
          &BCond->bc_coef);
    }
    Bc_space = (int *) global_alloc(bc_count*sizeof(int), "Bc_space");
    fclose(fd);
    return(TRUE);

  } else
    return(FALSE);
} /* get_geom() */

/*
|| FILL SPACE :  Loads particles into space.
*/
void fill_space() {
  int i;
  struct acell *ACell;
  struct particle *Part;

  for (i = 0; i < Global->num_mol; ++i) {
    Part = &Particles[i];
    Part->x = RANDOM*space_lengthm4 + 2.0;
    Part->y = RANDOM*space_heightm4 + 2.0;
    Part->z = RANDOM*space_widthm4 + 2.0;
    ACell = &Cells[CELL(Part->y)][CELL(Part->z)][CELL(Part->x)];
    if (ACell->space < 0)
      --i;
    else {
      Part->where = ACell;
      Part->u = thermal_factor*BOLTZMAN + fluid_speed;
      Part->v = thermal_factor*BOLTZMAN;
      Part->w = thermal_factor*BOLTZMAN;
      Part->r = thermal_factor*BOLTZMAN;
      Part->s = thermal_factor*BOLTZMAN;
      ++ACell->cell_pop;
    }
  } /* for */
} /* fill_space() */

/*
|| FILL RESERVOIR :  Loads num_res particles into reservoir.
|| The reservoir is a unit dimension cube used as a source
|| of free stream molecules when the exit flow is not enough
|| to sustain the required free stream or entrance flow.
*/
void fill_reservoir() {
  int i;
  struct ares *RPart;

  Ares = (struct ares *) global_alloc(num_res*sizeof(struct ares), "Ares");

  for (i = 0; i < num_res; i++) {
    RPart = &Ares[i];

    /* normalized spacial coordinates */
    RPart->xr = RANDOM;
    RPart->yr = RANDOM;
    RPart->zr = RANDOM;

    /* translational velocities */
    RPart->ur = thermal_factor*BOLTZMAN;
    RPart->vr = thermal_factor*BOLTZMAN;
    RPart->wr = thermal_factor*BOLTZMAN;

    /* rotational velocities */
    RPart->rr = thermal_factor*BOLTZMAN;
    RPart->sr = thermal_factor*BOLTZMAN;
  } /* for */
} /* fill_reservoir() */

/*
|| FILL MIX TABLE :  This procedure fills the table of collision
|| mix possibilities.  Every Permutation of possible mean
|| and relative velocity component pairings is loaded.  
|| This coupled with different possible sign 
|| choices (see next routine) yeilds 3840 possible collisions 
|| outcomes.
*/
void fill_mix_table() {
  int i,
      j,
      k,
      l,
      m,
      entry,
      ii, 
      jj, 
      kk, 
      ll;

  entry = 0;
  ii = 0;
  for (i=0; i<MIX_DOF; i++) {
    jj = 0;
    for (j=0; j<MIX_DOF-1; j++) {
      if (ii==jj)
        jj++;
      kk = 0;
      for (k=0; k<MIX_DOF-2; k++) {
        while ((kk==jj)||(kk==ii))
          kk++;
        ll = 0;
        for (l=0; l<MIX_DOF-3; l++) {
          while ((ll==jj)||(ll==ii)||(ll==kk))
            ll++;
          m = 0;
          while ((m==ii)||(m==jj)||(m==kk)||(m==ll))
            m++;
          Mix_table[entry][0] = ii;
          Mix_table[entry][1] = jj;
          Mix_table[entry][2] = kk;
          Mix_table[entry][3] = ll;
          Mix_table[entry][4] = m;
          entry++;
          ll++;
        } /* l loop */
        kk++;
      } /* k loop */
      jj++;
    } /* j loop */
    ii++;
  } /* i loop */
} /* fill_mix_table() */

/*
|| FILL SIGN TABLE :  This procedure fills the table of collision
|| mix sign possibilities.  Every Permutation of possible sign-relative
|| pairings is loaded.  This coupled with different possible mix
|| choices (see previous routine) yields 3840 possible collisions 
|| outcomes.
*/
void fill_sign_table() {
  int i,
      j,
      k,
      l,
      m,
      entry;
  float s0,
        s1,
        s2,
        s3,
        s4;

  s0 = s1 = s2 = s3 = s4 = 1.0;
  entry = 0;
  for (i=0; i<2; i++) {
    for (j=0; j<2; j++) {
      for (k=0; k<2; k++) {
        for (l=0; l<2; l++) {
          for (m=0; m<2; m++) {
            Sign_table[entry][0] = s0;
            Sign_table[entry][1] = s1;
            Sign_table[entry][2] = s2;
            Sign_table[entry][3] = s3;
            Sign_table[entry][4] = s4;
            entry++;
            s0 = -s0;
          }
          s1 = -s1;
        } /* l loop */
        s2 = -s2;
      } /* k loop */
      s3 = -s3;
    } /* j loop */
    s4 = -s4;
  } /* i loop */
} /* fill_sign_table() */
