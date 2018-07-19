#include "csiebox_server.h"

#include "csiebox_common.h"
#include "connect.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>

static int parse_arg(csiebox_server* server, int argc, char** argv);
static void handle_request(csiebox_server* server, int conn_fd);
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info);
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login);
static void logout(csiebox_server* server, int conn_fd);
static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info);

#define DIR_S_FLAG (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)//permission you can use to create new file
#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory

/*char deal_with_path_name(csiebox_protocol_meta meta, csiebox_server* server,int conn_fd){
    //處理傳進來的path_name
    char path_name[4096], str[4096];
    memset(path_name,0,sizeof(path_name));
    memset(str,0,sizeof(str));
    recv_message(conn_fd, str, meta.message.body.pathlen);
    sprintf(path_name ,"%s/%s/%s",server->arg.path,server->client[conn_fd]->account.user,str);
    return path_name;
}*/

void rm_on_server(int conn_fd, csiebox_protocol_rm rm, csiebox_server *server){
    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
    header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
    header.res.op = CSIEBOX_PROTOCOL_OP_RM;
    header.res.datalen = 0;
    char path_name[310], tmp[310];
    memset(path_name, 0, sizeof(path_name));
    memset(tmp, 0, sizeof(tmp));
    recv_message(conn_fd, tmp, rm.message.body.pathlen);
    sprintf(path_name,"%s/%s/%s",server->arg.path,server->client[conn_fd]->account.user, tmp);
    fprintf(stderr,"rm %s\n",path_name);
    struct stat stat;
    memset(&stat, 0, sizeof(stat));
    lstat(path_name, &stat);
    if(S_ISDIR( stat.st_mode)) rmdir(path_name);
    else unlink(path_name);
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    send_message(conn_fd, &header, sizeof(header));
}

void sync_hardlink_to_server(csiebox_protocol_hardlink hardlink, int conn_fd, csiebox_server *server){
    int source_len = hardlink.message.body.srclen;
    int target_len = hardlink.message.body.targetlen;
    char source_partial_path_name[310], target_partial_path_name[310];
    memset(source_partial_path_name,0,sizeof(source_partial_path_name));
    memset(target_partial_path_name,0,sizeof(target_partial_path_name));
    char source_path_name[310], target_path_name[310];
    memset(source_path_name,0,sizeof(source_path_name));
    memset(target_path_name,0,sizeof(target_path_name));
    csiebox_protocol_header response;
    recv_message(conn_fd, source_partial_path_name, source_len);
    recv_message(conn_fd, target_partial_path_name, target_len);
    sprintf(source_path_name,"%s/%s/%s", server->arg.path, server->client[conn_fd]->account.user, source_partial_path_name);
    sprintf(target_path_name,"%s/%s/%s", server->arg.path, server->client[conn_fd]->account.user, target_partial_path_name);
    fprintf(stderr ,"source_path_name: %s\n",source_path_name);
    fprintf(stderr ,"target_path_name: %s\n",target_path_name);
    //target 是原本就有的檔案(target一定要存在於server)   要把 source 連到 target 的 i-node
    fprintf(stderr,"server: sync_hardlink_to_server\n");
    int ac1 = access(source_path_name,F_OK);
    int ac2 = access(target_path_name,F_OK);
    fprintf(stderr,"access(source_path_name,F_OK) == %d\naccess(target_path_name,F_OK) == %d\n",ac1,ac2);
    if(ac2 == -1){
        fprintf(stderr,"%s\n",strerror(access(target_path_name,F_OK)));
    }
    struct stat stat;
    /*if( access(source_path_name,F_OK) == 0 && access(target_path_name,F_OK) == 0 ){ 
        // source 如果原本存在
        fprintf(stderr,"server: source target both exist\n");
        unlink(source_path_name);
        link(target_path_name, source_path_name);
    }
    else if( access(source_path_name,F_OK) < 0 && access(target_path_name,F_OK) == 0 ){
        // source 不存在於server   把 source 連到 target 的 i-node
        fprintf(stderr,"server: source doesn't exist, target exist\n");
        link(target_path_name, source_path_name);
    }*/
    if(lstat(source_path_name, &stat) == 0){
        // source 如果原本存在
        fprintf(stderr,"server: source target both exist\n");
        unlink(source_path_name);
        link(target_path_name, source_path_name);
    }
    else{
        fprintf(stderr,"%d\n",lstat(target_path_name, &stat));
        // source 不存在於server   把 source 連到 target 的 i-node
        fprintf(stderr,"server: source doesn't exist, target exist\n");
        int suc = link(target_path_name, source_path_name);
        fprintf(stderr,"link(target_path_name, source_path_name) == %d\n",suc);
        if(suc == -1){
            fprintf(stderr,"%s\n",strerror(suc));
        }
    }
    response.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    response.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
    response.res.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
    response.res.datalen = 0;
    send_message(conn_fd, &response, sizeof(response));
}

