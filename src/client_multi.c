#include "csapp.h"
#include "utils.h"

char *host, *port, *query;
int req_per_th;

void* worker(void *argp);

/* usage: ./client1 <server_ip> <server_port> <number_of_threads> <number_of_requests_per_thread> <word_to_search> */
int main(int argc, char **argv)
{
    int num_threads, i;
    host = argv[1];
    port = argv[2];
    num_threads = atoi(argv[3]);
    req_per_th = atoi(argv[4]);
    query = argv[5];
    printf("creating threads\n"); fflush(stdout);
    tc_central_init();
    pthread_t pids[num_threads];
    for(i=0; i<num_threads; i++){
        Pthread_create(&pids[i], NULL, worker, NULL);
    }

    for(i=0; i<num_threads; i++){
        Pthread_join(pids[i], NULL);
    }
    printf("all request complete\n"); fflush(stdout);
    exit(0);
}


void* worker(void *argp){
    char *ret;
    packet_t packet;
    
    int clientfd;
    rio_t rio;

    int argc;
    char *tok[2];
    tc_thread_init();
    clientfd = Open_clientfd(host, port);
    rio_readinitb(&rio, clientfd);
    for(int i=0; i<req_per_th; i++){
        send_packet(clientfd, query, REQ);
        // printf("wait\n"); fflush(stdout);
        while(1){
            ret = recieve_packet(&rio, RESP, &packet);

            if(ret == NULL){
                if(packet.type == ERROR){
                    if(packet.len != PACKET_SIZE){ //if there is error message, print it
                        ret = Calloc(packet.len-7, 1);
                        Rio_readnb(&rio, ret, packet.len - PACKET_SIZE);
                        fprintf(stderr, "ERROR: recieved error packet\nMSG from server: %s\n", ret);
                        Free(ret);
                        break;
                    }else{
                        fprintf(stderr,"ERROR: recieved error packet\n");
                        break;
                    }
                }else{
                    fprintf(stderr, "ERROR: unknown error occured\n");
                    break;
                }
                return NULL;
            }
            argc = parseline(ret, tok);
            if(atoi(tok[1])==-1)
                break;
            Free(ret);
        }
        // printf("recieved!\n");
    }
    Close(clientfd);
    fflush(stdout);
    return NULL;
}