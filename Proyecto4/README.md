# Proyecto de Sistemas Operativos
## Proyecto 4: Copy-on-Write y MiniDB (xv6)

### Integrantes

- Laura Santamaría
- Felipe Gallón
- Andrés Rengifo

### Curso

Sistemas Operativos

### Semestre

2026-2

---

# Descripción

The submission modifies xv6-riscv (commit 06aad25 + `base_proyecto3.patch`) in three parts.

**Part A, Copy-on-Write.** `fork()` no longer copies the parent's memory. `uvmcopy()` maps the same physical pages in the child, adds one reference to each and, if a page was writable, clears `PTE_W` and sets `PTE_COW` (bit 8) in both page tables. The first write raises a page fault (`scause` = 15) handled by `cowfault()`: if the page is still shared it is copied to a new one; if the process already holds the only reference, it just gets `PTE_W` back. `copyout()` uses the same function, because the kernel writes to user memory without raising page faults. `kalloc.c` keeps a reference count per physical page, protected by its own spinlock, and `kfree()` returns a page to the free list only when its count reaches 0.

**Part B, `cowstat()`.** System call number 25. It returns the free pages in the system, the pages with more than one reference, and two per-process counters (CoW faults handled and faults that copied the page). The two global values are kept as counters inside `kalloc.c`, so the call does not scan memory.

**Part C, MiniDB.** We completed `minidb snapshot M`. The parent computes the checksum and the total balance, clears the access bits with `pgaccess()` and calls `fork()`. The child writes `snap.db` (header plus the 8,192 accounts) straight from the table. The parent, without waiting, applies the `M` transfers, reports pages touched, CoW faults and CoW copies, and finally waits for the child, reads the file back and checks that it holds the table from before the `fork()`.

---

# Archivos modificados

| File | What changed and why |
|---|---|
| `kernel/riscv.h` | Defines `PTE_COW` (`1L << 8`), one of the RSW bits. It tells "not writable for now" apart from "never writable". |
| `kernel/kalloc.c` | `kref.refs[]` array with the reference count of every page between `KERNBASE` and `PHYSTOP`, with its own spinlock `kref.lock`. `kalloc()` leaves the page at 1, `krefinc()` adds one, `kfree()` subtracts one and frees only at 0. `freerange()` sets the count to 1 before calling `kfree()`, because those pages never went through `kalloc()`. Counters `nfree` and `nshared`, and functions `krefcount()`, `kfreepages()` and `ksharedpages()`. |
| `kernel/vm.c` | `uvmcopy()` shares instead of copying. New function `cowfault()`. `copyout()` resolves a CoW page before writing. |
| `kernel/trap.c` | `usertrap()` calls `cowfault()` when `scause` = 15. If it was not a CoW page, the fault follows the usual path: `vmfault()` for lazy pages, or the process is killed. |
| `kernel/proc.h`, `kernel/proc.c` | Fields `cowfaults` and `cowcopies` in `struct proc`; `allocproc()` sets them to 0. |
| `kernel/cowstat.h` | New file with `struct cowstat`. |
| `kernel/sysproc.c` | `sys_cowstat()`. |
| `kernel/syscall.h`, `kernel/syscall.c`, `user/usys.pl`, `user/user.h` | Registration of the `cowstat` call (number 25). |
| `kernel/defs.h` | Prototypes of `krefinc()`, `krefcount()`, `kfreepages()`, `ksharedpages()` and `cowfault()`. |
| `Makefile` | `_pgtbltest`, `_cowtest` and `_minidb` added to `UPROGS`. |
| `user/minidb.c` | `guardar_snapshot()` and `snapshot()`, plus three helpers: `total_balance()`, `read_snapshot()` and `apply_transfers()`. |
| `kernel/memlayout.h`, `user/ulib.c` | Not changed by the group. Submitted in full because the Project 3 base already modified them. |

---

# Integración con xv6

1. Clonar xv6-riscv, hacer `git switch -c proyecto4 06aad25` y
   aplicar `git apply base_proyecto3.patch`.
2. Copiar los archivos entregados en sus rutas.
3. Copiar los archivos del docente: user/cowtest.c, user/minidb.h,
   user/pgtbltest.c y user/usertests.c.
