// Yomitan dictionary databases as crengine's JpDictionary, for keeping
// Japanese words together at column breaks (koreader-tategumi fork, see
// crengine's jpwords.h). Fork-only file.

#ifndef JPDICTIONARY_H
#define JPDICTIONARY_H

#include <string>
#include <vector>

#include "jpwords.h"

// Identifies the databases at `paths` (their paths, sizes and modification
// times), as the dictionary opened from them would report in getHash().
uint32_t getJpSqliteDictionaryHash(const std::vector<std::string> & paths);

// Opens the dict.sqlite3 files written by tools/yomitan_convert.py, read-only.
// Returns NULL, with a message in `error`, when one cannot be used.
JpDictionary * newJpSqliteDictionary(const std::vector<std::string> & paths, std::string & error);

#endif
