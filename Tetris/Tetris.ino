#include <LedControl.h>

// Define pins for joystick
const int SW_pin = 2; // digital pin connected to switch output
const int X_pin = 0; // analog pin connected to X output
const int Y_pin = 1; // analog pin connected to Y output

/* Now we need a LedControl to work with.
  ***** These pin numbers will probably not work with your hardware *****
  pin 12 is connected to the DataIn 
  pin 11 is connected to LOAD(CS)
  pin 10 is connected to the CLK 
  We have only a single MAX72XX. */
LedControl lc = LedControl(12, 10, 11, 1);

/* we always wait a bit between updates of the display */
unsigned long longDelay = 500;
unsigned long shortDelay = 100;

enum stateEnum {
  INIT, // state 0
  WAIT_FOR_DROP, // state 1
  CHECK_DROPPABLE, // state 2
  DROP, // state 3
  WAIT_FOR_DROP_AGAIN, // state 4
  CHECK_DROPPABLE_AGAIN, // state 5
  PLACE, // state 6
  PLACE_FAST, // state 7
  CLEAR_ROWS, // state 8
  GAME_OVER // state 9
};

bool switchOff;
bool next;
int x;
int y;

bool droppable;
bool placeable; 
byte timer = 5; // longDelay / shortDelay (number of "cycles" per drop)
byte state;
bool restart;

int shiftCount;
int dropCount;
int rotation;
int r;
byte block[8];

byte ground[8] = {
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000
};

byte board[8] = {
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000, 
  B00000000
};

const byte tetronimos[7][4][4] = {
  // I
  {
    // rotated 0
    { B00111100, B00000000, B00000000, B00000000 },
    // rotated 90
    { B00001000, B00001000, B00001000, B00001000 }, 
    // rotated 180
    { B00000000, B00000000, B00111100, B00000000 },
    // rotated 270
    { B00010000, B00010000, B00010000, B00010000 }
  },
  
  // O
  {
    // rotated 0
    { B00011000, B00011000, B00000000, B00000000 }, 
    // rotated 90
    { B00011000, B00011000, B00000000, B00000000 }, 
    // rotated 180
    { B00011000, B00011000, B00000000, B00000000 }, 
    // rotated 270
    { B00011000, B00011000, B00000000, B00000000 }
  },

  // T
  {
    // rotated 0
    { B00010000, B00111000, B00000000, B00000000 }, 
    // rotated 90
    { B00010000, B00011000, B00010000, B00000000 }, 
    // rotated 180
    { B00000000, B00111000, B00010000, B00000000 }, 
    // rotated 270
    { B00010000, B00110000, B00010000, B00000000 }
  },

  // S
  {
    // rotated 0
    { B00011000, B00110000, B00000000, B00000000 }, 
    // rotated 90
    { B00010000, B00011000, B00001000, B00000000 }, 
    // rotated 180
    { B00000000, B00011000, B00110000, B00000000 }, 
    // rotated 270
    { B00100000, B00110000, B00010000, B00000000 }
  },

  // Z
  {
    // rotated 0
    { B00110000, B00011000, B00000000, B00000000 }, 
    // rotated 90
    { B00001000, B00011000, B00010000, B00000000 }, 
    // rotated 180
    { B00000000, B00110000, B00011000, B00000000 }, 
    // rotated 270
    { B00010000, B00110000, B00100000, B00000000 }
  },

  // J
  {
    // rotated 0
    { B00100000, B00111000, B00000000, B00000000 }, 
    // rotated 90
    { B00011000, B00010000, B00010000, B00000000 }, 
    // rotated 180
    { B00000000, B00111000, B00001000, B00000000 }, 
    // rotated 270
    { B00010000, B00010000, B00110000, B00000000 }
  },

  // L
  {
    // rotated 0
    { B00001000, B00111000, B00000000, B00000000 },
    // rotated 90
    { B00010000, B00010000, B00011000, B00000000 },
    // rotated 180 
    { B00000000, B00111000, B00100000, B00000000 },
    // rotated 270
    { B00110000, B00010000, B00010000, B00000000 }
  }
};

void setup() {
  pinMode(SW_pin, INPUT);
  digitalWrite(SW_pin, HIGH);
  
  lc.shutdown(0, false); // The MAX72XX is in power-saving mode on startup, we have to do a wakeup call
  lc.setIntensity(0, 8); // Set the brightness to a medium values
  lc.clearDisplay(0); // and clear the display

  state = INIT;
  
  Serial.begin(9600);
}

