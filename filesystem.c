#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#define SYS_PATH "system.txt"
#define BLK_MAXLEN 1024
#define MAX_STRING_LENGTH 65535

int SYSSIZE = 1048576;//1MB
int BLOCKSIZE = 1024;//1KB
int INODESIZE = 64;//64B
int MAXFILE = 8192;//8KB
int MAXNAME = 8;//8B
int FILEAMOUNT = 80;
int ROOTPLACE = 16;
int ROOTDES = 3;

int GETBLK(void* buf, int blk_no){
    int sys = open(SYS_PATH, O_RDONLY);
    if(sys < 0){
        write(2, "open file error!\n", 17);
        return -1;
    }

    if(lseek(sys, blk_no * BLOCKSIZE, SEEK_SET) < 0){
        write(2, "seeking error!\n", 15);
        return -1;
    }
    
    if(read(sys, buf, BLOCKSIZE) < 0){
        write(2, "read error!\n", 12);
    }
    close(sys);
}

int PUTBLK(void* buf, int blk_no){
    int sys = open(SYS_PATH,O_WRONLY);
    if(sys < 0){
        write(2, "open file error!\n", 17);
        return -1;
    }
    
    if(lseek(sys, blk_no * BLOCKSIZE, SEEK_SET) < 0){
        write(2, "seeking error!\n", 15);
        return -1;
    }

    if(write(sys, buf, BLOCKSIZE) < BLOCKSIZE){
        write(2, "write error!\n", 13);
    }
    close(sys);
}

int inttochar(char* buf, int place, int src){
    for(int i = 0; i < 4; i++){
      buf[place + i] = src & 0xff;
      src >>= 8;
    }
}
int chartoint(char* buf, int place){
    int ret = 0;
    for(int i = 0; i < 4; i++){
      ret += buf[place + i];
      ret <<= 8;
    }
    return ret;
}

typedef struct{
    int descriptor;
    int blk_no;
    int fa_inode;
    int size;
    int used_blk;
    time_t create_time;//8B
    time_t change_time;
    time_t access_time;
    char name[8];
    char flags;//read write hide dict
} Inode;

int writeInode(Inode* inode){
    int inode_num = inode->descriptor;
    int blk = inode_num / (BLOCKSIZE / INODESIZE) + 9;
    int offset = inode_num % (BLOCKSIZE / INODESIZE);
    Inode buf[(BLOCKSIZE / INODESIZE)];
    if(GETBLK(buf, blk) < 0){
        return -1;
    }
    buf[offset] = *inode;
    PUTBLK(buf, blk);
    return 0;
}
int intToInode(Inode* inode, int inode_num){
    int blk = inode_num / (BLOCKSIZE / INODESIZE) + 9;
    int offset = inode_num % (BLOCKSIZE / INODESIZE);
    Inode buf[(BLOCKSIZE / INODESIZE)];
    if(GETBLK(buf, blk) < 0){
        return -1;
    }
    *inode = buf[offset];
    return 0;
}

