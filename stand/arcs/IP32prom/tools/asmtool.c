#include <stdio.h>
#include <inttypes.h>

/*
 * Open an stripped elf image file and convert it into
 * a big data file (in mips assembly source form).
 */
main (int argc, char *argv[])
{
  FILE      *fp ;
  char      *PROG ;
  int       byteidx, c, didx ; 
  uint32_t  checksum = 0x0 ;

  /*
   * Open the image file for reading.
   */
  PROG = *argv  ;
  if (argc == 1) {
    fprintf (stderr, "%s: No image file\n", PROG) ;
    exit (-1) ;
  } else {
    if ((fp = fopen (*++argv, "r")) == NULL) {
      fprintf (stderr,"%s: Can not open image file %s for reading\n",*argv) ;
      exit (-2) ;
    } else {
      /*
       * Construct a standard assembly header
       */
      fprintf (stdout, "#include <asm.h>\n") ;
      fprintf (stdout, "#include <regdef.h>\n") ;
      fprintf (stdout, "#include <sys/cpu.h>\n") ;
      fprintf (stdout, ".set noreorder\n") ;
      fprintf (stdout, ".set nomacro\n") ; 
      fprintf (stdout, ".data\n") ; 
      fprintf (stdout, ".align 3\n") ;
      fprintf (stdout, ".globl POST1KSEG1_IMAGE\n") ;
      fprintf (stdout, "POST1KSEG1_IMAGE:\n") ;

      /*
       * We use byte size to avoid the data size alignment
       * problem.
       */
      checksum = 0 ;
      didx = byteidx = 0 ;
      while ((c = getc(fp)) != EOF) {
        checksum = checksum ^ (c & 0xff);
        if (byteidx == 0) fprintf (stdout, "\t.byte ") ;
        if (byteidx < 7)  {
          fprintf (stdout, "0x%02x ", c) ;
          byteidx += 1 ;
        } else {
          fprintf (stdout, "0x%02x", c) ;
          fprintf (stdout, "\t/* %08d , 0x%08x */\n", didx, didx) ;
          didx += 8 ; 
          byteidx = 0 ;
        }
      }
      fclose (fp) ; 
      fprintf (stdout, "\n") ;
      fprintf (stdout, ".align 3\n") ;
      fprintf (stdout, ".globl POST1KSEG1_IMAGE_END\n") ;
      fprintf (stdout, "POST1KSEG1_IMAGE_END:\n\t.word 0x%0x\n", checksum) ; 
      fprintf (stdout, "\t.word 0xdeadbeef\n") ;
    }
  }
}
