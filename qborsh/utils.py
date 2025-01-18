import copy
import json
from typing import Any, Dict, List, Optional


class dotdict(Dict[str, Any]):
    """
    Dictionary (nested) with dot notation with type hinting.

    Example:
        >>> config = dotdict({'a': 1, 'b': {'c': 2, 'd': 3}})
        >>> config.a
        1
        >>> config.b.c
        2
        >>> config.b.d
        3
    """

    __getattr__: Any = dict.__getitem__
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        for key, value in self.items():
            if isinstance(value, dict):
                self[key] = dotdict(value)

    def dict(self):
        """
        Converts all dotdicts to dicts.
        """
        d = {}
        for key, value in self.items():
            if isinstance(value, dotdict):
                d[key] = value.dict()
            else:
                d[key] = value
        return d

    def deepcopy(self):
        """
        Returns a deep copy of this object by using 'dict' instead of 'dotdict'.
        """
        return dotdict(copy.deepcopy(self.dict()))

    def pprint(self, hide: Optional[List[str]] = None):
        """
        Pretty prints the dictionary.

        Example:
            >>> config = dotdict({'a': 1, 'b': {'c': 2, 'd': 3}})
            >>> config.pprint(hide=['b.c'])
            {
                "a": 1,
                "b": {
                    "d": 3
                }
            }

        Args:
            hide: List of keys to hide in dot notation.
        """
        c = self.deepcopy()
        for key in hide or []:
            keys = key.split(".")
            last_key = keys.pop()
            d = c
            for k in keys:
                d = d[k]
            d[last_key] = "[HIDDEN]"
        print(json.dumps(c, indent=4))
