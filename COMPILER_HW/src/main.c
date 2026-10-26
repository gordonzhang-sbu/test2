/* main.c — source loader, fd lifecycle, CLI driver.
 *
 * Student-editable module.
 */
#include "svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>



#define STDIN_CAP (1 << 20)

/* Read the whole source into a freshly malloc'd, NUL-terminated buffer. */
static char *load_source(const char *path, size_t *out_len) {
    int fd;

    //If path is -, we read from standard input
    if (strcmp(path, "-") == 0) {
        fd = STDIN_FILENO;
    } else {
        //If not, we open the file path refers to
        fd = open(path, O_RDONLY);
        //Clean up if fail
        if (fd < 0){
            perror(path);
            return NULL;
        }
    }

    //Stores the current capacity of the buffer, and current length of the buffer
    size_t current_capacity = STDIN_CAP;
    size_t current_length = 0;
    //Plus one for terminator
    char *buffer = malloc(current_capacity + 1);
    //Close and clean up if fail
    if (buffer == NULL){
        if (strcmp(path, "-") != 0){
            close(fd);
        }
        return NULL;
    }

    //keep reading
    while(1){
        //If the current length hits the current capacity, we have to reallocate more memory
        if (current_length == current_capacity){
            char *temp = realloc(buffer, current_capacity * 2 + 1);
            if (temp == NULL){
                free(buffer);
                if (strcmp(path, "-") != 0){
                    close(fd);
                }
                return NULL;
            }

            //reasigned the pointer and update the current capacity
            buffer = temp;
            current_capacity *= 2;





        }

        //Check for how many byte read from read()
        int byte_read = read(fd, buffer + current_length, current_capacity - current_length);
        if (byte_read > 0){
            //If more than 0, update current length
            current_length += byte_read;
        } else if (byte_read == 0){
            //If equals 0, means EOF, break the infinite loop
            break;
        } else{
            //Else means read fail, clean up
            free(buffer);
            if (strcmp(path, "-") != 0){
                close(fd);  
            }
            return NULL;
        }


    }

    //Add the null terminator
    buffer[current_length] = '\0';
    //Assign the result
    *out_len = current_length;
    if (strcmp(path, "-") != 0){
        close(fd);  
    }    
    return buffer;
}

static char *default_output(const char *in) {
    if (strcmp(in, "-") == 0) return strdup("out.bvm");
    const char *dot = strrchr(in, '.');
    size_t base = dot ? (size_t)(dot - in) : strlen(in);
    char *out = malloc(base + 5);
    memcpy(out, in, base);
    strcpy(out + base, ".bvm");
    return out;
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s <input.sl|-> [-o out.bvm]\n", prog);
}

int main(int argc, char **argv) {
    const char *input = NULL;
    char       *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output = strdup(argv[++i]);
        } else if (input == NULL) {
            input = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (!input) { usage(argv[0]); return 2; }
    if (!output) output = default_output(input);

    size_t len = 0;
    char *src = load_source(input, &len);
    if (!src) { fprintf(stderr, "svc: could not read '%s'\n", input); free(output); return 1; }

    AstNode *prog = parse_program(src, len, input);
    if (!prog) { fprintf(stderr, "svc: compilation failed\n"); free(src); free(output); return 1; }

    Module *m = codegen(prog);
    if (!m) { fprintf(stderr, "svc: codegen failed\n"); free(src); free(output); return 1; }

    if (emit_module(m, output) != 0) {
        fprintf(stderr, "svc: could not write '%s'\n", output);
        module_free(m); free(src); free(output);
        return 1;
    }

    module_free(m);
    free(src);
    free(output);
    return 0;
}
