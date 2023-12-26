#include <RadioLib.h>
#include <LoRa.h>
#include <WiFi.h>
#include "configuration.h"
#include "aprs_is_utils.h"
#include "syslog_utils.h"
#include "pins_config.h"
#include "display.h"

extern Configuration  Config;
extern int            stationMode;

#ifdef HELTEC_V3
SX1262 radio = new Module(RADIO_CS_PIN, RADIO_DIO1_PIN, RADIO_RST_PIN, RADIO_BUSY_PIN);
bool transmissionFlag = true;
bool enableInterrupt = true;
#endif

int rssi, freqError;
float snr;

namespace LoRa_Utils {

  void setFlag(void) {
    #ifdef HELTEC_V3
    transmissionFlag = true;
    #endif
  }

  void setup() {
    #if defined(TTGO_T_LORA_V2_1) || defined(HELTEC_V2)
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
    long freq;
    if (stationMode==1 || stationMode==2) {
      freq = Config.loramodule.iGateFreq;
    } else {
      freq = Config.loramodule.digirepeaterTxFreq;
    }
    if (!LoRa.begin(freq)) {
      Serial.println("Starting LoRa failed!");
      show_display("ERROR", "Starting LoRa failed!");
      while (true) {
        delay(1000);
      }
    }
    LoRa.setSpreadingFactor(Config.loramodule.spreadingFactor);
    LoRa.setSignalBandwidth(Config.loramodule.signalBandwidth);
    LoRa.setCodingRate4(Config.loramodule.codingRate4);
    LoRa.enableCrc();
    LoRa.setTxPower(Config.loramodule.power);
    Serial.print("init : LoRa Module    ...     done!");
    #endif
    #ifdef HELTEC_V3
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
    float freq = (float)Config.loramodule.iGateFreq/1000000;
    int state = radio.begin(freq);
    if (state == RADIOLIB_ERR_NONE) {
      
      Serial.print("Initializing SX126X LoRa Module");
    } else {
      Serial.println("Starting LoRa failed!");
      while (true);
    }
    radio.setDio1Action(setFlag);
    radio.setSpreadingFactor(Config.loramodule.spreadingFactor);
    radio.setBandwidth(Config.loramodule.signalBandwidth);
    radio.setCodingRate(Config.loramodule.codingRate4);
    state = radio.setOutputPower(Config.loramodule.power + 2); // values available: 10, 17, 22 --> if 20 in tracker_conf.json it will be updated to 22.

    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("init : LoRa Module    ...     done!");
    } else {
      Serial.println("Starting LoRa failed!");
      while (true);
    }
    #endif
  }

  void sendNewPacket(const String &typeOfMessage, const String &newPacket) {
    digitalWrite(greenLed,HIGH);
    #if defined(TTGO_T_LORA_V2_1) || defined(HELTEC_V2)
    LoRa.beginPacket();
    LoRa.write('<');
    if (typeOfMessage == "APRS")  {
      LoRa.write(0xFF);
    } else if (typeOfMessage == "LoRa") {
      LoRa.write(0xF8);
    }
    LoRa.write(0x01);
    LoRa.write((const uint8_t *)newPacket.c_str(), newPacket.length());
    LoRa.endPacket();
    #endif
    #ifdef HELTEC_V3
    int state = radio.transmit("\x3c\xff\x01" + newPacket);
    if (state == RADIOLIB_ERR_NONE) {
      //Serial.println(F("success!"));
    } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
      Serial.println(F("too long!"));
    } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
      Serial.println(F("timeout!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
    }
    #endif
    digitalWrite(greenLed,LOW);
    SYSLOG_Utils::log("LoRa Tx", newPacket,0,0,0);
    Serial.print("---> LoRa Packet Tx    : ");
    Serial.println(newPacket);
  }

  String generatePacket(String aprsisPacket) {
    String firstPart, messagePart;
    aprsisPacket.trim();
    firstPart = aprsisPacket.substring(0, aprsisPacket.indexOf(","));
    messagePart = aprsisPacket.substring(aprsisPacket.indexOf("::")+2);
    return firstPart + ",TCPIP,WIDE1-1," + Config.callsign + "::" + messagePart;
  }

  String receivePacket() {
    String loraPacket = "";
    #if defined(TTGO_T_LORA_V2_1) || defined(HELTEC_V2)
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      while (LoRa.available()) {
        int inChar = LoRa.read();
        loraPacket += (char)inChar;
      }
      rssi      = LoRa.packetRssi();
      snr       = LoRa.packetSnr();
      freqError = LoRa.packetFrequencyError();
    }
    #endif
    #ifdef HELTEC_V3
    if (transmissionFlag) {
      transmissionFlag = false;
      radio.startReceive();
      int state = radio.readData(loraPacket);
      if (state == RADIOLIB_ERR_NONE) {
        Serial.println("LoRa Rx ---> " + loraPacket);
        rssi      = radio.getRSSI();
        snr       = radio.getSNR();
        freqError = radio.getFrequencyError();
      } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // timeout occurred while waiting for a packet
      } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println(F("CRC error!"));
      } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
      }
    }
    #endif
    #ifndef PinPointApp
    Serial.println("(RSSI:" +String(rssi) + " / SNR:" + String(snr) +  " / FreqErr:" + String(freqError) + ")");
    #endif
    if (Config.syslog.active && (stationMode==1 || stationMode==2 || (stationMode==5 && WiFi.status()==WL_CONNECTED))) {
      SYSLOG_Utils::log("LoRa Rx", loraPacket, rssi, snr, freqError);
    }
    return loraPacket;
  }

  void changeFreqTx() {
    delay(500);
    #if defined(TTGO_T_LORA_V2_1) || defined(HELTEC_V2)
    LoRa.setFrequency(Config.loramodule.digirepeaterTxFreq);
    #endif
    #ifdef HELTEC_V3
    float freq = (float)Config.loramodule.digirepeaterTxFreq/1000000;
    radio.setFrequency(freq);
    #endif
  }

  void changeFreqRx() {
    delay(500);
    #if defined(TTGO_T_LORA_V2_1) || defined(HELTEC_V2)
    LoRa.setFrequency(Config.loramodule.digirepeaterRxFreq);
    #endif
    #ifdef HELTEC_V3
    float freq = (float)Config.loramodule.digirepeaterRxFreq/1000000;
    radio.setFrequency(freq);
    #endif
  }

}