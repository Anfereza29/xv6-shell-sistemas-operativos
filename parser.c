#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/parser.h"

int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int tokenize(char *line, char *tokens[], int max) {
  int count = 0;
  char *p = line;

  while (*p != '\0' && count < max) {

    while (*p != '\0' && is_space(*p))
      p++;

    if (*p == '\0')
      break;

    tokens[count] = p;
    count++;

    while (*p != '\0' && !is_space(*p))
      p++;

    if (*p != '\0') {
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

  while (i < ntokens) {

    if (strcmp(tokens[i], "<") == 0) {

      if (i + 1 >= ntokens) {
        fprintf(2, "nsh: syntax error: '<' needs a file name\n");
        return -1;
      }

      if (cmd->infile != 0) {
        fprintf(2, "nsh: multiple input redirections\n");
        return -1;
      }

      cmd->infile = tokens[i + 1];
      i += 2;
    }

    else if (strcmp(tokens[i], ">") == 0) {

      if (i + 1 >= ntokens) {
        fprintf(2, "nsh: syntax error: '>' needs a file name\n");
        return -1;
      }

      if (cmd->outfile != 0) {
        fprintf(2, "nsh: multiple output redirections\n");
        return -1;
      }

      cmd->outfile = tokens[i + 1];
      i += 2;
    }

    else {

      if (argc >= MAXARGS - 1) {
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

int parse_pipeline(char *tokens[], int ntokens, struct command commands[], int maxcmds) {
  int start = 0;
  int i;
  int ncommands = 0;

  for (i = 0; i <= ntokens; i++) {

    if (i == ntokens || strcmp(tokens[i], "|") == 0) {

      if (i == start) {
        fprintf(2, "nsh: syntax error near pipe\n");
        return -1;
      }

      if (ncommands >= maxcmds) {
        fprintf(2, "nsh: too many commands in pipeline\n");
        return -1;
      }

      if (parse_command(&tokens[start], i - start, &commands[ncommands]) < 0) {
        return -1;
      }

      if (commands[ncommands].argv[0] == 0) {
        fprintf(2, "nsh: missing command\n");
        return -1;
      }

      ncommands++;

      start = i + 1;
    }
  }

  return ncommands;
}
