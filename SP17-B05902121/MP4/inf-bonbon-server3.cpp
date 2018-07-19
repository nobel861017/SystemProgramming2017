//inf-bonbon-server3.cpp
//#include <bits/stdc++.h>
#include <vector>
#include <iostream>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h> 
#include <fcntl.h>
#include <string>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include "cJSON.c"
#define MAX_CONNECT 2000

using namespace std;

typedef enum {
  TRY_MATCH_TWO_CLIENT = 0x01,
  MATCH_FAIL = 0x02,
  MATCH_SUCCESS = 0x03,
  MATCH_OTHER_CLIENT = 0x04,
} protocol_op;

typedef struct user{
	char name[33];
	unsigned int age;
	char gender[7];
	char introduction[1025];
} User;

typedef struct user_information{
    User user_inf;
    char filter[5000];
} USER_INF;

typedef struct protocol{
    int op;
    bool Is_matched;
    int fd1;
    int fd2;
    USER_INF c1;
    USER_INF c2;
} Protocol;

typedef struct assign_status{
    int status;
    int process_counter;
    int num_of_process;
    int matched_min_fd;
    int cur_idx;
    int Is_Last;
} Assign_Status;

vector < int > wait_list;
vector < int > try_match_list;
USER_INF user_info[MAX_CONNECT];
Assign_Status assign_client_stat[MAX_CONNECT];
int match_table[MAX_CONNECT];

void print_User(USER_INF ptr){
    fprintf(stderr,"name: %s\n", ptr.user_inf.name);
    fprintf(stderr,"age: %d\n", ptr.user_inf.age);
    fprintf(stderr,"gender: %s\n", ptr.user_inf.gender);
    fprintf(stderr,"introduction: %s\n", ptr.user_inf.introduction);
    fprintf(stderr, "filter_function: %s\n", ptr.filter);
}

int match_client(int fd1, int fd2, USER_INF c1, USER_INF c2){
    // make path    ./[ name + fd ].so
    char path1[256], path2[256];
    memset(path1, 0, sizeof(path1));
    memset(path2, 0, sizeof(path2));
    sprintf(path1, "./filter/%s%d.so",c1.user_inf.name, fd1);
    sprintf(path2, "./filter/%s%d.so",c2.user_inf.name, fd2);
    fprintf(stderr, "path1: %s\n", path1);
    fprintf(stderr, "path2: %s\n", path2);
    void *handle = dlopen( path2, RTLD_LAZY);
    //assert(NULL != handle);
    dlerror();
    int (*fil_func)(User) = (int(*)(User)) dlsym(handle, "filter_function");
    const char *dlsym_error = dlerror();
    if (dlsym_error){
        fprintf(stderr, "CRASH!!!!!: %s\n", dlsym_error);
        dlclose(handle);
        return 1;
    }
    int rt1 = fil_func(c1.user_inf);
    dlclose(handle);

    void *handle2 = dlopen( path1, RTLD_LAZY);
    //assert(NULL != handle2);
    dlerror();
    int (*fil_func2)(User) = (int(*)(User)) dlsym(handle2, "filter_function");
    dlsym_error = dlerror();
    if (dlsym_error){
        fprintf(stderr, "CRASH!!!!!: %s\n", dlsym_error);
        dlclose(handle);
        return 1;
    }
    int rt2 = fil_func2(c2.user_inf);
    dlclose(handle2);
    return rt1 && rt2;
}

void send_matched(int fd1, int fd2){
    cJSON *root;
    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "cmd", "matched");
    cJSON_AddStringToObject(root, "name", user_info[fd2].user_inf.name);
    cJSON_AddNumberToObject(root, "age", user_info[fd2].user_inf.age);
    cJSON_AddStringToObject(root, "gender", user_info[fd2].user_inf.gender);
    cJSON_AddStringToObject(root, "introduction", user_info[fd2].user_inf.introduction);
    cJSON_AddStringToObject(root, "filter_function", user_info[fd2].filter);
    char *send_cont = cJSON_PrintUnformatted(root);
    send(fd1, send_cont, strlen(send_cont), 0);
    send(fd1, "\n", 1, 0);
    fprintf(stderr, "Finish send \"matched\" and fd_info: %d to fd: %d\n",fd2, fd1);
}

Protocol get_message(int fd){
    Protocol message;
    memset(&message, 0, sizeof(message));
    read(fd, &message, sizeof(message));
    return message;
}

