/*
 * print.c  printing bloat
 */

#include <sys/types.h>
#include <stdio.h>

#include "process.h"
#include "print.h"

/*
 *  void PrintAllBloat(FILE *fp, PROGRAM *bloat)
 *
 *  Description:
 *      Print out info about all programs that are running
 *
 *  Parameters:
 *      fp     stream to print to
 *      bloat  the bloat to print
 */

void PrintAllBloat(FILE *fp, PROGRAM *bloat)
{
    long totSize = 0, totRes = 0, totPhys = 0, totPriv = 0;

    fprintf(fp, "-----------------------------------"
	    "-----------------------------------\n");

    fprintf(fp, "        All Programs:\n");
    fprintf(fp, "                "
	    "Name\t  Size\t   Res\t  Phys\t  Priv\tCopies\n");
    
    while (bloat) {
	if (bloat->print) {
	    fprintf(fp, "%20s\t%6d\t%6d\t%6d\t%6d\t%6d\n",
		    bloat->progName, bloat->size, bloat->resSize,
		    bloat->weightSize, bloat->privSize, bloat->nProc);
	    totSize += bloat->size;
	    totRes += bloat->resSize;
	    totPhys += bloat->weightSize;
	    totPriv += bloat->privSize;
	}
	bloat = bloat->next;
    }
    fprintf(fp, "              Totals:\t%6d\t%6d\t%6d\t%6d\n",
	    totSize, totRes, totPhys, totPriv);
}

/*
 *  void PrintMapBloat(FILE *fp, PROGRAM *bloat)
 *
 *  Description:
 *      Print out info about all resident mappings
 *
 *  Parameters:
 *      fp     stream to print to
 *      bloat  the bloat to print
 */

void PrintMapBloat(FILE *fp, PROGRAM *bloat)
{
    long totPhys = 0, totPriv = 0;

    fprintf(fp, "-----------------------------------"
	    "-----------------------------------\n");

    fprintf(fp, "   Resident Mappings:\n");

    fprintf(fp, "                "
	    "Name\t           Type\t  Phys\t  Priv\n");

    while (bloat) {
	if (bloat->print && bloat->mapType) {
	    fprintf(fp, "%20s\t%15s\t%6d\t%6d\n",
		    bloat->mapName ? bloat->mapName : "none",
		    bloat->mapType, bloat->weightSize,
		    bloat->privSize);
	    totPhys += bloat->weightSize;
	    totPriv += bloat->privSize;
	}
	bloat = bloat->next;
    }
    fprintf(fp, "              Totals:\t               \t%6d\t%6d\n",
	    totPhys, totPriv);
}

/*
 *  void PrintProcBloat(FILE *fp, PROGRAM *bloat, char *procName, pid_t pid)
 *
 *  Description:
 *      Print info about all the mappings for a particular program
 *
 *  Parameters:
 *      fp        stream to print to
 *      bloat     bloat to print
 *      procName  name of the program whose mappings are being printed
 *      pid       pid of the program whose mappings are being printed
 */

void PrintProcBloat(FILE *fp, PROGRAM *bloat, char *procName, pid_t pid)
{
    long totSize = 0, totRes = 0, totPhys = 0, totPriv = 0;
    char buf[50];

    fprintf(fp, "-----------------------------------"
	    "-----------------------------------\n");

    if (pid == -1) {
	snprintf(buf, sizeof buf, "%s", procName);
    } else {
	snprintf(buf, sizeof buf, "%s (%d)", procName, pid);
    }

    fprintf(fp, "%20s:\n", buf);
    fprintf(fp, "                "
	    "Name\t           Type\t  Size\t   Res\t  Phys\t  Priv\n");
    
    while (bloat) {
	if (bloat->print) {
	    fprintf(fp, "%20s\t%15s\t%6d\t%6d\t%6d\t%6d\n",
		    bloat->mapName ? bloat->mapName : "none",
		    bloat->mapType, bloat->size,
		    bloat->resSize, bloat->weightSize,
		    bloat->privSize);
	    totSize += bloat->size;
	    totRes += bloat->resSize;
	    totPhys += bloat->weightSize;
	    totPriv += bloat->privSize;
	}
	bloat = bloat->next;
    }
    fprintf(fp, "              Totals:\t              \t%6d\t%6d\t%6d\t%6d\n",
	    totSize, totRes, totPhys, totPriv);
}
