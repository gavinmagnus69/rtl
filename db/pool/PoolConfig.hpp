#pragma once

namespace db::pool {

static constexpr size_t g_default_max_connections = 10;
static constexpr size_t g_default_min_connections = 1;

struct PoolConfig {
  size_t max_connections{g_default_max_connections};
  size_t min_connections{g_default_min_connections};
  size_t connection_timeout_ms{0};
  size_t idle_timeout_ms{0};
};

}; // namespace db::pool
