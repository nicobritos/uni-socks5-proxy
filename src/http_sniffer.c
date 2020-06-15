#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "http_sniffer.h"

#define CHUNK_SIZE 10

/* Funciones auxiliares */
void * resize_if_needed(void * ptr, size_t ptr_size, size_t current_length);
char * add_char(char ** ans, char c, size_t * current_length);
struct http_credentials * error(struct http_credentials * ans, error_t error_type);
char * decodeBase64(const char * encoded, error_t * error_code);
char base64table(const char c);
error_t parseUserPass(struct http_credentials * ans, char * s);

// definición de maquina

enum states {
    ST_START,
    ST_HEAD_R,
    ST_HEAD,
    ST_A,
    ST_U,
    ST_T,
    ST_H,
    ST_O,
    ST_R,
    ST_I,
    ST_Z,
    ST_A_2,
    ST_T_2,
    ST_I_2,
    ST_O_2,
    ST_N,
    ST_COLON,
    ST_SPACE,
    ST_B,
    ST_A_3,
    ST_S,
    ST_I_3,
    ST_C,
    ST_AUTH,
    ST_AUTH_R,
    ST_FINISHED,
    ST_INVALID_INPUT_FORMAT,
};

enum event_type {
    SUCCESS,
    COPY_ENCODED_AUTH,
    END_ENCODED_AUTH,
    COPY_ENCODED_AUTH_R,
    INVALID_INPUT_FORMAT_T,
};

static void
next_state(struct parser_event *ret, const uint8_t c) {
    ret->type    = SUCCESS;
    ret->n       = 1;
    ret->data[0] = c;
}

static void
copy_auth(struct parser_event *ret, const uint8_t c) {
    ret->type    = COPY_ENCODED_AUTH;
    ret->n       = 1;
    ret->data[0] = c;
}

static void
end_auth(struct parser_event *ret, const uint8_t c) {
    ret->type    = END_ENCODED_AUTH;
    ret->n       = 1;
    ret->data[0] = c;
}

static void
copy_auth_r(struct parser_event *ret, const uint8_t c) {
    ret->type    = COPY_ENCODED_AUTH_R;
    ret->n       = 1;
    ret->data[0] = c;
}

static void
invalid_input(struct parser_event *ret, const uint8_t c) {
    ret->type    = INVALID_INPUT_FORMAT_T;
    ret->n       = 1;
    ret->data[0] = c;
}

