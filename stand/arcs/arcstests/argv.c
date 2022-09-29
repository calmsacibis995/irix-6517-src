
main(int argc, char **argv, char **envp)
{

    while (argc--)
	printf ("%s\n", *argv++);

    return;
}

