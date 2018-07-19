//server_test.c
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
#define Content_Len 1024*1024 + 1
#define unsigned long long ull

struct pipe_pair
{
    char *input_pipe;
    char *output_pipe;
};

struct fd_pair
{
    int input_fd;
    int output_fd;
};

struct server_config
{
    char *mine_file;
    struct pipe_pair *pipes;
    int num_miners;
};
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

int main(int argc, char **argv){
	struct server_config config;
	load_config_file(&config, argv[1]);
	fprintf(stderr,"Finish loading file\n");
	struct fd_pair client_fds[config.num_miners];
    memset(client_fds, 0, sizeof(client_fds));
    for (int ind = 0; ind < config.num_miners; ind += 1){
        struct fd_pair *fd_ptr = &client_fds[ind];
        struct pipe_pair *pipe_ptr = &config.pipes[ind];
        
        //if(!pipe_ptr) fprintf(stderr,"pipe_ptr is NULL\n");
        //assert(!pipe_ptr);
        fprintf(stderr, "%s\n", pipe_ptr->input_pipe);
        fd_ptr->input_fd = open(pipe_ptr->input_pipe, O_RDONLY);
        fprintf(stderr,"fd_ptr->input_fd = %d\n",fd_ptr->input_fd);
        assert (fd_ptr->input_fd >= 0);

        fd_ptr->output_fd = open(pipe_ptr->output_pipe, O_WRONLY);
        assert (fd_ptr->output_fd >= 0);
    }
    fprintf(stderr,"Finish client_fds\n");
	return 0;
}