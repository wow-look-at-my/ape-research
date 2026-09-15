#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
int main(int argc, char **argv) {
	char *av[] = { argv[1], NULL };
	char *ev[] = { NULL };
	execve(argv[1], av, ev);
	/* only reached if execve failed */
	printf("execve FAILED errno=%d (%s)\n", errno, strerror(errno));
	return 1;
}
