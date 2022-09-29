#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

char stack1[4096];
char stack2[4096];

int
cmp_stack(const char *s, const stack_t *s1, const stack_t *s2)
{
	int return_val = 0;
	if (s1->ss_sp != s2->ss_sp)
	{
		fprintf(stderr, "%s: sp differs: %p -> %p\n", s, s1->ss_sp, s2->ss_sp);
		return_val = 1;
	}
	if (s1->ss_size != s2->ss_size)
	{	
		fprintf(stderr, "%s size differs: %x -> %x\n", s, s1->ss_size, s2->ss_size);
		return_val = 1;
	}
	if (s1->ss_flags != s2->ss_flags)
	{	
		fprintf(stderr, "%s flags differ: %o -> %o\n", s,
			s1->ss_flags, s2->ss_flags);
		return_val = 1;
	}
	return return_val;
}

void
prt_stack(char *s, stack_t *sp)
{
	printf("\tsigstk: %s stack 0x%x: sp=%x size=%x flags=%x\n", 
		s, sp, sp->ss_sp, sp->ss_size, sp->ss_flags);
}

void on_ckpt() {;}

main(void)
{
	int succeeded = 1;
	pid_t ctest_pid = getppid();
	stack_t s1;
	stack_t s2;
	stack_t s0;
	signal(SIGCKPT, on_ckpt);
	if (sigaltstack(NULL, &s0) == -1)
	{
		perror("s0");
		exit(0);
	}
	prt_stack("s0:", &s0);

	s1.ss_sp = (char *) &stack1[4096 - sizeof(long)];
	s1.ss_size = sizeof(stack1);
	s1.ss_flags = 0;

	if (sigaltstack(&s1, &s2) == -1)
	{
		perror("s1");
	}
	prt_stack("s1:", &s1);
	prt_stack("s2:", &s2);

	pause();

	if (cmp_stack("orig to after setting stack1:", &s0, &s2))
		succeeded = 0;
	s2.ss_sp = (char *) &stack2[4096 - sizeof(long)];
	s2.ss_size = sizeof(stack2);
	s2.ss_flags = 0;
	if (sigaltstack(&s2, &s0) == -1)
	{
		perror("s2");
	}
	prt_stack("s2:", &s2);
	prt_stack("s0:", &s0);

	if (cmp_stack("stack1 to after setting stack2:", &s1, &s0))
		succeeded = 0;
	if (!cmp_stack("s2,s0:", &s2, &s0))
		succeeded = 0;
	if (succeeded)
		kill(ctest_pid, SIGUSR1);
	else
		kill(ctest_pid, SIGUSR2);
	printf("\n\tsigstk: Done\n");
}

