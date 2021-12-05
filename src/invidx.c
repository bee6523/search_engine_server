#include <stdio.h>
#include <stdlib.h>
#include "invidx.h"
#include "csapp.h"
#include "linkedlist.h"
#include "utils.h"


term_t* term_init(char *t){
    term_t *term = (term_t *)Malloc(sizeof(term_t));
    term->doc_freq = 0;
    term->posting_head = NULL;
    term->posting_tail = NULL;
    term->next = NULL;
    if(t != NULL){
        term->value = (char *)Malloc(_strlen(t)+1);
        _strcpy(term->value, t);
    }else term->value = NULL;
    return term;
}

void term_free(term_t *tp){
    index_t *ip, *next;
    ip=tp->posting_head;
    while(ip != NULL){
        next = ip->next;
        Free(ip);
        ip = next;
    }
    Free(tp->value);
    Free(tp);
}

index_t* index_init(int docid, int line){
    index_t *idx = (index_t *)Malloc(sizeof(index_t));
    idx->docid = docid;
    idx->line = line;
    idx->next = NULL;
    return idx;
}

dict_t* dict_init(){
    dict_t *dict = (dict_t *)Malloc(sizeof(dict_t));
    dict->head = term_init(NULL);
    return dict;
}

void dict_free(dict_t* dict){
    /* TODO: remove all elements */
    term_t *tp, *next;
    tp = dict->head->next;
    while(tp != NULL){
        next = tp->next;
        term_free(tp);
        tp = next;
    }
    Free(dict->head);
    Free(dict);
}

/* search term in invidx dictionary. 
create new head_t and return it if create=1(True) */
term_t* dict_search(term_t* term_head, char *term, char create){
    term_t *prev = NULL;
    term_t * tp = term_head;
    int diff;

    //skip if tp is list->head(value is null)
    if(tp != NULL && tp->value == NULL){
        prev = tp;
        tp = tp->next;
    }
    while(tp != NULL){
        diff = _strcmp(tp->value, term, _strlen(tp->value)+1);
        if (diff == 0){
            return tp;
        }else if(diff > 0){
            break;
        }
        prev = tp;
        tp = tp->next;
    }
    if(create){
        term_t* newterm = term_init(term);
        newterm->next = prev->next;
        prev->next = newterm;
        return newterm;
    }
    return NULL;
}

void posting_push(term_t *term, int docid, int line){
    index_t *new_idx = index_init(docid, line);
    if(term->posting_tail != NULL)
        term->posting_tail->next = new_idx;
    else if(term->posting_head==NULL)
        term->posting_head = new_idx;
    term->posting_tail = new_idx;
    term->doc_freq++;
}