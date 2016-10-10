#include "lmic.h"
#include "hal/hal.h"
#include "lmic/oslmic.h"

extern const u1_t NWKSKEY[16];
extern const u1_t APPSKEY[16];
extern const u4_t DEVADDR;

void setupLora(void);
void loopLora(void);
bool loraSendBytes(uint8_t *data, uint16_t len);
void loraSetSF(uint sf);
