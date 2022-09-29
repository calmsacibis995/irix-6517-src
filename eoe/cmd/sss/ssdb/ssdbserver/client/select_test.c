#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "mysql.h"

#define SELECT_QUERY "select name from test where num = %d"


int main(int argc, char **argv)
{
  int	count, num;
  MYSQL mysql,*sock;
  MYSQL_RES *res;
  char	qbuf[160];

  if (argc != 3)
  {
    fprintf(stderr,"usage : select_test <dbname> <num>\n\n");
    exit(1);
  }

  mysql_init(&mysql);
  if (!(sock = mysql_real_connect(&mysql,NULL,0,0,argv[1],0,NULL,0)))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n\n",mysql_error(&mysql));
    perror("");
    exit(1);
  }

  count = 0;
  num = atoi(argv[2]);
  while (count < num)
  {
    sprintf(qbuf,SELECT_QUERY,count);
    if(mysql_query(sock,qbuf) < 0)
    {
      fprintf(stderr,"Query failed (%s)\n",mysql_error(sock));
      exit(1);
    }
    if (!(res=mysql_store_result(sock)))
    {
      fprintf(stderr,"Couldn't get result from query failed\n",
	      mysql_error(sock));
      exit(1);
    }
#ifdef TEST
    printf("number of fields: %d\n",mysql_num_fields(res));
#endif
    mysql_free_result(res);
    count++;
  }
  mysql_close(sock);
  exit(0);
  return 0;					/* Keep some compilers happy */
}
