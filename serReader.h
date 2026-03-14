#include <ArduinoJson.h>
String hwSerStr = "", hwSerCharFirst = "", hwSerCharLast = "";
int lcdInt = 0;
String lcdString = "";
void readIotSerial() {
  if (iotSerial.available() > 0) {
    hwSerStr = iotSerial.readString();
    //delay(5);
  }



  if (hwSerStr != "") {

    ////
    ////<{"info":{"ssid":"iamwhatiam"}}>
    ////<{"info":{"conWifi":"start"}}>
    ////
    //Serial3.println("all [" + hwSerStr + "]");

    int start = hwSerStr.indexOf("<");
    int end = hwSerStr.indexOf(">", start + 1);

    if (start != -1 && end != -1 && start < end) {

      hwSerStr.trim();
      //Serial3.println("Cond1[" + hwSerStr + "]");
      ////Cond1[<{"info":{"ssid":"iamwhatiam"}}>
      ////<{"info":{"conWifi":"start"}}>]

      hwSerStr = hwSerStr.substring(hwSerStr.indexOf("<") + 1, hwSerStr.indexOf(">"));
      //Serial3.println("Cond2[" + hwSerStr + "]");
      ////Cond2[{"info":{"ssid":"iamwhatiam"}}]

      hwSerCharFirst = hwSerStr.charAt(0);
      hwSerCharLast = hwSerStr.substring(hwSerStr.lastIndexOf("}"));
      //Serial3.println(hwSerCharFirst);
      //Serial3.println(hwSerCharLast);

      if (hwSerCharFirst == "{" and hwSerCharLast == "}") {

        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, hwSerStr);

        if (error) {
          DEBUG_PRINTN("JSON parse error: ");
          DEBUG_PRINTN(error.c_str());
        } else {
          // Extract only the object inside "TS"

          // iotSerial.println("<" + hwSerStr + ">");

          Serial3.println(hwSerStr);

          // if (doc.containsKey("datetime")) {
          //   lcdString = doc["datetime"].as<String>();

          //   // Example: "2025-08-16T17:37:51"
          //   String date = lcdString.substring(0, 10);   // "2025-08-16"
          //   String time = lcdString.substring(11, 19);  // "17:37:51"
          //   lcd_set_cursor(0, 0);
          //   //lc_print("1234567890123456");
          //   lcd_print("DATE SYNC       ");
          //   lcd_set_cursor(1, 0);
          //   lcd_print(date.c_str());

          //   lcd_set_cursor(1, 11);  // right side
          //   lcd_print(time.c_str());
          // }


          if (doc["epoch"].is<uint32_t>()) {
            uint32_t epoch = doc["epoch"].as<uint32_t>();
            lcdString = doc["datetime"].as<String>();
            lcd_set_cursor(0, 0);
            lcd_print("    Time Sync     ");
            lcd_set_cursor(1, 0);  // right side
            lcd_print(lcdString.c_str());
            ds3231_setEpochIST(epoch - IST_OFFSET_SECS);
          }



          if (doc["info"].is<JsonObject>()) {
            JsonVariant infoObj = doc["info"];
            if (infoObj["rssi"].is<int>()) {
              lcdInt = infoObj["rssi"].as<int>();
              lcd_set_cursor(0, 0);
              lcd_print("                ");
              lcd_set_cursor(0, 0);
              lcd_print("   WiFi Signal  ");
              lcd_set_cursor(1, 0);
              char buf[8];  // enough for RSSI integer
              sprintf(buf, "        %d\%         ", lcdInt);
              lcd_print(buf);
            }


            if (infoObj["conWifi"].is<const char*>()) {
              lcdString = infoObj["conWifi"].as<String>();
              uiFunc(1);
              lcd_set_cursor(0, 0);
              lcd_print("                  ");
              lcd_set_cursor(0, 0);
              lcd_print("   Connecting   ");
              lcd_set_cursor(1, 0);
              lcd_print("      WiFi      ");
            }
            if (infoObj["ssid"].is<const char*>()) {
              lcdString = infoObj["ssid"].as<String>();
              uiFunc(1);
              lcd_set_cursor(0, 0);
              //c_print("1234567890123456");
              lcd_print("    WiFi SSID   ");
              lcd_set_cursor(1, 0);
              lcd_print("                ");
              lcd_set_cursor(1, (16 - lcdString.length()) / 2);
              lcd_print(lcdString.c_str());
            }
            if (infoObj["conCloud"].is<const char*>()) {
              lcdString = infoObj["conCloud"].as<String>();
              uiFunc(1);
              lcd_set_cursor(0, 0);
              lcd_print("                ");
              lcd_set_cursor(0, 0);
              //c_print("1234567890123456");
              lcd_print("   Connecting   ");
              lcd_set_cursor(1, 0);
              lcd_print("     cloud      ");
            }
            if (!infoObj["otaDone"].isNull()) {
              uiFunc(1);
              lcd_set_cursor(0, 0);
              //c_print("1234567890123456");
              lcd_print("      OTA       ");
              lcd_set_cursor(1, 0);
              lcd_print("    SUCCESS     ");
            }
          }

          if (doc["method"].is<const char*>() && doc["method"].as<String>() == "checkStatus") {
            iotDataSendVol(1);
          }
          if (!doc["com"].isNull()) {
            iotDataSendVol(1);
            InitAck = 1;
          }
          ///////////////////////////////////////////////
          if (doc["shared"].is<JsonObject>()) {
            JsonObject shared = doc["shared"];
            //loadcon();
            // nested checks inside "shared"


            if (shared["name"].is<const char*>()) {
              const char* name = shared["name"];  // get as C string
              strncpy(storage.dname, name, sizeof(storage.dname) - 1);
              storage.dname[sizeof(storage.dname) - 1] = '\0';  // ensure null termination
              uiFunc(1);
              lcd_nameShow(1);
            }

            if (!shared["a1ops"].isNull()) {
              int rawMode = 0;
              if (shared["a1ops"].is<const char*>()) {
                rawMode = String(shared["a1ops"].as<const char*>()).toInt();
              } else {
                rawMode = shared["a1ops"].as<int>();
              }

              // Accept normal 0-5 mode encoding, and map legacy 6 -> 5.
              if (rawMode == 6) {
                rawMode = 5;
              }

              if (rawMode >= 0 && rawMode <= 5) {
                uint8_t nextMode = (uint8_t)rawMode;
                if (isLocalModeChangeGuardActive() && nextMode != storage.modeM1) {
                  Serial3.print("Ignoring stale a1ops during local guard: ");
                  Serial3.println(rawMode);
                } else if (nextMode != storage.modeM1) {
                  storage.modeM1 = nextMode;
                  Serial3.print("a1ops: ");
                  Serial3.println(storage.modeM1);
                  savecon();
                  loadModeVal();
                  uiFunc(1);
                  lcd_modeShow();
                  iotSerial.println("<{\"TS\":{\"n\":" + String(storage.modeM1 + 1) + "}}>");
                  Serial3.println("<{\"TS\":{\"n\":" + String(storage.modeM1 + 1) + "}}>");
                  delay(500);
                } else {
                  Serial3.print("a1ops unchanged: ");
                  Serial3.println(rawMode);
                }
              } else {
                Serial3.print("Ignoring invalid a1ops: ");
                Serial3.println(rawMode);
              }
            }

            if (shared["a1auDly"].is<const char*>()) {
              const char* val = shared["a1auDly"];  // get as const char*
              strncpy(storage.autoM1, val, sizeof(storage.autoM1) - 1);
              storage.autoM1[sizeof(storage.autoM1) - 1] = '\0';  // ensure null-termination
              Serial3.print("a1auDly: ");
              Serial3.println(storage.autoM1);

              savecon();
              loadModeVal();
              m1Off();
              uiFunc(1);
              lcd_modeShow();
            }

            if (shared["a1lasRem"].is<const char*>()) {
              const char* val = shared["a1lasRem"];  // get as const char*
              strncpy(storage.lasRemM1, val, sizeof(storage.lasRemM1) - 1);
              storage.lasRemM1[sizeof(storage.lasRemM1) - 1] = '\0';  // ensure null-termination
              Serial3.print("a1lasRem: ");
              Serial3.println(storage.lasRemM1);

              savecon();
              loadModeVal();
              m1Off();
              uiFunc(1);
              lcd_modeShow();
            }

            if (shared["a1cyc"].is<const char*>()) {
              const char* val = shared["a1cyc"];  // get as const char*
              strncpy(storage.cyclicM1, val, sizeof(storage.cyclicM1) - 1);
              storage.autoM1[sizeof(storage.autoM1) - 1] = '\0';  // ensure null-termination
              Serial3.print("a1cyc: ");
              Serial3.println(storage.cyclicM1);

              savecon();
              loadModeVal();
              m1Off();
              uiFunc(1);
              lcd_modeShow();
            }

            if (shared["a1cTmr"].is<const char*>()) {
              const char* val = shared["a1cTmr"];  // get as const char*
              strncpy(storage.countM1, val, sizeof(storage.countM1) - 1);
              storage.autoM1[sizeof(storage.countM1) - 1] = '\0';  // ensure null-termination
              Serial3.print("a1cTmr: ");
              Serial3.println(storage.countM1);

              savecon();
              loadModeVal();
              m1On();
              uiFunc(1);
              lcd_modeShow();
            }

            for (int i = 0; i < 10; i++) {
              char key[3];
              sprintf(key, "s%d", i);  // makes "s0", "s1", ...

              if (!shared[key].isNull()) {
                strncpy(storage.sch[i], shared[key], sizeof(storage.sch[i]) - 1);
                storage.sch[i][sizeof(storage.sch[i]) - 1] = '\0';

                Serial3.print("storage.sch[");
                Serial3.print(i);
                Serial3.print("]: ");
                Serial3.println(storage.sch[i]);

                savecon();
                loadModeVal();
              }
            }

            if (shared["hvTh"].is<const char*>()) {
              String val = shared["hvTh"].as<String>();
              val.remove(0, 1);  // remove leading 'z'
              storage.ovrVol = (uint16_t)val.toInt();
              Serial3.print("hvTh -> storage.ovrVol: ");
              Serial3.println(storage.ovrVol);
              savecon();
              loadModeVal();
            }

            if (shared["lvTh"].is<const char*>()) {
              String val = shared["lvTh"].as<String>();
              val.remove(0, 1);
              storage.undVol = (uint16_t)val.toInt();
              Serial3.print("lvTh -> storage.undVol: ");
              Serial3.println(storage.undVol);
              savecon();
              loadModeVal();
            }

            if (shared["a1dry"].is<const char*>()) {
              String val = shared["a1dry"].as<String>();
              val.remove(0, 1);
              storage.dryRunM1 = val.toFloat();
              Serial3.print("a1dry -> storage.dryRunM1: ");
              Serial3.println(storage.dryRunM1, 1);
              savecon();
              loadModeVal();
            }

            if (shared["a1ovl"].is<const char*>()) {
              String val = shared["a1ovl"].as<String>();
              val.remove(0, 1);
              storage.ovLRunM1 = val.toFloat();
              Serial3.print("a1ovl -> storage.ovLRunM1: ");
              Serial3.println(storage.ovLRunM1, 1);
              savecon();
              loadModeVal();
            }

            ///////////////////////////////////////////
          }
          /////////////////////////////////////////////////
          if (doc["method"].is<const char*>() && doc["params"].is<bool>()) {
            String method = doc["method"].as<String>();
            bool params = doc["params"].as<bool>();

            Serial3.print("Method: ");
            Serial3.println(method);
            Serial3.print("Params: ");
            Serial3.println(params);

            if (method == "otaBegin" && params) {
              lcd_set_cursor(0, 0);
              lcd_print("       OTA      ");
              lcd_set_cursor(1, 0);
              lcd_print("    STARTING    ");
            } else if (method == "a1On" && params) {
              // ── App ON block: bypass switch or active fault ─────────────
              if (bypassActive) {
                remBlockReason = 1;
                a1et = 40;
                lcd_set_cursor(0, 0); lcd_print("  APP  : BLOCK  ");
                lcd_set_cursor(1, 0); lcd_print(" BYPASS  ACTIVE ");
                uiFunc(1);
                iotSerial.println("<{\"TS\":{\"n\":\"40\"}}>");
              } else if (elst > 0) {
                remBlockReason = 2;
                a1et = 41;
                lcd_set_cursor(0, 0); lcd_print("  APP  : BLOCK  ");
                lcd_set_cursor(1, 0); lcd_print(" VOLTAGE  FAULT ");
                uiFunc(1);
                iotSerial.println("<{\"TS\":{\"n\":\"41\"}}>");
              } else {
                // ── Normal app ON ───────────────────────────────────────
                remBlockReason = 0;
                lcd_set_cursor(0, 0);
                lcd_print(" MOTOR  CONTROL ");
                lcd_set_cursor(1, 0);
                lcd_print("  APP :   ON    ");
                m1On();
                a1et = 10;

                if (storage.modeM1 != 0) {  // fixed logic
                  storage.modeM1 = 0;
                  Serial3.println("mode normal app : on");
                  savecon();
                }
              }
            } else if (method == "a1On" && !params) {
              lcd_set_cursor(0, 0);
              lcd_print(" MOTOR  CONTROL ");
              lcd_set_cursor(1, 0);
              lcd_print("  APP :   OFF   ");
              m1Off();
              a1et = 11;

              if (storage.modeM1 != 0) {  // fixed logic
                storage.modeM1 = 0;
                Serial3.println("mode normal app : off");
                savecon();
              }
            } else if (method == "a2On" && params) {
              lcd_set_cursor(0, 0);
              lcd_print(" SWITCH CONTROL ");
              lcd_set_cursor(1, 0);
              lcd_print("  APP :   ON    ");
              a1et = 34;
              digitalWrite(PIN_LiREL, HIGH);
              iotDataSendVol(1);
              wdt_reset();
              delay(1000);
              if (storage.app2Run != 1) {  // fixed logic
                storage.app2Run = 1;
                Serial3.println("SWITCH CONTROL - app : on");
                savecon();
              }
            } else if (method == "a2On" && !params) {
              lcd_set_cursor(0, 0);
              lcd_print(" SWITCH CONTROL ");
              lcd_set_cursor(1, 0);
              lcd_print("  APP :   OFF   ");
              a1et = 35;
              digitalWrite(PIN_LiREL, LOW);
              iotDataSendVol(1);
              wdt_reset();
              delay(1000);
              if (storage.app2Run != 0) {  // fixed logic
                storage.app2Run = 0;
                Serial3.println("SWITCH CONTROL - app : off");
                savecon();
              }
            }
          }
        }
      }
    }
    hwSerStr = "";
  }
}

