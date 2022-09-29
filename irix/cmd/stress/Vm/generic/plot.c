# include <stdio.h>

char *malloc();

main(ac, av)
   int ac;
   char *av[];
{
   int steps;
   int *col;
   char buf[1024];
   int div;
   int cnt;
   int snum;
   int slop;

	steps = atoi(av[1]);
	div = atoi(av[2]) / steps;
	col = (int *) malloc(steps * sizeof(int));
	cnt = 0;
	snum = 0;
	slop = div;
	while (gets(buf) != NULL) {
		if (cnt++ > div) {
			snum++;
			div += slop;
		}
		col[snum] += atoi(buf);
	}
	for (cnt = 0; cnt < steps; cnt++)
		printf("%d\n", col[cnt]);
}
