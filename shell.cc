#include <cstdio>
#include <signal.h>
#include "shell.hh"
#include <unistd.h>
#include <wait.h>

int yyparse(void);
void yyrestart(FILE *file);


void Shell::prompt() {
  if (isatty(0)) {
    printf("\033[1;35m🌟 LeShell> \033[0m");
    fflush(stdout);
 }
}

extern "C" void ctrlC(int sig) {
  printf("\n");
  Shell::prompt();
}

extern "C" void zombie(int sig) {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}


int main(int argc, char *argv[]) {
  //  set environment for $
  std::string pid = std::to_string(getpid());
  setenv("$", pid.c_str(), 1);

  // set environment for SHELL
  char resolvedPath[1024];
  realpath(argv[0], resolvedPath);
  setenv("SHELL", resolvedPath, 1);

  // 2.1: CTRL-C handling babyyyyy
  struct sigaction ctrlSig;
  ctrlSig.sa_handler = ctrlC;
  sigemptyset(&ctrlSig.sa_mask);
  ctrlSig.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &ctrlSig, NULL)) {
    perror("sigaction");
    exit(2);
  }

  // 2.2: Zombie Signal handling (Eww)
  // Only check for Zombie signal when background flag is true (handout)
  struct sigaction zombieSig;
  zombieSig.sa_handler = zombie;
  sigemptyset(&zombieSig.sa_mask);
  zombieSig.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &zombieSig, NULL)) {
    perror("sigaction");
    exit(2);
  }

  // .shellrc handling? (need testing)
  FILE *fp = fopen(".shellrc", "r");

  if (fp != NULL) { // if file exist
    yyrestart(fp); // sets the file as the buffer input
    yyparse(); // execute all the commands
    yyrestart(stdin); // restores buffer to take input from terminal again
    fclose(fp); // close file pointer

  } else { // prevent double prompting
    Shell::prompt(); // call prompt
  }

  yyparse();
}

Command Shell::_currentCommand;
