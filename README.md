# malloc-rqsizes
malloc_stats is a shared library that wraps C library malloc().
The wrapper collects request size counts (amount of bytes per malloc call) for debugging.
This project has NO guarantees and/or warranty for usability/fittness for ANY particular purpose.

libmalloc_stats.so is LD_PRELOAD'ed into victim process to collect data:

env LD_PRELOAD=./libmalloc_stats.so <program>

when <program> exits, /tmp/malloc-stats.pid.<PID> file should have been created.
Next, run 

format_stats /tmp/malloc-stats.pid.<PID>

to print the malloc.stats contents to stdout.
malloc_stats sorts internally by request size, but sorting the output by occurences:

format_stats /tmp/malloc-stats.pid.<PID> | sort -gk3

produces more usable information.
