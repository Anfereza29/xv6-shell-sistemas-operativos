#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#define MAXARGS 16
#define BUFSIZE 128

struct command {
  char *argv[MAXARGS];
  char *infile;
  char *outfile;
};

int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int tokenize(char *line, char *tokens[], int max) {
  int count = 0;
  char *p = line;

  while(*p != '\0' && count < max){
    while(*p != '\0' && is_space(*p))
      p++;

    if(*p == '\0')
      break;

    tokens[count] = p;
    count++;

    while(*p != '\0' && !is_space(*p))
      p++;

    if(*p != '\0'){
      *p = '\0';
      p++;
    }
  }

  return count;
}

int parse_command(char *tokens[], int ntokens, struct command *cmd) {
  int argc = 0;
  int i = 0;

  cmd->infile = 0;
  cmd->outfile = 0;

  while(i < ntokens){
    if(strcmp(tokens[i], "<") == 0){
      if(i + 1 >= ntokens){
        fprintf(2, "nsh: syntax error: '<' needs a file name\n");
        return -1;
      }
      cmd->infile = tokens[i + 1];
      i = i + 2;
    } else if(strcmp(tokens[i], ">") == 0){
      if(i + 1 >= ntokens){
        fprintf(2, "nsh: syntax error: '>' needs a file name\n");
        return -1;
      }
      cmd->outfile = tokens[i + 1];
      i = i + 2;
    } else {
      if(argc >= MAXARGS - 1){
        fprintf(2, "nsh: too many arguments\n");
        return -1;
      }
      cmd->argv[argc] = tokens[i];
      argc++;
      i++;
    }
  }

  cmd->argv[argc] = 0;
  return argc;
}

void apply_redirections(struct command *cmd) {
  if(cmd->infile != 0){
    close(0);
    if(open(cmd->infile, O_RDONLY) < 0){
      fprintf(2, "nsh: cannot open %s\n", cmd->infile);
      exit(1);
    }
  }

  if(cmd->outfile != 0){
    close(1);
    if(open(cmd->outfile, O_WRONLY | O_CREATE | O_TRUNC) < 0){
      fprintf(2, "nsh: cannot create %s\n", cmd->outfile);
      exit(1);
    }
  }
}

int main(void) {
  static char buf[BUFSIZE];
  char *tokens[MAXARGS];
  struct command cmd;
  int ntokens, argc, pid;

  while(1){
    printf("nsh> ");

    memset(buf, 0, sizeof(buf));
    gets(buf, sizeof(buf));

    if(buf[0] == '\0')
      break;

    ntokens = tokenize(buf, tokens, MAXARGS);

    if(ntokens == 0)
      continue;

    argc = parse_command(tokens, ntokens, &cmd);

    if(argc < 0)
      continue;

    if(argc == 0){
      fprintf(2, "nsh: missing command\n");
      continue;
    }

    if(strcmp(cmd.argv[0], "exit") == 0)
      exit(0);

    pid = fork();

    if(pid < 0){
      fprintf(2, "nsh: fork failed\n");
    } else if(pid == 0){
      apply_redirections(&cmd);
      exec(cmd.argv[0], cmd.argv);
      fprintf(2, "nsh: cannot run %s\n", cmd.argv[0]);
      exit(1);
    } else {
      wait(0);
    }
  }

  exit(0);
}