//char hwSerialKey[21]; // for encryption purpose
/////////////////////////////////////////////////////////////
void getDeviceSerId() {
  getChipSerial(hwSerialKey, sizeof(hwSerialKey));
  DEBUG_PRINT("hwSerialKey: ");
  DEBUG_PRINTN(hwSerialKey);
}
/////////////////////////////////////////////////////////////