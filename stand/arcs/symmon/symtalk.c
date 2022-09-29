#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <libgen.h>


main(int argc, char **argv)
{
	int sock;
	struct sockaddr_in name, fromname;
	struct hostent *hp;
	char buf[256];
	int cc, junk;
	char *cmdname;

	cmdname = basename(argv[0]);
	if (argc != 2) {
		printf("Usage: %s <hostname>\n", cmdname);
		exit(-1);
	}
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		perror("open socket");
		exit(1);
	}
	hp = gethostbyname(argv[1]);
	if (hp == 0) {
		fprintf(stderr, "%s: unknown host\n", argv[1]);
		exit(2);
	}
	memcpy((char *) &name.sin_addr, (char *) hp->h_addr, hp->h_length);
	name.sin_family = AF_INET;
	name.sin_port = 3152;

	while (1) {
		printf(">> ");
		gets(buf);
		printf("%s\n", buf);
		switch (buf[0]) {
			case 's' :
			{
				char *data;
				int datalen;

				if (strlen(buf) < 3) {
					printf("ERROR: no data to send\n");
					break;
				}
					
				data = &buf[2];
				datalen = strlen(data);

				printf("Send <%s>\n", data);
				
				if (sendto(sock, &buf[2], strlen(&buf[2]), 0,
				     (struct sockaddr *) &name,
				   	sizeof name) < 0)
				{
					perror("send datagram");
				} else
					printf("Send succesful\n");
				break;
			}

			case 'r':
			{
				printf("Blocked in Recv command\n");

				if ((cc = recvfrom(sock, buf, 80, 0,
					(struct sockaddr *) &fromname,
						&junk)) < 0)
				{
					perror("recv datagram");
					break;
				}
				printf("Received %d chars from %s, port %d\n",
					cc, inet_ntoa(fromname.sin_addr),
						fromname.sin_port);
				buf[cc] = NULL;
				printf("%s\n", buf);
				break;
			}

			case 'e':
				goto out;

			case '?':
			case 'h':
				printf("Program to Test Symmon Connectivity\n");
				printf("\tSend Data: \t\ts <datastring>\n");
				printf("\tRecieve Data:\t\tr\n");
				printf("\tExit Program:\t\te\n");
				break;
			default:
				printf("Unknown Command <%c>\n", buf[0]);
		}
	}
					
out:
	close(sock);
	exit(0);
}
