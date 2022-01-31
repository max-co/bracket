#ifndef HELP_H
#define HELP_H

#include "version.h"

#define PROG_NAME "bracket"
#define PROG_VERSION PROG_NAME " " VERSION

// clang-format off
constexpr const char *usage = PROG_VERSION "\n"
    "usage: " PROG_NAME " [options] [input file]\n"
R"(
  if no input file is specified the input is read from the standard input

options:

  -g  : Possibly output a Graphviz representation of a found successful run
        to a file (default file: run.gv)

  -h  : Print this help message and exit

  -i <file> : Set <file> as the input file

  -l  : Output a logic programming representation of the automaton and possibly
        of a found successful run to a file (default file: automaton.lp)

  -o <file> : Set <file> as the output file for the -g option and also
              implicitly activate option -g

  -t <num>  : Set <num> (>= 1) as the maximun number of concurrent threads
              (default: 1)

  -w  : Overwite the content of output files that already exist

  -L <file> : Set <file> as the output file for the -l option and also
              implicitly activate option -l

  -V : Print version information and exit

Get help/Report bugs/Provide suggestions at: https://github.com/max-co/bracket
)";

#endif
