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

bool droppable = true;
bool placeable = true; // TODO
byte timer = 5; // longDelay / shortDelay (number of "cycles" per drop)
byte state;

int shiftCount = 0;
int dropCount = 0;
int rotation = 0;
int r = 0;
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
    { B00111100, B00000000, B00000000, B00000000 },
    // rotated 270
    { B00001000, B00001000, B00001000, B00001000 }
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
    { B00010000, B00011000, B00010000, B00000000 },
    // rotated 90
    { B00111000, B00010000, B00000000, B00000000 },
    // rotated 180
    { B00001000, B00011000, B00001000, B00000000 },
    // rotated 270
    { B00010000, B00111000, B00000000, B00000000 }
  },

  // S
  {
    // rotated 0 
    { B00010000, B00011000, B00001000, B00000000 },
    // rotated 90
    { B00001100, B00011000, B00000000, B00000000 }, 
    // rotated 180 
    { B00010000, B00011000, B00001000, B00000000 },
    // rotated 270
    { B00001100, B00011000, B00000000, B00000000 }
  },

  // Z
  {
    // rotated 0
    { B00001000, B00011000, B00010000, B00000000 }, 
    // rotated 90
    { B00110000, B00011000, B00000000, B00000000 }, 
    // rotated 180
    { B00001000, B00011000, B00010000, B00000000 }, 
    // rotated 270
    { B00110000, B00011000, B00000000, B00000000 }
  },

  // J
  {
    // rotated 0
    { B00011000, B00010000, B00010000, B00000000 },
    // rotated 90
    { B00111000, B00001000, B00000000, B00000000 },
    // rotated 180 
    { B00001000, B00001000, B00011000, B00000000 },
    // rotated 270
    { B00010000, B00011100, B00000000, B00000000 }
  },

  // L
  {
    // rotated 0
    { B00011000, B00001000, B00001000, B00000000 },
    // rotated 90
    { B00001000, B00111000, B00000000, B00000000 },
    // rotated 180 
    { B00010000, B00010000, B00011000, B00000000 },
    // rotated 270
    { B00011100, B00010000, B00000000, B00000000 }
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
  switchOff = digitalRead(SW_pin);
  x = analogRead(X_pin);
  y = analogRead(Y_pin);

  if (x > 895) {
    shiftLeft();
  } else if (x < 128) {
    shiftRight();
  }

  if (!switchOff && (state != INIT)) {
    rotate();
  }
  
  stateTransitionLogic();
  writeBoard();
  Serial.print("current state: ");
  Serial.println(state);

  delay(shortDelay);
}

void stateTransitionLogic() {
  switch(state) {
    case INIT:
      chooseRandomTetronimo();
      next = !switchOff;
      if (next) {
        state = WAIT_FOR_DROP;
        Serial.print("Game start!");
      }
      break;
    
    case WAIT_FOR_DROP:      
      if (timer == 0) {
        state = CHECK_DROPPABLE;
        timer = 5;
      } else {
        timer = timer - 1;
      }
      break;
    
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
    
    case DROP:
      drop();
      state = WAIT_FOR_DROP;
      break;
    
    case WAIT_FOR_DROP_AGAIN:
      if (timer == 0) {
        state = CHECK_DROPPABLE_AGAIN;
        timer = 5;
      } else {
        timer = timer - 1;
      }
      break;
    
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
    
    case PLACE:
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
    
    case PLACE_FAST:
      if (placeable) {
        state = CLEAR_ROWS;
      } else {
        state = GAME_OVER;
      }
      break;
    
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
      chooseRandomTetronimo();
      droppable = true;
      state = WAIT_FOR_DROP;
      break;
    
    case GAME_OVER:
      break;
  }
}

void writeBoard() { 
  for (int i = 0; i < 8; i++) {
    board[i] = (ground[i] | block[i]);
    lc.setRow(0, i, board[i]);
  }
}

// Chooses a random, default tetronimo shape
void chooseRandomTetronimo() {
  dropCount = 0;
  shiftCount = 0;
  r = random(0, 7);
  for (int i = 0; i < 4; i++) {
    block[i] = tetronimos[r][0][i];
  }
  for (int i = 4; i < 8; i++) {
    block[i] = B00000000;
  }
}

void drop() {
  for (int i = 7; i > 0; i--) {
    block[i] = block[i-1];
  }
  block[0] = B00000000;
  dropCount++;
}

void rotate() {
  if (rotation == 3) rotation = 0;
  else rotation++;
  
  for (int i = 0; i < 4; i++) {
    block[i] = tetronimos[r][rotation][i];
  }
  for (int i = 4; i < 8; i++) {
    block[i] = B00000000;
  }

  for (int i = 0; i < dropCount; i++) {
    drop();
    dropCount--;
  }
  
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
