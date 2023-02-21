/*  Copyright (C) 2023  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
// This program receives a fake GDB 'G', set registers, command and
// returns the number of characters which were correctly received.
enum EState {
  STATE_WAIT4_DOLLAR_SIGN,
  STATE_BUFFER_DATA_UNTIL_POUND_SIGN,
  STATE_BUFFER_CHECKSUM
};

static const uint8_t g_testData[] = "$G000000000000000001000000adc10020b4b90020d032002000000000010000001300800000000000000000000000000000000000a0b9002091ae010020e6010000000f61000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000#f8";
static uint8_t g_buffer[sizeof(g_testData) - 1];
static size_t g_bufferIndex = 0;
static size_t g_checksumIndex = 0;
static EState g_state = STATE_WAIT4_DOLLAR_SIGN;


void setup() {
  Serial.begin(230400);
}

void loop() {
  if (Serial.available() == 0) {
    return;
  }
  uint8_t by = Serial.read();

  g_bufferIndex++;
  if (g_state != STATE_WAIT4_DOLLAR_SIGN && g_bufferIndex < sizeof(g_buffer)) {
    g_buffer[g_bufferIndex] = by;
  }

  // Doesn't matter current state, if $ is seen then that is the start of the next packet.
  if (by == '$') {
    g_state = STATE_BUFFER_DATA_UNTIL_POUND_SIGN;
    g_bufferIndex = 0;
    g_buffer[g_bufferIndex] = by;
  }

  switch (g_state) {
    case STATE_WAIT4_DOLLAR_SIGN:
      // Already handled in previous if statement.
      break;
    case STATE_BUFFER_DATA_UNTIL_POUND_SIGN:
      if (by == '#') {
        g_state = STATE_BUFFER_CHECKSUM;
        g_checksumIndex = 0;
      }
      break;
    case STATE_BUFFER_CHECKSUM:
      g_checksumIndex++;
      if (g_checksumIndex >= 2) {
        g_state = STATE_WAIT4_DOLLAR_SIGN;
        checkBuffer();
      }
      break;
  }
}

static void checkBuffer() {
  if (g_bufferIndex >= sizeof(g_buffer)) {
    uint32_t count = g_bufferIndex+1;
    Serial.write((const char*)&count, sizeof(count));
    return;
  }

  size_t i;
  for (i = 0; i <= g_bufferIndex && g_testData[i] == g_buffer[i]; i++) {
  }

  // Write out the number of matched bytes.
  Serial.write((const char*)&i, sizeof(i));
}
