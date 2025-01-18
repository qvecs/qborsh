import typing

from qborsh.csrc import Buffer
from qborsh.types.base import BorshType
from qborsh.utils import dotdict


class Schema(BorshType):
    def __init__(
        self,
        validate: bool = False,
        dotdict: bool = False,
        exact_size: bool = False,
    ):
        self.validate = validate
        self.dotdict = dotdict
        self.exact_size = exact_size

        # Retrieves the instance's type hints.
        fields: dict[str, BorshType] = {}
        hints = typing.get_type_hints(self, globalns=globals(), localns=locals())
        for k, v in hints.items():
            if k.startswith("_"):
                continue
            elif not isinstance(v, BorshType) and not issubclass(v, BorshType):
                raise TypeError(f"Key '{k}' is of type {v}. Must be a subclass of BorshType.")

            if isinstance(v, BorshType):
                fields[k] = v
            else:
                fields[k] = v()

        self.__borsh_fields__ = fields.items()

    def serialize(self, buf: Buffer, data: dict[str, typing.Any]) -> None:
        if self.validate:
            schema_keys = set(i[0] for i in self.__borsh_fields__)
            data_keys = set(data.keys())

            # Ensure all required keys are present.
            missing_keys = schema_keys - data_keys
            if missing_keys:
                raise ValueError(f"Missing keys: {missing_keys}")

            # Ensure no extra keys are present.
            extra_keys = data_keys - schema_keys
            if extra_keys:
                raise ValueError(f"Extra keys: {extra_keys}")

        for field_name, field_type in self.__borsh_fields__:
            field_type.serialize(buf, data[field_name])

    def deserialize(self, buf: Buffer) -> dict[str, typing.Any]:
        data = {}
        for field_name, field_type in self.__borsh_fields__:
            data[field_name] = field_type.deserialize(buf)
            if field_type._PADDING:
                del data[field_name]

        if self.dotdict:
            return dotdict(data)
        return data

    def sizeof(self) -> typing.Optional[int]:
        size = 0
        for _, field_type in self.__borsh_fields__:
            field_size = field_type.sizeof()
            if field_size is None:
                if self.exact_size:
                    return None
                continue
            size += field_size
        return size


@typing.overload
def schema(
    *,
    validate: bool = False,
    dotdict: bool = False,
    exact_size: bool = True,
) -> typing.Callable[[type], Schema]: ...


@typing.overload
def schema(
    _cls: type,
    *,
    validate: bool = False,
    dotdict: bool = False,
    exact_size: bool = True,
) -> Schema: ...


def schema(
    _cls: typing.Optional[type] = None,
    *,
    validate: bool = False,
    dotdict: bool = False,
    exact_size: bool = True,
) -> typing.Callable[[type], Schema] | Schema:
    """
    Flexible decorator. Can be used in two ways:

      1. @qbporsh.schema                   (no parentheses)
      2. @qborsh.schema(validate=True)     (with parentheses)
    """

    def wrap(cls: typing.Type) -> Schema:
        cls = type(cls.__name__, (Schema,), dict(cls.__dict__))
        return cls(validate=validate, dotdict=dotdict, exact_size=exact_size)

    if _cls is None:
        return wrap
    return wrap(_cls)
