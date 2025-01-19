#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>

#define COLUMNS 15
#define ROWS 15
#define COVERAGE 20
#define AREA 2
#define EMPTY_CELL ' '
#define MINE_CELL '@'
#define HIDDEN_CELL '.'
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
  tcgetattr(0, &oldt); // STDIN_FILENO
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(0, TCSANOW, &newt); // STDIN_FILENO
  ch = getchar();
  tcsetattr(0, TCSANOW, &oldt); // STDIN_FILENO
  return ch;
}

// Move the cursor to the given position
void cursor_at(int x, int y) { printf("\x1B[%d;%dH", y, x); }

void clear_entire_screen() {
  printf("\x1B[2J");
  for (int i = 1; i < ROWS + 1; i++) {
    cursor_at(1, i);
    printf("\x1B[2K");
  }
  cursor_at(1, 1);
}

void init_term() {
  clear_entire_screen();
  printf("\x1B[?25l");
}

void reset_term() {
  clear_entire_screen();
  printf("\x1B[?25h");
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
int set_neighbours_bombs(struct Field *f) {
  int x = f->cursor.x, y = f->cursor.y;
  if (f->cells[y][x].type == MINE_TYPE)
    return 1;
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
  return count;
}

void reveal_area(struct Field *f) {
  int x = f->cursor.x, y = f->cursor.y;
  for (int i = y - AREA; i <= y + AREA; i++) {
    for (int j = x - AREA; j <= x + AREA; j++) {
      if (i >= 0 && i < f->rows && j >= 0 && j < f->columns) {
        if (f->cells[i][j].type == CELL_TYPE) {
          f->cursor.y = i;
          f->cursor.x = j;
          reveal_cell(f);
          set_neighbours_bombs(f);
        }
      }
    }
  }
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

// Check if all the cells that are not bombs have been revealed
int has_won(struct Field *f) {
  for (int i = 0; i < f->rows; i++) {
    for (int j = 0; j < f->columns; j++) {
      if (f->cells[i][j].type == CELL_TYPE && !f->cells[i][j].revealed)
        return 0;
    }
  }
  return 1;
}

// Check if user stepped on a mine
int has_lost(struct Field *f) {
  return f->cells[f->cursor.y][f->cursor.x].type == MINE_TYPE;
}

// What to do when the match ends wether the player won or lost
void match_end(struct Field *f, char *message) {
  reveal_all_cells(f);
  clear_entire_screen();
  print_field(f);
  printf("%s\n", message);
  printf("Press any key to exit\n");
  getch();
}

// Ask the player if they want to quit the game
int should_quit() {
  printf("Are you sure you want to quit? (y/N)\n");
  int c = getch();
  return c != 'y';
}

// Reset the terminal when Ctrl+C is pressed
void sigint_handler() {
  reset_term();
  exit(0);
}

// TODO: Flagging
int main() {
  srand(time(NULL));
  signal(SIGINT, sigint_handler);
  init_term();
  printf("\x1B[?25l");
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
      if (has_lost(&field)) {
        match_end(&field, "You lost!");
        running = 0;
        break;
      }
      if (has_won(&field)) {
        match_end(&field, "You won!");
        running = 0;
        break;
      }
      if (!set_neighbours_bombs(&field))
        reveal_area(&field);
      else
        reveal_cell(&field);
      break;
    }
    clear_entire_screen();
    print_field(&field);
  }
  reset_term();
  return 0;
};
