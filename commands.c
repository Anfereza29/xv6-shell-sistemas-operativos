#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "user/commands.h"

void apply_redirections(struct command *cmd) {
  if (cmd->infile != 0) {
    close(0);

    if (open(cmd->infile, O_RDONLY) < 0) {
      fprintf(2, "nsh: cannot open %s\n", cmd->infile);
      exit(1);
    }
  }

  if (cmd->outfile != 0) {
    close(1);

    if (open(cmd->outfile, O_WRONLY | O_CREATE | O_TRUNC) < 0) {
      fprintf(2, "nsh: cannot create %s\n", cmd->outfile);
      exit(1);
    }
  }
}

void execute_pipeline(struct command commands[], int ncommands) {
  int i;
  int fd[2];

  int prev_read = -1;

  for (i = 0; i < ncommands; i++) {
    if (i < ncommands - 1) {

      if (pipe(fd) < 0) {
        fprintf(2, "nsh: pipe failed\n");

        if (prev_read >= 0) {
        close(prev_read);
        }

        return;
      }
    }

    int pid = fork();

    if (pid < 0) {
      fprintf(2, "nsh: fork failed\n");

      if (prev_read >= 0)
        close(prev_read);

      if (i < ncommands - 1) {
        close(fd[0]);
        close(fd[1]);
      }

      return;
    }

    if (pid == 0) {
      if (prev_read >= 0) {
        close(0);
        dup(prev_read);
        close(prev_read);
      }

      if (i < ncommands - 1) {
        close(1);
        dup(fd[1]);

        close(fd[0]);
        close(fd[1]);
      }

      apply_redirections(&commands[i]);

      exec(commands[i].argv[0], commands[i].argv);

      fprintf(2, "nsh: cannot run %s\n", commands[i].argv[0]);

      exit(1);
    }

    if (prev_read >= 0) {
      close(prev_read);
    }

    if (i < ncommands - 1) {
      close(fd[1]);

      prev_read = fd[0];
    } else {
      prev_read = -1;
    }
  }

  for (i = 0; i < ncommands; i++) {
    wait(0);
  }
}