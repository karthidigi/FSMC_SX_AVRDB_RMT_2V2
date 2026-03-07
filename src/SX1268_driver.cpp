#include "SX1268_driver.h"

SPIClass* SX1268_spi = &SX1268_SPI;
uint32_t SX1268_spiFrequency = SX1268_SPI_FREQUENCY;
int8_t SX1268_nss = SX1268_PIN_NSS;
int8_t SX1268_busy = SX1268_PIN_BUSY;

void SX1268_setSPI(SPIClass &SpiObject, uint32_t frequency)
{
    SX1268_spi = &SpiObject;
    SX1268_spiFrequency = frequency ? frequency : SX1268_spiFrequency;
}

void SX1268_setPins(int8_t nss, int8_t busy)
{
    SX1268_nss = nss;
    SX1268_busy = busy;
}

void SX1268_reset(int8_t reset)
{
    pinMode(reset, OUTPUT);
    digitalWrite(reset, LOW);
    delayMicroseconds(500);
    digitalWrite(reset, HIGH);
    delayMicroseconds(100);
}

void SX1268_begin()
{
    pinMode(SX1268_nss, OUTPUT);
    pinMode(SX1268_busy, INPUT);
    SX1268_spi->begin();
}

bool SX1268_busyCheck(uint32_t timeout)
{
    uint32_t t = millis();
    while (digitalRead(SX1268_busy) == HIGH) if (millis() - t > timeout) return true;
    return false;
}

void SX1268_setSleep(uint8_t sleepConfig)
{
    SX1268_transfer(0x84, &sleepConfig, 1);
}

void SX1268_setStandby(uint8_t standbyConfig)
{
    SX1268_transfer(0x80, &standbyConfig, 1);
}

void SX1268_setFs()
{
    SX1268_transfer(0xC1, NULL, 0);
}

void SX1268_setTx(uint32_t timeout)
{
    uint8_t buf[3];
    buf[0] = timeout >> 16;
    buf[1] = timeout >> 8;
    buf[2] = timeout;
    SX1268_transfer(0x83, buf, 3);
}

void SX1268_setRx(uint32_t timeout)
{
    uint8_t buf[3];
    buf[0] = timeout >> 16;
    buf[1] = timeout >> 8;
    buf[2] = timeout;
    SX1268_transfer(0x82, buf, 3);
}

void SX1268_stopTimerOnPreamble(uint8_t enable)
{
    SX1268_transfer(0x9F, &enable, 1);
}

void SX1268_setRxDutyCycle(uint32_t rxPeriod, uint32_t sleepPeriod)
{
    uint8_t buf[6];
    buf[0] = rxPeriod >> 16;
    buf[1] = rxPeriod >> 8;
    buf[2] = rxPeriod;
    buf[3] = sleepPeriod >> 16;
    buf[4] = sleepPeriod >> 8;
    buf[5] = sleepPeriod;
    SX1268_transfer(0x94, buf, 6);
}

void SX1268_setCad()
{
    SX1268_transfer(0xC5, NULL, 0);
}

void SX1268_setTxContinuousWave()
{
    SX1268_transfer(0xD1, NULL, 0);
}

void SX1268_setTxInfinitePreamble()
{
    SX1268_transfer(0xD2, NULL, 0);
}

void SX1268_setRegulatorMode(uint8_t modeParam)
{
    SX1268_transfer(0x96, &modeParam, 1);
}

void SX1268_calibrate(uint8_t calibParam)
{
    SX1268_transfer(0x89, &calibParam, 1);
}

void SX1268_calibrateImage(uint8_t freq1, uint8_t freq2)
{
    uint8_t buf[2];
    buf[0] = freq1;
    buf[1] = freq2;
    SX1268_transfer(0x98, buf, 2);
}

