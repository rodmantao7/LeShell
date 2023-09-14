
/*
 * shell.l: lexical analyzer for shell
 */

%{

#include <cstring>
#include "y.tab.hh"
#include <wait.h>
#include <unistd.h>

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}
extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

     if (!isatty(0)) {
  // stdin is not a tty. Call real getc
    return getc(f);
  }

  // stdin is a tty. Call our read_line.
  if (p==NULL || *p == 0) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

%}

%option noyywrap

%s src

%%

source             BEGIN(src);

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

">>" {
  return GREATGREAT;
}

"|" {
  return PIPE;
}

"&" {
  return AMPERSAND;
}

"2>" {
  return TWOGREAT;
}

">&" {
  return GREATAND;
}

">>&" {
 return GREATGREATAND;
}

"<" {
  return LESS;
}


~[^ \n\t]* {
  // 3.2: tilde stuff

  std::string str (yytext); // get input string
  std::string homeEnv = getenv("HOME"); // get the home environment

  // if there's only "~"
  if (str.size() == 1) { // case 0: only ~
    yylval.cpp_string = new std::string(homeEnv);
  } else {
    if (str[1] == '/') { // case 1: (~/)
      str = homeEnv + str.substr(1, yyleng-1); 
    } else { // case 2: ~..../
      str = "/homes/" + str.substr(1, yyleng - 1);
    }
    yylval.cpp_string = new std::string(str);
  }
  return WORD;
}

\$\{[^\n]*\}* {
  // 3.1: Environment variable extension
  std::string str (yytext); // get input string
  std::string res = ""; // string to store the result
  size_t begin = str.find("$"); // find the first encounter of '$'
  while (begin != -1) {
   std::string env; // environment variable to store the content inside {}
   res += str.substr(0, begin); // add any letters in between if there's any
   if (str[begin + 1] == '{') {
    size_t end = str.find('}'); // find the first encounter of '}'
    if (end != -1) {
      env = str.substr(begin + 2, end - begin - 2); // get the variable from {}, end - begin = length of what's inside ${}
      if (getenv(env.c_str()) != NULL) {
        res += getenv(env.c_str());
      } else {
        res += env;
      }
      str = str.substr(end + 1, yyleng - 1); // shrink the string to whatever after the first '}'
    }
   }
   begin = str.find('$'); // reset begin, find the next encounter of '$'
  }

  res += str; // add null terminator

  yylval.cpp_string = new std::string(res);

  return WORD;
}


["][^\"]*["] {
  // Quotes stuff

  std::string str (yytext); // get input string
  yylval.cpp_string = new std::string(str.substr(1, yyleng - 2)); // get the rid of the quotes
  return WORD;
}

\`[^\n\`]*\`|\$\([^\n]*\) {

  // Begin subshell stuff
  // Biggest thanks to Maddie, I couldn't figure subshell on my own TT

  // get the input string
  std::string str (yytext);

  // get rid of $( and )
  str = str.substr(2, yyleng-3);

  // add exit so you can actually exit out of subshell dumbass
  str.append("\nexit\n");

  // get tempIn, tempOut
  int tmpIn = dup(0);
  int tmpOut = dup(1);

  // create and pipe pipe in
  int pin[2];
  pipe(pin);

  // create and pipe pipe out
  int pout[2];
  pipe(pout);

  // redirect
  dup2(pin[0], 0);
  close(pin[0]);
  dup2(pout[1], 1);
  close(pout[1]);

  // write the str to pin[1]
  write(pin[1], str.c_str(), str.size());
  close(pin[1]);

  // fork() time
  int pid = fork();

  // if you're in the child process
  if (pid == 0) {
    // execute commands from subshell
    char **commands = new char*[2];
    commands[0] = (char*) "/proc/self/exe";
    commands[1] = NULL;
    execvp(commands[0], commands);
    perror("execvp");
    exit(1);
  } else if (pid < 0) { // if it's an error
    perror("fork");
    exit(1);
  }

  waitpid(pid, NULL, 0);
  dup2(tmpIn, 0);
  dup2(tmpOut, 1);
  close(tmpIn);
  close(tmpOut);

  // get a buffer to read from pipe out
  char buffer[4096];
  int bytes = read(pout[0], buffer, 4096);
  close(pout[0]);

  // print it out in reverse since it's reading in reversse
  for (int i = bytes - 24; i >= 0; --i) {
    if (buffer[i] == '\n') {
      buffer[i] = '\t';
    }
    myunputc(buffer[i]);
  }

}

<src>[^ \t\n]+ {
  // source stuff
  // Huge thanks to Ethan and Shreya

  //FILE *tmpIn = yyin;

  // open file
  FILE *in = fopen(yytext, "r");

  // error handling
  if (!in) {
    perror("fopen");
    exit(1);
  }

  // Tbh, I dont even know what the hell this do, but TAs said put it down lmao
  yypush_buffer_state(yy_create_buffer(in, YY_BUF_SIZE)); // create the buffer from the in file, then push the new state to an internal stack
  BEGIN(INITIAL);
  yyparse(); // execute
  yypop_buffer_state(); // remove the current buffer state from the internal stack
  fclose(in); // close the file pointer 

}

[^ \t\n]*\\[^ \n\t]  {
  // escape stuff
  // Huge thanks to Maddie again for bailing me out

  // get the input
  char * str = strdup(yytext);

  // str to store the ouput
  char * out = (char*) malloc (100);

  int i = 0;
  // while loop to skip over the '\\'(escape)
  while (*str){
    if (*str == '\\'){
      if (*(str + 1) == '\\'){
        str = str + 2;
        out[i++] = '\\';
      } else {
        out[i++] = *(++str);
      }
    } else {
      out[i++] = *str;
    }
    str++;
  }
  out[i] = '\0';
  yylval.cpp_string = new std::string(out);
  return WORD;
}

[^ \|\>\t\n][^ \|\>\t\n]* {
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}

