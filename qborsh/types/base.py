from __future__ import annotations

import abc
import typing

from qborsh.constants import BUFFER_SIZE, GLOBAL_BUFFER
from qborsh.csrc import Buffer


class BorshTypeMeta(abc.ABCMeta):
    def __getitem__(cls, params) -> BorshType:
        if not issubclass(cls, BorshType):
            raise TypeError(f"Type {cls} is not a subclass of BorshType. Cannot use subscript syntax.")

        if not isinstance(params, tuple):
            params = (params,)

        # Resolve any type hints to BorshType instances.
        name = f"{cls.__name__}[{', '.join(repr(p) for p in params)}]"
        new_cls = type(name, (cls,), {"__args__": params})
        orig_init = new_cls.__init__

        def __init__(self, *init_args, **init_kwargs):
            if not init_args and not init_kwargs:
                real_args = []
                for p in self.__args__:
                    if isinstance(p, type) and issubclass(p, BorshType):
                        real_args.append(p())
                    else:
                        real_args.append(p)
                orig_init(self, *real_args, **init_kwargs)
            else:
                orig_init(self, *init_args, **init_kwargs)

        # Initialize the new class with resolved type hints.
        new_cls.__init__ = __init__
        return new_cls()  # type: ignore


class BorshType(abc.ABC, metaclass=BorshTypeMeta):
    """
    Abstract base for all Borsh types.
    """

    _SINGLETON: BorshType | None = None
    """
    Singleton instance for speed optimization.
    """

    _PADDING: bool = False
    """
    Special flag to indicate that this type is padding.
    """

    @abc.abstractmethod
    def serialize(self, buf: Buffer, value: typing.Any) -> None:
        """
        Write `value` into `buf`.
        """

    @abc.abstractmethod
    def deserialize(self, buf: Buffer) -> typing.Any:
        """
        Read and return a value from `buf`.
        """

    @abc.abstractmethod
    def sizeof(cls_or_self) -> typing.Optional[int]:
        """
        Return the fixed size of this type if known, else None if variable.

        Can be implemented as a classmethod or an instance method. Will
        depedent if the type being implemented requires inputs to determine
        the size. For example:

            1. qborsh.U32.sizeof()             (classmethod)
            2. qborsh.Vector[U32].sizeof()     (instance method)
        """

    @classmethod
    def encode(cls, value: typing.Any) -> bytes:
        if not cls._SINGLETON:
            cls._SINGLETON = cls()

        self = cls._SINGLETON

        if GLOBAL_BUFFER:
            buf = GLOBAL_BUFFER
            buf.reset()
        else:
            size = self.sizeof() or BUFFER_SIZE
            buf = Buffer(size)

        self.serialize(buf, value)
        data = bytes(buf.data[: buf.size])

        if not GLOBAL_BUFFER:
            buf.free()

        return data

    @classmethod
    def decode(cls, data: bytes) -> typing.Any:
        if not cls._SINGLETON:
            cls._SINGLETON = cls()

        self = cls._SINGLETON

        if GLOBAL_BUFFER:
            buf = GLOBAL_BUFFER
            buf.reset()
        else:
            size = self.sizeof() or BUFFER_SIZE
            buf = Buffer(size)

        buf.write_fixed_array(data)

        value = self.deserialize(buf)

        if not GLOBAL_BUFFER:
            buf.free()

        return value

    def __call__(self, *args, **kwargs) -> dict[str, typing.Any] | bytes:
        if args and kwargs:
            raise TypeError("Cannot provide both args and kwargs.")
        elif args:
            return self.decode(args[0])
        elif kwargs:
            return self.encode(kwargs)
        else:
            raise TypeError("Must provide either args or kwargs.")
