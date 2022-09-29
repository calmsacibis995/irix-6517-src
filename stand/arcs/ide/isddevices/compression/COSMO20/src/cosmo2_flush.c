
#include "cosmo2_defs.h"
#include "cosmo2_mcu_def.h"

extern UBYTE checkDataInFifo( UBYTE );
extern int cos2_EnablePioMode( void );

cosmo2_flush( int argc, char **argv  )
{

	int chnl, j;
	__uint64_t tmp;
    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &chnl);
                }
            break;
            default:
                msg_printf(SUM, " usage: -c chnl\n") ;
                break ;
        }
        argc--; argv++;
    }


        cos2_EnablePioMode( );
            while (checkDataInFifo(chnl) ) {
                tmp = cgi1_read_reg(cosmobase + cgi1_fifo_rw_o(chnl));
                    msg_printf(VRB, " chnl %x value %llx\n", chnl, tmp);
            }

	return 0;
}
