// Copy-on-write statistics returned by the cowstat() system call
// (Project 4). The structure is the one given in the assignment.
struct cowstat {
  uint64 freepages;   // páginas físicas libres en todo el sistema
  uint64 sharedpages; // páginas físicas con más de una referencia
  uint64 cowfaults;   // fallos CoW atendidos para el proceso que llama
  uint64 cowcopies;   // de esos fallos, cuántos copiaron la página
};
