#include "JsonFunctions.h"

#include <fstream>


bool rtl::checkJson(const json& doc, const std::vector<std::string_view>& field_names) {
    for (const auto& field_name : field_names) {
        if (!doc.contains(field_name)) {
            return false;
        }
    }
    return true;
}


std::optional<rtl::json> rtl::getJsonFromFile(const std::filesystem::path& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return std::nullopt;
    }
    json doc = json::parse(file);
    if (doc.empty()) {
        return std::nullopt;
    }
    return doc;
}