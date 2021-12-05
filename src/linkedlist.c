#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "linkedlist.h"
#include "csapp.h"

LinkedList *list_init(){
    LinkedList *list = (LinkedList *)Malloc(sizeof(LinkedList));
    list->head = NULL;
    list->tail = NULL;
    list->len = 0;
    return list;
}

node_t *node_init(char *term, int docid, int line){
    node_t *node = (node_t *)Malloc(sizeof(node_t));
    node->term = (char *)Malloc(_strlen(term)+1);
    node->docid = docid;
    node->line = line;
    _strcpy(node->term, term);
    return node;
}

void node_free(node_t *node){
    Free(node->term);
    Free(node);
}

void list_push(LinkedList *list, char *term, int docid, int line){
    node_t *node = node_init(term, docid, line);
    if(list->tail != NULL)
        list->tail->next = node;
    else if(list->head == NULL)
        list->head = node;
    list->tail = node;
}

/* return 1 if p is bigger, -1 if q is bigger */
int node_cmp(node_t *p, node_t *q){
    int diff = _strcmp(p->term, q->term, _strlen(p->term)+1);
    if(diff==0){
        diff = p->docid - q->docid;
        if(diff==0)
            diff = p->line - q->line;
    }
    return diff;
}

/* sort list using mergesort */
void list_sort(LinkedList *list){
    node_t *p, *q, *e, *newhead, *newtail;
    int k, mergecount, i;

    k=1;
    newhead = list->head;
    do {
        mergecount=0;
        p = newhead;
        newhead = NULL;
        newtail = NULL;

        while(1){
            if(p==NULL){
                break;
            }
            mergecount++;

            q=p;

            int psize=0, qsize=k;
            for(i=0;i<k && q != list->tail; i++){
                q = q->next;
                psize += 1;
            }
            while(psize>0 || (qsize>0 && q != NULL)){
                if (psize==0){
                    e = q;
                    q = q->next;
                    qsize--;
                }else if(qsize==0 || q==NULL){
                    e = p;
                    p = p->next;
                    psize--;
                }else{
                    /* compare two node */
                    if(node_cmp(p,q) < 0){
                        e = p;
                        p = p->next;
                        psize--;
                    }else{
                        e = q;
                        q = q->next;
                        qsize--;
                    }
                }
                /* push selected node to newlist */
                if(newtail != NULL)
                    newtail->next = e;
                else if(newhead == NULL)
                    newhead = e;
                newtail = e;
                e->next = NULL;
            }
            p=q;
        }
        k=k*2;
    } while(mergecount > 1);
    list->head = newhead;
    list->tail = newtail;
}

void list_free(LinkedList *list){
    node_t *p, *next;
    p = list->head;
    while(p != NULL){
        /* TODO: free elements in node */
        next = p->next;
        node_free(p);
        p = next;
    }
    Free(list);
}

void list_print(LinkedList *list){
    node_t *p = list->head;
    int i=0;
    while(p != NULL){
        printf("node %d : %s, %d-%d\n", i++, p->term, p->docid, p->line);
        p = p->next;
    }
}