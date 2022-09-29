#define LIM 5
#define SIZE 4092

main()
{
	int i;
	sbrk(1);
	for (i = 1; i < LIM; i++) {
		printf("0x%x \n", malloc(SIZE));
	}
}