int findDict(Inode *buf, Inode *inp, char *filename){

    int length = BLOCKSIZE / sizeof(int);
    int ints[length];
    if(!strcmp(filename, "/")){
        intToInode(inp, ROOTDES);
        *buf = *inp;
        return 0;
    }
    if(filename[0] == '/'){

        intToInode(inp, ROOTDES);
        if(strlen(filename) == 1){
            *buf = *inp;
            return 0;
        }
        int a = 0, b = 0, p = -1;
        char substr[80];
        for(; b < strlen(filename);a = b){
            b++;
            while(filename[b] != '/' && b < strlen(filename)){
                b++;
            }
            if(b == strlen(filename)){
                break;
            }
            strncpy(substr, filename + a + 1, b - a - 1);
            substr[b - a - 1] = '\0';
            p = findDict(buf, inp, substr);
            if(p < 0){
                return -1;
            }
            *inp = *buf;
        }
        if(filename[b - 1] == '/'){
            b--;
        }
        strncpy(substr, filename + a + 1, b - a);
        substr[b - a] = '\0';
        p = findDict(buf, inp, substr);
        return p;
    }

    if(!(inp->flags & 1)){
        return -1;
    }
    if(GETBLK(ints, inp->blk_no) < 0){
        return -1;
    }
    
    for(int i = 1; i <= ints[0]; i++){
        intToInode(buf, ints[i]);
        if(!strcmp(filename, buf->name)){
            return i;
        }
    }
    return -1;
}
int newFile(Inode* file, int father, char* name, int isDict){
        memset(file, 0, sizeof(file));
    char s[BLOCKSIZE], q[BLOCKSIZE];
    memset(q, 0, sizeof(q));
    int ints[BLOCKSIZE / sizeof(int)];
    char c;
    Inode inode, *inp = &inode, inode2;
    if(!strcmp(name, "/") && father != -1){
        return -1;
    }
    if(name[0] == '/' && father != -1){
        int i = strlen(name) - 1;
        if(name[i] == '/') return -1;
        while(name[i]!= '/' && i>=0) i--;
        if(i < 0){
            return -1;
        }
        strncpy(q, name, i);
        q[i] = '\0';
        if(findDict(&inode2, inp, q) < 0){
            return -1;
        }
        father = inode2.fa_inode;
        strncpy(q, name + i + 1, strlen(name) - i - 1);
        q[strlen(name) - i - 1] = '\0';
        strcpy(name, q);
    }
    if(strlen(name) > 8){
        fprintf(stderr, "create error, filename overflow!\n");
        return -1;
    }
    if(father > 0){
        intToInode(inp, father);
        if(findDict(&inode2, inp, name) >= 0){
          fprintf(stderr, "create error, filename already exist!\n");
            return -1;
        }
        GETBLK(ints, inp->blk_no);

    }
    GETBLK(s, 2);
    for(int i = 0; i < BLOCKSIZE; i++){
        if((s[i] & 1) == 0){
            s[i] = 0xff;
            break;
        }
        file->blk_no += 8;
    }
    if(file->blk_no > SYSSIZE / BLOCKSIZE){
        fprintf(stderr, "create error, max disk!\n");
        return -1;
    }
    memset(q, 0, sizeof(q));
    for(int i = file->blk_no; i < file->blk_no + 8; i++){
        PUTBLK(q, i);
    }
    PUTBLK(s, 2);
    GETBLK(s, 3);
    for(int i = 0; i < BLOCKSIZE; i++){
        c = s[i];
        for(int j = 0; j < 8; j++){
            if((c & 1) == 0){
                s[i] |= 1 << j;
                break;
            }
            c >>= 1;
            file->descriptor++;
        }
        if((c & 1) == 0){
            break;
        }
    }
    if(file->descriptor > FILEAMOUNT){
        fprintf(stderr, "create error, max file!\n");
        return -1;
    }
    PUTBLK(s, 3);

    file->fa_inode = father;
    file->size = 0;
    file->used_blk = 1;
    file->create_time = file->change_time = file->access_time = time(NULL);
    strcpy(file->name, name);
    file->flags = 0xC;//1100
    if(isDict){
        file->flags |= 1;
    }

    writeInode(file);

    if(father > 0){//not creating root
        ints[0]++;
        ints[ints[0]] = file->descriptor;
        PUTBLK(ints, inp->blk_no);
    }
    return 0;
}
int dictFlag(char c, char *s){
    strcpy(s,"----");
    if(c & 8) s[0] = 'r';
    if(c & 4) s[1] = 'w';
    if(c & 2) s[2] = 'h';
    if(c & 1) s[3] = 'd';
}
//ints[0]:length; 1~255: file_blk_no
int showDict(Inode *inps, int descriptor){
    Inode inode, *inp = &inode;
    intToInode(inp, descriptor);
    int length = BLOCKSIZE / sizeof(int);
    int ints[length];
    char s[5];
    char buf[80];
    struct tm * timeinfo;
    if(GETBLK(ints, inp->blk_no) < 0){
        return -1;
    }
    printf("flag filename %19s %19s %19s\n","create time","access time","change time");
    for(int i = 1; i <= ints[0]; i++){
        intToInode(&inps[i-1], ints[i]);
        dictFlag(inps[i-1].flags, s);
        printf("%s %8s ", s, inps[i-1].name);
        timeinfo = localtime(&inps[i-1].create_time);
        strftime(buf, 80, "%Y-%m-%d %H:%M:%S",timeinfo);
        printf("%s ",buf);
        timeinfo = localtime(&inps[i-1].access_time);
        strftime(buf, 80, "%Y-%m-%d %H:%M:%S",timeinfo);
        printf("%s ",buf);
        timeinfo = localtime(&inps[i-1].change_time);
        strftime(buf, 80, "%Y-%m-%d %H:%M:%S",timeinfo);
        printf("%s\n",buf);
    }
    return ints[0];
}
//des: return now des; 
//int : -1 for error, 0 for back 1 for forward
int gotoDict(int * des, char * filename){
    Inode inode, *inp = &inode, inode2;
    intToInode(inp, *des);
    if(!strcmp(filename, "../")){
        if(inp->fa_inode == -1){
            return -1;
        }
        *des = inp->fa_inode;
        return 0;//backward
    }
    if(findDict(&inode2, inp, filename) < 0){
        return -1;
    }
    if(!(inode2.flags & 1)){
        return -1;
    }
    *des = inode2.descriptor;
    if(filename[strlen(filename) - 1] != '/'){
        strcat(filename, "/");
    }
    if(filename[0] == '/') return 2;
    return 1;
}
int myremove(Inode *inp, char * filename){
    Inode inode = *inp, inode2;
    char s[BLOCKSIZE];
    int length = BLOCKSIZE / sizeof(int);
    int ints[length];
    if(GETBLK(ints, inode.blk_no) < 0){
        return -1;
    }
    if(!strcmp(filename,".")){
        for(int i = ints[0]; i >= 1; i--){
            intToInode(&inode2, ints[i]);
            if(inode2.flags & 1){
                myremove(&inode2, ".");
            }
            GETBLK(s, 2);
            s[inode2.blk_no / 8] = 0;
            PUTBLK(s, 2);
            GETBLK(s, 3);
            s[inode2.descriptor / 8] &= ~ (1 << (inode2.descriptor % 8));
            PUTBLK(s, 3);
            ints[0]--;
        }
        PUTBLK(ints, inode.blk_no);
        return 0;
    }

    int p = findDict(&inode2, &inode, filename);
    if(p < 0 || !strcmp(filename, "/")){
        return -1;
    }
    if(inode2.flags & 1){
        myremove(&inode2, ".");
    }
    GETBLK(s, 2);
    s[inode2.blk_no / 8] = 0;
    PUTBLK(s, 2);
    GETBLK(s, 3);
    s[inode2.descriptor / 8] &= ~ (1 << (inode2.descriptor % 8));
    PUTBLK(s, 3);

    for(int i = p; i < ints[0]; i++){
            ints[i] = ints[i + 1];
    }
    ints[0]--;
    PUTBLK(ints, inode.blk_no);
    return 0;
}
int openf(int nowdes, int *des, int *p, char * text, char * filename){
    Inode buf, inode;
    char cbuf[BLOCKSIZE];
    intToInode(&inode, nowdes);
    if(findDict(&buf, &inode, filename) < 0){
        return -1;
    }
    printf("%d\n",buf.descriptor);
    if(buf.flags & 1){
        return -1;
    }
    *des = buf.descriptor;
    *p = 0;
    memset(text, 0, sizeof(text));
    for(int i = 0; i < 8; i++){
        GETBLK(cbuf, buf.blk_no + i);
        printf("%d:%s\n",i,cbuf);
        strcat(text, cbuf);
    }
    return 0; 
}
int closef(int *des, int *p, char *text){
    Inode buf, inode;
    char cbuf[BLOCKSIZE];
    intToInode(&buf, *des);
    for(int i = 0; i < 8; i++){
        strncpy(cbuf, text + BLOCKSIZE * i, BLOCKSIZE);
        PUTBLK(cbuf, buf.blk_no + i);
    }
    if(*p > buf.size){
        buf.size = *p;
        writeInode(&buf);
    }
    *des = 0;
    return 0;
}
int readf(int *des, int *p, char *text, char *arg){
    int len = -1;
    char buf[MAX_STRING_LENGTH];
    Inode inode;
    intToInode(&inode, *des);
    sscanf(arg, "%d", &len);
    if(len == -1){
        return -1;
    }
    if((*p) + len > inode.size) return -1;
    strncpy(buf, text + (*p), len);
    buf[len] = '\0';
    printf("%s\n",buf);
    *p += len;
    inode.access_time = time(NULL);
    writeInode(&inode);
    return 0;
}
int writef(int *des, int *p, char *text, char *arg){
    int len = -1;
    char buf[MAX_STRING_LENGTH];
    Inode inode;
    intToInode(&inode, *des);
    len = strlen(arg);
    if((*p) + len > 8 * BLOCKSIZE) return -1;
    strncpy(text + (*p), arg, len);
    *p += len;
    inode.change_time = inode.access_time = time(NULL);
    writeInode(&inode);
    return 0;
}
int savef(int *des, int *p, char *text, char *arg){
    int len = -1;
    char buf[MAX_STRING_LENGTH];
    Inode inode;
    intToInode(&inode, *des);
    FILE *fp = fopen(arg, "r");
    if(fp == NULL) return -1;
    fscanf(fp, "%s", buf);
    len = strlen(buf);
    if((*p) + len > 8 * BLOCKSIZE) return -1;
    strncpy(text + (*p), buf, len);
    *p += len;
    inode.change_time = inode.access_time = time(NULL);
    writeInode(&inode);
    return 0;
}
int formatting(){
    char s[BLK_MAXLEN];
    memset(s, 0, sizeof(s));
    for(int i = 0; i < 1024; i++){
        PUTBLK(s, i);
    }
    PUTBLK(s, 0);//boot block

    int sys_size = 1048576;//1MB
    int block_size = 1024;//1KB
    int inode_size = 64;//64B
    int max_file = 8192;//8KB
    int max_name = 8;//8B
    int file_amount = 80;
    int root_place = 16;
    int root_descriptor = 3;
    inttochar(s, 0, sys_size);
    inttochar(s, 4, block_size);
    inttochar(s, 8, inode_size);
    inttochar(s, 12, max_file);
    inttochar(s, 16, max_name);
    inttochar(s, 20, file_amount);
    inttochar(s, 24, root_place);
    inttochar(s, 28, root_descriptor);
    PUTBLK(s, 1);//super block

    memset(s, 0, sizeof(s));
    int usage = 0xffff;//first 9
    inttochar(s, 0, usage);
    PUTBLK(s, 2);//data block bitmap

    memset(s, 0, sizeof(s));
    int inode = 0x7;//stdin,stdout,stderr
    inttochar(s, 0, inode);
    PUTBLK(s, 3);//inode bitmap
    
    Inode file;
    newFile(&file, -1, "/", 1);
   // lookup(filep->blk_no);
    
    //PUTBLK(file, 4);//inode table,length = 5
}
void test(int * xp,int * yp){
    int * zp = yp;
    *xp = (*zp)*2;

}
void boot(){
    char s[MAX_STRING_LENGTH];
    char c[MAX_STRING_LENGTH];
    char d[MAX_STRING_LENGTH];
    Inode inps[128];
    Inode file, *filep = &file;
    int nowDes = ROOTDES;
    int tmp = 0;
    char path[MAX_STRING_LENGTH] = "root@naide.me/";
    int openfile = 0;
    int openp = 0;
    char text[MAX_STRING_LENGTH];
    while(1){
        if(openfile == 0){
            printf("%d %s", nowDes,path);
            gets(s);
            sscanf(s,"%s %s",c,d);
            if(!strcmp(c, "ls")){
                showDict(inps, nowDes);
            }
            else if(!strcmp(c, "cd")){
                tmp = gotoDict(&nowDes, d);
                if(tmp == 0){
                    path[strlen(path) - 1] = '\0';
                    for(int i = strlen(path) -1; path[i] != '/'; i--){
                        path[i] = '\0';
                    }
                }
                else if(tmp == 1){
                    strcat(path, d);
                }
                else if(tmp == 2){
                    strcpy(path, "root@naide.me");
                    strcat(path, d);
                }
            }
            else if(!strcmp(c, "rm")){
                printf("Do you really want to remove %s?(Y/N)",d);
                gets(s);
                if(s[0] != 'Y' && s[0] != 'y') {
                    puts("remove cancelled..");
                    continue;
                }
                intToInode(filep, nowDes);
                if(myremove(filep, d) < 0){
                puts("error:file not found");
                }
            }
            else if(!strcmp(c, "new")){
                newFile(filep, nowDes, d, 0);
            }
            else if(!strcmp(c, "newdict")){
                newFile(filep, nowDes, d, 1);
            }
            else if(!strcmp(c, "format")){
                formatting();
            }
            else if(!strcmp(c, "shutdown")){
                printf("it's goint to shutdown..\n");
                return;
            }
            else if(!strcmp(c, "open")){
                openf(nowDes, &openfile, &openp, text, d);
            }
            else {
                puts("command not found!");
            }
        }
        else{
            intToInode(&file, openfile);
            printf("now opening: %s, >",file.name);
            gets(s);
            sscanf(s,"%s %s",c,d);
            if(!strcmp(c, "close")){
                closef(&openfile, &openp, text);
            }
            else if(!strcmp(c, "read")){
                readf(&openfile, &openp, text, d);
            }
            else if(!strcmp(c, "write")){
                writef(&openfile, &openp, text, d);
            }
            else if(!strcmp(c, "save")){
                savef(&openfile, &openp, text, d);
            }
            else {
                puts("command not found!");
            }
        }
            
    }
}
int main(){
    // do something from boot block

//    char path[PATH_LEN] = "/";
    boot();
    return 0;
}