// main loop
void loop() {
  // Various I/O operations happen here throughout the loop.
  switchOff = digitalRead(SW_pin);
  x = analogRead(X_pin);
  y = analogRead(Y_pin);

  if ((state != PLACE) && (state != CHECK_DROPPABLE_AGAIN) && (state != WAIT_FOR_DROP_AGAIN)) {
    if (x > 895) {
      shiftLeft();
    } else if (x < 128) {
      shiftRight();
    }
  }

  if (!switchOff && (state != INIT)) {
    rotate();
  }

  if (state != INIT) {
    if (y < 128) {
      state = PLACE_FAST;
    }
  }
  
  stateTransitionLogic();
  
  if (state != GAME_OVER) {
    writeBoard();
  }
  
  Serial.print("Current state: ");
  Serial.println(state);

  delay(shortDelay);
}

// Full FSM for the Tetris game. Includes all 
// the state transition logic, inputs/outputs, 
// and exit/loop conditions. 
void stateTransitionLogic() {
  switch(state) {
    // State 0: Initial start-up state.
    case INIT:
      // Clear the ground & board
      for (int i = 0; i < 8; i++) {
        ground[i] = B00000000;
        board[i] = B00000000;
      }

      // Resets various conditions 
      droppable = true;
      placeable = true; 
      shiftCount = 0;
      dropCount = 0;
      rotation = 0;
      r = 0;

      // Choose a random tetronimo every time game cycles INIT, 
      // which also makes for a nice animation :-)
      chooseRandomTetronimo();

      // Wait for button to start game
      next = !switchOff;
      if (next) {
        state = WAIT_FOR_DROP;
        Serial.print("Game start!");
      }
      break;

    // State 1: Starts the timer and checks whether enough 
    // "cycles" have passed for the block to be ready to drop.
    case WAIT_FOR_DROP:      
      if (timer == 0) {
        state = CHECK_DROPPABLE;
        timer = 5;
      } else {
        timer = timer - 1;
      }
      break;

    // State 2: Checks whether block can be successfully dropped 
    // to the row below without interference. If not, goes through  
    // another pass (just once more) to simulate delay in Tetris.
    case CHECK_DROPPABLE:
      if (block[7] != 0) {
        droppable = false;
      } else {
        for (int i = 0; i < 7; i++) {
          if ((block[i] & ground[i+1]) != 0) {
            droppable = false;
          }
        }
      }
      
      if (droppable) {
        state = DROP;
      } else {
        state = WAIT_FOR_DROP_AGAIN;
      }
      break;

    // State 3: Drops the block onto the row below.
    case DROP:
      drop();
      state = WAIT_FOR_DROP;
      break;

    // State 4: Same as state 1; second pass.
    case WAIT_FOR_DROP_AGAIN:
      if (timer == 0) {
        state = CHECK_DROPPABLE_AGAIN;
        timer = 5;
      } else {
        timer = timer - 1;
      }
      break;

    // State 5: Same as state 2; second pass. Proceeds 
    // to place the block onto ground if obstructed still.
    case CHECK_DROPPABLE_AGAIN:
      if (block[7] != 0) {
        droppable = false;
      } else {
        for (int i = 0; i < 7; i++) {
          if ((block[i] & ground[i+1]) != 0) {
            droppable = false;
          }
        }
      }
      
      if (droppable) {
        state = DROP;
      } else {
        state = PLACE;
      }
      break;

    // State 6: Checks whether the block can be placed onto 
    // the existing ground and placees it. If not possible, 
    // game is over and transitions to GAME_OVER.
    case PLACE:
      placeable = ((ground[0] & B00111100) == 0);
      
      if (placeable) {
        for (int i = 0; i < 8; i++) {
          ground[i] = block[i] | ground[i];
          block[i] = B00000000;
        }
        state = CLEAR_ROWS;
      } else {
        state = GAME_OVER;
      }
      break;

    // State 7: Same as state 6, but toggles when joystick 
    // is pushed down. Tries to place as far below possible.
    case PLACE_FAST:
      while (droppable) {
        if (block[7] != 0) {
          droppable = false;
        } else {
          for (int i = 0; i < 7; i++) {
            if ((block[i] & ground[i+1]) != 0) {
              droppable = false;
            }
          }
        }
        if (droppable) drop();
      }

      placeable = ((ground[0] & B00111100) == 0);
      
      if (placeable) {
        for (int i = 0; i < 8; i++) {
          ground[i] = block[i] | ground[i];
          block[i] = B00000000;
        }
        state = CLEAR_ROWS;
      } else {
        state = GAME_OVER;
      }
      break;

    // State 8: Occurs when a block is successfully placed in 
    // the previous state cycle. Checks if any rows can be 
    // cleared, and if so, "DEW IT" - Emperor Palpatine
    case CLEAR_ROWS:
      // Assume that the top-most row will never be cleared
      for (int i = 7; i > 0; i--) {
        if (ground[i] == B11111111) {
          for (int j = i; j > 0; j--) {
            ground[j] = ground[j-1];
          }
          i++;
        }
      }
      
      // Needs to choose another new tetronimo for the next block
      chooseRandomTetronimo();
      droppable = true;
      state = WAIT_FOR_DROP;
      break;

    // State 9: GG. Prints "GG" onto the board until new 
    // game is started by pressing joystick button. 
    case GAME_OVER:
      restart = !switchOff;
      if (restart) {
        state = INIT;
      } else {
        writeGG();
        Serial.print("GG");
      }
      break;
  }
}

