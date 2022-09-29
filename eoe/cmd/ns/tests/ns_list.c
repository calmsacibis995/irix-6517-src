#include <stdio.h>
#include <ns_api.h>

main(int argc, char **argv)
{
	struct rpcent *r;
	FILE *fp;
	char buf[4096];

	for (argc--, argv++; argc > 0; argc--, argv++) {
		fp = ns_list(0, 0, *argv, 0);
		if (fp) {
			while(fgets(buf, sizeof(buf), fp)) {
				fputs(buf, stdout);
			}
			fclose(fp);
		}
	}
}
