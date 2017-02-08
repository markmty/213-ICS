/*
 * andrew id: tianyum
 * date : Oct,7,2014
 *
 */

#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>


int s , b , E , t;
int hits = 0, miss = 0, evicts = 0;
char *tracename;


/* cache data structure:
 
*Typically, each cache contains S= 2^s sets,
 each set contains E =2^e lines,
 each line contains a valid sign, a tag and cache block.
 
*Therefore, cache stores a head-set pointer, an int S;
 set stores a head-line pointer, a next-set pointer, an int E;
 line stores a next-line pointer, an int v, and a tag.
 
 */
struct cacheline {
    int v;
    unsigned t;
    struct cacheline *next;
};

struct cacheset {
    int E;
    struct cacheline *head;
    struct cacheset *next;
};

struct cache {
    int S;
    struct cacheset *head;
};

/* get the parameter using getopt library */
void getinput(int argc, char **argv, int opt){
    int m = sizeof(long) *8;
    
    while ((opt = getopt(argc,argv,"s:E:b:t:"))!= -1) {
        switch (opt) {
            case 's':
                s = atoi (optarg);
                if (s < 0 && s > m) {
                    exit(-1);
                }
                break;
            case 'E':
                E = atoi (optarg);
                if (E <= 0) {
                    exit(-1);
                }
                break;
            case 'b':
                b = atoi (optarg);
                if (b < 0 || b > m) {
                    exit(-1);
                }
                break;
            case 't':
                tracename=optarg;
                break;
            default:
                break;
        }
    }
}

/* using malloc to initialize the target cache given the cache pointer, s, E*/
void initialize(struct cache *cache,int s,int E) {
    cache -> S = (1 << s);
    struct cacheset *setinit = (struct cacheset*)malloc(sizeof(struct cacheset));
    setinit -> E= E;
    setinit -> head = NULL;
    setinit -> next = NULL;
    cache -> head = setinit;
    for (int i = 1; i < cache -> S; i++) {
       struct cacheset *setnext = (struct cacheset*) malloc (sizeof(struct cacheset));
        setnext -> E = E;
        setnext -> head = NULL;
        setnext -> next = NULL;
        setinit -> next = setnext;
        setinit = setnext;
    }
}

/* calculate the number of lines in the given set and return the number */
int numofline (struct cacheset *set) {
    struct cacheline *nolhelper = set->head;
    int size = 0;
    while (nolhelper != NULL){
        nolhelper = nolhelper -> next;
        size++;
    }
    return size;
}

/* simulate the work of cache */
void cachework (struct cache *cache, unsigned address) {
    int tag = address >> (s + b);
    int setindex = ((1 << s) -1) & (address >>b);
    
    struct cacheset *target = cache -> head;
    for (int i=0; i<setindex; i++) {
        target = target -> next;
    }
    
    struct cacheline *line = target -> head;
    struct cacheline *linehelper1  = NULL;

    while (line != NULL) {
        
        /* when valid=1 and the tag matches, hit! 
           move the line to the head;
         */
        if (line -> v  && (line -> t == tag)) {
            hits++;
            if (linehelper1 !=NULL){
                linehelper1 -> next= line -> next;
                line -> next = target -> head;
                target -> head = line;
            }
            return;
        }
        linehelper1 = line;
        line = line -> next;
    }
    
    struct cacheline *newline = (struct cacheline *) malloc (sizeof(struct cacheline));
    newline -> v =1;
    newline -> t = tag;
    
    /* when the vacancy of the target set is not full, miss!
       just add a new line to the head;
     */
    if (numofline(target) != target->E) {
        newline -> next = target -> head;
        target -> head = newline ;
        miss++;
    }
    
    /* when the target set is full, miss! and evict!
       Due to LRU, delete the last line and add a new line to the head.
     */
    else {
        struct cacheline *linehelper2 = target -> head;
        struct cacheline *linehelper3 = NULL;
        struct cacheline *linehelper4 = NULL;
        while (linehelper2 != NULL) {
            linehelper4 = linehelper3 ;
            linehelper3 = linehelper2 ;
            linehelper2 = linehelper2 -> next;
        }
        if (linehelper4 != NULL) {
            linehelper4 -> next = NULL;
        }
        else {
            target -> head = NULL;
        }
        if (linehelper3 !=NULL) free(linehelper3);
        
        
        newline -> next = target -> head;
        target -> head = newline ;
        
        miss++;
        evicts++;
    }

}

/* free the cache and return the space back to heap */
void deinit(struct cache *cache) {
    struct cacheset *setfree = cache -> head;
    while (!setfree) {
        struct cacheline *linefree = setfree -> head;
        while (!linefree) {
            struct cacheline *linefreehelper = linefree;
            linefree = linefreehelper->next;
            free(linefreehelper);
        }
        struct cacheset *setfreehelper = setfree;
        setfree = setfreehelper->next;
        free(setfreehelper);
    }
    free(cache);
}


int main (int argc, char** argv) {
    char op;
    unsigned addr;
    int size;
    int opt=0;

  
    getinput(argc, argv, opt);
    struct cache *thecache = (struct cache *) malloc (sizeof(struct cache));
    initialize(thecache, s, E);
    
    FILE *fp = fopen (tracename, "r");
    while(fscanf(fp, "%c %x, %d", &op, &addr, &size)>0){
        switch (op) {
            case 'L':
                cachework(thecache,addr);
                break;
            case 'S':
                cachework(thecache,addr);
                break;
            case 'M':
                cachework(thecache,addr);
                cachework(thecache, addr);
                break;
            default:
                break;
        }
    }
        
    
    fclose(fp);
    deinit(thecache);
    printSummary (hits,miss,evicts);
    
    return 0;
    
    
    
    
}