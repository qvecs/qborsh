#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>
#include <limits.h> // for INT_MAX, etc.
#include "borsh.h"

/*
 * A global flag controlling validation (range checks). If you wish to skip range checks
 * for performance reasons, set this to 0 at runtime via `py_borsh.set_validation(False)`.
 * Otherwise, keep it at 1 to enforce validations.
 */
static int g_validation_enabled = 1;

/*
 * A simple wrapper object to hold a 'Buffer *' from borsh.h
 * and expose it in Python for BORSH-like read/write operations.
 */
typedef struct
{
    PyObject_HEAD Buffer *buf;
} PyBufferObject;

/*
 * Helper to check the underlying buffer for errors (e.g. OOM or out-of-bounds).
 * If an error is found, sets a Python exception and returns -1.
 */
static int CheckBufferError(Buffer *b)
{
    if (!b)
        return -1;
    if (buffer_has_error(b))
    {
        PyErr_SetString(PyExc_RuntimeError, "Buffer encountered an error (OOM or out-of-bounds).");
        return -1;
    }
    return 0;
}

/* -----------------------------------------------------
 * Deallocation / Initialization
 * ----------------------------------------------------- */
static void
PyBuffer_dealloc(PyBufferObject *self)
{
    if (self->buf)
    {
        free_buffer(self->buf);
        free(self->buf);
        self->buf = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
PyBuffer_init(PyBufferObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"capacity", NULL};
    Py_ssize_t capacity_py = 0;

    /*
     * Parse a single integer argument for initial capacity.
     * We'll init the underlying borsh Buffer with that capacity.
     */
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "n", kwlist, &capacity_py))
    {
        return -1;
    }
    if (capacity_py < 0)
    {
        PyErr_SetString(PyExc_ValueError, "capacity must not be negative");
        return -1;
    }

    self->buf = (Buffer *)malloc(sizeof(Buffer));
    if (!self->buf)
    {
        PyErr_NoMemory();
        return -1;
    }
    init_buffer(self->buf, (size_t)capacity_py);
    if (CheckBufferError(self->buf) < 0)
    {
        free(self->buf);
        self->buf = NULL;
        return -1;
    }
    return 0;
}

/*
 * Convenience function to fetch the underlying Buffer pointer.
 * Sets a Python exception if it is NULL.
 */
static inline Buffer *
GetBuffer(PyBufferObject *self)
{
    if (!self->buf)
    {
        PyErr_SetString(PyExc_RuntimeError, "Buffer is NULL");
    }
    return self->buf;
}

/* -----------------------------------------------------
 * Cleanup & Utilities
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_free(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->buf)
    {
        free_buffer(self->buf);
        free(self->buf);
        self->buf = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_reset_offset(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    reset_offset(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_reset(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    if (self->buf)
    {
        self->buf->size = 0;
        self->buf->offset = 0;
        self->buf->error = false;
    }
    Py_RETURN_NONE;
}

/* -----------------------------------------------------
 * Property Accessors
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_get_size(PyBufferObject *self, void *closure)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromSize_t(b->size);
}

static PyObject *
PyBuffer_get_capacity(PyBufferObject *self, void *closure)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromSize_t(b->capacity);
}

static PyObject *
PyBuffer_get_offset(PyBufferObject *self, void *closure)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromSize_t(b->offset);
}

/*
 * Returns a writable memoryview over the entire underlying data array.
 * This includes unused capacity, not just 'size', so use with caution.
 */
static PyObject *
PyBuffer_get_data(PyBufferObject *self, void *closure)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyMemoryView_FromMemory(
        (char *)b->data,
        (Py_ssize_t)b->capacity,
        PyBUF_WRITE);
}

/* ------------------------------------------------------------------
 * Helper macro for validation checks (when g_validation_enabled=1).
 * If g_validation_enabled=0, skip them for speed.
 * ------------------------------------------------------------------ */
#define VALIDATE_OR_RETURN(cond, msg)             \
    if (g_validation_enabled && !(cond))          \
    {                                             \
        PyErr_SetString(PyExc_ValueError, (msg)); \
        return NULL;                              \
    }

