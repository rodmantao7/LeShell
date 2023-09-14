/*
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>

#define MAX_BUFFER_LINE 2048

extern void tty_raw_mode(void);

// Buffer where line is stored
int line_length;
int pos;
char line_buffer[MAX_BUFFER_LINE];
//char temp_buffer[MAX_BUFFER_LINE];

// Simple history array
// This history does not change. 
// Yours have to be updated.
int history_index = 0;
//int prev_history_index;
/*
char * history [] = {
  "ls -al | grep x", 
  "ps -e",
  "cat read-line-example.c",
  "vi hello.c",
  "make",
  "ls -al | grep xxx | grep yyy"
};
*/
char **history = NULL;
int history_length = 0;
int history_size = 5;

void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n"
    " ctrl-D       Delete key at the current position\n"
    " ctrl-H       Removes the character at the position before the cursor\n"
    " ctrl-A       The cursor moves to the beginning of the line\n"
    " ctrl-E       The cursor moves to the end of the line\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {

  // Set terminal in raw mode
  struct termios og_attributes;
  tcgetattr(0, &og_attributes);
  tty_raw_mode();

  line_length = 0;
  pos = 0;

  // Read one line until enter is typed
  while (1) {
    if (history == NULL) {
      history = (char **) malloc(history_size * sizeof(char **));

    }
    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);


    if (ch >= 32 && ch < 127) {
      // It is a printable character. 
      if (pos == line_length) {
        // Do echo
        write(1,&ch,1);

        // If max number of character reached return.
        if (line_length==MAX_BUFFER_LINE-2) break; 

        // add char to buffer.
        line_buffer[pos]=ch;
        line_length++;
        pos++;
      } else {
        char *temp = (char *) malloc(MAX_BUFFER_LINE * sizeof(char));
        for (int i = 0; i < MAX_BUFFER_LINE; i++) {
          if (line_buffer[pos + i] == '\0') {
            break;
          }
          temp[i] = line_buffer[pos + i];
        }

        write(1, &ch, 1);

        if (line_length == MAX_BUFFER_LINE - 2) {
          break;
        }

        line_buffer[pos]=ch;
        line_length++;
        pos++;

        int count = 0;
        for (int j = 0; j < MAX_BUFFER_LINE; j++) {
          count++;
          write(1, &temp[j], 1);
          if (line_buffer[pos + j] == '\0') {
            break;
          }
        }

        char c = 8;
        for (int k = 0; k < count; k++) {
          write(1, &c, 1);
        }
      }

    } else if (ch==10) {
      // <Enter> was typed. Return line
      //if (strlen(line_buffer) != 0) {
        //char *line = (char *) malloc(strlen(line_buffer) * sizeof(char));
        //strcpy(line, line_buffer);
        //char *line = strdup(line_buffer);
      write(1, &ch, 1);
      if (history_length == history_size) {
        history_size *= 2;
        history = (char **) realloc(history, history_size * sizeof(char*));
        assert(history != NULL);
      }
      line_buffer[line_length] = 0;

      if (line_buffer[0]) {
        history[history_length] = (char *) malloc(MAX_BUFFER_LINE * sizeof(char));
        //history[history_length] = line;
        strcpy(history[history_length], line_buffer);
        history_length++;
        //history_index++;
      }
      //if (history_index >= history_length) {
        //history_index = 0;
      //}
      //}

      //printf("%s\n", history[history_index]);
      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      break;
    }
    else if ((ch == 8 || ch == 127) && line_length != 0) {
      // <backspace> was typed. Remove previous character read.

      // Go back one character
      ch = 8;
      write(1,&ch,1);

      //for (int i = line_length - 1; i >= 0; i--) {
        //char c = line_buffer[i];
       // write(1, &c, 1);
      //}

      // Write a space to erase the last character read
      ch = ' ';
      write(1,&ch,1);

      // Go back one character
      //for (int i = 0; i< line_length + 1; i++) {
      ch = 8;
      write(1, &ch, 1);
      //}
      // Remove one character from buffer
      //line_buffer[pos] = '\0';
      line_length--;
      pos--;
    } else if (ch == 4 && line_length != 0) {
      // ctrl-D handling
      int i;
      for(i = pos; i<line_length-1; i++) {
        line_buffer[i] = line_buffer[i+1];
        write(1, &line_buffer[i], 1);
      }
      line_buffer[line_length] = ' ';
      line_length--;

      for(i = pos; i<line_length; i++) {
        ch = line_buffer[i];
        write(1, &ch, 1);
      }

      ch = ' ';
      write(1, &ch, 1);
      ch = 8;
      write(1, &ch, 1);

      for(i = line_length; i > pos; i--) {
        write(1, &ch, 1);
      }
    } else if (ch == 1) {
      // ctrl-A handling

      for (int i = pos; i > 0; i--) {
        ch = 8;
        write(1, &ch, 1);
        pos--;
      }
    } else if (ch == 5) {
      for(int i = pos; i < line_length; i++) {
        char ch = line_buffer[i];
        write(1, &ch, 1);
        pos++;
      }
    } else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1; 
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91 && ch2==65) {
  // Up arrow. Print next line in history.

  // Erase old line
  // Print backspaces
        int i = 0;
        for (i = line_length - pos; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

  // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

  // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        strcpy(line_buffer, history[history_index]);
        line_length = strlen(line_buffer);
        history_index = (history_index + 1) % history_length;

        write(1, line_buffer, line_length);
        pos = line_length;

      } else if (ch1 == 91 && ch2 == 66) {
        // Down arrow handling
        int i = 0;
        for (i = line_length - pos; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

         // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        if(history_index > 0) {
          // Copy line from history
          strcpy(line_buffer, history[history_index]);
          line_length = strlen(line_buffer);
          history_index = (history_index + 1) % history_length;

          // echo line
          write(1, line_buffer, line_length);
          pos = line_length;
        } else {
          strcpy(line_buffer, "");
          line_length = strlen(line_buffer);
          write(1, line_buffer, line_length);
          pos = line_length;
        }

        //write(1, line_buffer, line_length);

      } else if (ch1 == 91 && ch2 == 68) {
        if (pos <= 0) {
          continue;
        }

        if (pos > 0) {
          pos--;
          char c = 8;
          write(1, &c, 1);
        }
      } else if (ch1 == 91 && ch2 == 67) {
        if (pos < line_length) {
          char c = line_buffer[pos];
          write(1, &c, 1);
          pos++;
        }
      }

    }

  }

  // Add eol and null char at the end of string
  line_buffer[line_length]=10;
  line_length++;
  line_buffer[line_length]=0;
  tcsetattr(0, TCSANOW, &og_attributes);

  return line_buffer;
}

