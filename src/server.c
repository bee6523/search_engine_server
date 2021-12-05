#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <semaphore.h>
#include "utils.h"
#include "invidx.h"
#include "linkedlist.h"
#include "csapp.h"

#define NUM_THREADS 10

enum state {IDLE, ALLOC};

typedef struct {
    int maxfd;
    fd_set ready_set;
    fd_set read_set;
    int nready;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool_t;

typedef struct {
    enum state status;
    char *job;
    int fd;
} thread_t;

int num_docs;
LinkedList *doclist;
dict_t *dict;

dict_t* dict_create(LinkedList *list);
void bootstrap(char *dirpath);

void thread_init(thread_t *t){
    t->job = NULL;
    t->status=IDLE;
}

void *worker(void *arg){
    thread_t *th = (thread_t *)arg;
    char *argv[MAXARGS];
    char buf[MAXLINE];
    char *query;
    int argc, i;
    node_t *docnode;
    printf("worker online\n");
    tc_thread_init();
    while(1){
        while(th->job == NULL){
            usleep(1);//spinlock
        }
        query = th->job;
        term_t *term = dict_search(dict->head, query, 0);
        if(term==NULL){  //term not found
            sprintf(buf, ". -1");//term code
            send_packet(th->fd, buf, RESP);
            Free(th->job);
            th->job = NULL;
            continue;
        }
        for(index_t *it = term->posting_head; it != NULL; it=it->next){
            docnode = doclist->head;
            for(i=0;i<it->docid;i++){
                docnode = docnode->next;
            }
            sprintf(buf, "%s %d", docnode->term, it->line);
            send_packet(th->fd, buf, RESP);
        }
        sprintf(buf, ". -1");//term code
        send_packet(th->fd, buf, RESP);
        //printf("send packet len %d", _strlen(th->job));
        Free(th->job);
        th->job = NULL;
    }
}

int main(int argc, char **argv)
{
    int listenfd, connfd, i, cursor=0;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp, *port;
    unsigned short client_port;
    unsigned int ip;

    tc_central_init();
    tc_thread_init();
    pool_t *pool = Malloc(sizeof(pool_t));
    pthread_t pids[NUM_THREADS];
    thread_t threads[NUM_THREADS];


    port = argv[2];

    bootstrap(argv[1]);

    printf("creating worker threads..\n");
    //worker thread created
    for(i=0; i<NUM_THREADS; i++){
        thread_init(&threads[i]);
        Pthread_create(&pids[i], NULL, worker, &threads[i]);
    }
    
    listenfd = Open_listenfd(port);
    pool->maxfd = listenfd;
    FD_ZERO(&pool->read_set);
    FD_SET(listenfd, &pool->read_set);
    for(i=0;i<FD_SETSIZE; i++){
        pool->clientfd[i] = -1;
    }

    printf("everything set. start server\n");
    while(1){
        pool->ready_set = pool->read_set;
        pool->nready = Select(pool->maxfd+1, &pool->ready_set, NULL, NULL, NULL);
        
        clientlen = sizeof(clientaddr);
        if(FD_ISSET(listenfd, &pool->ready_set)){
            for(i=0;i<FD_SETSIZE;i++){
                if(pool->clientfd[i]<0)
                    break;
            }
            if(i<FD_SETSIZE){  //if fdset was available, accept connection
                connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
                // hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
                haddrp = inet_ntoa(clientaddr.sin_addr);
                client_port = ntohs(clientaddr.sin_port);
                pool->clientfd[i] = connfd;
                Rio_readinitb(&pool->clientrio[i], connfd);
                FD_SET(connfd, &pool->read_set);
                if(pool->maxfd < connfd){
                    pool->maxfd = connfd;
                }
                // printf("server connected to %s (%s), port %u, fd %d\n", hp->h_name, haddrp, client_port, connfd);
            }
        }
        // printf("check if client is ready\n");
        packet_t packet;
        char *buf;
        for(int i=0; i<FD_SETSIZE; i++){
            connfd = pool->clientfd[i];
            if((connfd > 0) && FD_ISSET(connfd, &pool->ready_set)){
                // printf("connected. fd: %d\n",connfd);
                if((buf = recieve_packet(&pool->clientrio[i], REQ, &packet)) == NULL){
                    FD_CLR(connfd, &pool->read_set);
                    pool->clientfd[i] = -1;
                    Close(connfd);
                    continue;
                }
                
                
                //allocate job to thread
                while(1){
                    cursor = (cursor+1)%NUM_THREADS;
                    if(threads[cursor].job==NULL){
                        threads[cursor].fd = connfd;
                        threads[cursor].job = buf;
                        break;
                    }
                }
            }
        }

    }
}

/* create inverted list dictionary from sorted linked list */
dict_t* dict_create(LinkedList *list){
    dict_t *dict = dict_init();
    term_t *tp = dict->head; // dictionary

    for(node_t *np = list->head; np != NULL; np=np->next){
        tp = dict_search(tp, np->term, 1);
        posting_push(tp, np->docid, np->line);
    }
    return dict;
}

void bootstrap(char *dirpath){
    DIR *dir;
    FILE *file;
    char buf[MAXLINE];
    struct dirent *ent;
    
    /* bootstrapping */
    num_docs = 0;
    LinkedList *list = list_init();
    doclist = list_init();
    dir = opendir(dirpath);
    if(dir != NULL){
        while((ent = readdir(dir)) != NULL){
            if(_strcmp(ent->d_name, ".", 2) != 0 && _strcmp(ent->d_name, "..", 3) != 0){
                //open file
                char *path = abs_path(dirpath, ent->d_name);
                file = fopen(path, "r");
                if(file == NULL){
                    fprintf(stderr, "inverted_index: cannot open file %s\n", path);
                    Free(path);
                    continue;
                }
                list_push(doclist, path, num_docs, 0);
                Free(path);

                //read, tokenize, add to list
                int line=1;
                while(Fgets(buf, MAXLINE, file) != NULL){
                    char *cp = _strtok(buf);
                    do{
                        if(*cp != '\0'){
                            list_push(list, cp, num_docs, line);
                        }
                    } while((cp = _strtok(NULL)) != NULL);

                    if(_strlen(buf) == MAXLINE-1 && *(buf+MAXLINE-2) != '\n') 
                        continue;
                    line++;
                }                
                Fclose(file);
                num_docs++;
            }
        }
        closedir(dir);
    }
    list_sort(list);
    dict = dict_create(list);
    list_free(list);

    printf("*** Bootstrapping Complete: %d file(s) loaded ***\n", num_docs);
}