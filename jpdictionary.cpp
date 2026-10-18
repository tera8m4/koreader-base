// Yomitan dictionary databases as crengine's JpDictionary, for keeping
// Japanese words together at column breaks (koreader-tategumi fork, see
// crengine's jpwords.h). Fork-only file.
//
// Words are looked up in the databases the Yomitan plugin already uses, by
// their indexes on terms.expression and terms.reading, so no separate word
// list is needed. crengine fetches all keys sharing their first two chars at
// once and caches them.

#include "jpdictionary.h"

#include <sqlite3.h>
#include <sys/stat.h>

namespace {

// The rows of a key or of a key prefix: expressions, and readings that differ
// from their expression (a reading equal to it is the same key).
const char * EXACT_SQL =
    "SELECT expression, 1, definition_tags, rules, term_tags FROM terms WHERE expression = ?1 "
    "UNION ALL "
    "SELECT reading, 0, definition_tags, rules, term_tags FROM terms WHERE reading = ?1 AND reading <> expression";
const char * PREFIX_SQL =
    "SELECT expression, 1, definition_tags, rules, term_tags FROM terms WHERE expression >= ?1 AND expression < ?2 "
    "UNION ALL "
    "SELECT reading, 0, definition_tags, rules, term_tags FROM terms WHERE reading >= ?1 AND reading < ?2 AND reading <> expression";

uint32_t hashBytes(uint32_t hash, const void * data, size_t len) {
    // FNV-1a
    const unsigned char * p = (const unsigned char *)data;
    for ( size_t i = 0; i < len; i++ ) {
        hash ^= p[i];
        hash *= 16777619u;
    }
    return hash;
}

std::string columnText(sqlite3_stmt * stmt, int column) {
    const unsigned char * text = sqlite3_column_text(stmt, column);
    return text ? std::string((const char *)text, sqlite3_column_bytes(stmt, column)) : std::string();
}

class SqliteDictionary : public JpDictionary {
public:
    explicit SqliteDictionary(uint32_t hash) : _hash(hash) {}

    ~SqliteDictionary() {
        for ( size_t i = 0; i < _databases.size(); i++ ) {
            sqlite3_finalize(_databases[i].exact);
            sqlite3_finalize(_databases[i].prefix);
            sqlite3_close(_databases[i].db);
        }
    }

    bool open(const std::string & path, std::string & error) {
        Database database = { NULL, NULL, NULL };
        int rc = sqlite3_open_v2(path.c_str(), &database.db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL);
        if ( rc == SQLITE_OK )
            rc = sqlite3_prepare_v2(database.db, EXACT_SQL, -1, &database.exact, NULL);
        if ( rc == SQLITE_OK )
            rc = sqlite3_prepare_v2(database.db, PREFIX_SQL, -1, &database.prefix, NULL);
        if ( rc != SQLITE_OK ) {
            error = path + ": " + (database.db ? sqlite3_errmsg(database.db) : sqlite3_errstr(rc));
            sqlite3_finalize(database.exact);
            sqlite3_finalize(database.prefix);
            sqlite3_close(database.db);
            return false;
        }
        _databases.push_back(database);
        return true;
    }

    void findRows(const std::string & key, bool prefix, std::vector<JpDictionaryRow> & rows) {
        // 0xFF never occurs in UTF-8, so every key starting with `key` sorts
        // before this bound (keys compare as bytes).
        std::string upper = key + "\xFF";
        for ( size_t i = 0; i < _databases.size(); i++ ) {
            sqlite3_stmt * stmt = prefix ? _databases[i].prefix : _databases[i].exact;
            sqlite3_bind_text(stmt, 1, key.data(), (int)key.size(), SQLITE_STATIC);
            if ( prefix )
                sqlite3_bind_text(stmt, 2, upper.data(), (int)upper.size(), SQLITE_STATIC);
            while ( sqlite3_step(stmt) == SQLITE_ROW ) {
                JpDictionaryRow row;
                row.key = columnText(stmt, 0);
                row.is_expression = sqlite3_column_int(stmt, 1) != 0;
                row.definition_tags = columnText(stmt, 2);
                row.rules = columnText(stmt, 3);
                row.term_tags = columnText(stmt, 4);
                rows.push_back(row);
            }
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
    }

    uint32_t getHash() const { return _hash; }

private:
    struct Database {
        sqlite3 * db;
        sqlite3_stmt * exact;
        sqlite3_stmt * prefix;
    };
    std::vector<Database> _databases;
    uint32_t _hash;
};

} // namespace

uint32_t getJpSqliteDictionaryHash(const std::vector<std::string> & paths) {
    uint32_t hash = 2166136261u;
    for ( size_t i = 0; i < paths.size(); i++ ) {
        struct stat st;
        long long values[2] = { -1, -1 };
        if ( stat(paths[i].c_str(), &st) == 0 ) {
            values[0] = (long long)st.st_size;
            values[1] = (long long)st.st_mtime;
        }
        hash = hashBytes(hash, paths[i].c_str(), paths[i].size() + 1);
        hash = hashBytes(hash, values, sizeof(values));
    }
    return hash ? hash : 1;
}

JpDictionary * newJpSqliteDictionary(const std::vector<std::string> & paths, std::string & error) {
    SqliteDictionary * dictionary = new SqliteDictionary(getJpSqliteDictionaryHash(paths));
    for ( size_t i = 0; i < paths.size(); i++ ) {
        if ( !dictionary->open(paths[i], error) ) {
            delete dictionary;
            return NULL;
        }
    }
    return dictionary;
}
