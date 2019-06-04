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

#include "ringbuffer.h"

#include <errno.h>
#include <assert.h>
#include <string.h>

size_t get_available_write(size_t allocated_size,
                           size_t write_index,
                           size_t read_index)
{
    size_t result = read_index - write_index - 1;
    if (write_index >= read_index) {
        result += allocated_size;
    }
    return result;
}

size_t get_available_read(size_t allocated_size,
                          size_t write_index,
                          size_t read_index)
{
    if (write_index >= read_index) {
        return write_index - read_index;
    }
    return write_index + allocated_size - read_index;
}

int ringbuffer_init(ringbuffer_t* ringbuffer, size_t size) {
    if (!ringbuffer) {
        return -EINVAL;
    }

    // extra bit used to distinguish read and write indices when full
    size_t allocated_size = size + 1;
    ringbuffer->data = (int8_t *) calloc(allocated_size, sizeof(int8_t));
    if(!ringbuffer->data) {
        return -ENOMEM;
    }

    ringbuffer->allocated_size = allocated_size;
    ringbuffer->read_index = 0;
    ringbuffer->write_index = 0;

    return 0;
}
void ringbuffer_destroy(ringbuffer_t* ringbuffer) {
    assert(ringbuffer);

    free(ringbuffer->data);
}

int ringbuffer_size(const ringbuffer_t* ringbuffer)
{
    if (!ringbuffer) {
        return -EINVAL;
    }

    return ringbuffer->allocated_size - 1;
}

int ringbuffer_available_write(const ringbuffer_t* ringbuffer)
{
    if (!ringbuffer) {
        return -EINVAL;
    }
    return get_available_write(ringbuffer->allocated_size,
                               ringbuffer->read_index,
                               ringbuffer->write_index);
}

int ringbuffer_available_read(const ringbuffer_t* ringbuffer)
{
    if (!ringbuffer) {
        return -EINVAL;
    }
    return get_available_read(ringbuffer->allocated_size,
                              ringbuffer->read_index,
                              ringbuffer->write_index);
}

int ringbuffer_write(ringbuffer_t* ringbuffer, const int8_t* data, size_t count)
{
    if (!ringbuffer) {
        return 0;
    }

    const size_t write_index = atomic_load_explicit(&ringbuffer->write_index,
                                                    memory_order_relaxed);
    const size_t read_index = atomic_load_explicit(&ringbuffer->read_index,
                                                    memory_order_acquire);
    const size_t available_write = get_available_write(ringbuffer->allocated_size,
                                                       write_index,
                                                       read_index);
    if(!available_write) {
        // can't write a thing
        return 0;
    } else if (count > available_write) {
        // write all we can
        count = available_write;
    }

    size_t write_index_after = write_index + count;

    if (write_index + count > ringbuffer->allocated_size) {
        size_t countA = ringbuffer->allocated_size - write_index;
        size_t countB = count - countA;

        memcpy(ringbuffer->data + write_index, data, countA);
        memcpy(ringbuffer->data, data + countA, countB);
        write_index_after -= ringbuffer->allocated_size;
    } else {
        memcpy(ringbuffer->data + write_index, data, count * sizeof(int8_t));
        if (write_index_after == ringbuffer->allocated_size) {
            write_index_after = 0;
        }
    }

    atomic_store_explicit(&ringbuffer->write_index, write_index_after, memory_order_release);
    return count;
}
int ringbuffer_read(ringbuffer_t* ringbuffer, int8_t* data, size_t count)
{
    if (!ringbuffer) {
        return 0;
    }

    const size_t write_index = atomic_load_explicit(&ringbuffer->write_index,
                                                   memory_order_acquire);
    const size_t read_index = atomic_load_explicit(&ringbuffer->read_index,
                                                  memory_order_relaxed);
    const size_t available_read = get_available_read(ringbuffer->allocated_size,
                                                     write_index,
                                                     read_index);
    if(!available_read) {
        // can't read a thing
        return 0;
    } else if(count > available_read) {
        // read all we can
        count = available_read;
    }

    size_t read_index_after = read_index + count;

    if (read_index + count > ringbuffer->allocated_size) {
        size_t countA = ringbuffer->allocated_size - read_index;
        size_t countB = count - countA;

        memcpy(data, ringbuffer->data + read_index, countA * sizeof(int8_t));
        memcpy(data + countA, ringbuffer->data, countB * sizeof(int8_t));

        read_index_after -= ringbuffer->allocated_size;
    } else {
        memcpy(data, ringbuffer->data + read_index, count * sizeof(int8_t));
        if (read_index_after == ringbuffer->allocated_size) {
            read_index_after = 0;
        }
    }

    atomic_store_explicit(&ringbuffer->read_index, read_index_after, memory_order_release);
    return count;
}
