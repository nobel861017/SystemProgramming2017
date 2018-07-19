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
typedef unsigned long long ull;
int quit_flag = 0;
typedef enum {
  QUIT = 0x01,
  FOUND_TREASURE = 0x02,
  START_SERVER = 0x03,
  CLIENT_CANNOT_FIND_TREASURE = 0x04,
  MINE = 0x05,
  STATUS = 0x06,
} protocol_op;

typedef struct My_Protocol{
    char append_str[Content_Len];
    int strlength;
    int op;
    int n_treasure;
    char who_found_treasure[PATH_MAX];
    char md5[33];
    ull start, end;
    MD5_CTX c;
} Protocol;

void md5_add(MD5_CTX *miner_ans, int i){
    unsigned char str[1];
    memset(str, 0, sizeof(str));
    str[0] = (unsigned char)i;
    MD5_Update(miner_ans, str, 1);
    return;
}

void print_md5(MD5_CTX miner_ans){
    //fprintf(stderr,"print_md5:");
    unsigned char md5[17];
    memset(md5, 0, sizeof(md5));
    MD5_Final(md5, &miner_ans);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        printf("%02x",md5[i]);
    }
    puts("");
}

Protocol Get_one_Protocol_from_server(int input_fd, struct timeval *timeout){
    //fprintf(stderr, "In Get_one_Protocol_from_server function\n");
    Protocol message;
    memset(&message, 0, sizeof(message));
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(input_fd, &readset);
    select(FD_SETSIZE, &readset, NULL, NULL, timeout);
    if(FD_ISSET(input_fd, &readset)){
        //printf("error\n");
        //fprintf(stderr, "start read\n");
        read(input_fd, &message, sizeof(message));
        //fprintf(stderr,"message.op = %d\n",message.op);
    }
    return message;
}

Protocol Get_multiple_Protocol_from_server(int input_fd, MD5_CTX miner_ans, int flag){
    //fprintf(stderr, "In Get_multiple_Protocol_from_server function\n");
    fd_set readset;
    Protocol message;
    while(1){
        memset(&message, 0, sizeof(message));
        FD_ZERO(&readset);
        FD_SET(input_fd, &readset);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10;
        if(select(FD_SETSIZE, &readset, NULL, NULL, &timeout) == 0) continue;
        if(FD_ISSET(input_fd, &readset)){
            //fprintf(stderr, "GmPfs read\n");
            read(input_fd, &message, sizeof(message));
            if(message.op == STATUS){
                //printf("GmPfs:");
                printf("I'm working on ");
                print_md5(miner_ans);
                continue;
            }
            if(flag){
                if(message.op == MINE) continue;
            }
        }
        return message;
    }
}

void Check_Prefix_Zeros(int input_fd, int output_fd, MD5_CTX miner_ans, int *done, int n_treasure, int depth, char *mine_str, char *name){
    //fprintf(stderr, "In Check_Prefix_Zeros function\n");
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
        //fprintf(stderr, "valid\n");
        *done = 1;
        Protocol Found;
        memset(&Found, 0, sizeof(Found));
        //fprintf(stderr, "append_str:");
        for(int i = 0; i < depth; i++){
            Found.append_str[i] = mine_str[i];
            //fprintf(stderr, "%c", mine_str[i]);
        }
        //fprintf(stderr, "\n");
        //fprintf(stderr, "append_str: %s\n", mine_str);
        strcpy(Found.md5, md5_str);
        Found.strlength = depth;
        Found.op = FOUND_TREASURE;
        Found.n_treasure = n_treasure;
        strcpy(Found.who_found_treasure, name);
        //MD5_CTX tmp2 = miner_ans;
        //print_md5(tmp2);
        Found.c = miner_ans;
        write(output_fd, &Found, sizeof(Found));
        //fprintf(stderr, "Told server I FOUND_TREASURE\n");
        Protocol message;
        memset(&message, 0, sizeof(message));
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10;
        message = Get_multiple_Protocol_from_server(input_fd, miner_ans, 1);
        //fprintf(stderr, "Got message from server\n");
        //printf("message.op = %d  job: %d\n",message.op,message.n_treasure);
        if( message.op == FOUND_TREASURE && strcmp(message.who_found_treasure, name) == 0 ){
            //printf("In check: ");
            printf("I win a %d-treasure! ", n_treasure);
            MD5_CTX tmp;
            MD5_Init(&tmp);
            tmp = message.c;
            print_md5(tmp);
        }
        else if(message.op == FOUND_TREASURE){
            //printf("In check: ");
            printf("%s wins a %d-treasure! ", message.who_found_treasure, message.n_treasure);
            MD5_CTX tmp;
            MD5_Init(&tmp);
            tmp = message.c;
            print_md5(tmp);
            return;
        }
        else if(message.op == QUIT){
            quit_flag = 1;
            *done = 1;
            printf("BOSS is at rest.\n");
        }
    }
}

