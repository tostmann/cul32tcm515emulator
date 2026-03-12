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

#ifndef ENOCEAN_STREAM_H
#define ENOCEAN_STREAM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct enocean_stream_t enocean_stream_t;

struct enocean_stream_t {
    void *ctx;
    int    (*init)       (enocean_stream_t *stream, uint32_t baud);
    int    (*available)  (enocean_stream_t *stream);
    int    (*read)       (enocean_stream_t *stream);
    size_t (*read_buf)   (enocean_stream_t *stream, uint8_t *buf, size_t size);
    size_t (*write)      (enocean_stream_t *stream, uint8_t byte);
    size_t (*write_buf)  (enocean_stream_t *stream, const uint8_t *buf, size_t size);
};

#define STREAM_INIT(s, baud)       ((s)->init((s), (baud)))
#define STREAM_AVAILABLE(s)        ((s)->available((s)))
#define STREAM_READ(s)             ((s)->read((s)))
#define STREAM_READ_BUF(s, b, sz)  ((s)->read_buf((s), (b), (sz)))
#define STREAM_WRITE(s, b)         ((s)->write((s), (b)))
#define STREAM_WRITE_BUF(s, b, sz) ((s)->write_buf((s), (b), (sz)))

#ifdef __cplusplus
}
#endif

#endif