void sync_soft_link_to_server(int conn_fd, const char *path_name){
    char buf[310];
    memset(buf,0,310);
    csiebox_protocol_file file;
    memset(&file,0,sizeof(file));
    csiebox_protocol_header response;
    memset(&response,0, sizeof(response));
    recv_message(conn_fd, &file,sizeof(file));
    int file_len = file.message.body.datalen;
    recv_message(conn_fd,buf,file_len);
    symlink(buf, path_name);
    response.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
    response.res.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
    response.res.datalen = 0;
    response.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    send_message(conn_fd, &response, sizeof(response));
}

void sync_file_to_server(int conn_fd, char *path_name){
    char buf[4096];
    memset(buf,0,4096);
    csiebox_protocol_file file;
    memset(&file,0,sizeof(file));
    csiebox_protocol_header response;
    memset(&response,0, sizeof(response));
    int fd = open(path_name,O_WRONLY | O_CREAT | O_TRUNC, REG_S_FLAG);
    recv_message(conn_fd, &file, sizeof(file));
    int file_len = file.message.body.datalen;
    recv_message(conn_fd, buf,file_len);
    write(fd, buf, file_len);
    close(fd);
    response.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
    response.res.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
    response.res.datalen = 0;
    response.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    fprintf(stderr,"server: make file\n");
    send_message(conn_fd, &response, sizeof(response));
}

void sync_meta_to_server(csiebox_protocol_meta req, int conn_fd, csiebox_server *server){
    csiebox_protocol_header response;
    memset(&response,0,sizeof(response));
    response.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
    response.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    response.res.datalen = 0;

    struct stat stat;
    char path_name[310];
    memset(path_name,0,sizeof(path_name));
    //strcpy(path_name,deal_with_path_name(meta,server,conn_fd));
    sprintf(path_name, "%s/%s/",server->arg.path,server->client[conn_fd]->account.user);
    recv_message(conn_fd, 
        path_name+strlen(server->arg.path)+strlen(server->client[conn_fd]->account.user)+2,
        req.message.body.pathlen);
    fprintf(stderr,"server_path: %s\n",path_name);
    if(lstat(path_name, &stat) < 0){ // file 或 directory 不存在
        if(S_ISDIR(req.message.body.stat.st_mode)){ //  這是directory, 建立好回傳OK給client
            fprintf(stderr,"server: dir doesn't exist\n");
            mkdir( path_name, req.message.body.stat.st_mode);
            response.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
            send_message(conn_fd, &response, sizeof(response));
        }
        else if(S_ISREG(req.message.body.stat.st_mode)){    //  這是regular file 建立好回傳More給client
            fprintf(stderr,"server: reg doesn't exist return MORE\n");
            response.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
            send_message(conn_fd, &response, sizeof(response));
            sync_file_to_server(conn_fd,path_name);
        }
        else if(S_ISLNK(req.message.body.stat.st_mode)){    //  這是symbolic link  回傳More給client
            response.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
            fprintf(stderr,"server: soft link return MORE\n");
            send_message(conn_fd, &response, sizeof(response));
            sync_soft_link_to_server(conn_fd,path_name);
        }
    }
    else{   //  file 或 directory 存在
        if(S_ISDIR(req.message.body.stat.st_mode)){ //  這是directory 回傳OK給client
            fprintf(stderr,"server: dir exist\n");
            // directory 沒有修改過的問題
            response.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
            send_message(conn_fd, &response, sizeof(response));
        }
        else if(S_ISREG(req.message.body.stat.st_mode)){    //regular file要檢查有沒有modifiy過
            char hash[35];
            md5_file(path_name, hash);
            if(strcmp(req.message.body.hash , hash) == 0){  //  檔案沒變 回傳OK給client
                response.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
                send_message(conn_fd,&response,sizeof(response));
            }
            else{   //  檔案有更動過  需要 sync_server_file
                response.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
                send_message(conn_fd, &response, sizeof(response));
                sync_file_to_server(conn_fd,path_name);
            }
        }
        else if(S_ISLNK(req.message.body.stat.st_mode)){    //  這是一個soft link
            char buffer[4096], hash[35];
            int len;
            memset(buffer,0,4096);
            memset(hash,0,35);
            md5(buffer,readlink( path_name, buffer, 4096),hash);
            if(strcmp(req.message.body.hash, hash) == 0){   //檔案內容沒變 回傳OK給client
                response.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
                send_message(conn_fd,&response,sizeof(response));
            }
            else{   //  檔案內容改變 回傳MORE給client
                response.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
                send_message(conn_fd, &response, sizeof(response));
                sync_soft_link_to_server(conn_fd,path_name);
            }
        }
    }
    struct timespec second[2];
    memset(&second,0,sizeof(second));
    second[0].tv_sec = req.message.body.stat.st_atime;
    second[1].tv_sec = req.message.body.stat.st_mtime;
    utimensat(AT_FDCWD,path_name,second,AT_SYMLINK_NOFOLLOW);
    if(!S_ISLNK(req.message.body.stat.st_mode)){       
        chmod(path_name,req.message.body.stat.st_mode);
    }
}

