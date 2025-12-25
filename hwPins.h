//////////////////////////////////////

#define LLCC68_MOSI PIN_PA4
#define LLCC68_MISO PIN_PA5
#define LLCC68_SCK PIN_PA6
#define LLCC68_NSS PIN_PA7
#define LLCC68_BUSY PIN_PA2
#define LLCC68_RESET PIN_PA3
#define LLCC68_DIO1 PIN_PB3

#define PIN_VRY PIN_PD1
#define PIN_VYB PIN_PD2
#define PIN_VBR PIN_PD3

#define PIN_IR PIN_PE1
#define PIN_IY PIN_PE2
#define PIN_IBM1 PIN_PE3
#define PIN_IBM2 PIN_PE0

#define PIN_BMS PIN_PD0

#define PIN_RCLK PIN_PB2
#define PIN_RDT PIN_PC2
#define PIN_RSW PIN_PC3

#define PIN_M1ON PIN_PD4
#define PIN_M1OFF PIN_PD5
#define PIN_M2ON PIN_PD6
#define PIN_M2OFF PIN_PD7

#define PIN_LED_BLUE PIN_PC7
#define PIN_LED_RED PIN_PC6
#define PIN_LED_GREEN PIN_PC5
#define PIN_LED_YELLOW PIN_PC4

#define PIN_ERST PIN_PB5

///////////////////////////////////////////////////



void initHwPins() {

  pinMode(PIN_M1ON, OUTPUT);
  pinMode(PIN_M1OFF, OUTPUT);
  pinMode(PIN_M2ON, OUTPUT);
  pinMode(PIN_M2OFF, OUTPUT);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);

  pinMode(PIN_RCLK, INPUT_PULLUP);
  pinMode(PIN_RDT, INPUT_PULLUP);
  pinMode(PIN_RSW, INPUT_PULLUP);  // internal pull-up resistor

  pinMode(PIN_LED_BLUE, INPUT);

  pinMode(LLCC68_NSS, OUTPUT);
  pinMode(LLCC68_RESET, OUTPUT);
  pinMode(LLCC68_BUSY, INPUT);
  pinMode(LLCC68_DIO1, INPUT);

  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_YELLOW, HIGH);
  delay(300);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  delay(300);
  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_YELLOW, HIGH);


  pinMode(PIN_ERST, OUTPUT);
  digitalWrite(PIN_ERST, HIGH);
  delay(100);
  digitalWrite(PIN_ERST, LOW);
  delay(100);
  pinMode(PIN_ERST, INPUT);
}