int pid[5];
void child_process(int idx){
    fprintf(stderr, "In child process: %d!!!!!!!!!!!!!!\n", idx);
    char in_path[10], out_path[10];
    memset(in_path, 0, sizeof(in_path));
    memset(out_path, 0, sizeof(out_path));
    sprintf(in_path, "Pipe/%d_in", idx);
    sprintf(out_path, "Pipe/%d_out", idx);
    int input_fd = open(in_path, O_RDONLY);
    fprintf(stderr, "child open Pipe/%d_in\n", idx);
    int output_fd = open(out_path, O_WRONLY);
    fprintf(stderr, "child open Pipe/%d_out\n", idx);
    fd_set rset;
    fd_set working_rset;
    FD_ZERO(&rset);
    FD_SET(input_fd, &rset);
    while(1){
        memcpy(&working_rset, &rset, sizeof(fd_set));
        select(input_fd + 1, &working_rset, NULL, NULL, NULL);
        Protocol message = get_message(input_fd);
        if(message.op == TRY_MATCH_TWO_CLIENT){
            fprintf(stderr,"child process name: %s\n", message.c1.user_inf.name);
            int rt = match_client(message.fd1, message.fd2, message.c1, message.c2);
            Protocol mes;
            mes.fd1 = message.fd1;
            mes.fd2 = message.fd2;
            mes.c1 = message.c1;
            mes.c2 = message.c2;
            if(rt){
                mes.op = MATCH_SUCCESS;
            }
            else{
                mes.op = MATCH_FAIL;
            }
            write(output_fd, &mes, sizeof(mes));
        }
    }
}
int input_fd[5], output_fd[5];
int process_empty = 1;
void assign_job(){
    fprintf(stderr, "try_match_list.size(): %d\n", try_match_list.size());
    if(try_match_list.size() == 0) return;
    vector< int > ::iterator ite;
    ite = try_match_list.begin();
    int fd = *ite;
    vector <int> ::iterator it;
    int counter = 0;
    if(process_empty){
        for(it = wait_list.begin() + assign_client_stat[fd].cur_idx; it < wait_list.end(); ++it){
            if(fd == *it){
                fprintf(stderr, "fd == *it -> continue\n");
                assign_client_stat[fd].cur_idx++;
                continue;
            }
            Protocol message;
            memset(&message, 0, sizeof(&message));
            message.op = TRY_MATCH_TWO_CLIENT;
            message.fd1 = fd;
            message.fd2 = *it;
            message.c1 = user_info[fd];
            fprintf(stderr, "assign func: %s\n",user_info[fd].user_inf.name);
            message.c2 = user_info[*it];
            fprintf(stderr, "assign func: %s\n",user_info[*it].user_inf.name);
            if(it == wait_list.end() - 1) assign_client_stat[fd].Is_Last = 1;
            write(output_fd[counter], &message, sizeof(message));
            counter++;
            assign_client_stat[fd].cur_idx++;
            if(counter == 5){
                break;
            }
        }
        process_empty = 0;
        assign_client_stat[fd].num_of_process = counter;
    }
}

void init_assign_struct(){
    for(int i = 0; i < MAX_CONNECT; i++){
        memset(&assign_client_stat[i], 0, sizeof(&assign_client_stat[i]));
        assign_client_stat[i].matched_min_fd = 10000;
        assign_client_stat[i].status = MATCH_FAIL;
    }
}

