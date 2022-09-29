#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <elf.h>

/*
 * Hack to force the type of a .o file to -mips2
 */
int main(int argc, char** argv){
  int fd;
  long flags;
  off_t offset = offsetof(Elf32_Ehdr,e_flags);

  if (argc!=2)
    return 1;

  fd = open(argv[1],O_RDWR);
  if (fd<0)
    return 1;

  lseek(fd,offset,SEEK_SET);
  if (read(fd,&flags,sizeof(flags)!=sizeof(flags)))
    return 1;
  lseek(fd,offset,SEEK_SET);
  flags = (flags & ~EF_MIPS_ARCH) | EF_MIPS_ARCH_2;
  write(fd,&flags,sizeof(flags));
  close(fd);
  return 0;
}
