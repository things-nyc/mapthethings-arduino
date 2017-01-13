#include "lmic.h"
#include "hal/hal.h"
#include "lmic/oslmic.h"

#define TTN_NETWORK_ID 0x13

extern "C"{
  void debugPrint(const char *msg);
  void debugLog(const char *msg, uint32_t value);
  void debugLogData(const char *msg, uint8_t data[], uint16_t len);
}

typedef void (*JoinResultCallbackFn) (u1_t *appskey, u1_t *nwkskey, u1_t *devaddr);
typedef void (*TransmitResultCallbackFn) (uint16_t error, uint32_t seq_no, u1_t *received, u1_t length);

void setupLora(TransmitResultCallbackFn txcb);
void loopLora(void);
void loraJoin(uint32_t seq_no, u1_t *appkey, u1_t *appeui, u1_t *deveui, JoinResultCallbackFn joincb);
void loraSetSessionKeys(uint32_t seq_no, u1_t *appskey, u1_t *nwkskey, u1_t *devaddr);
bool loraSendBytes(uint8_t *data, uint16_t len);
void loraSetSF(uint sf);
