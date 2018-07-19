//boss4.c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <unistd.h>
#include <openssl/md5.h>
#include "boss.h"
#define Content_Len 4000

typedef unsigned long long ull;

char best_md5[33],  best_mine[Content_Len];
int best_treasure = 0;

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

int load_config_file(struct server_config *config, char *path){
    FILE *fp = fopen(path,"r");
    int counter = 0;
    char *buf1 = malloc(PATH_MAX), *buf2 = malloc(PATH_MAX), *buf3 = malloc(PATH_MAX);
    config->pipes = malloc(sizeof(struct pipe_pair)*100);
    memset(buf1,0,PATH_MAX);
    memset(buf2,0,PATH_MAX);
    memset(buf3,0,PATH_MAX);
    fscanf(fp,"%s %s",buf1,buf2);

    config->mine_file = malloc(PATH_MAX);
    strcpy(config->mine_file, buf2);
    while(fscanf(fp,"%s %s %s",buf1,buf2,buf3) != EOF){
        config->pipes[counter].input_pipe = malloc(PATH_MAX);
        config->pipes[counter].output_pipe = malloc(PATH_MAX);
        strcpy(config->pipes[counter].input_pipe, buf3);
        strcpy(config->pipes[counter].output_pipe, buf2);
        counter++;
    }
    
    config->num_miners = counter;
    fclose(fp);
    free(buf1), free(buf2), free(buf3);
    return 0;
}

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

Protocol Get_one_Protocol_from_client(int input_fd){
    fprintf(stderr, "In Get_one_Protocol_from_server function\n");
    Protocol message;
    memset(&message, 0, sizeof(message));
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(input_fd, &readset);
    select(FD_SETSIZE, &readset, NULL, NULL, NULL);
    if(FD_ISSET(input_fd, &readset)){
        read(input_fd, &message, sizeof(message));
        fprintf(stderr, "finish read from %d\n", input_fd);
    }
    return message;
}
Protocol Get_multiple_Protocol_from_client(int input_fd, MD5_CTX miner_ans){
    fprintf(stderr, "In Get_multiple_Protocol_from_server function\n");
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10;
    fd_set readset;
    Protocol message;
    while(1){
        memset(&message, 0, sizeof(message));
        FD_ZERO(&readset);
        FD_SET(input_fd, &readset);
        if(!select(FD_SETSIZE, &readset, NULL, NULL, &timeout)) continue;
        if(FD_ISSET(input_fd, &readset)){
            fprintf(stderr, "GmPfc read\n");
            read(input_fd, &message, sizeof(message));
            fprintf(stderr, "finish read from %d\n", input_fd);
            if(message.op == STATUS){
                printf("I'm working on ");
                print_md5(miner_ans);
                continue;
            }
        }
        return message;
    }
}

void Tell_client_boss_start(struct fd_pair *client_fds, int n){
	for(int i = 0; i < n; i++){
		Protocol message;
		memset(&message, 0, sizeof(message));
		message.op = START_SERVER;
		write(client_fds[i].output_fd, &message, sizeof(message));
	}
}

int handle_command( char *cmd, struct server_config *config, char *path, struct fd_pair *client_fds){
    /* TODO parse user commands here */

    if (strcmp(cmd, "status")){
        /* TODO show status */
        printf("best %d-treasure %s in %d bytes\n", best_treasure, best_md5, strlen(best_mine));
        return 1;
    }
    else if (strcmp(cmd, "dump")){
        /* TODO write best n-treasure to specified file */
        int fd = open( path, O_RDONLY | O_CREAT);
        int fd_flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);
        write(fd, best_mine, strlen(best_mine));
        close(fd);
        return 2;
    }
    else{
        /* TODO tell clients to cease their jobs and exit normally */
        int fd;
        Protocol quit;
        memset(&quit, 0, sizeof(quit));
        quit.op = QUIT;
        for(int i = 0; i < config->num_miners; i++){
            //fd = open(config->pipes[i].output_pipe,O_WRONLY);
            write(client_fds[i].output_fd, &quit, sizeof(quit));
            //close(fd);
        }
        return 0;
    }
}

void assign_jobs(struct fd_pair *client_fds, int num_of_clients, int n_treasure, MD5_CTX *c){
	int range, pre = 96;
	range = 26/num_of_clients;
	for(int i = 0; i < num_of_clients; i++){
		Protocol assign;
		memset(&assign, 0, sizeof(assign));
		assign.n_treasure = n_treasure;
		assign.op = MINE;
		assign.c = *c;
		assign.start = pre + 1;
		assign.end = (i != (num_of_clients-1) ? pre + range : 122);
		pre = assign.end;
		write(client_fds[i].output_fd, &assign, sizeof(assign));
		//fprintf(stderr, "assign to %d\n", client_fds[i].output_fd);
	}
	return;
}

int Check_Prefix_Zeros(MD5_CTX miner_ans){
    unsigned char md5[MD5_DIGEST_LENGTH], md5_str[34];
    memset(md5, 0, sizeof(md5));
    memset(md5_str, 0, sizeof(md5_str));
    MD5_CTX tmp = miner_ans;
    MD5_Final(md5, &tmp);
    int count = 0;
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(md5_str+(i*2),(i != (MD5_DIGEST_LENGTH - 1)) ? "%02x":"%02x\0",md5[i]);
    for(int i = 0; i < 32; i++){
        if(md5_str[i] == '0') count++;
    	else break;
    }
    return count;
}

