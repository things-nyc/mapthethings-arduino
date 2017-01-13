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

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 19,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 18,
    .dio = {16, 5, 6}, // Moved dio0 from 17 because of overlapping ExtInt4 (pin6)
};

typedef enum LoraModeEnum {
  NeedsConfiguration,   // Not enough information to do anything
  ReadyToJoin,          // Has AppKey, AppEUI, and DevEUI
  Ready,                // We have session vars via ABP or OTAA - communicate at will
} LoraMode;


static LoraMode mode = NeedsConfiguration;

static JoinResultCallbackFn onJoinCb = NULL;
static TransmitResultCallbackFn onTransmitCb = NULL;

static u1_t join_appkey[16];
static u1_t join_appeui[8];
static u1_t join_deveui[8];

void os_getArtEui (u1_t* buf) {
  debugLogData("Asking for AppEUI", join_appeui, sizeof(join_appeui));
  memcpy(buf, join_appeui, sizeof(join_appeui));
}
void os_getDevEui (u1_t* buf) {
  debugLogData("Asking for DevEUI", join_deveui, sizeof(join_deveui));
  memcpy(buf, join_deveui, sizeof(join_deveui));
}
void os_getDevKey (u1_t* buf) {
  debugLogData("Asking for AppKey", join_appkey, sizeof(join_appkey));
  memcpy(buf, join_appkey, sizeof(join_appkey));
}

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
  if (mode!=Ready) {
    Log.Debug(F("mode not ready, not sending" CR));
    return false; // Did not enqueue
  }
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
            mode = Ready;
            if (onJoinCb) {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkSKey[16];
              u1_t appSKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkSKey, appSKey);
              devaddr = __builtin_bswap32(devaddr); // Settings and Bluetooth deal with devAddr as big endian byte array
              onJoinCb(appSKey, nwkSKey, (u1_t *)&devaddr);
            }
            break;
        case EV_RFU1:
            Log.Debug(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Log.Debug(F("EV_JOIN_FAILED"));
            if (onJoinCb) {
              onJoinCb(NULL, NULL, NULL);
            }
            break;
        case EV_REJOIN_FAILED:
            Log.Debug(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            os_clearCallback(&timeoutjob);
            Log.Debug(F("EV_TXCOMPLETE (includes waiting for RX windows)" CR));
            digitalWrite(LED_BUILTIN, LOW); // off
            if (onTransmitCb) {
              Log.Debug(F("Calling transmit callback..." CR));
              u1_t *received = NULL;
              u1_t len = 0;
              if (LMIC.dataLen>0) {
                received = LMIC.frame+LMIC.dataBeg;
                len = LMIC.dataLen;

                // Log received data.
                u1_t *p = received;
                for (int i=0; i<LMIC.dataLen; ++i) {
                  Log.Debug("%x", *p++);
                }
                // Log.Debug("%*h", LMIC.dataLen, LMIC.frame+LMIC.dataBeg);
                Log.Debug(CR);
              }
              uint32_t tx_seq_no = LMIC_getSeqnoUp()-1; // LMIC_getSeqnoUp returns the NEXT one. We want to return the one used.
              onTransmitCb(0 /* success */, tx_seq_no, received, len);
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
        case EV_SCAN_FOUND:
            Log.Debug(F("EV_SCAN_FOUND"));
            break;
        case EV_TXSTART:
            Log.Debug(F("EV_TXSTART"));
            break;
        default:
            Log.Debug(F("Unknown event: %d"), (int)ev);
            break;
    }
    Log.Debug(CR);
}

static void configureLora(uint32_t seq_no) {
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

    debugLog("Set LoRa seq no:", seq_no);
    LMIC_setSeqnoUp(seq_no);
}

void setupLora(TransmitResultCallbackFn txcb) {
    onTransmitCb = txcb;

    #ifdef VCC_ENABLE
    // For Pinoccio Scout boards
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
    #endif
}

void loopLora() {
  if (mode!=NeedsConfiguration) {
    os_runloop_once();
  }
}

void loraJoin(uint32_t seq_no, u1_t *appkey, u1_t *appeui, u1_t *deveui, JoinResultCallbackFn joincb) {
  onJoinCb = joincb;

  memcpy(join_appkey, appkey, sizeof(join_appkey));
  memcpy(join_appeui, appeui, sizeof(join_appeui));
  memcpy(join_deveui, deveui, sizeof(join_deveui));

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  configureLora(seq_no);

  if (LMIC_startJoining()) {
    debugPrint("Started joining.");
  }
  else {
    debugPrint("Error: Expected to start joining, but did not!");
  }

  mode = ReadyToJoin;
}

void loraSetSessionKeys(uint32_t seq_no, u1_t *appskey, u1_t *nwkskey, u1_t *devaddr) {
  debugLogData("Devaddr: ", devaddr, 4);
  debugLogData("appskey", appskey, 16);
  debugLogData("nwkskey", nwkskey, 16);

  // LMIC init
  os_init();
  // Reset the MAC state. Session and pending data transfers will be discarded.
  LMIC_reset();

  devaddr_t da = *(devaddr_t *)devaddr;
  da = __builtin_bswap32(da); // Settings and Bluetooth deal with devAddr as big endian byte array
  LMIC_setSession(0x1 /* TTN_NETWORK_ID */, da, nwkskey, appskey);

  configureLora(seq_no);

  mode = Ready;
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
