# Proyecto de Sistemas Operativos
## Implementación de un Shell para xv6
### Integrantes
- Andres Rengifo Zapata
- Juan Felipe Gallón Maldonado
- Laura Santamaria Espinosa

### Curso
Sistemas Operativos

### Semestre
2026-2
---

# Descripción
Este proyecto implementa un shell para el sistema operativo xv6. El programa
permite interpretar comandos ingresados por el usuario desde la entrada
estándar y ejecutar los programas existentes en xv6 mediante la creación de
nuevos procesos.
La implementación soporta:
- Ejecución de comandos simples.
- Ejecución de comandos con argumentos.
- Redirección de entrada (`<`).
- Redirección de salida (`>`).
- Tuberías simples y múltiples (`|`).
- Comando interno `exit` para finalizar el shell.
La solución fue desarrollada utilizando lenguaje C y se integra al proceso de
compilación estándar de xv6 ejecutado sobre QEMU.

---

# Objetivo del proyecto

El objetivo principal es comprender, mediante una implementación práctica, cómo
un shell funciona como intermediario entre el usuario y el sistema operativo.
Para lograrlo, el programa recibe una línea de texto, identifica los comandos y
operadores escritos, crea los procesos necesarios y conecta su entrada y salida
según lo solicitado.

El proyecto permite aplicar conceptos fundamentales de sistemas operativos,
entre ellos la creación y sincronización de procesos, el uso de llamadas al
sistema, la administración de descriptores de archivo y la comunicación entre
procesos por medio de tuberías.

El shell no implementa programas nuevos. Su función es ejecutar los programas
que ya se encuentran disponibles en la distribución de xv6.

---

# Estructura de archivos

La versión final del proyecto está organizada de la siguiente manera:

```text
README.md
commands.c
commands.h
parser.c
parser.h
sh.c
```

## Descripción de los archivos

### sh.c

Es el punto de entrada del shell y contiene su ciclo principal. Muestra el
prompt `nsh>`, lee la línea ingresada por el usuario, solicita al parser que la
analice y envía los comandos obtenidos al módulo de ejecución. También reconoce
el comando interno `exit` cuando se escribe como un único comando.

### parser.h

Define las constantes generales del proyecto, la estructura `command` y los
prototipos de las funciones del parser.

La estructura `command` guarda:

- `argv`: nombre del programa y sus argumentos.
- `infile`: archivo utilizado para una redirección de entrada.
- `outfile`: archivo utilizado para una redirección de salida.

Las constantes principales son:

- `BUFSIZE`: tamaño máximo de la línea de entrada, establecido en 128
  caracteres.
- `MAXARGS`: tamaño del arreglo de argumentos, establecido en 16 posiciones.
- `MAXCMDS`: cantidad máxima de comandos en una tubería, establecida en 8.

### parser.c

Contiene las funciones encargadas de analizar la línea escrita por el usuario:

- `is_space` reconoce espacios, tabulaciones y saltos de línea.
- `tokenize` divide la línea en tokens separados por espacios.
- `parse_command` separa el nombre del programa, sus argumentos y las
  redirecciones `<` y `>`.
- `parse_pipeline` divide la línea cada vez que encuentra `|` y construye un
  arreglo de comandos.

Este módulo también detecta errores como tuberías sin un comando a alguno de
sus lados, redirecciones sin nombre de archivo, redirecciones repetidas y una
cantidad de comandos o argumentos superior a los límites definidos.

### commands.h

Declara las funciones utilizadas para aplicar redirecciones y ejecutar uno o
varios comandos. Incluye `parser.h` porque estas funciones trabajan con la
estructura `command`.

### commands.c

Contiene la lógica relacionada con procesos, redirecciones y tuberías:

- `apply_redirections` conecta la entrada o la salida estándar con los archivos
  indicados por el usuario.
