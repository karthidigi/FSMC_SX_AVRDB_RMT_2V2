
bool autoM1Trigd = 0;
unsigned long autoM1BuffMillis = 0;
unsigned long atSec = 0;
unsigned long atMins = 0;
unsigned long atLastTrigdMillis = 0;

unsigned long localModeChangeGuardUntil = 0;

static inline void markLocalModeChange() {
  // 30 s guard: long enough for the starter to push a1ops to the cloud via
  // iotSerial and for the cloud to acknowledge before any stale shared-attribute
  // push can override the locally-set mode.
  localModeChangeGuardUntil = millis() + 30000UL;
}

static inline bool isLocalModeChangeGuardActive() {
  return millis() < localModeChangeGuardUntil;
}

bool countM1Trigd = 0;
unsigned long countM1BuffMills = 0;
bool countM1Init = 0;               // set on first case-3 entry; cleared on expiry
unsigned long countM1LastTrigMs = 0; // retry timer for motor start in countdown mode


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

  countM1Trigd     = 0;
  countM1Init      = 0;
  countM1LastTrigMs = 0;
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
  cyclicM1OnDurMillis = (cyOnHrs + cyOnMins) * 1000UL + millis();
  cyclicM1OffDurMillis = (cyOffHrs + cyOffMins) * 1000 + millis();

  laStaRemM1Trigd = 0;
  laStaRemM1remTrigrd = 0;   // clear remote-trigger block so mode re-enable always allows auto-start
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
  // ── Yellow LED control ───────────────────────────────────────────────────────
  // Normal mode        → OFF
  // Non-normal, timer  → BLINK  (restart delay / cyclic-OFF waiting)
  // Non-normal, motor ON or idle → solid ON
  if (storage.modeM1 == 0) {
    tickYelLedVars = 0;
    digitalWrite(PIN_LED_YELLOW, HIGH);   // OFF
  } else {
    bool blink = false;
    if (storage.modeM1 == 1) {
      // Auto: blink while waiting for the auto-start countdown (pre-trigger, no fault)
      blink = (!autoM1Trigd && elst == 0);
    } else if (storage.modeM1 == 2) {
      // Cyclic: blink during the OFF phase (motor stopped, timer running)
      blink = (cyclicM1State == 2);
    } else if (storage.modeM1 == 5) {
      // Last State: blink while delay timer runs before motor restarts
      blink = (storage.app1Run && !m1StaVars && !laStaRemM1Trigd);
    }
    if (blink) {
      tickYelLedVars = 1;
      YellowLedTickerFunc();
    } else {
      tickYelLedVars = 0;
      digitalWrite(PIN_LED_YELLOW, LOW);  // solid ON
    }
  }

  switch (storage.modeM1) {
    ////////////////////////
    // Normal
    case 0:
      //Serial3.println("M1-NormalMode");
      break;

    ////////////////////////
    // Auto
    case 1:
      // Countdown: only count when no fault present and trigger hasn't fired yet.
      if (!autoM1Trigd && elst == 0) {
        if (millis() >= ((atMins + atSec) * 1000UL) + autoM1BuffMillis) {
          m1On();
          a1et = 13;
          atLastTrigdMillis = millis();
          Serial3.println("M1-Auto");
          autoM1Trigd = 1;
        }
      }

      // Fault handling: on ANY fault (power loss, under/over voltage, phase cut),
      // reset autoM1Trigd and restart the delay timer regardless of whether the
      // motor had already started.  This ensures the full configured ON delay runs
      // again after every fault recovery — both pre- and post-trigger.
      // PowChecks() independently calls m1Off() to stop the motor while elst > 0.
      if (elst > 0) {
        autoM1BuffMillis = millis();
        autoM1Trigd      = 0;   // full delay must run again on recovery
      }

      // Retry: delay fired and m1On() was called but motor not yet confirmed running
      // (e.g. contactor slow to pull in) — retry every 3 s until motor starts.
      // Guard with !m1OffActive so we don't re-trigger during the OFF relay latch.
      if (autoM1Trigd && !m1StaVars && !m1OffActive
          && (millis() - atLastTrigdMillis >= 3000)) {
        a1et = 13;
        m1On();
        atLastTrigdMillis = millis();
      }

      break;


    ////////////////////////
    // Cyclic
    case 2:
      // Guard: both ON and OFF durations must be non-zero.
      // Default storage "z00000000" gives 0 ms — without this guard both timers
      // expire at millis() causing Init→OFF→ON→OFF… every loop() pass.
      if ((cyOnHrs + cyOnMins) == 0 || (cyOffHrs + cyOffMins) == 0) {
        break;
      }

      // Fault active: hold off any m1On() call to prevent relay chatter.
      // modeM1() runs before PowChecks(); without this guard the init block would
      // call m1On() on the same pass that PowChecks() calls m1Off().
      if (elst > 0) {
        cyclicM1Init  = 0;
        cyclicM1State = 0;
        break;
      }

      // One-time init: start motor immediately (spec: "Motor must start immediately after configuration")
      if (!cyclicM1Init) {
        a1et = 16;
        m1On();
        cyclicM1Init = 1;
        cyclicM1State = 1;
        cyclicM1OnDurMillis = ((cyOnHrs + cyOnMins) * 1000UL) + millis();
        Serial3.println("M1-Cyclic-Init");
      }

      if (cyclicM1State == 1) {
        if (millis() >= cyclicM1OnDurMillis) {
          a1et = 18;
          m1Off();
          Serial3.println("M1-Cyclic-OFF");
          cyclicM1OffDurMillis = ((cyOffHrs + cyOffMins) * 1000UL) + millis();
          cyclicM1State = 2;
        }
      } else if (cyclicM1State == 2) {
        // else-if prevents state-2 from firing on the same pass as state-1:
        // a just-expired state-1 sets cyclicM1OffDurMillis = millis() and the
        // bare-if below would fire immediately in the same modeM1() call.
        if (millis() >= cyclicM1OffDurMillis) {
          a1et = 16;
          m1On();
          Serial3.println("M1-Cyclic-ON");
          cyclicM1OnDurMillis = ((cyOnHrs + cyOnMins) * 1000UL) + millis();
          cyclicM1State = 1;
        }
      }

      break;
    ////////////////////////
    // Countdown
    case 3:
      // One-time init: start motor and begin countdown timer immediately
      if (!countM1Init) {
        a1et = 20;
        m1On();
        countM1BuffMills   = millis();
        countM1Trigd       = 1;
        countM1Init        = 1;
        countM1LastTrigMs  = millis();
        Serial3.println("M1-CountDown-Start");
      }

      // Retry: timer running but motor not yet detected — same 3-second retry as AUTO mode
      if (countM1Trigd && !m1StaVars && !m1OffActive
          && (millis() - countM1LastTrigMs >= 3000)) {
        a1et = 20;
        m1On();
        countM1LastTrigMs = millis();
      }

      // Timer expiry: stop motor, return to Normal mode
      if (countM1Trigd && millis() >= ((cTHrs + cTMins) * 1000UL) + countM1BuffMills) {
        a1et = 20;
        m1Off();
        Serial3.println("M1-CountDown-End");
        countM1Trigd   = 0;
        countM1Init    = 0;
        storage.modeM1 = 0;   // spec: "SystemMode = NORMAL_MODE"
        savecon();
      }

      break;
    ////////////////////////
    // Scheduler
    case 4:
      // Per-slot last-triggered minute trackers — prevent re-firing within the same
      // minute without a blocking delay.  Initialised to 255 (impossible tMin value).
      static uint8_t schOnTrigMin[10]  = {255,255,255,255,255,255,255,255,255,255};
      static uint8_t schOffTrigMin[10] = {255,255,255,255,255,255,255,255,255,255};

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
                  // Trigger ON once per minute per slot (no blocking delay needed).
                  if (elst == 0 && schOnTrigMin[i] != tMin) {
                    schOnTrigMin[i] = tMin;
                    a1et = 21;
                    m1On();
                  }
                }
              } else {
                if (hrs24 == String(storage.sch[i]).substring(7, 9).toInt() && tMin == String(storage.sch[i]).substring(9, 11).toInt()) {
                  // Trigger OFF once per minute per slot.
                  // Replaced delay(1000) — per-slot minute tracking prevents re-trigger
                  // without blocking the main loop or stalling button detection.
                  if (schOffTrigMin[i] != tMin) {
                    schOffTrigMin[i] = tMin;
                    a1et = 22;
                    m1Off();
                  }
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
      // Refresh the local-mode-change guard on every loop while in Last State mode.
      // Without this, ThingsBoard re-pushes its cached a1ops=0 shared attribute after
      // the 30 s guard expires (on reconnect or periodic sync), silently resetting
      // Last State back to Normal. Continuously refreshing the guard blocks that path.
      // The guard only expires naturally once the mode is changed away from 5.
      markLocalModeChange();

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

      if (!laStaRemM1Trigd && elst == 0 && storage.app1Run && !m1StaVars && !laStaRemM1remTrigrd) {
        if (millis() >= ((laStaRemMins + laStaRemSec) * 1000) + laStaRemM1BuffMillis) {
          m1On();
          a1et = 31;
          laStaRemM1TrigdMillis = millis();
          Serial3.println("M1-lastRemeber trigerring");
          laStaRemM1Trigd = 1;
        }
      }
      if (elst > 0) {
        laStaRemM1BuffMillis  = millis();
        laStaRemM1Trigd       = 0;
        laStaRemM1remTrigrd   = 0;   // clear remote-trigger block so fault recovery can restart
      }
      //below code is to trigger if the starter is stopped
      if (laStaRemM1Trigd && storage.app1Run && !m1StaVars && !m1OffActive && (millis() - laStaRemM1TrigdMillis >= 3000)) {
        a1et = 31;
        m1On();
        Serial3.println("M1-on after 3 sec");
        laStaRemM1TrigdMillis = millis();
      }
      break;
  }
}

