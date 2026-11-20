// minidb: base de datos de cuentas en memoria (Proyecto 4).
// Sistemas Operativos 2026-2, Universidad EAFIT.
//
// ESQUELETO entregado por el docente. Ya están hechos:
//   minidb consulta K     reporte por sucursal con K procesos trabajadores
//   minidb mapa           vmprint() de un hijo antes y después de escribir
// El grupo debe implementar (busquen "TODO"):
//   minidb snapshot M     guarda snap.db en un hijo mientras el padre
//                         aplica M transferencias (sección C.2 del enunciado)
//
// Estudien consulta(): es el modelo de multiprogramación del proyecto
// (varios hijos con fork(), un pipe por hijo, el padre combina).
//
// Group note: "minidb snapshot M" is implemented in the section marked
// Part C.2 below (guardar_snapshot(), snapshot() and three helpers).
// consulta() and mapa() are unchanged from the skeleton.
#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/cowstat.h"
#include "user/user.h"
#include "user/minidb.h"

static struct cuenta *tabla;

static void
morir(char *msg)
{
  printf("minidb: error: %s\n", msg);
  exit(1);
}

static void
cargar_tabla(void)
{
  if((tabla = reservar_tabla()) == 0)
    morir("no hay memoria para la tabla");
  for(int i = 0; i < NCUENTAS; i++)
    generar_cuenta(i, &tabla[i]);
}

static uint64
libres(void)
{
  struct cowstat st;
  if(cowstat(&st) < 0)
    morir("cowstat() falló");
  return st.freepages;
}

static void
agregar(struct resultado *r, int desde, int hasta, int con_firma)
{
  memset(r, 0, sizeof(*r));
  r->desde = desde;
  r->hasta = hasta;
  for(int i = desde; i < hasta; i++){
    struct cuenta *c = &tabla[i];
    struct agregado *a = &r->suc[c->sucursal];
    a->cuentas++;
    a->saldo_total += c->saldo;
    if(c->saldo > a->saldo_max)
      a->saldo_max = c->saldo;
    if(con_firma)
      a->firmas += firma(c);
  }
}

static int
leer_todo(int fd, void *buf, int n)
{
  int total = 0;
  while(total < n){
    int r = read(fd, (char *)buf + total, n - total);
    if(r <= 0)
      return -1;
    total += r;
  }
  return total;
}

static int
escribir_todo(int fd, void *buf, int n)
{
  int total = 0;
  while(total < n){
    int r = write(fd, (char *)buf + total, n - total);
    if(r <= 0)
      return -1;
    total += r;
  }
  return total;
}

static void
consulta(int k)
{
  if(k < 1 || k > MAXWORKERS)
    morir("K debe estar entre 1 y 8");
  cargar_tabla();
  printf("consulta: trabajadores=%d\n", k);

  int fds[MAXWORKERS][2];
  uint64 libres0 = libres();
  int t0 = uptime();
  for(int w = 0; w < k; w++){
    if(pipe(fds[w]) < 0)
      morir("pipe() falló");
    int pid = fork();
    if(pid < 0)
      morir("fork() falló");
    if(pid == 0){
      close(fds[w][0]);
      struct resultado r;
      agregar(&r, w * NCUENTAS / k, (w + 1) * NCUENTAS / k, 1);
      r.pid = ugetpid();
      if(escribir_todo(fds[w][1], &r, sizeof(r)) < 0)
        exit(1);
      exit(0);
    }
    close(fds[w][1]);
  }
  uint64 libres1 = libres();

  struct agregado total[NSUCURSALES];
  memset(total, 0, sizeof(total));
  int ok = 1, cubiertas = 0;
  for(int w = 0; w < k; w++){
    struct resultado r;
    if(leer_todo(fds[w][0], &r, sizeof(r)) < 0)
      morir("un trabajador no envió su resultado");
    close(fds[w][0]);
    printf("trabajador pid=%d cuentas=[%d,%d)\n", r.pid, r.desde, r.hasta);
    if(r.desde != w * NCUENTAS / k || r.hasta != (w + 1) * NCUENTAS / k)
      ok = 0;
    cubiertas += r.hasta - r.desde;
    for(int s = 0; s < NSUCURSALES; s++){
      total[s].cuentas += r.suc[s].cuentas;
      total[s].saldo_total += r.suc[s].saldo_total;
      total[s].firmas += r.suc[s].firmas;
      if(r.suc[s].saldo_max > total[s].saldo_max)
        total[s].saldo_max = r.suc[s].saldo_max;
    }
  }
  for(int w = 0; w < k; w++){
    int estado;
    if(wait(&estado) < 0 || estado != 0)
      ok = 0;
  }
  int t1 = uptime();

  // Verificación secuencial (sin la firma, que es la parte costosa).
  struct resultado sec;
  agregar(&sec, 0, NCUENTAS, 0);
  uint firmas = 0;
  for(int s = 0; s < NSUCURSALES; s++){
    printf("sucursal %d: cuentas=%d total=%ld max=%d\n", s,
           total[s].cuentas, total[s].saldo_total, total[s].saldo_max);
    if(total[s].cuentas != sec.suc[s].cuentas ||
       total[s].saldo_total != sec.suc[s].saldo_total ||
       total[s].saldo_max != sec.suc[s].saldo_max)
      ok = 0;
    firmas += total[s].firmas;
  }
  if(cubiertas != NCUENTAS)
    ok = 0;
  printf("consulta: firma=0x%x\n", firmas);
  printf("consulta: paginas_libres antes=%ld con_trabajadores=%ld\n", libres0, libres1);
  printf("consulta: ticks=%d\n", t1 - t0);
  printf("consulta: %s\n", ok ? "OK" : "FALLO");
  exit(ok ? 0 : 1);
}


