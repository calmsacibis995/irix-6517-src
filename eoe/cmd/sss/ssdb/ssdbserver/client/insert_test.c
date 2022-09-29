#include <stdio.h>
#include <stdlib.h>
#include "mysql.h"

#define INSERT_QUERY "insert into test (name,num) values ('item %d', %d)"


int main(int argc, char **argv)
{
  int	count,num;
  MYSQL *sock,mysql;
  char	qbuf[160];

  if (argc != 3)
  {
    fprintf(stderr,"usage : insert_test <dbname> <Num>\n\n");
    exit(1);
  }

  if (!(sock = mysql_connect(&mysql,NULL,0,0)))
  {
    fprintf(stderr,"Couldn't connect to engine!\n%s\n",mysql_error(&mysql));
    perror("");
    exit(1);
  }

  if (mysql_select_db(sock,argv[1]) < 0)
  {
    fprintf(stderr,"Couldn't select database %s!\n%s\n",argv[1],
	    mysql_error(sock));
  }

  num = atoi(argv[2]);
  count = 0;
  while (count < num)
  {
    sprintf(qbuf,INSERT_QUERY,count,count);
    if(mysql_query(sock,qbuf) < 0)
    {
      fprintf(stderr,"Query failed (%s)\n",mysql_error(sock));
      exit(1);
    }
    count++;
  }
  mysql_close(sock);
  exit(0);
  return 0;
}