//read config file, and start to listen
void csiebox_server_init(
  csiebox_server** server, int argc, char** argv) {
  csiebox_server* tmp = (csiebox_server*)malloc(sizeof(csiebox_server));
  if (!tmp) {
    fprintf(stderr, "server malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_server));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = server_start();
  if (fd < 0) {
    fprintf(stderr, "server fail\n");
    free(tmp);
    return;
  }
  tmp->client = (csiebox_client_info**)
      malloc(sizeof(csiebox_client_info*) * getdtablesize());
  if (!tmp->client) {
    fprintf(stderr, "client list malloc fail\n");
    close(fd);
    free(tmp);
    return;
  }
  memset(tmp->client, 0, sizeof(csiebox_client_info*) * getdtablesize());
  tmp->listen_fd = fd;
  *server = tmp;
}

//wait client to connect and handle requests from connected socket fd
int csiebox_server_run(csiebox_server* server) {
  int conn_fd, conn_len;
  struct sockaddr_in addr;
  while (1) {
    memset(&addr, 0, sizeof(addr));
    conn_len = 0;
    // waiting client connect
    conn_fd = accept(
      server->listen_fd, (struct sockaddr*)&addr, (socklen_t*)&conn_len);
    if (conn_fd < 0) {
      if (errno == ENFILE) {
          fprintf(stderr, "out of file descriptor table\n");
          continue;
        } else if (errno == EAGAIN || errno == EINTR) {
          continue;
        } else {
          fprintf(stderr, "accept err\n");
          fprintf(stderr, "code: %s\n", strerror(errno));
          break;
        }
    }
    // handle request from connected socket fd
    handle_request(server, conn_fd);
  }
  return 1;
}

void csiebox_server_destroy(csiebox_server** server) {
  csiebox_server* tmp = *server;
  *server = 0;
  if (!tmp) {
    return;
  }
  close(tmp->listen_fd);
  int i = getdtablesize() - 1;
  for (; i >= 0; --i) {
    if (tmp->client[i]) {
      free(tmp->client[i]);
    }
  }
  free(tmp->client);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_server* server, int argc, char** argv) {
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
  int accept_config_total = 2;
  int accept_config[2] = {0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(server->arg.path)) {
        strncpy(server->arg.path, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("account_path", key) == 0) {
      if (vallen <= sizeof(server->arg.account_path)) {
        strncpy(server->arg.account_path, val, vallen);
        accept_config[1] = 1;
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

//It is a sample function
//you may remove it after understanding
/*int sampleFunction(int conn_fd, csiebox_protocol_meta* meta) {
  
  printf("In sampleFunction:\n");
  uint8_t hash[MD5_DIGEST_LENGTH];
  memset(&hash, 0, sizeof(hash));
  md5_file(".gitignore", hash);
  printf("pathlen: %d\n", meta->message.body.pathlen);
  if (memcmp(hash, meta->message.body.hash, strlen(hash)) == 0) {
    printf("hashes are equal!\n");
  }

  //use the pathlen from client to recv path 
  char buf[400];
  memset(buf, 0, sizeof(buf));
  recv_message(conn_fd, buf, meta->message.body.pathlen);
  printf("This is the path from client:%s\n", buf);

  //send OK to client
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  header.res.datalen = 0;
  header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
  if(!send_message(conn_fd, &header, sizeof(header))){
    fprintf(stderr, "send fail\n");
    return 0;
  }

  return 1;
}*/


//this is where the server handle requests, you should write your code here
static void handle_request(csiebox_server* server, int conn_fd) {
    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
    while (recv_message(conn_fd, &header, sizeof(header))) {
        if (header.req.magic != CSIEBOX_PROTOCOL_MAGIC_REQ) {
            continue;
        }
    switch (header.req.op) {
        case CSIEBOX_PROTOCOL_OP_LOGIN:
            fprintf(stderr, "login\n");
            csiebox_protocol_login req;
            if (complete_message_with_header(conn_fd, &header, &req)) {
                login(server, conn_fd, &req);
            }
            break;

        case CSIEBOX_PROTOCOL_OP_SYNC_META:
            fprintf(stderr, "sync meta\n");
            csiebox_protocol_meta meta;
            if (complete_message_with_header(conn_fd, &header, &meta)) {
                //This is a sample function showing how to send data using defined header in common.h
                //You can remove it after you understand
                //sampleFunction(conn_fd, &meta);
                //-------------TODO--------------
                sync_meta_to_server(meta, conn_fd, server);
            }
            break;

        /*case CSIEBOX_PROTOCOL_OP_SYNC_FILE:
            fprintf(stderr, "sync file\n");
            csiebox_protocol_file file;
            if (complete_message_with_header(conn_fd, &header, &file)) {
              //====================
              //        TODO
              //====================
                sync_file_to_server(conn_fd,path_name);
            }
            break;*/

        case CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK:
            fprintf(stderr, "sync hardlink\n");
            csiebox_protocol_hardlink hardlink;
            if (complete_message_with_header(conn_fd, &header, &hardlink)) {
                //--------------TODO-------------
                sync_hardlink_to_server(hardlink, conn_fd, server);
            }
            break;

        /*case CSIEBOX_PROTOCOL_OP_SYNC_END:
            fprintf(stderr, "sync end\n");
            csiebox_protocol_header end;
                //        TODO

            break;*/

        case CSIEBOX_PROTOCOL_OP_RM:
            fprintf(stderr, "rm\n");
            csiebox_protocol_rm rm;
            if (complete_message_with_header(conn_fd, &header, &rm)) {
                //        TODO
                rm_on_server(conn_fd, rm, server);
            }
            break;

        default:
            fprintf(stderr, "unknown op %x\n", header.req.op);
            break;
    }
  }
  fprintf(stderr, "end of connection\n");
  logout(server, conn_fd);
}

//open account file to get account information
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info) {
  FILE* file = fopen(server->arg.account_path, "r");
  if (!file) {
    return 0;
  }
  size_t buflen = 100;
  char* buf = (char*)malloc(sizeof(char) * buflen);
  memset(buf, 0, buflen);
  ssize_t len;
  int ret = 0;
  int line = 0;
  while ((len = getline(&buf, &buflen, file) - 1) > 0) {
    ++line;
    buf[len] = '\0';
    char* u = strtok(buf, ",");
    if (!u) {
      fprintf(stderr, "illegal form in account file, line %d\n", line);
      continue;
    }
    if (strcmp(user, u) == 0) {
      memcpy(info->user, user, strlen(user));
      char* passwd = strtok(NULL, ",");
      if (!passwd) {
        fprintf(stderr, "illegal form in account file, line %d\n", line);
        continue;
      }
      md5(passwd, strlen(passwd), info->passwd_hash);
      ret = 1;
      break;
    }
  }
  free(buf);
  fclose(file);
  return ret;
}

//handle the login request from client
static void login(
  csiebox_server* server, int conn_fd, csiebox_protocol_login* login) {
  int succ = 1;
  csiebox_client_info* info =
    (csiebox_client_info*)malloc(sizeof(csiebox_client_info));
  memset(info, 0, sizeof(csiebox_client_info));
  if (!get_account_info(server, login->message.body.user, &(info->account))) {
    fprintf(stderr, "cannot find account\n");
    succ = 0;
  }
  if (succ &&
      memcmp(login->message.body.passwd_hash,
             info->account.passwd_hash,
             MD5_DIGEST_LENGTH) != 0) {
    fprintf(stderr, "passwd miss match\n");
    succ = 0;
  }

  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  header.res.datalen = 0;
  if (succ) {
    if (server->client[conn_fd]) {
      free(server->client[conn_fd]);
    }
    info->conn_fd = conn_fd;
    server->client[conn_fd] = info;
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    header.res.client_id = info->conn_fd;
    char* homedir = get_user_homedir(server, info);
    mkdir(homedir, DIR_S_FLAG);
    free(homedir);
  } else {
    header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
    free(info);
  }
  send_message(conn_fd, &header, sizeof(header));
}

static void logout(csiebox_server* server, int conn_fd) {
  free(server->client[conn_fd]);
  server->client[conn_fd] = 0;
  close(conn_fd);
}

static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info) {
  char* ret = (char*)malloc(sizeof(char) * PATH_MAX);
  memset(ret, 0, PATH_MAX);
  sprintf(ret, "%s/%s", server->arg.path, info->account.user);
  return ret;
}

