// ========== TWI1 (I2C) Low-Level Functions ==========
void i2c1Init() {
  TWI1.MBAUD = (F_CPU / (2 * 100000UL)) - 5;
  TWI1.MCTRLA = TWI_ENABLE_bm;
  TWI1.MSTATUS = TWI_BUSSTATE_IDLE_gc;
}

uint8_t i2c1_start(uint8_t addr, bool read) {
  TWI1.MADDR = (addr << 1) | (read ? 1 : 0);
  while (!(TWI1.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm)));
  if (TWI1.MSTATUS & TWI_RXACK_bm) {
    TWI1.MCTRLB = TWI_MCMD_STOP_gc;
    return 0;  // NACK
  }
  return 1;    // ACK
}

void i2c1_write(uint8_t data) {
  TWI1.MDATA = data;
  while (!(TWI1.MSTATUS & TWI_WIF_bm));
}

uint8_t i2c1_read(bool ack) {
  while (!(TWI1.MSTATUS & TWI_RIF_bm));
  uint8_t val = TWI1.MDATA;
  if (ack)
    TWI1.MCTRLB = TWI_ACKACT_ACK_gc | TWI_MCMD_RECVTRANS_gc;
  else
    TWI1.MCTRLB = TWI_ACKACT_NACK_gc | TWI_MCMD_STOP_gc;
  return val;
}

void i2c1_stop() {
  TWI1.MCTRLB = TWI_MCMD_STOP_gc;
}
