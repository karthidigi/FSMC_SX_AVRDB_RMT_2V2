
bool autoM1Trigd = 0;
unsigned long autoM1BuffMillis = 0;
unsigned long atSec = 0;
unsigned long atMins = 0;
unsigned long atLastTrigdMillis = 0;

bool countM1Trigd = 0;
long countM1BuffMills = 0;


unsigned long cyM1BuffMillis = 0;
unsigned long cyOnHrs = 0;
unsigned long cyOnMins = 0;
unsigned long cyOffHrs = 0;
unsigned long cyOffMins = 0;

unsigned long cTHrs = 0;
unsigned long cTMins = 0;

bool cyclicM1Init = 0;
int cyclicM1State = 0;

unsigned long cyclicM1OnDurMillis = 0;
unsigned long cyclicM1OffDurMillis = 0;


bool laStaRemM1Trigd = 0;
unsigned long laStaRemM1BuffMillis = 0;
unsigned long laStaRemSec = 0;
unsigned long laStaRemMins = 0;
unsigned long laStaRemM1TrigdMillis = 0;
bool laStaAppStaSavShow = 0;
bool laStaRemM1remTrigrd = 0;



void loadModeVal() {
  autoM1Trigd = 0;
  autoM1BuffMillis = millis();

  atMins = String(storage.autoM1).substring(1, 3).toInt() * 60;
  atSec = String(storage.autoM1).substring(3, 5).toInt();

  countM1Trigd = 0;
  countM1BuffMills = millis();

  cyclicM1Init = 0;
  cyclicM1State = 0;

  cyM1BuffMillis = millis();

  cTHrs = String(storage.countM1).substring(1, 3).toInt() * 60 * 60;
  cTMins = String(storage.countM1).substring(3, 5).toInt() * 60;

  cyOnHrs = String(storage.cyclicM1).substring(1, 3).toInt() * 60 * 60;
  cyOnMins = String(storage.cyclicM1).substring(3, 5).toInt() * 60;
  cyOffHrs = String(storage.cyclicM1).substring(5, 7).toInt() * 60 * 60;
  cyOffMins = String(storage.cyclicM1).substring(7, 9).toInt() * 60;
  cyclicM1OnDurMillis = (cyOnHrs + cyOnMins) * 1000 + millis() + 30000;
  cyclicM1OffDurMillis = (cyOffHrs + cyOffMins) * 1000 + millis();

  laStaRemM1Trigd = 0;
  laStaRemM1BuffMillis = millis();
  laStaRemMins = String(storage.lasRemM1).substring(1, 3).toInt() * 60;
  laStaRemSec = String(storage.lasRemM1).substring(3, 5).toInt();

  // if (storage.app1Run) {
  //   digitalWrite(PIN_LED_YELLOW, LOW);
  // } else {
  //   digitalWrite(PIN_LED_YELLOW, HIGH);
  // }
}

