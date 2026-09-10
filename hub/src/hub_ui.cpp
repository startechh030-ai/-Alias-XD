#include "hub_ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "lume/log.hpp"
#include "theme.hpp"

#ifdef LUME_WITH_IMGUI
#include "imgui.h"
#endif

namespace lume::hub {
namespace {

constexpr int kNewProjectNameCap = 128;

}  // namespace

int HubUi::columns_for_width(float width) const {
  const float usable = std::max(320.0f, width - theme::kSidebarW - theme::kGridPad * 2.0f);
  const int cols = static_cast<int>(
      std::floor((usable + theme::kCardGap) / (theme::kCardW + theme::kCardGap)));
  return std::max(1, std::min(cols, 4));
}

bool HubUi::open_new_project_dialog() {
  new_project_open_ = true;
  new_project_error_.clear();
  return true;
}

#ifdef LUME_WITH_IMGUI
namespace {

void set_style() {
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = theme::kRadius;
  style.FrameRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 6.0f;
  style.WindowBorderSize = 0.0f;
  style.FrameBorderSize = 1.0f;
  style.ItemSpacing = ImVec2(theme::kCardGap, 10.0f);
  style.WindowPadding = ImVec2(theme::kGridPad, theme::kGridPad);
  style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(theme::kWindow | 0xFF000000u);
  style.Colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(theme::kField | 0xFF000000u);
  style.Colors[ImGuiCol_FrameBgHovered] =
      ImGui::ColorConvertU32ToFloat4(theme::kRaised | 0xFF000000u);
  style.Colors[ImGuiCol_FrameBgActive] =
      ImGui::ColorConvertU32ToFloat4(theme::kPanel | 0xFF000000u);
  style.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(theme::kText | 0xFF000000u);
  style.Colors[ImGuiCol_TextDisabled] =
      ImGui::ColorConvertU32ToFloat4(theme::kTextFaint | 0xFF000000u);
  style.Colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(theme::kCard | 0xFF000000u);
  style.Colors[ImGuiCol_ButtonHovered] =
      ImGui::ColorConvertU32ToFloat4(theme::kRaised | 0xFF000000u);
  style.Colors[ImGuiCol_ButtonActive] =
      ImGui::ColorConvertU32ToFloat4(theme::kPanel | 0xE6000000u);
  style.Colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(theme::kRaised | 0x99000000u);
  style.Colors[ImGuiCol_HeaderHovered] =
      ImGui::ColorConvertU32ToFloat4(theme::kRaised | 0xFF000000u);
  style.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(theme::kBorder | 0xFF000000u);
  style.Colors[ImGuiCol_Separator] =
      ImGui::ColorConvertU32ToFloat4(theme::kBorder | 0xFF000000u);
  style.Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(theme::kPanel | 0xFA000000u);
  style.Colors[ImGuiCol_CheckMark] =
      ImGui::ColorConvertU32ToFloat4(theme::kAccentTeal | 0xFF000000u);
}

const char* icon_for(const Card& card) {
  if (card.kind == CardKind::Template) return "+";
  if (card.kind == CardKind::Notice) return "i";
  return "";
}

}  // namespace

UiAction HubUi::frame(float /*dt*/) {
  UiAction action;
  if (!state_) return action;
  if (focused_ && !was_focused_) state_->refresh();  // picking up a file drop or an external edit
  was_focused_ = focused_;

  const std::uint32_t accent = theme::accent_for(state_->manager().settings().accent);

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width_, height_), ImGuiCond_Always);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoNavFocus;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(theme::kWindow | 0xFF000000u));
  ImGui::Begin("##hub", nullptr, flags);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();

  // ---- top bar ---------------------------------------------------------------------
  dl->AddRectFilled(origin, ImVec2(origin.x + width_, origin.y + theme::kTopBarH),
                    theme::kPanel | 0xFF000000u);
  dl->AddLine(ImVec2(origin.x, origin.y + theme::kTopBarH),
              ImVec2(origin.x + width_, origin.y + theme::kTopBarH),
              theme::kBorder | 0xFF000000u);
  ImGui::SetCursorScreenPos(ImVec2(origin.x + theme::kGridPad, origin.y + 16.0f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(theme::kTextHi | 0xFF000000u));
  ImGui::TextUnformatted("lume");
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::SetCursorPosX(origin.x + 92.0f);
  ImGui::TextDisabled("hub");

  // search
  ImGui::SetCursorScreenPos(ImVec2(origin.x + width_ - 340.0f, origin.y + 14.0f));
  ImGui::PushItemWidth(200.0f);
  char query[128];
  std::snprintf(query, sizeof query, "%s", state_->query().c_str());
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(theme::kField | 0xFF000000u));
  if (ImGui::InputTextWithHint("##search", "Search projects", query, sizeof query)) {
    state_->set_query(query);
  }
  ImGui::PopStyleColor();
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("New", ImVec2(72.0f, 26.0f))) {
    pending_template_ = state_->manager().settings().default_template;
    open_new_project_dialog();
  }

  // ---- sidebar ---------------------------------------------------------------------
  const float sidebar_x = origin.x;
  const float sidebar_y = origin.y + theme::kTopBarH;
  dl->AddRectFilled(ImVec2(sidebar_x, sidebar_y),
                    ImVec2(sidebar_x + theme::kSidebarW, origin.y + height_),
                    theme::kPanel | 0xFF000000u);
  dl->AddLine(ImVec2(sidebar_x + theme::kSidebarW, sidebar_y),
              ImVec2(sidebar_x + theme::kSidebarW, origin.y + height_),
              theme::kBorder | 0xFF000000u);

  ImGui::SetCursorScreenPos(ImVec2(sidebar_x + 12.0f, sidebar_y + 16.0f));
  for (Tab tab : {Tab::Recent, Tab::Projects, Tab::Templates, Tab::Community}) {
    const bool selected = state_->tab() == tab;
    if (selected) {
      const ImVec2 p = ImGui::GetCursorScreenPos();
      dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + theme::kSidebarW - 24.0f, p.y + 30.0f),
                        theme::kRaised | 0xFF000000u, theme::kRadius);
      dl->AddRectFilled(ImVec2(p.x, p.y + 6.0f), ImVec2(p.x + 3.0f, p.y + 24.0f),
                        accent | 0xFF000000u, 2.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, selected ? ImGui::ColorConvertU32ToFloat4(theme::kTextHi | 0xFF000000u)
                                                  : ImGui::ColorConvertU32ToFloat4(theme::kTextDim | 0xFF000000u));
    char label[64];
    std::snprintf(label, sizeof label, "%s  %zu", HubState::tab_label(tab), state_->count_in(tab));
    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_None, ImVec2(theme::kSidebarW - 24.0f, 30.0f))) {
      state_->set_tab(tab);
    }
    ImGui::PopStyleColor();
  }

  ImGui::SetCursorScreenPos(ImVec2(sidebar_x + 12.0f, origin.y + height_ - 96.0f));
  ImGui::TextDisabled("sort");
  ImGui::SetCursorScreenPos(ImVec2(sidebar_x + 12.0f, origin.y + height_ - 76.0f));
  if (ImGui::BeginCombo("##sort", HubState::sort_label(state_->sort()))) {
    for (SortBy s : {SortBy::LastOpened, SortBy::Name, SortBy::LastModified, SortBy::Created}) {
      bool is_sel = state_->sort() == s;
      if (ImGui::Selectable(HubState::sort_label(s), &is_sel)) state_->set_sort(s);
    }
    ImGui::EndCombo();
  }

  // ---- grid ------------------------------------------------------------------------
  const float grid_x = sidebar_x + theme::kSidebarW + theme::kGridPad;
  const float grid_y = sidebar_y + theme::kGridPad;
  const int cols = columns_for_width(width_ - theme::kSidebarW);
  const auto& cards = state_->cards();

  ImGui::BeginGroup();
  ImGui::SetCursorScreenPos(ImVec2(grid_x, grid_y));
  for (std::size_t i = 0; i < cards.size(); ++i) {
    const Card& card = cards[i];
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(static_cast<int>(i));

    dl->AddRectFilled(p, ImVec2(p.x + theme::kCardW, p.y + theme::kCardH),
                      theme::kCard | 0xFF000000u, theme::kRadius);
    // Thumbnail: a render when one exists, otherwise the card's own tint gradient.
    dl->AddRectFilled(ImVec2(p.x + 1, p.y + 1),
                      ImVec2(p.x + theme::kCardW - 1.0f, p.y + theme::kThumbH),
                      theme::kThumbTop | 0xFF000000u, theme::kRadius - 1.0f, ImDrawFlags_RoundCornersTop);
    if (!card.thumbnail.empty() && draw_image_) {
      draw_image_(card.thumbnail, p.x + 1.0f, p.y + 1.0f, theme::kCardW - 2.0f, theme::kThumbH - 2.0f);
    } else if (card.tint) {
      dl->AddRectFilledMultiColor(ImVec2(p.x + 1, p.y + 1),
                                  ImVec2(p.x + theme::kCardW - 1.0f, p.y + theme::kThumbH),
                                  card.tint | 0x33000000u, card.tint | 0x1A000000u,
                                  theme::kThumbBottom | 0x00000000u, theme::kThumbBottom | 0x00000000u);
    }
    if (!icon_for(card).empty()) {
      dl->AddText(ImVec2(p.x + theme::kCardW - 26.0f, p.y + 10.0f),
                  theme::kTextDim | 0xFF000000u, icon_for(card));
    }

    dl->AddText(ImVec2(p.x + theme::kTextPad, p.y + theme::kTextTitle),
                theme::kTextHi | 0xFF000000u, card.title.c_str());
    dl->AddText(ImVec2(p.x + theme::kTextPad, p.y + theme::kTextMeta),
                theme::kTextDim | 0xFF000000u, card.subtitle.c_str());
    dl->AddText(ImVec2(p.x + theme::kTextPad, p.y + theme::kTextNote),
                theme::kTextFaint | 0xFF000000u, card.note.c_str());
    if (!card.badge.empty()) {
      // The badge sits on the thumbnail, bottom-right: the text block below is already full.
      const char* b = card.badge.c_str();
      const ImVec2 ts = ImGui::CalcTextSize(b);
      const ImVec2 bp(p.x + theme::kCardW - ts.x - 24.0f, p.y + theme::kThumbH - ts.y - 14.0f);
      dl->AddRectFilled(bp, ImVec2(bp.x + ts.x + 10.0f, bp.y + ts.y + 6.0f),
                        theme::kPanel | 0xFF000000u, 4.0f);
      dl->AddText(ImVec2(bp.x + 5.0f, bp.y + 3.0f), accent | 0xFF000000u, b);
    }
    dl->AddRect(p, ImVec2(p.x + theme::kCardW, p.y + theme::kCardH),
                theme::kBorder | 0xFF000000u, theme::kRadius, 0, 1.0f);

    // One invisible button drives hover, click and the context menu.
    if (ImGui::InvisibleButton("##card", ImVec2(theme::kCardW, theme::kCardH))) {
      action = state_->activate(i);
      action.card_index = i;
    }
    if (ImGui::IsItemHovered()) {
      dl->AddRect(p, ImVec2(p.x + theme::kCardW, p.y + theme::kCardH), accent | 0xFF000000u,
                  theme::kRadius, 0, 1.5f);
      state_->set_message(card.subtitle);
    }
    if (ImGui::BeginPopupContextItem("##cardmenu")) {
      if (ImGui::MenuItem("Open")) action = state_->activate(i);
      if (ImGui::MenuItem("Duplicate")) action = state_->secondary(i, 1);
      if (ImGui::MenuItem("Show in folder")) action = state_->secondary(i, 0);
      ImGui::Separator();
      if (ImGui::MenuItem("Delete", nullptr, false, card.kind == CardKind::Project)) {
        action.type = UiAction::Type::DeleteProject;
        action.card_index = i;
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();

    const std::size_t row = i / static_cast<std::size_t>(cols);
    const std::size_t col = i % static_cast<std::size_t>(cols);
    if (col + 1 < static_cast<std::size_t>(cols) && i + 1 < cards.size()) {
      ImGui::SameLine();
      ImGui::SetCursorScreenPos(
          ImVec2(grid_x + static_cast<float>(col + 1) * (theme::kCardW + theme::kCardGap),
                 grid_y + static_cast<float>(row) * (theme::kCardH + theme::kCardGap)));
    } else if (i + 1 < cards.size()) {
      ImGui::SetCursorScreenPos(ImVec2(grid_x, grid_y + (row + 1) * (theme::kCardH + theme::kCardGap)));
    }
  }
  ImGui::EndGroup();

  // ---- new project dialog ----------------------------------------------------------
  if (new_project_open_) {
    ImGui::OpenPopup("New project");
    new_project_open_ = false;
  }
  static char name_buf[kNewProjectNameCap] = {0};
  if (ImGui::BeginPopupModal("New project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("Name", name_buf, sizeof name_buf, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextDisabled("%s", state_->footer_path().c_str());
    if (!new_project_error_.empty()) {
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::kDanger | 0xFF000000u), "%s",
                         new_project_error_.c_str());
    }
    if (ImGui::Button("Create")) {
      // Validating here would duplicate HubState's rules, so the name travels out and the
      // caller performs the write; only "is it blank" is answered locally.
      if (std::strlen(name_buf) == 0) {
        new_project_error_ = "give the project a name";
      } else {
        new_project_error_.clear();
        action.type = UiAction::Type::CreateProject;
        action.text = name_buf;
        action.template_id = pending_template_;
        std::memset(name_buf, 0, sizeof name_buf);
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      new_project_error_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // ---- status strip ----------------------------------------------------------------
  dl->AddRectFilled(ImVec2(origin.x, origin.y + height_ - theme::kFooterH),
                    ImVec2(origin.x + width_, origin.y + height_),
                    theme::kPanel | 0xFF000000u);
  dl->AddText(ImVec2(origin.x + theme::kSidebarW + theme::kGridPad,
                     origin.y + height_ - theme::kFooterH + 7.0f),
              theme::kTextDim | 0xFF000000u, state_->status_line().c_str());

  ImGui::End();
  ImGui::PopStyleColor();

  // A template card click opens the dialog instead of creating immediately - every Studio
  // hub asks for a name first.
  if (action.type == UiAction::Type::CreateProject && !action.text.empty() &&
      action.card_index < cards.size() && cards[action.card_index].kind == CardKind::Template) {
    pending_template_ = action.text;
    new_project_open_ = true;
    action.type = UiAction::Type::None;
    action.text.clear();
  }

  if (action.type == UiAction::Type::DeleteProject) {
    std::string err;
    if (!state_->delete_at(action.card_index, &err)) {
      message_ = err;
      action.type = UiAction::Type::None;
    } else {
      message_ = "Deleted";
    }
  }
  return action;
}

#else  // LUME_WITH_IMGUI ------------------------------------------------------------

UiAction HubUi::frame(float) {
  // No immediate-mode build in this configuration: the headless hub (lume-hub --print) is
  // the product here until Dear ImGui is vendored next to Filament in step 2.
  if (focused_ && !was_focused_ && state_) state_->refresh();
  was_focused_ = focused_;
  return UiAction{};
}

#endif

}  // namespace lume::hub
