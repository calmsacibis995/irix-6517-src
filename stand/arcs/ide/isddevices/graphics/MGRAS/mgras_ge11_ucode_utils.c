#include <stdio.h>

/*
 * Utility program to convert the GE Ucode hex file into
 * a static array of addr, data2, data1, data0.
 */
main(int argc, char **argv) 
{
    FILE *fp, *ftmp;
    char str_array[20];
    char *str;

    unsigned int addr, data0, data1, data2;
    int 	prev_addr,i;
    int		len;

    if ((fp = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "Failed to open input hex file\n");
	exit(1);
    }
    printf("static  ge_ucode        ge_table[] = {\n");
    prev_addr = -1;
    len = 0;

    while (!feof(fp)) {
	if (fscanf(fp, "%x %2x%8x%8x", &addr, &data2, &data1, &data0) != 4) {
	   if (feof(fp)) break;
	   fprintf(stderr, "Error in reading ucode file at addr = %x\n", addr);
	   exit (1);
	}

	if (addr > (prev_addr + 1)) {
	    len = addr - (prev_addr + 1);
	    for (i=prev_addr+1; i < addr; i++) 
		printf("0x%x, 0x%x, 0x%x, \n", 0,0,0);
        }		
	printf("0x%x, 0x%x, 0x%x,\n", data2, data1, data0);
	len++;
	prev_addr = addr;
    }
    if ((len & 0x1) == 0x1)  {
	printf("0x%x, 0x%x, 0x%x, \n", 0,0,0);
	len++;
    }
    printf("};\n");
    fclose(fp);

    if ((ftmp = fopen("mgras_ge.ucode", "w")) == NULL) {
	fprintf(stderr, "Failed to create mgras_ge.ucode file\n");
	exit(1);
    }
    if ((fp = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "Failed to open input hex file\n");
	exit(1);
    }

    str = str_array;
    prev_addr = -1;
    len = 0;

    while (!feof(fp)) {
	if (fscanf(fp, "%x %s", &addr, str) != 2) {
	   if (feof(fp)) break;
	   fprintf(stderr, "Error in reading ucode file at addr = %x\n", addr);
	   exit(1);
	}
	if (addr > (prev_addr + 1)) {
	    len = addr - (prev_addr + 1);
	    for (i=prev_addr+1; i < addr; i++) 
		fprintf(ftmp, "000000000000000000\n");
        }		
	fprintf(ftmp, "%s\n", str);
	len++;
	prev_addr = addr;

    }
    if ((len & 0x1) == 0x1)  {
	fprintf(ftmp, "000000000000000000\n");
	len++;
    }
    fclose(ftmp);
    fclose(fp);
}
