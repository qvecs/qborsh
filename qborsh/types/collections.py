import typing

from qborsh import Buffer
from qborsh.constants import BUFFER_SIZE, GLOBAL_BUFFER_SUB, GLOBAL_BUFFER_SUB
from qborsh.types import BorshType

T = typing.TypeVar("T", bound=BorshType)
K = typing.TypeVar("K", bound=BorshType)


class Optional(BorshType, typing.Generic[T]):
    def __init__(self, element: BorshType):
        self.element = element

    def serialize(self, buf: Buffer, value: typing.Any):
        if value is None:
            buf.write_bool(False)
        else:
            buf.write_bool(True)
            self.element.serialize(buf, value)

    def deserialize(self, buf: Buffer) -> typing.Any:
        value = buf.read_bool()
        if value:
            return self.element.deserialize(buf)
        return None

    def sizeof(self):
        return None


class Set(BorshType, typing.Generic[T]):
    def __init__(self, element_type: BorshType):
        self.element_type = element_type

    def serialize(self, buf: Buffer, value: typing.Set[typing.Any]) -> None:
        if not isinstance(value, set):
            raise TypeError(f"Expected set. Received: {type(value)}")

        temp_set = set()
        for element in value:
            if GLOBAL_BUFFER_SUB:
                tmp_buf = GLOBAL_BUFFER_SUB
                tmp_buf.reset()
            else:
                tmp_buf = Buffer(BUFFER_SIZE)

            self.element_type.serialize(tmp_buf, element)
            element_bytes = bytes(tmp_buf.data[: tmp_buf.size])

            if not GLOBAL_BUFFER_SUB:
                tmp_buf.free()

            temp_set.add(element_bytes)

        buf.write_hashset(temp_set)

    def deserialize(self, buf: Buffer) -> typing.Set[typing.Any]:
        raw_set = buf.read_hashset()
        typed_set = set()

        for element_bytes in raw_set:
            if GLOBAL_BUFFER_SUB:
                tmp_buf = GLOBAL_BUFFER_SUB
                tmp_buf.reset()
            else:
                tmp_buf = Buffer(len(element_bytes))

            tmp_buf.write_fixed_array(element_bytes)
            tmp_buf.reset_offset()
            element = self.element_type.deserialize(tmp_buf)

            if not GLOBAL_BUFFER_SUB:
                tmp_buf.free()

            typed_set.add(element)

        return typed_set

    def sizeof(self) -> typing.Optional[int]:
        return None


class Vector(BorshType, typing.Generic[T]):
    def __init__(self, element: BorshType):
        self.element = element

    def serialize(self, buf: Buffer, value: list):
        if not isinstance(value, list):
            raise TypeError(f"Expected list. Received: {value}")

        buf.write_u32(len(value))
        for elem in value:
            self.element.serialize(buf, elem)

    def deserialize(self, buf: Buffer) -> typing.List[typing.Any]:
        length = buf.read_u32()

        elements = []
        for _ in range(length):
            elements.append(self.element.deserialize(buf))
        return elements

    def sizeof(self):
        return None


class Array(BorshType, typing.Generic[T, K]):
    def __init__(self, element: BorshType, size: int):
        self.element = element
        self.size = size

    def serialize(self, buf: Buffer, value: list):
        if not isinstance(value, list):
            raise TypeError(f"Expected list. Received: {value}")

        if len(value) != self.size:
            raise ValueError(f"Expected list of size {self.size}. Received: {value} of size {len(value)}")

        for elem in value:
            self.element.serialize(buf, elem)

    def deserialize(self, buf: Buffer) -> typing.List[typing.Any]:
        elements = []
        for _ in range(self.size):
            elements.append(self.element.deserialize(buf))
        return elements

    def sizeof(self) -> typing.Optional[int]:
        element_size = self.element.sizeof()
        if element_size is None:
            return None
        return element_size * self.size


class Map(BorshType, typing.Generic[T, K]):
    def __init__(self, key_type: BorshType, value_type: BorshType):
        self.key_type = key_type
        self.value_type = value_type

    def serialize(self, buf: Buffer, value: dict[typing.Any, typing.Any]) -> None:
        if not isinstance(value, dict):
            raise TypeError(f"Expected dict. Received: {type(value)}")

        temp_dict: dict[bytes, bytes] = {}
        for key_obj, val_obj in value.items():
            if GLOBAL_BUFFER_SUB:
                key_buf = GLOBAL_BUFFER_SUB
                key_buf.reset()
            else:
                key_buf = Buffer(BUFFER_SIZE)
            self.key_type.serialize(key_buf, key_obj)
            key_bytes = bytes(key_buf.data[: key_buf.size])
            if not GLOBAL_BUFFER_SUB:
                key_buf.free()

            if GLOBAL_BUFFER_SUB:
                val_buf = GLOBAL_BUFFER_SUB
                val_buf.reset()
            else:
                val_buf = Buffer(BUFFER_SIZE)
            self.value_type.serialize(val_buf, val_obj)
            val_bytes = bytes(val_buf.data[: val_buf.size])
            if not GLOBAL_BUFFER_SUB:
                val_buf.free()

            temp_dict[key_bytes] = val_bytes

        buf.write_hashmap(temp_dict)

    def deserialize(self, buf: Buffer) -> dict[typing.Any, typing.Any]:
        raw_dict = buf.read_hashmap()
        typed_dict = {}

        for key_bytes, val_bytes in raw_dict.items():
            if GLOBAL_BUFFER_SUB:
                key_buf = GLOBAL_BUFFER_SUB
                key_buf.reset()
            else:
                key_buf = Buffer(len(key_bytes))
            key_buf.write_fixed_array(key_bytes)
            key_buf.reset_offset()
            typed_key = self.key_type.deserialize(key_buf)
            if not GLOBAL_BUFFER_SUB:
                key_buf.free()

            if GLOBAL_BUFFER_SUB:
                val_buf = GLOBAL_BUFFER_SUB
                val_buf.reset()
            else:
                val_buf = Buffer(len(val_bytes))
            val_buf.write_fixed_array(val_bytes)
            val_buf.reset_offset()
            typed_val = self.value_type.deserialize(val_buf)
            if not GLOBAL_BUFFER_SUB:
                val_buf.free()

            typed_dict[typed_key] = typed_val

        return typed_dict

    def sizeof(self) -> typing.Optional[int]:
        return None
