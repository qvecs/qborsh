import pytest

import qborsh


class TestOptional:
    opt_u8 = qborsh.Optional[qborsh.U8]

    def test_none_roundtrip(self):
        encoded = self.opt_u8.encode(None)
        out = self.opt_u8.decode(encoded)
        assert out is None

    def test_value_roundtrip(self):
        encoded = self.opt_u8.encode(255)
        out = self.opt_u8.decode(encoded)
        assert out == 255

    def test_nested_error(self):
        with pytest.raises(ValueError):
            self.opt_u8.encode(256)

    def test_optional_string(self):
        opt_str = qborsh.Optional[qborsh.String]
        encoded = opt_str.encode("Hello")
        out = opt_str.decode(encoded)
        assert out == "Hello"


class TestVector:
    vec_u8 = qborsh.Vector[qborsh.U8]
    vec_i16 = qborsh.Vector[qborsh.I16]

    def test_vector_roundtrip(self):
        data = [10, 20, 30]
        encoded = self.vec_u8.encode(data)
        out = self.vec_u8.decode(encoded)
        assert out == data

    def test_vector_negative(self):
        data = [-1, -32768, 32767]
        encoded = self.vec_i16.encode(data)
        out = self.vec_i16.decode(encoded)
        assert out == data


class TestArray:
    arr_u8_3 = qborsh.Array[qborsh.U8, 3]

    def test_array_roundtrip(self):
        data = [1, 2, 3]
        encoded = self.arr_u8_3.encode(data)
        out = self.arr_u8_3.decode(encoded)
        assert out == data

    def test_array_wrong_size(self):
        with pytest.raises(ValueError):
            self.arr_u8_3.encode([1, 2, 3, 4])

    def test_array_wrong_type(self):
        with pytest.raises(TypeError):
            self.arr_u8_3.encode("not a list")


class TestMap:
    map_u8_str = qborsh.Map[qborsh.U8, qborsh.String]

    def test_map_roundtrip(self):
        data = {1: "one", 2: "two"}
        encoded = self.map_u8_str.encode(data)
        out = self.map_u8_str.decode(encoded)
        assert out == data

    def test_map_wrong_type(self):
        with pytest.raises(TypeError):
            self.map_u8_str.encode(["this", "not", "a", "dict"])

    def test_map_key_out_of_range(self):
        with pytest.raises(ValueError):
            self.map_u8_str.encode({999: "big key"})
