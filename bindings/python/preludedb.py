import prelude

from _preludedb import *

class PreludeDBError(prelude.PreludeError):
    def __str__(self):
        if self._strerror:
            return self._strerror
        return preludedb_strerror(self.errno)
