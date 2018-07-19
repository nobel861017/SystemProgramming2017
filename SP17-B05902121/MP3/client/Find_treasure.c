//Find_treasure.c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <string.h>
#include <time.h>
#define Content_Len 1024*1024 + 1
char mine[Content_Len], mine_md5[33];
int flag = 0, max_found_treasure = 0;
int Find_treasure(char *str, int max_treasure_num){
	int count = 0;
	MD5_CTX c;
    unsigned char md5[17], md5_str[33];
    memset(md5, 0, sizeof(md5));
    memset(md5_str, 0, sizeof(md5_str));
    MD5_Init(&c);
    MD5_Update(&c, str, strlen(str));
    MD5_Final(md5,&c);
    for(int i = 0; i < 16; i++){
    	//printf(i != 15 ? "%02x":"%02x\n",md5[i]);
    	sprintf(md5_str+(i*2),i != 15 ? "%02x":"%02x\0",md5[i]);
    }
    //printf("%s\n",md5_str);
    for(int i = 0; i < 32; i++){
    	if(md5_str[i] == '0'){
    		count++;
    		if((count == (max_found_treasure + 1)) && (count <= max_treasure_num) && (md5_str[i+1] != '0')){
    			max_found_treasure = count;
    			strcpy( mine_md5, md5_str);
    			strcpy( mine, str);
    			flag = 1;
    			if(count < max_treasure_num) return 1;
    			else return 2;
    		}
    	}
    	else return 0;
    }
}

int main(){
	char mine_str[Content_Len];
    memset(mine_str, 0, sizeof(mine_str));
    memset(mine, 0 , sizeof(mine));
    memset(mine_md5, 0 , sizeof(mine_md5));
    int n, max_j, max_append_num;
    printf("insert max_treasure_num:");
    scanf("%d",&n);
    printf("insert max_append_num:");
    scanf("%d",&max_append_num);
    printf("insert max_j:");
    scanf("%d",&max_j);
    srand( time(NULL) );
    strcpy(mine_str, "B05902121");
    int flag2 = 0;
    for(int i = 0; i < max_append_num;i++){
    	for(int j = 0;j < max_j;j++){
    		char append[Content_Len], tmp_str[Content_Len];
    		memset(append, 0, sizeof(append));
    		memset(tmp_str, 0, sizeof(tmp_str));
	    	for(int k = 0; k <= i; k++){
	    		int x = rand() % 256;
	    		sprintf(append+(k*4),"\\x%02x", x);
	    	}
	    	strcpy(tmp_str,mine_str);
	    	strcat(tmp_str, append);
	    	//printf("%s\n",tmp_str);
	    	int flags = Find_treasure(tmp_str,n);
	    	if(flags == 1){
	    		strcat(mine_str, append);
	    		i = 0;
	    		break;
	    		printf("%s\n",mine_str);
	    	}
	    	else if(flags == 2){
	    		strcat(mine_str, append);
	    		flag2 = 1;
	    		printf("%s\n",mine_str);
	    		break;
	    	}
	    	else continue;
    	}
    	if(flag2) break;
    }
    if(!flag){
    	printf("Can't find treasure.\n");
    }
    else{
    	printf("max_found_treasure: %d\n",max_found_treasure);
    	printf("mine = %s\n",mine);
    	printf("md5: %s\n",mine_md5);
    }
	return 0;
}