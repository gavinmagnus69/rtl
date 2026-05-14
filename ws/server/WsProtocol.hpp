#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

#include <zlib.h>
namespace ws::server {

using json = nlohmann::json;

struct JsonMessage {
  std::string id{};
  std::string correlation_id{};
  std::string kind{"event"};
  std::string ts{};
  std::string room{};
  std::string type{};
  std::string sender{};
  std::string version{"1"};
  std::optional<std::string> error{};
  json payload = json::object(); // arbitrary message content, any JSON value
  json meta = json::object();    // auxiliary metadata, any JSON value
};

inline json message_to_json(const JsonMessage& msg) {
  json j;
  j["id"] = msg.id;
  j["correlation_id"] = msg.correlation_id;
  j["kind"] = msg.kind;
  j["ts"] = msg.ts;
  j["room"] = msg.room;
  j["type"] = msg.type;
  j["sender"] = msg.sender;
  j["version"] = msg.version;
  if (msg.error.has_value()) {
    j["error"] = *msg.error;
  }
  j["payload"] = msg.payload;
  j["meta"] = msg.meta;
  return j;
}

inline void set_payload(JsonMessage& msg, const json& payload) {
  msg.payload = payload;
}

inline std::optional<json> get_payload(const JsonMessage& msg) {
  return msg.payload;
}

inline std::optional<JsonMessage> json_to_message(const json& json) {
  if (!json.is_object()) {
    return std::nullopt;
  }
  const auto read_string_field = [&json](std::string_view key, std::string& target) -> bool {
    const auto it = json.find(key);
    if (it == json.end()) {
      return true;
    }
    if (!it->is_string()) {
      return false;
    }
    target = it->get<std::string>();
    return true;
  };

  JsonMessage message{};
  if (!read_string_field("id", message.id) || !read_string_field("correlation_id", message.correlation_id) || !read_string_field("kind", message.kind) || !read_string_field("ts", message.ts) ||
      !read_string_field("room", message.room) || !read_string_field("type", message.type) || !read_string_field("sender", message.sender) || !read_string_field("version", message.version)) {
    return std::nullopt;
  }
  const auto error_it = json.find("error");
  if (error_it != json.end()) {
    if (error_it->is_null()) {
      message.error.reset();
    } else if (error_it->is_string()) {
      message.error = error_it->get<std::string>();
    } else {
      return std::nullopt;
    }
  }
  const auto payload_it = json.find("payload");
  if (payload_it == json.end()) {
    message.payload = json::object();
  } else {
    message.payload = *payload_it;
  }
  const auto meta_it = json.find("meta");
  if (meta_it == json.end()) {
    message.meta = json::object();
  } else {
    message.meta = *meta_it;
  }
  return message;
}

inline std::optional<JsonMessage> string_to_message(const std::string& text) {
  const auto parsed = json::parse(text, nullptr, false);
  if (parsed.is_discarded()) {
    return std::nullopt;
  }
  return json_to_message(parsed);
}

inline uint32_t crc32_of_bytes(const std::vector<std::byte>& data) {
  return crc32(0L, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
}

inline uint32_t crc32_of_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);

  if (!file)
    throw std::runtime_error("Cannot open file");

  const std::size_t buffer_size = 64 * 1024;
  std::vector<unsigned char> buffer(buffer_size);

  uLong crc = crc32(0L, Z_NULL, 0);

  while (file) {
    file.read(reinterpret_cast<char*>(buffer.data()), buffer_size);
    std::streamsize read = file.gcount();

    if (read > 0) {
      crc = crc32(crc, buffer.data(), static_cast<uInt>(read));
    }
  }

  return static_cast<uint32_t>(crc);
}

inline constexpr size_t g_bytes_to_reserve = 1'000'000;

inline std::vector<std::byte> read_binary_file(const std::filesystem::path& file_path) {
  std::ifstream file{file_path, std::ios::binary};
  if (!file.is_open()) {
    return {};
  }
  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<std::byte> binaryData(file_size);
  file.read(reinterpret_cast<char*>(binaryData.data()), file_size);
  return binaryData;
}

