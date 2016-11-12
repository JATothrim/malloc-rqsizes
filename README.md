# malloc-rqsizes
malloc_stats is a shared library that wraps C library malloc().
The wrapper collects request size counts (amount of bytes per malloc call) for debugging.<br>

DISCLAIMER: This project has NO guarantees and/or usability/fitness for ANY particular purpose.<br>

libmalloc_stats.so is LD_PRELOAD'ed into victim process to collect data:<br>

<code>env LD_PRELOAD=./libmalloc_stats.so <i>program</i></code><br>

when <i>program</i> exits, /tmp/malloc-stats.pid.<i>PID</i> file should have been created.<br>
Next, run <br>
<code>format_stats /tmp/malloc-stats.pid.<i>PID</i></code><br>
to print the malloc.stats contents to stdout.<br>

malloc_stats sorts internally by request size, but sorting the output by occurences:<br>
<code>format_stats /tmp/malloc-stats.pid.<i>PID</i> | sort -gk3</code><br>
produces more usable information.
