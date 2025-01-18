from qborsh import Buffer
from qborsh.types import BorshType


class String(BorshType):
    def serialize(self, buf: Buffer, value: str):
        if not isinstance(value, str):
            raise TypeError("String expects a string input.")
        bytes = value.encode("utf-8")
        buf.write_vec(bytes)

    def deserialize(self, buf: Buffer) -> str:
        return buf.read_vec().decode("utf-8")

    @classmethod
    def sizeof(cls):
        return None


class Bytes(BorshType):
    def serialize(self, buf: Buffer, value: bytes):
        if not isinstance(value, bytes):
            raise TypeError("Bytes expects a bytes input.")
        buf.write_vec(value)

    def deserialize(self, buf: Buffer) -> bytes:
        return buf.read_vec()

    @classmethod
    def sizeof(cls):
        return None
