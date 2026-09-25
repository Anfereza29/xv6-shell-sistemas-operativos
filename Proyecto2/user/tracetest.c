#include "kernel/types.h"
#include "user/user.h"

// Usage: trace sys_kill tracetest

int
main(void)
{
  int pid, r;

  printf("tracetest: I'M PID %d\n", getpid());

  pid = fork();
  if (pid < 0) {
    fprintf(2, "tracetest: fork fallo\n");
    exit(1);
  }
  if (pid == 0) {
    // put son to sleep until kill
    for (;;)
      pause(10);
  }

  pause(2); // give time to run son

  r = kill(pid);
  printf("tracetest: kill(%d) return %d (esperado 0)\n", pid, r);
  wait(0);

  r = kill(9999);
  printf("tracetest: kill(9999) return %d (esperado -1)\n", r);

  r = open("no_existe.txt", 0);
  printf("tracetest: open(\"no_existe.txt\") return %d (esperado -1)\n", r);

  exit(0);
}