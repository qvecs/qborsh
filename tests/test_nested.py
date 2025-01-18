import pytest

import qborsh


@qborsh.schema
class inner1:
    x: qborsh.U8
    y: qborsh.String


@qborsh.schema
class inner2:
    a: qborsh.I16
    b: qborsh.Optional[qborsh.Bool]


@qborsh.schema
class OuterSchema:
    w: qborsh.U16
    inner: inner1
    inner2: inner2
    list_inner: qborsh.Vector[inner1]


def test_nested_roundtrip():
    """
    Ensure that nested schemas round-trip correctly, using .encode()/.decode().
    """
    data = {
        "w": 65535,
        "inner": {
            "x": 255,
            "y": "nested-data",
        },
        "inner2": {
            "a": -123,
            "b": True,
        },
        "list_inner": [
            {"x": 1, "y": "one"},
            {"x": 2, "y": "two"},
            {"x": 3, "y": "three"},
        ],
    }

    encoded = OuterSchema.encode(data)
    decoded = OuterSchema.decode(encoded)

    assert decoded["w"] == 65535
    assert decoded["inner"]["x"] == 255
    assert decoded["inner"]["y"] == "nested-data"
    assert decoded["inner2"]["a"] == -123
    assert decoded["inner2"]["b"] is True
    assert len(decoded["list_inner"]) == 3
    assert decoded["list_inner"][0]["x"] == 1
    assert decoded["list_inner"][0]["y"] == "one"
    assert decoded["list_inner"][1]["x"] == 2
    assert decoded["list_inner"][1]["y"] == "two"
    assert decoded["list_inner"][2]["x"] == 3
    assert decoded["list_inner"][2]["y"] == "three"


def test_nested_missing_field():
    data = {
        "w": 1000,
        "inner2": {
            "a": -50,
            "b": False,
        },
        "list_inner": [],
    }
    with pytest.raises(KeyError):
        OuterSchema.encode(data)


def test_nested_out_of_range():
    data = {
        "w": 65536,  # out of range for U16
        "inner": {
            "x": 999,  # out of range for U8
            "y": "hello",
        },
        "inner2": {"a": 100, "b": None},
        "list_inner": [],
    }
    with pytest.raises(ValueError):
        OuterSchema.encode(data)
