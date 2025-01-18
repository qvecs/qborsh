import typing

import qbase58 as base58

from qborsh.csrc import Buffer
from qborsh.types.base import BorshType

T = typing.TypeVar("T", bound=BorshType)


class PubKey(BorshType):
    def serialize(self, buf: Buffer, value: bytes | str) -> None:
        if isinstance(value, bytes):
            raw_bytes = value
        else:
            raw_bytes = base58.decode(value)
        if len(raw_bytes) != 32:
            raise ValueError("PubKey must be exactly 32 bytes.")
        buf.write_fixed_array(raw_bytes)

    def deserialize(self, buf: Buffer) -> str:
        raw_bytes = buf.read_fixed_array(32)
        return base58.encode(raw_bytes)

    def sizeof(self) -> int:
        return 32


class Padding(BorshType, typing.Generic[T]):
    _PADDING = True

    def __init__(self, element: BorshType):
        self.element = element

        size = self.element.sizeof()
        if size is None:
            raise ValueError("Padding element must have a fixed size.")
        self.element_size = size

    def serialize(self, buf: Buffer, value: typing.Optional[None] = None) -> None:
        buf.write_fixed_array(b"\x00" * self.element_size)

    def deserialize(self, buf: Buffer) -> None:
        buf.read_fixed_array(self.element_size)
        return None

    def sizeof(self) -> int:
        return self.element_size
