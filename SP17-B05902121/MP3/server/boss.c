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
#include "boss.h"
#define Content_Len 1024*1024 + 1
#define unsigned long long ull

char best_md5[33],  best_mine[Content_Len];
int best_treasure = 0;

typedef enum {
  STATUS = 0x00,
  QUIT = 0x01,
  FOUND_TREASURE = 0x02,
  START_FIND = 0x03,
  CLIENT_CANNOT_FIND_TREASURE = 0x04,
} protocol_op;

typedef struct My_Protocol{
    char append_str[Content_Len];
    int max_append_num;
    int max_random_times;
    int op;
    int n_treasure;
    char who_found_treasure[PATH_MAX];
    char md5[33];
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
/*void check_config_file(struct server_config *config){
    printf("%s\n",config->mine_file);
    int n = config->num_miners;
    for(int i = 0; i < n; i++){
        printf("%s %s\n", config->pipes[i].input_pipe, config->pipes[i].output_pipe);
    }
    printf("counter = %d\n",n);
}*/
void assign_jobs(char *str, struct server_config *config, int n, int max_append_num, struct fd_pair *client_fds){
    /* TODO design your own (1) communication protocol (2) job assignment algorithm */
    fprintf(stderr,"In assign function\n");
    Protocol assign;
    memset(&assign, 0, sizeof(Protocol));
    assign.op = START_FIND;
    assign.n_treasure = n;
    assign.max_random_times = 10000;
    assign.max_append_num = max_append_num;
    strcpy(assign.append_str, str);
    for(int i = 0; i < config->num_miners; i++){
        //fd = open(config->pipes[i].output_pipe,O_WRONLY);
        fprintf(stderr, "Start assign at fd = %d\n",client_fds[i].output_fd);
        write(client_fds[i].output_fd, &assign, sizeof(assign));
        fprintf(stderr,"Finish assign at fd = %d\n",client_fds[i].output_fd);
        //close(fd);
    }
    return;
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
    fprintf(stderr,"Finish loading file\n");
    //check_config_file(&config);
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
    fprintf(stderr,"Finish client_fds\n");
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
        //int fd = open(config.pipes[i].input_pipe, O_RDONLY);
        //close(fd);
        FD_SET(client_fds[i].input_fd, &readset);
        maxfd = (maxfd < client_fds[i].input_fd ? client_fds[i].input_fd : maxfd);
    }
    maxfd++;
    fprintf( stderr,"maxfd = %d\n", maxfd - 1);
    /* assign jobs to clients */
    char str[Content_Len];
    memset(str, 0, sizeof(str));
    fprintf(stderr, "Start Read %s\n", config.mine_file);

    FILE *fp = fopen(config.mine_file, "r");
    fscanf(fp,"%s", str);
    fclose(fp);
    fprintf(stderr,"Start assign jobs\n");
    assign_jobs(str, &config, 1, 3, client_fds);

    while (1){
        memcpy(&working_readset, &readset, sizeof(readset));
        int ret = select(maxfd, &working_readset, NULL, NULL, NULL);
        fprintf(stderr, "ret = %d\n",ret);
        if (FD_ISSET(STDIN_FILENO, &working_readset)){
            /* TODO handle user input here */
            fprintf(stderr, "There is a command\n");
            char cmd[10], path[PATH_MAX];
            memset(cmd, 0, sizeof(cmd));
            memset(path, 0, sizeof(path));
            scanf("%s", cmd);
            if(strcmp(cmd, "dump")){
                scanf("%s", path);
            }
            int rt = handle_command(cmd, &config, path, client_fds);
            if(!rt) break;
        }

        /* TODO check if any client send me some message
           you may re-assign new jobs to clients*/
        for(int i = 0; i < config.num_miners; i++){
            if(FD_ISSET(client_fds[i].input_fd, &working_readset)){
                fprintf(stderr,"FD_ISSET: %d\n",client_fds[i].input_fd);
                Protocol message;
                memset(&message, 0, sizeof(Protocol));
                //open(config.pipes[i].input_pipe, O_RDONLY);
                read(client_fds[i].input_fd, &message, sizeof(message));
                //close(client_fds[i].input_fd);
                if(message.op == FOUND_TREASURE){
                    fprintf(stderr, "FOUND_TREASURE\n");
                    strcpy(best_md5, message.md5);
                    strcat(best_mine, message.append_str);
                    best_treasure = message.n_treasure;
                    Protocol Found;
                    memset(&Found, 0, sizeof(Protocol));
                    Found.op = FOUND_TREASURE;
                    Found.n_treasure = message.n_treasure;
                    strcpy(Found.who_found_treasure, message.who_found_treasure);
                    strcpy(Found.md5, message.md5);
                    for(int k = 0; k < config.num_miners; k++){
                        if(i == k) continue;
                        else{
                            fprintf(stderr, "write to %d\n",client_fds[k].output_fd);
                            //open(config.pipes[k].output_pipe, O_WRONLY);
                            write(client_fds[k].output_fd, &Found, sizeof(Found));
                            fprintf(stderr, "Finish write to %d\n",client_fds[k].output_fd);
                            //close(client_fds[k].output_fd);
                        }
                    }
                    if(best_treasure < 3){
                        assign_jobs(best_mine, &config, best_treasure + 1, 3, client_fds);
                        break;
                    }
                }
                else if(message.op == CLIENT_CANNOT_FIND_TREASURE){
                    assign_jobs(best_mine, &config, best_treasure, 3, client_fds);
                    break;
                }

            }
        }
    }

    /* TODO close file descriptors */

    return 0;
}
