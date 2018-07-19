#include "list_file.h"
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#define DEFAULT_BUFFER_SIZE 128

char last_filename[1024][270], last_file_MD5[1024][270], now_file_MD5[1024][40];
int last_filename_num, idx = 0;
int cmp(const void *a,const void *b){
    return strcmp( *(const char **)a,*(const char **)b );
}

typedef unsigned int UINT4;

/* Data structure for MD5 (Message Digest) computation */
typedef struct {
  UINT4 i[2];                   /* number of _bits_ handled mod 2^64 */
  UINT4 buf[4];                                    /* scratch buffer */
  unsigned char in[64];                              /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
} MD5_CTX;

void MD5Init ();
void MD5Update ();
void MD5Final ();

/* -- include the following line if the md5.h header file is separate -- */
/* #include "md5.h" */

/* forward declaration */
static void Transform ();

static unsigned char PADDING[64] = {
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* F, G and H are basic MD5 functions: selection, majority, parity */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z))) 

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s, ac) \
  {(a) += F ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) \
  {(a) += G ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) \
  {(a) += H ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) \
  {(a) += I ((b), (c), (d)) + (x) + (UINT4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }

void MD5Init (mdContext)
MD5_CTX *mdContext;
{
  mdContext->i[0] = mdContext->i[1] = (UINT4)0;

  /* Load magic initialization constants.
   */
  mdContext->buf[0] = (UINT4)0x67452301;
  mdContext->buf[1] = (UINT4)0xefcdab89;
  mdContext->buf[2] = (UINT4)0x98badcfe;
  mdContext->buf[3] = (UINT4)0x10325476;
}

void MD5Update (mdContext, inBuf, inLen)
MD5_CTX *mdContext;
unsigned char *inBuf;
unsigned int inLen;
{
  UINT4 in[16];
  int mdi;
  unsigned int i, ii;

  /* compute number of bytes mod 64 */
  mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

  /* update number of bits */
  if ((mdContext->i[0] + ((UINT4)inLen << 3)) < mdContext->i[0])
    mdContext->i[1]++;
  mdContext->i[0] += ((UINT4)inLen << 3);
  mdContext->i[1] += ((UINT4)inLen >> 29);

  while (inLen--) {
    /* add new character to buffer, increment mdi */
    mdContext->in[mdi++] = *inBuf++;

    /* transform if necessary */
    if (mdi == 0x40) {
      for (i = 0, ii = 0; i < 16; i++, ii += 4)
        in[i] = (((UINT4)mdContext->in[ii+3]) << 24) |
                (((UINT4)mdContext->in[ii+2]) << 16) |
                (((UINT4)mdContext->in[ii+1]) << 8) |
                ((UINT4)mdContext->in[ii]);
      Transform (mdContext->buf, in);
      mdi = 0;
    }
  }
}

void MD5Final (mdContext)
MD5_CTX *mdContext;
{
  UINT4 in[16];
  int mdi;
  unsigned int i, ii;
  unsigned int padLen;

  /* save number of bits */
  in[14] = mdContext->i[0];
  in[15] = mdContext->i[1];

  /* compute number of bytes mod 64 */
  mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

  /* pad out to 56 mod 64 */
  padLen = (mdi < 56) ? (56 - mdi) : (120 - mdi);
  MD5Update (mdContext, PADDING, padLen);

  /* append length in bits and transform */
  for (i = 0, ii = 0; i < 14; i++, ii += 4)
    in[i] = (((UINT4)mdContext->in[ii+3]) << 24) |
            (((UINT4)mdContext->in[ii+2]) << 16) |
            (((UINT4)mdContext->in[ii+1]) << 8) |
            ((UINT4)mdContext->in[ii]);
  Transform (mdContext->buf, in);

  /* store buffer in digest */
  for (i = 0, ii = 0; i < 4; i++, ii += 4) {
    mdContext->digest[ii] = (unsigned char)(mdContext->buf[i] & 0xFF);
    mdContext->digest[ii+1] =
      (unsigned char)((mdContext->buf[i] >> 8) & 0xFF);
    mdContext->digest[ii+2] =
      (unsigned char)((mdContext->buf[i] >> 16) & 0xFF);
    mdContext->digest[ii+3] =
      (unsigned char)((mdContext->buf[i] >> 24) & 0xFF);
  }
}

/* Basic MD5 step. Transform buf based on in.
 */
static void Transform (buf, in)
UINT4 *buf;
UINT4 *in;
{
  UINT4 a = buf[0], b = buf[1], c = buf[2], d = buf[3];

  /* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
  FF ( a, b, c, d, in[ 0], S11, 3614090360); /* 1 */
  FF ( d, a, b, c, in[ 1], S12, 3905402710); /* 2 */
  FF ( c, d, a, b, in[ 2], S13,  606105819); /* 3 */
  FF ( b, c, d, a, in[ 3], S14, 3250441966); /* 4 */
  FF ( a, b, c, d, in[ 4], S11, 4118548399); /* 5 */
  FF ( d, a, b, c, in[ 5], S12, 1200080426); /* 6 */
  FF ( c, d, a, b, in[ 6], S13, 2821735955); /* 7 */
  FF ( b, c, d, a, in[ 7], S14, 4249261313); /* 8 */
  FF ( a, b, c, d, in[ 8], S11, 1770035416); /* 9 */
  FF ( d, a, b, c, in[ 9], S12, 2336552879); /* 10 */
  FF ( c, d, a, b, in[10], S13, 4294925233); /* 11 */
  FF ( b, c, d, a, in[11], S14, 2304563134); /* 12 */
  FF ( a, b, c, d, in[12], S11, 1804603682); /* 13 */
  FF ( d, a, b, c, in[13], S12, 4254626195); /* 14 */
  FF ( c, d, a, b, in[14], S13, 2792965006); /* 15 */
  FF ( b, c, d, a, in[15], S14, 1236535329); /* 16 */

  /* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
  GG ( a, b, c, d, in[ 1], S21, 4129170786); /* 17 */
  GG ( d, a, b, c, in[ 6], S22, 3225465664); /* 18 */
  GG ( c, d, a, b, in[11], S23,  643717713); /* 19 */
  GG ( b, c, d, a, in[ 0], S24, 3921069994); /* 20 */
  GG ( a, b, c, d, in[ 5], S21, 3593408605); /* 21 */
  GG ( d, a, b, c, in[10], S22,   38016083); /* 22 */
  GG ( c, d, a, b, in[15], S23, 3634488961); /* 23 */
  GG ( b, c, d, a, in[ 4], S24, 3889429448); /* 24 */
  GG ( a, b, c, d, in[ 9], S21,  568446438); /* 25 */
  GG ( d, a, b, c, in[14], S22, 3275163606); /* 26 */
  GG ( c, d, a, b, in[ 3], S23, 4107603335); /* 27 */
  GG ( b, c, d, a, in[ 8], S24, 1163531501); /* 28 */
  GG ( a, b, c, d, in[13], S21, 2850285829); /* 29 */
  GG ( d, a, b, c, in[ 2], S22, 4243563512); /* 30 */
  GG ( c, d, a, b, in[ 7], S23, 1735328473); /* 31 */
  GG ( b, c, d, a, in[12], S24, 2368359562); /* 32 */

  /* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
  HH ( a, b, c, d, in[ 5], S31, 4294588738); /* 33 */
  HH ( d, a, b, c, in[ 8], S32, 2272392833); /* 34 */
  HH ( c, d, a, b, in[11], S33, 1839030562); /* 35 */
  HH ( b, c, d, a, in[14], S34, 4259657740); /* 36 */
  HH ( a, b, c, d, in[ 1], S31, 2763975236); /* 37 */
  HH ( d, a, b, c, in[ 4], S32, 1272893353); /* 38 */
  HH ( c, d, a, b, in[ 7], S33, 4139469664); /* 39 */
  HH ( b, c, d, a, in[10], S34, 3200236656); /* 40 */
  HH ( a, b, c, d, in[13], S31,  681279174); /* 41 */
  HH ( d, a, b, c, in[ 0], S32, 3936430074); /* 42 */
  HH ( c, d, a, b, in[ 3], S33, 3572445317); /* 43 */
  HH ( b, c, d, a, in[ 6], S34,   76029189); /* 44 */
  HH ( a, b, c, d, in[ 9], S31, 3654602809); /* 45 */
  HH ( d, a, b, c, in[12], S32, 3873151461); /* 46 */
  HH ( c, d, a, b, in[15], S33,  530742520); /* 47 */
  HH ( b, c, d, a, in[ 2], S34, 3299628645); /* 48 */

  /* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
  II ( a, b, c, d, in[ 0], S41, 4096336452); /* 49 */
  II ( d, a, b, c, in[ 7], S42, 1126891415); /* 50 */
  II ( c, d, a, b, in[14], S43, 2878612391); /* 51 */
  II ( b, c, d, a, in[ 5], S44, 4237533241); /* 52 */
  II ( a, b, c, d, in[12], S41, 1700485571); /* 53 */
  II ( d, a, b, c, in[ 3], S42, 2399980690); /* 54 */
  II ( c, d, a, b, in[10], S43, 4293915773); /* 55 */
  II ( b, c, d, a, in[ 1], S44, 2240044497); /* 56 */
  II ( a, b, c, d, in[ 8], S41, 1873313359); /* 57 */
  II ( d, a, b, c, in[15], S42, 4264355552); /* 58 */
  II ( c, d, a, b, in[ 6], S43, 2734768916); /* 59 */
  II ( b, c, d, a, in[13], S44, 1309151649); /* 60 */
  II ( a, b, c, d, in[ 4], S41, 4149444226); /* 61 */
  II ( d, a, b, c, in[11], S42, 3174756917); /* 62 */
  II ( c, d, a, b, in[ 2], S43,  718787259); /* 63 */
  II ( b, c, d, a, in[ 9], S44, 3951481745); /* 64 */

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}

static void MDPrint (mdContext)
MD5_CTX *mdContext;
{
  for (int i = 0; i < 16; i++)
    if(i == 0)
        sprintf (now_file_MD5[idx],"%02x", mdContext->digest[i]);
    else sprintf(now_file_MD5[idx]+strlen(now_file_MD5[idx]),"%02x",mdContext->digest[i]);
}

/* size of test block */
#define TEST_BLOCK_SIZE 1000

/* number of blocks to process */
#define TEST_BLOCKS 10000

/* number of test bytes = TEST_BLOCK_SIZE * TEST_BLOCKS */
static long TEST_BYTES = (long)TEST_BLOCK_SIZE * (long)TEST_BLOCKS;

/* A time trial routine, to measure the speed of MD5.
   Measures wall time required to digest TEST_BLOCKS * TEST_BLOCK_SIZE
   characters.
 */
static void MDTimeTrial ()
{
  MD5_CTX mdContext;
  time_t endTime, startTime;
  unsigned char data[TEST_BLOCK_SIZE];
  unsigned int i;

  /* initialize test data */
  for (i = 0; i < TEST_BLOCK_SIZE; i++)
    data[i] = (unsigned char)(i & 0xFF);

  /* start timer */
  printf ("MD5 time trial. Processing %ld characters...\n", TEST_BYTES);
  time (&startTime);

  /* digest data in TEST_BLOCK_SIZE byte blocks */
  MD5Init (&mdContext);
  for (i = TEST_BLOCKS; i > 0; i--)
    MD5Update (&mdContext, data, TEST_BLOCK_SIZE);
  MD5Final (&mdContext);

  /* stop timer, get time difference */
  time (&endTime);
  MDPrint (&mdContext);
  printf (" is digest of test input.\n");
  printf
    ("Seconds to process test input: %ld\n", (long)(endTime-startTime));
  printf
    ("Characters processed per second: %ld\n",
     TEST_BYTES/(endTime-startTime));
}

/* Computes the message digest for string inString.
   Prints out message digest, a space, the string (in quotes) and a
   carriage return.
 */
static void MDString (inString)
char *inString;
{
  MD5_CTX mdContext;
  unsigned int len = strlen (inString);

  MD5Init (&mdContext);
  MD5Update (&mdContext, inString, len);
  MD5Final (&mdContext);
  MDPrint (&mdContext);
  printf (" \"%s\"\n\n", inString);
}

/* Computes the message digest for a specified file.
   Prints out message digest, a space, the file name, and a carriage
   return.
 */
static void MDFile (filename)
char *filename;
{
  FILE *inFile = fopen (filename, "rb");
  MD5_CTX mdContext;
  int bytes;
  unsigned char data[1024];

  if (inFile == NULL) {
    printf ("%s can't be opened.\n", filename);
    return;
  }

  MD5Init (&mdContext);
  while ((bytes = fread (data, 1, 1024, inFile)) != 0)
    MD5Update (&mdContext, data, bytes);
  MD5Final (&mdContext);
  MDPrint (&mdContext);
  //printf (" %s\n", filename);
  fclose (inFile);
}

/* Writes the message digest of the data from stdin onto stdout,
   followed by a carriage return.
 */
static void MDFilter ()
{
  MD5_CTX mdContext;
  int bytes;
  unsigned char data[16];

  MD5Init (&mdContext);
  while ((bytes = fread (data, 1, 16, stdin)) != 0)
    MD5Update (&mdContext, data, bytes);
  MD5Final (&mdContext);
  MDPrint (&mdContext);
  printf ("\n");
}

/* Runs a standard suite of test data.
 */
static void MDTestSuite ()
{
  printf ("MD5 test suite results:\n\n");
  MDString ("");
  MDString ("a");
  MDString ("abc");
  MDString ("message digest");
  MDString ("abcdefghijklmnopqrstuvwxyz");
  MDString
    ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
  MDString
    ("1234567890123456789012345678901234567890\
1234567890123456789012345678901234567890");
  /* Contents of file foo are "abc" */
  MDFile ("foo");
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
    for(size_t i = 0; i < file_names.length; i++){
        char pathname[1024];
        sprintf(pathname,"%s/%s",dirname,file_names.names[i]);
        MDFile(pathname);
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