void SX1268_setPaConfig(uint8_t paDutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut)
{
    uint8_t buf[4];
    buf[0] = paDutyCycle;
    buf[1] = hpMax;
    buf[2] = deviceSel;
    buf[3] = paLut;
    SX1268_transfer(0x95, buf, 4);
}

void SX1268_setRxTxFallbackMode(uint8_t fallbackMode)
{
    SX1268_transfer(0x93, &fallbackMode, 1);
}

void SX1268_writeRegister(uint16_t address, uint8_t* data, uint8_t nData)
{
    uint8_t nBuf = nData + 2;
    uint8_t buf[nBuf];
    buf[0] = address >> 8;
    buf[1] = address;
    for(uint8_t i=0; i<nData; i++) buf[i + 2] = data[i];
    SX1268_transfer(0x0D, buf, nBuf);
}

void SX1268_readRegister(uint16_t address, uint8_t* data, uint8_t nData)
{
    uint8_t nBuf = nData + 1;
    uint8_t buf[nBuf];
    uint8_t addr[2];
    addr[0] = address >> 8;
    addr[1] = address;
    SX1268_transfer(0x1D, buf, nBuf, addr, 2);
    for(uint8_t i=0; i<nData; i++) data[i] = buf[i + 1];
}

void SX1268_writeBuffer(uint8_t offset, uint8_t* data, uint8_t nData)
{
    uint8_t nBuf = nData + 1;
    uint8_t buf[nBuf];
    buf[0] = offset;
    for(uint8_t i=0; i<nData; i++) buf[i + 1] = data[i];
    SX1268_transfer(0x0E, buf, nBuf);
}

void SX1268_readBuffer(uint8_t offset, uint8_t* data, uint8_t nData)
{
    uint8_t nBuf = nData + 1;
    uint8_t buf[nBuf];
    SX1268_transfer(0x1E, buf, nBuf, &offset, 1);
    for(uint8_t i=0; i<nData; i++) data[i] = buf[i + 1];
}

void SX1268_setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask)
{
    uint8_t buf[8];
    buf[0] = irqMask >> 8;
    buf[1] = irqMask;
    buf[2] = dio1Mask >> 8;
    buf[3] = dio1Mask;
    buf[4] = dio2Mask >> 8;
    buf[5] = dio2Mask;
    buf[6] = dio3Mask >> 8;
    buf[7] = dio3Mask;
    SX1268_transfer(0x08, buf, 8);
}

void SX1268_getIrqStatus(uint16_t* irqStatus)
{
    uint8_t buf[3];
    SX1268_transfer(0x12, buf, 3);
    *irqStatus = (buf[1] << 8) | buf[2];
}

void SX1268_clearIrqStatus(uint16_t clearIrqParam)
{
    uint8_t buf[2];
    buf[0] = clearIrqParam >> 8;
    buf[1] = clearIrqParam;
    SX1268_transfer(0x02, buf, 2);
}

void SX1268_setDio2AsRfSwitchCtrl(uint8_t enable)
{
    SX1268_transfer(0x9D, &enable, 1);
}

void SX1268_setDio3AsTcxoCtrl(uint8_t tcxoVoltage, uint32_t delay)
{
    uint8_t buf[4];
    buf[0] = tcxoVoltage;
    buf[1] = delay >> 16;
    buf[2] = delay >> 8;
    buf[3] = delay;
    SX1268_transfer(0x97, buf, 4);
}

void SX1268_setRfFrequency(uint32_t rfFreq)
{
    uint8_t buf[4];
    buf[0] = rfFreq >> 24;
    buf[1] = rfFreq >> 16;
    buf[2] = rfFreq >> 8;
    buf[3] = rfFreq;
    SX1268_transfer(0x86, buf, 4);
}

void SX1268_setPacketType(uint8_t packetType)
{
    SX1268_transfer(0x8A, &packetType, 1);
}

void SX1268_getPacketType(uint8_t* packetType)
{
    uint8_t buf[2];
    SX1268_transfer(0x11, buf, 2);
    *packetType = buf[1];
}

