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

//char name[PATH_MAX];
typedef enum {
  STATUS = 0x00,
  QUIT = 0x01,
  FOUND_TREASURE = 0x02,
  START_FIND = 0x03,
  CLIENT_CANNOT_FIND_TREASURE = 0x04,
  CONTINUE_WORK = 0x05,
} protocol_op;

typedef struct My_Protocol{
    char append_str[Content_Len];
    int max_append_num;
    int max_random_times;
    int op;
    int n_treasure;
    char who_found_treasure[PATH_MAX];
    char md5[33];
    //unsigned long long start, end;
} Protocol;

typedef unsigned long long ull;

char cur_md5[PATH_MAX];
int server_start = 1;

int MD5_CHECK(char *append, char *str, int max_treasure_num, int output_fd, int input_fd, char *name, struct timeval *timeout,fd_set *working_readset,fd_set *readset){
    //fprintf(stderr, "mine str: %s\n", str);
    int count = 0;
    MD5_CTX c;
    unsigned char md5[17], md5_str[34];
    memset(md5, 0, sizeof(md5));
    memset(md5_str, 0, sizeof(md5_str));
    MD5_Init(&c);
    MD5_Update(&c, str, strlen(str));
    MD5_Final(md5,&c);
    for(int i = 0; i < 16; i++){
        //printf(i != 15 ? "%02x":"%02x\n",md5[i]);
        sprintf(md5_str+(i*2),i != 15 ? "%02x":"%02x\0",md5[i]);
        //fprintf(stderr,i != 15 ? "%02x ":"%02x\n",md5[i]);
    }
    strcpy(cur_md5, md5_str);
    for(int i = 0; i < 32; i++){
        if(md5_str[i] == '0'){
            count++;
            if((count == max_treasure_num) && (md5_str[i+1] != '0')){
                Protocol Found;
                memset(&Found, 0, sizeof(Protocol));
                Found.op = FOUND_TREASURE;
                strcpy(Found.who_found_treasure, name);
                strcpy(Found.append_str, append);
                Found.n_treasure = max_treasure_num;
                strcpy(Found.md5, md5_str);
                //open(input_pipe, O_WRONLY);

                fprintf(stderr, "Start write FOUND_TREASURE\n");
                write(output_fd, &Found, sizeof(Found));
                fprintf(stderr, "Finish write FOUND_TREASURE\n");
                //close(output_fd);
                //memcpy(&working_readset, &readset, sizeof(readset));
                FD_SET(input_fd, working_readset);
                int ret = select(input_fd + 1, working_readset,  NULL, NULL, NULL);
                fprintf(stderr, "md5_ret = %d\n", ret);
                if(ret){
                    Protocol message;
                    memset(&message, 0, sizeof(Protocol));
                    read( input_fd, &message, sizeof(message));
                    fprintf(stderr, "op = %d\n", message.op);
                    if(message.op == CONTINUE_WORK){
                        printf("I win a %d-treasure! %s\n", max_treasure_num, md5_str);
                    }
                    else return 1;
                }
                
                return 1;
            }
        }
        else return 0;
    }
}
void Find_treasure(Protocol *message, int output_fd, int input_fd, fd_set *working_readset, struct timeval *timeout, char *name, fd_set *readset){
    srand( time(NULL) );
    char mine_str[Content_Len];
    memset(mine_str, 0, sizeof(mine_str));
    strcpy(mine_str, message->append_str);
    //fprintf(stderr,"Find_treasure : %s\n", mine_str);
    for(int i = 0; i < message->max_append_num;i++){
        ull max_j;
        //max_j = (i < 3 ? pow(((ull)256),i+1) : message->max_random_times);
        max_j = message->max_random_times;
        memcpy(&working_readset, &readset, sizeof(readset));
        int ret = select(input_fd + 1, working_readset,  NULL, NULL, timeout);
        fprintf(stderr, "ret = %d\n", ret);
        if(!ret){
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
                //fprintf(stderr, "mine_str: %s\n",tmp_str);
                int flags = MD5_CHECK(append , tmp_str, message->n_treasure, output_fd, input_fd, name, timeout, working_readset, readset);
                if(!flags){
                    //strcat(mine_str, append);
                    continue;
                }
                else if(flags == 1) return;
            }
        }
        else{
            Protocol message;
            memset(&message, 0, sizeof(Protocol));
            //open(input_pipe, O_RDONLY);
            read(input_fd, &message, sizeof(message));
            //close(input_fd);
            if(message.op == FOUND_TREASURE){
                printf("%s wins a %d-treasure! %s\n", message.who_found_treasure, message.n_treasure, message.md5);
                return;
            }
            else if(message.op == STATUS){
                printf("I'm working on %s\n", cur_md5);
                i--;
                continue;
            }
            else if(message.op == QUIT){
                printf("BOSS is at rest.\n");
                server_start = 1;
                return;
            }
        }
    }
    Protocol NO_FIND;
    memset(&NO_FIND, 0, sizeof(Protocol));
    NO_FIND.op = CLIENT_CANNOT_FIND_TREASURE;
    //open(output_pipe, O_WRONLY);
    write(output_fd, &NO_FIND, sizeof(NO_FIND));
    //close(output_fd);
    return;
}

int main(int argc, char **argv){
    memset(cur_md5, 0, sizeof(cur_md5));
    //memset(name, 0, sizeof(name));
    /* parse arguments */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    char *name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);
    assert (!ret);

    ret = mkfifo(output_pipe, 0644);
    assert (!ret);

    /* open pipes */
    int output_fd = open(output_pipe, O_WRONLY);
    assert (output_fd >= 0);
    fprintf(stderr, "%s: %d\n",output_pipe,output_fd);
    int input_fd = open(input_pipe, O_RDONLY);
    assert (input_fd >= 0);
    fprintf(stderr, "%s: %d\n",input_pipe,input_fd);
    //close(input_fd);
    
    fprintf(stderr, "Finish PIPE open\n");
    //close(output_fd);
    int maxfd;
    fd_set readset;
    fd_set working_readset;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 60;

    FD_ZERO(&readset);
    FD_SET(input_fd, &readset);
    //memcpy(&working_readset, &readset, sizeof(readset));
    /* TODO write your own (1) communication protocol (2) computation algorithm */
    int flag = 1;
    while(1){
        if(flag){
            fprintf(stderr, "Start Select\n");
            flag = 0;
        }
        //fprintf(stderr, "initial ret = %d\n",ret);
        memcpy(&working_readset, &readset, sizeof(readset));
        ret = select(input_fd + 1, &working_readset,  NULL, NULL, NULL);
        fprintf(stderr, "while(1)_ret = %d\n",ret);
        //fprintf(stderr, "ret = %d\n",ret);
        if(ret < 0) fprintf(stderr, "client: %s select error\n", name);
        else if(ret > 0){
            Protocol message;
            memset(&message, 0, sizeof(Protocol));
            //open(input_pipe, O_RDONLY);
            read(input_fd, &message, sizeof(message));
            //close(input_fd);
            switch(message.op){
                case START_FIND:
                    if(server_start){
                        printf("BOSS is mindful.\n");
                        server_start = 0;
                    }
                    Find_treasure(&message, output_fd, input_fd, &working_readset, &timeout, name, &readset);
                    break;
                /*case STATUS:
                    printf("I'm working on %s\n", cur_md5);
                    break;
                case QUIT:
                    printf("BOSS is at rest.\n");
                    server_start = 1;
                */
            }
        }
        
    }
    /* To prevent from blocked read on input_fd, try select() or non-blocking I/O */

    return 0;
}