// I/O for the 8x8 matrix LED board. Writes the current 
// state of the board at every pass of the loop, with a 
// small, specified delay (shortDelay).
void writeBoard() { 
  for (int i = 0; i < 8; i++) {
    board[i] = (ground[i] | block[i]);
    lc.setRow(0, i, board[i]);
  }
}

// Writes "GG" onto the board when game is over. 
void writeGG() {
  byte boardGG[8] = { 
    B00000000, 
    B11100111, 
    B10000100, 
    B10000100, 
    B10100101, 
    B10100101, 
    B11100111, 
    B00000000, 
  };
  
  for (int i = 7; i >= 0; i--) {
    lc.setRow(0, i, boardGG[i]);
    delay(50);
  }
  delay(50);
  
  for (int i = 7; i >= 0; i--) {
    lc.setRow(0, i, boardGG[7]);
    delay(50);
  }
  delay(50);
}

// Chooses a random, default tetronimo shape.
void chooseRandomTetronimo() {
  dropCount = 0;
  shiftCount = 0;
  rotation = 0;
  r = random(0, 7);
  for (int i = 0; i < 4; i++) {
    block[i] = tetronimos[r][0][i];
  }
  for (int i = 4; i < 8; i++) {
    block[i] = B00000000;
  }
}

// Drops the tetronimo down a row.
void drop() {
  for (int i = 7; i > 0; i--) {
    block[i] = block[i-1];
  }
  block[0] = B00000000;
  dropCount++;
}

// Rotates the tetronimo 90 degrees clockwise.
void rotate() {
  // 0...90...180...270...0...
  if (rotation == 3) rotation = 0;
  else rotation++;

  // Represents cycling through the possible shapes. 
  // The first dimension is set to "r", which are 
  // various rotations of the given tetronimo. 
  for (int i = 0; i < 4; i++) {
    block[i] = tetronimos[r][rotation][i];
  }
  for (int i = 4; i < 8; i++) {
    block[i] = B00000000;
  }

  // Choosing a new tetronimo as above always 
  // resets the value, so we need to vertically 
  // offset it to the right position. 
  for (int i = 0; i < dropCount; i++) {
    drop();
    dropCount--;
  }

  // We apply the same offsetting procedure here, 
  // except horizontally. 
  if (shiftCount < 0) {
    for (int i = 0; i > shiftCount; i--) {
      for (int j = 0; j < 8; j++) {
        block[j] = block[j] << 1;
      }
    }
  } else if (shiftCount > 0) {
    for (int i = 0; i < shiftCount; i++) {
      for (int j = 0; j < 8; j++) {
        block[j] = block[j] >> 1;
      }
    }
  }
}

// Shifts the tetronimo left one column, if possible.
void shiftLeft() {
  bool shiftable = true;
  
  for (int i = 0; i < 8; i++) {
    if ((block[i] << 1) > 255 || ((block[i] << 1) & ground[i]) != 0) {
      shiftable = false;
      break;
    }
  }
  if (shiftable) {
    for (int i = 0; i < 8; i++) {
      block[i] = block[i] << 1;
    }
    shiftCount--;
  }
}

// Shifts the tetronimo right one column, if possible.
void shiftRight() {
  bool shiftable = true;
  
  for (int i = 0; i < 8; i++) {
    if (((block[i] >> 1) << 1) != block[i] || ((block[i] >> 1) & ground[i]) != 0) {
      shiftable = false;
      break;
    }
  }
  if (shiftable) {
    for (int i = 0; i < 8; i++) {
      block[i] = block[i] >> 1;
    }
    shiftCount++;
  }
}
