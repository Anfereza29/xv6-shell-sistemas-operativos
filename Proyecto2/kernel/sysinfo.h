// Struct for kernel to fill in sys_sysinfo()
#ifndef SYSINFO_H
#define SYSINFO_H

struct sysinfo {
  uint64 freemem;    // free memory (bytes)
  uint64 freepages;
  uint64 usedpages;
  uint64 totalpages; // pages managed by kalloc
  uint64 nrunnable;  // RUNNABLE procs
  uint64 nrunning;   // RUNNING procs
};

#endif