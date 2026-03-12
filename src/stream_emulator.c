/*
 * Copyright (c) 2026 Dirk Tostmann tostmann@busware.de
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "enocean_stream.h"
#include "TCMSerial.h"

static int emu_init(enocean_stream_t *stream, uint32_t baud) {
    TCMSerial_begin(baud);
    return 0;
}

static int emu_available(enocean_stream_t *stream) {
    return TCMSerial_available();
}

static int emu_read(enocean_stream_t *stream) {
    return TCMSerial_read();
}

static size_t emu_read_buf(enocean_stream_t *stream, uint8_t *buf, size_t size) {
    return TCMSerial_read_buf(buf, size);
}

static size_t emu_write(enocean_stream_t *stream, uint8_t byte) {
    return TCMSerial_write(byte);
}

static size_t emu_write_buf(enocean_stream_t *stream, const uint8_t *buf, size_t size) {
    return TCMSerial_write_buf(buf, size);
}

enocean_stream_t* get_emulator_stream(void) {
    static enocean_stream_t emu_stream = {
        .ctx       = NULL,
        .init      = emu_init,
        .available = emu_available,
        .read      = emu_read,
        .read_buf  = emu_read_buf,
        .write     = emu_write,
        .write_buf = emu_write_buf
    };
    return &emu_stream;
}
