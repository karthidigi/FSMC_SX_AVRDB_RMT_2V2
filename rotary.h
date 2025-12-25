
int RotCounter = 0;
int lastCLKState;
int currentCLKState;
bool knobRoll = 0;
unsigned long knobRollMillis = 0;

volatile bool rotValPlus = 0;
volatile bool rotValMinus = 0;
volatile bool rotPush = 0;
bool menuUiFunc = 0;

void rotaryInit() {
  lastCLKState = digitalRead(PIN_RCLK);
}




void rotaryFunc() {
  currentCLKState = digitalRead(PIN_RCLK);

  if (currentCLKState != lastCLKState && currentCLKState == LOW) {
    if (digitalRead(PIN_RDT) != currentCLKState) {
      rotValPlus = 0;
      rotValMinus = 1;
      RotCounter++;
      knobRoll = 1;
      knobRollMillis = millis();
    } else {
      rotValMinus = 0;
      rotValPlus = 1;
      RotCounter--;
      if (RotCounter <= 0) {
        RotCounter = 0;
        knobRoll = 1;
        knobRollMillis = millis();
      }
    }
  }
  //DEBUG_PRINTN("Position: " + String(RotCounter));
  lastCLKState = currentCLKState;

  if (digitalRead(PIN_RSW) == LOW) {
    if(!menuUiFunc){
      menuUiFunc = 1;
    }
    rotPush = 1;
    wdt_reset();
    
    DEBUG_PRINTN("Button pressed!");
    delay(300);  // debounce delay
  }
}
