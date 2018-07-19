#include "csiebox_client.h"
#include "csiebox_common.h"
#include "connect.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <utime.h>
#include <time.h>
#include <sys/types.h>
#include <linux/inotify.h> //header for inotify

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#define Path_len_max 310

static int parse_arg(csiebox_client* client, int argc, char** argv);
static int login(csiebox_client* client);

char longest_path[Path_len_max];
int deepest_depth = -1;
int idx = 0;
typedef struct i_node{
    char path_name[Path_len_max];
    int i_node_num;
} I_node;

I_node i_nodes[Path_len_max];

char **event_path;

/*char load_file_to_string(char *path_name, int len){
    FILE *fp = fopen(path_name,"r+");
    char *buf = malloc(sizeof(char) * (len + 1));
    fread(buf, sizeof(char), len, fp);
    return buf;
}*/

int get_file_length(char *path_name){
    int fd, len;
    char buf[4096];
    fd = open(path_name, O_RDONLY);
    len = read(fd,buf,4096);
    close(fd);
    /*FILE *fp = fopen(path_name,"r+");
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    fclose(fp);*/
    return len;
}

void rm(char *path_name, csiebox_client *client){
    csiebox_protocol_rm rm;
    csiebox_protocol_header header;
    memset(&rm,0,sizeof(rm));
    memset(&header, 0, sizeof(header));
    rm.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    rm.message.header.req.op = CSIEBOX_PROTOCOL_OP_RM;
    rm.message.header.req.datalen = sizeof(rm) - sizeof(rm.message.header);
    rm.message.body.pathlen = strlen(path_name);
    send_message(client->conn_fd, &rm, sizeof(rm));
    send_message(client->conn_fd, path_name, strlen(path_name));
    recv_message(client->conn_fd, &header, sizeof(header));
}

void send_symbolic_link_to_server(char *path_name, csiebox_client *client){
    csiebox_protocol_file file;
    csiebox_protocol_header header;
    memset(&file,0,sizeof(file));
    memset(&header, 0, sizeof(header));
    int fd;
    char buf[Path_len_max];
    memset(buf,0,sizeof(buf));
    file.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    file.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
    file.message.header.req.datalen = sizeof(file) - sizeof(file.message.header);
    file.message.body.datalen = readlink(path_name, buf, Path_len_max);
    send_message(client->conn_fd, &file, sizeof(file));
    send_message(client->conn_fd, buf, file.message.body.datalen);
    recv_message(client->conn_fd, &header, sizeof(header));
}

void send_hardlink_to_server(char *source, char *target, const csiebox_client *client){
    csiebox_protocol_hardlink hardlink;
    memset(&hardlink,0,sizeof(hardlink));
    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
    hardlink.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    hardlink.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
    hardlink.message.header.req.datalen = sizeof(hardlink) - sizeof(hardlink.message.header);
    hardlink.message.body.srclen = strlen(source);
    hardlink.message.body.targetlen = strlen(target);
    send_message(client->conn_fd, &hardlink, sizeof(hardlink));
    send_message(client->conn_fd, source, strlen(source));
    send_message(client->conn_fd, target, strlen(target));
    recv_message(client->conn_fd, &header, sizeof(header));
    if(header.res.status == CSIEBOX_PROTOCOL_STATUS_OK){
        fprintf(stderr,"client: make hardlink success\n");
    }
}

int send_meta_to_server(char *path_name, const csiebox_client *client){
    csiebox_protocol_meta req;
    memset(&req, 0, sizeof(req));
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    req.message.body.pathlen = strlen(&path_name[2]);
    lstat(path_name, &(req.message.body.stat));

    if(S_ISREG(req.message.body.stat.st_mode)) md5_file(path_name,req.message.body.hash);
    else if(S_ISLNK(req.message.body.stat.st_mode)){   
        char buf[Path_len_max];
        int len;
        memset(buf,0,sizeof(buf));
        len = readlink(path_name,buf,Path_len_max);
        md5(buf, len, req.message.body.hash);
    }

    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
    send_message(client->conn_fd, &req, sizeof(req));
    send_message(client->conn_fd, &path_name[2], strlen(&path_name[2]));
    recv_message(client->conn_fd, &header, sizeof(header));
    if(header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE){
        return 2;
    }
    else{
        return 1;
    }
}

