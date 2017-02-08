/*
    Andrew ID: tianyum
    Name: Tianyu Mi
    Date: 4/Dec/2014 
 */

/*
   this is a small web proxy that supports multiple concurrent
   requests and caching.
   Generally, it parses requests from clients, forward them to the web
   server,reads the server's response and eventually forwards the 
   response to the client.
   The cache feature enables the proxy to check if the requested 
   object is in the cache or not. if it is, proxy gets it from cache
   and forwards it to the client; otherwise, it sends request to the 
   server and get response from server, store it in the cache and 
   pass it to the client.
   The multi-threads feature supports concurrent requests from multiple
   clients.
 
 */

#include <stdio.h>
#include "cache.h"

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";


/* return an error message to proxy's client */
void clienterror(int fd, char *cause, char *num,
                 char *s_message, char *l_message)
{
    char buf[MAXLINE],body[MAXLINE];
    
    /* build body */
    sprintf(body,"%s%s: %s\r\n",body,num,s_message);
    sprintf(body,"%s<p>%s: %s\r\n", body, l_message, cause);

    
    /* print response*/
    sprintf(buf, "HTTP/1.0 %s %s\r\n",num,s_message);
    rio_writen(fd,buf,strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd,buf,strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n",(int)strlen(body));
    rio_writen(fd,buf,strlen(buf));
    rio_writen(fd,body,strlen(body));
}


/* forward request to web server */
void sendreq(request_info req_head, char *request, int client_fd)
{
    int proxy_clientfd;
    unsigned char buf[MAXLINE];
    unsigned char object[MAX_OBJECT_SIZE];
    size_t obj_length, n, i;
    rio_t rio;
    int discard;
    
    proxy_clientfd=Open_clientfd(req_head.hostname,req_head.port);
    if (proxy_clientfd<0){
        puts("open_clientfd error.\n");
        return;
    }
    Rio_readinitb(&rio,proxy_clientfd);
    
    rio_writen(proxy_clientfd,request,strlen(request));
    
    obj_length=0;
    discard=0;
    while ((n=rio_readlineb(&rio,buf,MAXLINE))!=0) {
        rio_writen(client_fd, buf, n);
        if (!discard) {
            if (obj_length+n<=MAX_OBJECT_SIZE) {
                for(i=0;i<n;i++){
                    object[obj_length ++]=buf[i];
                }
            }
            else{
                discard=1;
            }
        }
    }
    Close(proxy_clientfd);
    if (discard==0) {
        insert_element(&req_head,object,obj_length);
    }
}

/* make the request and forward it to the web server */
void serve_help(char *hostname,char *uri, request_info req_head,
                int fd, int port, char *request, char *method, rio_t rio)
{
    
    /* make request */
    strcpy(request,"");
    sprintf(request,"%s %s HTTP/1.0\r\n",method,uri);

    rio_t *rp= &rio;
    char buf[MAXLINE];
    /* to find whether request has its own host header.
     0 means false, 1 means true */
    int hashost=0;
    rio_readlineb(rp,buf,MAXLINE);
    if (strstr(buf,"Host:")) {
        hashost=1;
        sprintf(request,"%s%s",request,buf);
    }
    while (strcmp(buf,"\r\n")) {
        rio_readlineb(rp,buf,MAXLINE);
        if (strstr(buf,"Host:")) {
            hashost=1;
            sprintf(request,"%s %s",request,buf);
        }
    }
    
    if (hashost==0) {
        sprintf(request,"%sHost: %s\r\n",request,hostname);
    }
    
    sprintf(request,"%s%s",request,user_agent_hdr);
    sprintf(request,"%s%s",request,accept_hdr);
    sprintf(request,"%s%s",request, accept_encoding_hdr);
    sprintf(request,"%sConnection: close\r\n",request);
    sprintf(request,"%sProxy-Connection:close\r\n",request);
    sprintf(request,"%s\r\n",request);
    
    /* find req web object in web cache */
    strcpy(req_head.hostname,hostname);
    strcpy(req_head.uri,uri);
    req_head.port=port;
    
    /* if object is in the cache, send it to client directly */
    struct cached_elem *cached_elem;
    if ((cached_elem=fetch_element(&req_head))) {
        rio_writen(fd,cached_elem->object,cached_elem->obj_length);
        return;
    }
    /* if object is not in the cache, forward request to web server */
    sendreq(req_head,request,fd);
}

/* do with a clent's request */
void serve(int fd)
{
    char method[MAXLINE],url[MAXLINE],version[MAXLINE];
    char buf[MAXLINE],hostname[MAXLINE],uri[MAXLINE];
    char request[MAXLINE];
    
    int port=80;
    rio_t rio;
    request_info req_head={"",port,""};
    

    /* read in a request from client */
    Rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf,MAXLINE);
    /* change buffer into string*/
    sscanf(buf,"%s %s %s",method,url,version);
    
    if(strcasecmp(method,"GET"))
    {
        printf("want something other than GET\n");
        clienterror(fd,method,"501","request not implemented",
                    "none");
        return;
    }
    /* get hostname, port and uri from url */
    if (strstr(url,"http")) {
        char *ptr=strchr(url,'/');
        char *uri_orig=strchr(ptr+2,'/');
        char *port_orig=strchr(ptr,':');
        ptr =ptr+2;
        
        if (uri_orig) {
            *uri_orig='\0';
            uri_orig++;
            strcpy(uri,"/");
            strcat(uri,uri_orig);
        }
        else{
            strcpy(uri,"/");
        }
        
        if (port_orig) {
            *port_orig='\0';
            port_orig ++;
            port=atoi(port_orig);
        }
        strcpy(hostname,ptr);
        
    }
    
    serve_help(hostname,uri,req_head,fd,port,request,method,rio);

}


/* handle a single transaction */
void *thread(void *arg)
{
    int connfd=*((int *)arg);
    Pthread_detach(Pthread_self());
    Free(arg);
    serve(connfd);
    Close(connfd);
    return NULL;
}


void terminate(int sig){
    puts("Proxy ignores SIGPIPE \n");
}

int main(int argc, char **argv)
{
    printf("start proxy information\n");
    printf("%s%s%s", user_agent_hdr, accept_hdr, accept_encoding_hdr);
    printf("end proxy information\n");
    
    int listenfd, *connfdp, port, clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;
    
    
    Signal(SIGPIPE, terminate);
    
    /* cache initialization */
    init_cache();
    
    /* check cmd line args */
    if (argc != 2) {
        fprintf(stderr,"usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* initialize listening port */
    port=atoi(argv[1]);
    listenfd = Open_listenfd(port);

    while (1) {
        clientlen=sizeof(clientaddr);
        connfdp= Calloc(1,sizeof(int));
        *connfdp=Accept(listenfd, (SA *)&clientaddr,(socklen_t *)&clientlen);
        Pthread_create(&tid,NULL,thread,connfdp);
        
    }
    
    return 0;
}









































