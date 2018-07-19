#include "list_file.h"
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#define DEFAULT_BUFFER_SIZE 128

char last_filename[1024][270], last_file_MD5[1024][270], now_file_MD5[1024][40];
int last_filename_num;
int cmp(const void *a,const void *b){
    return strcmp( *(const char **)a,*(const char **)b );
}
typedef union uwb {
    unsigned w;
    unsigned char b[4];
} MD5union;
typedef unsigned DigestArray[4];
unsigned func0( unsigned abcd[] ){return ( abcd[1] & abcd[2]) | (~abcd[1] & abcd[3]);}
unsigned func1( unsigned abcd[] ){return ( abcd[3] & abcd[1]) | (~abcd[3] & abcd[2]);}
unsigned func2( unsigned abcd[] ){return  abcd[1] ^ abcd[2] ^ abcd[3];}
unsigned func3( unsigned abcd[] ){return abcd[2] ^ (abcd[1] |~ abcd[3]);}
typedef unsigned (*DgstFctn)(unsigned a[]);
unsigned *calctable( unsigned *k){
    double s, pwr;
    int i;
    pwr = pow( 2, 32);
    for (i=0; i<64; i++) {
        s = fabs(sin(1+i));
        k[i] = (unsigned)( s * pwr );
    }
    return k;
}
unsigned rol( unsigned r, short N ){
    unsigned  mask1 = (1<<N) -1;
    return ((r>>(32-N)) & mask1) | ((r<<N) & ~mask1);
}
unsigned *md5( char *msg, int mlen){
    /*Initialize Digest Array as A , B, C, D */
    static DigestArray h0 = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476 };
    static DgstFctn ff[] = { &func0, &func1, &func2, &func3 };
    static short M[] = { 1, 5, 3, 7 };
    static short O[] = { 0, 1, 5, 0 };
    static short rot0[] = { 7,12,17,22};
    static short rot1[] = { 5, 9,14,20};
    static short rot2[] = { 4,11,16,23};
    static short rot3[] = { 6,10,15,21};
    static short *rots[] = {rot0, rot1, rot2, rot3 };
    static unsigned kspace[64];
    static unsigned *k;
    static DigestArray h;
    DigestArray abcd;
    DgstFctn fctn;
    short m, o, g;
    unsigned f;
    short *rotn;
    union {
        unsigned w[16];
        char     b[64];
    }mm;
    int os = 0;
    int grp, grps, q, p;
    unsigned char *msg2;
    if (k==NULL) k= calctable(kspace);
    for (q=0; q<4; q++) h[q] = h0[q];   // initialize
    {
        grps  = 1 + (mlen+8)/64;
        msg2 = malloc( 64*grps);
        memcpy( msg2, msg, mlen);
        msg2[mlen] = (unsigned char)0x80; 
        q = mlen + 1;
        while (q < 64*grps){ msg2[q] = 0; q++ ; }
        {
            MD5union u;
            u.w = 8*mlen;
            q -= 8;
            memcpy(msg2+q, &u.w, 4 );
        }
    }
    for (grp=0; grp<grps; grp++){
        memcpy( mm.b, msg2+os, 64);
        for(q=0;q<4;q++) abcd[q] = h[q];
        for (p = 0; p<4; p++) {
            fctn = ff[p];
            rotn = rots[p];
            m = M[p]; o= O[p];
            for (q=0; q<16; q++) {
                g = (m*q + o) % 16;
                f = abcd[1] + rol( abcd[0]+ fctn(abcd) + k[q+16*p] + mm.w[g], rotn[q%4]);
                abcd[0] = abcd[3];
                abcd[3] = abcd[2];
                abcd[2] = abcd[1];
                abcd[1] = f;
            }
        }
        for (p=0; p<4; p++)
            h[p] += abcd[p];
        os += 64;
    }
    return h;
}
FILE *makefile(const char *dirname, const char *mode ){
    char pathname[1024];
    sprintf( pathname, "%s/%s", dirname, ".loser_record" );
    FILE *fp = fopen( pathname, mode );
    return fp;
}
struct FileNames list_file(const char *directory_path) {
    struct FileNames file_names;
    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        file_names.length = -1;
        return file_names;
    }
    file_names.length = 0;
    int real_length = DEFAULT_BUFFER_SIZE;
    file_names.names = (char **)malloc(sizeof(char *) * real_length);
    struct dirent *dp;
    while ((dp = readdir(dir)) != NULL) {
        if(strcmp(".",dp->d_name) == 0 || strcmp("..",dp->d_name) == 0 || strcmp(".loser_record",dp->d_name) == 0)
            continue;
        file_names.names[file_names.length] = (char *)malloc(sizeof(char) * strlen(dp->d_name));;
        strcpy(file_names.names[file_names.length], dp->d_name);
        file_names.length++;
        if (file_names.length == real_length) {
            real_length *= 2;
            file_names.names = (char **)realloc(file_names.names, sizeof(char *) * real_length);
        }
    }
    (void)closedir(dir);
    return file_names;
}
FILE *open_file(const char *dirname, const char *filename,const char *mode ){
    char pathname[1024];
    sprintf( pathname, "%s/%s", dirname, filename );
    FILE *fp = fopen( pathname, mode );
    return fp;
}
int Find_last_commit(char *dirname, struct FileNames file_names){
    struct dirent *dp;
    DIR *dir = opendir(dirname);
    while ((dp = readdir(dir)) != NULL){ // 找出test底下的 .loser_record 的指標
        if(!strcmp(dp->d_name,".loser_record")) break;
    }
    if(!dp && file_names.length){ //沒有 .loser_record 這個檔案 且目錄底下有檔案 於是開一個.loser_record
        FILE *fp = open_file(dirname ,".loser_record", "w+");
        fclose(fp);
        return 0;
    }
    FILE *fp = open_file(dirname ,".loser_record", "r+");
    char str[11];
    fseek(fp,0,SEEK_END);
    int size = ftell(fp);
    if(size == 0) return 0;
    fseek(fp,-1,SEEK_END);
    while(!fseek(fp,-1,SEEK_CUR)){
        fseek(fp,-9,SEEK_CUR);
        fgets(str,10,fp);
        if(strcmp(str,"# commit ") == 0){
            int num;
            fscanf(fp,"%d",&num);
            fclose(fp);
            return num;
        }
    }
}
void free_file_names(struct FileNames file_names) {
    for (int i = 0; i < file_names.length; i++) {
        free(file_names.names[i]);
    }
    free(file_names.names);
}
void Get_last_filename(const char *dirname){    //  由(MD5)往下找
    FILE *fp = open_file(dirname,".loser_record","r");
    char str1[11], buffer[300], z;
    fseek(fp,-1,SEEK_END);
    while(!fseek(fp,-1,SEEK_CUR)){
        fseek(fp,-5,SEEK_CUR);
        fgets(str1,6,fp);
        if(strcmp(str1,"(MD5)") == 0){
            z = fgetc(fp);
            if(z != ' '){   //避免 "(MD5)"為檔名
                fseek(fp,-1,SEEK_CUR);
                break;
            }
            fseek(fp,-1,SEEK_CUR);
        }
    }
    int index = 0;
    while(1){
        if(fscanf( fp , "%s", last_filename[index]) == EOF){
            last_filename_num = index;
            break;
        }
        fscanf( fp , "%s", last_file_MD5[index]);
        index++;
    }
    fclose(fp);
}
void Make_now_file_MD5(const char *dirname, struct FileNames file_names){
    //開啟現有目錄底下的檔案並將內容轉成string存在now_file_cont，再餵給md5函式，再把每個檔案的MD5存在 now_file_MD5
    int idx = 0;
    for(size_t i = 0; i < file_names.length; i++){
        FILE *fp = open_file(dirname,file_names.names[i],"r");
        char *now_file_cont[1024], buf[10000];
        fseek(fp, 0, SEEK_END);  //segmentation fault if fptr is NULL
        long input_file_size = ftell(fp);
        rewind(fp);
        now_file_cont[idx] =  malloc((input_file_size+1) * (sizeof(char)));
        int first = 1;
        while (fgets(buf, sizeof(buf), fp)){
            if(first){
                sprintf(now_file_cont[idx],"%s",buf);
                first = 0;
            }
            else sprintf(now_file_cont[idx]+strlen(now_file_cont[idx]),"%s",buf);
        }
        fclose(fp);
        unsigned *d = md5(now_file_cont[idx], strlen(now_file_cont[idx]));
        MD5union u;                     //以下MD5需要存到字串陣列裡
        for (int j=0; j<4; j++){
            u.w = d[j];
            if(j == 0)
                sprintf(now_file_MD5[idx],"%02x%02x%02x%02x",u.b[0],u.b[1],u.b[2],u.b[3]);
            else sprintf(now_file_MD5[idx]+strlen(now_file_MD5[idx]),"%02x%02x%02x%02x",u.b[0],u.b[1],u.b[2],u.b[3]);
        }
        idx++;
    }
}
int main(int argc, char *argv[]){
    int config_status = 0, config_commit = 0, config_log = 0;
    struct FileNames file_names;
    if(argc == 3) file_names = list_file(argv[2]);
    else if(argc == 4) file_names = list_file(argv[3]);
    else return 0;
    qsort(file_names.names ,file_names.length,sizeof(file_names.names),cmp);
    //printf("file_names.length = %d\n",file_names.length);
    for (size_t i = 0; i < file_names.length; i++) {
        //設定了 .loser_config ，可以用 別名 來代替 status/commit/log
        //直接拿 argv[1] 看有沒有出現在別名裡，有的話再去看他等於什麼
        if(strcmp(file_names.names[i],".loser_config") == 0){
            char pathname[1024];
            if(argc == 3)
                sprintf( pathname , "%s/%s", argv[2], ".loser_config");
            else if(argc == 4)
                sprintf( pathname , "%s/%s", argv[3], ".loser_config");
            FILE *f = fopen(pathname,"r");
            char str3[300], buf[3], str4[10];
            while(1){
                if(fscanf( f , "%s", str3) == EOF){    //  st
                    break;
                }
                fscanf( f , "%s", buf);         //  '='
                fscanf( f , "%s", str4);        //  status or commit or log
                if(strcmp(argv[1],str3) == 0){
                    if(strcmp(str4,"status") == 0) config_status = 1;
                    else if(strcmp(str4,"commit") == 0) config_commit = 1;
                    else if(strcmp(str4,"log") == 0) config_log = 1;
                }
            }
            fclose(f);
            break;
        }
    }
    if(argc == 3){
        Make_now_file_MD5(argv[2],file_names);
        // 執行 ./loser commit test
        if(strcmp(argv[1],"commit") == 0 || config_commit){
            int last_commit = Find_last_commit(argv[2],file_names);
            //printf("last commit : %d",last_commit);
            if(last_commit == 0){
                if(file_names.length == 0) return 0; //.loser_record 檔案不存在、也不存在任何其他檔案：依然不做任何動作，連 .loser_record 檔案都不建立
                FILE *fp = open_file(argv[2],".loser_record","w+");
                fprintf(fp,"# commit 1\n");
                fprintf(fp,"[new_file]\n");
                for (size_t i = 0; i < file_names.length; i++) {
                    fprintf(fp,"%s\n", file_names.names[i]);
                }
                fprintf(fp,"[modified]\n");
                fprintf(fp,"[copied]\n");
                fprintf(fp,"(MD5)\n");
                for(size_t i = 0; i < file_names.length; i++){
                    if(i == file_names.length - 1)
                        fprintf(fp, "%s %s", file_names.names[i], now_file_MD5[i]);
                    else fprintf(fp, "%s %s\n", file_names.names[i], now_file_MD5[i]);
                }
                fprintf(fp,"\n");
                fclose(fp);
                return 0; //第一次commit不用標準輸出任何東西
            }
            Get_last_filename(argv[2]); //取得.loser_record裡最後一次的檔案名稱
            //printf("file_names.length = %d\n",file_names.length);
            //檔案比對
            int type[1024] = {}, k = 0;
            // 1 [new_file]  2 [modified]  3 [copied] 4 same 記錄現有檔案的類型
            // 初步分類
            // 檔名出現過的直接比對MD5確認類別(2 or 4), 檔名新出現的先當成1(可能是3)，之後再判斷
            int id = 0;
            while(id < last_filename_num){
                //printf("-----------\nlast_filename:%s\n-------------\n",last_filename[i]);
                while(strcmp(file_names.names[k],last_filename[id]) != 0){
                    type[k] = 1;
                    //printf("filename:%s #type = %d\n",file_names.names[k],type[k]);
                    k++;
                    if(k == file_names.length) break;
                }
                if(k == file_names.length) break;  
                if(strcmp(now_file_MD5[k],last_file_MD5[id]) == 0){  //比對到一樣的檔名，檢查兩個的MD5
                    type[k] = 4;
                    //printf("filename:%s #type = %d\n",file_names.names[k],type[k]);
                }
                else{
                    type[k] = 2;
                    //printf("filename:%s #type = %d\n",file_names.names[k],type[k]);
                }
                k++;
                if(k == file_names.length) break;
                else if((id != last_filename_num-1) && k < file_names.length) id++;
            }
            //判斷新出現的檔案是類型1或者類型3
            //拿現有的檔案且 type = 1 的出來跟last_file_MD5全部比一次
            char copied[1024][700];
            for(int i = 0; i < file_names.length; i++){
                if(type[i] == 1){
                    for(int j = 0; j < last_filename_num; j++){
                        //printf("last_filename = %s last_file_MD5 = %s\n",last_filename[j],last_file_MD5[j]);
                        //printf("now_filename = %s now_file_MD5 = %s\n",file_names.names[i],now_file_MD5[i]);
                        if(strcmp(last_file_MD5[j],now_file_MD5[i]) == 0){
                            type[i] = 3;
                            sprintf(copied[i],"%s => %s",last_filename[j],file_names.names[i]);
                            break;
                        }
                    }
                }
                else continue;
            }
            //檔案與上一次 commit 沒有任何不同：不記錄 commit ，也就是不做任何操作
            if(last_filename_num == file_names.length){ // 檔案個數一樣才有可能全部都是type 4
                int flag = 1;
                for(int i=0;i<file_names.length;i++){
                    if(type[i] != 4){
                        flag = 0;
                        break;
                    }
                }
                if(flag) return 0;
            }

            FILE *fp = open_file(argv[2],".loser_record","a+");
            fprintf(fp,"\n%s%d\n","# commit ",last_commit+1);
            fprintf(fp,"%s\n","[new_file]");
            for(int i=0;i<file_names.length;i++){
                if(type[i] == 1){
                    fprintf(fp,"%s\n",file_names.names[i]);
                }
            }
            fprintf(fp,"%s\n","[modified]");
            for(size_t i=0;i<file_names.length;i++){
                if(type[i] == 2){
                    fprintf(fp,"%s\n",file_names.names[i]);
                }
            }
            fprintf(fp,"%s\n","[copied]");
            for(size_t i=0;i<file_names.length;i++){
                if(type[i] == 3){
                    fprintf(fp,"%s\n",copied[i]);
                }
            }
            fprintf(fp,"%s\n","(MD5)");
            for(size_t i = 0; i < file_names.length; i++){
                if(i == file_names.length - 1)
                    fprintf(fp, "%s %s", file_names.names[i], now_file_MD5[i]);
                else fprintf(fp, "%s %s\n", file_names.names[i], now_file_MD5[i]);
            }
            fprintf(fp,"\n");
            fclose(fp);
        }
        if(strcmp(argv[1],"status") == 0 || config_status == 1){
            FILE *fp = open_file(argv[2] ,".loser_record", "r+");
            if(!fp){    // test 目錄中沒有 .loser_record 所以會將所有檔案當作新檔案
                printf("[new_file]\n");
                for (size_t i = 0; i < file_names.length; i++) {
                    printf("%s\n", file_names.names[i]);
                }
                printf("[modified]\n");
                printf("[copied]\n");
            }
            else{
                Get_last_filename(argv[2]); //取得.loser_record裡最後一次的檔案名稱
                int type[1024] = {}, k = 0;
                // 1 [new_file]  2 [modified]  3 [copied] 4 same 記錄現有檔案的類型
                // 初步分類
                // 檔名出現過的直接比對MD5確認類別(2 or 4), 檔名新出現的先當成1(可能是3)，之後再判斷
                int id = 0;
                while(id < last_filename_num){
                    //printf("-----------\nlast_filename:%s\n-------------\n",last_filename[i]);
                    while(strcmp(file_names.names[k],last_filename[id]) != 0){
                        type[k] = 1;
                        //printf("filename:%s #type = %d\n",file_names.names[k],type[k]);
                        k++;
                        if(k == file_names.length) break;
                    }
                    if(k == file_names.length) break;  
                    if(strcmp(now_file_MD5[k],last_file_MD5[id]) == 0){  //比對到一樣的檔名，檢查兩個的MD5
                        type[k] = 4;
                        //printf("filename:%s #type = %d\n",file_names.names[k],type[k]);
                    }
                    else{
                        type[k] = 2;
                        //printf("filename:%s #type = %d\n",file_names.names[k],type[k]);
                    }
                    k++;
                    if(k == file_names.length) break;
                    else if((id != last_filename_num-1) && k < file_names.length) id++;
                }
                //判斷新出現的檔案是類型1或者類型3
                //拿現有的檔案且 type = 1 的出來跟last_file_MD5全部比一次
                char copied[1024][700];
                for(int i = 0; i < file_names.length; i++){
                    if(type[i] == 1){
                        for(int j = 0; j < last_filename_num; j++){
                            //printf("last_filename = %s last_file_MD5 = %s\n",last_filename[j],last_file_MD5[j]);
                            //printf("now_filename = %s now_file_MD5 = %s\n",file_names.names[i],now_file_MD5[i]);
                            if(strcmp(last_file_MD5[j],now_file_MD5[i]) == 0){
                                type[i] = 3;
                                sprintf(copied[i],"%s => %s",last_filename[j],file_names.names[i]);
                                break;
                            }
                        }
                    }
                    else continue;
                }
                printf("%s\n","[new_file]");
                for(int i=0;i<file_names.length;i++){
                    if(type[i] == 1){
                        printf("%s\n",file_names.names[i]);
                    }
                }
                printf("%s\n","[modified]");
                for(size_t i=0;i<file_names.length;i++){
                    if(type[i] == 2){
                        printf("%s\n",file_names.names[i]);
                    }
                }
                printf("%s\n","[copied]");
                for(size_t i=0;i<file_names.length;i++){
                    if(type[i] == 3){
                        printf("%s\n",copied[i]);
                    }
                }
            }
        }
    }
    else if(argc == 4){  // 執行 ./loser log 3 test
        if(strcmp(argv[1],"log") == 0 || config_log == 1){
            FILE *fp = open_file(argv[3] ,".loser_record", "r+");
            if(!fp) return 0;
            char str[11];
            int flag = atoi(argv[2]);
            int tmp = Find_last_commit(argv[3],file_names);
            if(tmp > flag) tmp = flag;
            else flag = tmp;
            fseek(fp,-1,SEEK_END);
            
            while((!fseek(fp,-1,SEEK_CUR)) && tmp){
                fseek(fp,-9,SEEK_CUR);
                fgets(str,10,fp);
                if(strcmp(str,"# commit ") == 0){
                    //printf("-----------\ntmp = %d\n--------\n",tmp);
                    fseek(fp,-9,SEEK_CUR);
                    char buff[512];
                    long output_file_size = 0;
                    int end_of_file = 0;
                    if(flag != tmp) printf("\n");
                    while(1){
                        if(!fgets(buff,512,fp)) end_of_file = 1;
                        int len = strlen(buff);
                        output_file_size += len;
                        if(strcmp(buff,"\n") == 0 || end_of_file == 1){
                            break;
                        }
                        printf("%s",buff);
                    }
                    //if(flag == tmp) printf("\n");
                    fseek(fp,-output_file_size,SEEK_CUR);
                    tmp--;
                }
            }
            fclose(fp);
        }
    }
    free_file_names(file_names);
    return 0;
}  