void send_file_to_server(char *path_name, csiebox_client *client){
    fprintf(stderr,"client: send file to server\n");
    csiebox_protocol_file file;
    csiebox_protocol_header header;
    memset(&file,0,sizeof(file));
    memset(&header, 0, sizeof(header));
    int fd;
    char buf[4096];
    fd = open(path_name, O_RDONLY);
    file.message.body.datalen = read(fd,buf,sizeof(buf));
    close(fd);
    file.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    file.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
    file.message.header.req.datalen = sizeof(file) - sizeof(file.message.header);
    send_message(client->conn_fd, &file, sizeof(file));
    send_message(client->conn_fd, buf, file.message.body.datalen);
    recv_message(client->conn_fd, &header, sizeof(header));
    if(header.res.status == CSIEBOX_PROTOCOL_STATUS_OK)
        fprintf(stderr,"client: server make file success\n");
}
int inotify_fd, wd;
void Find_Longest_Path(csiebox_client *client, char *name,int depth){
    DIR *dir;
    struct dirent *entry;
    char path_name[Path_len_max];
    if (!(dir = opendir(name))) return;
    while ((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        int tmp_depth = depth;
        sprintf(path_name, "%s/%s", name, entry->d_name);
        fprintf(stderr,"-----------\nclient: %s\ndepth: %d\n",path_name,depth);
        
        //int flag = send_meta_to_server(path_name, client);
        int flag2 = 0;
        if(entry->d_type == DT_DIR){
            wd = inotify_add_watch(inotify_fd, path_name, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY );
            event_path[wd] = malloc(Path_len_max);
            memset(event_path[wd],0,Path_len_max);
            strcpy(event_path[wd],path_name);

            flag2 = send_meta_to_server(path_name, client);
            fprintf(stderr,"client: flag2 = %d\n",flag2);
            fprintf(stderr,"client: entry->d_type == DT_DIR\n");
            if(depth > deepest_depth){
                deepest_depth = depth;
                fprintf(stderr,"client: longest_path_update\n");
                strcpy(longest_path,path_name);
            }
            Find_Longest_Path(client, path_name, depth + 1);
        }
        else{   //檔案 可能是reg 或者是 hardlink
            struct stat stat;
            lstat(path_name, &stat);
            tmp_depth++;
            if(tmp_depth > deepest_depth){
                deepest_depth = tmp_depth;
                fprintf(stderr,"client: longest_path_update\n");
                strcpy(longest_path,path_name);
            }
            csiebox_protocol_header header;
            memset(&header, 0, sizeof(header));
            
            //fprintf(stderr,"client: receive MORE from server\n");
            if(stat.st_nlink == 1 && !S_ISLNK(stat.st_mode)){   //reg FILE
                fprintf(stderr,"client: regular file no hardlink\n");
                flag2 = send_meta_to_server(path_name, client);
                fprintf(stderr,"client: flag2 = %d\n",flag2);
                if(flag2 == 2)
                    send_file_to_server(path_name, client);
                fprintf(stderr,"client: server make file success\n");
            }
            else if(S_ISREG(stat.st_mode)){
                int flag = 1;
                strcpy(i_nodes[idx].path_name,&path_name[2]);
                i_nodes[idx].i_node_num = stat.st_ino;
                fprintf(stderr,"idx = %d\n",idx);
                for(int i = 0;i < idx; i++){
                    if(i_nodes[i].i_node_num == stat.st_ino){
                        flag = 0;
                        fprintf(stderr,"client: hardlink\n");
                        send_hardlink_to_server(&path_name[2],i_nodes[i].path_name,client);
                        break;
                    }
                }
                idx++;
                if(flag){
                    fprintf(stderr,"client: stat.st_nlink != 1 && first appear\n");
                    flag2 = send_meta_to_server(path_name, client);
                    fprintf(stderr,"client: flag2 = %d\n",flag2);
                    if(flag2 == 2)
                        send_file_to_server(path_name, client);
                }
            }
            else if(S_ISLNK(stat.st_mode)){
                fprintf(stderr,"client: is soft link\n",flag2);
                flag2 = send_meta_to_server(path_name, client);
                fprintf(stderr,"client: flag2 = %d\n",flag2);
                if(flag2 == 2)
                    send_symbolic_link_to_server(path_name, client);
            }
            
        }
        
    }
    if(strcmp(".",name) != 0){
        int flag3 = send_meta_to_server(name, client);
    }
    closedir(dir);
}

//read config file, and connect to server
void csiebox_client_init(
  csiebox_client** client, int argc, char** argv) {
  csiebox_client* tmp = (csiebox_client*)malloc(sizeof(csiebox_client));
  if (!tmp) {
    fprintf(stderr, "client malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_client));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = client_start(tmp->arg.name, tmp->arg.server);
  if (fd < 0) {
    fprintf(stderr, "connect fail\n");
    free(tmp);
    return;
  }
  tmp->conn_fd = fd;
  *client = tmp;
}

//show how to use csiebox_protocol_meta header
//other headers is similar usage
//please check out include/common.h
//using .gitignore for example only for convenience  
/*int sampleFunction(csiebox_client* client){
  csiebox_protocol_meta req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  char path[] = "just/a/testing/path";
  req.message.body.pathlen = strlen(path);

  //just show how to use these function
  //Since there is no file at "just/a/testing/path"
  //I use ".gitignore" to replace with
  //In fact, it should be 
  //lstat(path, &req.message.body.stat);
  //md5_file(path, req.message.body.hash);
  lstat(".gitignore", &req.message.body.stat);
  md5_file(".gitignore", req.message.body.hash);


  //send pathlen to server so that server can know how many characters it should receive
  //Please go to check the samplefunction in server
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "send fail\n");
    return 0;
  }

  //send path
  //send_message(client->conn_fd, path, strlen(path));

  //receive csiebox_protocol_header from server
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      printf("Receive OK from server\n");
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}*/

//this is where client sends request, you sould write your code here
int csiebox_client_run(csiebox_client* client) {
    if (!login(client)) {
        fprintf(stderr, "login fail\n");
        return 0;
    }
    fprintf(stderr, "login success\n");
    
    //This is a sample function showing how to send data using defined header in common.h
    //You can remove it after you understand
    //sampleFunction(client); 
    
    //====================
    //        TODO
    //====================
    event_path = malloc(sizeof(char *) * Path_len_max);
    //create a instance and returns a file descriptor
    inotify_fd = inotify_init();
    if (inotify_fd < 0) perror("inotify_init");
    chdir(client->arg.path);
    wd = inotify_add_watch(inotify_fd, ".", IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY );
    event_path[wd] = malloc(Path_len_max);
    memset(event_path[wd],0,Path_len_max);
    strcpy(event_path[wd],".");
    fprintf(stderr,"start traverse!!!!!\n");
    Find_Longest_Path(client,".",0);
    fprintf(stderr,"finish traverse!!!!!\n");
    fprintf(stderr,"lstpath: %s\n",&longest_path[2]);
    FILE *fp = fopen("longestPath.txt","w+");
    fprintf(fp,"%s",&longest_path[2]);
    fclose(fp);

    // deal with longestpath.txt
    send_meta_to_server("./longestPath.txt", client);
    send_file_to_server("./longestPath.txt", client);
    
    //inotify_fd = inotify_init();
    int length, i = 0, type = 0;
    char buffer[EVENT_BUF_LEN];
    memset(buffer, 0, EVENT_BUF_LEN);
    fprintf(stderr,"start inotify monitor\n");
    //wd = inotify_add_watch(fd, ".", IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY );
    while ((length = read(inotify_fd, buffer, EVENT_BUF_LEN)) > 0) {
        i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            char path_name[Path_len_max];
            memset(path_name,0,Path_len_max);
            if(strcmp(event->name, "longestPath.txt") == 0){
                i += EVENT_SIZE + event->len;
                continue;
            }
            sprintf(path_name,"%s/%s",event_path[event->wd], event->name);
            if( event->mask & IN_ISDIR ){
                if (event->mask & IN_CREATE ) {
                    fprintf(stderr,"client: create dir %s\n",path_name);
                    wd = inotify_add_watch(inotify_fd, path_name, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY );
                    event_path[wd] = malloc(Path_len_max);
                    memset(event_path[wd],0,Path_len_max);
                    strcpy(event_path[wd], path_name);
                    //sync directory to server
                    type = send_meta_to_server(path_name, client);
                    if(type == 1) fprintf(stderr,"client: create dir %s success\n",path_name);
                }
                if (event->mask & IN_DELETE) {
                    fprintf(stderr,"delete dir %s\n",path_name);
                    //rm
                    rm(&path_name[2], client);
                }
                if (event->mask & IN_ATTRIB ) {
                    fprintf(stderr,"attrib dir %s\n",path_name);
                    //sync directory to server
                    type = send_meta_to_server(path_name, client);
                    if(type == 1) fprintf(stderr,"client: attrib dir %s success\n",path_name);
                }
                if (event->mask & IN_MODIFY) {
                    fprintf(stderr,"modify dir %s\n",path_name);
                    //sync directory to server
                    type = send_meta_to_server(path_name, client);
                    if(type == 1) fprintf(stderr,"client: modify dir %s success\n",path_name);
                }
            }
            else{
                if (event->mask & IN_CREATE) {
                    fprintf(stderr,"create file %s\n",path_name);
                    type = send_meta_to_server(path_name, client);
                    //sync file to server
                    if(type == 2){
                        send_file_to_server(path_name, client);
                        fprintf(stderr,"create file %s success\n",path_name);
                    }
                }
                if (event->mask & IN_DELETE) {
                    fprintf(stderr,"delete file %s\n",path_name);
                    //rm
                    rm(&path_name[2], client);
                }
                if (event->mask & IN_ATTRIB) {
                    fprintf(stderr,"attrib file %s\n",path_name);
                    type = send_meta_to_server(path_name, client);
                    //sync file to server
                    if(type == 2){
                        send_file_to_server(path_name, client);
                    }
                }
                if (event->mask & IN_MODIFY) {
                    fprintf(stderr,"modify file %s\n",path_name);
                    type = send_meta_to_server(path_name, client);
                    //sync file to server
                    if(type == 2){
                        send_file_to_server(path_name, client);
                        fprintf(stderr,"update file success\n");
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
        memset(buffer, 0, EVENT_BUF_LEN);
    }

    //inotify_rm_watch(fd, wd);
    close(inotify_fd);

    return 1;
}

void csiebox_client_destroy(csiebox_client** client) {
  csiebox_client* tmp = *client;
  *client = 0;
  if (!tmp) {
    return;
  }
  close(tmp->conn_fd);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_client* client, int argc, char** argv) {
  if (argc != 2) {
    return 0;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    return 0;
  }
  fprintf(stderr, "reading config...\n");
  size_t keysize = 20, valsize = 20;
  char* key = (char*)malloc(sizeof(char) * keysize);
  char* val = (char*)malloc(sizeof(char) * valsize);
  ssize_t keylen, vallen;
  int accept_config_total = 5;
  int accept_config[5] = {0, 0, 0, 0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("name", key) == 0) {
      if (vallen <= sizeof(client->arg.name)) {
        strncpy(client->arg.name, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("server", key) == 0) {
      if (vallen <= sizeof(client->arg.server)) {
        strncpy(client->arg.server, val, vallen);
        accept_config[1] = 1;
      }
    } else if (strcmp("user", key) == 0) {
      if (vallen <= sizeof(client->arg.user)) {
        strncpy(client->arg.user, val, vallen);
        accept_config[2] = 1;
      }
    } else if (strcmp("passwd", key) == 0) {
      if (vallen <= sizeof(client->arg.passwd)) {
        strncpy(client->arg.passwd, val, vallen);
        accept_config[3] = 1;
      }
    } else if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(client->arg.path)) {
        strncpy(client->arg.path, val, vallen);
        accept_config[4] = 1;
      }
    }
  }
  free(key);
  free(val);
  fclose(file);
  int i, test = 1;
  for (i = 0; i < accept_config_total; ++i) {
    test = test & accept_config[i];
  }
  if (!test) {
    fprintf(stderr, "config error\n");
    return 0;
  }
  return 1;
}

static int login(csiebox_client* client) {
  csiebox_protocol_login req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  memcpy(req.message.body.user, client->arg.user, strlen(client->arg.user));
  md5(client->arg.passwd,
      strlen(client->arg.passwd),
      req.message.body.passwd_hash);
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "send fail\n");
    return 0;
  }
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_LOGIN &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      client->client_id = header.res.client_id;
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}
