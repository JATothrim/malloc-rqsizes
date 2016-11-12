#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* stats_format */
static const size_t RECORD_SZ = sizeof(int)*2;

int main()
{
	FILE * f; 
	int record[2];
	
	printf("Reading malloc.stats ...\n");
	
	f = fopen("malloc.stats", "r");
	if(!f)
		exit(1);
	
	size_t cnt = 0;
	while(fread(record, RECORD_SZ, 1, f) == 1) {
		if(record[0] > 0) {
		printf("malloced-size: %d %d times\n", record[0], record[1]);
		++cnt;
		}
	}
	fclose(f);
	printf("total of %lu malloced-sizes\n", cnt);
	return 0;
}