- `execute_pipeline` crea los pipes necesarios, genera un proceso hijo por cada
  comando, conecta los descriptores correspondientes, ejecuta los programas y
  espera a que todos los procesos terminen.

# Orden recomendado para revisar el código

Para comprender la implementación con mayor facilidad, se recomienda abrir los
archivos en este orden:

1. `parser.h`, para conocer las constantes, la estructura `command` y las
   funciones disponibles.
2. `parser.c`, para entender cómo una línea de texto se convierte en comandos,
   argumentos, redirecciones y tuberías.
3. `commands.h`, para identificar la interfaz del módulo de ejecución.
4. `commands.c`, para revisar cómo se crean los procesos y cómo se conectan sus
   entradas y salidas.
5. `sh.c`, para observar cómo todos los módulos se integran en el ciclo principal
   del shell.

Aunque este orden facilita la lectura, durante la ejecución el programa comienza
en `sh.c`.

---

# Funcionamiento general

El shell repite el siguiente proceso hasta que el usuario escribe `exit` o se
cierra la entrada estándar:

1. Muestra el prompt `nsh>`.
2. Lee una línea de hasta 128 caracteres con `gets`.
3. `tokenize` separa la línea en palabras y operadores.
4. `parse_pipeline` divide la entrada en uno o varios comandos usando el
   operador `|`.
5. `parse_command` construye la información de cada comando, incluyendo sus
   argumentos y posibles redirecciones.
6. `execute_pipeline` crea un proceso hijo para cada comando mediante `fork`.
7. Cada proceso configura sus descriptores de archivo y ejecuta el programa con
   `exec`.
8. El proceso padre cierra los descriptores que ya no necesita y utiliza `wait`
   para esperar la finalización de todos los hijos.

## Ejecución de un comando simple

Para una entrada como:

```text
echo hola mundo
```

el parser construye un único comando. Luego el shell crea un proceso hijo. El
proceso hijo ejecuta `echo` con los argumentos `hola` y `mundo`, mientras el
proceso padre espera su terminación antes de volver a mostrar el prompt.

## Redirección de entrada

En una instrucción como:

```text
wc < datos.txt
```

el proceso hijo cierra el descriptor `0`, correspondiente a la entrada
estándar, y abre `datos.txt` en modo lectura. xv6 asigna al archivo el descriptor
libre de menor número, por lo que pasa a ocupar el descriptor `0`. Desde ese
momento, `wc` lee el contenido del archivo en lugar de leer desde el teclado.

## Redirección de salida

En una instrucción como:

```text
echo hola > salida.txt
```

el proceso hijo cierra el descriptor `1`, correspondiente a la salida estándar,
y abre o crea `salida.txt` en modo escritura. El archivo se trunca si ya
existía. Como resultado, la salida de `echo` se almacena en el archivo en lugar
de aparecer en la consola.

## Tuberías simples y múltiples

Una tubería permite que la salida de un comando se convierta en la entrada del
siguiente. Por ejemplo:

```text
echo hola | wc
```

El shell crea un pipe con un extremo de lectura y otro de escritura. La salida
estándar de `echo` se conecta al extremo de escritura y la entrada estándar de
`wc` se conecta al extremo de lectura.

Para una tubería múltiple como:

```text
cat datos.txt | grep error | wc
```

se crea un proceso hijo por cada comando y un pipe entre cada par de comandos
consecutivos. La variable `prev_read` conserva temporalmente el extremo de
lectura del pipe anterior para conectarlo con el siguiente proceso. Cada proceso
cierra los extremos que no utiliza, lo que evita fugas de descriptores y permite
que los lectores reciban el fin de archivo correctamente.

Todos los procesos de la tubería se crean antes de que el padre comience a
esperarlos. De esta forma, los comandos pueden ejecutarse de manera concurrente
y transferir datos a través de los pipes.

## Comando interno exit

`exit` es procesado directamente por `sh.c` y no se ejecuta mediante `exec`.
Esto permite finalizar el proceso del shell de forma controlada.

