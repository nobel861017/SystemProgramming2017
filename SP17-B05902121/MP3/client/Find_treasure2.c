//Find_treasure2.c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#define Content_Len 4000
int ind = 9;
typedef unsigned long long ull;
char str[4000];
void md5_add(MD5_CTX *miner_ans, int i){
    unsigned char str[1];
    memset(str, 0, sizeof(str));
    str[0] = (unsigned char)i;
    MD5_Update(miner_ans, str, 1);
    return;
}

void print_md5(MD5_CTX miner_ans){
    unsigned char md5[17];
    memset(md5, 0, sizeof(md5));
    MD5_Final(md5,&miner_ans);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        printf("%02x",md5[i]);
    }
    puts("");
}


void Check_Prefix_Zeros(MD5_CTX miner_ans, int *done, int n_treasure, int depth, char *mine_str){
    unsigned char md5[MD5_DIGEST_LENGTH], md5_str[34];
    memset(md5, 0, sizeof(md5));
    memset(md5_str, 0, sizeof(md5_str));
    MD5_CTX tmp = miner_ans;
    MD5_Final(md5, &tmp);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        sprintf(md5_str+(i*2),(i != (MD5_DIGEST_LENGTH - 1)) ? "%02x":"%02x\0",md5[i]);
    }
    int valid = 1;
    for(int i = 0; i < n_treasure; i++){
        if(md5_str[i] != '0') valid = 0;
    }
    if(md5_str[n_treasure] == '0') valid = 0;
    if(valid){
        *done = 1;
        printf("%s\n",md5_str);
        for(int i = 0; i < depth; i++){
        	str[ind] = mine_str[i];
        	ind++;
        }
        printf("str: %s\n",str);
        printf("Find %d-treasure!!!\n", n_treasure);
        FILE *fp = fopen("treasure", "w+");
        fwrite(str, 1, ind, fp);
        fclose(fp);
    }
    return;
}

void receive(int *done, MD5_CTX *miner_ans, int depth, char *mine_str, int idx, int n_treasure, int start, int end){
    if(*done == 1) return;
    if(idx == depth){
        Check_Prefix_Zeros(*miner_ans, done, n_treasure, depth, mine_str);
        return;
    }
    if(idx == 0){   //第一層
        for(int i = start; i <= end; i++){
            if(*done == 1) return;
            mine_str[idx] = (unsigned char)i;
            MD5_CTX tmp = *miner_ans;
            md5_add(&tmp, i);
            receive(done, &tmp, depth, mine_str, idx + 1, n_treasure, start, end);
        }
    }
    else{
        for(int i = 97; i < 123; i++){
            if(*done == 1) return;
            mine_str[idx] = (unsigned char)i;
            MD5_CTX tmp = *miner_ans;
            md5_add(&tmp, i);
            receive(done, &tmp, depth, mine_str, idx + 1, n_treasure, start, end);
        }
    }
    return;
}

void first_receive(MD5_CTX *miner_ans, int n_treasure, int start, int end){
    int depth = 1;
    int done = 0;
    unsigned char mine_str[4000];
    memset(mine_str, 0, sizeof(mine_str));
    while(!done){
        receive(&done, miner_ans, depth, mine_str, 0, n_treasure, start, end);
        depth++;
    }
}


int main(int argc, char **argv){
	int n_treasure = 1, start, end;
	//printf("n_treasure:");
	//scanf("%d", &n_treasure);
	printf("start:");
	scanf("%d",&start);
	printf("end:");
	scanf("%d",&end);
    MD5_CTX miner_ans;
    strcat(str,"B05902121");
    while(1){
    	MD5_Init(&miner_ans);
    	MD5_Update(&miner_ans, str, ind);
    	first_receive(&miner_ans, n_treasure++, start, end);
    }
    

    return 0;
}
