# Quick Borsh _(qborsh)_

<p align="center">

  <a href="https://github.com/qvecs/qborsh/actions?query=workflow%3ABuild">
    <img src="https://github.com/qvecs/qborsh/workflows/Build/badge.svg">
  </a>

  <a href="https://opensource.org/licenses/MIT">
    <img src="https://img.shields.io/badge/License-MIT-blue.svg">
  </a>
</p>

An optimized Borsh (Binary Object Representation Serializer for Hashing) library for Python, written in C.

## Install

```
pip install qborsh
```

## Usage

### Type Encoding/Decoding

```python
import qborsh

encoded = qborsh.String.encode("Hello world!")
decoded = qborsh.String.decode(encoded)

array_of_u8s = qborsh.Array[qborsh.U8, 5]
encoded = array_of_u8s.encode([1, 2, 3, 4, 5])
decoded = array_of_u8s.decode(encoded)
```

### Schema Encoding/Decoding

```python
import qborsh

@qborsh.schema
class Example:
    # Numeric types
    u8_int: qborsh.U8
    u16_int: qborsh.U16
    u32_int: qborsh.U32
    u64_int: qborsh.U64
    u128_int: qborsh.U128
    i8_int: qborsh.I8
    i16_int: qborsh.I16
    i32_int: qborsh.I32
    i64_int: qborsh.I64
    i128_int: qborsh.I128
    f32_float: qborsh.F32
    f64_float: qborsh.F64

    # Bool, String, Bytes
    bool_val: qborsh.Bool
    string_val: qborsh.String
    bytes_val: qborsh.Bytes

    # Optionals
    optional_val: qborsh.Optional[qborsh.U32]

    # Collections
    set_val: qborsh.Set[qborsh.U8]
    vector_val: qborsh.Vector[qborsh.U16]
    array_val: qborsh.Array[qborsh.U8, 5]
    map_val: qborsh.Map[qborsh.U8, qborsh.String]

    # Helpers
    pubkey: qborsh.PubKey # Solana Base58 Encoded PubKey
    padding: qborsh.Padding[qborsh.Array[qborsh.U8, 10]]

@qborsh.schema
class ExampleNested:
    example: Example

data = {
    "u8_int": 255,
    "u16_int": 65535,
    "u32_int": 4294967295,
    "u64_int": 18446744073709551615,
    "u128_int": 340282366920938463463374607431768211455,
    "i8_int": -128,
    "i16_int": -32768,
    "i32_int": -2147483648,
    "i64_int": -9223372036854775808,
    "i128_int": -170141183460469231731687303715884105728,
    "f32_float": 3.4028234663852886e38,
    "f64_float": 1.7976931348623157e308,
    "bool_val": True,
    "string_val": "Hello, World!",
    "bytes_val": b"Hello, World!",
    "optional_val": 42,
    "set_val": {1, 2, 3, 4, 5},
    "vector_val": [1, 2, 3, 4, 5],
    "array_val": [1, 2, 3, 4, 5],
    "map_val": {1: "one", 2: "two", 3: "three"},
    "pubkey": "B62qoNf6QJk9kXJfzr6z7Z1J6VrYv6bQfMz7zD7c5Z9M",
    # Does not matter. Can be any value, or not present at all during
    # serialization. Must be present during deserialization however.
    "padding": "Does not matter. This is padding. Will write 0s.",
}

encoded = ExampleNested.encode({"example": data})
decoded = ExampleNested.decode(encoded)

# You may also do:
encoded = ExampleNested(example=data)   # kwargs resolve to encoding.
decoded = ExampleNested(encoded)        # args resolve to decoding.
```

There are three params to `qborsh.schema` (defaults in code-block below):

```python
import qborsh

@qborsh.schema(
    validate: bool = False,
    dotdict: bool = False,
    exact_size: bool = True
)
class Example:
    ...
```

* `validate`: Validate data keys during serialization. Checks if there are missing or extra keys when encoding only.
* `dotdict`: Convert decoded dict into a dict with dot access for keys.
* `exact_size`: If `True`, will return `None` when `sizeof()` is called and the schema has variable-sized field(s).

### Package Wide Configuration

#### Buffer Size

The default size (in bytes) of buffers used throughout qborsh. If you expect very large or very small payloads, you may wish to adjust this. A too-small buffer may lead to more frequent resizing; a too-large buffer may waste memory on small payloads. Defaults to `512`.

```python
import qborsh

qborsh.set_buffer_size(1024)
```

or using environment variables:

```bash
QBORSH_BUFFER_SIZE=1024
```

#### Global Buffer

Whether to use a global buffer for faster serialization/deserialization. Enabling this can reduce repeated allocations in the underlying C layer, providing up to ~20-40% speedup in small- and medium-sized schemas. For large schemas, the gain is smaller but still measurable. *This is not thread-safe*. Defaults to `True`. 

```python
import qborsh

qborsh.set_global_buffer(False)
```

or using environment variables:

```bash
QBORSH_GLOBAL_BUFFER=False
```

#### Validate

Whether to enable validation (range checks) in the C extension. Defaults to `True`.

```python
import qborsh

qborsh.set_validate(False)
```

or using environment variables:

```bash
QBORSH_VALIDATE=False
```

## Benchmark

Comparing with another Python qborsh library:

* [borsh-construct](https://pypi.org/project/borsh-construct)

Note that 'without val' means without schema validation.

```bash
--- Schema: Simple ---
borsh-construct         => avg: 258.0539 ms (std: 3.5250 ms)
qborsh (with val)       => avg: 36.9735 ms (std: 0.3608 ms)
qborsh (without val)    => avg: 36.1387 ms (std: 1.8255 ms)

--- Schema: Medium ---
borsh-construct         => avg: 334.0135 ms (std: 2.8085 ms)
qborsh (with val)       => avg: 36.2419 ms (std: 0.7954 ms)
qborsh (without val)    => avg: 36.5602 ms (std: 0.4150 ms)

--- Schema: Large ---
borsh-construct         => avg: 641.5983 ms (std: 21.6217 ms)
qborsh (with val)       => avg: 80.2482 ms (std: 2.8515 ms)
qborsh (without val)    => avg: 67.7815 ms (std: 2.1862 ms)
```

See `scripts/benchmark.py` for benchmarking details.