#include <stdlib.h>
#include "utils.h"
#include "csapp.h"

int send_error(int fd, char *msg){
    return send_packet(fd, msg, ERROR);
}

int send_packet(int fd, char *msg, int type){
    int len = _strlen(msg)+9;
    char *buf = Calloc(len, 1);
    packet_t *packet = (packet_t *)buf;
    packet->len = len;
    packet->type = type;
    _strcpy(buf+8, msg);
    // printf("send packet with length %d, type '%s'\n", packet->len, buf+8);
    Rio_writen(fd, buf, packet->len);
    Free(buf);
    return 0;
}

char * recieve_packet(rio_t *rio, int type, packet_t *packet){
    char *buf;
    packet_t tmp;
    if(packet==NULL)
        packet = &tmp;
    
    if(Rio_readnb(rio, packet, sizeof(packet_t)) == 0){
        //closed connection
        return NULL;
    }
    buf = Calloc(packet->len-PACKET_SIZE,1);
    if(packet->type == ERROR){
        return NULL;
    }
    if(packet->type != type || 
        Rio_readnb(rio, buf, packet->len-PACKET_SIZE) != packet->len-PACKET_SIZE)
    {
        return NULL;
    }
    // printf("packet recieved. len %d, type '%s'\n", packet->len, buf);
    return buf;
}

/* check if given char is alphanumeric. */
int isalphanumeric(char c){
    if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')){
        return 1;
    }else{
        return 0;
    }
}

/* return new char * with absolute path. need to manually free after use */
char* abs_path(const char *dir, const char *fname){
    char *path, *cp;
    path = (char *)Malloc(_strlen(dir)+_strlen(fname)+2);
    cp = path + _strlen(dir);
    _strcpy(path, dir);
    if(*(cp-1) != '/')
        *cp++ = '/';
    _strcpy(cp, fname);
    return path;
}

/* custom implementation of strcmp_s */
int _strcmp(const char* src, const char* trg, int len){
    int count=0;
    short diff;
    while(count<len){
        diff = *(src+count) - *(trg+count);
        if(diff != 0){
            return diff;
        }else if(*(src+count) == '\0'){
            return 0;
        }
        count++;
    }
    return 0;
}

char* _strcpy(char *dst, const char *src){
    const char *cp = src;
    char *dp = dst;
    while(*cp != '\0'){
        *dp++ = *cp++;
    }
    *dp = '\0';
    return dst;
}

/* custom immplemenation of strlen */
int _strlen(const char* src){
    const char *cp=src;
    int len=0;
    while(*cp++ != '\0')
        len++;
    return len;
}

/* custom implementation of strtok. delimiter is all non-alphanumeric words */
char* _strtok(char* buf){
    static char *next=NULL;
    char* cp;
    if(buf != NULL){
        next = buf;
    }
    if(next==NULL)
        return NULL;
    //find the start position of strtok
    while(!isalphanumeric(*next) && *next != '\0'){
        next++;
    }
    cp=next;
    while(isalphanumeric(*next)){
        next++;
    }
    if(*next == '\0')
        next=NULL;
    else
        *next++ = '\0';
    return cp;
}


/* parse command line input to arguments */
int parseline(char* buf, char* argv[]){
    int argc=0;
    char* cp=buf;
    while(*cp != '\n' && *cp != '\0' && argc<MAXARGS){
        while(*cp == ' '){
            cp++;
        }
        if(*cp == '\n') break;

        argv[argc] = cp;
        argc++;
        while(*cp != ' ' && *cp != '\n' && *cp != '\0'){
            cp++;
        }
        *cp++='\0';
    }
    return argc;
}

