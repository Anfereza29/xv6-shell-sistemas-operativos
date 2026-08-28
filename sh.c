#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/parser.h"
#include "user/commands.h"

// Función principal y punto de inicio del shell.
int main(void) {
  // Almacena la línea escrita por el usuario.
  // static permite reservar el arreglo una sola vez.
  static char buf[BUFSIZE];

  // Guarda las palabras y operadores encontrados en la línea.
  char *tokens[BUFSIZE / 2];

  // Guarda los comandos que forman la tubería.
  struct command commands[MAXCMDS];

  // Cantidad de tokens encontrados.
  int ntokens;

  // Cantidad de comandos encontrados.
  int ncommands;

  // Mantiene el shell funcionando continuamente.
  while (1) {
    // Muestra el prompt para indicar que espera un comando.
    printf("nsh> ");

    // Limpia el búfer antes de leer una nueva línea.
    memset(buf, 0, sizeof(buf));

    // Lee lo escrito por el usuario desde la entrada estándar.
    gets(buf, sizeof(buf));

    // Un búfer vacío indica que se cerró la entrada estándar.
    if (buf[0] == '\0') {
      exit(0);
    }

    // Divide la línea en palabras y operadores.
    ntokens = tokenize(buf, tokens, BUFSIZE / 2);

    // Si la línea no contiene tokens, vuelve a mostrar el prompt.
    if (ntokens == 0) {
      continue;
    }

    // Convierte los tokens en uno o varios comandos.
    ncommands = parse_pipeline(tokens, ntokens, commands, MAXCMDS);

    // Un valor negativo indica que ocurrió un error de sintaxis.
    if (ncommands < 0){
      continue;
    }

    // exit es un comando interno y finaliza directamente el shell.
    if (ncommands == 1 && strcmp(commands[0].argv[0], "exit") == 0) {
      exit(0);
    }

    // Ejecuta los comandos y crea las tuberías necesarias.
    execute_pipeline(commands, ncommands);
  }

  // Finaliza el proceso del shell.
  exit(0);
}
