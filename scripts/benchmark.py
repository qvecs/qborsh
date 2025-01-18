"""
Comparing the following libraries for BORSH (de)serialization performance:

1) borsh_construct      (https://pypi.org/project/borsh-construct/)
2) qborsh               (with validation)
3) qborsh               (without validation)

We test three different schemas:
    1) Simple (3 fields)
    2) Medium (8 fields)
    3) Large (1000 instances of Medium in a vector)

To run:
    virtualenv .venv
    source .venv/bin/activate
    pip install qborsh borsh-construct
    python scripts/benchmark.py
"""

import time
import statistics
import borsh_construct
import qborsh

# Simple Schema
borsh_simple_construct_schema = borsh_construct.CStruct(
    "w" / borsh_construct.U8,
    "x" / borsh_construct.I16,
    "y" / borsh_construct.String,
)


@qborsh.schema(validate=True)
class qborsh_simple_schema_validate:
    w: qborsh.U8
    x: qborsh.I16
    y: qborsh.String


@qborsh.schema
class qborsh_simple_schema:
    w: qborsh.U8
    x: qborsh.I16
    y: qborsh.String


SIMPLE_DATA = {
    "w": 123,
    "x": 30000,
    "y": "hello",
}

# Medium Schema
borsh_medium_construct_schema = borsh_construct.CStruct(
    "w" / borsh_construct.U8,
    "x" / borsh_construct.I16,
    "y" / borsh_construct.String,
    "z" / borsh_construct.U32,
    "a" / borsh_construct.I32,
    "q" / borsh_construct.HashMap(borsh_construct.U8, borsh_construct.I8),
    "b" / borsh_construct.Option(borsh_construct.U64),
    "c" / borsh_construct.Vec(borsh_construct.I8),
)


@qborsh.schema(validate=True)
class qborsh_medium_schema_validate:
    w: qborsh.U8
    x: qborsh.I16
    y: qborsh.String
    z: qborsh.U32
    a: qborsh.I32
    q: qborsh.Map[qborsh.U8, qborsh.I8]
    b: qborsh.Optional[qborsh.U64]
    c: qborsh.Vector[qborsh.I8]


@qborsh.schema
class qborsh_medium_schema:
    w: qborsh.U8
    x: qborsh.I16
    y: qborsh.String
    z: qborsh.U32
    a: qborsh.I32
    q: qborsh.Map[qborsh.U8, qborsh.I8]
    b: qborsh.Optional[qborsh.U64]
    c: qborsh.Vector[qborsh.I8]


MEDIUM_DATA = {
    "w": 123,
    "x": 30000,
    "y": "hello",
    "z": 1234,
    "a": -1234,
    "q": {1: 2, 3: 4},
    "b": 1234,
    "c": [1, 2, 3, 4, 5],
}

# Large Schema
borsh_large_construct_schema_inner = borsh_construct.CStruct(
    "w" / borsh_construct.U8,
    "x" / borsh_construct.I16,
    "y" / borsh_construct.String,
    "z" / borsh_construct.U32,
    "a" / borsh_construct.I32,
    "q" / borsh_construct.HashMap(borsh_construct.U8, borsh_construct.I8),
    "b" / borsh_construct.Option(borsh_construct.U64),
    "c" / borsh_construct.Vec(borsh_construct.I8),
)

borsh_large_construct_schema = borsh_construct.CStruct(
    "q" / borsh_construct.Vec(borsh_large_construct_schema_inner),
)


@qborsh.schema(validate=True)
class qborsh_large_schema_validate_inner:
    w: qborsh.U8
    x: qborsh.I16
    y: qborsh.String
    z: qborsh.U32
    a: qborsh.I32
    q: qborsh.Map[qborsh.U8, qborsh.I8]
    b: qborsh.Optional[qborsh.U64]
    c: qborsh.Vector[qborsh.I8]


@qborsh.schema(validate=True)
class qborsh_large_schema_validate:
    q: qborsh.Vector[qborsh_large_schema_validate_inner]


