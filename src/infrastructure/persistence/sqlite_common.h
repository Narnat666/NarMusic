#pragma once
// ============================================================================
// SQLite 持久化层公共定义
// ============================================================================
#include <sqlite3.h>
#include <cstdint>

namespace narnat {

// SQLITE_TRANSIENT 的 C++ 等价物（使用 reinterpret_cast 避免 C 风格转型警告）
inline const sqlite3_destructor_type kSqliteTransient =
    reinterpret_cast<sqlite3_destructor_type>(static_cast<intptr_t>(-1));

} // namespace narnat
