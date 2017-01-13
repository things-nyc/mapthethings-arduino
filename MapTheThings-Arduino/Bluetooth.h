#include <stdint.h>

typedef void (*WriteCharacteristicCallbackFn) (uint8_t[], uint16_t);

typedef struct {
  int32_t charId;
  const char *charDef;
  WriteCharacteristicCallbackFn callback;
} CharacteristicConfigType;

#define UNINITIALIZED -100
#define COUNT(x) (sizeof(x) / sizeof(*x))

void setupBluetooth(CharacteristicConfigType *cconfigs, int32_t cccount);
void loopBluetooth(void);
void bluetoothDisconnect();

void setBluetoothCharData(uint8_t charID, uint8_t const data[], uint8_t size);

void sendBatteryLevel(uint8_t level);
void sendTxResult(uint8_t bleSeq, uint16_t error, uint32_t seq_no);
void sendLogMessage(const char *s);

bool writeNVInt(uint8_t offset, int32_t number);
bool writeNVBytes(uint8_t offset, uint8_t *bytes, uint8_t length);
bool readNVInt(uint8_t offset, int32_t *number);
bool readNVBytes(uint8_t offset, uint8_t *bytes, uint8_t length);
