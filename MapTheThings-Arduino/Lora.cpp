/*
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in g1,
 *  0.1% in g2).
 *
 * Change DEVADDR to a unique address!
 * See http://thethingsnetwork.org/wiki/AddressSpace
 *
 * Do not forget to define the radio type correctly in config.h.
 */
#include <SPI.h>
#include "Lora.h"
#include "Logging.h"

#if defined(DISABLE_INVERT_IQ_ON_RX)
#error This example requires DISABLE_INVERT_IQ_ON_RX to be NOT set. Update \
       config.h in the lmic library to set it.
#endif

// LoRaWAN NwkSKey, network session key
// This is the default Semtech key, which is used by the prototype TTN
// network initially.
// 2B7E151628AED2A6ABF7158809CF4F3C
//static const PROGMEM u1_t NWKSKEY[16] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };

// LoRaWAN AppSKey, application session key
// This is the default Semtech key, which is used by the prototype TTN
// network initially.
//static const u1_t PROGMEM APPSKEY[16] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
//static const u4_t DEVADDR = 0x03FF0001 ; // <-- Change this address for every node!
//static const u4_t DEVADDR = 0x02D1EFEF ; // Manny's

//static const PROGMEM u1_t NWKSKEY[16] = { 0x80, 0x42, 0x43, 0x64, 0x2C, 0x1E, 0x3B, 0x04, 0x36, 0x6D, 0x36, 0xC3, 0x90, 0x9F, 0xCA, 0xA2 };
//static const u1_t PROGMEM APPSKEY[16] = { 0x43, 0x0D, 0x53, 0xB2, 0x72, 0xA6, 0x47, 0xAF, 0x5D, 0xFF, 0x6A, 0x16, 0x7A, 0xB7, 0x9A, 0x20 };
const u1_t NWKSKEY[16] = { 0x80, 0x42, 0x43, 0x64, 0x2C, 0x1E, 0x3B, 0x04, 0x36, 0x6D, 0x36, 0xC3, 0x90, 0x9F, 0xCA, 0xA2 };
const u1_t APPSKEY[16] = { 0x43, 0x0D, 0x53, 0xB2, 0x72, 0xA6, 0x47, 0xAF, 0x5D, 0xFF, 0x6A, 0x16, 0x7A, 0xB7, 0x9A, 0x20 };

// LoRaWAN end-device address (DevAddr)
// See http://thethingsnetwork.org/wiki/AddressSpace
const u4_t DEVADDR = 0xDEADAAAA ; // <-- Change this address for every node!

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 19,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 18,
    .dio = {16, 5, 6}, // Moved dio0 from 17 because of overlapping ExtInt4 (pin6)
};

static osjob_t timeoutjob;
static void txtimeout_func(osjob_t *job) {
  if (LMIC.opmode & OP_JOINING) {
     // keep waiting.. and don't time out.
     return;
  }
  digitalWrite(LED_BUILTIN, LOW); // off
  Log.Debug(F("Transmit Timeout" CR));
  //txActive = false;
  LMIC_clrTxData ();
}

bool loraSendBytes(uint8_t *data, uint16_t len) {
  ostime_t t = os_getTime();
  //os_setTimedCallback(&txjob, t + ms2osticks(100), tx_func);
  // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Log.Debug(F("OP_TXRXPEND, not sending" CR));
        return false; // Did not enqueue
    } else {
        // Prepare upstream data transmission at the next possible time.
        Log.Debug(F("Packet queued" CR));
        digitalWrite(LED_BUILTIN, HIGH); // off
        LMIC_setTxData2(1, data, len, 0);
        if (! (LMIC.opmode & OP_JOINING)) {
          // connection is up, message is queued:
          // Timeout TX after 20 seconds
          os_setTimedCallback(&timeoutjob, t + ms2osticks(20000), txtimeout_func);
        }
        return true;
    }
}

void onEvent (ev_t ev) {
    Log.Debug("%d: ", os_getTime());
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Log.Debug(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Log.Debug(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Log.Debug(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Log.Debug(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Log.Debug(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Log.Debug(F("EV_JOINED"));
            break;
        case EV_RFU1:
            Log.Debug(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Log.Debug(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Log.Debug(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            os_clearCallback(&timeoutjob);
            Log.Debug(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            digitalWrite(LED_BUILTIN, LOW); // off

            if(LMIC.dataLen) {
                // data received in rx slot after tx
                Log.Debug(F("Data Received: "));
                u1_t *p = LMIC.frame+LMIC.dataBeg;
                for (int i=0; i<LMIC.dataLen; ++i) {
                  Log.Debug("%x", *p++);
                }
                // Log.Debug("%*h", LMIC.dataLen, LMIC.frame+LMIC.dataBeg);
                Log.Debug(CR);
            }
            break;
        case EV_LOST_TSYNC:
            Log.Debug(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Log.Debug(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Log.Debug(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Log.Debug(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Log.Debug(F("EV_LINK_ALIVE"));
            break;
         default:
            Log.Debug(F("Unknown event"));
            break;
    }
    Log.Debug(CR);
}

void setupLora() {
    #ifdef VCC_ENABLE
    // For Pinoccio Scout boards
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
    #endif

    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    // Set static session parameters. Instead of dynamically establishing a session
    // by joining the network, precomputed session parameters are provided.
    #ifdef PROGMEM
    // On AVR, these values are stored in flash and only copied to RAM
    // once. Copy them to a temporary buffer here, LMIC_setSession will
    // copy them into a buffer of its own again.
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
    #else
    // If not running an AVR with PROGMEM, just use the arrays directly
    LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
    #endif

    #if defined(CFG_eu868)
    // Set up the channels used by the Things Network, which corresponds
    // to the defaults of most gateways. Without this, only three base
    // channels from the LoRaWAN specification are used, which certainly
    // works, so it is good for debugging, but can overload those
    // frequencies, so be sure to configure the full frequency range of
    // your network here (unless your network autoconfigures them).
    // Setting up channels should happen after LMIC_setSession, as that
    // configures the minimal channel set.
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
    // TTN defines an additional channel at 869.525Mhz using SF9 for class B
    // devices' ping slots. LMIC does not have an easy way to define set this
    // frequency and support for class B is spotty and untested, so this
    // frequency is not configured here.
    #endif

    LMIC_selectSubBand(1);

    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // Set data rate and transmit power (note: txpow seems to be ignored by the library)
    LMIC_setDrTxpow(DR_SF10,20);
}

void loopLora() {
    os_runloop_once();
}

void loraSetSF(uint sf) {
  dr_t dr;
  switch (sf) {
    case 7: dr = DR_SF7; break;
    case 8: dr = DR_SF8; break;
    case 9: dr = DR_SF9; break;
    case 10: dr = DR_SF10; break;
    default:
      dr = DR_SF10;
      Log.Debug(F("Invalid SF value: %d" CR), sf);
      break;
  }
  LMIC_setDrTxpow(dr,20);
}