4. Compilar con `make qemu CPUS=3`.
5. Ejecutar `cowtest`, `minidb consulta 3`, `minidb snapshot 2000`,
   `pgtbltest` y `usertests -q`.

---

# Respuestas a las preguntas

## 1. PTE antes y después de una escritura CoW

From the output of `minidb mapa` (in `pruebas/minidb.txt`), the first page of the table is at virtual address `0x6000`:

```
before:  .. .. ..0x0000000000006000: pte 0x0000000021fd11d3 pa 0x0000000087f44000
after:   .. .. ..0x0000000000006000: pte 0x0000000021fb28d7 pa 0x0000000087eca000
```

The low 10 bits of the PTE are the flags; the rest is the physical page number (PPN).

| | Flags | COW (8) | D (7) | A (6) | U (4) | X (3) | W (2) | R (1) | V (0) | PPN → physical address |
|---|---|---|---|---|---|---|---|---|---|---|
| Before | `0x1d3` | 1 | 1 | 1 | 1 | 0 | **0** | 1 | 1 | `0x87f44` → `0x87f44000` |
| After | `0x0d7` | **0** | 1 | 1 | 1 | 0 | **1** | 1 | 1 | `0x87eca` → `0x87eca000` |

Before writing, the child sees the parent's page: it is valid, user-accessible and readable, but it has no `PTE_W` and it does have `PTE_COW`. The A and D bits are inherited, because the parent had already written `t[0] = 1` before the `fork()` and `uvmcopy()` keeps the other flags.

When the child runs `t[0] = 2`, the hardware finds the PTE without `PTE_W` and raises a write page fault. `cowfault()` reads the count of page `0x87f44000`, which is 2 (parent and child). Since it is shared, it allocates another page (`0x87eca000`), copies the 4096 bytes, points the child's PTE to the copy with `PTE_W` and without `PTE_COW`, and drops one reference to the original. That is why the physical address changes: the child stops using the parent's page and gets its own.

The parent's PTE is not touched. It still points to `0x87f44000`, without `PTE_W` and with `PTE_COW`, and the `1` it wrote is still there. The only thing that changes is the count of that page, which goes down to 1. If the parent writes to it again it will take a page fault, but `cowfault()` will see a single reference and give `PTE_W` back without copying anything.

## 2. Páginas sin PTE_W y USYSCALL en fork()

`uvmcopy()` walks the addresses from 0 to `p->sz` and applies a single rule: the physical page is shared with the child and gains a reference; if the PTE had `PTE_W`, it is cleared and `PTE_COW` is set in both page tables.

- **Text.** Its PTEs are R, X, U, without `PTE_W`. They are mapped in the child with the same flags and without `PTE_COW`. Parent and child share those physical pages indefinitely.
- **Guard page.** In xv6, `exec()` creates it with `uvmalloc(..., PTE_W)` and then `uvmclear()` removes `PTE_U`. Its PTE does have `PTE_W`, so `uvmcopy()` sets `PTE_COW` on it like on any other page. That does not make it writable: `cowfault()` requires `PTE_U`, and without that bit it returns -1 and `usertrap()` kills the process, exactly as before the `fork()`.
- **USYSCALL.** It lives at the top of the address space, above `p->sz`, so `uvmcopy()` never walks it. Every process gets its own in `allocproc()`, which allocates a page with `kalloc()`, writes the pid into it and maps it with R and U. The same holds for the trapframe.

They are not marked CoW because `PTE_COW` means "this page was writable and must be copied when someone writes". If text pages carried the mark, a write to the process's own code after a `fork()` would not kill it: `cowfault()` would hand it a writable copy and the program could modify itself. Without the mark, the write fault reaches `usertrap()` as an error and the process dies (test `solo_lectura`).

If `USYSCALL` were shared, the child's page table would point to the parent's physical page, which holds the parent's pid. `ugetpid()` reads that value without entering the kernel, so in the child it would return the parent's pid and `ugetpid() != getpid()`. It could not be fixed by writing the child's pid into the shared page, because then the parent would be the one reading someone else's pid.

## 3. Carrera en el conteo de referencias

A page P is shared by two processes, A and B, and its count is 2. Incrementing and decrementing take three steps (read, compute, write), and without a lock they can interleave.

