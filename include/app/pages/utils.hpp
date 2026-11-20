#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gsl/span>
#include <imgui.h>
#include <implot.h>
#include <stdx/option.hh>
#include <stdx/types.hh>

namespace mbr {

struct app_context;
class telemetry_data;

namespace pages::utils {

template <typename T>
void plot_if_non_empty(const char*        label,
                       gsl::span<const T> x,
                       gsl::span<const T> y,
                       bool               extra_condition    = true,
                       usize              data_clip_position = 0) {
    if (!x.empty() && !y.empty() && extra_condition) {
        const usize n     = std::min(x.size(), y.size());
        const usize count = n - std::min(n, data_clip_position);
        ImPlot::PlotLine(label, x.data(), y.data(), count);
    }
}

class text_drawers {
  public:
    explicit text_drawers(const std::shared_ptr<app_context>& ctx) : context_{ctx} {}

    void start_logging_button();
    bool start_serial_button();
    void send_data_button();

    // Appends the buf data to the filepath, clearing the buf on success.
    void data_download_button(const std::vector<std::string>& raw_lines, std::string& buf);

    // Writes the current data set to a CSV file, clearing the buf on success.
    void data_csv_download_button(const telemetry_data& data, std::string& buf);

  private:
    std::shared_ptr<app_context> context_;
};

bool draw_input_box(const char*               label,
                    std::string&              buf,
                    stdx::option<const char*> hint        = stdx::none,
                    float                     width_scale = 200.0F,
                    ImGuiInputTextFlags       flags       = 0);
void draw_data_log(gsl::span<const std::string> raw_lines);

// Builds a display label for a configured series, e.g. "Dyno Force (N)". Falls back to the
// key itself when it is not present in the telemetry configuration.
[[nodiscard]] std::string series_label(const telemetry_data& data, std::string_view key);

} // namespace pages::utils

} // namespace mbr
