#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "user/commands.h"

// Aplica las redirecciones de entrada y salida de un comando.
void apply_redirections(struct command *cmd) {
  // infile es diferente de 0 cuando se utilizó el operador <.
  if (cmd->infile != 0) {
    // Cierra la entrada estándar, cuyo descriptor es 0.
    close(0);

    // Abre el archivo en modo lectura y ocupa el descriptor 0.
    if (open(cmd->infile, O_RDONLY) < 0) {
      // El descriptor 2 corresponde a la salida de errores.
      fprintf(2, "nsh: cannot open %s\n", cmd->infile);
      exit(1);
    }
  }

  // outfile es diferente de 0 cuando se utilizó el operador >.
  if (cmd->outfile != 0) {
    // Cierra la salida estándar, cuyo descriptor es 1.
    close(1);

    // Abre o crea el archivo para usarlo como nueva salida estándar.
    // O_TRUNC borra el contenido anterior si el archivo ya existía.
    if (open(cmd->outfile, O_WRONLY | O_CREATE | O_TRUNC) < 0) {
      fprintf(2, "nsh: cannot create %s\n", cmd->outfile);
      exit(1);
    }
  }
}

// Ejecuta todos los comandos y los conecta mediante pipes.
void execute_pipeline(struct command commands[], int ncommands) {
  int i;

  // fd[0] es el extremo de lectura y fd[1] el de escritura.
  int fd[2];

  // Guarda la lectura del pipe anterior. -1 indica que todavía no existe.
  int prev_read = -1;

  // Se crea un proceso hijo para cada comando.
  for (i = 0; i < ncommands; i++) {
    // El último comando no necesita crear un pipe nuevo.
    if (i < ncommands - 1) {

      // Crea el canal que conectará este comando con el siguiente.
      if (pipe(fd) < 0) {
        fprintf(2, "nsh: pipe failed\n");

        // Cierra el descriptor anterior si estaba abierto.
        if (prev_read >= 0) {
        close(prev_read);
        }

        return;
      }
    }

    // fork crea una copia del proceso actual.
    int pid = fork();

    // Un valor negativo indica que no se pudo crear el proceso hijo.
    if (pid < 0) {
      fprintf(2, "nsh: fork failed\n");

      if (prev_read >= 0)
        close(prev_read);

      // Cierra ambos extremos del pipe que ya no se utilizará.
      if (i < ncommands - 1) {
        close(fd[0]);
        close(fd[1]);
      }

      return;
    }

    // pid es 0 únicamente dentro del proceso hijo.
    if (pid == 0) {
      // Si existe un pipe anterior, se usa como entrada estándar.
      if (prev_read >= 0) {
        close(0);
        dup(prev_read);
        close(prev_read);
      }

      // Si no es el último comando, su salida se envía al siguiente pipe.
      if (i < ncommands - 1) {
        close(1);
        dup(fd[1]);

        // El hijo cierra los extremos que ya no necesita.
        close(fd[0]);
        close(fd[1]);
      }

      // Aplica posibles operadores < o > antes de ejecutar el comando.
      apply_redirections(&commands[i]);

      // Reemplaza el proceso hijo por el programa solicitado.
      exec(commands[i].argv[0], commands[i].argv);

      // exec solo continúa hasta aquí cuando ocurre un error.
      fprintf(2, "nsh: cannot run %s\n", commands[i].argv[0]);

      exit(1);
    }

    // A partir de aquí continúa el proceso padre.
    if (prev_read >= 0) {
      close(prev_read);
    }

    if (i < ncommands - 1) {
      // El padre no escribe en el pipe.
      close(fd[1]);

      // Conserva la lectura para conectarla con el próximo comando.
      prev_read = fd[0];
    } else {
      // No quedan más comandos por conectar.
      prev_read = -1;
    }
  }

  // Espera a que terminen todos los procesos hijos creados.
  for (i = 0; i < ncommands; i++) {
    wait(0);
  }
}
