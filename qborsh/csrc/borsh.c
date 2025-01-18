#include "borsh.h"
#include <stdio.h>  // for fprintf, stderr
#include <stdlib.h> // for malloc, free, realloc
#include <string.h> // for memcpy

/*
 * Internal 'error' flag: if set, subsequent ops no-op.
 * Helps prevent further corruption after an OOM or invalid read.
 */

/*
 * Private helper to set buffer error.
 * Used instead of open-coding error assignment for consistency.
 */
static void set_buffer_error(Buffer *buf)
{
    if (buf)
    {
        buf->error = true;
    }
}

/*
 * Returns whether the buffer is in an error state.
 */
bool buffer_has_error(const Buffer *buf)
{
    if (!buf)
        return true;
    return buf->error;
}

/*
 * A small inline helper to expand buffer capacity if needed.
 * Uses a custom growth strategy to avoid multiple expansions on large writes.
 */
static inline void ensure_capacity(Buffer *buf, size_t additional)
{
    // No expansion if already errored
    if (buf->error)
        return;

    // If we don't have enough space, reallocate
    if (buf->size + additional > buf->capacity)
    {
        size_t needed = buf->size + additional;

        // Custom growth strategy: double until 1KB, then 1.5x
        size_t new_capacity = 0;
        if (buf->capacity < 1024)
        {
            new_capacity = buf->capacity * 2;
        }
        else
        {
            new_capacity = (buf->capacity * 3) / 2;
        }
        // Ensure at least 'needed'
        if (new_capacity < needed)
        {
            new_capacity = needed;
        }

        // Check for overflow
        if (new_capacity < buf->size || new_capacity < additional)
        {
            fprintf(stderr, "ensure_capacity: capacity overflow\n");
            set_buffer_error(buf);
            return;
        }

        // Attempt to expand
        uint8_t *new_data = (uint8_t *)realloc(buf->data, new_capacity);
        if (!new_data)
        {
            fprintf(stderr, "ensure_capacity: out of memory\n");
            set_buffer_error(buf);
            return;
        }
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
}

/*
 * Reserve space in the buffer and return a pointer to where new data can be written.
 * Ensures the capacity is sufficient and updates size, so the caller can memcpy data directly.
 */
static inline uint8_t *reserve_space(Buffer *buf, size_t count)
{
    if (buf->error)
        return NULL;
    ensure_capacity(buf, count);
    if (buf->error)
        return NULL;
    uint8_t *dest = buf->data + buf->size;
    buf->size += count;
    return dest;
}

/*
 * Inline helper to write raw bytes into the buffer at the current size.
 */
static inline void write_le(Buffer *buf, const uint8_t *bytes, size_t count)
{
    // Reserve space and copy if no error
    uint8_t *dest = reserve_space(buf, count);
    if (!dest)
        return; // error set, so no-op
    memcpy(dest, bytes, count);
}

/*
 * Inline helper to read raw bytes from the buffer at the current offset.
 * Checks for read overrun before memcpy.
 */
static inline void read_le(Buffer *buf, uint8_t *out_bytes, size_t count)
{
    if (buf->error)
        return;
    // Ensure we have enough data to read
    if (buf->offset + count > buf->size)
    {
        fprintf(stderr, "read_le: attempt to read past buffer\n");
        set_buffer_error(buf);
        return;
    }
    memcpy(out_bytes, buf->data + buf->offset, count);
    buf->offset += count;
}

/* -----------------------------------------------------
 * Initialization / Cleanup
 * ----------------------------------------------------- */

void init_buffer(Buffer *buf, size_t initial_capacity)
{
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
    buf->offset = 0;
    buf->error = false;

    // Default capacity if caller didn't provide one
    if (initial_capacity == 0)
    {
        initial_capacity = 128;
    }

    if (initial_capacity > 0)
    {
        buf->data = (uint8_t *)malloc(initial_capacity);
        if (!buf->data)
        {
            fprintf(stderr, "init_buffer: out of memory\n");
            set_buffer_error(buf);
            return;
        }
        buf->capacity = initial_capacity;
    }
}

void free_buffer(Buffer *buf)
{
    if (buf->data)
    {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
    buf->offset = 0;
    buf->error = false;
}

/* -----------------------------------------------------
 * Write Functions
 * ----------------------------------------------------- */

/*
 * All write_* functions rely on write_le internally,
 * removing redundant error checks for simplicity and speed.
 */

void write_u8(Buffer *buf, uint8_t value)
{
    write_le(buf, &value, 1);
}

void write_u16(Buffer *buf, uint16_t value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    write_le(buf, (uint8_t *)&value, 2);
#else
    // Fallback for big-endian if needed
    uint8_t temp[2];
    temp[0] = (uint8_t)(value & 0xFF);
    temp[1] = (uint8_t)((value >> 8) & 0xFF);
    write_le(buf, temp, 2);
#endif
}

void write_u32(Buffer *buf, uint32_t value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    write_le(buf, (uint8_t *)&value, 4);
#else
    uint8_t temp[4];
    temp[0] = (uint8_t)(value & 0xFF);
    temp[1] = (uint8_t)((value >> 8) & 0xFF);
    temp[2] = (uint8_t)((value >> 16) & 0xFF);
    temp[3] = (uint8_t)((value >> 24) & 0xFF);
    write_le(buf, temp, 4);
#endif
}

void write_u64(Buffer *buf, uint64_t value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    write_le(buf, (uint8_t *)&value, 8);
#else
    uint8_t temp[8];
    for (int i = 0; i < 8; i++)
    {
        temp[i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    }
    write_le(buf, temp, 8);
#endif
}

void write_u128(Buffer *buf, unsigned __int128 value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    write_le(buf, (uint8_t *)&value, 16);
#else
    // Big-endian fallback
    uint8_t temp[16];
    for (int i = 0; i < 16; i++)
    {
        temp[i] = (uint8_t)((value >> (8ULL * i)) & 0xFF);
    }
    write_le(buf, temp, 16);
#endif
}

void write_i8(Buffer *buf, int8_t value)
{
    write_le(buf, (uint8_t *)&value, 1);
}

void write_i16(Buffer *buf, int16_t value)
{
    write_u16(buf, (uint16_t)value);
}

void write_i32(Buffer *buf, int32_t value)
{
    write_u32(buf, (uint32_t)value);
}

void write_i64(Buffer *buf, int64_t value)
{
    write_u64(buf, (uint64_t)value);
}

void write_i128(Buffer *buf, __int128 value)
{
    write_u128(buf, (unsigned __int128)value);
}

void write_f32(Buffer *buf, float value)
{
    uint32_t bitpattern;
    memcpy(&bitpattern, &value, sizeof(float));
    write_u32(buf, bitpattern);
}

void write_f64(Buffer *buf, double value)
{
    uint64_t bitpattern;
    memcpy(&bitpattern, &value, sizeof(double));
    write_u64(buf, bitpattern);
}

void write_bool(Buffer *buf, bool value)
{
    uint8_t bval = value ? 1 : 0;
    write_u8(buf, bval);
}

/*
 * Writes a fixed-size array. The new approach uses reserve_space
 * directly, avoiding repeated memcpy calls in other helpers.
 */
void write_fixed_array(Buffer *buf, const void *array_data,
                       size_t elem_size, size_t length)
{
    size_t total = elem_size * length;
    uint8_t *dest = reserve_space(buf, total);
    if (!dest)
        return; // error flagged
    memcpy(dest, array_data, total);
}

/*
 * Writes a vector by first writing the length, then copying the elements.
 */
void write_vec(Buffer *buf, const void *elem_data,
               size_t elem_size, size_t length)
{
    write_u32(buf, (uint32_t)length);
    uint8_t *dest = reserve_space(buf, elem_size * length);
    if (!dest)
        return;
    memcpy(dest, elem_data, elem_size * length);
}

void write_option(Buffer *buf, const void *data,
                  bool is_some, WriteFunc write_func)
{
    write_bool(buf, is_some);
    if (is_some && write_func)
    {
        write_func(buf, data);
    }
}

void write_enum(Buffer *buf, uint8_t variant_index,
                const void *variant_data, WriteFunc write_func)
{
    write_u8(buf, variant_index);
    if (variant_data && write_func)
    {
        write_func(buf, variant_data);
    }
}

void write_hashmap(Buffer *buf, MapEntry *entries,
                   size_t length,
                   WriteFunc key_write_func,
                   WriteFunc val_write_func)
{
    write_u32(buf, (uint32_t)length);
    for (size_t i = 0; i < length; i++)
    {
        key_write_func(buf, entries[i].key);
        val_write_func(buf, entries[i].value);
    }
}

void write_hashset(Buffer *buf, void **keys,
                   size_t length, WriteFunc key_write_func)
{
    write_u32(buf, (uint32_t)length);
    for (size_t i = 0; i < length; i++)
    {
        key_write_func(buf, keys[i]);
    }
}

/* -----------------------------------------------------
 * Read Functions
 * ----------------------------------------------------- */

/*
 * All read_* functions rely on read_le internally.
 * Big-endian fallback is used if necessary.
 * No redundant error checks in each function.
 */

uint8_t read_u8(Buffer *buf)
{
    uint8_t value = 0;
    read_le(buf, &value, 1);
    return value;
}

uint16_t read_u16(Buffer *buf)
{
    uint16_t value = 0;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    read_le(buf, (uint8_t *)&value, 2);
#else
    uint8_t temp[2];
    read_le(buf, temp, 2);
    value = (uint16_t)(temp[0] | ((uint16_t)temp[1] << 8));
#endif
    return value;
}

uint32_t read_u32(Buffer *buf)
{
    uint32_t value = 0;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    read_le(buf, (uint8_t *)&value, 4);
#else
    uint8_t temp[4];
    read_le(buf, temp, 4);
    for (int i = 0; i < 4; i++)
    {
        value |= ((uint32_t)temp[i]) << (8 * i);
    }
#endif
    return value;
}

uint64_t read_u64(Buffer *buf)
{
    uint64_t value = 0;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    read_le(buf, (uint8_t *)&value, 8);
#else
    uint8_t temp[8];
    read_le(buf, temp, 8);
    for (int i = 0; i < 8; i++)
    {
        value |= ((uint64_t)temp[i]) << (8 * i);
    }
#endif
    return value;
}

unsigned __int128 read_u128(Buffer *buf)
{
    unsigned __int128 value = 0;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    read_le(buf, (uint8_t *)&value, 16);
#else
    uint8_t temp[16];
    read_le(buf, temp, 16);
    for (int i = 0; i < 16; i++)
    {
        value |= ((unsigned __int128)temp[i]) << (8ULL * i);
    }
#endif
    return value;
}

int8_t read_i8(Buffer *buf)
{
    int8_t value = 0;
    read_le(buf, (uint8_t *)&value, 1);
    return value;
}

int16_t read_i16(Buffer *buf)
{
    return (int16_t)read_u16(buf);
}

int32_t read_i32(Buffer *buf)
{
    return (int32_t)read_u32(buf);
}

int64_t read_i64(Buffer *buf)
{
    return (int64_t)read_u64(buf);
}

__int128 read_i128(Buffer *buf)
{
    return (__int128)read_u128(buf);
}

float read_f32(Buffer *buf)
{
    float value = 0.0f;
    uint32_t bitpattern = read_u32(buf);
    memcpy(&value, &bitpattern, sizeof(float));
    return value;
}

double read_f64(Buffer *buf)
{
    double value = 0.0;
    uint64_t bitpattern = read_u64(buf);
    memcpy(&value, &bitpattern, sizeof(double));
    return value;
}

bool read_bool(Buffer *buf)
{
    return (read_u8(buf) != 0);
}

void read_fixed_array(Buffer *buf, void *out_array,
                      size_t elem_size, size_t length)
{
    read_le(buf, (uint8_t *)out_array, elem_size * length);
}

void read_vec(Buffer *buf, void *out_array,
              size_t elem_size, size_t *out_length,
              ReadFunc read_func)
{
    uint32_t length = read_u32(buf);
    if (out_length)
    {
        *out_length = (size_t)length;
    }
    // If read_func is provided, read each element individually
    if (read_func)
    {
        for (uint32_t i = 0; i < length; i++)
        {
            uint8_t *dest = (uint8_t *)out_array + i * elem_size;
            read_func(buf, dest);
        }
    }
    else
    {
        // Otherwise read raw
        read_le(buf, (uint8_t *)out_array, elem_size * length);
    }
}

bool read_option(Buffer *buf, void *out_data, ReadFunc read_func)
{
    bool is_some = read_bool(buf);
    if (is_some && read_func)
    {
        read_func(buf, out_data);
    }
    return is_some;
}

uint8_t read_enum_variant(Buffer *buf)
{
    return read_u8(buf);
}

void read_enum_data(Buffer *buf, void *out_variant_data, ReadFunc read_func)
{
    if (read_func)
    {
        read_func(buf, out_variant_data);
    }
}

void read_hashmap(Buffer *buf, MapEntry *out_entries,
                  size_t *out_length,
                  ReadFunc key_read_func,
                  ReadFunc val_read_func)
{
    uint32_t length = read_u32(buf);
    if (out_length)
    {
        *out_length = (size_t)length;
    }
    for (uint32_t i = 0; i < length; i++)
    {
        key_read_func(buf, out_entries[i].key);
        val_read_func(buf, out_entries[i].value);
    }
}

void read_hashset(Buffer *buf, void **out_keys,
                  size_t *out_length, ReadFunc key_read_func)
{
    uint32_t length = read_u32(buf);
    if (out_length)
    {
        *out_length = (size_t)length;
    }
    for (uint32_t i = 0; i < length; i++)
    {
        key_read_func(buf, out_keys[i]);
    }
}

/* -----------------------------------------------------
 * Utility
 * ----------------------------------------------------- */

void reset_offset(Buffer *buf)
{
    if (!buf->error)
    {
        buf->offset = 0;
    }
}
