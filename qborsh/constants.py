import os

from qborsh import csrc


# The default size (in bytes) of buffers used throughout qborsh. If you expect
# very large or very small payloads, you may wish to adjust this. A too-small
# buffer may lead to more frequent resizing; a too-large buffer may waste
# memory on small payloads.
def set_buffer_size(size: int) -> None:
    os.environ["QBORSH_BUFFER_SIZE"] = str(size)


BUFFER_SIZE = int(os.environ.get("QBORSH_BUFFER_SIZE", 512))


# Whether to use a global buffer for faster serialization/deserialization.
# Enabling this can reduce repeated allocations in the underlying C layer,
# providing up to ~20-40% speedup in small- and medium-sized schemas. For large
# schemas, the gain is smaller but still measurable.
#
# The second buffer (GLOBAL_BUFFER_SUB) is optional for the case when you want
# a dedicated global buffer for sub-structures, preventing them from interfering
# with the top-level buffer. See types/collections.py to see how its used.
def set_global_buffer(enable: bool) -> None:
    os.environ["QBORSH_GLOBAL_BUFFER"] = str(enable)


if os.environ.get("QBORSH_GLOBAL_BUFFER", "true").lower() in {"true", "on", "yes"}:
    GLOBAL_BUFFER = csrc.Buffer(BUFFER_SIZE)
    GLOBAL_BUFFER_SUB = csrc.Buffer(BUFFER_SIZE)
else:
    GLOBAL_BUFFER = None
    GLOBAL_BUFFER_SUB = None


# Whether to enable validation (range checks) in the C extension.
def set_validation(enable: bool) -> None:
    os.environ["QBORSH_VALIDATE"] = str(enable)


if os.environ.get("QBORSH_VALIDATE", "true").lower() in {"true", "on", "yes"}:
    csrc.set_validation(True)
else:
    csrc.set_validation(False)

__all__ = [
    "BUFFER_SIZE",
    "GLOBAL_BUFFER",
    "GLOBAL_BUFFER_SUB",
    "set_buffer_size",
    "set_global_buffer",
    "set_validation",
]
