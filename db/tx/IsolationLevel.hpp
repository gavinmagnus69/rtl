#pragma once

namespace db::tx {

enum class IsolationLevel { read_committed, repeatable_read, serializable };

};