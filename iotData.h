bool lastElstAckVars = 0;
int lastElstState = 0;
bool InitAck = 0;
unsigned long iotDataSendMillis = 600000;
//unsigned long iotDataSendMillis = 30000;


void iotDataSendVol(bool now) {
  static unsigned long timerMillis = 0;
  static int prev_a1et = -1, prev_elst = -1, elstAck =-1;
  static float prev_temperature = 0;

  if (now || m1StaAckVars || (millis() - timerMillis > iotDataSendMillis)) {

    // if ( now  || (millis() - timerMillis > iotDataSendMillis)) {
    String payload = "<{\"TS\":{\"a\":" + String(volRY);  //
    payload += ",\"b\":" + String(volYB);                 //
    payload += ",\"c\":" + String(volBR);                 //

    payload += ",\"d\":\"" + String(curR);      //
    payload += "\",\"e\":\"" + String(curY);    //
    payload += "\",\"f\":\"" + String(curBM1);  //

    payload += "\",\"j\":" + String(m1StaVars);  //
    payload += ",\"l\":" + String(storage.app2Run);


    // if (elst > 0 && prev_elst != elst) {
    //   payload += ",\"k\":" + String(27-elst);  //
    //   payload += ",\"n\":" + String(27-elst);  //
    //   elstAck = 1;
    //   prev_elst = elst;
    // }else if (a1et != prev_a1et) {
    //   payload += ",\"k\":" + String(a1et);  //
    //   payload += ",\"n\":" + String(a1et);  //
    //   prev_a1et = a1et;
    // }
    // if (elst == 0 && elstAck) {
    //   payload += ",\"k\":" + String(27-elst);  //
    //   payload += ",\"n\":" + String(27-elst);  //
    //   elstAck = 0;
    // }

    if (elst > 0) {
      a1et = 27 - elst;
      prev_elst = elst;
    } else if (prev_elst > 0) {
      a1et = 27 - elst;
      prev_elst = 0;
    }

    if (a1et != prev_a1et) {
      payload += ",\"k\":" + String(a1et);  //
      payload += ",\"n\":" + String(a1et);  //
      prev_a1et = a1et;
    }


    ds3231_getTemperature();
    if (prev_temperature != dsTemp) {
      payload += ",\"t\":\"" + String(dsTemp) + "\"";  //
      //Serial3.println(dsTemp);
      prev_temperature = dsTemp;
    }

    if(elst != elstAck){
      payload += ",\"o\":" + String(elst);
      elstAck = elst;
    }
    
    payload += "}}>";

    iotSerial.println(payload);  //
    Serial3.println(payload);    //

    m1StaAckVars = 0;
    timerMillis = millis();
  }
}



void ackElst() {
  if (millis() > 30000) {
    if (lastElstState != elst) {
      iotDataSendVol(1);
      lastElstState = elst;
    }
  }
}

void AttribIotDatSendSend() {
  static int lastValue = -1;            // store previous value
  static unsigned long changeTime = 0;  // when the change was detected
  static bool pending = false;          // waiting flag
  static int SCValue = 1;               // store previous value

  if (InitAck && (storage.modeM1 != lastValue)) {
    lastValue = storage.modeM1;
    changeTime = millis();
    pending = true;
    Serial3.println("Sending AttribIotDatSendSend");
    Serial3.println(lastValue);
  }
  if (pending && millis() - changeTime >= 1000) {
    switch (SCValue) {
      case 1:
        Serial3.println("Sending case 1");
        iotSerial.println("<{\"AT\":{\"a1ops\":\"" + String(storage.modeM1) + "\"}}>");
        Serial3.println("<{\"AT\":{\"a1ops\":\"" + String(storage.modeM1) + "\"}}>");
        SCValue++;
        break;

      case 2:
        Serial3.println("Sending case 2");
        iotDataSendVol(1);
        Serial3.println("Sending AttribIotDatSendSend");
        SCValue++;
        break;

      default:
        SCValue = 1;
        pending = false;
    }
  }
}