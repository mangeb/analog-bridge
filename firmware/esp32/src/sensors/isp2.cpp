/**
 *  Analog Bridge — ISP2 Implementation
 *
 *  Ported from AVR analog-bridge.ino lines 446-939.
 *  Nearly verbatim — the ISP2 state machine is platform-agnostic.
 *
 *  Changes from AVR:
 *    - HardwareSerial(2) with explicit pin assignment
 *    - Uses shared isp2_defs.h for protocol constants
 *    - No F() macros
 */
#include "isp2.h"
#include "config.h"
#include <Arduino.h>
#include "isp2_defs.h"

static HardwareSerial isp2Serial(ISP2_UART_NUM);

// ISP2 state machine
enum ISP2State { ISP2_SYNC_HIGH, ISP2_SYNC_LOW, ISP2_READING_PAYLOAD };
static ISP2State isp2State       = ISP2_SYNC_HIGH;
static byte      isp2Header[2];
static byte      isp2Data[ISP2_MAX_WORDS * 2];
static int       isp2PacketLen   = 0;
static bool      isp2IsData      = false;
static int       isp2BytesRead   = 0;
static int       isp2BytesExpected = 0;
static unsigned long isp2LastByte = 0;

static uint8_t   isp2AuxCount   = 0;
static uint8_t   isp2Lc1Count   = 0;

//----------------------------------------------------------------
// Process a complete ISP2 data packet
//----------------------------------------------------------------
static void processISP2Data(SensorData &data) {
  uint8_t auxIdx = 0;
  uint8_t lc1Idx = 0;
  float auxV[8];
  float afrVal[4];

  int w = 0;
  while (w < isp2PacketLen) {
    byte hi = isp2Data[w * 2];
    byte lo = isp2Data[w * 2 + 1];

    if (hi & ISP2_LC1_FLAG) {
      // LC-1 header word
      int func    = (hi >> 2) & 0x07;
      int afrMult = ((hi & 0x01) << 7) | (lo & 0x7F);

      // Next word is lambda
      w++;
      if (w >= isp2PacketLen) break;
      hi = isp2Data[w * 2];
      lo = isp2Data[w * 2 + 1];

      int lambda = ((hi & 0x3F) << 7) | (lo & 0x7F);

      if (lc1Idx < 4) {
        if (func <= 1) {
          afrVal[lc1Idx] = ((float)(lambda + 500) * (float)afrMult) / 10000.0f;
        } else {
          afrVal[lc1Idx] = 0.0f;
        }
      }
      lc1Idx++;
    } else {
      // Aux sensor word: 10-bit value
      int raw = ((hi & 0x07) << 7) | (lo & 0x7F);
      if (auxIdx < 8) {
        auxV[auxIdx] = (float)raw / 1023.0f * 5.0f;
      }
      auxIdx++;
    }
    w++;
  }

  isp2AuxCount = auxIdx;
  isp2Lc1Count = lc1Idx;

  // Map aux channels (daisy-chain order)
  if (auxIdx >= 1) data.coolant = AUX_COOLANT_F(auxV[0]);
  if (auxIdx >= 2) data.oilp    = AUX_OILP_PSI(auxV[1]);
  if (auxIdx >= 3) data.map     = AUX_MAP_INHG(auxV[2]);
  if (auxIdx >= 4) data.vss     = AUX_VSS_MPH(auxV[3]);

  // Map LC-1 devices to AFR
  if (lc1Idx >= 1) data.afr  = afrVal[0];
  if (lc1Idx >= 2) data.afr1 = afrVal[1];

#ifdef ISP2_DEBUG
  Serial.printf("ISP2: %dxLC1 %dxAUX | AFR=%.1f AFR1=%.1f VSS=%.1fmph MAP=%.1f OIL=%.0f CLT=%.0f\n",
    lc1Idx, auxIdx, data.afr, data.afr1, data.vss, data.map, data.oilp, data.coolant);
#endif
}

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

void isp2Init() {
  isp2Serial.begin(ISP2_BAUD, SERIAL_8N1, ISP2_RX_PIN, ISP2_TX_PIN);
  Serial.println("INF: ISP2 @ 19200");
}

HardwareSerial& isp2GetSerial() {
  return isp2Serial;
}

uint8_t isp2GetAuxCount() { return isp2AuxCount; }
uint8_t isp2GetLc1Count() { return isp2Lc1Count; }
int     isp2GetState()    { return (int)isp2State; }

void isp2Read(SensorData &data) {
  // Watchdog: resync if stuck mid-payload
  if (isp2State == ISP2_READING_PAYLOAD &&
      (millis() - isp2LastByte > ISP2_TIMEOUT_MS)) {
    isp2State = ISP2_SYNC_HIGH;
  }

  while (isp2Serial.available() > 0) {
    byte b = isp2Serial.read();
    isp2LastByte = millis();

    switch (isp2State) {
      case ISP2_SYNC_HIGH:
        if ((b & ISP2_H_SYNC_MASK) == ISP2_H_SYNC_MASK) {
          isp2Header[0] = b;
          isp2State = ISP2_SYNC_LOW;
        }
        break;

      case ISP2_SYNC_LOW:
        if ((b & ISP2_L_SYNC_MASK) == ISP2_L_SYNC_MASK) {
          isp2Header[1] = b;
          isp2IsData = bitRead(isp2Header[0], 4);
          isp2PacketLen = ((isp2Header[0] & 0x01) << 7) | (isp2Header[1] & 0x7F);

          if (isp2PacketLen > 0 && isp2PacketLen <= ISP2_MAX_WORDS) {
            isp2BytesExpected = isp2PacketLen * 2;
            isp2BytesRead = 0;
            isp2State = ISP2_READING_PAYLOAD;
          } else {
            isp2State = ISP2_SYNC_HIGH;
          }
        } else if ((b & ISP2_H_SYNC_MASK) == ISP2_H_SYNC_MASK) {
          isp2Header[0] = b;
          // stay in SYNC_LOW — treat as new high sync
        } else {
          isp2State = ISP2_SYNC_HIGH;
        }
        break;

      case ISP2_READING_PAYLOAD:
        isp2Data[isp2BytesRead++] = b;
        if (isp2BytesRead >= isp2BytesExpected) {
          if (isp2IsData) {
            processISP2Data(data);
          }
          isp2State = ISP2_SYNC_HIGH;
        }
        break;
    }
  }
}