@qborsh.schema
class qborsh_large_schema_inner:
    w: qborsh.U8
    x: qborsh.I16
    y: qborsh.String
    z: qborsh.U32
    a: qborsh.I32
    q: qborsh.Map[qborsh.U8, qborsh.I8]
    b: qborsh.Optional[qborsh.U64]
    c: qborsh.Vector[qborsh.I8]


@qborsh.schema
class qborsh_large_schema:
    q: qborsh.Vector[qborsh_large_schema_inner]


LARGE_DATA = {
    "q": [
        {
            "w": 123,
            "x": 30000,
            "y": "hello",
            "z": 1234,
            "a": -1234,
            "q": {1: 2, 3: 4},
            "b": 1234,
            "c": [1, 2, 3, 4, 5],
        }
    ]
    * 1000
}


def call_borsh_construct(num_iterations: int, schema, data: dict) -> None:
    for _ in range(num_iterations):
        encoded = schema.build(data)
        _ = schema.parse(encoded)


def call_qborsh(num_iterations: int, schema, data: dict) -> None:
    for _ in range(num_iterations):
        encoded = schema.encode(data)
        _ = schema.decode(encoded)


def time_library(label: str, func, iterations: int) -> float:
    start = time.perf_counter()
    func(iterations)
    end = time.perf_counter()
    return end - start


def run_benchmark(
    loops_simple=20000,
    loops_medium=5000,
    loops_large=10,
    repeats=5,
):
    """
    Repeatedly times BORSH operations over 3 schema sizes (simple, medium, large),
    using borsh_construct, qborsh with validation, and qborsh without validation.
    """

    data_sets = [
        (
            "Simple",
            (borsh_simple_construct_schema, SIMPLE_DATA),
            (qborsh_simple_schema_validate, SIMPLE_DATA),
            (qborsh_simple_schema, SIMPLE_DATA),
            loops_simple,
        ),
        (
            "Medium",
            (borsh_medium_construct_schema, MEDIUM_DATA),
            (qborsh_medium_schema_validate, MEDIUM_DATA),
            (qborsh_medium_schema, MEDIUM_DATA),
            loops_medium,
        ),
        (
            "Large",
            (borsh_large_construct_schema, LARGE_DATA),
            (qborsh_large_schema_validate, LARGE_DATA),
            (qborsh_large_schema, LARGE_DATA),
            loops_large,
        ),
    ]

    print("==== BORSH Encode+Decode Benchmark ====\n")
    for label, borsh_args, qborsh_validate_args, qborsh_args_no_val, iterations in data_sets:
        print(f"--- Schema: {label} ---")
        # borsh_construct
        durations = []
        for _ in range(repeats):
            duration = time_library(
                "borsh-construct",
                lambda i: call_borsh_construct(i, borsh_args[0], borsh_args[1]),
                iterations,
            )
            durations.append(duration)
        avg_time = statistics.mean(durations)
        std_time = statistics.pstdev(durations)
        print(f"borsh-construct\t\t=> avg: {avg_time * 1000:.4f} ms (std: {std_time * 1000:.4f} ms)")

        # qborsh with validation
        durations = []
        for _ in range(repeats):
            duration = time_library(
                "qborsh (with val)",
                lambda i: call_qborsh(i, qborsh_validate_args[0], qborsh_validate_args[1]),
                iterations,
            )
            durations.append(duration)
        avg_time = statistics.mean(durations)
        std_time = statistics.pstdev(durations)
        print(f"qborsh (with val)\t=> avg: {avg_time * 1000:.4f} ms (std: {std_time * 1000:.4f} ms)")

        # qborsh without validation
        durations = []
        for _ in range(repeats):
            duration = time_library(
                "qborsh (without val)",
                lambda i: call_qborsh(i, qborsh_args_no_val[0], qborsh_args_no_val[1]),
                iterations,
            )
            durations.append(duration)
        avg_time = statistics.mean(durations)
        std_time = statistics.pstdev(durations)
        print(f"qborsh (without val)\t=> avg: {avg_time * 1000:.4f} ms (std: {std_time * 1000:.4f} ms)\n")


if __name__ == "__main__":
    run_benchmark(
        loops_simple=20000,
        loops_medium=5000,
        loops_large=10,
        repeats=5,
    )
