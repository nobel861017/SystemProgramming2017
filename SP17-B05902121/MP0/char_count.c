//char_count.c
#include <stdio.h>
#include <string.h>
int main(int argc, char *argv[]){
	char c;
	FILE *fp;
	fp = fopen(argv[argc-1], "r");

	int len = strlen(argv[1]);
	if(fp == NULL && argc >= 3){
		printf("error\n");
		return 0;
	}
	
	if(argc == 2){
		int b[128] = {}, count = 0;
		while((c = getchar())){
			if(c != EOF) b[c]++;
			if(c == EOF) break;
			if(c == '\n' | c == EOF){
				for(int i=0;i<len;i++){
					count += b[argv[1][i]]; 
				}
				printf("%d\n",count);
				for(int i=0;i<128;i++) b[i] = 0;
				count = 0;
			}
		}
		return 0;
	}

	while(1){
		int a[128] = {}, counter = 0;
		while((c = fgetc(fp)) != '\n' && c != EOF){
			a[c]++;
		}
		if(c == '\n') a[c]++;
		if(c == EOF) break;
		for(int i=0;i<len;i++){
			counter += a[argv[1][i]]; 
		}
		printf("%d\n",counter);
	}
	fclose(fp);
	return 0;
}
