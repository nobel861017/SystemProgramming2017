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

/*typedef enum {
  STATUS = 0x00,
  QUIT = 0x01,
  FOUND_TREASURE = 0x02,
  START_FIND = 0x03,
  CLIENT_CANNOT_FIND_TREASURE = 0x04,
  //SERVER_START = 0x05,
  //CLIENT_READY = 0x06,
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
} Protocol;*/