static const struct parser_state_transition START [] =  {
    {.when = '\r',       .dest = ST_HEAD_R,                     .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition HEAD_R [] =  {
    {.when = '\n',       .dest = ST_HEAD,                       .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition HEAD [] =  {
    {.when = 'A',        .dest = ST_A,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition A [] =  {
    {.when = 'u',        .dest = ST_U,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition U [] =  {
    {.when = 't',        .dest = ST_T,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition T [] =  {
    {.when = 'h',        .dest = ST_H,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition H [] =  {
    {.when = 'o',        .dest = ST_O,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition O [] =  {
    {.when = 'r',        .dest = ST_R,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition R [] =  {
    {.when = 'i',        .dest = ST_I,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition I [] =  {
    {.when = 'z',        .dest = ST_Z,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition Z [] =  {
    {.when = 'a',        .dest = ST_A_2,                        .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition A_2 [] =  {
    {.when = 't',        .dest = ST_T_2,                        .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition T_2 [] =  {
    {.when = 'i',        .dest = ST_I_2,                        .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition I_2 [] =  {
    {.when = 'o',        .dest = ST_O_2,                        .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition O_2 [] =  {
    {.when = 'n',        .dest = ST_N,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition N [] =  {
    {.when = ':',        .dest = ST_COLON,                      .act1 = next_state,},
    {.when = ANY,        .dest = ST_START,                      .act1 = next_state,},
};

static const struct parser_state_transition COLON [] =  {
    {.when = ' ',        .dest = ST_SPACE,                      .act1 = next_state,},
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition SPACE [] =  {
    {.when = 'B',        .dest = ST_B,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition B [] =  {
    {.when = 'a',        .dest = ST_A_3,                        .act1 = next_state,},
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition A_3 [] =  {
    {.when = 's',        .dest = ST_S,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition S [] =  {
    {.when = 'i',        .dest = ST_I_3,                        .act1 = next_state,},
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition I_3 [] =  {
    {.when = 'c',        .dest = ST_C,                          .act1 = next_state,},
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition C [] =  {
    {.when = ' ',        .dest = ST_AUTH,                       .act1 = next_state,},
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition AUTH [] =  {
    {.when = '\r',       .dest = ST_AUTH_R,                     .act1 = next_state,},
    {.when = ANY,        .dest = ST_AUTH,                       .act1 = copy_auth,},
};

static const struct parser_state_transition AUTH_R [] =  {
    {.when = '\n',       .dest = ST_FINISHED,                   .act1 = end_auth,},
    {.when = ANY,        .dest = ST_AUTH,                       .act1 = copy_auth_r,},
};

static const struct parser_state_transition FINISHED [] =  {
    {.when = ANY,        .dest = ST_FINISHED,                   .act1 = next_state,},
};

static const struct parser_state_transition INVALID_INPUT_FORMAT [] =  {
    {.when = ANY,        .dest = ST_INVALID_INPUT_FORMAT,       .act1 = invalid_input,},
};

static const struct parser_state_transition *states [] = {
    START,
    HEAD_R,
    HEAD,
    A,
    U,
    T,
    H,
    O,
    R,
    I,
    Z,
    A_2,
    T_2,
    I_2,
    O_2,
    N,
    COLON,
    SPACE,
    B,
    A_3,
    S,
    I_3,
    C,
    AUTH,
    AUTH_R,
    FINISHED,
    INVALID_INPUT_FORMAT,
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t states_n [] = {
    N(START),
    N(HEAD_R),
    N(HEAD),
    N(A),
    N(U),
    N(T),
    N(H),
    N(O),
    N(R),
    N(I),
    N(Z),
    N(A_2),
    N(T_2),
    N(I_2),
    N(O_2),
    N(N),
    N(COLON),
    N(SPACE),
    N(B),
    N(A_3),
    N(S),
    N(I_3),
    N(C),
    N(AUTH),
    N(AUTH_R),
    N(FINISHED),
    N(INVALID_INPUT_FORMAT),
};

static struct parser_definition definition = {
    .states_count = N(states),
    .states       = states,
    .states_n     = states_n,
    .start_state  = ST_START,
};

struct http_credentials * http_sniffer(char * s){
    struct parser *parser = parser_init(parser_no_classes(), &definition);
    struct http_credentials * ans = calloc(1, sizeof(*ans));
    size_t auth_current_length = 0;
    char finish = 0;
    char * auth = NULL;
    for(int i = 0; !finish && s[i]; i++){
        struct parser_event* ret = parser_feed(parser, s[i]);
        switch (ret->type){
            case COPY_ENCODED_AUTH:
                add_char(&auth, ret->data[0], &auth_current_length);
                if(auth == NULL){
                    return error(ans, REALLOC_ERROR);
                }
            break;
            case COPY_ENCODED_AUTH_R:
                add_char(&auth, '\r', &auth_current_length);
                if(auth == NULL){
                    return error(ans, REALLOC_ERROR);
                }
                add_char(&auth, ret->data[0], &auth_current_length);
                if(auth == NULL){
                    return error(ans, REALLOC_ERROR);
                }
            break;
            case END_ENCODED_AUTH:
                add_char(&auth, '\0', &auth_current_length);
                if(auth == NULL){
                    return error(ans, REALLOC_ERROR);
                }
                auth = realloc(auth, sizeof(*auth) * auth_current_length);
                if(auth == NULL){
                    return error(ans, REALLOC_ERROR);
                }
                finish = 1;
            break;
            case INVALID_INPUT_FORMAT_T:
                return error(ans, INVALID_INPUT_FORMAT_ERROR);
                finish = 1;
            break;
        }
    }
    parser_destroy(parser);
    auth_current_length--;
    if(auth == NULL || ((auth_current_length % 4) != 0)){
        return error(ans, INVALID_INPUT_FORMAT_ERROR);
    }
    error_t error_code = NO_ERROR;
    char * temp = decodeBase64(auth, &error_code);
    free(auth);
    if(error_code != NO_ERROR){
        return error(ans, error_code);
    }
    if(temp == NULL){
        return error(ans, INVALID_INPUT_FORMAT_ERROR);
    }
    error_code = parseUserPass(ans, temp);
    free(temp);
    if(error_code != NO_ERROR){
        return error(ans, error_code);
    }
    return ans;
}

void free_http_credentials(struct http_credentials * ans){
    if(ans != NULL){
        if(ans->user != NULL){
            free(ans->user);
        }
        if(ans->password){
            free(ans->password);
        }
        free(ans);
    }
}

char * decodeBase64(const char * encoded, error_t * error_code){ // Algoritmo copiado del pseudocodigo del paper subido por Juan en el foro
    char * aux = NULL;
    size_t encoded_length = strlen(encoded);
    size_t length = 0;
    for(int i = 0; i < encoded_length; i+=4){
        char a = base64table(encoded[i]);
        char b = base64table(encoded[i+1]);
        char c = base64table(encoded[i+2]);
        char d = base64table(encoded[i+3]);
        if(a<0 || b<0 || c<0 || d<0){
            *error_code = INVALID_INPUT_FORMAT_ERROR;
            if(aux != NULL){
                free(aux);
            }
            return NULL;
        }
        aux = resize_if_needed(aux, sizeof(*aux), length);
        if(aux == NULL){
            *error_code = REALLOC_ERROR;
            return NULL;
        }
        aux[length++] = a*4 + b/16;
        if(encoded[i+2] == '='){
            if(encoded[i+3] != '='){
                *error_code = INVALID_INPUT_FORMAT_ERROR;
                return NULL;
            }
            aux = resize_if_needed(aux, sizeof(*aux), length);
            if(aux == NULL){
                *error_code = REALLOC_ERROR;
                return NULL;
            }
            aux[length++] = '\0';
            return aux;
        } else {
            aux = resize_if_needed(aux, sizeof(*aux), length);
            if(aux == NULL){
                *error_code = REALLOC_ERROR;
                return NULL;
            }
            aux[length++] = (b*16)%256 + c/4;
        }
        if(encoded[i+3] == '='){
            aux = resize_if_needed(aux, sizeof(*aux), length);
            if(aux == NULL){
                *error_code = REALLOC_ERROR;
                return NULL;
            }
            aux[length++] = '\0';
            return aux;
        } else {
            aux = resize_if_needed(aux, sizeof(*aux), length);
            if(aux == NULL){
                *error_code = REALLOC_ERROR;
                return NULL;
            }
            aux[length++] = (c*64)%256 + d;
        }
    }
    aux = resize_if_needed(aux, sizeof(*aux), length);
    if(aux == NULL){
        *error_code = REALLOC_ERROR;
        return NULL;
    }
    aux[length++] = '\0';
    return aux;
}

char base64table(const char c){ // Mirar tabla del paper subido por Juan en el foro
    if(c >= 'A' && c <= 'Z'){
        return c - 'A';
    }
    else if(c >= 'a' && c <= 'z'){
        return c - 'a' + 26;
    }
    else if(c >= '0' && c<= '9'){
        return c - '0' + 52;
    }
    else if(c == '+'){
        return 62;
    }
    else if(c == '/'){
        return 63;
    }
    else if(c == '='){
        return 0;
    } else {
        return -1;
    }
}

error_t parseUserPass(struct http_credentials * ans, char * s){
    int isUser = 1;
    int length = 0;
    for(int i = 0; s[i]; i++){
        if(s[i] == ':'){
            ans->user = resize_if_needed(ans->user, sizeof(*(ans->user)), length);
            if(ans->user == NULL){
                return REALLOC_ERROR;
            }
            ans->user[length++] = '\0';
            ans->user = realloc(ans->user, sizeof(*(ans->user)) * length);
            if(ans->user == NULL){
                return REALLOC_ERROR;
            }
            isUser = 0;
            length = 0;
        } else {
            if(isUser){
                ans->user = resize_if_needed(ans->user, sizeof(*(ans->user)), length);
                if(ans->user == NULL){
                    return REALLOC_ERROR;
                }
                ans->user[length++] = s[i];
            } else {
                ans->password = resize_if_needed(ans->password, sizeof(*(ans->password)), length);
                if(ans->password == NULL){
                    return REALLOC_ERROR;
                }
                ans->password[length++] = s[i];
            }
        }
    }
    if(isUser){
        return NO_COLON;
    }
    ans->password = resize_if_needed(ans->password, sizeof(*(ans->password)), length);
    if(ans->password == NULL){
        return REALLOC_ERROR;
    }
    ans->password[length++] = '\0';
    ans->password = realloc(ans->password, sizeof(*(ans->password)) * length);
    if(ans->password == NULL){
        return REALLOC_ERROR;
    }
    return NO_ERROR;
}

struct http_credentials * error(struct http_credentials * ans, error_t error_type){
    free_http_credentials(ans);
    ans = malloc(sizeof(*ans));
    ans->user = NULL;
    ans->password = NULL;
    ans->error = error_type;
    return ans;
}

char * add_char(char ** ans, char c, size_t * current_length){
    *ans = resize_if_needed(*ans, sizeof(**ans), *current_length);
    if(*ans == NULL){
        return NULL;
    }
    (*ans)[(*current_length)++] = c;
    return *ans;
}

void * resize_if_needed(void * ptr, size_t ptr_size, size_t current_length){
    if(current_length % CHUNK_SIZE == 0){
        return realloc(ptr, ptr_size * (current_length + CHUNK_SIZE));
    }
    return ptr;
}

/** 
 * To test with afl-fuzz uncomment the main function below and run:
 *     1. Create a directory for example inputs (i.e. parser_test_case)
 *     2. Insert at least 1 (one) example file in the created directory
 *     3. Run in the terminal "afl-clang http_sniffer.c parser.c -o http_sniffer -pedantic -std=c99" (or afl-gcc)
 *     4. Run in the terminal "afl-fuzz -i parser_test_case -o afl-output -- ./http_sniffer @@"
 */

/*
int main(int argc, char ** argv){
    FILE * fp;
    char c;
    int size = 1000;
    char *buffer = calloc(1,size * sizeof(*buffer));
    if(buffer == NULL){
        return 1;
    }
    if(argc != 2){
        return 1;
    }
    fp = fopen(argv[1], "r");
    int i = 0;
    while((c=fgetc(fp)) != EOF){
        if(i == size){
            size += size;
            buffer = realloc(buffer, size * sizeof(*buffer));
            if(buffer == NULL){
                return 1;
            }
        }
        buffer[i] = c;
        i++;
    }
    buffer = realloc(buffer, (i+1) * sizeof(*buffer));
    buffer[i] = '\0';
    fclose(fp);

    struct http_credentials * ans = http_sniffer(buffer);
    if(ans->error != 0){
        printf("error\n");
    } else {
        printf("%s\n", ans->user);
        printf("%s\n", ans->password);
    }
    free_http_credentials(ans);
    free(buffer);
    return 0;
}
*/