#ifndef _LIBPRELUDE_PRELUDEDB_PATH_SELECTION_HXX
#define _LIBPRELUDE_PRELUDEDB_PATH_SELECTION_HXX

#include "preludedb-path-selection.h"

namespace PreludeDB {
        class PathSelection {
            private:
                preludedb_path_selection_t *_ps;

            public:
                ~PathSelection();
                PathSelection();
                PathSelection(const PathSelection &selection);
                PathSelection(const std::vector<std::string> &selection);

                operator preludedb_path_selection_t *() const;
                PathSelection &operator=(const PathSelection &selection);
        };
};

#endif
