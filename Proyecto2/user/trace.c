#include "kernel/types.h"
#include "user/user.h"



static void
usage(void)
{
  fprintf(2, "usage: trace <syscall> <cmd> [args...]\n");
  fprintf(2, "  <syscall>: name with or without sys_ prefix (ej: kill, sys_kill)\n");
  fprintf(2, "  example: trace sys_kill tracetest\n");
}

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(2, "trace: missing args\n");
    usage();
    exit(1);
  }

  if (trace(argv[1]) < 0) {
    fprintf(2, "trace: '%s' its not a valid syscall\n", argv[1]);
    usage();
    exit(1);
  }

  exec(argv[2], &argv[2]);

  // exec returns if fail
  fprintf(2, "trace: could not execute '%s'\n", argv[2]);
  exit(1);
}