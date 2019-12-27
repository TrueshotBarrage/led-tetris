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
unsigned long longDelay = 1000;
unsigned long shortDelay = 100;

enum stateEnum {
  INIT, 
  WAIT_FOR_FALL, 
  CHECK_FALLABLE, 
  DROP, 
  WAIT_FOR_FALL_AGAIN, 
  CHECK_FALLABLE_AGAIN, 
  PLACE, 
  PLACE_FAST, 
  CLEAR_ROWS, 
  GAME_OVER
};

bool switchOff;
bool next = !switchOff;
int x;
int y;

bool droppable = true; // must assign this
bool placeable = false; // must assign this
uint8_t timer = 10; // longDelay / shortDelay (number of "cycles" per drop)
uint8_t state = INIT;


int pivot = 0;
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
      if (next) state = WAIT_FOR_FALL;
      break;
    
    case WAIT_FOR_FALL:      
      if (timer == 0) {
        state = CHECK_FALLABLE;
        timer = 20;
      } else {
        timer = timer - 1;
      }
      break;
    
    case CHECK_FALLABLE:
      if (droppable) {
        state = DROP;
      } else {
        state = WAIT_FOR_FALL_AGAIN;
      }
      break;
    
    case DROP:
      state = WAIT_FOR_FALL;
      drop();
      break;
    
    case WAIT_FOR_FALL_AGAIN:
      if (timer == 0) {
        state = CHECK_FALLABLE_AGAIN;
        timer = 20;
      } else {
        timer = timer - 1;
      }
      break;
    
    case CHECK_FALLABLE_AGAIN:
      if (droppable) {
        state = DROP;
      } else {
        state = PLACE;
      }
      break;
    
    case PLACE:
      if (placeable) {
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
      state = WAIT_FOR_FALL;
      break;
    
    case GAME_OVER:
      break;
  }
}

void writeBoard() { 
  for (int i = 0; i < 8; i++) {
    board[i] = ground[i] | block[i];
    lc.setRow(0, i, board[i]);
  }
}

// Chooses a random, default tetronimo shape
void chooseRandomTetronimo() {
  int r = random(0, 7);
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
}

void shiftLeft() {  
  byte shiftedBlock[8];

  for (int i = 0; i < 8; i++) {
    shiftedBlock[i] = block[i] << 1;
    if (shiftedBlock[i] < block[i]) break;
  }
  for (int i = 0; i < 8; i++) {
    block[i] = shiftedBlock[i];
  }
}

void shiftRight() {  
  byte shiftedBlock[8];

  for (int i = 0; i < 8; i++) {
    shiftedBlock[i] = block[i] >> 1;
    if (shiftedBlock[i] > block[i]) break;
  }
  for (int i = 0; i < 8; i++) {
    block[i] = shiftedBlock[i];
  }
}
