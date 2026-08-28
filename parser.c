#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/parser.h"

// Indica si un carácter es un espacio o un salto de línea.
int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Divide la línea ingresada por el usuario en palabras llamadas tokens.
int tokenize(char *line, char *tokens[], int max) {
  // Cuenta cuántos tokens se han encontrado.
  int count = 0;

  // p recorre la línea carácter por carácter.
  char *p = line;

  // Continúa hasta el final de la línea o hasta alcanzar el límite.
  while (*p != '\0' && count < max) {

    // Ignora los espacios que aparecen antes de una palabra.
    while (*p != '\0' && is_space(*p))
      p++;

    // Si se llegó al final, no quedan más tokens.
    if (*p == '\0')
      break;

    // Guarda la dirección donde comienza el token.
    tokens[count] = p;
    count++;

    // Avanza hasta encontrar el final de la palabra.
    while (*p != '\0' && !is_space(*p))
      p++;

    if (*p != '\0') {
      // Reemplaza el espacio por \0 para marcar el final del token.
      *p = '\0';
      p++;
    }
  }

  // Devuelve la cantidad de tokens encontrados.
  return count;
}

// Convierte un grupo de tokens en una estructura command.
int parse_command(char *tokens[], int ntokens, struct command *cmd) {
  // argc cuenta el nombre del programa y sus argumentos.
  int argc = 0;

  // i indica el token que se está analizando.
  int i = 0;

  // Inicialmente el comando no tiene redirecciones.
  cmd->infile = 0;
  cmd->outfile = 0;

  while (i < ntokens) {

    // El operador < indica una redirección de entrada.
    if (strcmp(tokens[i], "<") == 0) {

      // Después de < debe aparecer el nombre de un archivo.
      if (i + 1 >= ntokens) {
        fprintf(2, "nsh: syntax error: '<' needs a file name\n");
        return -1;
      }

      // No se permiten dos redirecciones de entrada en un comando.
      if (cmd->infile != 0) {
        fprintf(2, "nsh: multiple input redirections\n");
        return -1;
      }

      // Guarda el nombre del archivo de entrada.
      cmd->infile = tokens[i + 1];

      // Avanza dos posiciones: el operador y el archivo.
      i += 2;
    }

    // El operador > indica una redirección de salida.
    else if (strcmp(tokens[i], ">") == 0) {

      // Después de > debe aparecer el nombre de un archivo.
      if (i + 1 >= ntokens) {
        fprintf(2, "nsh: syntax error: '>' needs a file name\n");
        return -1;
      }

      // No se permiten dos redirecciones de salida en un comando.
      if (cmd->outfile != 0) {
        fprintf(2, "nsh: multiple output redirections\n");
        return -1;
      }

      // Guarda el nombre del archivo de salida.
      cmd->outfile = tokens[i + 1];

      // Avanza dos posiciones: el operador y el archivo.
      i += 2;
    }

    // Si no es una redirección, el token es un comando o argumento.
    else {

      // Se reserva una posición final para el valor 0.
      if (argc >= MAXARGS - 1) {
        fprintf(2, "nsh: too many arguments\n");
        return -1;
      }

      // Agrega el token al arreglo de argumentos.
      cmd->argv[argc] = tokens[i];
      argc++;
      i++;
    }
  }

  // exec necesita que el arreglo de argumentos termine en 0.
  cmd->argv[argc] = 0;

  // Devuelve la cantidad de elementos almacenados en argv.
  return argc;
}

// Separa una línea en varios comandos usando el operador |.
int parse_pipeline(char *tokens[], int ntokens, struct command commands[], int maxcmds) {
  // start marca dónde comienza el comando actual.
  int start = 0;
  int i;

  // Cuenta cuántos comandos forman la tubería.
  int ncommands = 0;

  // Se usa <= para procesar también el último comando.
  for (i = 0; i <= ntokens; i++) {

    // Se encontró un pipe o se llegó al final de los tokens.
    if (i == ntokens || strcmp(tokens[i], "|") == 0) {

      // Detecta pipes sin un comando a uno de sus lados.
      if (i == start) {
        fprintf(2, "nsh: syntax error near pipe\n");
        return -1;
      }

      // Evita superar el tamaño del arreglo de comandos.
      if (ncommands >= maxcmds) {
        fprintf(2, "nsh: too many commands in pipeline\n");
        return -1;
      }

      // Analiza solamente los tokens que pertenecen al comando actual.
      if (parse_command(&tokens[start], i - start, &commands[ncommands]) < 0) {
        return -1;
      }

      // Comprueba que exista un programa para ejecutar.
      if (commands[ncommands].argv[0] == 0) {
        fprintf(2, "nsh: missing command\n");
        return -1;
      }

      // El comando actual fue procesado correctamente.
      ncommands++;

      // El siguiente comando comienza después del operador |.
      start = i + 1;
    }
  }

  // Devuelve la cantidad total de comandos encontrados.
  return ncommands;
}
