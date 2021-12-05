#include <stdint.h>
#include "csapp.h"

#define PACKET_SIZE 8
#define MAXARGS 2

enum msg_type {REQ = 0x10, RESP = 0x11, ERROR = 0x20};

typedef struct {
    uint32_t len;
    uint32_t type;
} packet_t;

int send_error(int fd, char *msg);
int send_packet(int fd, char *msg, int type);
char *recieve_packet(rio_t *rio, int type, packet_t *packet);

int isalphanumeric(char c);

int _strlen(const char* src);
int _strcmp(const char* src, const char* trg, int len);
char* _strtok(char* buf);
char* _strcpy(char *dst, const char *src);

int parseline(char* buf, char* argv[]);
char* abs_path(const char *dir, const char *fname);