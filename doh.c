#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "doh.h"

/*
POST / HTTP/1.1
Host: 
*/

uint8_t http_header[] = {0x50, 0x4f, 0x53, 0x54, 0x20, 0x2f, 0x20, 0x48, 
0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0x0d, 
0x0a, 0x48, 0x6f, 0x73, 0x74, 0x3a, 0x20};

// localhost set
//0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74,


/*
User-Agent: curl-doh/1.0
Connection: Upgrade, HTTP2-Settings
Upgrade: h2c
HTTP2-Settings: AAMAAABkAARAAAAAAAIAAAAA
Content-Type: application/dns-message
Accept: application/dns-message
Content-Length: 
*/
uint8_t http_body[] = {0x0d, 0x0a, 0x55, 0x73, 0x65, 0x72, 
0x2d, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x3a, 0x20, 
0x63, 0x75, 0x72, 0x6c, 0x2d, 0x64, 0x6f, 0x68, 
0x2f, 0x31, 0x2e, 0x30, 0x0d, 0x0a, 

0x43, 0x6f, 0x6e, 0x74, 0x65, 
0x6e, 0x74, 0x2d, 0x54, 0x79, 0x70, 0x65, 0x3a, 
0x20, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 
0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x64, 0x6e, 0x73, 
0x2d, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 
0x0d, 0x0a, 0x41, 0x63, 0x63, 0x65, 0x70, 0x74, 
0x3a, 0x20, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 
0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x64, 0x6e, 
0x73, 0x2d, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 
0x65, 0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 
0x6e, 0x74, 0x2d, 0x4c, 0x65, 0x6e, 0x67, 0x74, 
0x68, 0x3a, 0x20};


uint8_t dns_header[] = {0x0d, 0x0a, 0x0d, 0x0a, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00};

#define DNS_END_LEN 4
uint8_t dns_end_ipv4[] = {0x00, 0x01, 0x00, 0x01};
uint8_t dns_end_ipv6[] = {0x00, 0x1c, 0x00, 0x01};

uint8_t buffer[BUFSIZE];


// FUNCIONES LOCALES
static size_t bincopy(uint8_t * target, uint8_t * source, size_t from, size_t n){
size_t i = from;
size_t j = 0;
for (; i < (from + n); i++)
{
  target[i]=source[j++];
}
return i;
}


static uint8_t * encodeName(size_t * size, uint8_t * name){
    // Back up name 
    size_t name_len = strlen((char *) name);
    uint8_t aux[name_len + 1];
    strcpy((char *) aux,(char *) name);


    uint8_t qname[MAXSTRINGLENGTH][MAXSTRINGLENGTH];
    size_t j = 0;
    char * token = strtok((char *) aux, ".");
    while( token != NULL ) {
      sprintf((char *) qname[j++],"%s", token );
      token = strtok(NULL, ".");
   }
    
    uint8_t * result = malloc(MAXSTRINGLENGTH);
    if(result==NULL)
    return result;
    size_t k = 0;
    for (size_t i = 0; i < j; i++){
    size_t sub_len = strlen((char *) qname[i]);
    result[k++] = sub_len;
    bincopy(result+k,qname[i],0,sub_len);
    k+=sub_len;
    }
    result[k++]=0;

    *size = k;
    result = realloc(result,k);
    if(result==NULL){
      return result;
    }
    return result;
}

/*******************************/


// ITERFACE

uint8_t * getRequest (ssize_t * len, uint8_t * name, int version, uint8_t * dns_hostname ){
  
  uint8_t * request = malloc(REQ_MAXSIZE);
  if(request==NULL){
    return request;
  }
  size_t i = 0;
  i = bincopy(request,http_header,i,sizeof(http_header));
  size_t dns_hostname_len = strlen((char *) dns_hostname);
  i = bincopy(request,dns_hostname,i,dns_hostname_len);
  i = bincopy(request,http_body,i,sizeof(http_body));

  // encode name into dns message format 
  size_t qname_len=0;
  uint8_t * qname = encodeName(&qname_len,name);
  size_t dns_message_len = (sizeof(dns_header) + qname_len );
  uint8_t string_len[MAXSTRINGLENGTH]={0};
  i = bincopy(request,string_len,i,sprintf((char *) string_len,"%ld",dns_message_len));
  i = bincopy(request,dns_header,i,sizeof(dns_header));

  i = bincopy(request,qname,i,qname_len);
  if(version==IPV4_VER){
      i = bincopy(request,dns_end_ipv4,i,DNS_END_LEN);
  } else if(version==IPV6_VER){
  i = bincopy(request,dns_end_ipv6,i,DNS_END_LEN);
  }else
  {
    free(qname);
    free(request);
    return NULL;
  }
  request = realloc(request,i);
  if(request==NULL){
    return request;
  }
  *len = i;
  free(qname);
  return request;
}

/*******************************/