inline std::vector<JsonMessage> blob_to_chunk(const std::filesystem::path& path_to_file, size_t chunk_size, std::string_view naming) {
  if (chunk_size == 0) {
    return {};
  }
  auto binaryData = read_binary_file(path_to_file);
  if (binaryData.empty()) {
    return {};
  }
  const size_t size_of_file = binaryData.size();
  const std::string transfer_id = naming.empty() ? path_to_file.filename().string() : std::string(naming);
  const size_t total_chunks = (size_of_file + chunk_size - 1) / chunk_size;
  std::vector<JsonMessage> chunks;
  chunks.reserve(total_chunks + 2);

  JsonMessage start_message{};
  start_message.id = transfer_id + ":start";
  start_message.correlation_id = transfer_id;
  start_message.type = "chunk_start";
  start_message.meta = {
      {"transfer_id", transfer_id}, {"file_name", path_to_file.filename().string()}, {"total_bytes", size_of_file}, {"total_chunks", total_chunks}, {"chunk_size", chunk_size},
  };
  start_message.payload = json::object();
  chunks.push_back(std::move(start_message));

  for (size_t chunk_index = 0; chunk_index < total_chunks; ++chunk_index) {
    const size_t offset = chunk_index * chunk_size;
    const size_t current_chunk_size = std::min(chunk_size, size_of_file - offset);

    JsonMessage chunk_message{};
    chunk_message.id = transfer_id + ":" + std::to_string(chunk_index);
    chunk_message.correlation_id = transfer_id;
    chunk_message.type = "chunk_data";
    chunk_message.meta = {
        {"transfer_id", transfer_id},
        {"chunk_index", chunk_index},
        {"total_chunks", total_chunks},
        {"chunk_bytes", current_chunk_size},
    };

    json payload = json::array();
    for (size_t i = 0; i < current_chunk_size; ++i) {
      payload.push_back(std::to_integer<unsigned int>(binaryData[offset + i]));
    }
    chunk_message.payload = std::move(payload);
    chunks.push_back(std::move(chunk_message));
  }

  JsonMessage end_message{};
  end_message.id = transfer_id + ":end";
  end_message.correlation_id = transfer_id;
  end_message.type = "chunk_end";
  end_message.meta = {
      {"transfer_id", transfer_id},
      {"total_bytes", size_of_file},
      {"total_chunks", total_chunks},
  };
  end_message.payload = json::object();
  chunks.push_back(std::move(end_message));

  return chunks;
}

inline bool chunks_to_blob(const std::vector<JsonMessage>& chunks, const std::filesystem::path& created_file_name) {
  if (chunks.size() < 3) {
    return false;
  }

  const auto* start_message = &chunks.front();
  const auto* end_message = &chunks.back();
  if (start_message->type != "chunk_start" || end_message->type != "chunk_end") {
    return false;
  }
  if (!start_message->meta.is_object() || !end_message->meta.is_object()) {
    return false;
  }

  const auto read_size_field = [](const json& obj, std::string_view key) -> std::optional<size_t> {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_number_unsigned()) {
      return std::nullopt;
    }
    return it->get<size_t>();
  };
  const auto read_string_field = [](const json& obj, std::string_view key) -> std::optional<std::string> {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->is_string()) {
      return std::nullopt;
    }
    return it->get<std::string>();
  };

  const auto transfer_id = read_string_field(start_message->meta, "transfer_id");
  const auto total_chunks = read_size_field(start_message->meta, "total_chunks");
  const auto total_bytes = read_size_field(start_message->meta, "total_bytes");
  if (!transfer_id.has_value() || !total_chunks.has_value() || !total_bytes.has_value()) {
    return false;
  }

  const auto end_transfer_id = read_string_field(end_message->meta, "transfer_id");
  const auto end_total_chunks = read_size_field(end_message->meta, "total_chunks");
  const auto end_total_bytes = read_size_field(end_message->meta, "total_bytes");
  if (!end_transfer_id.has_value() || !end_total_chunks.has_value() || !end_total_bytes.has_value()) {
    return false;
  }
  if (*transfer_id != *end_transfer_id || *total_chunks != *end_total_chunks || *total_bytes != *end_total_bytes) {
    return false;
  }
  if (chunks.size() != *total_chunks + 2) {
    return false;
  }

  std::vector<std::vector<std::byte>> ordered_chunks(*total_chunks);
  std::vector<bool> seen(*total_chunks, false);
  size_t accumulated_bytes = 0;

  for (size_t i = 1; i + 1 < chunks.size(); ++i) {
    const auto& chunk = chunks[i];
    if (chunk.type != "chunk_data" || !chunk.meta.is_object() || !chunk.payload.is_array()) {
      return false;
    }

    const auto chunk_transfer_id = read_string_field(chunk.meta, "transfer_id");
    const auto chunk_index = read_size_field(chunk.meta, "chunk_index");
    const auto chunk_count = read_size_field(chunk.meta, "total_chunks");
    const auto chunk_bytes = read_size_field(chunk.meta, "chunk_bytes");
    if (!chunk_transfer_id.has_value() || !chunk_index.has_value() || !chunk_count.has_value() || !chunk_bytes.has_value()) {
      return false;
    }
    if (*chunk_transfer_id != *transfer_id || *chunk_count != *total_chunks || *chunk_index >= *total_chunks || seen[*chunk_index]) {
      return false;
    }
    if (chunk.payload.size() != *chunk_bytes) {
      return false;
    }

    auto& target = ordered_chunks[*chunk_index];
    target.reserve(*chunk_bytes);
    for (const auto& item : chunk.payload) {
      if (!item.is_number_unsigned()) {
        return false;
      }
      const auto value = item.get<unsigned int>();
      if (value > 0xFFu) {
        return false;
      }
      target.push_back(static_cast<std::byte>(value));
    }

    accumulated_bytes += target.size();
    seen[*chunk_index] = true;
  }

  if (!std::all_of(seen.begin(), seen.end(), [](bool value) { return value; })) {
    return false;
  }
  if (accumulated_bytes != *total_bytes) {
    return false;
  }

  std::ofstream output(created_file_name, std::ios::binary);
  if (!output.is_open()) {
    return false;
  }

  for (const auto& chunk : ordered_chunks) {
    if (!chunk.empty()) {
      output.write(reinterpret_cast<const char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
      if (!output) {
        return false;
      }
    }
  }

  return true;
}

}; // namespace ws::server
