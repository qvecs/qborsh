#ifndef BORSH_H
#define BORSH_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h> // for bool
#include <stdint.h>  // for uint8_t, uint16_t, etc.
#include <stddef.h>  // for size_t

    /*
     * The primary buffer structure, storing data, capacity, size, etc.
     * 'error' indicates an out-of-bounds or out-of-memory condition.
     */
    typedef struct
    {
        uint8_t *data;
        size_t size;
        size_t capacity;
        size_t offset;
        bool error;
    } Buffer;

    /*
     * Function pointer for writing a data object into a Buffer.
     * Expects: write_func(buf, &object).
     */
    typedef void (*WriteFunc)(Buffer *buf, const void *value);

    /*
     * Function pointer for reading a data object from a Buffer.
     * Expects: read_func(buf, &object).
     */
    typedef void (*ReadFunc)(Buffer *buf, void *out_value);

    /*
     * Holds a key-value pair. Useful for writing/reading hashmap data.
     */
    typedef struct
    {
        void *key;
        void *value;
    } MapEntry;

    /* -----------------------------------------------------
     * Error Checking
     * ----------------------------------------------------- */
    bool buffer_has_error(const Buffer *buf);

    /* -----------------------------------------------------
     * Initialization / Cleanup
     * ----------------------------------------------------- */
    void init_buffer(Buffer *buf, size_t initial_capacity);
    void free_buffer(Buffer *buf);

    /* -----------------------------------------------------
     * Write Functions
     * ----------------------------------------------------- */
    void write_u8(Buffer *buf, uint8_t value);
    void write_u16(Buffer *buf, uint16_t value);
    void write_u32(Buffer *buf, uint32_t value);
    void write_u64(Buffer *buf, uint64_t value);
    void write_u128(Buffer *buf, unsigned __int128 value);

    void write_i8(Buffer *buf, int8_t value);
    void write_i16(Buffer *buf, int16_t value);
    void write_i32(Buffer *buf, int32_t value);
    void write_i64(Buffer *buf, int64_t value);
    void write_i128(Buffer *buf, __int128 value);

    void write_f32(Buffer *buf, float value);
    void write_f64(Buffer *buf, double value);
    void write_bool(Buffer *buf, bool value);

    void write_fixed_array(Buffer *buf, const void *array_data,
                           size_t elem_size, size_t length);
    void write_vec(Buffer *buf, const void *elem_data,
                   size_t elem_size, size_t length);
    void write_option(Buffer *buf, const void *data,
                      bool is_some, WriteFunc write_func);
    void write_enum(Buffer *buf, uint8_t variant_index,
                    const void *variant_data, WriteFunc write_func);
    void write_hashmap(Buffer *buf, MapEntry *entries,
                       size_t length,
                       WriteFunc key_write_func,
                       WriteFunc val_write_func);
    void write_hashset(Buffer *buf, void **keys,
                       size_t length, WriteFunc key_write_func);

    /* -----------------------------------------------------
     * Read Functions
     * ----------------------------------------------------- */
    uint8_t read_u8(Buffer *buf);
    uint16_t read_u16(Buffer *buf);
    uint32_t read_u32(Buffer *buf);
    uint64_t read_u64(Buffer *buf);
    unsigned __int128 read_u128(Buffer *buf);

    int8_t read_i8(Buffer *buf);
    int16_t read_i16(Buffer *buf);
    int32_t read_i32(Buffer *buf);
    int64_t read_i64(Buffer *buf);
    __int128 read_i128(Buffer *buf);

    float read_f32(Buffer *buf);
    double read_f64(Buffer *buf);
    bool read_bool(Buffer *buf);

    void read_fixed_array(Buffer *buf, void *out_array,
                          size_t elem_size, size_t length);
    void read_vec(Buffer *buf, void *out_array,
                  size_t elem_size, size_t *out_length,
                  ReadFunc read_func);
    bool read_option(Buffer *buf, void *out_data, ReadFunc read_func);
    uint8_t read_enum_variant(Buffer *buf);
    void read_enum_data(Buffer *buf, void *out_variant_data, ReadFunc read_func);
    void read_hashmap(Buffer *buf, MapEntry *out_entries,
                      size_t *out_length,
                      ReadFunc key_read_func,
                      ReadFunc val_read_func);
    void read_hashset(Buffer *buf, void **out_keys,
                      size_t *out_length, ReadFunc key_read_func);

    /* -----------------------------------------------------
     * Utility
     * ----------------------------------------------------- */
    void reset_offset(Buffer *buf);

#ifdef __cplusplus
}
#endif

#endif /* BORSH_H */
