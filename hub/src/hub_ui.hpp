#pragma once
// The hub's frame. Header is renderer-agnostic on purpose: the image callback takes a path
// and a rect, so neither Filament nor ImGui types leak into the hub's logic.
//
// hub_ui.cpp compiles twice: without LUME_WITH_IMGUI it draws nothing (the headless hub, and
// every CI job that has no Dear ImGui drop yet), with it the whole dark hub is drawn.

#include <functional>
#include <string>

#include "hub_state.hpp"
#include "lume/fs.hpp"

namespace lume::hub {

using ImageDrawer = std::function<void(const std::string& path, float x, float y, float w, float h)>;

class HubUi {
 public:
  HubUi(HubState* state, float width, float height)
      : state_(state), width_(width), height_(height) {}

  void set_image_drawer(ImageDrawer drawer) { draw_image_ = std::move(drawer); }
  void set_viewport(float width, float height) {
    width_ = width;
    height_ = height;
  }
  void set_focused(bool focused) { focused_ = focused; }

  // One frame: paint, and hand back whatever the user just asked for.
  UiAction frame(float dt);

  const std::string& message() const { return message_; }
  void set_message(const std::string& message) { message_ = message; }
  bool wants_quit() const { return quit_; }
  void request_quit() { quit_ = true; }

  // Grid metrics, exposed so tests can assert the layout reflows instead of overflowing.
  int columns_for_width(float width) const;

 private:
  bool open_new_project_dialog();

  HubState* state_ = nullptr;
  ImageDrawer draw_image_;
  float width_ = 1280.0f;
  float height_ = 720.0f;
  bool focused_ = true;
  bool was_focused_ = true;
  bool quit_ = false;
  std::string message_;
  bool new_project_open_ = false;
  std::string new_project_name_;
  std::string new_project_error_;
  std::string pending_template_;   // chosen before the name is typed
};

}  // namespace lume::hub
