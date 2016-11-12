/**
 * malloc_stats tool is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This malloc_stats is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
/* 
 * (C) 2016 Jarmo A Tiitto.
 * 
 * the C library malloc is hijacked via LD_PRELOAD mechanism
 * to collect counts of each total size requested.
 * 
 * This is really intresting in case we are developing
 * malloc() by our selves or just pure curiosity. :-)
 * 
 * compile with: gcc -shared -fPIC
 * 
 * load it with LD_PRELOAD=./malloc_stats.so <yourtestsubject>
 * 
 * the hijacked program creates a file '/tmp/malloc-stats.pid.<PID>'
 * that can then read by format_stats once victim exits cleanly.
 **/
 
#undef malloc

static int mhs_fd = 0;
static _Atomic void* mhs_p = 0;
static _Atomic size_t mhs_cnt = 0; //<mhs_p buffer end pos in RECORD_SZ>
static _Atomic size_t mhs_pages = 0; //<mhs_p buffer capacity in PAGES>
static pthread_mutex_t mhs_mtx = PTHREAD_MUTEX_INITIALIZER;

static const size_t PAGE = 4096;

typedef struct record {
	int size;
	int hits;
} record_t;

static const size_t RECORD_SZ = sizeof(record_t);


// get mhs_p mapping capacity.
static size_t curr_stat_capacity() {
	return mhs_pages * PAGE;
}

// get max usable RECORD index.
static size_t max_cnt() {
	return (mhs_pages * PAGE) / RECORD_SZ;
}

// print error message and kill the process
// it tries to cope with fact C runtime is not fully initialized.
static void __attribute__((noreturn)) exit_fail(const char * msg) {
	int fderr = dup(2);
	if(fderr == -1) {
		_Exit(99);
	}
	write(fderr, msg, strlen(msg));
	close(fderr);
	// kill process.
	_Exit(99);
}

static void sort_records(record_t * base, size_t count );

// define destructor to be invoked on program exit.
static void malloc_stats_cleanup(void) __attribute__((destructor));

static void malloc_stats_cleanup(void) {
	if(mhs_fd) {
		pthread_mutex_lock(&mhs_mtx);
		
		// sort stats
		sort_records((record_t*)mhs_p, mhs_cnt);
		
		// flush
		msync(mhs_p, curr_stat_capacity(), MS_SYNC);
		fsync(mhs_fd);
		munmap(mhs_p, curr_stat_capacity());
		close(mhs_fd);
		mhs_p = 0;
		pthread_mutex_unlock(&mhs_mtx);
		pthread_mutex_destroy(&mhs_mtx);
	}
}

// zero-out memory
static void memclear(void * p, void * end) {
	char * wp = (char*)p;
	for(wp = (char*)p; wp < (char*)end; wp++)
		*wp = 0;
}

/* resize stat mapping area.
 *  pre:	mhs_fd must be initialized;
 * 			nbytes must be multiple of system page;
 * 			mhs_mtx must be locked.
 *  post:	mhs_p is set to buffer with capacity of nbytes.
 * 			mhs_pages is set to nbytes / PAGE
 * */
static void resize_backing(size_t nbytes) {
	
	// resize the backing file
	if(ftruncate(mhs_fd, nbytes)) {
		exit_fail("Error: ftruncate failed!\n");
	}
	
	if(!mhs_p) {
		// Initial mapping.
		mhs_p = mmap(0, nbytes, PROT_READ | PROT_WRITE, 
			MAP_SHARED, mhs_fd, 0);
		if(mhs_p == MAP_FAILED) {
			exit_fail("Error mmap failed!");
		}
		memclear(mhs_p, (char*)mhs_p + nbytes);
		mhs_pages = nbytes / PAGE;
	} else {
		
		mhs_p = mremap(mhs_p, mhs_pages * PAGE, nbytes, MREMAP_MAYMOVE);
		if(mhs_p == MAP_FAILED) {
			exit_fail("Error mremap failed!");
		}
		// zero the new area
		memclear((char*)mhs_p + mhs_pages * PAGE, (char*)mhs_p + nbytes);
		mhs_pages = nbytes / PAGE;
	}
}

static  void init_malloc_stats() {
	
	// clear LD_PRELOAD env.
	// this prevents leaking us into other started processes.
	unsetenv("LD_PRELOAD");
	
	// open our stats backing file.
	char fmt_file[128] = {0};
	snprintf(fmt_file, 127, "/tmp/malloc-stats.pid.%d", getpid());
	mhs_fd = open(fmt_file, 
		O_CREAT|O_RDWR|O_TRUNC|O_CLOEXEC,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP );
	
	if(!mhs_fd) {
		exit_fail("Error failed to create/open stats file!");
	}
	
	// use memory-mapped file to speed things up.
	// first bring up the backing file..
	resize_backing(PAGE);
	
	// print banner that we are doing our black wizardy magic..
	const char * banner = "Collecting malloc() request size statistics.\n";
	write(fileno(stderr), banner, strlen(banner));
}

static void sort_records(record_t * base, size_t count ) {
	// simple insertion sort. Note: qsort will not work!
	record_t tmp;
	ptrdiff_t k,j;
	for(k = 1;  k < count; ++k) {
		tmp = base[k];
		j = k - 1;
		while(j >= 0 && base[j].size < tmp.size) {
			base[j+1] = base[j];
			--j;
		}
		base[j+1] = tmp;
	}
}

// do binary search for x
static ptrdiff_t binary_srch(size_t x, record_t * base, ptrdiff_t low, ptrdiff_t high) {
	ptrdiff_t mid;
	while( low <= high ) {
		mid = (high + low)/2;
		if(base[mid].size < x) {
			high = mid - 1;
		} else if(base[mid].size > x) {
			low = mid + 1;
		} else
			return mid;	// found
	}
	return -1;
}

// find equal request size and inc hit count.
static int malloc_stats_gather(size_t asize) {
	
	record_t * rec = (record_t*)mhs_p;
	ptrdiff_t srch;
	
	// find request size.
	srch = binary_srch(asize, (record_t*)mhs_p, 0, mhs_cnt);
	if(srch >= 0) {
		rec[srch].hits++;
		return 1;
	} else
		return 0;
}

// grow the statistics data mapping capacity.
static void malloc_stats_grow(size_t asize) {
	if(mhs_cnt+1 >= max_cnt()) {		
		// expand backing file
		resize_backing((mhs_pages+1) * PAGE);
	}
	record_t * rec = (record_t*)mhs_p;
	rec[mhs_cnt].size = asize;
	rec[mhs_cnt].hits += 1;
	mhs_cnt++;
	// sort
	sort_records((record_t*)mhs_p, mhs_cnt);
}

void * malloc(size_t asize)
{
    static void* (*real_malloc)(size_t) = NULL;
    if (!real_malloc) {
		// <the boring part actually wrapping malloc>
        real_malloc = dlsym(RTLD_NEXT, "malloc");
        
        // <initialize the malloc statistics>
        init_malloc_stats();
	}
	
	// inc hit counter
	pthread_mutex_lock(&mhs_mtx);
	if(!malloc_stats_gather(asize)) {
		// no enough capacity in stat mapping.
		malloc_stats_grow(asize);
	}
	pthread_mutex_unlock(&mhs_mtx);
	
	// <the boring part actually wrapping malloc>
    void *p = real_malloc(asize);
    return p;
}