// ---------------------------------------------------------------------
// Part C.2 (implemented by the group): consistent snapshot.
// Identifiers that come from minidb.h and the output lines of Annex B
// keep their original names, because the test script checks them.
// ---------------------------------------------------------------------

// Pages used by the table (64, the most pgaccess() accepts).
#define TABLE_PAGES ((int)(NCUENTAS * sizeof(struct cuenta) / PGSIZE))
#define TABLE_BYTES ((int)(NCUENTAS * sizeof(struct cuenta)))

static uint64
total_balance(void)
{
  uint64 total = 0;
  for(int i = 0; i < NCUENTAS; i++)
    total += tabla[i].saldo;
  return total;
}

// Snapshot child. Its page table points to the account table as it was
// at fork(): later writes by the parent land on copies (CoW), so the
// child can write the file straight from memory, without copying the
// table and without stopping the parent.
static void
guardar_snapshot(uint checksum)
{
  struct snap_encabezado header;
  header.magia = SNAP_MAGIA;
  header.n = NCUENTAS;
  header.checksum = checksum;
  header.reservado = 0;

  int fd = open(SNAP_ARCHIVO, O_CREATE | O_WRONLY | O_TRUNC);
  if(fd < 0)
    exit(1);
  if(escribir_todo(fd, &header, sizeof(header)) < 0 ||
     escribir_todo(fd, tabla, TABLE_BYTES) < 0)
    exit(1);
  close(fd);
  exit(0);
}

// Read SNAP_ARCHIVO back in full. Leaves the header in *header and the
// checksum of the saved accounts in *checksum, and returns the file size
// in bytes, or -1 if the whole file could not be read.
static int
read_snapshot(struct snap_encabezado *header, uint *checksum)
{
  // The user stack is a single page: the buffer comes from sbrk().
  struct cuenta *copy = (struct cuenta *)sbrk(TABLE_BYTES);
  if(copy == (struct cuenta *)-1)
    return -1;
  int fd = open(SNAP_ARCHIVO, O_RDONLY);
  if(fd < 0)
    return -1;
  struct stat st;
  int ok = fstat(fd, &st) == 0 &&
           leer_todo(fd, header, sizeof(*header)) >= 0 &&
           leer_todo(fd, copy, TABLE_BYTES) >= 0;
  close(fd);
  if(!ok)
    return -1;
  *checksum = checksum_tabla(copy, NCUENTAS);
  return st.size;
}

// Apply m transfers of the deterministic sequence (seed 4) to the table.
// Returns how many were applied; the rest are rejected because source and
// destination are the same account or the source balance is too low.
static int
apply_transfers(int m)
{
  uint seed = 4;
  int applied = 0;
  for(int i = 0; i < m; i++){
    struct transferencia t;
    generar_transferencia(&seed, &t);
    struct cuenta *src = &tabla[t.origen];
    struct cuenta *dst = &tabla[t.destino];
    if(t.origen == t.destino || src->saldo < t.monto)
      continue;
    src->saldo -= t.monto;
    dst->saldo += t.monto;
    src->nmov++;
    dst->nmov++;
    applied++;
  }
  return applied;
}

