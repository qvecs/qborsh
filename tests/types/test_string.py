import pytest

import qborsh


class TestString:
    str_type = qborsh.String

    def test_empty_string(self):
        encoded = self.str_type.encode("")
        result = self.str_type.decode(encoded)
        assert result == ""

    def test_basic_string(self):
        data = "Hello, World!"
        encoded = self.str_type.encode(data)
        result = self.str_type.decode(encoded)
        assert result == data

    def test_unicode_string(self):
        data = "こんにちは世界"
        encoded = self.str_type.encode(data)
        result = self.str_type.decode(encoded)
        assert result == data

    def test_wrong_type(self):
        with pytest.raises(TypeError):
            self.str_type.encode(1234)

    def test_sizeof(self):
        assert self.str_type.sizeof() is None


class TestBytes:
    bytes_type = qborsh.Bytes

    def test_empty_bytes(self):
        encoded = self.bytes_type.encode(b"")
        result = self.bytes_type.decode(encoded)
        assert result == b""

    def test_basic_bytes(self):
        data = b"Hello, \x00 World!"
        encoded = self.bytes_type.encode(data)
        result = self.bytes_type.decode(encoded)
        assert result == data

    def test_wrong_type(self):
        with pytest.raises(TypeError):
            self.bytes_type.encode("not bytes")

    def test_sizeof(self):
        assert self.bytes_type.sizeof() is None
