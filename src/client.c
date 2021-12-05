#include "csapp.h"
#include "utils.h"

int req_per_th;

void* worker(void *argp);

void eval(int clientfd, rio_t *rio, char *buf){
    int argc, i;
    char *argv[MAXARGS], *ret;
    char *tok[4];
    packet_t packet;

    argc = parseline(buf, argv);
    if(_strcmp(argv[0],"exit",5)==0){
        exit(0);
    }else if(_strcmp(argv[0], "search", 7)==0){
        if(argc>2){
            printf("ERROR: too many arguments\nsyntax is 'search [QUERY WORD]'\n");
            return;
        }
        send_packet(clientfd, argv[1], REQ);
        while(1){
            ret = recieve_packet(rio, RESP, &packet);
            if(ret == NULL){
                if(packet.type == ERROR){
                    if(packet.len != PACKET_SIZE){ //if there is error message, print it
                        ret = Calloc(packet.len-7, 1);
                        Rio_readnb(rio, ret, packet.len - PACKET_SIZE);
                        fprintf(stderr, "ERROR: recieved error packet\nERROR CODE:%s\n", ret);
                        Free(ret);
                    }else{
                        fprintf(stderr,"ERROR: recieved error packet\n");
                    }
                }else{
                    fprintf(stderr, "ERROR: unknown error occured\n");
                }
                break;
            }
            argc = parseline(ret, tok);
            if(argc != 2){
                printf("Error: something went wrong\n");
            }
            if(atoi(tok[1])==-1) //termcode recieved
                break;
            printf("%s: line #%d\n", tok[0], atoi(tok[1]));
            Free(ret);
        }
    }
}

/* usage: ./client1 <server_ip> <server_port> */
int main(int argc, char **argv)
{
    char *pgm_name, *host, *port, *query, buf[MAXLINE];
    int num_threads, i;
    pgm_name = argv[0];
    host = argv[1];
    port = argv[2];
    
    int clientfd;
    rio_t rio;
    clientfd = Open_clientfd(host, port);
    rio_readinitb(&rio, clientfd);
    tc_central_init();
    tc_thread_init();

    while(1){
        printf("%s> ", pgm_name);
        Fgets(buf, MAXLINE, stdin);
        eval(clientfd, &rio, buf);
    }
}