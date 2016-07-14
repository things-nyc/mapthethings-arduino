/*******************************************************************************
 * Copyright (c) 2016 Frank Leon Rose
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 *******************************************************************************/


#include "Lora.h"
#include "Bluetooth.h"

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting"));
    digitalWrite(LED_BUILTIN, LOW); // off

    setupBluetooth();
    setupLora();
}

void loop() {
    loopLora();
    loopBluetooth();
}