/* -----------------------------------------------------
 * Write/Read U8
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_u8(PyBufferObject *self, PyObject *args)
{
    /*
     * We first parse as a PyObject to check sign.
     * If negative => "u8 cannot be negative."
     * Then parse again as unsigned long to confirm range.
     */
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *valobj;
    if (!PyArg_ParseTuple(args, "O", &valobj))
    {
        return NULL;
    }

    long signed_val = PyLong_AsLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(signed_val >= 0, "u8 cannot be negative");

    unsigned long val = PyLong_AsUnsignedLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(val <= 0xFF, "u8 out of range (0..255)");

    write_u8(b, (uint8_t)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_u8(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    uint8_t val = read_u8(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromUnsignedLong(val);
}

/* -----------------------------------------------------
 * Write/Read I8
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_i8(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *valobj;
    if (!PyArg_ParseTuple(args, "O", &valobj))
    {
        return NULL;
    }
    long val = PyLong_AsLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }

    VALIDATE_OR_RETURN(val >= -128 && val <= 127, "i8 out of range (-128..127)");

    write_i8(b, (int8_t)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_i8(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    int8_t val = read_i8(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromLong(val);
}

/* -----------------------------------------------------
 * Write/Read U16
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_u16(PyBufferObject *self, PyObject *args)
{
    /*
     * We parse as PyObject, check sign => if negative => "u16 cannot be negative"
     * Then parse again as unsigned long to confirm range.
     */
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *valobj;
    if (!PyArg_ParseTuple(args, "O", &valobj))
    {
        return NULL;
    }

    long signed_val = PyLong_AsLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(signed_val >= 0, "u16 cannot be negative");

    unsigned long val = PyLong_AsUnsignedLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(val <= 0xFFFF, "u16 out of range (0..65535)");

    write_u16(b, (uint16_t)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_u16(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    uint16_t val = read_u16(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromUnsignedLong(val);
}

/* -----------------------------------------------------
 * Write/Read I16
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_i16(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *valobj;
    if (!PyArg_ParseTuple(args, "O", &valobj))
    {
        return NULL;
    }
    long val = PyLong_AsLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(val >= -32768 && val <= 32767, "i16 out of range (-32768..32767)");

    write_i16(b, (int16_t)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_i16(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    int16_t val = read_i16(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromLong(val);
}

/* -----------------------------------------------------
 * Write/Read U32 / I32
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_u32(PyBufferObject *self, PyObject *args)
{
    /*
     * We parse as PyObject, check sign => if negative => "u32 cannot be negative"
     * Then parse again as unsigned long to confirm range.
     */
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *valobj;
    if (!PyArg_ParseTuple(args, "O", &valobj))
    {
        return NULL;
    }

    long signed_val = PyLong_AsLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(signed_val >= 0, "u32 cannot be negative");

    unsigned long val = PyLong_AsUnsignedLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(val <= 0xFFFFFFFFUL, "u32 out of range (0..4294967295)");

    write_u32(b, (uint32_t)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_u32(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    uint32_t val = read_u32(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromUnsignedLong(val);
}

static PyObject *
PyBuffer_write_i32(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *valobj;
    if (!PyArg_ParseTuple(args, "O", &valobj))
    {
        return NULL;
    }
    long val = PyLong_AsLong(valobj);
    if (PyErr_Occurred())
    {
        return NULL;
    }
    VALIDATE_OR_RETURN(val >= -2147483648L && val <= 2147483647L, "i32 out of range");

    write_i32(b, (int32_t)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_i32(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    int32_t val = read_i32(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromLong(val);
}

/* -----------------------------------------------------
 * Write/Read U64 / I64
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_u64(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *valobj;
    if (!PyArg_ParseTuple(args, "O", &valobj))
    {
        return NULL;
    }

    // If validation is on, check negativity and upper bound:
    if (g_validation_enabled)
    {
        int is_neg = PyObject_RichCompareBool(valobj, PyLong_FromLong(0), Py_LT);
        if (is_neg < 0)
            return NULL;
        if (is_neg == 1)
        {
            PyErr_SetString(PyExc_ValueError, "u64 cannot be negative");
            return NULL;
        }
    }

    // Parse as unsigned long long
    unsigned long long val_ull = 0;
    PyObject *temp_tuple = Py_BuildValue("(O)", valobj);
    if (!temp_tuple)
    {
        return NULL;
    }
    if (!PyArg_ParseTuple(temp_tuple, "K", &val_ull))
    {
        Py_DECREF(temp_tuple);
        return NULL; // Overflow if >= 2^64
    }
    Py_DECREF(temp_tuple);

    if (g_validation_enabled)
    {
        // Check if val_ull > 2^64 - 1
        PyObject *max_u64 = PyLong_FromUnsignedLongLong(0xFFFFFFFFFFFFFFFFULL);
        if (!max_u64)
        {
            return NULL;
        }
        int cmp = PyObject_RichCompareBool(valobj, max_u64, Py_GT);
        Py_DECREF(max_u64);
        if (cmp < 0)
        {
            return NULL;
        }
        if (cmp == 1)
        {
            PyErr_SetString(PyExc_ValueError, "u64 out of range (0..18446744073709551615)");
            return NULL;
        }
    }

    write_u64(b, (uint64_t)val_ull);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_u64(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    uint64_t val = read_u64(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromUnsignedLongLong(val);
}

static PyObject *
PyBuffer_write_i64(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    long long val = 0;
    /*
     * Using format "L" for 64-bit signed long long
     * which also does overflow checks in Python.
     */
    if (!PyArg_ParseTuple(args, "L", &val))
    {
        return NULL;
    }
    if (PyErr_Occurred())
    {
        return NULL;
    }

    if (g_validation_enabled)
    {
        if (val < -9223372036854775807LL - 1 || val > 9223372036854775807LL)
        {
            PyErr_SetString(PyExc_ValueError, "i64 out of range");
            return NULL;
        }
    }

    write_i64(b, (int64_t)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_i64(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    int64_t val = read_i64(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromLongLong(val);
}

/* -----------------------------------------------------
 * Write/Read U128
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_u128(PyBufferObject *self, PyObject *arg)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    if (!PyLong_Check(arg))
    {
        PyErr_SetString(PyExc_TypeError, "Expected int for u128");
        return NULL;
    }

    /*
     * Range check: 0 <= val <= 2^128 - 1, if g_validation_enabled
     */
    if (g_validation_enabled)
    {
        // Check negative
        if (PyObject_RichCompareBool(arg, PyLong_FromLong(0), Py_LT) == 1)
        {
            PyErr_SetString(PyExc_ValueError, "U128 cannot be negative");
            return NULL;
        }

        // Check > 2^128-1
        PyObject *max_u128 = PyLong_FromString("340282366920938463463374607431768211455", NULL, 10);
        if (!max_u128)
            return NULL;
        int cmp = PyObject_RichCompareBool(arg, max_u128, Py_GT);
        Py_DECREF(max_u128);
        if (cmp < 0)
            return NULL;
        if (cmp == 1)
        {
            PyErr_SetString(PyExc_ValueError, "U128 too large (exceeds 2^128 - 1)");
            return NULL;
        }
    }

    unsigned char bytes[16];
    memset(bytes, 0, 16);

#if PY_VERSION_HEX >= 0x030D0000
    // For Python 3.13+ (dev) that expects 6 parameters:
    if (_PyLong_AsByteArray(
            (PyLongObject *)arg,
            bytes,
            16,
            /* little_endian= */ 1,
            /* is_signed= */ 0,
            /* new extra param= */ 1) < 0)
    {
        return NULL;
    }
#else
    // For Python <= 3.12 that expects 5 parameters:
    if (_PyLong_AsByteArray(
            (PyLongObject *)arg,
            bytes,
            16,
            /* little_endian= */ 1,
            /* is_signed= */ 0) < 0)
    {
        return NULL;
    }
#endif

    /* Reconstruct from byte array. */
    unsigned __int128 val = 0;
    for (int i = 0; i < 16; i++)
    {
        val |= ((unsigned __int128)bytes[i]) << (8 * i);
    }

    write_u128(b, val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_u128(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    unsigned __int128 val = read_u128(b);
    if (CheckBufferError(b) < 0)
        return NULL;

    unsigned char bytes[16];
    for (int i = 0; i < 16; i++)
    {
        bytes[i] = (unsigned char)((val >> (8 * i)) & 0xFF);
    }

    return _PyLong_FromByteArray(bytes, 16, 1, 0);
}

/* -----------------------------------------------------
 * Write/Read I128
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_i128(PyBufferObject *self, PyObject *arg)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    if (!PyLong_Check(arg))
    {
        PyErr_SetString(PyExc_TypeError, "Expected int for i128");
        return NULL;
    }

    if (g_validation_enabled)
    {
        // Range: -2^127 <= val <= 2^127 - 1
        PyObject *min_i128 = PyLong_FromString("-170141183460469231731687303715884105728", NULL, 10);
        PyObject *max_i128 = PyLong_FromString("170141183460469231731687303715884105727", NULL, 10);

        if (!min_i128 || !max_i128)
        {
            Py_XDECREF(min_i128);
            Py_XDECREF(max_i128);
            return NULL;
        }

        if (PyObject_RichCompareBool(arg, min_i128, Py_LT) == 1)
        {
            PyErr_SetString(PyExc_ValueError, "I128 out of range (too small)");
            Py_DECREF(min_i128);
            Py_DECREF(max_i128);
            return NULL;
        }
        if (PyObject_RichCompareBool(arg, max_i128, Py_GT) == 1)
        {
            PyErr_SetString(PyExc_ValueError, "I128 out of range (too large)");
            Py_DECREF(min_i128);
            Py_DECREF(max_i128);
            return NULL;
        }
        Py_DECREF(min_i128);
        Py_DECREF(max_i128);
    }

    unsigned char bytes[16];
    memset(bytes, 0, 16);

#if PY_VERSION_HEX >= 0x030D0000
    // For Python 3.13+ (dev) that expects 6 parameters:
    if (_PyLong_AsByteArray((PyLongObject *)arg,
                            bytes,
                            16,
                            /* little_endian= */ 1,
                            /* is_signed= */ 1,
                            /* with_exc= */ 1) < 0)
    {
        return NULL;
    }
#else
    // For Python <= 3.12 that expects 5 parameters:
    if (_PyLong_AsByteArray((PyLongObject *)arg,
                            bytes,
                            16,
                            /* little_endian= */ 1,
                            /* is_signed= */ 1) < 0)
    {
        return NULL;
    }
#endif

    __int128 val = 0;
    for (int i = 0; i < 16; i++)
    {
        val |= ((__int128)bytes[i]) << (8 * i);
    }

    write_i128(b, val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_i128(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    __int128 val = read_i128(b);
    if (CheckBufferError(b) < 0)
        return NULL;

    unsigned char bytes[16];
    for (int i = 0; i < 16; i++)
    {
        bytes[i] = (unsigned char)((val >> (8 * i)) & 0xFF);
    }

    return _PyLong_FromByteArray(bytes, 16, 1, 1);
}

/* -----------------------------------------------------
 * Write/Read F32 / F64
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_f32(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    float val = 0.0f;
    if (!PyArg_ParseTuple(args, "f", &val))
    {
        return NULL;
    }
    write_f32(b, val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_f32(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    float val = read_f32(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyFloat_FromDouble((double)val);
}

static PyObject *
PyBuffer_write_f64(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    double val = 0.0;
    if (!PyArg_ParseTuple(args, "d", &val))
    {
        return NULL;
    }
    write_f64(b, val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_f64(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    double val = read_f64(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyFloat_FromDouble(val);
}

/* -----------------------------------------------------
 * Write/Read Bool
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_bool(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    int val = 0;
    /* "p": parse a bool-like integer (0 or 1) from Python */
    if (!PyArg_ParseTuple(args, "p", &val))
    {
        return NULL;
    }
    write_bool(b, (bool)val);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_bool(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    bool val = read_bool(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    if (val)
    {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

/* -----------------------------------------------------
 * Write/Read Fixed Array
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_fixed_array(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    const char *raw_data = NULL;
    Py_ssize_t data_len = 0;

    /* "y#": parse a bytes-like object into a pointer + length */
    if (!PyArg_ParseTuple(args, "y#", &raw_data, &data_len))
    {
        return NULL;
    }
    if (data_len < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Negative data length");
        return NULL;
    }
    write_fixed_array(b, raw_data, 1, (size_t)data_len);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_fixed_array(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    Py_ssize_t length = 0;
    if (!PyArg_ParseTuple(args, "n", &length))
    {
        return NULL;
    }
    if (length < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Negative length");
        return NULL;
    }
    PyObject *out_bytes = PyBytes_FromStringAndSize(NULL, length);
    if (!out_bytes)
    {
        return NULL;
    }
    char *dest = PyBytes_AsString(out_bytes);

    read_fixed_array(b, dest, 1, (size_t)length);
    if (CheckBufferError(b) < 0)
    {
        Py_DECREF(out_bytes);
        return NULL;
    }
    return out_bytes;
}

/* -----------------------------------------------------
 * Write/Read Vec
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_vec(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    const char *raw_data = NULL;
    Py_ssize_t data_len = 0;

    /* "y#": parse a bytes-like object. We'll store length + bytes. */
    if (!PyArg_ParseTuple(args, "y#", &raw_data, &data_len))
    {
        return NULL;
    }
    if (data_len < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Negative data length");
        return NULL;
    }
    write_vec(b, raw_data, 1, (size_t)data_len);
    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_vec(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    uint32_t length_prefix = read_u32(b);
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *out_bytes = PyBytes_FromStringAndSize(NULL, length_prefix);
    if (!out_bytes)
    {
        return NULL;
    }
    char *dest = PyBytes_AsString(out_bytes);
    read_fixed_array(b, dest, 1, length_prefix);
    if (CheckBufferError(b) < 0)
    {
        Py_DECREF(out_bytes);
        return NULL;
    }
    return out_bytes;
}

/* -----------------------------------------------------
 * Write/Read Option
 * ----------------------------------------------------- */
/*
 * We store:
 *   - 1 byte bool is_some
 *   - if is_some, then a u32 length + raw bytes
 */
static PyObject *
PyBuffer_write_option(PyBufferObject *self, PyObject *arg)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    if (arg == Py_None)
    {
        write_bool(b, false);
        if (CheckBufferError(b) < 0)
            return NULL;
    }
    else if (PyBytes_Check(arg))
    {
        write_bool(b, true);
        if (CheckBufferError(b) < 0)
            return NULL;

        Py_ssize_t data_len = PyBytes_Size(arg);
        if (data_len < 0)
        {
            PyErr_SetString(PyExc_ValueError, "Negative data length");
            return NULL;
        }
        write_u32(b, (uint32_t)data_len);
        if (CheckBufferError(b) < 0)
            return NULL;

        const char *raw_data = PyBytes_AsString(arg);
        write_fixed_array(b, raw_data, 1, (size_t)data_len);
        if (CheckBufferError(b) < 0)
            return NULL;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, "Expected None or bytes");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_option(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    bool is_some = read_bool(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    if (!is_some)
    {
        Py_RETURN_NONE;
    }
    uint32_t length_prefix = read_u32(b);
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *out_bytes = PyBytes_FromStringAndSize(NULL, length_prefix);
    if (!out_bytes)
        return NULL;
    char *dest = PyBytes_AsString(out_bytes);

    read_fixed_array(b, dest, 1, length_prefix);
    if (CheckBufferError(b) < 0)
    {
        Py_DECREF(out_bytes);
        return NULL;
    }
    return out_bytes;
}

/* -----------------------------------------------------
 * Write/Read Enum
 * ----------------------------------------------------- */
static PyObject *
PyBuffer_write_enum(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    unsigned long variant_idx;
    PyObject *maybe_data = NULL;

    if (!PyArg_ParseTuple(args, "k|O", &variant_idx, &maybe_data))
    {
        return NULL;
    }
    if (g_validation_enabled && variant_idx > 255)
    {
        PyErr_SetString(PyExc_ValueError, "Variant index out of u8 range (0..255)");
        return NULL;
    }
    write_u8(b, (uint8_t)variant_idx);
    if (CheckBufferError(b) < 0)
        return NULL;

    if (!maybe_data || maybe_data == Py_None)
    {
        /* No payload */
    }
    else if (PyBytes_Check(maybe_data))
    {
        Py_ssize_t data_len = PyBytes_Size(maybe_data);
        const char *raw_data = PyBytes_AsString(maybe_data);
        write_fixed_array(b, raw_data, 1, data_len);
        if (CheckBufferError(b) < 0)
            return NULL;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, "Expected bytes or None for enum payload");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_enum_variant(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    uint8_t variant = read_u8(b);
    if (CheckBufferError(b) < 0)
        return NULL;
    return PyLong_FromUnsignedLong(variant);
}

static PyObject *
PyBuffer_read_enum_data(PyBufferObject *self, PyObject *args)
{
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    Py_ssize_t length = 0;
    if (!PyArg_ParseTuple(args, "n", &length))
    {
        return NULL;
    }
    if (length < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Negative length");
        return NULL;
    }

    PyObject *out_bytes = PyBytes_FromStringAndSize(NULL, length);
    if (!out_bytes)
        return NULL;
    char *dest = PyBytes_AsString(out_bytes);

    read_fixed_array(b, dest, 1, length);
    if (CheckBufferError(b) < 0)
    {
        Py_DECREF(out_bytes);
        return NULL;
    }
    return out_bytes;
}

/* -----------------------------------------------------
 * HashMap / HashSet Implementation
 * ----------------------------------------------------- */

static PyObject *
PyBuffer_write_hashmap(PyBufferObject *self, PyObject *args)
{
    /* Expect one argument: a Python dict {bytes: bytes} */
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *dict = NULL;
    if (!PyArg_ParseTuple(args, "O!", &PyDict_Type, &dict))
    {
        return NULL; /* Expecting a dict */
    }

    /* Get the dict size */
    Py_ssize_t dict_size = PyDict_Size(dict);
    if (dict_size < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Negative dict size?");
        return NULL;
    }

    /* Write the length of the dict as u32 */
    if (g_validation_enabled && (uint64_t)dict_size > 0xFFFFFFFFULL)
    {
        PyErr_SetString(PyExc_ValueError, "Too many dict items for u32 length");
        return NULL;
    }
    write_u32(b, (uint32_t)dict_size);
    if (CheckBufferError(b) < 0)
        return NULL;

    /*
     * For each key-value pair in the dict:
     *   - write key as a 'vec<u8>' (u32 length + raw bytes)
     *   - write value as a 'vec<u8>'
     */
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while (PyDict_Next(dict, &pos, &key, &value))
    {
        if (!PyBytes_Check(key) || !PyBytes_Check(value))
        {
            PyErr_SetString(PyExc_TypeError, "Keys/values must be bytes");
            return NULL;
        }
        Py_ssize_t klen = PyBytes_GET_SIZE(key);
        Py_ssize_t vlen = PyBytes_GET_SIZE(value);

        if (klen < 0 || vlen < 0)
        {
            PyErr_SetString(PyExc_ValueError, "Negative length in dict");
            return NULL;
        }
        if (g_validation_enabled)
        {
            if ((uint64_t)klen > 0xFFFFFFFFULL)
            {
                PyErr_SetString(PyExc_ValueError, "Key too large for u32 length");
                return NULL;
            }
            if ((uint64_t)vlen > 0xFFFFFFFFULL)
            {
                PyErr_SetString(PyExc_ValueError, "Value too large for u32 length");
                return NULL;
            }
        }

        write_u32(b, (uint32_t)klen);
        if (CheckBufferError(b) < 0)
            return NULL;
        const char *kraw = PyBytes_AS_STRING(key);
        write_fixed_array(b, kraw, 1, (size_t)klen);
        if (CheckBufferError(b) < 0)
            return NULL;

        write_u32(b, (uint32_t)vlen);
        if (CheckBufferError(b) < 0)
            return NULL;
        const char *vraw = PyBytes_AS_STRING(value);
        write_fixed_array(b, vraw, 1, (size_t)vlen);
        if (CheckBufferError(b) < 0)
            return NULL;
    }

    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_hashmap(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    /* Returns a dict of {bytes: bytes} */
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    /* Read the dict size (u32) */
    uint32_t dict_size = read_u32(b);
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *result = PyDict_New();
    if (!result)
        return NULL;

    /* For each entry, read key (vec<u8>) and value (vec<u8>) */
    for (uint32_t i = 0; i < dict_size; i++)
    {
        uint32_t klen = read_u32(b);
        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(result);
            return NULL;
        }
        PyObject *key_bytes = PyBytes_FromStringAndSize(NULL, klen);
        if (!key_bytes)
        {
            Py_DECREF(result);
            return NULL;
        }
        char *kbuf = PyBytes_AsString(key_bytes);
        read_fixed_array(b, kbuf, 1, klen);
        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(key_bytes);
            Py_DECREF(result);
            return NULL;
        }

        uint32_t vlen = read_u32(b);
        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(key_bytes);
            Py_DECREF(result);
            return NULL;
        }
        PyObject *val_bytes = PyBytes_FromStringAndSize(NULL, vlen);
        if (!val_bytes)
        {
            Py_DECREF(key_bytes);
            Py_DECREF(result);
            return NULL;
        }
        char *vbuf = PyBytes_AsString(val_bytes);
        read_fixed_array(b, vbuf, 1, vlen);
        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(key_bytes);
            Py_DECREF(val_bytes);
            Py_DECREF(result);
            return NULL;
        }

        /* Insert into the dict */
        if (PyDict_SetItem(result, key_bytes, val_bytes) < 0)
        {
            Py_DECREF(key_bytes);
            Py_DECREF(val_bytes);
            Py_DECREF(result);
            return NULL;
        }
        Py_DECREF(key_bytes);
        Py_DECREF(val_bytes);
    }

    return result;
}

static PyObject *
PyBuffer_write_hashset(PyBufferObject *self, PyObject *args)
{
    /* Expect one argument: a Python set of bytes */
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *pyset = NULL;
    if (!PyArg_ParseTuple(args, "O!", &PySet_Type, &pyset))
    {
        return NULL; /* Expecting a set */
    }

    Py_ssize_t set_size = PySet_Size(pyset);
    if (set_size < 0)
    {
        PyErr_SetString(PyExc_ValueError, "Negative set size?");
        return NULL;
    }

    /* Write length (u32) */
    if (g_validation_enabled && (uint64_t)set_size > 0xFFFFFFFFULL)
    {
        PyErr_SetString(PyExc_ValueError, "Too many set elements for u32 length");
        return NULL;
    }
    write_u32(b, (uint32_t)set_size);
    if (CheckBufferError(b) < 0)
        return NULL;

    /*
     * For each element in the set:
     *  - write as a 'vec<u8>' (length + raw bytes)
     */
    PyObject *iterator = PyObject_GetIter(pyset);
    if (!iterator)
        return NULL;

    PyObject *item;
    while ((item = PyIter_Next(iterator)))
    {
        if (!PyBytes_Check(item))
        {
            Py_DECREF(item);
            Py_DECREF(iterator);
            PyErr_SetString(PyExc_TypeError, "HashSet elements must be bytes");
            return NULL;
        }
        Py_ssize_t length = PyBytes_GET_SIZE(item);
        if (length < 0)
        {
            Py_DECREF(item);
            Py_DECREF(iterator);
            PyErr_SetString(PyExc_ValueError, "Negative element length");
            return NULL;
        }
        if (g_validation_enabled && (uint64_t)length > 0xFFFFFFFFULL)
        {
            Py_DECREF(item);
            Py_DECREF(iterator);
            PyErr_SetString(PyExc_ValueError, "Element too large for u32 length");
            return NULL;
        }

        write_u32(b, (uint32_t)length);
        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(item);
            Py_DECREF(iterator);
            return NULL;
        }
        const char *raw = PyBytes_AsString(item);
        write_fixed_array(b, raw, 1, (size_t)length);
        Py_DECREF(item);

        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(iterator);
            return NULL;
        }
    }
    Py_DECREF(iterator);
    if (PyErr_Occurred())
    {
        /* Iteration error */
        return NULL;
    }

    if (CheckBufferError(b) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
PyBuffer_read_hashset(PyBufferObject *self, PyObject *Py_UNUSED(ignored))
{
    /* Returns a set of bytes */
    Buffer *b = GetBuffer(self);
    if (!b)
        return NULL;
    if (CheckBufferError(b) < 0)
        return NULL;

    uint32_t set_size = read_u32(b);
    if (CheckBufferError(b) < 0)
        return NULL;

    PyObject *result = PySet_New(NULL);
    if (!result)
        return NULL;

    /*
     * For each element:
     *   - read length (u32), read raw bytes
     */
    for (uint32_t i = 0; i < set_size; i++)
    {
        uint32_t length = read_u32(b);
        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(result);
            return NULL;
        }
        PyObject *elem_bytes = PyBytes_FromStringAndSize(NULL, length);
        if (!elem_bytes)
        {
            Py_DECREF(result);
            return NULL;
        }
        char *buf = PyBytes_AsString(elem_bytes);
        read_fixed_array(b, buf, 1, length);
        if (CheckBufferError(b) < 0)
        {
            Py_DECREF(elem_bytes);
            Py_DECREF(result);
            return NULL;
        }

        /* Insert into the set */
        if (PySet_Add(result, elem_bytes) < 0)
        {
            Py_DECREF(elem_bytes);
            Py_DECREF(result);
            return NULL;
        }
        Py_DECREF(elem_bytes);
    }

    return result;
}

/* -----------------------------------------------------
 * Buffer Methods
 * ----------------------------------------------------- */
static PyObject *
PyBorsh_set_validation(PyObject *self, PyObject *args)
{
    int val = 1; // default to "True"
    if (!PyArg_ParseTuple(args, "p", &val))
        return NULL;

    g_validation_enabled = (val != 0);
    Py_RETURN_NONE;
}

/* -----------------------------------------------------
 * Method Table
 * ----------------------------------------------------- */
static PyMethodDef PyBuffer_methods[] = {
    {"free", (PyCFunction)PyBuffer_free, METH_NOARGS, ""},
    {"reset", (PyCFunction)PyBuffer_reset, METH_NOARGS, ""},
    {"reset_offset", (PyCFunction)PyBuffer_reset_offset, METH_NOARGS, ""},

    {"write_u8", (PyCFunction)PyBuffer_write_u8, METH_VARARGS, ""},
    {"read_u8", (PyCFunction)PyBuffer_read_u8, METH_NOARGS, ""},

    {"write_i8", (PyCFunction)PyBuffer_write_i8, METH_VARARGS, ""},
    {"read_i8", (PyCFunction)PyBuffer_read_i8, METH_NOARGS, ""},

    {"write_u16", (PyCFunction)PyBuffer_write_u16, METH_VARARGS, ""},
    {"read_u16", (PyCFunction)PyBuffer_read_u16, METH_NOARGS, ""},

    {"write_i16", (PyCFunction)PyBuffer_write_i16, METH_VARARGS, ""},
    {"read_i16", (PyCFunction)PyBuffer_read_i16, METH_NOARGS, ""},

    {"write_u32", (PyCFunction)PyBuffer_write_u32, METH_VARARGS, ""},
    {"read_u32", (PyCFunction)PyBuffer_read_u32, METH_NOARGS, ""},

    {"write_i32", (PyCFunction)PyBuffer_write_i32, METH_VARARGS, ""},
    {"read_i32", (PyCFunction)PyBuffer_read_i32, METH_NOARGS, ""},

    {"write_u64", (PyCFunction)PyBuffer_write_u64, METH_VARARGS, ""},
    {"read_u64", (PyCFunction)PyBuffer_read_u64, METH_NOARGS, ""},

    {"write_i64", (PyCFunction)PyBuffer_write_i64, METH_VARARGS, ""},
    {"read_i64", (PyCFunction)PyBuffer_read_i64, METH_NOARGS, ""},

    {"write_u128", (PyCFunction)PyBuffer_write_u128, METH_O, ""},
    {"read_u128", (PyCFunction)PyBuffer_read_u128, METH_NOARGS, ""},

    {"write_i128", (PyCFunction)PyBuffer_write_i128, METH_O, ""},
    {"read_i128", (PyCFunction)PyBuffer_read_i128, METH_NOARGS, ""},

    {"write_f32", (PyCFunction)PyBuffer_write_f32, METH_VARARGS, ""},
    {"read_f32", (PyCFunction)PyBuffer_read_f32, METH_NOARGS, ""},

    {"write_f64", (PyCFunction)PyBuffer_write_f64, METH_VARARGS, ""},
    {"read_f64", (PyCFunction)PyBuffer_read_f64, METH_NOARGS, ""},

    {"write_bool", (PyCFunction)PyBuffer_write_bool, METH_VARARGS, ""},
    {"read_bool", (PyCFunction)PyBuffer_read_bool, METH_NOARGS, ""},

    {"write_fixed_array", (PyCFunction)PyBuffer_write_fixed_array, METH_VARARGS, ""},
    {"read_fixed_array", (PyCFunction)PyBuffer_read_fixed_array, METH_VARARGS, ""},

    {"write_vec", (PyCFunction)PyBuffer_write_vec, METH_VARARGS, ""},
    {"read_vec", (PyCFunction)PyBuffer_read_vec, METH_NOARGS, ""},

    {"write_option", (PyCFunction)PyBuffer_write_option, METH_O, ""},
    {"read_option", (PyCFunction)PyBuffer_read_option, METH_NOARGS, ""},

    {"write_enum", (PyCFunction)PyBuffer_write_enum, METH_VARARGS, ""},
    {"read_enum_variant", (PyCFunction)PyBuffer_read_enum_variant, METH_NOARGS, ""},
    {"read_enum_data", (PyCFunction)PyBuffer_read_enum_data, METH_VARARGS, ""},

    {"write_hashmap", (PyCFunction)PyBuffer_write_hashmap, METH_VARARGS, ""},
    {"read_hashmap", (PyCFunction)PyBuffer_read_hashmap, METH_NOARGS, ""},
    {"write_hashset", (PyCFunction)PyBuffer_write_hashset, METH_VARARGS, ""},
    {"read_hashset", (PyCFunction)PyBuffer_read_hashset, METH_NOARGS, ""},

    {NULL, NULL, 0, NULL}};

/* -----------------------------------------------------
 * Property Table
 * ----------------------------------------------------- */
static PyGetSetDef PyBuffer_getset[] = {
    {"size", (getter)PyBuffer_get_size, NULL, NULL, NULL},
    {"capacity", (getter)PyBuffer_get_capacity, NULL, NULL, NULL},
    {"offset", (getter)PyBuffer_get_offset, NULL, NULL, NULL},
    {"data", (getter)PyBuffer_get_data, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}};

/* -----------------------------------------------------
 * Buffer Type
 * ----------------------------------------------------- */
static PyTypeObject PyBufferType = {
    PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "py_borsh.Buffer",
    .tp_basicsize = sizeof(PyBufferObject),
    .tp_dealloc = (destructor)PyBuffer_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "Python wrapper around borsh Buffer",
    .tp_methods = PyBuffer_methods,
    .tp_getset = PyBuffer_getset,
    .tp_init = (initproc)PyBuffer_init,
    .tp_new = PyType_GenericNew,
};

/* -----------------------------------------------------
 * Module-level method table
 * ----------------------------------------------------- */

/*
 * set_validation( bool ) -> None
 *
 * Enable or disable numeric range checks at runtime.
 *  - True => enforce checks (default)
 *  - False => skip checks for performance
 */
static PyMethodDef module_methods[] = {
    {"set_validation", PyBorsh_set_validation, METH_VARARGS,
     "Enable or disable numeric range checks.\n\n"
     "Usage:\n"
     "  import py_borsh\n"
     "  py_borsh.set_validation(True)   # enable checks\n"
     "  py_borsh.set_validation(False)  # disable checks\n"},
    {NULL, NULL, 0, NULL}};

/* -----------------------------------------------------
 * Module Definition
 * ----------------------------------------------------- */
static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "py_borsh", /* m_name */
    "C extension for BORSH serialization/deserialization via borsh.h/c (with runtime validation toggle)",
    -1, /* m_size */
    module_methods,
};

PyMODINIT_FUNC
PyInit_py_borsh(void)
{
    PyObject *m;

    if (PyType_Ready(&PyBufferType) < 0)
    {
        return NULL;
    }
    m = PyModule_Create(&moduledef);
    if (!m)
    {
        return NULL;
    }
    Py_INCREF(&PyBufferType);
    if (PyModule_AddObject(m, "Buffer", (PyObject *)&PyBufferType) < 0)
    {
        Py_DECREF(&PyBufferType);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