**Case 1: a page still in use is freed.** A calls `fork()` on CPU 0 (creating C, which must raise the count to 3) while B exits on CPU 1 (which must lower it).

| Step | CPU 0: `krefinc(P)` for A's fork | CPU 1: `kfree(P)` for B's exit | Count in memory |
|---|---|---|---|
| 1 | | reads 2 | 2 |
| 2 | reads 2, computes 3, writes 3 | | 3 |
| 3 | | computes 2 − 1 and writes 1 | 1 |

Two page tables now point to P (those of A and C), but the count says 1. If C exits, `kfree()` lowers it to 0 and returns P to the free list while A is still using it; the next `kalloc()` hands it to another process or to the kernel, and A reads and writes someone else's data. If A writes first instead, `cowfault()` sees a single reference and gives `PTE_W` back without copying, and C starts seeing A's writes.

**Case 2: a page is lost forever.** A and B exit at the same time. Both CPUs read 2 and both write 1. Neither sees the 0, P never returns to the free list and no page table points to it any more. The `sin_fugas` test and the free-page check in `concurrente` detect exactly this.

With `kref.lock`, reading, modifying and writing the count is a single critical section, so these interleavings cannot happen.

## 4. Consistencia del snapshot

**Why the file has the checksum from before the fork.** At the moment of the `fork()`, the child's page table points to the same physical pages as the parent, all without `PTE_W`. When the parent applies a transfer and writes to a table page, it takes a page fault; since the page has 2 references, `cowfault()` gives the parent a new copy and leaves the original untouched. The child keeps pointing to the original, and nobody else writes to it. The child therefore walks an image of the table frozen at the instant of the `fork()`, no matter how many transfers the parent applies meanwhile or how the two processes interleave. That is why `snap.db` has checksum `0x6915D295` while the parent's table ends at `0x4AB3E209`.

**Why `copias_cow` is less than 64.** Only the pages the parent writes to are copied. The generator concentrates 255 of every 256 transfers on the 1,024 most active accounts, which fill the first 8 pages (128 accounts per page). Of the 2,000 transfers, only 16 draw from the whole range, and they touch 22 other pages. That makes 30 pages touched and 30 copies; the other 34 table pages stay shared until the child exits and are never duplicated.

**If the parent waited for the child before the transfers.** On exit, the child's page table is freed and every table page goes back to a single reference. The parent's PTEs still have no `PTE_W` and still carry `PTE_COW`, so the first write to each page still faults, but `cowfault()` sees one reference and gives `PTE_W` back without copying. We checked this by temporarily moving the `wait()`: the output was `paginas_tocadas=30 fallos_cow=30 copias_cow=0`. The cost would be elsewhere: the parent would be stopped while the file is written, which is exactly what the snapshot is meant to avoid.

## 5. Aceleración y planificador

Ticks of `minidb consulta K` measured on our machine (full table in `REPORTE.pdf`):

| K | 1 | 2 | 3 | 4 | 8 |
|---|---|---|---|---|---|
| `CPUS=1` | 41 | 39 | 36 | 41 | 42 |
| `CPUS=3` | 44 | 23 | 22 | 22 | 21 |

The total work is fixed: 8,192 signatures, each with 10,000 rounds of pure CPU. There is no I/O and no waiting, so the time depends only on how many CPUs compute signatures at once.

**From 1 to 3 workers with `CPUS=3`.** The xv6 scheduler is Round Robin over a single process table: every CPU runs its own `scheduler()`, which scans the table and takes the first `RUNNABLE` process it finds. With one worker, one CPU does all the computation and the other two find nothing to run. With several workers, each idle CPU takes a different one and the account ranges are processed at the same time. The workers do not get in each other's way because they only read the table: with CoW they share the same pages and none of them causes a copy. In our measurement the time halves with 2 workers and no longer improves with 3, because the computer running QEMU had 2 physical cores: xv6 sees 3 CPUs, but QEMU can only really run 2 at a time. On a machine with 3 or more free cores, the time with K = 3 drops to about a third (the reference output shipped with the assignment reports 42, 20 and 14 ticks).

