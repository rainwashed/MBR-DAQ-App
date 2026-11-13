#include "esp32/data.hpp"

#include <algorithm>
#include <charconv>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <stdx/profiler.hh>
#include <stdx/types.hh>

#include "core/log.hpp"
#include "core/time.hpp"

namespace mbr {

namespace {

void append_csv_field(std::string& out, std::string_view field) {
    if (field.find_first_of(",\"\n\r") == std::string_view::npos) {
        out.append(field);
        return;
    }

    out.push_back('"');
    for (const char c : field) {
        if (c == '"') { out.push_back('"'); }
        out.push_back(c);
    }
    out.push_back('"');
}

} // namespace

telemetry_data::telemetry_data(const std::filesystem::path& json_path, log_fn_t log)
    : log_{std::move(log)} {
    init_json(json_path);
    init_data();
}

void telemetry_data::init_json(const std::filesystem::path& path) {
    std::ifstream f{path};
    if (!f.is_open()) {
        log_error(log_, "JSON FILE NOT OPENED");
        return;
    }
    data_info temp;
    json      jason = json::parse(f);
    try {
        for (const auto& data : jason.at("fields")) {
            temp.key      = data.at("key").get<std::string>();
            temp.name     = data.at("name").get<std::string>();
            temp.required = data.at("required").get<bool>();
            temp.plot     = data.at("plot").get<bool>();
            temp.unit     = data.value("unit", "");
            temp.group    = data.value("group", "");
            data_values.emplace_back(std::move(temp));
        }
    } catch (const json::exception& e) {
        log_error(log_, "Invalid JSON config: {}", e.what());
        return;
    }
}

void telemetry_data::init_data() {
    for (const auto& data : data_values) { series.try_emplace(data.key, std::vector<f64>{}); }
}

void telemetry_data::write_data(const std::string& identifier, const std::string& value) {
    PROFILE_FUNCTION();
    try {
        // This cannot be from_chars since apple clang doesn't support it :(
        f64 val = std::stod(value);
        series[identifier].emplace_back(val);
        if (identifier == "T") {
            u64 raw_micros = static_cast<u64>(val);
            time_no_normal_micros_.emplace_back(raw_micros);
            time_.emplace_back(local_time{raw_micros}.minutes_since_midnight());
        }
    } catch (const std::exception& e) { log_error(log_, "Couldn't write to existing data vector"); }
}

std::string telemetry_data::to_csv() const {
    std::string csv;

    for (usize col = 0; col < data_values.size(); ++col) {
        if (col != 0) { csv.push_back(','); }
        append_csv_field(csv, data_values[col].key);
    }
    csv.push_back('\n');

    // Prefer the raw packet lines: they preserve which fields were actually present in each
    // packet, so missing samples become empty cells instead of shifting later columns.
    if (!raw_lines_.empty()) {
        std::vector<std::string_view> row(data_values.size());
        for (const auto& raw : raw_lines_) {
            std::ranges::fill(row, std::string_view{});
            const std::string_view line{raw};

            usize pos = 0;
            while (pos < line.size()) {
                while (pos < line.size() && line[pos] == ' ') { ++pos; }
                const usize key_begin = pos;
                while (pos < line.size() && line[pos] != ' ') { ++pos; }
                if (key_begin == pos) { break; }
                const std::string_view key = line.substr(key_begin, pos - key_begin);

                while (pos < line.size() && line[pos] == ' ') { ++pos; }
                const usize value_begin = pos;
                while (pos < line.size() && line[pos] != ' ') { ++pos; }
                if (value_begin == pos) { break; }
                const std::string_view value = line.substr(value_begin, pos - value_begin);

                for (usize col = 0; col < data_values.size(); ++col) {
                    if (std::string_view{data_values[col].key} == key) {
                        row[col] = value;
                        break;
                    }
                }
            }

            for (usize col = 0; col < row.size(); ++col) {
                if (col != 0) { csv.push_back(','); }
                append_csv_field(csv, row[col]);
            }
            csv.push_back('\n');
        }
        return csv;
    }

    // No raw lines (e.g. data loaded from a text file): fall back to index alignment.
    usize rows = time_.size();
    for (const auto& entry : series) { rows = std::max(rows, entry.second.size()); }

    for (usize row = 0; row < rows; ++row) {
        for (usize col = 0; col < data_values.size(); ++col) {
            if (col != 0) { csv.push_back(','); }
            const auto it = series.find(data_values[col].key);
            if (it != series.end() && row < it->second.size()) {
                csv.append(fmt::format("{}", it->second[row]));
            }
        }
        csv.push_back('\n');
    }

    return csv;
}

void telemetry_data::clear() {
    for (auto& [key, values] : series) { values.clear(); }
    time_.clear();
    time_no_normal_micros_.clear();
    raw_lines_.clear();
    current_line_.clear();
    sync_lt_.reset();
    logged_missing_keys_.clear();
}

void telemetry_data::save_current_line(const std::string& line) { current_line_ = line; }

void telemetry_data::write_raw_line(const std::string& full_message) {
    raw_lines_.emplace_back(full_message);
}

} // namespace mbr
