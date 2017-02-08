#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Structrure for request information */
typedef struct {
    char hostname[MAXLINE];
    int port;
    char uri[MAXLINE];
}request_info;

/* data structure for elements in the cache
   it is doubly linked with pointers prev and next.
 */
struct cached_elem{
    struct cached_elem *prev;
    struct cached_elem *next;
    request_info tag;
    char used;
    unsigned char *object;
    size_t obj_length;
};

/* Function prototypes */
void init_cache();
struct cached_elem* fetch_element(request_info *req_hdr);
void insert_element(request_info *req_hdr,
                    unsigned char *object, size_t obj_length);
void update();
int issamehead(request_info *a, request_info *b);