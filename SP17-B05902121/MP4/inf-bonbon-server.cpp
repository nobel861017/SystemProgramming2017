//inf-bonbon-server.cpp
//#include <bits/stdc++.h>
#include <vector>
#include <iostream>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
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

typedef struct user {
	char name[33];
	unsigned int age;
	char gender[7];
	char introduction[1025];
} User;

typedef struct user_information{
    User *user_inf;
    char filter[5000];
} USER_INF;

vector < int > wait_list;
USER_INF *user_info[MAX_CONNECT];
int match_table[1001];
void print_User(USER_INF *ptr){
    fprintf(stderr,"name: %s\n", ptr->user_inf->name);
    fprintf(stderr,"age: %d\n", ptr->user_inf->age);
    fprintf(stderr,"gender: %s\n", ptr->user_inf->gender);
    fprintf(stderr,"introduction: %s\n", ptr->user_inf->introduction);
    fprintf(stderr, "filter_function: %s\n", ptr->filter);
}


int match_client(int fd1, int fd2){
    // make path    ./[ name + fd ].so
    char path1[256], path2[256];
    memset(path1, 0, sizeof(path1));
    memset(path2, 0, sizeof(path2));
    sprintf(path1, "./filter/%s%d.so",user_info[fd1]->user_inf->name, fd1);
    sprintf(path2, "./filter/%s%d.so",user_info[fd2]->user_inf->name, fd2);
    fprintf(stderr, "path1: %s\n", path1);
    fprintf(stderr, "path2: %s\n", path2);
    void *handle = dlopen( path2, RTLD_LAZY);
    assert(NULL != handle);
    dlerror();
    int (*fil_func)(User) = (int(*)(User)) dlsym(handle, "filter_function");
    const char *dlsym_error = dlerror();
    if (dlsym_error){
        fprintf(stderr, "Cannot load symbol 'multiple': %s\n", dlsym_error);
        dlclose(handle);
        return 1;
    }
    int rt1 = fil_func(*user_info[fd1]->user_inf);
    dlclose(handle);

    void *handle2 = dlopen( path1, RTLD_LAZY);
    assert(NULL != handle2);
    dlerror();
    int (*fil_func2)(User) = (int(*)(User)) dlsym(handle2, "filter_function");
    int rt2 = fil_func2(*user_info[fd2]->user_inf);
    dlclose(handle2);
    return rt1 && rt2;
}

void send_matched(int fd1, int fd2){
    /*
    fprintf(stderr, "---------------\n");
    print_User(user_info[fd2]);
    fprintf(stderr, "---------------\n");
    */
    cJSON *root;
    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "cmd", "matched");
    cJSON_AddStringToObject(root, "name", user_info[fd2]->user_inf->name);
    cJSON_AddNumberToObject(root, "age", user_info[fd2]->user_inf->age);
    cJSON_AddStringToObject(root, "gender", user_info[fd2]->user_inf->gender);
    cJSON_AddStringToObject(root, "introduction", user_info[fd2]->user_inf->introduction);
    cJSON_AddStringToObject(root, "filter_function", user_info[fd2]->filter);
    char *send_cont = cJSON_PrintUnformatted(root);
    send(fd1, send_cont, strlen(send_cont), 0);
    send(fd1, "\n", 1, 0);
    fprintf(stderr, "Finish send \"matched\" and fd_info: %d to fd: %d\n",fd2, fd1);
}

