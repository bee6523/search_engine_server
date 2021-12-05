typedef struct dict_t{
    struct term_t *head;
} dict_t;

typedef struct term_t{
    char *value;
    int doc_freq;
    struct index_t *posting_head;
    struct index_t *posting_tail;
    struct term_t *next;
} term_t;

typedef struct index_t{
    int docid;
    int line;
    struct index_t *next;
} index_t;

dict_t* dict_init();
void dict_free(dict_t* dict);
term_t* dict_search(term_t* term_head, char *term, char create);
void posting_push(term_t *term, int docid, int line);