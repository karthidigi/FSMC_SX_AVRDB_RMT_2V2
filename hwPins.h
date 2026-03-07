//////////////////////////////////////

#define LLCC68_MOSI PIN_PA4
#define LLCC68_MISO PIN_PA5
#define LLCC68_SCK PIN_PA6
#define LLCC68_NSS PIN_PC7  // Veppam CUS
#define LLCC68_BUSY PIN_PC5
#define LLCC68_RESET PIN_PC6
#define LLCC68_DIO1 PIN_PC4

#define LLCC68_RXEN -1
#define LLCC68_TXEN -1

// SX1268 aliases – same physical pins as LLCC68 (drop-in replacement)
#define SX1268_MOSI LLCC68_MOSI
#define SX1268_MISO LLCC68_MISO
#define SX1268_SCK LLCC68_SCK
#define SX1268_NSS LLCC68_NSS
#define SX1268_BUSY LLCC68_BUSY
#define SX1268_RESET_PIN LLCC68_RESET  // avoids clash with SX1268_PIN_RESET in driver
#define SX1268_DIO1 LLCC68_DIO1

#define PIN_VRY PIN_PE0
#define PIN_VYB PIN_PE1
#define PIN_VBR PIN_PE2

#define PIN_IR PIN_PE3
#define PIN_IY PIN_PF4
#define PIN_IBM1 PIN_PF5
#define PIN_IBM2 -1

#define PIN_BMS PIN_PD7

#define PIN_BYPass PIN_PA0
#define PIN_KEY_ADC PIN_PD6

#define PIN_M1ON PIN_PD2
#define PIN_M1OFF PIN_PD3
#define PIN_M2ON PIN_PD4
#define Bypass PIN_PD5

#define PIN_LED_BLUE PIN_PA3
#define PIN_LED_RED PIN_PA2    // LOGIC low turns on the LED
#define PIN_LED_GREEN PIN_PA2  // Logic high turns on the LED
#define PIN_LED_YELLOW PIN_PA1

#define PIN_ERST PIN_PF6

// 1602 LCD in 4-bit parallel mode
#define PIN_LCD_RS PIN_PA7
#define PIN_LCD_EN PIN_PB2
#define PIN_LCD_D4 PIN_PB3
#define PIN_LCD_D5 PIN_PB4
#define PIN_LCD_D6 PIN_PB5
#define PIN_LCD_D7 PIN_PC2

///////////////////////////////////////////////////



void initHwPins() {

  pinMode(PIN_M1ON, OUTPUT);
  pinMode(PIN_M1OFF, OUTPUT);
  digitalWrite(PIN_M1OFF, HIGH);   // NO relay: energise at boot → contact closed → normal run
  pinMode(PIN_M2ON, OUTPUT);
  // pinMode(PIN_M2OFF, OUTPUT);

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);

  pinMode(PIN_KEY_ADC, INPUT);

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
