#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

#define PAGESZ 4096        // Page size
#define MB     (1024 * 1024)

static int fails = 0;

static void
check(int cond, char *msg)
{
  if (cond) {
    printf("  [PASS] %s\n", msg);
  } else {
    fprintf(2, "  [FAIL] %s\n", msg);
    fails++;
  }
}

// Call Syscall and check failure
static void
getinfo(struct sysinfo *info)
{
  if (sysinfo(info) < 0) {
    fprintf(2, "sysinfo: Syscall failed\n");
    exit(1);
  }
}

static void
show(struct sysinfo *info)
{
  printf("Free Memory:        %lu MB (%lu KB)\n", info->freemem / MB, info->freemem / 1024);
  printf("Used Pages:         %lu\n", info->usedpages);
  printf("Available Pages:    %lu\n", info->freepages);
  printf("Total Pages:        %lu\n", info->totalpages);
  printf("Runnable Processes: %lu\n", info->nrunnable);
  printf("Running Processes:  %lu\n", info->nrunning);
}

// Test 1: data consistency
static void
test_consistency(void)
{
  struct sysinfo s;
  printf("Test 1: data consistency\n");
  getinfo(&s);
  check(s.freepages + s.usedpages == s.totalpages, "freepages + usedpages == totalpages");
  check(s.freemem == s.freepages * PAGESZ, "freemem == freepages * 4096");
  check(s.nrunning >= 1, "at least 1 process running");
}

// Test 2: allocating and freeing memory must show in pages
static void
test_memory(void)
{
  struct sysinfo before, during, after;
  int n = 64; // pages to alocate

  printf("Test 2: memory (sbrk of %d pages)\n", n);
  getinfo(&before);
  if (sbrk(n * PAGESZ) == SBRK_ERROR) {
    fprintf(2, "sysinfo: sbrk failure\n");
    exit(1);
  }
  getinfo(&during);
  if (sbrk(-n * PAGESZ) == SBRK_ERROR) {
    fprintf(2, "sysinfo: negative sbrk failure\n");
    exit(1);
  }
  getinfo(&after);

  printf("  free pages: before=%lu  with sbrk=%lu  after releasing=%lu\n",
         before.freepages, during.freepages, after.freepages);
  check(before.freepages - during.freepages >= n,
        "sbrk reduces free pages in at least n");
  check(during.usedpages - before.usedpages >= n,
        "sbrk increases used pages in at least n");
  check(after.freepages - during.freepages >= n,
        "neagtive sbrk returns at least n pages");
}

// Test 3: childs in infinite loops must show RUNNABLE/RUNNING
static void
test_runnable(void)
{
  struct sysinfo before, during, after;
  int k = 6, pids[6], i;

  printf("Test 3: RUNNABLE procs (%d childs in loop)\n", k);
  getinfo(&before);
  for (i = 0; i < k; i++) {
    pids[i] = fork();
    if (pids[i] < 0) {
      fprintf(2, "sysinfo: fork failed\n");
      exit(1);
    }
    if (pids[i] == 0) {
      volatile int x = 0;
      for (;;)
        x++;
    }
  }
  pause(5);
  getinfo(&during);

  for (i = 0; i < k; i++)
    kill(pids[i]);
  for (i = 0; i < k; i++)
    wait(0);
  getinfo(&after);

  printf("  RUNNABLE: before=%lu  with children=%lu  after=%lu\n",
         before.nrunnable, during.nrunnable, after.nrunnable);
  printf("  RUNNING:  before=%lu  with children=%lu  after=%lu\n",
         before.nrunning, during.nrunning, after.nrunning);
  check(during.nrunnable + during.nrunning >= k + 1,
        "childs + parent are RUNNABLE or RUNNING");
  check(during.nrunnable >= 1, "there are RUNNABLE procs waiting CPU");
}

// Test 4: invalid addr must be rejected by copyout
static void
test_badptr(void)
{
  printf("Test 4: invalid pointers\n");
  check(sysinfo((struct sysinfo *)0) < 0,
        "addr 0 (Only Reading text) -> -1");
  check(sysinfo((struct sysinfo *)0x80000000L) < 0,
        "kernel addr (KERNBASE) -> -1");
  check(sysinfo((struct sysinfo *)0xffffffffffffffffL) < 0,
        "addr outside MAXVA -> -1");
}

int
main(int argc, char *argv[])
{
  struct sysinfo info;

  if (argc == 1) {
    getinfo(&info);
    show(&info);
    exit(0);
  }

  if (argc == 2 && strcmp(argv[1], "-t") == 0) {
    test_consistency();
    test_memory();
    test_runnable();
    test_badptr();
    if (fails) {
      fprintf(2, "sysinfo: %d test(s) failed\n", fails);
      exit(1);
    }
    printf("sysinfo: all tests succeed\n");
    exit(0);
  }

  fprintf(2, "uso: sysinfo [-t]\n");
  exit(1);
}