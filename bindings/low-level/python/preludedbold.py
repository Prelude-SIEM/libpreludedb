import preludeold

from _preludedbold import *

class PreludeDBError(preludeold.PreludeError):
    def __str__(self):
        if self._strerror:
            return self._strerror
        return preludedb_strerror(self.errno)
