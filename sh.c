#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/parser.h"
#include "user/commands.h"

int main(void) {
  static char buf[BUFSIZE];
  char *tokens[BUFSIZE / 2];
  struct command commands[MAXCMDS];
  int ntokens;
  int ncommands;

  while (1) {
    printf("nsh> ");
    memset(buf, 0, sizeof(buf));
    gets(buf, sizeof(buf));

    if (buf[0] == '\0') {
      exit(0);
    }

    ntokens = tokenize(buf, tokens, BUFSIZE / 2);
    if (ntokens == 0) {
      continue;
    }

    ncommands = parse_pipeline(tokens, ntokens, commands, MAXCMDS);
    if (ncommands < 0){
      continue;
    }

    if (ncommands == 1 && strcmp(commands[0].argv[0], "exit") == 0) {
      exit(0);
    }

    execute_pipeline(commands, ncommands);
  }

  exit(0);
}