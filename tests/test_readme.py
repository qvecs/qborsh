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
    vector_val: qborsh.Vector[qborsh.U16]
    array_val: qborsh.Array[qborsh.U8, 5]
    map_val: qborsh.Map[qborsh.U8, qborsh.String]

    # Helpers
    pubkey: qborsh.PubKey
    padding: qborsh.Padding[qborsh.Array[qborsh.U8, 10]]


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
    "vector_val": [1, 2, 3, 4, 5],
    "array_val": [1, 2, 3, 4, 5],
    "map_val": {1: "one", 2: "two", 3: "three"},
    "pubkey": "B62qoNf6QJk9kXJfzr6z7Z1J6VrYv6bQfMz7zD7c5Z9M",
    "padding": "Does not matter. This is padding. Will write 0s.",
}


def test_readme():
    encoded = Example.encode(data)
    decoded = Example.decode(encoded)

    # "padding" field doesn't matter for equality, but the rest should match exactly.
    del data["padding"]
    assert decoded == data