void readHWSerial() {
  if (debugSerial.available() > 0) {
    Serial3.println(debugSerial.readString());
    //delay(5);
  }
}



// {"datetime":"2025-08-12T12:39:10.041040+00:00","epoch":1755002350}

// {"info":{"ssid":"iamwhatiam"}}
// {"info":{"conWifi":"start"}}
// {"info":{"conCloud":"success"}}
// {"info":{"rssi":65}}
// {"datetime":"2025-08-16T17:10:35"}

// {"shared":{"name":"karthik wroom"}}
// {"shared":{"a1cTmr":"z0010","a1cyc":"z12012600001200560000","a1auDly":"z6006060006","a1ops":"4"}}
// {"shared":{"hvTh":"500","a1dry":"001.1","a1ovl":"010.1","lvTh":"350"}}
// {"shared":{"s0":"z11000001001111100","s1":"z11000001000000011"}}
// {"shared":{"s3":"z11040005000000111","s2":"z11020003001111000"}}
// {"shared":{"s4":"z11050006000000011","s5":"z11070008000000110"}}
// {"shared":{"s6":"z11080009000000100","s7":"z11100011000000011"}}
// {"shared":{"s8":"z11130014000000010","s9":"z11140015000000001"}}

//{"method":"a1On","params":true}
//{“method”:”a1On”,”params”:true}
//{“method”:”a1On”,”params”:true}




