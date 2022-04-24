#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int token;            // current token
char *src, *old_src;  // pointer to source code
int poolsize;         // the default size of the data/text/stack
int line;             // line number

/**
 * @brief for lexical analysis, get the next token, it will automatically ignore
 * whitespace characters
 */
void next() {
    token = *src++;
    return;
}

/**
 * @brief for parsing an expression
 * @param level
 */
void expression(int level) {
    next();  // get next token
    while (token > 0) {
        printf("token is %c\n", token);
        next();
    }
}

/**
 * @brief the entry point of the virtual machine,
 * used to interpret the object code
 */
int eval() {
    // TODO: implement this function
    return 0;
}

/**
 * @brief the main function
 */
int main(int argc, char* argv[]) {
    int i, fd;
    argc--;
    argv++;

    poolsize = 256 * 1024;

    // open the source file
    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    // init the segment and stack space
    if (!(src = old_src = malloc(poolsize))) {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }

    // read the source file
    if ((i = read(fd, src, poolsize - 1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0;  // add EOF character
    close(fd);

    program();
    return 0;
}