#define _POSIX_C_SOURCE 2

#include "bufman.h"
#include "fail.h"
#include "utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


const char* progname;


//
// constant strings
//


const char* const msg_usage =
    "Usage:  %s FILE1\n"
    "\n"
    "Arguments:\n"
    "  FILE1    the assembly file to read\n"
    "\n"
    "Available options:\n"
    "  -l\n"
    "      list supported processors\n"
    "  -h\n"
    "      show this usage text\n"
    "  -p PROCESSOR\n"
    "      assemble for the specified processor\n"
    ;

const char* const msg_processor_not_supported =
    "Processor \"%s\" is not supported at the moment. Pass the -l option to\n"
    "see the list of supported processors.\n";


//
// processors
//


enum processor {
    P16F1454,
    NONE,
};

const char* const processors[] = {
    [P16F1454] = "16F1454",
};

//
// utility-ish functions
//


void exit_with_usage()
{
    fprintf(stderr, msg_usage, progname);
    exit(3);
}


void exit_with_processors()
{
    fprintf(stderr, "Processors currently supported:\n");
    for (unsigned int i = 0; i < lengthof(processors); ++i)
        fprintf(stderr, "  %s\n", processors[i]);
    exit(3);
}


struct args {
    enum processor processor;
} process_args(int argc, char** argv)
{
    struct args args = {
        .processor = -1,
    };

    while (true) {
        int c = getopt(argc, argv, "lhp:");
        if (c == -1) {
            break;
        } else if (c == 'l') {
            exit_with_processors();
        } else if (c == 'h') {
            exit_with_usage();
        } else if (c == 'p') {
            unsigned int i;
            for (i = 0; i < lengthof(processors); ++i)
                if (0 == strcmp(optarg, processors[i]))
                    break;
            if (i == lengthof(processors)) {
                fprintf(stderr, msg_processor_not_supported, optarg);
                exit(2);
            }

            args.processor = i;
        };
    }

    return args;
}


//
// everything else
//


int main(int argc, char** argv)
{
    progname = argv[0];

    if (argc < 2)
        exit_with_usage();

    struct args args = process_args(argc, argv);
    (void)args;

    return 0;
}