int main(int argc, char **argv){
    memset(best_md5, 0, sizeof(best_md5));
    memset(best_mine, 0, sizeof(best_mine));
    /* sanity check on arguments */
    if (argc != 2){
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    /* load config file */
    struct server_config config;
    load_config_file(&config, argv[1]);
    //fprintf(stderr,"Finish loading file\n");
    /* open the named pipes */
    struct fd_pair client_fds[config.num_miners];
    memset(client_fds, 0, sizeof(client_fds));
    for (int ind = 0; ind < config.num_miners; ind += 1){
        struct fd_pair *fd_ptr = &client_fds[ind];
        struct pipe_pair *pipe_ptr = &config.pipes[ind];
        
        fd_ptr->input_fd = open(pipe_ptr->input_pipe, O_RDONLY);
        fprintf(stderr,"fd_ptr->input_fd = %d\n",fd_ptr->input_fd);
        assert (fd_ptr->input_fd >= 0);
        
        fd_ptr->output_fd = open(pipe_ptr->output_pipe, O_WRONLY);
        fprintf(stderr,"fd_ptr->output_fd = %d\n",fd_ptr->output_fd);
        assert (fd_ptr->output_fd >= 0);
    }
    //fprintf(stderr,"Finish client_fds\n");
    /* initialize data for select() */
    int maxfd;
    fd_set readset;
    fd_set working_readset;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    // TODO add input pipes to readset, setup maxfd
    for(int i = 0; i < config.num_miners; i++){
        FD_SET(client_fds[i].input_fd, &readset);
        //maxfd = (maxfd < client_fds[i].input_fd ? client_fds[i].input_fd : maxfd);
    }
    //maxfd++;
    //fprintf( stderr,"maxfd = %d\n", maxfd - 1);
    //fprintf(stderr,"Tell_client_boss_started\n");
    Tell_client_boss_start(client_fds, config.num_miners);

    int flag = 1;
    int n_treasure;
    int len;
    MD5_CTX initial;
    MD5_Init(&initial);
    char buff[4096];
    FILE *fp = fopen(config.mine_file, "r");
    while( (len = fread(buff, 1, sizeof(buff), fp)) != 0 ){
    	//printf("initial mine: %s\nlen = %d\n",buff,len);
    	strcat(best_mine,buff);
    	MD5_Update(&initial, buff, strlen(buff));
    }
    fclose(fp);
    MD5_CTX tmpp = initial;
    n_treasure = Check_Prefix_Zeros(tmpp);
    fprintf(stderr,"n_treasure = %d\n",n_treasure);
    Protocol message[8];
    while (1){
    	if(flag){
    		fprintf(stderr,"in while(1) loop\n");
    		flag = 0;
    	}
    	assign_jobs(client_fds, config.num_miners, n_treasure, &initial);
    	//fprintf(stderr, "Finish assign_jobs\n");
    	memcpy(&working_readset, &readset, sizeof(readset));
    	int ret = select(FD_SETSIZE, &working_readset, NULL, NULL, NULL);
    	//fprintf(stderr, "ret = %d\n", ret);
    	if(ret){
	    	for(int i = 0; i < config.num_miners; i++){
	    		if(FD_ISSET(client_fds[i].input_fd, &working_readset)){
	    			memset(&message[i], 0, sizeof(message[i]));
	    			read(client_fds[i].input_fd, &message[i], sizeof(message[i]));
	        		//fprintf(stderr, "finish read from %d\n", client_fds[i].input_fd);
	    		}
	    	}
	    	for(int i = 0; i < config.num_miners; i++){
	    		if(FD_ISSET(client_fds[i].input_fd, &working_readset)){
	    			if(message[i].op == FOUND_TREASURE  && (message[i].n_treasure == (n_treasure)) ){
	    				fprintf(stderr, "%s FOUND_TREASURE\n", message[i].who_found_treasure);
	    				Protocol Found;
	                    memset(&Found, 0, sizeof(Protocol));
	                    Found.op = FOUND_TREASURE;
	                    Found.n_treasure = message[i].n_treasure;
	                    Found.strlength = message[i].strlength;
	                    strcpy(Found.who_found_treasure, message[i].who_found_treasure);
	                    //fprintf(stderr, "best_mine: %s\n", best_mine);
	                    for(int j = 0; j < config.num_miners; j++){
	                    	write(client_fds[j].output_fd, &Found, sizeof(Found));
	                    	//fprintf(stderr, "Finish write to %d\n", client_fds[j].output_fd);
	                    }
	                    //strcpy(Found.append_str, message[i].append_str);
	            		for(int j = 0; j < Found.strlength; j++){
				            Found.append_str[i] = message[i].append_str[j];
				        }
	                    Found.c = message[i].c;
	                    for(int j = 0; j < config.num_miners; j++){
	                    	write(client_fds[j].output_fd, &Found, sizeof(Found));
	                    	fprintf(stderr, "Finish write to %d\n", client_fds[j].output_fd);
	                    }
	                    strcat(best_mine, message[i].append_str);
	                    fprintf(stderr, "best_mine: %s\n", best_mine);
	                    fprintf(stderr,"Found.strlength = %d\nFound.append_str: %s\n",Found.strlength,Found.append_str);
	                    /*for(int j = 0; j < Found.strlength; j++){
		                    md5_add(&initial, (int)Found.append_str[i]);
		                }*/
		                
		                MD5_Init(&initial);
		                initial = Found.c;
		                MD5_CTX *tmp = &initial;
		                print_md5(*tmp);
	                    //MD5_Update(&initial, message[i].append_str, strlen(message[i].append_str));
	                    break;
	    			}
	    		}
	    	}
	    	n_treasure++;
	    }
    }

    /* TODO close file descriptors */

    return 0;
}
