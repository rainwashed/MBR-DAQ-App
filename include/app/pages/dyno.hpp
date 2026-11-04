#pragma once

#include <memory>
#include <string>
#include <vector>

#include <gsl/span>

#include "app/pages/page.hpp"
#include "app/pages/utils.hpp"

namespace mbr::pages {

class dyno_page : public page {
  public:
    explicit dyno_page(const std::shared_ptr<app_context>& ctx) : page{ctx}, text_drawer_{ctx} {}
    ~dyno_page() override = default;

    void on_enter() override;
    void on_exit() override;
    void update() override;

  private:
    void DrawLHS(const std::vector<std::string>& raw_lines);
    void DrawRHS(gsl::span<const double> time,
                 gsl::span<const double> force,
                 gsl::span<const double> displacement);

  private:
    utils::text_drawers text_drawer_;
    std::string         download_fd_text_;
};

} // namespace mbr::pages
