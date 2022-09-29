char arg[20480];

main()
{
	int i;

	for (i = 0; i < 20480; i++)
		arg[i] = 'a';

	arg[20479] = '\0';

	execl("/bin/sh", arg, arg, (char *)0);
}
