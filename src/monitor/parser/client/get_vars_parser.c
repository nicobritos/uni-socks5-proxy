#include <stdio.h>
#include <stdlib.h>

#include "../../../utils/parser.h"
#include "get_vars_parser.h"

#define CHUNK_SIZE 10
#define LOG_SEVERITY_ERROR 4
#define LOG_SEVERITY_WARNING 3
#define LOG_SEVERITY_INFO 2
#define LOG_SEVERITY_DEBUG 1

/* Funciones auxiliares */
static struct vars *error(struct vars *ans, parser_error_t error_type);

static enum log_severity get_log_enum(uint8_t n);

// definición de maquina

enum states {
    ST_VCODE,
    ST_SYSTEM_LMODE_VALUE,
    ST_SOCKS_LMODE_VALUE,
    ST_LMODE_END,
    ST_END,
    ST_INVALID_INPUT_FORMAT,
};

enum event_type {
    SUCCESS,
    COPY_SYSTEM_LMODE,
    COPY_SOCKS_LMODE,
    END_T,
    INVALID_INPUT_FORMAT_T,
};

static void
next_state(struct parser_event *ret, const uint8_t c) {
    ret->type = SUCCESS;
    ret->n = 1;
    ret->data[0] = c;
}

static void
copy_system_lmode(struct parser_event *ret, const uint8_t c) {
    ret->type = COPY_SYSTEM_LMODE;
    ret->n = 1;
    ret->data[0] = c;
}

static void
copy_socks_lmode(struct parser_event *ret, const uint8_t c) {
    ret->type = COPY_SOCKS_LMODE;
    ret->n = 1;
    ret->data[0] = c;
}

static void
end(struct parser_event *ret, const uint8_t c) {
    ret->type = END_T;
    ret->n = 1;
    ret->data[0] = c;
}

static void
invalid_input(struct parser_event *ret, const uint8_t c) {
    ret->type = INVALID_INPUT_FORMAT_T;
    ret->n = 1;
    ret->data[0] = c;
}

static const struct parser_state_transition VCODE[] = {
        {.when = '\0', .dest = ST_END, .act1 = end,},
        {.when = '\2', .dest = ST_SYSTEM_LMODE_VALUE, .act1 = next_state,},
        {.when = '\3', .dest = ST_SOCKS_LMODE_VALUE, .act1 = next_state,},
        {.when = ANY, .dest = ST_INVALID_INPUT_FORMAT, .act1 = invalid_input,},
};

static const struct parser_state_transition SYSTEM_LMODE_VALUE[] = {
        {.when = '\0', .dest = ST_VCODE, .act1 = next_state,},
        {.when = '1', .dest = ST_LMODE_END, .act1 = copy_system_lmode,},
        {.when = '2', .dest = ST_LMODE_END, .act1 = copy_system_lmode,},
        {.when = '3', .dest = ST_LMODE_END, .act1 = copy_system_lmode,},
        {.when = '4', .dest = ST_LMODE_END, .act1 = copy_system_lmode,},
        {.when = ANY, .dest = ST_INVALID_INPUT_FORMAT, .act1 = invalid_input,},
};

static const struct parser_state_transition SOCKS_LMODE_VALUE[] = {
        {.when = '\0', .dest = ST_VCODE, .act1 = next_state,},
        {.when = '1', .dest = ST_LMODE_END, .act1 = copy_socks_lmode,},
        {.when = '2', .dest = ST_LMODE_END, .act1 = copy_socks_lmode,},
        {.when = '3', .dest = ST_LMODE_END, .act1 = copy_socks_lmode,},
        {.when = '4', .dest = ST_LMODE_END, .act1 = copy_socks_lmode,},
        {.when = ANY, .dest = ST_INVALID_INPUT_FORMAT, .act1 = invalid_input,},
};

static const struct parser_state_transition LMODE_END[] = {
    {.when = '\0', .dest = ST_VCODE, .act1 = next_state,},
    {.when = ANY, .dest = ST_INVALID_INPUT_FORMAT, .act1 = invalid_input,},
};

