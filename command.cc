#include <cstdio>
#include <cstdlib>
#include <wait.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "command.hh"
#include "shell.hh"
#include <cstring>

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _append = 0;
    _background = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    // get the command
    std::string* command = _simpleCommands[0]->_arguments[0];

    // setenv handling
    if (strcmp(command->c_str(), "setenv") == 0) {
      if ((_simpleCommands[0]->_arguments.size()) != 3) {
        perror("setenv");
        return;
      }

      const char *a = _simpleCommands[0]->_arguments[1]->c_str();
      const char *b = _simpleCommands[0]->_arguments[2]->c_str();
      setenv(a, b, 1);
      clear();
      Shell::prompt();
      return;
    }

    // unsetenv handling
    if (strcmp(command->c_str(), "unsetenv") == 0) {
      const char *a = _simpleCommands[0]->_arguments[1]->c_str();
      unsetenv(a);
      clear();
      Shell::prompt();
      return;
    }

    // cd handling
    if (strcmp(command->c_str(), "cd") == 0) {
      if(_simpleCommands[0]->_arguments.size() == 1) {
        chdir(getenv("HOME"));
      } else {
        const char *dir = _simpleCommands[0]->_arguments[1]->c_str();
        int tmp = chdir(dir);
        if (tmp < 0) {
          fprintf(stderr, "cd: can't cd to %s: ", dir);
          perror("");
        }
      }
      clear();
      Shell::prompt();
      return;
    }

    // geting tempIn, tempOut, tempErr from dup
    int tempIn = dup(0);
    int tempOut = dup(1);
    int tempErr = dup(2);

    // fdIn, out, err to open _inFile, _outFile, _errFile
    int fdIn = 0;
    int fdOut = 0;
    int fdErr = 0;

    // open _inFile if it's not null, otherwise dup(tempIn)
    if (_inFile != NULL) {
      fdIn = open(_inFile->c_str(), O_RDONLY, 0600);
    } else {
      fdIn = dup(tempIn);
    }

    // open _errFile if it's not null, otherwise dup(tempErr)
    if (_errFile != NULL) {
        if (_append == 1) {
          fdErr = open(_errFile->c_str(), O_WRONLY | O_APPEND | O_CREAT, 0600);
        } else {
          fdErr = open(_errFile->c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0600);
        }
    } else {
       fdErr = dup(tempErr);
    }

    // redirect fdErr, then close
    dup2(fdErr, 2);
    close(fdErr);

    // initialize pid
    int pid;

    // For loop to go over the commands
    for (size_t i = 0; i < _simpleCommands.size(); i++) {
      // redirect fdIn, then close it
      dup2(fdIn, 0);
      close(fdIn);

      // if it's the last word in command
      if (i == _simpleCommands.size() - 1) {
        // if _outFile is not null, open it and check for appending
        if (_outFile != NULL) {
          if (_append == 1) {
            fdOut = open(_outFile->c_str(), O_WRONLY | O_APPEND | O_CREAT, 0600);
          } else {
            fdOut = open(_outFile->c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0600);
          }
        } else {
          fdOut = dup(tempOut);
        }

      } else {
        // piping
        int fdpipe[2];
        pipe(fdpipe);
        fdIn = fdpipe[0];
        fdOut = fdpipe[1];
      }

      // redirecting fdOut, then close it
      dup2(fdOut, 1);
      close(fdOut);
      // set environment for _, the last argument in the fully expanded previous command
      setenv("_", _simpleCommands[i]->_arguments[_simpleCommands[i]->_arguments.size() - 1]->c_str(), 1);

      // fork() time!!
      pid = fork();

      if (pid < 0) { // fork() error handling
        perror("fork");
        return;
      } else if (pid == 0) { // if in the child process

        // printenv handling
        if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "printenv") == 0) {
          char **env = environ;
          while (*env) {
            printf("%s\n", *env);
            env++;
          } // while printenv
          exit(1);
        }

        // set environment for _, the last argument in the fully expanded previous command
        //setenv("_", _simpleCommands[i]->_arguments[_simpleCommands[i]->_arguments.size() - 1]->c_str(), 1);

        // getting the commands, and convert it to chars**
        const char ** argv = (const char **) malloc((_simpleCommands[i]->_arguments.size() + 1) * sizeof(const char *));
        for (size_t j = 0; j < _simpleCommands[i]->_arguments.size(); j++) {
          argv[j] = _simpleCommands[i]->_arguments[j]->c_str();
        } //for (convert to char**)


        // ZA SHELL ENGINE ("Where the magic happens" - Gustavo, 2023)
        argv[_simpleCommands[i]->_arguments.size()] = NULL;
        execvp(argv[0], (char* const*)argv);
        waitpid(pid, NULL, 0);
        perror("execvp");
        exit(1);

      } // else if pid
    } // for _simpleCommands.size()

    // redirect and close everything
    dup2(fdOut, 1);
    close(fdOut);
    dup2(tempIn, 0);
    dup2(tempOut, 1);
    dup2(tempErr, 2);
    close(tempIn);
    close(tempOut);
    close(tempErr);

    // background stuff
    if(!_background) {
      int stat_loc = 0;
      waitpid(pid, &stat_loc, 0);

      // set environment for ?
      std::string statLoc = std::to_string(WEXITSTATUS(stat_loc));
      setenv("?", statLoc.c_str(), 1);

    } else {
      // set environment for !
      std::string pidStr = std::to_string(pid);
      setenv("!", pidStr.c_str(), 1);
    }

    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;