---

# Llamadas al sistema utilizadas

- `fork`: crea un proceso hijo para ejecutar cada comando.
- `exec`: reemplaza el proceso hijo por el programa solicitado.
- `wait`: permite que el shell espere la terminación de sus procesos hijos.
- `pipe`: crea el canal de comunicación entre dos comandos.
- `dup`: copia un descriptor para conectar un pipe con la entrada o la salida
  estándar.
- `open`: abre los archivos utilizados en las redirecciones.
- `close`: libera descriptores y permite reemplazar la entrada o salida estándar.
- `exit`: termina el shell o un proceso hijo cuando ocurre un error.

---

# Integración con xv6

## Requisitos

Para compilar y ejecutar el proyecto se necesita:

- Un entorno compatible con las herramientas de xv6.
- El repositorio oficial de xv6 para RISC-V.
- QEMU con soporte para RISC-V.
- El compilador cruzado requerido por xv6.

En Windows se recomienda trabajar dentro de WSL, ya que xv6 utiliza herramientas
propias de un entorno Unix.

## Paso 1. Obtener xv6

```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
```

## Paso 2. Copiar los archivos

Desde la raíz de este repositorio, copiar los siguientes archivos dentro del
directorio `user/` de xv6:

```text
sh.c
parser.c
parser.h
commands.c
commands.h
```

El archivo `user/sh.c` incluido originalmente en xv6 debe reemplazarse por el
archivo `sh.c` de este proyecto. El README permanece en la raíz del repositorio
y no necesita copiarse dentro de xv6 para realizar la compilación.

## Paso 3. Modificar el Makefile

El programa `sh` ya forma parte normalmente de la lista `UPROGS` de xv6. Se debe
conservar la entrada `$U/_sh` y añadir los módulos `parser.o` y `commands.o` como
dependencias del ejecutable:

```makefile
$U/_sh: $U/parser.o $U/commands.o
```

Esta línea permite que el enlazador incluya las funciones implementadas fuera de
`sh.c`. Puede ubicarse junto a las demás reglas de programas de usuario del
Makefile.

## Paso 4. Compilar y abrir xv6

Desde la raíz del repositorio de xv6, ejecutar:

```bash
make clean
make qemu
```

`make clean` es recomendable después de reemplazar archivos o modificar las
dependencias, aunque no es necesario en todas las compilaciones.

## Paso 5. Ejecutar el shell

Como se reemplaza el shell estándar de xv6, después del arranque puede aparecer
directamente el prompt:

```text
nsh>
```

Si aparece el prompt estándar `$`, el shell se inicia escribiendo:

```text
sh
```

Para salir de QEMU se puede utilizar la combinación de teclas establecida por el
entorno de xv6, normalmente `Ctrl-a` seguida de `x`.

---

# Casos de prueba

Las siguientes pruebas cubren las funcionalidades principales del proyecto. Los
operadores `<`, `>` y `|` deben escribirse separados por espacios.

## 1. Ejecución simple

```text
ls
```

Resultado esperado: se muestran los archivos y programas disponibles en xv6.

## 2. Comando con argumentos

```text
echo hola mundo
```

Resultado esperado: se muestra `hola mundo` en la consola.

## 3. Redirección de salida

```text
echo hola mundo > salida.txt
cat salida.txt
```

Resultado esperado: el primer comando crea `salida.txt` y el segundo muestra su
contenido.

## 4. Redirección de entrada

```text
wc < salida.txt
```

Resultado esperado: `wc` procesa el contenido de `salida.txt`.

## 5. Tubería simple

```text
echo hola | wc
```

Resultado esperado: la salida de `echo` es procesada por `wc`.

## 6. Tubería múltiple

```text
cat salida.txt | grep hola | wc
```

Resultado esperado: `cat` envía el archivo a `grep`, y el resultado de `grep` es
procesado por `wc`.

## 7. Redirección combinada con una tubería