static const struct parser_state_transition END[] = {
    {.when = ANY, .dest = ST_INVALID_INPUT_FORMAT, .act1 = invalid_input,},
};

static const struct parser_state_transition INVALID_INPUT_FORMAT[] = {
    {.when = ANY, .dest = ST_INVALID_INPUT_FORMAT, .act1 = invalid_input,},
};

static const struct parser_state_transition *states[] = {
        VCODE,
        SYSTEM_LMODE_VALUE,
        SOCKS_LMODE_VALUE,
        LMODE_END,
        END,
        INVALID_INPUT_FORMAT,
};

#define N(x) (sizeof(x)/sizeof((x)[0]))

static const size_t states_n[] = {
    N(VCODE),
    N(SYSTEM_LMODE_VALUE),
    N(SOCKS_LMODE_VALUE),
    N(LMODE_END),
    N(END),
    N(INVALID_INPUT_FORMAT),
};

static struct parser_definition definition = {
        .states_count = N(states),
        .states       = states,
        .states_n     = states_n,
        .start_state  = ST_VCODE,
};

struct vars * get_vars_parser_init(){
    struct vars * ans = calloc(1, sizeof(*ans));
    ans->parser = parser_init(parser_no_classes(), &definition);
    return ans;
}

struct vars * get_vars_parser_consume(uint8_t *s, size_t length, struct vars * ans) {
    
    for (size_t i = 0; i<length; i++) {
        const struct parser_event* ret = parser_feed(ans->parser, s[i]);
        switch (ret->type) {
            case COPY_SYSTEM_LMODE:
                ans->system_lmode = get_log_enum(s[i] - '0');
                break;
            case COPY_SOCKS_LMODE:
                ans->socks_lmode = get_log_enum(s[i] - '0');
                break;
            case END_T:
                ans->finished = 1;
            break;
            case INVALID_INPUT_FORMAT_T:
                return error(ans, INVALID_INPUT_FORMAT_ERROR);
        }
    }
    return ans;
}

void free_vars(struct vars *vars) {
    if (vars != NULL) {
        if(vars->parser != NULL){
            parser_destroy(vars->parser);
            vars->parser = NULL;
        }
        free(vars);
    }
}

static struct vars *error(struct vars *ans, parser_error_t error_type) {
    free_vars(ans);
    ans = calloc(1, sizeof(*ans));
    ans->error = error_type;
    return ans;
}

static enum log_severity get_log_enum(uint8_t n) {
    switch (n) {
        case LOG_SEVERITY_INFO: return log_severity_info;
        case LOG_SEVERITY_DEBUG: return log_severity_debug;
        case LOG_SEVERITY_ERROR: return log_severity_error;
        case LOG_SEVERITY_WARNING: return log_severity_warning;
        default: return 0;
    }
}

/** 
 * To test with afl-fuzz uncomment the main function below and run:
 *     1. Create a directory for example inputs (i.e. parser_test_case)
 *     2. Insert at least 1 (one) example file in the created directory
 *     3. Run in the terminal "afl-clang auth_server_response_parser.c parser.c -o auth_server_response_parser -pedantic -std=c99" (or afl-gcc)
 *     4. Run in the terminal "afl-fuzz -i parser_test_case -o afl-output -- ./auth_server_response_parser @@"
 */

/*
int main(int argc, char ** argv){
    FILE * fp;
    int16_t c;
    int size = 1000;
    uint8_t *buffer = calloc(1,size * sizeof(*buffer));
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
    buffer = realloc(buffer, i * sizeof(*buffer));
    fclose(fp);

    struct vars * ans = get_vars_parser(buffer, i);
    free(buffer);
    if(ans->error != NO_ERROR){
        printf("error\n");
    } else {
        printf("IO Timeout = %zu\n", ans->io_timeout);
        printf("Logger Severity = %u\n", ans->lmode);
    }
    free_vars(ans);
    return 0;
}
*/
