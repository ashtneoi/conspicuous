#define _POSIX_C_SOURCE 2

#include "cpic.h"

#include "bufman.h"
#include "fail.h"
#include "P16F1454.h"
#include "utils.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define E_COMMON 1
#define E_ARG 2
#define E_INFO 3
#define E_RARE 4


const char* progname;
int verbosity = 0;


//
// constant strings
//


const char* const msg_usage =
    "Usage:  %s FILE\n"
    "\n"
    "Arguments:\n"
    "  FILE    the assembly file to read\n"
    "\n"
    "Available options:\n"
    "  -l\n"
    "      list supported processors\n"
    "  -h\n"
    "      show this usage text\n"
    "  -p PROCESSOR\n"
    "      assemble for the specified processor\n"
    "  -v\n"
    "      increase verbosity (can be passed up to 2 times)\n"
    ;

const char* const msg_processor_not_supported =
    "Processor \"%s\" is not supported at the moment. Pass the -l option to\n"
    "see the list of supported processors.\n";

const char* const msg_processor_required =
    "The -p option is required.\n";


//
// processors
//


enum processor {
    P16F1454,
    P_NONE,
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
    exit(E_INFO);
}


void exit_with_processors()
{
    fprintf(stderr, "Processors currently supported:\n");
    for (unsigned int i = 0; i < lengthof(processors); ++i)
        fprintf(stderr, "  %s\n", processors[i]);
    exit(E_INFO);
}


struct args {
    enum processor processor;
    int source_idx;
} process_args(int argc, char** argv)
{
    struct args args = {
        .processor = P_NONE,
    };

    while (true) {
        int c = getopt(argc, argv, "lhp:v");
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
                exit(E_ARG);
            }

            args.processor = i;
        } else if (c == 'v') {
            ++verbosity;
        }
    }

    if (args.processor == P_NONE) {
        fputs(msg_processor_required, stderr);
        exit(E_ARG);
    }

    args.source_idx = optind;

    return args;
}


//
// everything else
//


int main(int argc, char** argv)
{
    // Do some setup.

    progname = argv[0];

    if (argc < 2)
        exit_with_usage();

    struct args args = process_args(argc, argv);

    // Get ready to read the source file.

    if (argv[args.source_idx] == NULL)
        fatal(E_COMMON, "No file specified");

    v2("Opening source file");
    int src = open(argv[args.source_idx], O_RDONLY);
    if (src < 0)
        fatal_e(E_COMMON, "Can't open file \"%s\"", argv[args.source_idx]);

    // Assemble the source file.

    switch (args.processor) {
    case P16F1454:
        if (!assemble_16F1454(src))
            return E_COMMON;
        break;
    case P_NONE:
        fatal(E_RARE, "This should never happen!");
        break;
    }

    close(src); // (Ignore errors.)
    return 0;
}
