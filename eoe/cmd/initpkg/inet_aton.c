/*
 * tiny binary to use inet_aton, from libc/src/inet/inet_addr.c,
 * to check for valid ascii representation of an Internet address.
 */

#include <arpa/inet.h>

int main (int argc, char *argv[])
{
    if (argc != 2) {
        char *msg = "Usage: inet_aton inetaddr\n";
        write (2, msg, strlen(msg));
        exit (1);
    }
    if (inet_aton (argv[1], 0) != 1)
        exit (2);
    exit (0);
}
