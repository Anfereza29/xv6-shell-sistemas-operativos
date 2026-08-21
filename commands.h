#ifndef COMMANDS_H
#define COMMANDS_H

#include "user/parser.h"

void apply_redirections(struct command *cmd);
void execute_pipeline(struct command commands[], int ncommands);

#endif