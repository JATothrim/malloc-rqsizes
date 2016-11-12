#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

void * work_thread(void*run) {
	
	int g = 1;
	srand(clock());
	while(g < 30) {
		printf("loop %d\n", g);
		for(size_t i = 1; i < 4096*2; ++i) {
			free(malloc(rand() % i + 1));
		}
		++g;
		
	}
}
	
// Test the malloc_stats preload wrapper in more forgiving way.
int main()
{
	char * str = malloc(128);
	int g = 1;
	strcpy(str, "malloc_stats testing..!\n");
	printf(str);
	free(str);
	
	pthread_t thrid;
	int run = 1;
	pthread_create(&thrid, NULL, work_thread, &run);
	
	work_thread(0);
	
	pthread_join(thrid, NULL);
	
	return 0;
}