int main(int argc, char **argv){
    init_assign_struct();
    mkdir("Pipe", 0777);
    // 宣告 select() 使用的資料結構
    fd_set readset;
    fd_set working_readset;
    FD_ZERO(&readset);
    for(int i = 0; i < 5; i++){
        char in_path[12], out_path[12];
        memset(in_path, 0, sizeof(in_path));
        memset(out_path, 0, sizeof(out_path));
        sprintf(in_path, "Pipe/%d_in", i);
        sprintf(out_path, "Pipe/%d_out", i);
        mkfifo(in_path, 0644);
        mkfifo(out_path, 0644);
    }
    fprintf(stderr, "Finish making FIFO\n");
    for(int i = 0; i < 5; i++){
        pid[i] = fork();
        if(pid[i] == 0) child_process(i);
        else if(pid[i] > 0){
            char input_pipe[12], output_pipe[12];
            memset(input_pipe, 0, sizeof(input_pipe));
            memset(output_pipe, 0, sizeof(output_pipe));
            sprintf(input_pipe, "Pipe/%d_out", i);
            sprintf(output_pipe, "Pipe/%d_in", i);
            fprintf(stderr, "output_pipe: %s\n", output_pipe);
            fprintf(stderr, "input_pipe: %s\n", input_pipe);
            output_fd[i] = open(output_pipe, O_WRONLY);
            fprintf(stderr, "parent open Pipe/%d_in\n", i);
            input_fd[i] = open(input_pipe, O_RDONLY);
            fprintf(stderr, "parent open Pipe/%d_out\n", i);
            FD_SET(input_fd[i], &readset);
        }
    }
    for(int i = 0; i < 5; i++)
        fprintf(stderr, "pid[%d] = %d!!!!!!\n", i, pid[i]);
    fprintf(stderr, "Finish making child_process\n");
    /*
    for(int i = 0; i < 5; i++){
        char input_pipe[10], output_pipe[10];
        memset(input_pipe, 0, sizeof(input_pipe));
        memset(output_pipe, 0, sizeof(output_pipe));
        sprintf(input_pipe, "%d_out", i);
        sprintf(output_pipe, "%d_in", i);
        input_fd[i] = open(input_pipe, O_RDONLY);
        output_fd[i] = open(output_pipe, O_WRONLY);
        FD_SET(input_fd[i], &readset);
    }
    */
    fprintf(stderr, "Finish openning FIFO\n");
    memset(match_table, -1, sizeof(match_table));
    memset(&user_info, 0, sizeof(&user_info));
    //ios_base::sync_with_stdio(false);
    //cin.tie(0);
	// 宣告 socket 檔案描述子
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    // 利用 struct sockaddr_in 設定伺服器"自己"的位置
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // 清零初始化，不可省略
    server_addr.sin_family = PF_INET;      // 表示位置類型是網際網路位置
    server_addr.sin_addr.s_addr = INADDR_ANY; //// INADDR_ANY 是特殊的IP位置，表示接受所有外來的連線
    server_addr.sin_port = htons(atoi(argv[1]));

    // 使用 bind() 將伺服socket"綁定"到特定 IP 位置
    int retval;
    retval = bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    //assert(!retval);
    if(retval){
        printf("socket fail\n");
        return 0;
    }
    // 呼叫 listen() 告知作業系統這是一個伺服socket
    retval = listen(sockfd, MAX_CONNECT);
    assert(!retval);

	// 將 socket 檔案描述子放進 readset
	FD_SET(sockfd, &readset);

    mkdir("filter", 0777);
    while(1){
    	memcpy(&working_readset, &readset, sizeof(fd_set));
    	int retval = select(FD_SETSIZE, &working_readset, NULL, NULL, NULL);
    	if (retval == 0) continue;
    	if(retval > 0){
    		for (int fd = 0; fd < FD_SETSIZE; fd += 1){
    			// 排除沒有事件的描述子
        		if (!FD_ISSET(fd, &working_readset)) continue;
        		// 分成兩個情形：接受新連線用的 socket 和資料傳輸用的 socket
        		if (fd == sockfd){
		            // sockfd 有事件，表示有新連線
		            struct sockaddr_in client_addr;
		            socklen_t addrlen = sizeof(client_addr);
                    fprintf(stderr, "fd = %d\n",fd);
		            int c_fd = accept(fd, (struct sockaddr*) &client_addr, &addrlen);
                    fprintf(stderr,"There is a client. c_fd = %d\n",c_fd);
                    if(c_fd >= 0) FD_SET(c_fd, &readset); // 加入新創的描述子，用於和客戶端連線
		        }
		        else if(fd != input_fd[1] && fd != input_fd[2] && fd != input_fd[3] && fd != input_fd[4] && fd != input_fd[0] &&
                    fd != output_fd[1] && fd != output_fd[2] && fd != output_fd[3] && fd != output_fd[4] && fd != output_fd[0]){
		            // 這裏的描述子來自 accept() 回傳值，用於和客戶端連線
		            ssize_t sz;
		            char buffer[6000];
                    memset(buffer, 0, sizeof(buffer));
		            sz = recv(fd, buffer, sizeof(buffer), 0); // 接收資料
                    fprintf(stderr, "sz: %d!!!!!!!!!!!!!\n", sz);
                    fprintf(stderr, "buffer_content: %s!!!!!\n", buffer);
		            if(sz == 0 || sz == -1){	// client is off line
                        fprintf(stderr, "client is off line\n");
		                close(fd);
		                FD_CLR(fd, &readset);
                        // if client was talking, tell the other one to quit
                        if(match_table[fd] != -1){
                            cJSON *root_tmp;
                            root_tmp = cJSON_CreateObject();
                            cJSON_AddStringToObject(root_tmp, "cmd", "other_side_quit");
                            char *send_cont = cJSON_PrintUnformatted(root_tmp);
                            send(match_table[fd], send_cont, strlen(send_cont), 0);
                            send(match_table[fd], "\n", 1, 0);
                            match_table[match_table[fd]] = -1, match_table[fd] = -1;
                        }
                        memset(&user_info[fd], 0, sizeof(&user_info[fd]));
                        // remove client from waiting list
                        for(vector<int>::iterator ite = wait_list.begin(); ite != wait_list.end(); ++ite){
                            if(*ite == fd){
                                wait_list.erase(ite);
                                break;
                            }
                        }
		            }
		            else if(sz > 0){ // sz > 0，表示有新資料讀入
                        fprintf(stderr, "There is some data\n");
                        //fprintf(stderr, "buffer_content: %s!!!!!\n", buffer);
		                cJSON *root = cJSON_Parse(buffer);
                        //fprintf(stderr, "sizeof root: %d!!!!!!\n", sizeof(root));
                        cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "cmd");
                        //fprintf(stderr, "sizeof command: %d!!!!!!\n", sizeof(command));
                        //if(command->valuestring) fprintf(stderr, "Invalid string!!!!!!!!!\n");
                        char cmd[32];
                        memset(cmd, 0, sizeof(cmd));
                        //fprintf(stderr, "******%s\n", command->valuestring);
                        strcpy(cmd, command->valuestring);
                        if(strcmp(cmd, "try_match") == 0){
                            //try_match_list.push_back(fd);
                            // send "cmd": "try_match"
                            fprintf(stderr, "\"cmd\": \"try_match\"\n");
                            cJSON *root_tm;
                            root_tm = cJSON_CreateObject();
                            cJSON_AddStringToObject(root_tm, "cmd", "try_match");
                            char *send_cont = cJSON_PrintUnformatted(root_tm);
                            send(fd, send_cont, strlen(send_cont), 0);
                            send(fd, "\n", 1, 0);
                            fprintf(stderr, "Finish send \"cmd\": \"try_match\" to client\n");
                            // read rest of the content in JSON
                            cJSON *c_name = cJSON_GetObjectItemCaseSensitive(root, "name");
                            cJSON *c_age = cJSON_GetObjectItemCaseSensitive(root, "age");
                            cJSON *c_gender = cJSON_GetObjectItemCaseSensitive(root, "gender");
                            cJSON *c_introduction = cJSON_GetObjectItemCaseSensitive(root, "introduction");
                            cJSON *c_filter_function = cJSON_GetObjectItemCaseSensitive(root, "filter_function");
                            fprintf(stderr, "Finish getting JSON\n");
                            USER_INF tmp;
                            memset(&tmp, 0, sizeof(&tmp));
                            memset(&tmp.user_inf, 0, sizeof(&tmp.user_inf));
                            strcpy(tmp.user_inf.name, c_name->valuestring);
                            tmp.user_inf.age = c_age->valueint;
                            strcpy(tmp.user_inf.gender, c_gender->valuestring);
                            strcpy(tmp.user_inf.introduction, c_introduction->valuestring);
                            strcpy(tmp.filter, c_filter_function->valuestring);
                            fprintf(stderr, "=================\n");
                            print_User(tmp);
                            fprintf(stderr, "=================\n");
                            memset(&user_info[fd], 0, sizeof(&user_info[fd]));
                            user_info[fd] = tmp;

                            // make filter_function.c
                            char path[256];
                            memset(path, 0, sizeof(path));
                            strcpy(path, "filter/");
                            strcat(path, c_name->valuestring);
                            sprintf(path+strlen(path), "%d", fd); //path = name + fd
                            //fprintf(stderr, "path: %s\n", path);
                            char c_path[256];
                            memset(c_path, 0, sizeof(c_path));
                            strcat(c_path, path);
                            strcat(c_path, ".c");//c_path = name + fd .c
                            fprintf(stderr, "c_path: %s\n", c_path);
                            FILE *fp = fopen(c_path,"w+");
                            if(!fp) fprintf(stderr, "Open FILE ERROR\n");
                            char buf[5000];
                            memset(buf, 0, sizeof(buf));
                            strcpy(buf, "struct User{char name[33];unsigned int age;char gender[7];char introduction[1025];};\n");
                            strcat(buf, c_filter_function->valuestring);
                            fwrite(buf, 1, strlen(buf), fp);
                            fclose(fp);

                            char compile[256];
                            memset(compile, 0, sizeof(compile));
                            sprintf(compile, "gcc -fPIC -O2 -std=c11 %s.c -shared -o %s.so",path, path);
                            fprintf(stderr, "%s\n", compile);
                            //int ret = system(compile.c_str());
                            int ret = system(compile);
                            fprintf(stderr, "system_ret = %d\n", ret);
                            if(wait_list.size() == 0){  // wait_list empty, no one to match
                                fprintf(stderr, "wait_list is empty\n");
                                wait_list.push_back(fd); // put client into waiting list
                            }
                            else{
                                try_match_list.push_back(fd);
                            }
                            
                        }
                        else if(strcmp(cmd, "send_message") == 0){
                            fprintf(stderr, "\"cmd\": \"send_message\"\n");
                            cJSON *c_message = cJSON_GetObjectItemCaseSensitive(root, "message");
                            cJSON *c_sequence = cJSON_GetObjectItemCaseSensitive(root, "sequence");
                            cJSON *root_tm;
                            root_tm = cJSON_CreateObject();
                            cJSON_AddStringToObject(root_tm, "cmd", "send_message");
                            cJSON_AddStringToObject(root_tm, "message", c_message->valuestring);
                            cJSON_AddNumberToObject(root_tm, "sequence", c_sequence->valueint);
                            char *send_cont = cJSON_PrintUnformatted(root_tm);
                            send(fd, send_cont, strlen(send_cont), 0);
                            send(fd, "\n", 1, 0);
                            fprintf(stderr, "Finish send \"cmd\": \"send_message\" to client\n");
                            
                            cJSON *root_tmp;
                            root_tmp = cJSON_CreateObject();
                            cJSON_AddStringToObject(root_tmp, "cmd", "receive_message");
                            cJSON_AddStringToObject(root_tmp, "message", c_message->valuestring);
                            cJSON_AddNumberToObject(root_tmp, "sequence", c_sequence->valueint);
                            char *send_cont4 = cJSON_PrintUnformatted(root_tmp);
                            send(match_table[fd], send_cont4, strlen(send_cont4), 0);
                            send(match_table[fd], "\n", 1, 0);
                            fprintf(stderr, "Finish send \"cmd\": \"receive_message\" to client\n");
                        }
                        else if(strcmp(cmd, "quit") == 0){
                            for(vector<int>::iterator ite = wait_list.begin(); ite != wait_list.end(); ++ite){
                                if(*ite == fd){
                                    wait_list.erase(ite);
                                    break;
                                }
                            }
                            cJSON *root_tm;
                            root_tm = cJSON_CreateObject();
                            cJSON_AddStringToObject(root_tm, "cmd", "quit");
                            char *send_cont = cJSON_PrintUnformatted(root_tm);
                            send(fd, send_cont, strlen(send_cont), 0);
                            send(fd, "\n", 1, 0);
                            fprintf(stderr, "Finish send \"cmd\": \"quit\" to client\n");
                            memset(&user_info[fd], 0, sizeof(&user_info[fd]));
                            if(match_table[fd] != -1){
                                cJSON *root_tmp;
                                root_tmp = cJSON_CreateObject();
                                cJSON_AddStringToObject(root_tmp, "cmd", "other_side_quit");
                                char *send_cont3 = cJSON_PrintUnformatted(root_tmp);
                                send(match_table[fd], send_cont3, strlen(send_cont3), 0);
                                send(match_table[fd], "\n", 1, 0);
                                memset(&user_info[match_table[fd]], 0, sizeof(&user_info[match_table[fd]]));
                                match_table[match_table[fd]] = -1, match_table[fd] = -1;
                            }
                            
                        }
		            }
                    
        		}
                else if(fd != output_fd[1] && fd != output_fd[2] && fd != output_fd[3] && fd != output_fd[4] && fd != output_fd[0]){
                    // deal with pipe   
                    int in_pipe_fd;
                    // check which client is finished matching filter function
                    for(int i = 0; i < 5; i++){
                        if(fd == input_fd[i]){
                            in_pipe_fd = fd;
                            break;
                        }
                    }
                    Protocol message = get_message(in_pipe_fd);
                    /*
                    process_counter[message.fd1]++;
                    if(message.op == MATCH_SUCCESS){
                        if(matched_min_fd[message.fd1] > message.fd2) matched_min_fd[message.fd1] = message.fd2;
                    }
                    if(process_counter[message.fd1] == 5){
                        // child process matching finished
                        //set process_counter to 0
                        process_counter[message.fd1] = 0;
                        if(matched_min_fd[message.fd1] != 10000){
                            //  two client have matched, put them in match_table
                            match_table[message.fd1] = matched_min_fd[message.fd1];
                            match_table[message.fd2] = message.fd1;
                            matched_min_fd[message.fd1] = 10000;
                        }
                        else{
                            // client didn't match any of the five in wait_list, reassign job
                            assign_job();
                        }
                    }
                    */
                    int c_fd = message.fd1;
                    assign_client_stat[c_fd].process_counter++;
                    if(message.op == MATCH_SUCCESS){
                        assign_client_stat[c_fd].status = MATCH_SUCCESS;
                        if(assign_client_stat[c_fd].matched_min_fd > message.fd2)
                            assign_client_stat[c_fd].matched_min_fd = message.fd2;
                    }
                    if( assign_client_stat[c_fd].process_counter == assign_client_stat[c_fd].num_of_process){
                        // child process matching finished
                        if(assign_client_stat[c_fd].status = MATCH_SUCCESS){
                            //  two client have matched, put them in match_table
                            send_matched(c_fd, assign_client_stat[c_fd].matched_min_fd);
                            send_matched(assign_client_stat[c_fd].matched_min_fd, c_fd);
                            fprintf(stderr, "client %d match %d\n", c_fd,assign_client_stat[c_fd].matched_min_fd);
                            match_table[c_fd] = assign_client_stat[c_fd].matched_min_fd;
                            match_table[assign_client_stat[c_fd].matched_min_fd] = c_fd;
                            
                            // remove c_fd from try_match_list
                            for(vector<int>::iterator it = try_match_list.begin(); it != try_match_list.end(); ++it){
                                if(*it == c_fd){
                                    try_match_list.erase(it);
                                    break;
                                }
                            }
                            fprintf(stderr, "MATCH_SUCCESS, wait_list.size(): %d\n", wait_list.size());
                            for(vector<int>::iterator it = wait_list.begin(); it != wait_list.end(); ++it){
                                if(*it == assign_client_stat[c_fd].matched_min_fd){
                                    wait_list.erase(it);
                                    break;
                                }
                            }
                            fprintf(stderr, "MATCH_SUCCESS, wait_list.size(): %d\n", wait_list.size());
                            assign_client_stat[c_fd].process_counter = 0;
                            assign_client_stat[c_fd].matched_min_fd = 10000;
                            assign_client_stat[c_fd].cur_idx = 0;
                            assign_client_stat[c_fd].num_of_process = 0;
                            assign_client_stat[c_fd].status = MATCH_FAIL;
                        }
                        else if(assign_client_stat[c_fd].status = MATCH_FAIL && assign_client_stat[fd].Is_Last){
                            // client can't match any in the wait_list, add client to wait list, and take out from try_match_list
                            fprintf(stderr, "No match in wait_list, put in wait_list and take out from try_match_list\n");
                            wait_list.push_back(c_fd);
                            for(vector<int>::iterator it = try_match_list.begin(); it != try_match_list.end(); ++it){
                                if(*it == c_fd){
                                    try_match_list.erase(it);
                                    break;
                                }
                            }
                        }
                        process_empty = 1;
                    }
                }
    		}
    	}
    	// assign job
        assign_job();
        for(int i = 0; i < 5; i++)
            fprintf(stderr, "pid[%d] = %d!!!!!!\n", i, pid[i]);
        int flag = 0;
        for(int i = 0; i < 5; i++){
            int rtv = waitpid(pid[i], NULL, WNOHANG);
            fprintf(stderr, "rtv = %d!!!!!!!!\n", rtv);
            if(rtv == pid[i]){
                flag = 1;
                fprintf(stderr, "child: %d is dead\n");
                pid[i] = fork();
                if(pid[i] == 0) child_process(i);
            }
        }
        if(flag) fprintf(stderr, "End of while loop\n");
    }





	return 0;
}