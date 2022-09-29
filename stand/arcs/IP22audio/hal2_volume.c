/********************************************************************
 *
 * volume.c -
 *     cycle values thru the volume registers and reads 'em back.
 *
 ********************************************************************/

/*#include <sys/IP22.h>*/
#include <sys/hpc3.h>
#include <sys/hal2.h>
#include <sys/sbd.h>

#define  MAX_VOLUME_LEVEL  255               /* a bold assumption */

extern void hal2_configure_pbus_pio(void);
void        hal2_cycle_volume(void);

main()
{
    hal2_configure_pbus_pio();
    hal2_cycle_volume();
}

void
hal2_cycle_volume()
{
    int i, j;
    int error_count = 0;

    volatile unsigned long *volume_left  = 
        (unsigned long *) PHYS_TO_K1(VOLUME_LEFT); 
    volatile unsigned long *volume_right = 
        (unsigned long *) PHYS_TO_K1(VOLUME_RIGHT); 
    volatile unsigned char readback;

    printf("cycling left volume register\n");
    for (i = 0; i < MAX_VOLUME_LEVEL; i++)
    {
         *volume_left = i;
         us_delay(50);
         readback     = *volume_left;
         if (readback != i)
         {
              printf("left volume: read/write values don't match.\n");
              error_count++;
         }
    }
    for (i = 0; i < MAX_VOLUME_LEVEL; i++)
    {
         *volume_right = i;
         us_delay(50);
         readback      = *volume_right;
         if (readback != i)
         {
              printf("right volume: read/write values don't match.\n");
              error_count++;
         }
    }
    for (i = 0; i < MAX_VOLUME_LEVEL; i++)
    {
         *volume_left  = i;
         us_delay(50);
         readback      = *volume_left;
         if (readback != i)
         {
              printf("left volume: read/write values don't match.\n");
              error_count++;
         }
         *volume_right = i;
         us_delay(50);
         readback      = *volume_right;
         if (readback != i)
         {
              printf("right volume: read/write values don't match.\n");
              error_count++;
         }
    }
    printf("cumulative error count = %d\n", error_count);
}
