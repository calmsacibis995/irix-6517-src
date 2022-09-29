#include <sys/types.h>
#include <stdio.h>

#define NUMPATTERNS 12
#define NUMBITS 8
#define MAXDATA 0xffff

/* compile with cc in a 4.0.5 environment. Somehow, this doesn't like
 * Sherwood
 */

void main(int argc, char *argv[])
{
	register uint j, k, l, num;
	register unsigned long i;
	register int finished[NUMPATTERNS];
	register int count[NUMPATTERNS];
	register unsigned long value;

	unsigned long cbitpat[] = {
    	0x0,    /* data input of 0x0 will create this pattern */
	0xff,
    	0x73,
    	0xb3,
	0xd3,
	0xe3, 
	0x67,
	0x6b,
	0x6d,
	0x6e,
    	0xaa,
    	0x11
	};


	for (i=0; i < NUMPATTERNS ; i++)
		finished[i] = 0;
		count[i] = 0;

	for (i=0; i < MAXDATA ; i++)
	{
		value = makecheckbit(i);
/*		printf("data is %x and cbit pattern is %x\n", i, value); */
		for (j = 0; j < NUMPATTERNS; j++)	
		{
		   if (value == cbitpat[j])
		   {
		/* only print out for the first data to create the cbitpat */
			if (finished[j] == 0)
	    printf("data = %x creates cbitpat %x\n",i,cbitpat[j]);
			finished[j] = 1;
			count[j] += 1;
			num = 0;
			for (k = 0; k < NUMPATTERNS; k++)
			{
			   if (finished[k])
				num++;
			}
			if (num == NUMPATTERNS) {
				printf("\n");
				for (l = 0; l < NUMPATTERNS; l++)	
	printf("cbitpat %d: %x had %d matches\n",l,cbitpat[l],count[l]);
				printf("\nALL cbitpats found!\n");
				return;
			}
		else if ( i == MAXDATA-1 ) {
				printf("\ndata done,Not all cbitpats found\n");
				printf("\n");
				for (l = 0; l < NUMPATTERNS; l++)	
	printf("cbitpat %d: %x had %d matches\n",l,cbitpat[l],count[l]);
			 }  

		   }  /* if value == cbitpat */
		}  /* for j */
	}  /* for i */
} /* main */


unsigned long 
makecheckbit(unsigned long data)
{
	register int i;
	register unsigned long bit;
	register unsigned long temp;

/*
	unsigned long cbitBits[] = {
		0x145011110ff014ff,
		0x24ff2222f000249f,
		0x4c9f444400ff44d0,
		0x84d08888ff0f8c50,
		0x0931f0ff11110b21,
		0x0b22ff002222fa32,
		0xfa24000f4444ff24,
		0xff280ff088880928
	};
*/
	register unsigned long cbitBits[] = {
                0x0ff014ff,
                0xf000249f,
                0x00ff44d0,
                0xff0f8c50,
                0x11110b21,
                0x2222fa32,
                0x4444ff24,
                0x88880928
	};


	bit = 0x0;

	for (i = 0; i < NUMBITS; i++)
	{
		temp = data & cbitBits[i];
/* printf("temp is %x, data is %x, cbitBits is %x\n", temp, data, cbitBits[i]);
*/
		if (temp) /* the case for at least one bit high */
		{
			if (temp != cbitBits[i]) /* all bits high,then xor=0 */
				bit |= (1 << i);
		}
	}
	return(bit);
}
			
