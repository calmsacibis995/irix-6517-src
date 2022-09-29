#include <string.h>
#include <uif.h>
#include <IP32_status.h>
#include "execlib.h"

static int zzerrors;
static int zzUnit;
static int zzTest;


code_t plp_error_code;

void InfoMsg(char *msg)
{

    msg_printf(INFO,"\n\t%s",msg);
}

void ErrorMsg(int testNumber, char *msg)
{
    plp_error_code.sw = SW_IDE;
    plp_error_code.func = FUNC_PARALLEL;
    plp_error_code.code = (unsigned char)(testNumber & 0x00ff);
    report_error(&plp_error_code);
    msg_printf(ERR,"\n\t\a%d). %s\n",testNumber,msg);
    zzerrors++; 
}

void StartTest(char *test, int Unit, int Test)
{
    zzerrors = 0;
    zzUnit = Unit;
    zzTest = Test;
    plp_error_code.test = Test;
    msg_printf(INFO,"\nTesting \"%s\" (unit==0x%x,test==0x%x)...\n",
				test, Unit, Test);
}

int EndTest(char *test)
{
    if (zzerrors)
	msg_printf(ERR,"Hardware failure detected whil \"%s\"\n", test);
    else 
	msg_printf(VRB,"\"%s\" completed with no errors\n", test);
    return zzerrors;
}