void SX1268_setTxParams(uint8_t power, uint8_t rampTime)
{
    uint8_t buf[2];
    buf[0] = power;
    buf[1] = rampTime;
    SX1268_transfer(0x8E, buf, 2);
}

void SX1268_setModulationParamsLoRa(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t ldro)
{
    uint8_t buf[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    buf[0] = sf;
    buf[1] = bw;
    buf[2] = cr;
    buf[3] = ldro;
    SX1268_transfer(0x8B, buf, 8);
}

void SX1268_setModulationParamsFSK(uint32_t br, uint8_t pulseShape, uint8_t bandwidth, uint32_t Fdev)
{
    uint8_t buf[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    buf[0] = br >> 16;
    buf[1] = br >> 8;
    buf[2] = br;
    buf[3] = pulseShape;
    buf[4] = bandwidth;
    buf[5] = Fdev >> 16;
    buf[6] = Fdev >> 8;
    buf[7] = Fdev;
    SX1268_transfer(0x8B, buf, 8);
}

void SX1268_setPacketParamsLoRa(uint16_t preambleLength, uint8_t headerType, uint8_t payloadLength, uint8_t crcType, uint8_t invertIq)
{
    uint8_t buf[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    buf[0] = preambleLength >> 8;
    buf[1] = preambleLength;
    buf[2] = headerType;
    buf[3] = payloadLength;
    buf[4] = crcType;
    buf[5] = invertIq;
    SX1268_transfer(0x8C, buf, 9);
}

void SX1268_setPacketParamsFSK(uint16_t preambleLength, uint8_t preambleDetector, uint8_t syncWordLength, uint8_t addrComp, uint8_t packetType, uint8_t payloadLength, uint8_t crcType, uint8_t whitening)
{
    uint8_t buf[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    buf[0] = preambleLength >> 8;
    buf[1] = preambleLength;
    buf[2] = preambleDetector;
    buf[3] = syncWordLength;
    buf[4] = addrComp;
    buf[5] = packetType;
    buf[6] = payloadLength;
    buf[7] = crcType;
    buf[8] = whitening;
    SX1268_transfer(0x8C, buf, 9);
}

void SX1268_setCadParams(uint8_t cadSymbolNum, uint8_t cadDetPeak, uint8_t cadDetMin, uint8_t cadExitMode, uint32_t cadTimeout)
{
    uint8_t buf[7];
    buf[0] = cadSymbolNum;
    buf[1] = cadDetPeak;
    buf[2] = cadDetMin;
    buf[3] = cadExitMode;
    buf[4] = cadTimeout >> 16;
    buf[5] = cadTimeout >> 8;
    buf[6] = cadTimeout;
    SX1268_transfer(0x88, buf, 7);
}

void SX1268_setBufferBaseAddress(uint8_t txBaseAddress, uint8_t rxBaseAddress)
{
    uint8_t buf[2];
    buf[0] = txBaseAddress;
    buf[1] = rxBaseAddress;
    SX1268_transfer(0x8F, buf, 2);
}

void SX1268_setLoRaSymbNumTimeout(uint8_t symbnum)
{
    SX1268_transfer(0xA0, &symbnum, 1);
}

void SX1268_getStatus(uint8_t* status)
{
    uint8_t buf;
    SX1268_transfer(0xC0, &buf, 1);
    *status = buf;
}

void SX1268_getRxBufferStatus(uint8_t* payloadLengthRx, uint8_t* rxStartBufferPointer)
{
    uint8_t buf[3];
    SX1268_transfer(0x13, buf, 3);
    *payloadLengthRx = buf[1];
    *rxStartBufferPointer = buf[2];
}

void SX1268_getPacketStatus(uint8_t* rssiPkt, uint8_t* snrPkt, uint8_t* signalRssiPkt)
{
    uint8_t buf[4];
    SX1268_transfer(0x14, buf, 4);
    *rssiPkt = buf[1];
    *snrPkt = buf[2];
    *signalRssiPkt = buf[3];
}

void SX1268_getRssiInst(uint8_t* rssiInst)
{
    uint8_t buf[2];
    SX1268_transfer(0x15, buf, 2);
    *rssiInst = buf[1];
}

void SX1268_getStats(uint16_t* nbPktReceived, uint16_t* nbPktCrcError, uint16_t* nbPktHeaderErr)
{
    uint8_t buf[7];
    SX1268_transfer(0x10, buf, 7);
    *nbPktReceived  = (buf[1] >> 8) | buf[2];
    *nbPktCrcError  = (buf[3] >> 8) | buf[4];
    *nbPktHeaderErr = (buf[5] >> 8) | buf[6];
}

void SX1268_resetStats()
{
    uint8_t buf[6] = {0, 0, 0, 0, 0, 0};
    SX1268_transfer(0x00, buf, 6);
}

void SX1268_getDeviceErrors(uint16_t* opError)
{
    uint8_t buf[3];
    SX1268_transfer(0x17, buf, 3);
    *opError = buf[2];
}

void SX1268_clearDeviceErrors()
{
    uint8_t buf[2] = {0, 0};
    SX1268_transfer(0x07, buf, 2);
}

void SX1268_fixLoRaBw500(uint32_t bw)
{
    uint8_t packetType;
    SX1268_getPacketType(&packetType);
    uint8_t value;
    SX1268_readRegister(SX1268_REG_TX_MODULATION, &value, 1);
    if ((packetType == SX1268_LORA_MODEM) && (bw == 500000)) value &= 0xFB;
    else value |= 0x04;
    SX1268_writeRegister(SX1268_REG_TX_MODULATION, &value, 1);
}

void SX1268_fixResistanceAntenna()
{
    uint8_t value;
    SX1268_readRegister(SX1268_REG_TX_CLAMP_CONFIG, &value, 1);
    value |= 0x1E;
    SX1268_writeRegister(SX1268_REG_TX_CLAMP_CONFIG, &value, 1);
}

void SX1268_fixRxTimeout()
{
    uint8_t value = 0x00;
    SX1268_writeRegister(SX1268_REG_RTC_CONTROL, &value, 1);
    SX1268_readRegister(SX1268_REG_EVENT_MASK, &value, 1);
    value = value | 0x02;
    SX1268_writeRegister(SX1268_REG_EVENT_MASK, &value, 1);
}

void SX1268_fixInvertedIq(uint8_t invertIq)
{
    uint8_t value;
    SX1268_readRegister(SX1268_REG_IQ_POLARITY_SETUP, &value, 1);
    if (invertIq) value |= 0x04;
    else value &= 0xFB;
    SX1268_writeRegister(SX1268_REG_IQ_POLARITY_SETUP, &value, 1);
}

void SX1268_transfer(uint8_t opCode, uint8_t* data, uint8_t nBytes)
{
    SX1268_transfer(opCode, data, nBytes, NULL, 0);
}

void SX1268_transfer(uint8_t opCode, uint8_t* data, uint8_t nBytes, uint8_t* address, uint8_t nAddress)
{
    if (SX1268_busyCheck(SX1268_BUSY_TIMEOUT)) return;

    digitalWrite(SX1268_nss, LOW);
    SX1268_spi->beginTransaction(SPISettings(SX1268_spiFrequency, MSBFIRST, SPI_MODE0));
    SX1268_spi->transfer(opCode);
    for (int8_t i=0; i<nAddress; i++) SX1268_spi->transfer(address[i]);
    for (int8_t i=0; i<nBytes; i++) data[i] = SX1268_spi->transfer(data[i]);
    SX1268_spi->endTransaction();
    digitalWrite(SX1268_nss, HIGH);
}
