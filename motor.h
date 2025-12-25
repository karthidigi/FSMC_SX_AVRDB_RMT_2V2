bool m1StaVars = 0;
bool m1LastAckSta = 0;
unsigned long m1OnBuffMillis = 0;
int m1StaAckVars = 0;
int a1et = 0;

bool motSimulation = 0;

bool manOnHapp = 1;
bool manOffHap = 1;

void m1On() {
  manOnHapp = 0;
  digitalWrite(PIN_M1ON, HIGH);
  delay(1000);
  digitalWrite(PIN_M1ON, LOW);
  motSimulation = 1;
}


void m1Off() {
  manOffHap = 0;
  digitalWrite(PIN_M1OFF, HIGH);
  delay(1000);
  digitalWrite(PIN_M1OFF, LOW);
  motSimulation = 0;
}

void motorStaCheck() {
  if (curR >= STA_MIN_AMPHS || curY >= STA_MIN_AMPHS || curBM1 >= STA_MIN_AMPHS ) {

    m1StaVars = 1;
    digitalWrite(PIN_LED_GREEN, LOW);
    

    if (!m1LastAckSta) {
      m1OnBuffMillis = millis();
      m1StaAckVars = 1;
      m1LastAckSta = 1;
      //Serial3.println(" motor 1 on - status");
      if (manOnHapp){
        a1et = 6;
      }else{
        manOnHapp = 1;
      }

    }

  } else {

    m1StaVars = 0;
    digitalWrite(PIN_LED_GREEN, HIGH);

    if (m1LastAckSta) {
      m1StaAckVars = 1;
      m1LastAckSta = 0;
      //Serial3.println(" motor 1 off - status");
      if (manOffHap){
        a1et = 7;
      }else{
        manOffHap = 1;
      }
    }
  }
}
