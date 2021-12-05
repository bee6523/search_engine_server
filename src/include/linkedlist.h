typedef struct LinkedList{
    struct node_t *head;
    struct node_t *tail;
    int len;
} LinkedList;

typedef struct node_t{
    char *term;
    int docid;
    int line;
    struct node_t *next;
} node_t;

LinkedList *list_init();
void list_push(LinkedList *list, char *term, int docid, int line);
void list_sort(LinkedList *list);
void list_free(LinkedList *list);
void list_print(LinkedList *list);