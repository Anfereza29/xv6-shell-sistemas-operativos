#ifndef PARSER_H
#define PARSER_H

#define MAXARGS 16
#define MAXCMDS 8
#define BUFSIZE 128

struct command {
  char *argv[MAXARGS];
  char *infile;
  char *outfile;
};

int is_space(char c);
int tokenize(char *line, char *tokens[], int max);
int parse_command(char *tokens[], int ntokens, struct command *cmd);
int parse_pipeline(char *tokens[], int ntokens, struct command commands[], int maxcmds);

#endif