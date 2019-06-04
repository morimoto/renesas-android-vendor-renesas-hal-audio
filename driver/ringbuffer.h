/*
 * Copyright (C) 2019 GlobalLogic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdlib.h>
#include <stdatomic.h>

/* C11 lock-free ringbuffer implementation
 * This remains thread safe with a single producer and single consumer
 */
typedef struct ringbuffer {
    int8_t *data;
    _Atomic size_t allocated_size;
    _Atomic size_t write_index, read_index;
} ringbuffer_t;

int ringbuffer_init(ringbuffer_t* ringbuffer, size_t size);
void ringbuffer_destroy(ringbuffer_t* ringbuffer);
int ringbuffer_size(const ringbuffer_t* ringbuffer);
int ringbuffer_available_write(const ringbuffer_t* ringbuffer);
int ringbuffer_available_read(const ringbuffer_t* ringbuffer);
int ringbuffer_write(ringbuffer_t* ringbuffer, const int8_t* data, size_t count);
int ringbuffer_read(ringbuffer_t* ringbuffer, int8_t* data, size_t count);

#endif