int main(int argc, char **argv){
    memset(match_table, -1, sizeof(match_table));
    memset(user_info, 0, sizeof(user_info));
    ios_base::sync_with_stdio(false);
    cin.tie(0);
	// 宣告 socket 檔案描述子
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    // 利用 struct sockaddr_in 設定伺服器"自己"的位置
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // 清零初始化，不可省略
    server_addr.sin_family = PF_INET;      // 表示位置類型是網際網路位置
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // 連線目標 IP 是 127.0.0.1
    server_addr.sin_port = htons(atoi(argv[1]));

    // 使用 bind() 將伺服socket"綁定"到特定 IP 位置
    int retval;
    retval = bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    assert(!retval);
    // 呼叫 listen() 告知作業系統這是一個伺服socket
    retval = listen(sockfd, 1001);
    assert(!retval);

    // 宣告 select() 使用的資料結構
	fd_set readset;
	fd_set working_readset;
	FD_ZERO(&readset);

	// 將 socket 檔案描述子放進 readset
	FD_SET(sockfd, &readset);

    //vector < int > client_fd;
    
    //int client_fd[1001] = { }, idx = 0;
    //User *user_info[1001];
    mkdir("filter", 0777);
    while(1){
    	memcpy(&working_readset, &readset, sizeof(fd_set));
    	int retval = select(FD_SETSIZE, &working_readset, NULL, NULL, NULL);
    	if (retval == 0) continue;
    	if(retval > 0){
    		//while(client_fd[idx % 1000] == 0) idx++;
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
		        else{
		            // 這裏的描述子來自 accept() 回傳值，用於和客戶端連線
		            ssize_t sz;
		            char buffer[6000];
                    memset(buffer, 0, sizeof(buffer));
		            sz = recv(fd, buffer, sizeof(buffer), 0); // 接收資料
		            if(sz == 0){	// client is off line
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
		            else{ // sz > 0，表示有新資料讀入
                        fprintf(stderr, "There is some data\n");
		                cJSON *root = cJSON_Parse(buffer);
                        cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "cmd");
                        char cmd[32];
                        memset(cmd, 0, sizeof(cmd));
                        strcpy(cmd, command->valuestring);
                        if(strcmp(cmd, "try_match") == 0){
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
                            USER_INF *tmp = (USER_INF*)malloc(sizeof(USER_INF));
                            memset(tmp, 0, sizeof(tmp));
                            tmp->user_inf = (User*)malloc(sizeof(tmp->user_inf));
                            memset(tmp->user_inf, 0, sizeof(tmp->user_inf));
                            strcpy(tmp->user_inf->name, c_name->valuestring);
                            tmp->user_inf->age = c_age->valueint;
                            strcpy(tmp->user_inf->gender, c_gender->valuestring);
                            strcpy(tmp->user_inf->introduction, c_introduction->valuestring);
                            strcpy(tmp->filter, c_filter_function->valuestring);
                            //print_User(tmp);
                            memset(&user_info[fd], 0, sizeof(&user_info[fd]));
                            user_info[fd] = tmp;
                            //free(tmp->user_inf);
                            //free(tmp);
                            /*
                            fprintf(stderr, "\n###############\n");
                            print_User(user_info[fd]);
                            fprintf(stderr, "\n###############\n");
                            */
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
                            /*string compile = "gcc -fPIC -O2 -std=c11 ";
                            compile += path;
                            compile += ".c -shared -o ";
                            compile += path;
                            compile += ".so";
                            cout << compile << endl;*/
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
                                // match client
                                bool Is_matched = 0;
                                for(vector<int>::iterator it = wait_list.begin(); it < wait_list.end(); ++it){
                                    int rt = match_client(fd, *it);
                                    if(rt == 1){
                                        Is_matched = 1;
                                        // i and fd matched, remove them from wait_list and add them to match_table
                                        match_table[fd] = *it, match_table[*it] = fd;
                                        /*
                                        fprintf(stderr, "**************\n");
                                        print_User(user_info[fd]);
                                        print_User(user_info[*it]);
                                        fprintf(stderr, "**************\n");
                                        */
                                        send_matched(fd, *it);
                                        send_matched(*it, fd);
                                        fprintf(stderr, "client %d match %d\n", fd, *it);
                                        wait_list.erase(it);
                                        //wait_list.erase(remove(wait_list.begin(), wait_list.end(), fd));
                                        for(vector<int>::iterator ite = wait_list.begin(); ite != wait_list.end(); ++ite){
                                            if(*ite == fd){
                                                wait_list.erase(ite);
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                fprintf(stderr, "wait_list size: %d\n", wait_list.size());
                                if(!Is_matched) wait_list.push_back(fd); // put client into waiting list
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
    		}
    	}
    	

    }





	return 0;
}