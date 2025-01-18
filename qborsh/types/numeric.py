from typing import Literal

from qborsh import Buffer
from qborsh.types import BorshType


class _Numeric(BorshType):
    type: Literal["u", "i", "f"]
    bits: int

    def __init__(self):
        self._serialize = getattr(Buffer, f"write_{self.type}{self.bits}")
        self._deserialize = getattr(Buffer, f"read_{self.type}{self.bits}")

    def serialize(self, buf: Buffer, value: int | float) -> None:
        self._serialize(buf, value)

    def deserialize(self, buf: Buffer) -> int | float:
        return self._deserialize(buf)

    @classmethod
    def sizeof(cls) -> int:
        return cls.bits // 8


class U8(_Numeric):
    type = "u"
    bits = 8


class U16(_Numeric):
    type = "u"
    bits = 16


class U32(_Numeric):
    type = "u"
    bits = 32


class U64(_Numeric):
    type = "u"
    bits = 64


class U128(_Numeric):
    type = "u"
    bits = 128


class I8(_Numeric):
    type = "i"
    bits = 8


class I16(_Numeric):
    type = "i"
    bits = 16


class I32(_Numeric):
    type = "i"
    bits = 32


class I64(_Numeric):
    type = "i"
    bits = 64


class I128(_Numeric):
    type = "i"
    bits = 128


class F32(_Numeric):
    type = "f"
    bits = 32


class F64(_Numeric):
    type = "f"
    bits = 64


class Bool(BorshType):
    def serialize(self, buf: Buffer, value: bool | int) -> None:
        if not ((value is True or value is False) or (value == 0 or value == 1)):
            raise TypeError("Bool expects a bool or integer value of 0 or 1.")
        buf.write_bool(bool(value))

    def deserialize(self, buf: Buffer) -> bool:
        return buf.read_bool()

    @classmethod
    def sizeof(cls) -> int:
        return 1
