import pytest
import qbase58
import qborsh


class TestPubKey:
    pubkey_type = qborsh.PubKey

    def test_serialize_base58(self):
        raw = b"\x02" * 32
        base58_str = qbase58.encode(raw)
        encoded = self.pubkey_type.encode(base58_str)
        decoded = self.pubkey_type.decode(encoded)
        assert decoded == base58_str

    def test_wrong_size_raw(self):
        with pytest.raises(ValueError, match="must be exactly 32 bytes"):
            self.pubkey_type.encode(b"\x01" * 31)

    def test_wrong_size_base58(self):
        raw = b"\x02" * 31
        base58_str = qbase58.encode(raw)
        with pytest.raises(ValueError, match="must be exactly 32 bytes"):
            self.pubkey_type.encode(base58_str)

    def test_sizeof(self):
        assert self.pubkey_type().sizeof() == 32


class TestPadding:
    padding_u8 = qborsh.Padding[qborsh.U8]
    padding_i64 = qborsh.Padding[qborsh.I64]

    def test_padding_u8_serialize(self):
        encoded = self.padding_u8.encode(None)
        assert len(encoded) == 1
        assert encoded[0] == 0

    def test_padding_u8_deserialize(self):
        encoded = self.padding_u8.encode(None)
        result = self.padding_u8.decode(encoded)
        assert result is None

    def test_padding_i64(self):
        encoded = self.padding_i64.encode(None)
        assert len(encoded) == 8
        for b in encoded:
            assert b == 0

        result = self.padding_i64.decode(encoded)
        assert result is None

    def test_padding_variable_type_error(self):
        with pytest.raises(ValueError, match="must have a fixed size"):
            qborsh.Padding(qborsh.String())

    def test_sizeof(self):
        assert self.padding_u8.sizeof() == 1
        assert self.padding_i64.sizeof() == 8
