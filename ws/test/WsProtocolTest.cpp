#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <string>

#include "WsProtocol.hpp"
#include "nlohmann/json.hpp"

namespace {

using ws::server::JsonMessage;
using ws::server::json;
namespace fs = std::filesystem;

void expect(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

fs::path make_temp_json_path(const std::string &filename) {
  return fs::temp_directory_path() / filename;
}

void write_json_file(const fs::path &path, const json &content) {
  std::ofstream stream(path);
  expect(stream.is_open(), "test file should be writable");
  stream << content.dump(2);
}

json read_json_file(const fs::path &path) {
  std::ifstream stream(path);
  expect(stream.is_open(), "test file should be readable");
  return json::parse(stream, nullptr, true);
}

void test_message_to_json_with_object_payload() {
  JsonMessage message{
      .id = "1",
      .correlation_id = "req-1",
      .kind = "event",
      .ts = "2026-03-06T12:00:00Z",
      .room = "main",
      .type = "joined",
      .sender = "alice",
      .version = "2",
      .payload = {{"user", "alice"}, {"count", 2}},
      .meta = {{"trace_id", "trace-1"}},
  };

  const json serialized = ws::server::message_to_json(message);

  expect(serialized.is_object(), "serialized message should be an object");
  expect(serialized["id"] == "1", "id should round-trip");
  expect(serialized["correlation_id"] == "req-1",
         "correlation_id should round-trip");
  expect(serialized["kind"] == "event", "kind should round-trip");
  expect(serialized["ts"] == "2026-03-06T12:00:00Z", "ts should round-trip");
  expect(serialized["room"] == "main", "room should round-trip");
  expect(serialized["type"] == "joined", "type should round-trip");
  expect(serialized["sender"] == "alice", "sender should round-trip");
  expect(serialized["version"] == "2", "version should round-trip");
  expect(serialized["payload"].is_object(), "payload should stay a JSON object");
  expect(serialized["payload"]["user"] == "alice", "payload.user should match");
  expect(serialized["payload"]["count"] == 2, "payload.count should match");
  expect(serialized["meta"]["trace_id"] == "trace-1", "meta should round-trip");
}

void test_message_to_json_with_scalar_payload_and_error() {
  JsonMessage message{
      .id = "2",
      .correlation_id = "req-2",
      .kind = "response",
      .ts = "2026-03-06T12:05:00Z",
      .room = "main",
      .type = "note",
      .sender = "server",
      .version = "1",
      .error = "permission_denied",
      .payload = "plain-text-payload",
      .meta = {{"severity", "high"}},
  };

  const json serialized = ws::server::message_to_json(message);

  expect(serialized["kind"] == "response", "kind should round-trip");
  expect(serialized["error"] == "permission_denied", "error should round-trip");
  expect(serialized["payload"].is_string(), "string payload should stay a string");
  expect(serialized["payload"] == "plain-text-payload",
         "plain text payload should be preserved");
  expect(serialized["meta"]["severity"] == "high", "meta should round-trip");
}

void test_json_to_message_with_object_payload() {
  const json raw = {
      {"id", "3"},
      {"correlation_id", "req-3"},
      {"kind", "event"},
      {"ts", "2026-03-06T12:10:00Z"},
      {"room", "main"},
      {"type", "update"},
      {"sender", "system"},
      {"version", "3"},
      {"payload", {{"ready", true}, {"count", 7}}},
      {"meta", {{"source", "scheduler"}}},
  };

  const auto parsed = ws::server::json_to_message(raw);

  expect(parsed.has_value(), "object JSON should parse into a message");
  expect(parsed->id == "3", "parsed id should match");
  expect(parsed->correlation_id == "req-3",
         "parsed correlation_id should match");
  expect(parsed->kind == "event", "parsed kind should match");
  expect(parsed->ts == "2026-03-06T12:10:00Z", "parsed ts should match");
  expect(parsed->room == "main", "parsed room should match");
  expect(parsed->type == "update", "parsed type should match");
  expect(parsed->sender == "system", "parsed sender should match");
  expect(parsed->version == "3", "parsed version should match");
  expect(parsed->payload.is_object(), "parsed payload should stay JSON");
  expect(parsed->payload["ready"] == true, "payload.ready should match");
  expect(parsed->payload["count"] == 7, "payload.count should match");
  expect(parsed->meta["source"] == "scheduler", "meta should match");
}

void test_json_to_message_with_string_payload_and_error() {
  const json raw = {
      {"id", "4"},
      {"correlation_id", "req-4"},
      {"kind", "response"},
      {"ts", "2026-03-06T12:15:00Z"},
      {"room", "main"},
      {"type", "message"},
      {"sender", "server"},
      {"version", "1"},
      {"error", "bad_request"},
      {"payload", "hello"},
      {"meta", {{"retryable", false}}},
  };

  const auto parsed = ws::server::json_to_message(raw);

  expect(parsed.has_value(), "string payload JSON should parse");
  expect(parsed->payload == "hello", "string payload should be preserved");
  expect(parsed->error.has_value(), "error should be present");
  expect(*parsed->error == "bad_request", "error should match");
  expect(parsed->meta["retryable"] == false, "meta should match");
}

void test_json_to_message_with_non_object_json_values() {
  const json raw = {
      {"id", "4a"},
      {"correlation_id", "req-4a"},
      {"kind", "event"},
      {"ts", "2026-03-06T12:16:00Z"},
      {"room", "main"},
      {"type", "variants"},
      {"sender", "server"},
      {"version", "1"},
      {"payload", json::array({"hello", 7, true})},
      {"meta", false},
  };

  const auto parsed = ws::server::json_to_message(raw);

  expect(parsed.has_value(), "array payload and boolean meta should parse");
  expect(parsed->payload.is_array(), "payload should preserve array type");
  expect(parsed->payload[0] == "hello", "array payload element should match");
  expect(parsed->payload[1] == 7, "numeric array payload element should match");
  expect(parsed->payload[2] == true, "boolean array payload element should match");
  expect(parsed->meta.is_boolean(), "meta should preserve boolean type");
  expect(parsed->meta == false, "boolean meta should match");
}

void test_message_to_json_with_null_payload_and_numeric_meta() {
  JsonMessage message{
      .id = "2a",
      .correlation_id = "req-2a",
      .kind = "event",
      .ts = "2026-03-06T12:06:00Z",
      .room = "main",
      .type = "metrics",
      .sender = "server",
      .version = "1",
      .payload = nullptr,
      .meta = 42,
  };

  const json serialized = ws::server::message_to_json(message);

  expect(serialized["payload"].is_null(), "null payload should stay null");
  expect(serialized["meta"].is_number_integer(),
         "numeric meta should stay numeric");
  expect(serialized["meta"] == 42, "numeric meta should be preserved");
}

void test_json_to_message_defaults_missing_payload_and_meta_to_empty_object() {
  const json raw = {
      {"id", "4b"},
      {"correlation_id", "req-4b"},
      {"kind", "event"},
      {"ts", "2026-03-06T12:17:00Z"},
      {"room", "main"},
      {"type", "defaults"},
      {"sender", "server"},
      {"version", "1"},
  };

  const auto parsed = ws::server::json_to_message(raw);

  expect(parsed.has_value(), "message without payload/meta should parse");
  expect(parsed->payload.is_object(), "missing payload should default to object");
  expect(parsed->payload.empty(), "default payload object should be empty");
  expect(parsed->meta.is_object(), "missing meta should default to object");
  expect(parsed->meta.empty(), "default meta object should be empty");
}

void test_json_to_message_rejects_invalid_shapes() {
  expect(!ws::server::json_to_message(json::array({1, 2, 3})).has_value(),
         "non-object JSON should be rejected");

  const json invalid_field = {
      {"id", 10},
      {"payload", {{"ok", true}}},
  };
  expect(!ws::server::json_to_message(invalid_field).has_value(),
         "non-string scalar fields should be rejected");

  const json invalid_error = {
      {"id", "7"},
      {"error", 10},
  };
  expect(!ws::server::json_to_message(invalid_error).has_value(),
         "non-string error should be rejected");
}

void test_message_to_json_file_round_trip() {
  const JsonMessage source{
      .id = "5",
      .correlation_id = "req-5",
      .kind = "event",
      .ts = "2026-03-06T12:20:00Z",
      .room = "ops",
      .type = "snapshot",
      .sender = "collector",
      .version = "4",
      .payload = {{"status", "ok"}, {"items", json::array({1, 2, 3})}},
      .meta = {{"partition", "a"}},
  };

  const fs::path file_path = make_temp_json_path("ws_protocol_message_to_json.json");
  write_json_file(file_path, ws::server::message_to_json(source));
  const json from_file = read_json_file(file_path);
  const auto parsed = ws::server::json_to_message(from_file);

  expect(parsed.has_value(), "json file produced from JsonMessage should parse back");
  expect(parsed->id == source.id, "file round-trip id should match");
  expect(parsed->kind == source.kind, "file round-trip kind should match");
  expect(parsed->ts == source.ts, "file round-trip ts should match");
  expect(parsed->room == source.room, "file round-trip room should match");
  expect(parsed->type == source.type, "file round-trip type should match");
  expect(parsed->sender == source.sender, "file round-trip sender should match");
  expect(parsed->version == source.version,
         "file round-trip version should match");
  expect(parsed->payload == source.payload, "file round-trip payload should match");
  expect(parsed->meta == source.meta, "file round-trip meta should match");

  fs::remove(file_path);
}

void test_json_file_to_message_round_trip() {
  const json source = {
      {"id", "6"},
      {"correlation_id", "req-6"},
      {"kind", "event"},
      {"ts", "2026-03-06T12:25:00Z"},
      {"room", "ops"},
      {"type", "notice"},
      {"sender", "monitor"},
      {"version", "1"},
      {"payload", {{"text", "hello"}, {"priority", 3}}},
      {"meta", {{"trace_id", "trace-6"}}},
  };

  const fs::path file_path = make_temp_json_path("ws_protocol_json_to_message.json");
  write_json_file(file_path, source);
  const json from_file = read_json_file(file_path);
  const auto parsed = ws::server::json_to_message(from_file);

  expect(parsed.has_value(), "json file should parse into JsonMessage");
  const json restored = ws::server::message_to_json(*parsed);

  expect(restored == source, "message converted from file should serialize back to same JSON");

  fs::remove(file_path);
}

void test_file_chunk_round_trip_crc32() {
  const fs::path source_path =
      make_temp_json_path("ws_protocol_chunk_source.bin");
  const fs::path restored_path =
      make_temp_json_path("ws_protocol_chunk_restored.bin");

  {
    std::ofstream stream(source_path, std::ios::binary);
    expect(stream.is_open(), "chunk source file should be writable");
    for (int i = 0; i < 4096; ++i) {
      const unsigned char value = static_cast<unsigned char>((i * 37) % 256);
      stream.write(reinterpret_cast<const char *>(&value), 1);
    }
  }

  const auto chunks =
      ws::server::blob_to_chunk(source_path, 257, "chunk-test-transfer");
  expect(!chunks.empty(), "blob_to_chunk should produce chunk messages");
  expect(chunks.front().type == "chunk_start",
         "first chunk message should be chunk_start");
  expect(chunks.back().type == "chunk_end",
         "last chunk message should be chunk_end");

  const bool restored = ws::server::chunks_to_blob(chunks, restored_path);
  expect(restored, "chunks_to_blob should recreate the file");

  const auto original_crc = ws::server::crc32_of_file(source_path.string());
  const auto restored_crc = ws::server::crc32_of_file(restored_path.string());
  expect(original_crc == restored_crc,
         "restored file checksum should match original file checksum");

  fs::remove(source_path);
  fs::remove(restored_path);
}

} // namespace

int main() {
  // test_message_to_json_with_object_payload();
  // test_message_to_json_with_scalar_payload_and_error();
  // test_message_to_json_with_null_payload_and_numeric_meta();
  // test_json_to_message_with_object_payload();
  // test_json_to_message_with_string_payload_and_error();
  // test_json_to_message_with_non_object_json_values();
  // test_json_to_message_defaults_missing_payload_and_meta_to_empty_object();
  // test_json_to_message_rejects_invalid_shapes();
  // test_message_to_json_file_round_trip();
  // test_json_file_to_message_round_trip();
  test_file_chunk_round_trip_crc32();
  std::cout << "WsProtocolTest passed\n";
  return EXIT_SUCCESS;
}