```text
cat < salida.txt | grep hola > resultado.txt
cat resultado.txt
```

Resultado esperado: la entrada se toma de `salida.txt`, atraviesa la tubería y
el resultado final se guarda en `resultado.txt`.

## 8. Error de sintaxis en una tubería

```text
ls | | wc
```

Resultado esperado: el shell informa un error de sintaxis y vuelve a mostrar el
prompt sin finalizar.

## 9. Redirección sin archivo

```text
echo hola >
```

Resultado esperado: el shell informa que `>` necesita un nombre de archivo.

## 10. Comando inexistente

```text
comando_inexistente
```

Resultado esperado: el proceso hijo informa que no puede ejecutar el comando y
el shell continúa funcionando.

## 11. Salida del shell

```text
exit
```

Resultado esperado: el shell termina de forma controlada.

---

# Decisiones de diseño

- Se separó el análisis de la entrada y la ejecución en módulos diferentes para
  facilitar la lectura, las pruebas y el mantenimiento del código.
- Se utilizó una estructura `command` para conservar juntos el programa, sus
  argumentos y sus redirecciones.
- Se crea un proceso independiente para cada etapa de una tubería, lo que
  permite que los programas se ejecuten concurrentemente.
- Los pipes se crean de forma progresiva. Solo se conserva el extremo de lectura
  que necesita el siguiente comando, reduciendo la cantidad de descriptores
  abiertos.
- Las redirecciones se aplican dentro del proceso hijo para no modificar la
  entrada ni la salida estándar del shell principal.
- El proceso padre espera a todos los hijos después de crear la tubería completa,
  evitando bloquear la construcción de una tubería múltiple.
- Los mensajes de error se escriben en el descriptor `2`, correspondiente a la
  salida de error estándar.

---

# Manejo de errores

La implementación comprueba y reporta los siguientes casos:

- Fallo al crear un proceso con `fork`.
- Fallo al crear una tubería con `pipe`.
- Programa inexistente o imposible de ejecutar con `exec`.
- Archivo de entrada inexistente o imposible de abrir.
- Archivo de salida imposible de crear.
- Operadores de redirección sin un nombre de archivo posterior.
- Más de una redirección de entrada o salida dentro del mismo comando.
- Tuberías vacías o con comandos faltantes.
- Exceso de argumentos o de comandos en una tubería.

Ante un error de análisis, el shell descarta la línea y solicita una nueva. Ante
un error dentro de un proceso hijo, ese hijo termina y el proceso principal
continúa disponible.

---

# Limitaciones conocidas

- Los operadores `<`, `>` y `|` deben estar separados por espacios. Por ejemplo,
  debe escribirse `echo hola > salida.txt` y no `echo hola>salida.txt`.
- No se soportan comillas simples ni dobles.
- No se soportan variables de entorno ni sustitución de variables.
- No se expanden comodines como `*` o `?`.
- No se soporta la ejecución en segundo plano con `&`.
- No se implementan control de trabajos ni manejo avanzado de señales.
- No se implementan comandos internos como `cd`, `history`, `jobs`, `fg`, `bg`
  o `kill`.
- No se soporta la redirección de salida acumulativa con `>>`.
- La entrada está limitada a 128 caracteres.
- Cada comando puede almacenar hasta 15 elementos en `argv`, incluyendo el
  nombre del programa, y cada tubería puede contener hasta 8 comandos.
- Solo pueden ejecutarse programas que existan dentro del sistema de archivos de
  xv6.

Estas limitaciones corresponden al alcance definido para el proyecto y no
afectan los requisitos funcionales obligatorios.

---

# Declaración de uso de IA

Durante el desarrollo del proyecto se utilizaron herramientas de inteligencia
artificial generativa como apoyo para comprensión de conceptos, revisión de
código y generación de ejemplos.

Los integrantes asumen plena responsabilidad académica sobre el contenido
entregado y están en capacidad de explicar y justificar la implementación.