void receive(int *done, MD5_CTX *miner_ans, int depth, char *mine_str, char *name, int input_fd, int output_fd, int idx, int start, int end, int n_treasure){
    //fprintf(stderr, "In receive function idx = %d\n",idx);
    Protocol message;
    memset(&message, 0, sizeof(message));
    if(*done == 1) return;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 60;
    message = Get_one_Protocol_from_server(input_fd, &timeout);
    if(message.op == STATUS){
        //fprintf(stderr, "message.op == STATUS\n");
        //printf("job %d: ",n_treasure);
        printf("I'm working on ");
        MD5_CTX tmp = *miner_ans;
        print_md5(tmp);
    }
    else if( message.op == FOUND_TREASURE && strcmp(message.who_found_treasure, name) == 0 ){
        //printf("job %d: ",n_treasure);
        printf("I win a %d-treasure! ", n_treasure);
        MD5_CTX tmp;
        MD5_Init(&tmp);
        tmp = message.c;
        print_md5(tmp);
        *done = 1;
    }
    else if(message.op == FOUND_TREASURE){
        //printf("job %d: ",n_treasure);
        printf("%s wins a %d-treasure! ", message.who_found_treasure, message.n_treasure);
        MD5_CTX tmp;
        MD5_Init(&tmp);
        tmp = message.c;
        print_md5(tmp);
        *done = 1;
    }
    else if(message.op == QUIT){
        *done = 1;
        quit_flag = 1;
        printf("BOSS is at rest.\n");
    }
    if(*done == 1) return;
    if(idx == depth){
        Check_Prefix_Zeros(input_fd, output_fd, *miner_ans, done, n_treasure, depth, mine_str, name);
        return;
    }
    if(idx == 0){   //第一層
        //ull start = message.start;
        //ull end = message.end;
        //printf("start = %llu end = %llu\n",start,end);
        for(int i = start; i <= end; i++){
            if(*done == 1) return;
            mine_str[idx] = (unsigned char)i;
            MD5_CTX tmp = *miner_ans;
            md5_add(&tmp, (int)i);
            receive(done, &tmp, depth, mine_str, name, input_fd, output_fd, idx + 1, start , end, n_treasure);
        }
    }
    else{
        for(int i = 0; i < 255; i++){
            if(*done == 1) return;
            mine_str[idx] = (unsigned char)i;
            MD5_CTX tmp = *miner_ans;
            md5_add(&tmp, (int)i);
            receive(done, &tmp, depth, mine_str, name, input_fd,output_fd, idx + 1, start , end, n_treasure);
        }
    }
    return;
}

void first_receive(MD5_CTX *miner_ans, char *name, int input_fd, int output_fd, int start, int end, int n_treasure){
    //fprintf(stderr, "In first_receive function\n");
    int depth = 1;
    int done = 0;
    unsigned char mine_str[4000];
    while(!done){
        receive(&done, miner_ans, depth, mine_str, name, input_fd, output_fd, 0, start , end, n_treasure);
        depth++;
    }
}

void BOSS_is_mindful(fd_set *readset, int input_fd){
    //fprintf(stderr, "In boss is mindful function\n");
    Protocol message;
    while(1){
        memset(&message, 0, sizeof(message));
        message = Get_one_Protocol_from_server(input_fd, NULL);
        if(message.op == START_SERVER){
            printf("BOSS is mindful.\n");
            break;
        }
        else{
            continue;
        }
    }
    return;
}

int main(int argc, char **argv){
    /* parse arguments */
    if (argc != 4){
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    char *name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);

    ret = mkfifo(output_pipe, 0644);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    /* TODO write your own (1) communication protocol (2) computation algorithm */
    int flag1 = 1, flag2 = 1;
    MD5_CTX miner_ans;
    while(1){
        quit_flag = 0;
        int output_fd = open(output_pipe, O_WRONLY);
        int input_fd = open(input_pipe, O_RDONLY);
        /*if(flag1){
            fprintf(stderr, "in first while(1) loop\n");
            flag1 = 0;
        }*/
        fd_set readset;
        fd_set working_readset;
        FD_ZERO(&readset);
        FD_SET(input_fd, &readset);
        BOSS_is_mindful(&readset, input_fd);
        while(1){
            Protocol dig_mine;
            memset(&dig_mine, 0, sizeof(dig_mine));
            dig_mine = Get_multiple_Protocol_from_server(input_fd, miner_ans, 0);
            if(dig_mine.op == MINE){
                MD5_Init(&miner_ans);
                //MD5_CTX tmp = dig_mine.c;
                miner_ans = dig_mine.c;
                //print_md5(tmp);
                first_receive(&miner_ans, name, input_fd, output_fd, dig_mine.start, dig_mine.end, dig_mine.n_treasure);
            }
            else if(dig_mine.op == QUIT){
                quit_flag = 1;
                printf("BOSS is at rest.\n");
            }
            if(quit_flag){
                //fprintf(stderr,"quit_flag == 1\n");
                //flag1 = 1;
                close(input_fd);
                close(output_fd);
                break;
            }
        }
    }
    /* To prevent from blocked read on input_fd, try select() or non-blocking I/O */

    return 0;
}
