#include "app/pages/dyno.hpp"

#include <shared_mutex>
#include <string>

#include <fmt/format.h>
#include <gsl/span>
#include <gsl/util>
#include <imgui.h>
#include <implot.h>
#include <stdx/profiler.hh>
#include <stdx/types.hh>

#include "app/pages/utils.hpp"
#include "app/style.hpp"
#include "core/log.hpp"
#include "core/time.hpp"

namespace mbr::pages {

void dyno_page::on_enter() { log_info(context_->log, "Entered DynoPage"); }
void dyno_page::on_exit() { log_info(context_->log, "Exited DynoPage"); }

void dyno_page::update() {
    PROFILE_FUNCTION();
    if (ImGui::BeginTable(
            "##viewsplit", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Resizable)) {
        const auto             cleanup_split{gsl::finally(ImGui::EndTable)};
        const std::shared_lock lock{context_->backend->get_data_latch()};
        const auto&            data = context_->backend->get_data();

        ImGui::TableNextColumn();
        DrawLHS(data);
        ImGui::TableNextColumn();
        DrawRHS(data.get_time(), data.get_series("DF"), data.get_series("DD"));
    }
}

void dyno_page::DrawLHS(const telemetry_data& data) {
    const auto& raw_lines = data.get_raw_lines();
    if (ImGui::BeginChild("##datalog")) {
        const auto cleanup_data{gsl::finally(ImGui::EndChild)};
        BOLD_HEADER(ImGui::Text("Data Log"));

        ImGui::Separator();
        text_drawer_.start_logging_button();
        ImGui::SameLine();
        text_drawer_.data_download_button(raw_lines, download_fd_text_);
        ImGui::SameLine();
        text_drawer_.data_csv_download_button(data, download_fd_text_);
        ImGui::SameLine();

        HEADER(utils::draw_input_box("##extra_dyno", download_fd_text_, "File descriptor"));
        ImGui::Separator();
        utils::draw_data_log(raw_lines);
    }
}

void dyno_page::DrawRHS(gsl::span<const f64> time,
                        gsl::span<const f64> force,
                        gsl::span<const f64> displacement) {
    if (ImGui::BeginChild("##graph")) {
        const auto cleanup_graph{gsl::finally(ImGui::EndChild)};
        const auto sync_lt    = context_->backend->get_data().get_sync_lt();
        const auto plot_title = sync_lt
                                    ? fmt::format("Dyno Data from {}", sync_lt.value().to_string())
                                    : "No Synced Time";

        if (ImPlot::BeginPlot(plot_title.c_str(), {-1, -1})) {
            const auto cleanup_plot{gsl::finally(ImPlot::EndPlot)};
            ImPlot::SetupAxes(
                "Time (min)", "Force (g)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxis(
                ImAxis_Y2, "Displacement (cm)", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Opposite);

            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
            utils::plot_if_non_empty<f64>("Dyno Force", time, force);

            ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
            utils::plot_if_non_empty<f64>("Dyno Displacement", time, displacement);
        }
    }
}

} // namespace mbr::pages