void modeM1() {
  switch (storage.modeM1) {
    ////////////////////////
    // Normal
    case 0:
      //Serial3.println("M1-NormalMode");
      digitalWrite(PIN_LED_YELLOW, HIGH);
      break;

    ////////////////////////
    // Auto
    case 1:
      YellowLedTickerFunc();
      if (!autoM1Trigd && elst == 0) {
        tickYelLedVars = 1;
        if (millis() >= ((atMins + atSec) * 1000) + autoM1BuffMillis) {
          m1On();
          a1et = 13;
          digitalWrite(PIN_LED_YELLOW, LOW);
          atLastTrigdMillis = millis();
          Serial3.println("M1-Auto");
          autoM1Trigd = 1;
          tickYelLedVars = 0;
        }
      }
      if (elst > 0) {
        autoM1BuffMillis = millis();
        autoM1Trigd = 0;
      }
      if (autoM1Trigd && !m1StaVars && (millis() - atLastTrigdMillis >= 3000)) {
        a1et = 13;
        m1On();
        atLastTrigdMillis = millis();
      }

      break;


    ////////////////////////
    // Cyclic
    case 2:

      if (!cyclicM1Init && millis() - cyM1BuffMillis >= 30000) {
        a1et = 16;
        m1On();
        cyclicM1Init = 1;
        cyclicM1State = 1;
        //  Serial3.println("Sending counter ON ticks");
        // iotSerial.println("<{\"TS\":{\"n\":\"15\"}}>");
        // Serial3.println("<{\"TS\":{\"n\":\"15\"}}>");
      }

      if (cyclicM1State == 1) {
        //  if (a1et == 16){
        //   a1et = 18;
        // Serial3.println("Sending counter Off ticks");
        // iotSerial.println("<{\"TS\":{\"n\":\"17\"}}>");
        // Serial3.println("<{\"TS\":{\"n\":\"17\"}}>");
        // }
        if (millis() >= cyclicM1OnDurMillis) {
          a1et = 18;
          m1Off();
          Serial3.println("M1-Cyclic-OFF");
          cyclicM1OffDurMillis = ((cyOffHrs + cyOffMins) * 1000) + millis();
          cyclicM1State = 2;
        }
      }

      if (cyclicM1State == 2) {
        // if (a1et == 18){
        //   a1et = 16;
        // Serial3.println("Sending counter ON ticks");
        // iotSerial.println("<{\"TS\":{\"n\":\"15\"}}>");
        // Serial3.println("<{\"TS\":{\"n\":\"15\"}}>");
        // }
        if (millis() >= cyclicM1OffDurMillis) {
          a1et = 16;
          m1On();
          Serial3.println("M1-Cyclic-ON");
          cyclicM1OnDurMillis = ((cyOnHrs + cyOnMins) * 1000) + millis();
          cyclicM1State = 1;
        }
      }

      if (elst > 0) {
        cyM1BuffMillis = millis();
        cyclicM1Init = 0;
      }

      break;
    ////////////////////////
    // Countdown
    case 3:

      if (countM1Trigd) {
        if (millis() >= ((cTHrs + cTMins) * 1000) + countM1BuffMills) {
          a1et = 20;
          m1Off();

          Serial3.println("M1-CountDown");
          countM1Trigd = 0;
        }
      } else if (m1StaVars) {
        countM1BuffMills = millis();
        countM1Trigd = 1;
      } else {
        countM1Trigd = 0;
      }

      break;
    ////////////////////////
    // Scheduler
    case 4:
      //Serial3.println("M1-Scheduler");
      for (int i = 0; i < 10; i++) {
        //EDHHMMHHMM
        //12345678901234567
        if (strcmp(storage.sch[i], "z00000000000000000") != 0) {

          if (String(storage.sch[i][10 + tWday]).toInt()) {

            if (tSec < 5) {

              if (!m1StaVars) {
                //  Serial3.print(" 4 ");

                if (hrs24 == String(storage.sch[i]).substring(3, 5).toInt() && tMin == String(storage.sch[i]).substring(5, 7).toInt()) {
                  //Serial3.println(tSec);
                  // Serial3.print(hrs24);
                  // Serial3.print(" = ");
                  // Serial3.print(String(storage.sch[i]).substring(3, 5).toInt());
                  // Serial3.print("  -  ");
                  // Serial3.print(tMin);
                  // Serial3.print(" = ");
                  // Serial3.print(String(storage.sch[i]).substring(5, 7).toInt());
                  if (elst == 0) {
                    a1et = 21;
                    m1On();
                  }
                }
              } else {
                if (hrs24 == String(storage.sch[i]).substring(7, 9).toInt() && tMin == String(storage.sch[i]).substring(9, 11).toInt()) {
                  //Serial3.println(tSec);
                  // Serial3.print(hrs24);
                  // Serial3.print(" = ");
                  // Serial3.print(String(storage.sch[i]).substring(7, 9).toInt());
                  // Serial3.print("  -  ");
                  // Serial3.print(tMin);
                  // Serial3.print(" = ");
                  // Serial3.print(String(storage.sch[i]).substring(9, 11).toInt());
                  a1et = 22;
                  m1Off();

                  delay(1000);
                }
              }
            }
          }
          //Serial3.println("x");
        }


        // int schEnabled = String(storage.sch[i][1]).toInt(); // 0 or 1
        // int schDevId = String(storage.sch[i][2]).toInt(); // 1 or 2
        // int schOnHrs = String(storage.sch[i]).substring(3, 5).toInt(); //HH
        // int schOnMin = String(storage.sch[i]).substring(5, 7).toInt(); //MM
        // int schOfHrs = String(storage.sch[i]).substring(7, 9).toInt(); //HH
        // int schOfMin = String(storage.sch[i]).substring(9, 11).toInt(); // MM
        // bool sun = String(storage.sch[i][11]).toInt(); // 0 or 1
        // bool mon = String(storage.sch[i][12]).toInt(); // 0 or 1
        // bool tue = String(storage.sch[i][13]).toInt(); // 0 or 1
        // bool wed = String(storage.sch[i][14]).toInt(); // 0 or 1
        // bool thu = String(storage.sch[i][15]).toInt(); // 0 or 1
        // bool fri = String(storage.sch[i][16]).toInt(); // 0 or 1
        // bool sat = String(storage.sch[i][17]).toInt(); // 0 or 1

        // Serial3.println("----- Schedule Debug Info -----");
        // Serial3.print("Raw: "); Serial3.println(storage.sch[i]);
        // Serial3.print("Device ID: "); Serial3.println(schDevId);
        // Serial3.print("ON Time: ");
        // Serial3.print(schOnHrs); Serial3.print(":"); Serial3.println(schOnMin);
        // Serial3.print("OFF Time: ");
        // Serial3.print(schOfHrs); Serial3.print(":"); Serial3.println(schOfMin);

        // Serial3.print("Sun: "); Serial3.println(sun);
        // Serial3.print("Mon: "); Serial3.println(mon);
        // Serial3.print("Tue: "); Serial3.println(tue);
        // Serial3.print("Wed: "); Serial3.println(wed);
        // Serial3.print("Thu: "); Serial3.println(thu);
        // Serial3.print("Fri: "); Serial3.println(fri);
        // Serial3.print("Sat: "); Serial3.println(sat);
        // Serial3.println("-------------------------------");
      }

      break;
    ////////////////////////
    case 5:
      ////////////////////////
      // last state remembers
      // bool laStaRemM1Trigd = 0;
      // unsigned long laStaRemM1BuffMillis = 0;
      // unsigned long laStaRemSec = 0;
      // unsigned long laStaRemMins = 0;
      // unsigned long laStaRemM1TrigdMillis = 0;


      // uncommment below to add rememberence on last state with starter

      // if (storage.app1Run && laStaRemM1Trigd) {
      //   if (!m1StaVars) {
      //     storage.app1Run = 0;
      //     savecon();
      //     Serial3.println("M1-lastRemeber starter off manual");
      //     delay(1000);
      //     laStaAppStaSavShow = 1;
      //   }
      // }
      // if (!storage.app1Run && millis() > 10000) {
      //   if (m1StaVars) {
      //     storage.app1Run = 1;
      //     laStaRemM1Trigd = 1;
      //     savecon();
      //     Serial3.println("M1-lastRemeber starter on manual saved");
      //     delay(1000);
      //     laStaAppStaSavShow = 1;
      //   }
      // }

      if (storage.app1Run) {
        YellowLedTickerFunc();
      } else {
        tickYelLedVars = 0;
      }


      if (!laStaRemM1Trigd && elst == 0 && storage.app1Run && !m1StaVars && !laStaRemM1remTrigrd) {
        tickYelLedVars = 1;
        if (millis() >= ((laStaRemMins + laStaRemSec) * 1000) + laStaRemM1BuffMillis) {
          m1On();
          a1et = 31;
          laStaRemM1TrigdMillis = millis();
          Serial3.println("M1-lastRemeber trigerring");
          laStaRemM1Trigd = 1;
          tickYelLedVars = 0;
          digitalWrite(PIN_LED_YELLOW, LOW);
        }
      }
      if (elst > 0) {
        laStaRemM1BuffMillis = millis();
        laStaRemM1Trigd = 0;
      }
      //below code is to trigger if the starter is stopped
      if (laStaRemM1Trigd && storage.app1Run && !m1StaVars && (millis() - laStaRemM1TrigdMillis >= 3000)) {
        a1et = 31;
        m1On();
        Serial3.println("M1-on after 3 sec");
        laStaRemM1TrigdMillis = millis();
      }
      break;
  }
}