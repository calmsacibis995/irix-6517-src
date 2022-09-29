
/* soundstruct - Build a C structure out of a libaudiofile compatible file */

#include "adpcm.h"
#include "audiofile.h"
#include "stdio.h"
#include "fcntl.h"

struct adpcm_state state;

#define NSAMPLES 32 /* must be even */

char	abuf[NSAMPLES/2];
short	sbuf[NSAMPLES];

int fd;
AFfilehandle file;
long sampfmt, sampwidth;
char arrayname[100],lenconname[100];
int total_output_bytes;
int nchannels;

main(argc,argv)
int argc;
char **argv;
{
    int n;

    if(argc < 2 || argc > 3) {
	fprintf(stderr,"Usage %s filename [arrayname]\n",argv[0]);
	exit(1);
    }

    /* Figure out the name of the array and the name of the global
       constant that will contain the number of bytes in the array */

    if(argc==3)
	strcpy(arrayname,argv[2]);
    else
	strcpy(arrayname,"soundbuf");
    sprintf(lenconname,"%s_size",arrayname);

    /* Try to open the input file */

    fd=open(argv[1],O_RDONLY);
    if(fd < 0) {
	fprintf(stderr,"Can't open input file %s\n",argv[1]);
	exit(2);
    }
    file=AFopenfd(fd,"r",AF_NULL_FILESETUP);
    if(file == AF_NULL_FILEHANDLE) {
        fprintf(stderr,"Can't use input file %s\n",argv[1]);
        exit(3);
    }

    /* Make sure the file format is acceptable */

    AFgetsampfmt(file,AF_DEFAULT_TRACK,&sampfmt, &sampwidth);
    nchannels=AFgetchannels(file,AF_DEFAULT_TRACK);
    if(sampfmt != AF_SAMPFMT_TWOSCOMP || sampwidth != 16 || nchannels != 1) {
        fprintf(stderr,"Can't handle input file format, file %s\n",argv[1]);
        fprintf(stderr,"It must by 2's complement, 16 bit per sample, mono\n");
        exit(4);
    }

    printf("char %s[]={\n",arrayname);
    while(1) {
	n = AFreadframes(file, AF_DEFAULT_TRACK, sbuf, NSAMPLES);
	if ( n < 0 ) {
	    perror("input file");
	    exit(5);
	}
	if ( n == 0 ) break;
	adpcm_coder(sbuf, abuf, n, &state);
	total_output_bytes += (n+1)/2;
        outputsamps(abuf,(n+1)/2);
    }
    printf("};\nint %s = %d;\n",lenconname, total_output_bytes);
#ifdef DEBUG
    fprintf(stderr, "Final valprev=%d, index=%d\n",
	    state.valprev, state.index);
#endif
    exit(0);
}
outputsamps(buf,cnt)
char *buf;
int cnt;
{
    int i;
    printf("    ");
    for(i=0;i<cnt;i++)printf("%3d,",(unsigned)buf[i]);
    printf("\n");
}
