
/*
 * shell.y: parser for shell
 *
 */

%code requires 
{
#include <string>
#include <cstring>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT NEWLINE PIPE AMPERSAND LESS GREATAND GREATGREAT GREATGREATAND TWOGREAT

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"
#include <dirent.h>
#include <regex.h>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#define MAXLENGTH 4056

void yyerror(const char * s);
int yylex();

int maxEntries = 20;
int nEntries = 0;
char ** entries;

int cmpfunc (const void *a, const void *b) {
  const char *_a = *(const char **) a;
  const char *_b = *(const char **) b;
  return strcmp(_a, _b);
}

void expandWildCards(char * prefix, char * arg) {
  char * temp = arg;
  char * save = (char *) malloc (strlen(arg) + 10);
  char * dir = save;

  // Parse the argument to find the directory path
  if(temp[0] == '/') {
    *(save++) = *(temp++);
  }

  while (*temp != '/' && *temp) {
    *(save++) = *(temp++);
  }

  *save = '\0';

  // check if the directory contains any wildcards character
  if (strchr(dir, '*') || strchr(dir, '?')) {
    // create a regular expression from the wildcard pattern, and use it to match files
    if (!prefix && arg[0] == '/') {
      prefix = strdup("/");
      dir++;
    }

    // create regex from wildcard pattern, given from the textbook
    char * reg = (char *) malloc (2 * strlen(arg) + 10);
    char * a = dir;
    char * r = reg;

    *r = '^';
    r++;
    while (*a) {
      if (*a == '*') { *r='.'; r++; *r='*'; r++; }
      else if (*a == '?') { *r='.'; r++; }
      else if (*a == '.') { *r='\\'; r++; *r='.'; r++; }
      else { *r=*a; r++; }
      a++;
    }
    *r = '$';
    r++;
    *r = '\0';

    // compile the regex
    regex_t re;
    int expbuf = regcomp(&re, reg, REG_EXTENDED | REG_NOSUB);

    // determine the directory to open
    char * toOpen;
    if (prefix) {
      toOpen = strdup(prefix);
    } else {
      toOpen = strdup(".");
    }
    DIR * dir = opendir(toOpen);
    if (dir == NULL) {
      perror("opendir");
      return;
    }

    // Loop through all files and directories in the specified directory
    struct dirent * ent;
    regmatch_t match;

    while ((ent = readdir(dir)) != NULL) {
      // Check if the filename matches the regular expression
      if (!regexec(&re, ent->d_name, 1, &match, 0)) {
        // If the wildcard pattern is not fully expanded yet
        if (*temp) {
          // Check if the current entry is a directory
          if (ent->d_type == DT_DIR) {
            // Recursively expand the wildcard with the new prefix and the remaining part of the argument
            char * nPrefix = (char *) malloc (150);
            if (!strcmp(toOpen, ".")) { 
              nPrefix = strdup(ent->d_name);
            } else if (!strcmp(toOpen, "/")) {
              sprintf(nPrefix, "%s%s", toOpen, ent->d_name);
            } else {
              sprintf(nPrefix, "%s/%s", toOpen, ent->d_name);
            }
            if (*temp == '/') {
              expandWildCards(nPrefix, ++temp);
            } else {
              expandWildCards(nPrefix, temp);
            }
          }
        } else {
          // The wildcard pattern is fully expanded
          // Add the matched entry to the entries array
          if (nEntries == maxEntries) {
            maxEntries *= 2;
            entries = (char **) realloc (entries, maxEntries * sizeof(char *));
          }
          char * argument = (char *) malloc (100);
          argument[0] = '\0';
          if (prefix) {
            sprintf(argument, "%s/%s", prefix, ent->d_name);
          }
          if (ent->d_name[0] == '.') {
            if (arg[0] == '.') {
              if (argument[0] != '\0') {
                entries[nEntries++] = strdup(argument);
              } else {
                entries[nEntries++] = strdup(ent->d_name);
              }
            }
          } else {
            if (argument[0] != '\0') {
              entries[nEntries++] = strdup(argument);
            } else {
              entries[nEntries++] = strdup(ent->d_name);
            }
          }
          free(argument);
        }
      }
    }
    closedir(dir);
    regfree(&re);
    free(toOpen);

    } else {
    // If there's no more wildcards character in the remaining part of the argument

    // Create a new prefix for the fully expanded wildcard entry.
    char * newPrefix = (char *) malloc (100);
    if(prefix) {
      sprintf(newPrefix, "%s/%s", prefix, dir);
    } else {
      newPrefix = strdup(dir);
    }
    // If there is more to expand in the argument, call expandWildCards recursively
    if(*temp) {
      expandWildCards(newPrefix, ++temp);
    }
    free(newPrefix);
  }



}
void expandWildcardsIfNecessary(std::string * arg) {
  // convert to C string
  char * arg_new = (char *) malloc(arg->length()+1);
  strcpy(arg_new,arg->c_str());

  // variables to store wildcard expansion result
  maxEntries = 20;
  nEntries = 0;
  entries = (char **) malloc (maxEntries * sizeof(char *));

  // check if contains wildcards
  if (strchr(arg_new, '*') || strchr(arg_new, '?')) {
    // expand wildcards
    expandWildCards(NULL, arg_new);
    // If no matches were found, insert the original argument without expansion
    if(nEntries == 0) {
      Command::_currentSimpleCommand->insertArgument(arg);
      return;
    }
    // Sort the matched entries in alphabetical order
    qsort(entries, nEntries, sizeof(char *), cmpfunc);
    // Insert the sorted entries into the command as arguments
    for (int i = 0; i < nEntries; i++) {
      std::string * str = new std::string(entries[i]);
      Command::_currentSimpleCommand->insertArgument(str);
    }
  } else {
    // If the argument does not contain wildcards, insert it into the command as is
    Command::_currentSimpleCommand->insertArgument(arg);
  }

  free(entries);
  entries = NULL;
  return;
}


%}

%%

goal:
  command_list
  ;

command_list:
  command_line
  | command_list command_line
  ;

command_line:
  pipe_list io_modifier_list
  background_opt NEWLINE{
    Shell::_currentCommand.execute();
  }
  | NEWLINE
  | error NEWLINE{yyerrok;}
  ;

pipe_list:
  command_and_args
  | pipe_list PIPE command_and_args
  ;

command_and_args:
  command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

command_word:
  WORD {
    if (strcmp($1->c_str(), "exit") == 0) {
      printf("YOU'RE DONE ALREADY?!\n");
      exit(1);
    }
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

argument_list:
  argument_list argument
  | /* can be empty */
  ;

argument:
  WORD {
    expandWildcardsIfNecessary($1);
  }
  ;

io_modifier_list:
  io_modifier_list iomodifier_opt
  | /* empty */
  ;

iomodifier_opt:
  GREAT WORD {
   if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
   }
   Shell::_currentCommand._outFile = $2;
  }
  | GREATAND WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
  }
  | GREATGREAT WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }
    Shell::_currentCommand._append = 1;
    Shell::_currentCommand._outFile = $2;
  }
  | GREATGREATAND WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }

    Shell::_currentCommand._append = 1;
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = $2;
  }
  | LESS WORD {
    if (Shell::_currentCommand._inFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }

    Shell::_currentCommand._inFile = $2;
  }
  | TWOGREAT WORD {
    if (Shell::_currentCommand._errFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }

    Shell::_currentCommand._append = 0;
    Shell::_currentCommand._errFile = $2;
  }
  ;

background_opt:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  |
  ;

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