**From 3 to 8 workers.** There are no idle CPUs left. The 8 processes share the same 3 CPUs: on every clock tick `usertrap()` calls `yield()` and the CPU moves on to the next `RUNNABLE` process. Each worker has fewer accounts to process but gets a smaller share of CPU, and the sum of the work is the same. Total time stays put; only context switches and more memory (9 pages per worker) are added.

**With `CPUS=1`.** There is a single CPU for everything. Round Robin alternates the K workers one tick each, which gives concurrency but not parallelism: at any instant only one signature is being computed. The time stays near 40 ticks for any K; differences of a few ticks are measurement noise.

---

# Resultados de las pruebas

Run with `ejecutar_pruebas.py` (QEMU with 3 CPUs). The full output is in `pruebas/`.

| Suite | Result |
|---|---|
| `cowtest` | TODAS LAS PRUEBAS PASARON (11 de 11) |
| `minidb` | Result review: OK. `consulta` with 1, 2, 3 and 8 workers, `snapshot 2000` (aplicadas=1978, rechazadas=22, 30 pages touched, 30 copies), `snapshot 0` (0 copies) and `mapa` |
| `pgtbltest` | TODAS LAS PRUEBAS PASARON (11 de 11) |
| `usertests -q` | ALL TESTS PASSED |

The `usertrap(): unexpected scause 0xf` messages that show up in `cowtest` are expected: they come from `solo_lectura` (invalid writes on purpose) and from `sin_memoria` (processes the kernel kills because it cannot copy).

---

# Decisiones de diseño

- **Two separate locks in `kalloc.c`.** `kmem.lock` protects the free list and `kref.lock` protects the counts. They are never held at the same time: `kfree()` releases `kref.lock` before touching the list and `kalloc()` releases `kmem.lock` before setting the count. So there is no lock order to respect and no risk of deadlock. This is safe because a page with count 0 that is not yet on the list cannot be reached by anyone.
- **The copy decision is made by reading the count, without holding the lock during the copy.** A count of 1 cannot go up while `cowfault()` runs, because only a `fork()` by the same process would add a reference. If the count was 2 and the other process drops the page while we copy, we make a copy that was not needed, but the result is correct and the original is freed in `kfree()`.
- **`cowfault()` is tried before `vmfault()`.** If the page is not CoW it returns -1 and the fault continues to the lazy-allocation path. Lazy pages have no valid PTE, so they are never mistaken for a CoW page.
- **The parent loses `PTE_W` after the child is mapped.** If `mappages()` fails halfway through the `fork()`, the page being processed stays intact in the parent. The earlier ones are left with `PTE_COW` and a single reference, and become writable again without a copy on their next write.
- **Counters instead of scans.** `freepages` and `sharedpages` are updated in `kalloc()`, `kfree()` and `krefinc()`, so `cowstat()` runs in constant time.
- **Per-process counters are updated inside `cowfault()`,** so faults arriving through `usertrap()` and those resolved by `copyout()` are counted the same way. Only faults handled successfully are counted.
- **Snapshot.** The `cowstat()` measurement starts after the `printf()` that announces the child. That `printf()` has already written to the stack, so the stack's CoW fault is left out and the difference covers table pages only. The size of `snap.db` is read with `fstat()` rather than inferred from what was read.
- **Naming.** Our own identifiers and comments are in English, like the rest of xv6. Names that come from `minidb.h` and the output lines of Annex B are kept as given, because the test script checks them.

---

# Limitaciones conocidas

- If there is no memory for the copy during a `copyout()`, the system call returns -1 instead of killing the process, the same thing xv6 does when `vmfault()` fails on that path. The process is killed when the fault arrives through `usertrap()`, which is the case covered by `sin_memoria`.
- Two processes writing at the same time to the same shared page may both copy it; one of them could have kept the original. It costs one extra copy and does not affect correctness.
- The count array uses one `int` per page (128 KB for 128 MB of RAM). A smaller type would be enough.
- Page-table pages are not shared with CoW, and nothing the assignment leaves out of scope was implemented.
- `snap.db` stays on disk after `minidb snapshot`; every run truncates it and writes it again.
- The ticks in question 5 were measured on a 2-core computer, so the speedup with `CPUS=3` stops at 2x.

---

# Declaración de uso de IA

Claude (Anthropic) was used as an assistant for the implementation of Parts A, B and C.2, for running the tests and experiments, and for writing this README.

