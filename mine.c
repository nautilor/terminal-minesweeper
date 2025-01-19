#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define COLUMNS 15
#define ROWS 15
#define COVERAGE 20
#define EMPTY_CELL ' '
#define MINE_CELL '@'
#define HIDDEN_CELL '.'
#define CLEAR "\x1B[2J"
#define CLEARLINE "\x1B[2K"
#define CURSOR "\x1B[%d;%dH"
#define UP "\x1B[%dA"
#define DOWN "\x1B[%dB"
#define RIGHT "\x1B[%dC"
#define LEFT "\x1B[%dD"
#define RESET "\x1B[0m"
#define HIDE_CURSOR "\x1B[?25l"
#define SHOW_CURSOR "\x1B[?25h"
#define CELL_TYPE 0
#define MINE_TYPE 1

struct Cell {
  int type;
  char value;
  int revealed;
  int col, row;
};

struct Cursor {
  int x, y;
};

struct Field {
  int columns, rows;
  int coverage;
  struct Cell **cells;
  struct Cursor cursor;
} Field_Default = {COLUMNS, ROWS, COVERAGE, NULL, {0, 0}};

typedef struct Field Field;

// Get a character directly from the terminal without waiting for a newline
int getch(void) {
  int ch;
  struct termios oldt;
  struct termios newt;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  ch = getchar();
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

// Initialize the terminal with the necessary settings
void initialize_term() {
  srand(time(NULL));
  printf(CLEAR);
  printf(HIDE_CURSOR);
  printf(CURSOR, 1, 1);
}

// Reset the terminal to its original state
void finalize_term() {
  printf(SHOW_CURSOR);
  printf(RESET);
  printf(CLEAR);
  printf(CURSOR, 1, 1);
}

// Initialize the field with the given dimensions and coverage
void initialize_field(struct Field *f, int cols, int rows, int cov) {
  f->cells = (struct Cell **)malloc(sizeof(struct Cell *) * rows);
  for (int i = 0; i < rows; i++) {
    f->cells[i] = (struct Cell *)malloc(sizeof(struct Cell) * cols);
    for (int j = 0; j < cols; j++) {
      f->cells[i][j].type = CELL_TYPE;
      f->cells[i][j].value = HIDDEN_CELL;
      f->cells[i][j].revealed = 0;
      f->cells[i][j].col = j;
      f->cells[i][j].row = i;
    }
  }
}

// Print the field to the terminal
void print_field(struct Field *f) {
  for (int i = 0; i < f->rows; i++) {
    for (int j = 0; j < f->columns; j++) {
      if (f->cells[i][j].value == '0') {
        f->cells[i][j].value = EMPTY_CELL;
      }
      if (f->cursor.x == j && f->cursor.y == i) {
        printf("[%c]", f->cells[i][j].value);
      } else {
        printf(" %c ", f->cells[i][j].value);
      }
    }
    printf("\n");
  }
}

// Move the cursor in the given direction
void move_cursor(struct Field *f, int direction) {
  switch (direction) {
  case 0:
    if (f->cursor.y > 0)
      f->cursor.y--;
    break;
  case 1:
    if (f->cursor.y < f->rows - 1)
      f->cursor.y++;
    break;
  case 2:
    if (f->cursor.x < f->columns - 1)
      f->cursor.x++;
    break;
  case 3:
    if (f->cursor.x > 0)
      f->cursor.x--;
    break;
  }
}

// Change the state of the cell at the cursor position
void reveal_cell(struct Field *f) {
  int x = f->cursor.x, y = f->cursor.y;
  if (f->cells[y][x].revealed)
    return;
  f->cells[y][x].revealed = 1;
  if (f->cells[y][x].type != CELL_TYPE) {
    f->cells[y][x].value = MINE_CELL;
  }
}

// Generate mines in the field based on the coverage percentage
void generate_mines(struct Field *f) {
  int mines = (f->columns * f->rows) * f->coverage / 100;
  while (mines) {
    int x = rand() % f->columns;
    int y = rand() % f->rows;
    if (f->cells[y][x].type == MINE_TYPE ||
        (x == f->cursor.x && y == f->cursor.y))
      continue;
    f->cells[y][x].type = MINE_TYPE;
    mines--;
  }
}

// Set the value of the cell to the number of neighbouring bombs
void set_neighbours_bombs(struct Field *f) {
  if (f->cells[f->cursor.y][f->cursor.x].type == MINE_TYPE)
    return;
  int x = f->cursor.x, y = f->cursor.y;
  int count = 0;
  for (int i = y - 1; i <= y + 1; i++) {
    for (int j = x - 1; j <= x + 1; j++) {
      if (i >= 0 && i < f->rows && j >= 0 && j < f->columns) {
        if (f->cells[i][j].type == MINE_TYPE && !(i == y && j == x))
          count++;
      }
    }
  }
  f->cells[y][x].value = count + '0';
}

// Reveal all cells in the field
void reveal_all_cells(struct Field *f) {
  for (int i = 0; i < f->rows; i++) {
    for (int j = 0; j < f->columns; j++) {
      f->cursor.x = j;
      f->cursor.y = i;
      if (f->cells[i][j].type == CELL_TYPE)
        f->cells[i][j].value = HIDDEN_CELL;
      reveal_cell(f);
    }
  }
}

// Check if all the cells have been revealed
int has_won(struct Field *f) {
  for (int i = 0; i < f->rows; i++) {
    for (int j = 0; j < f->columns; j++) {
      if (f->cells[i][j].type == CELL_TYPE && !f->cells[i][j].revealed)
        return 0;
    }
  }
  return 1;
}

// Print the message for the player losing the game
void player_lost(struct Field *f) {
  reveal_all_cells(f);
  printf(UP, f->rows);
  print_field(f);
  printf("You Lost!\n");
  printf("Press any key to exit\n");
  getch();
}

// Print the message for the player winning the game
void player_won(struct Field *f) {
  reveal_all_cells(f);
  printf(UP, f->rows);
  print_field(f);
  printf("You Won!\n");
  printf("Press any key to exit\n");
  getch();
}

// Ask the player if they want to quit the game
int should_quit() {
  printf("Are you sure you want to quit? (y/n)\n");
  int c = getch();
  if (c == 'y') {
    return 0;
  } else {
    printf(CLEARLINE);
    printf(UP, 1);
    printf(CLEARLINE);
    return 1;
  }
}

// Handle the SIGINT signal (Ctrl+C) to reset the terminal before exiting
void sigint_handler() {
  finalize_term();
  exit(0);
}

// TODO: Area discover when cell is empty
// TODO: Flagging
int main() {
  signal(SIGINT, sigint_handler);
  initialize_term();
  int running = 1, first_move = 1;
  Field field = Field_Default;
  initialize_field(&field, COLUMNS, ROWS, COVERAGE);
  print_field(&field);

  while (running) {
    int c = getch();
    switch (c) {
    case 'w':
      move_cursor(&field, 0);
      break;
    case 's':
      move_cursor(&field, 1);
      break;
    case 'd':
      move_cursor(&field, 2);
      break;
    case 'a':
      move_cursor(&field, 3);
      break;
    case 'q':
      running = should_quit();
      break;
    case ' ':
      if (first_move) {
        generate_mines(&field);
        first_move = 0;
      }
      set_neighbours_bombs(&field);
      reveal_cell(&field);
      if (field.cells[field.cursor.y][field.cursor.x].type == MINE_TYPE) {
        player_lost(&field);
        running = 0;
      } else if (has_won(&field)) {
        player_won(&field);
        running = 0;
      }
      break;
    }
    printf(UP, field.rows);
    print_field(&field);
  }
  finalize_term();
  return 0;
};
