#include "cache.h"

/* Global variables */
static struct cached_elem *cache_list=NULL;
static size_t cache_size=0;
static int count =0;
static sem_t mutex,w;

/* initialize cache list */
void init_cache()
{
    cache_list =NULL;
    cache_size =0;
    count = 0;
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0 ,1);
}

/* use the new object and tag to create new element
 insert it into the cache list */
void insert_element(request_info *req_hdr, unsigned char *object,
                    size_t obj_length)
{
    /* lock cache list for writing */
    P(&w);
    
    /* formulate new cache element*/
    struct cached_elem *new_elem=
    (struct cached_elem *)Malloc(sizeof(struct cached_elem));
    strcpy(new_elem->tag.hostname, req_hdr->hostname);
    strcpy(new_elem->tag.uri, req_hdr->uri);
    new_elem->tag.port=req_hdr->port;
    new_elem->object =
    (unsigned char *)Malloc(sizeof(unsigned char)* obj_length);
    
    size_t i;
    for (i=0; i<obj_length; i++) {
        new_elem->object[i]=object[i];
    }
    
    new_elem->used=1;
    new_elem->obj_length=obj_length;
    
    cache_size = cache_size + obj_length;
    /* if size exceeds limit, evict*/
    while(cache_size>MAX_CACHE_SIZE){
        struct cached_elem *cur=cache_list;
        /* get the tail */
        while ((cur->next)!=0) {
            cur = cur->next;
        }
        /* remove the tail */
        if(cur->prev){
            cur->prev->next=cur->next;
        }
        if (cur->next) {
            cur->next->prev=cur->prev;
        }
        cache_size= cache_size - (cur->obj_length);
        Free(cur);
    }
    
    new_elem->prev=NULL;
    new_elem->next =cache_list;
    if (cache_list) {
        cache_list->prev =new_elem;
    }
    cache_list=new_elem;
    
    
    /*unlock cache list*/
    V(&w);
    
    
}


/* check if 2 tags are the same
 if it is return 1; otherwise return 0*/
int issamehead(request_info *a, request_info *b)
{
    if (!strcmp(a->hostname,b->hostname)) {
        if (!strcmp(a->uri,b->uri)) {
            if (a->port == b-> port) {
                return 1;
            }
        }
    }
    return 0;
}

/* find cached object matching request header */
struct cached_elem* fetch_element(request_info *req_hdr)
{
    /*lock*/
    P(&mutex);
    count++;
    if (count == 1) {
        /*first reader lock for writing */
        P(&w);
    }
    
    /* unlock */
    V(&mutex);
    /* find cached element */
    struct cached_elem *cur= cache_list;
    while (cur) {
        if (issamehead(req_hdr, &(cur->tag))) {
            cur->used =1;
            break;
        }
        cur=cur->next;
    }
    
    /* lock */
    P(&mutex);
    count--;
    if (count==0) {
        
        /* update the cache when all readers finish reading */
        struct cached_elem *cur = cache_list;
        struct cached_elem *next;
        while (cur) {
            if (cur->used) {
                cur->used =0;
                /*first element*/
                if (cur==cache_list) {
                    cur = cur->next;
                    continue;
                }
                /* remove the element */
                if(cur->prev){
                    cur->prev->next=cur->next;
                }
                if (cur->next) {
                    cur->next->prev=cur->prev;
                }
                next=cur->next;
                /* move it to the front */
                cur->prev =NULL;
                cur->next = cache_list;
                if (cache_list) {
                    cache_list->prev =cur;
                }
                cache_list = cur;
                
                cur=next;
            }
            else{
                cur=cur->next;
            }
        }
        V(&w);
    }
    /*unlock*/
    V(&mutex);
    return cur;
}