static void
snapshot(int m)
{
  if(m < 0)
    morir("M cannot be negative");
  cargar_tabla();

  // State of the table at the instant that must be saved.
  uint checksum0 = checksum_tabla(tabla, NCUENTAS);
  uint64 total0 = total_balance();
  printf("snapshot: checksum_inicial=0x%x\n", checksum0);

  // pgaccess() clears the PTE_A bits: whatever it reports after the fork
  // is only the pages used by the transfers.
  uint64 accessed;
  if(pgaccess(tabla, TABLE_PAGES, &accessed) < 0)
    morir("pgaccess() failed");

  int pid = fork();
  if(pid < 0)
    morir("fork() failed");
  if(pid == 0)
    guardar_snapshot(checksum0);
  printf("snapshot: hijo pid=%d escribiendo %s\n", pid, SNAP_ARCHIVO);

  // The parent keeps working without waiting for the child. The printf()
  // above already wrote to the stack, so its CoW fault stays out of the
  // measurement and the counter difference is only table pages.
  struct cowstat before, after;
  if(cowstat(&before) < 0)
    morir("cowstat() failed");
  int applied = apply_transfers(m);
  if(cowstat(&after) < 0)
    morir("cowstat() failed");
  if(pgaccess(tabla, TABLE_PAGES, &accessed) < 0)
    morir("pgaccess() failed");
  int touched = 0;
  for(int i = 0; i < TABLE_PAGES; i++)
    if(accessed & (1L << i))
      touched++;
  printf("snapshot: transferencias=%d aplicadas=%d rechazadas=%d\n",
         m, applied, m - applied);
  printf("snapshot: paginas_tocadas=%d fallos_cow=%ld copias_cow=%ld\n", touched,
         after.cowfaults - before.cowfaults, after.cowcopies - before.cowcopies);

  // The file must hold the table from before the fork, not the current one.
  int status = -1;
  if(wait(&status) != pid)
    status = -1;
  struct snap_encabezado header;
  uint file_checksum = 0;
  memset(&header, 0, sizeof(header));
  int bytes = read_snapshot(&header, &file_checksum);
  int matches = status == 0 &&
                bytes == (int)sizeof(header) + TABLE_BYTES &&
                header.magia == SNAP_MAGIA && header.n == NCUENTAS &&
                header.checksum == checksum0 && header.reservado == 0 &&
                file_checksum == checksum0;
  printf("snapshot: archivo bytes=%d checksum=0x%x coincide=%s\n",
         bytes < 0 ? 0 : bytes, file_checksum, matches ? "si" : "no");

  // Transfers move balance between accounts: the total does not change.
  int conserved = total_balance() == total0;
  printf("snapshot: checksum_actual=0x%x total_conservado=%s\n",
         checksum_tabla(tabla, NCUENTAS), conserved ? "si" : "no");

  int ok = matches && conserved;
  printf("snapshot: %s\n", ok ? "OK" : "FALLO");
  exit(ok ? 0 : 1);
}

static void
mapa(void)
{
  // Una tabla pequeña para que la salida de vmprint() sea corta.
  char *t = sbrk(2 * PGSIZE);
  if(t == (char *)-1)
    morir("sbrk() falló");
  t[0] = t[PGSIZE] = 1;
  printf("mapa: tabla en 0x%lx (2 páginas)\n", (uint64)t);
  int pid = fork();
  if(pid < 0)
    morir("fork() falló");
  if(pid == 0){
    printf("mapa: hijo antes de escribir\n");
    vmprint();
    t[0] = 2;
    printf("mapa: hijo después de escribir en 0x%lx\n", (uint64)t);
    vmprint();
    exit(0);
  }
  wait(0);
  exit(0);
}

int
main(int argc, char *argv[])
{
  if(argc >= 2 && strcmp(argv[1], "consulta") == 0)
    consulta(argc >= 3 ? atoi(argv[2]) : 1);
  if(argc >= 2 && strcmp(argv[1], "snapshot") == 0)
    snapshot(argc >= 3 ? atoi(argv[2]) : 2000);
  if(argc >= 2 && strcmp(argv[1], "mapa") == 0)
    mapa();
  printf("uso: minidb consulta K | minidb snapshot M | minidb mapa\n");
  exit(1);
}
