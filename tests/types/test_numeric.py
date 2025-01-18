import pytest

import qborsh


@pytest.mark.parametrize(
    "borsh_type, valid_values, invalid_values, expected_size",
    [
        (qborsh.U8, [0, 255], [-1, 256], 1),
        (qborsh.U16, [0, 65535], [-1, 65536], 2),
        (qborsh.U32, [0, 2**32 - 1], [-1, 2**32], 4),
        (qborsh.U64, [0, 2**64 - 1], [-1, 2**64], 8),
        (qborsh.U128, [0, 2**128 - 1], [-1, 2**128], 16),
        (qborsh.I8, [-128, 127], [-129, 128], 1),
        (qborsh.I16, [-(2**15), 2**15 - 1], [-(2**15) - 1, 2**15], 2),
        (qborsh.I32, [-(2**31), 2**31 - 1], [-(2**31) - 1, 2**31], 4),
        (qborsh.I64, [-(2**63), 2**63 - 1], [-(2**63) - 1, 2**63], 8),
        (qborsh.I128, [-(2**127), 2**127 - 1], [-(2**127) - 1, 2**127], 16),
        (qborsh.F32, [0.0, 3.4028234663852886e38, -3.4028234663852886e38], [], 4),
        (qborsh.F64, [0.0, 1.7976931348623157e308, -1.7976931348623157e308], [], 8),
        (qborsh.Bool, [True, False, 0, 1], ["true", 2], 1),
    ],
)
def test_roundtrip(borsh_type, valid_values, invalid_values, expected_size):
    for val in valid_values:
        encoded = borsh_type.encode(val)
        result = borsh_type.decode(encoded)
        assert result == val, f"Numeric mismatch: wrote {val}, read {result}"

        # Check that the size of the serialized value is as expected.
        assert borsh_type.sizeof() == expected_size

    for val in invalid_values:
        # Bool is a special case that wants TypeError
        if borsh_type is qborsh.Bool:
            with pytest.raises(TypeError):
                borsh_type.encode(val)
        else:
            with pytest.raises((OverflowError, ValueError)):
                borsh_type.encode(val)
