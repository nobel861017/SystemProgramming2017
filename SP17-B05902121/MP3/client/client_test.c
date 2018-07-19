//client_test.c
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
int main(){
	char *name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);
    assert (!ret);

    ret = mkfifo(output_pipe, 0644);
    assert (!ret);
    
	return 0;
}