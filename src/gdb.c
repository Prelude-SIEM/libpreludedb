#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "gdb.h"

static char *gdb_argv0;


static void gdb_run(int signal)
{
	char pidbuf[20];
	int status;
	
	snprintf(pidbuf, 20, "%d", getpid());
	if (fork() == 0) {
		printf("%d: received signal %d, running gdb %s %s\n...", getpid(), 
			signal, gdb_argv0, pidbuf);
		execlp("/usr/bin/gdb", "gdb", gdb_argv0, pidbuf, NULL);
	} else {
		close(STDIN_FILENO);
		printf("%d: waiting for gdb to finish\n", getpid());
		wait(&status);
		printf("%d: original process exiting\n", getpid());
		exit(1);
	}
}


void gdb_setup(int argc, char **argv)
{
	gdb_argv0 = strdup(argv[0]);
	signal(SIGSEGV, gdb_run);
	printf("gdb SIGSEGV handler registered!\n");
}
