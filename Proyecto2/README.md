# Proyecto 2 — System Calls en xv6 (`trace` y `sysinfo`)

**Curso:** Sistemas Operativos 2026-2 — Universidad EAFIT
**Docente:** José Luis Montoya Pareja

## Integrantes

- Juan Felipe Gallón Maldonado
- Laura Santamaría Espinosa
- Andrés Felipe Rengifo

## Video

_[Enlace al video](https://youtu.be/FLnmGCwsJKg)_

---

## 1. ¿Qué hicimos?

Agregamos dos llamadas al sistema (syscalls) nuevas al sistema operativo xv6:

- **`trace`**: permite vigilar una syscall específica, indicándola por su nombre (por ejemplo `kill` o `sys_kill`). Cada vez que el programa la usa, el kernel muestra qué proceso la llamó, qué valor devolvió y el contenido de los registros del procesador en ese momento.
- **`sysinfo`**: le pide al kernel un resumen del estado del sistema: cuánta memoria hay libre, cuántas páginas de memoria están usadas y disponibles, y cuántos procesos están esperando para ejecutarse (RUNNABLE) o ejecutándose (RUNNING).

También creamos tres programas para usarlas y probarlas:

| Programa | Para qué sirve |
|---|---|
| `trace` | Activa el monitoreo y ejecuta el comando indicado. Uso: `trace <syscall> <comando>`. |
| `sysinfo` | Muestra el estado del sistema. Con `sysinfo -t` ejecuta pruebas automáticas. |
| `tracetest` | Programa de apoyo que llama a `kill` con resultados conocidos (un caso exitoso y uno con error), para comprobar lo que muestra `trace`. |

---

## 2. Archivos modificados

Partimos de xv6-riscv en el commit [`9e3161a`](https://github.com/mit-pdos/xv6-riscv/commit/9e3161a9abf5f51ea402562d1874caf6c4926597). Este repositorio contiene solo los archivos que modificamos o creamos.

| Archivo | Qué cambiamos |
|---|---|
| `kernel/syscall.h` | Números de las nuevas syscalls: `trace` (23) y `sysinfo` (24). |
| `kernel/syscall.c` | Registramos las syscalls nuevas; agregamos la lista de nombres de syscalls, la búsqueda por nombre y la impresión de lo que se monitorea. |
| `kernel/sysproc.c` | Implementación de `sys_trace` y `sys_sysinfo`. |
| `kernel/sysinfo.h` | **Nuevo.** Estructura con los datos que devuelve `sysinfo`. |
| `kernel/proc.h` | Campo nuevo en cada proceso para saber qué syscall está vigilando. |
| `kernel/proc.c` | Ese campo se reinicia al crear un proceso y se copia a los hijos; función que cuenta los procesos por estado. |
| `kernel/kalloc.c` | Funciones que cuentan las páginas de memoria libres y totales. |
| `kernel/defs.h` | Declaraciones de las funciones nuevas del kernel. |
| `user/trace.c` | **Nuevo.** Programa `trace`. |
| `user/sysinfo.c` | **Nuevo.** Programa `sysinfo` con sus pruebas. |
| `user/tracetest.c` | **Nuevo.** Programa de apoyo para probar `trace`. |
| `user/user.h` | Declaraciones de `trace()` y `sysinfo()` para los programas. |
| `user/usys.pl` | Genera el código que conecta los programas con las syscalls nuevas. |
| `Makefile` | Agrega los tres programas nuevos al sistema. |

> El `Makefile` está en la raíz del repositorio porque así está ubicado en xv6.

---

## 3. Diseño

### 3.1 Cómo funciona una syscall en xv6

Cuando un programa llama, por ejemplo, a `trace("kill")`, se ejecuta un pequeño fragmento de código que guarda el número de la syscall en un registro del procesador y ejecuta la instrucción `ecall`. Esa instrucción pasa el control al kernel, que guarda los registros del programa, busca la syscall por su número en una tabla y la ejecuta. El resultado vuelve al programa como valor de retorno.

### 3.2 `trace`

- **Qué syscall vigilar se guarda en cada proceso**, no de forma global, para que el monitoreo afecte solo al programa indicado.
- **El nombre se busca en el kernel.** El programa envía el texto (`"kill"`) y el kernel lo copia de forma segura desde la memoria del programa y lo busca en una lista de nombres. Si no existe, devuelve un error.
- **La interceptación se hace en un solo lugar:** la función del kernel por la que pasan todas las syscalls. Así no hubo que modificar cada syscall por separado.
- **Los argumentos se guardan antes de ejecutar la syscall**, porque al terminar, el registro del primer argumento se reemplaza con el valor de retorno.
- **`exit` es un caso especial:** nunca regresa, así que se muestra antes de ejecutarse y se marca con `(no return)`.
- **Los hijos heredan el monitoreo:** al crear un proceso hijo, se le copia el valor del padre, así que también se vigila lo que hacen los procesos que crea el comando.

### 3.3 `sysinfo`

- **Memoria:** xv6 guarda las páginas libres (bloques de 4 KB) en una lista. Para contarlas, recorremos esa lista. Preferimos esto a mantener un contador porque así no modificamos el código que reserva y libera memoria, que usa todo el sistema. El total de páginas se calcula a partir de dónde termina el kernel y dónde termina la memoria.
- **Procesos:** recorremos la tabla de procesos contando los que están RUNNABLE y RUNNING. Incluimos RUNNING porque el propio `sysinfo` siempre está en ese estado mientras se ejecuta; sin él, casi siempre veríamos 0 procesos y parecería un error.
- **Protección de datos compartidos:** varias CPUs trabajan al mismo tiempo, así que al contar tomamos los mismos candados (locks) que usa xv6, para no leer datos a medio modificar.
- **Envío de los datos al programa:** el kernel llena una estructura y la copia a la memoria del programa con `copyout`, que verifica que la dirección sea válida. Si el programa pasa una dirección inválida, la syscall devuelve un error sin escribir nada.
- **La estructura está en un solo archivo** (`kernel/sysinfo.h`) que usan tanto el kernel como el programa, para que ambos la interpreten igual.

### 3.4 Manejo de errores

Todos los errores muestran un mensaje en la salida de errores (stderr) y terminan el programa con un código distinto de cero: faltan argumentos, el nombre de la syscall no existe, el comando no se puede ejecutar, la opción es inválida o falla alguna prueba.

---

## 4. Compilación

Se necesita Linux con **Ubuntu 24.04** (en Windows, WSL2 con Ubuntu 24.04), porque xv6 exige QEMU 7.2 o superior.

```bash
sudo apt update
sudo apt install git build-essential bc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu qemu-system-misc
```

Luego:

```bash
# 1. Clonar este repositorio
git clone <URL-de-este-repositorio> proyecto2-entrega

# 2. Clonar xv6 en la versión sobre la que trabajamos
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
git checkout 9e3161a9abf5f51ea402562d1874caf6c4926597

# 3. Copiar nuestros archivos sobre xv6
cp ../proyecto2-entrega/Makefile .
cp ../proyecto2-entrega/kernel/* kernel/
cp ../proyecto2-entrega/user/* user/

# 4. Compilar
make
```

El paso `git checkout` es necesario: nuestros archivos son copias completas de la versión modificada, y sobre una versión más nueva de xv6 podrían no compilar.

## 5. Ejecución

```bash
make qemu        # para salir: Ctrl-a y luego x
```

Dentro de xv6:

```text
$ trace sys_kill tracetest     # vigila kill: un caso exitoso y uno con error
$ trace kill kill 99           # vigila kill usando el programa kill de xv6
$ trace sys_exit echo hola     # caso especial de exit
$ trace open cat README        # vigila open: devuelve el número del archivo abierto
$ sysinfo                      # estado actual del sistema
$ sysinfo -t                   # pruebas automáticas
$ usertests -q                 # pruebas oficiales de xv6
```

---

## 6. Resultados

### `trace sys_kill tracetest`

```text
tracetest: I'M PID 3

[trace] PID: 3 (tracetest)  SYSCALL: sys_kill (#6)  RETURN: 0
  a0: 0x4  a1: 0x3edf  a2: 0x1
  a3: 0x3ea9  a4: 0x16  a5: 0x966  a7: 6
  s0: 0x3fd0  s1: 0x4
  sp: 0x3fb0  ra: 0x52  epc: 0x3a0
tracetest: kill(4) return 0 (awaited 0)

[trace] PID: 3 (tracetest)  SYSCALL: sys_kill (#6)  RETURN: -1
  a0: 0x270f  a1: 0x3edf  a2: 0x1
  a3: 0x3ea9  a4: 0x2a  a5: 0x9b2  a7: 6
  s0: 0x3fd0  s1: 0x4
  sp: 0x3fb0  ra: 0x72  epc: 0x3a0
tracetest: kill(9999) return -1 (awaited -1)
tracetest: open("no_existe.txt") return -1 (awaited -1)
```

Los valores coinciden con lo que hace el programa: `a0` es el argumento de `kill` (el PID 4 del hijo, y luego 9999, que en hexadecimal es `0x270f`), y `RETURN` es lo mismo que imprime `tracetest`. La llamada a `open` no aparece porque solo se vigila `kill`.

### `sysinfo`

```text
Free Memory:        127 MB (130136 KB)
Used Pages:         201
Available Pages:    32534
Total Pages:        32735
Runnable Processes: 0
Running Processes:  1
```

El total de páginas por 4 KB da unos 128 MB, que es la memoria de xv6. El único proceso en ejecución es el propio `sysinfo`.

### `sysinfo -t`

```text
Test 1: data consistency
  [PASS] freepages + usedpages == totalpages
  [PASS] freemem == freepages * 4096
  [PASS] at least 1 process running
Test 2: memory (sbrk of 64 pages)
  free pages: before=32534  with sbrk=32470  after releasing=32534
  [PASS] sbrk reduces free pages in at least n
  [PASS] sbrk increases used pages in at least n
  [PASS] neagtive sbrk returns at least n pages
Test 3: RUNNABLE procs (6 childs in loop)
  RUNNABLE: before=0  with children=4  after=0
  RUNNING:  before=1  with children=3  after=1
  [PASS] childs + parent are RUNNABLE or RUNNING
  [PASS] there are RUNNABLE procs waiting CPU
Test 4: invalid pointers
  [PASS] addr 0 (Only Reading text) -> -1
  [PASS] kernel addr (KERNBASE) -> -1
  [PASS] addr outside MAXVA -> -1
sysinfo: all tests succeed
```

Qué comprueba cada prueba:

1. Los datos son coherentes entre sí.
2. Al reservar 64 páginas de memoria, las libres bajan exactamente 64; al liberarlas, vuelven.
3. Al crear 6 procesos que no paran de trabajar, aparecen procesos RUNNABLE esperando su turno (con 3 CPUs, 3 se ejecutan y 4 esperan).
4. Si se le pasa al kernel una dirección de memoria inválida, la rechaza.

### Casos de error

Probamos `trace` sin argumentos, `trace kil echo x` (nombre inexistente), `trace kill noexiste` (comando inexistente) y `sysinfo -x` (opción inválida). Todos muestran un mensaje de error y terminan con código distinto de cero.

---

## 7. Uso de IA

En el desarrollo de este proyecto se utilizó **Claude (Anthropic)** como asistente de IA generativa. Se usó para analizar el diseño que se podía proponer y guiar la implementación por fases: estudio del recorrido de una syscall existente, integración mínima de las nuevas syscalls, implementación de `trace`, implementación de `sysinfo` y diseño de las pruebas, adicionalmente fue usada para la corrección de código. También se usó para redactar este README. El equipo desarrolló y ejecutó los cambios en cada archivo siguiendo esa guía, compiló y ejecutó cada fase en QEMU, y verificó los resultados; las salidas que aparecen en este documento provienen de esas ejecuciones. Cada integrante comprende la implementación.

