#include "cpic.h"

#include "fail.h"
#include "arch_emr.h"
#include "utils.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


const char* progname;
int verbosity = 0;


const char* const msg_usage =
    "Usage:  %s [OPTIONS] FILE\n"
    "\n"
    "Arguments:\n"
    "  FILE    the assembly file to read\n"
    "\n"
    "Available options:\n"
    "  -h\n"
    "      show this usage text\n"
    "  -v\n"
    "      increase verbosity (can be passed up to 2 times)\n"
    ;

void exit_with_usage()
{
    fprintf(stderr, msg_usage, progname);
    exit(E_INFO);
}


int process_args(int argc, char** argv)
{
    while (true) {
        int c = getopt(argc, argv, "hv");
        if (c == -1) {
            break;
        } else if (c == 'h') {
            exit_with_usage();
        } else if (c == 'v') {
            ++verbosity;
        }
    }

    return optind;
}


int main(int argc, char** argv)
{
    // Do some setup.

    progname = argv[0];

    if (argc < 2)
        exit_with_usage();

    int source_idx = process_args(argc, argv);

    // Get ready to read the source file.

    if (source_idx >= argc)
        fatal(E_COMMON, "No file specified");

    int src = open(argv[source_idx], O_RDONLY);
    if (src < 0)
        fatal_e(E_COMMON, "Can't open file \"%s\"", argv[source_idx]);

    // Assemble the source file.

    assemble_emr(src);

    // Clean up and exit.

    close(src); // (Ignore errors.)
    return 0;
}
