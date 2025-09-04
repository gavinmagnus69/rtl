#ifndef rtl_json_jsonfunctions_h
#define rtl_json_jsonfunctions_h


#include <nlohmann/json.hpp>


namespace rtl {
using nlohmann::json;

bool checkJson(const json& doc, const std::vector<std::string_view>& field_names);

std::optional<json> getJsonFromFile(const std::filesystem::path& filepath);
}; // namespace rtl


#endif