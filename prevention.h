bool overloadVars = 0;
bool dryRunVars = 0;

unsigned long OLDetCounter = 0;
unsigned long DRDetCounter = 0;
unsigned long UnVolCounter = 0;
unsigned long OvVolCounter = 0;

bool powerLoss = 0;
bool phaseFault = 0;

bool unVolVars = 0;
bool hiVolVars = 0;
//////////////////////////////////////

//////////////////////////////////////
void overLoadCheck() {
  if (m1StaVars && (millis() - m1OnBuffMillis >= 1000)) {
    if ((curR >= storage.ovLRunM1) || (curY >= storage.ovLRunM1) || (curBM1 >= storage.ovLRunM1)) {
      overloadVars = 1;
      //Serial3.println("OverLoad M1");
      OLDetCounter += 1;
      //Serial3.println(OLDetCounter);
      if (OLDetCounter >= CURRENT_TH_DELAY_MILLIS) {
        m1Off();
        //iotSerial.println("<{\"TS\":{\"n\":\"29\"}}>");
        a1et = 29;
      }
    }
  } else {
    overloadVars = 0;
    OLDetCounter = 0;
  }
}


void dryRunCheck() {
  if (m1StaVars && millis() - m1OnBuffMillis >= 1000) {
    if ((curR <= storage.dryRunM1) || (curY <= storage.dryRunM1) || (curBM1 <= storage.dryRunM1)) {
      dryRunVars = 1;
      //Serial3.println("DryRun M1");
      DRDetCounter += 1;
      //Serial3.println(DRDetCounter);
      if (DRDetCounter >= CURRENT_TH_DELAY_MILLIS) {
        m1Off();
        a1et = 28;
        //iotSerial.println("<{\"TS\":{\"n\":\"28\"}}>");
      }
    }
  } else {
    dryRunVars = 0;
    DRDetCounter = 0;
  }
}


void underVoltageCheck() {
  //  if ((volRY <= storage.undVol) || (volYB <= storage.undVol) || (volBR <= storage.undVol)) {
  if (volRY <= storage.undVol | volYB <= storage.undVol | volBR <= storage.undVol) {
    UnVolCounter += 1;
    if (UnVolCounter >= VOLTAGE_TH_DELAY_MILLIS) {
      unVolVars = 1;
      elst = 3;
      //Serial3.println("underVoltage");
    }
  } else {
    unVolVars = 0;
    UnVolCounter = 0;
  }
}


void highVoltageCheck() {
  //  if ((volRY >= storage.ovrVol) || (volYB >= storage.ovrVol) || (volBR >= storage.ovrVol)) {
  if (volRY >= storage.ovrVol | volYB >= storage.ovrVol | volBR >= storage.ovrVol) {
    OvVolCounter += 1;
    if (OvVolCounter >= VOLTAGE_TH_DELAY_MILLIS) {
      hiVolVars = 1;
      elst = 4;
      //Serial3.println("highVoltage");
    }
  } else {
    hiVolVars = 0;
    OvVolCounter = 0;
  }
}


void phaseChecks() {
  if (volRY <= PHASE_VOLAGE_MINIMUM | volYB <= PHASE_VOLAGE_MINIMUM | volBR <= PHASE_VOLAGE_MINIMUM) {
    phaseFault = 1;
    elst = 2;
  } else {
    phaseFault = 0;
  }

  if (volRY <= PHASE_VOLAGE_MINIMUM & volYB <= PHASE_VOLAGE_MINIMUM & volBR <= PHASE_VOLAGE_MINIMUM) {
    powerLoss = 1;
    phaseFault = 0;
    elst = 1;
  } else {
    powerLoss = 0;
  }
}



void PowChecks() {
  if (millis() > 5000) {
    elst = 0;
    underVoltageCheck();
    highVoltageCheck();
    phaseChecks();
    if (elst > 0) {
      tickRedLedVars = 1;
      digitalWrite(PIN_M1OFF, HIGH);
    } else {
      tickRedLedVars = 0;
      digitalWrite(PIN_LED_RED, HIGH);
      digitalWrite(PIN_M1OFF, LOW);
    }
  }
}
