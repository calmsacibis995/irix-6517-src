#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/spb.h>
#include <libsc.h>

char filebuf[2048];

type(int argc, char *argv[])
{
    LONG fid, cnt, total = 0, i;
    LARGE off;

    if (argc < 2)
	return -1;

    argv++;
    if ((*SPB->TransferVector->Open)
	    ((CHAR *)*argv, OpenReadOnly, &fid) != ESUCCESS) {
	printf("\nError Opening File %s", *argv);
	return -1;
    }

    if (argc == 3) {
	argv++;
	/* wants to seek */
	off.hi = 0;
	atob(*argv, (int *)&off.lo);
    (*SPB->TransferVector->Seek)(fid, &off, SeekAbsolute);
    }
    
    while ((*SPB->TransferVector->GetReadStatus)(fid) == ESUCCESS) {
	
        if ((*SPB->TransferVector->Read)(fid, (void *)filebuf, sizeof(filebuf), &cnt) != ESUCCESS) {
	    printf("\nError Reading File\n");
	    return 0;
        }
	else {
		total += cnt;

	    for (i = 0; i < cnt; i++) 
		putchar(filebuf[i]);

	
        }
    }
    printf("\nNumber of Characters Read = %d\n", total);
    (*SPB->TransferVector->Close)(fid);
    return 0;
}