// 04:53:03.761 -> {"shared":{"a1ops":"0"}}
// 04:53:06.748 -> {"shared":{"a1auDly":"z0006","a1ops":"1"}}
// 04:53:16.258 -> {"shared":{"a1cyc":"z01000100","a1ops":"3"}}
// 04:53:38.600 -> {"shared":{"a1cTmr":"z2606","a1ops":"2"}}
// 04:53:47.899 -> {"shared":{"a1ops":"4"}}

// a1dry,a1ovl,a1ops,a1auDly,a1cTmr,a1cyc
//s0


//{"AT":{"a1dry":storage.dryRunM1}}
//{"AT":{"a1ovl":storage.ovLRunM1}}

//{"AT":{"a1dry":storage.dryRunM1}}
//{"AT":{"a1ovl":storage.ovLRunM1}}
//{"AT":{"a1ops":storage.modeM1}}
//{"AT":{"a1auDly":storage.autoM1}}
//{"AT":{"a1cTmr":storage.countM1}}
//{"AT":{"a1cyc":storage.cyclicM1}}

//{"TS":{"volr":200,"voly":201,"volb":301,"elst":0}}
//{"TS":{"a1st":1,"cury":15,"curb":5,"curr":5,"dlog":10}}
//{"TS":{"a1st":0,"cury":0,"curb":0,"curr":0,"dlog":11}}
