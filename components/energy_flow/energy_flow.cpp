#include "energy_flow.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>

namespace esphome::energy_flow {

static const char *const TAG = "energy_flow";

using tessera::Font;

namespace ui = tessera::ui;

static constexpr int DOTS_MAX = 6;       // most dots per line; the option "dots" says how many are made
static constexpr float MIN_WATT = 1.0f;  // below this a line counts as idle

// Symbols of Tessera's set (Material Design Icons codepoints). A codepoint the set does not have shows as nothing.
static constexpr uint32_t ICON_SOLAR = 0xF0A0F;    // solar panel with a bolt
static constexpr uint32_t ICON_GRID = 0xF0D3F;     // transmission tower
static constexpr uint32_t ICON_HOME = 0xF02DC;     // house
static constexpr uint32_t ICON_BATTERY = 0xF0079;  // battery
static constexpr uint32_t ICON_BOLT = 0xF0241;     // bolt (wallbox and Speicher2)

static float read(const sensor::Sensor *s) { return s ? s->state : NAN; }
// A text option of the tile ("" when empty).
static std::string opt_text(const tessera::TileContext &c, const char *id) {
  const char *v = c.options[id] | "";
  return v ? std::string(v) : std::string();
}

static float nz(float v) { return std::isnan(v) ? 0.0f : v; }

// "850 W", "1.2 kW" (the direction is shown by a prefix or by the dots, not by a minus).
static std::string watts(const tessera::Plugin *plugin, float w) {
  if (std::isnan(w)) return "--";
  char buf[24];
  const float a = std::fabs(w);
  if (a >= 1000.0f)
    snprintf(buf, sizeof buf, "%.1f kW", a / 1000.0f);
  else
    snprintf(buf, sizeof buf, "%.0f W", a);
  std::string out = buf;
  const std::string decimal = plugin->text("decimal");
  if (!decimal.empty() && decimal[0] == ',') std::replace(out.begin(), out.end(), '.', ',');
  return out;
}

// One circle of the picture. Rows from top to bottom: top (optional), icon, b1, b2 (optional); the name sits outside.
struct Node {
  lv_obj_t *ring{}, *icon{}, *name{}, *top{}, *b1{}, *b2{};
  theme::Role role{theme::INK};
};

// One line of the picture: a path of 2 or 3 points and the dots that run along it.
// dir: +1 the energy flows from the first to the last point, -1 the other way, 0 nothing flows.
struct Line {
  lv_obj_t *seg[2]{};
  lv_obj_t *dots[DOTS_MAX]{};
  int n = 0;
  int px[3]{}, py[3]{};
  float seg_len[2]{}, total = 1.0f;
  int last_x[DOTS_MAX], last_y[DOTS_MAX];  // where the dots are now, so a dot is moved only when it moved
  bool shown[DOTS_MAX]{};
  float phase = 0.0f, watt = 0.0f;
  int dir = 0;
  theme::Role colour{theme::MARK_AMBER};
};

class FlowTile : public tessera::Tile {
 public:
  explicit FlowTile(EnergyFlow *plugin) : plugin_(plugin) {}
  ~FlowTile() override { plugin_->detach(this); }

  void create(const tessera::TileContext &c) override {
    const int w = c.width, h = c.height;
    max_power_ = std::max(100.0f, static_cast<float>(c.options["max_power"] | 2000));
    invert_grid_ = c.options["invert_grid"] | true;
    invert_battery_ = c.options["invert_battery"] | true;
    invert_storage2_ = c.options["invert_storage2"] | false;
    animate_ = true;
    calc_home_ = c.options["home_calc"] | false;
    fps_ = std::min(4, std::max(1, static_cast<int>(c.options["fps"] | 2)));
    dots_ = std::min(DOTS_MAX, std::max(1, static_cast<int>(c.options["dots"] | 2)));

    lbl_pv_ = opt_text(c, "label_pv");
    lbl_wallbox_ = opt_text(c, "label_wallbox");
    lbl_grid_ = opt_text(c, "label_grid");
    lbl_battery_ = opt_text(c, "label_battery");
    lbl_storage2_ = opt_text(c, "label_storage2");

    th_ = ui::line_height(Font::BODY);
    // Three rows of circles, a name above the top row and below the bottom row, and room between the rows for lines.
    d_ = std::min(static_cast<int>(w * 0.26f), static_cast<int>((h - 2 * th_) * 0.26f));

    // A tile too small for the picture shows four lines of text instead: no circles, no dots, a handful of objects.
    compact_ = w < ui::px(150) || h < ui::px(110) || d_ < th_ * 13 / 10;
    if (compact_) {
      create_compact(c);
      return;
    }

    icon_font_ = d_ >= 3 * th_ + ui::line_height(Font::ICON) ? Font::ICON : Font::ICON_SMALL;
    ih_ = ui::line_height(icon_font_);
    dot_ = ui::px(ui::large() ? 9 : 6);
    thick_ = std::max(2, ui::px(3));

    const int R = d_ / 2;
    const int cx0 = R, cx1 = w / 2, cx2 = w - R;                      // columns: grid | PV, battery | home, loads
    const int cy0 = th_ + R, cy1 = h / 2, cy2 = h - th_ - R;          // rows: PV, wallbox | grid, home | battery, Speicher2
    const int o = std::max(2, R / 3), hx = R / 2;                      // where parallel lines enter a circle

    // Lines first, so the circles sit above them. All paths run horizontally or vertically.
    setup_line(l_gh_, c.parent, theme::MARK_BLUE, {cx0 + R, cx2 - R}, {cy1, cy1});
    setup_line(l_sb_, c.parent, theme::MARK_AMBER, {cx1, cx1}, {cy0 + R, cy2 - R});
    setup_line(l_sh_, c.parent, theme::MARK_AMBER, {cx1 + hx, cx1 + hx, cx2 - R}, {cy0 + R, cy1 - o, cy1 - o});
    setup_line(l_bh_, c.parent, theme::ACCENT, {cx1 + hx, cx1 + hx, cx2 - R}, {cy2 - R, cy1 + o, cy1 + o});
    setup_line(l_sg_, c.parent, theme::MARK_AMBER, {cx1 - hx, cx1 - hx, cx0 + R}, {cy0 + R, cy1 - o, cy1 - o});
    setup_line(l_gb_, c.parent, theme::MARK_BLUE, {cx0 + R, cx1 - hx, cx1 - hx}, {cy1 + o, cy1 + o, cy2 - R});
    setup_line(l_hw_, c.parent, theme::MARK_GREEN, {cx2, cx2}, {cy1 - R, cy0 + R});
    setup_line(l_hm_, c.parent, theme::MARK_PURPLE, {cx2, cx2}, {cy1 + R, cy2 - R});

    const int ring = std::max(2, ui::px(2));
    //                 node   parent    x    y    colour              ring       tops bots symbol       name  above
    setup_node(pv_, c.parent, cx1, cy0, theme::MARK_AMBER, ring, 1, 1, ICON_SOLAR, true, true);
    setup_node(wall_, c.parent, cx2, cy0, theme::MARK_GREEN, ring, 0, 1, ICON_BOLT, true, true);
    setup_node(grid_, c.parent, cx0, cy1, theme::MARK_BLUE, ring, 0, 2, ICON_GRID, true, false);
    setup_node(home_, c.parent, cx2, cy1, theme::MARK_AMBER, std::max(3, ui::px(3)), 0, 1, ICON_HOME, false, false);
    setup_node(batt_, c.parent, cx1, cy2, theme::ACCENT, ring, 1, 2, ICON_BATTERY, true, false);
    setup_node(mar_, c.parent, cx2, cy2, theme::MARK_PURPLE, ring, 0, 1, ICON_BOLT, true, false);

    plugin_->attach(this);
  }

  // Once a second: read the sensors, set the texts and the direction of every line.
  void on_tick(uint32_t) override { refresh(); }

  void on_theme() override {
    if (compact_) {
      for (int i = 0; i < 4; i++) ui::set_color(text_[i], text_role_[i]);
      return;
    }
    for (Node *n : nodes()) {
      lv_obj_set_style_border_color(n->ring, ui::color(n->role), 0);
      for (lv_obj_t *l : {n->icon, n->top, n->b1, n->b2})
        if (l) ui::set_color(l, theme::INK);
      if (n->name) ui::set_color(n->name, theme::MUTED);
    }
    for (Line *l : lines()) {
      for (int i = 0; i < dots_; i++) lv_obj_set_style_bg_color(l->dots[i], ui::color(l->colour), 0);
    }
    refresh();
  }

  // Every 250 ms (called by the plugin): move the dots along their lines, at most `fps` times a second.
  void animate(uint32_t now_ms) {
    if (compact_ || !animate_) return;
    const uint32_t every = 1000u / static_cast<uint32_t>(fps_);
    if (last_ms_ && now_ms - last_ms_ + 30u < every) return;
    uint32_t dt = last_ms_ ? now_ms - last_ms_ : 250;
    last_ms_ = now_ms;
    if (dt > 1000) dt = 1000;
    for (Line *l : lines()) {
      if (l->dir == 0) continue;
      const float speed = 0.2f + 1.3f * std::min(1.0f, l->watt / max_power_);  // rounds per second
      l->phase += speed * static_cast<float>(dt) / 1000.0f;
      l->phase -= std::floor(l->phase);
      place(*l);
    }
  }

 private:
  std::array<Line *, 8> lines() { return {&l_gh_, &l_sb_, &l_sh_, &l_bh_, &l_sg_, &l_gb_, &l_hw_, &l_hm_}; }
  std::array<Node *, 6> nodes() { return {&pv_, &wall_, &grid_, &home_, &batt_, &mar_}; }

  void create_compact(const tessera::TileContext &c) {
    const int lh = ui::line_height(Font::BODY);
    const theme::Role roles[4] = {theme::MARK_AMBER, theme::INK, theme::MARK_BLUE, theme::ACCENT};
    for (int i = 0; i < 4; i++) {
      text_role_[i] = roles[i];
      text_[i] = ui::label(c.parent, Font::BODY, roles[i]);
      lv_obj_set_size(text_[i], c.width, lh);
      lv_obj_set_pos(text_[i], 0, i * lh);
      fits_[i] = i == 0 || (i + 1) * lh <= c.height;
    }
  }

  // Does a circle have room for `texts` rows of text and the icon?
  bool fits(int texts) const { return texts * th_ + ih_ <= d_ * 9 / 10; }

  void setup_line(Line &l, lv_obj_t *parent, theme::Role colour, std::initializer_list<int> xs,
                  std::initializer_list<int> ys) {
    l.colour = colour;
    l.n = static_cast<int>(xs.size());
    int k = 0;
    for (int v : xs) l.px[k++] = v;
    k = 0;
    for (int v : ys) l.py[k++] = v;
    l.total = 0.0f;
    for (int s = 0; s + 1 < l.n; s++) {
      const int dx = l.px[s + 1] - l.px[s], dy = l.py[s + 1] - l.py[s];
      const int len = std::abs(dx) + std::abs(dy);
      l.seg_len[s] = static_cast<float>(len);
      l.total += static_cast<float>(len);
      lv_obj_t *t = ui::block(parent, theme::TRACK);
      if (dx == 0) {  // vertical
        lv_obj_set_size(t, thick_, std::max(1, len) + thick_);
        lv_obj_set_pos(t, l.px[s] - thick_ / 2, std::min(l.py[s], l.py[s + 1]) - thick_ / 2);
      } else {  // horizontal
        lv_obj_set_size(t, std::max(1, len) + thick_, thick_);
        lv_obj_set_pos(t, std::min(l.px[s], l.px[s + 1]) - thick_ / 2, l.py[s] - thick_ / 2);
      }
      l.seg[s] = t;
    }
    if (l.total < 1.0f) l.total = 1.0f;
    for (int i = 0; i < dots_; i++) {
      l.last_x[i] = INT_MIN;
      l.last_y[i] = INT_MIN;
      lv_obj_t *d = ui::block(parent, colour);
      lv_obj_set_size(d, dot_, dot_);
      lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_opa(d, LV_OPA_TRANSP, 0);
      l.dots[i] = d;
    }
  }

  // A circle with its rows. Too small for all rows: the extra row below goes first, then the top row, then the icon.
  void setup_node(Node &n, lv_obj_t *parent, int cx, int cy, theme::Role role, int ring_w, int tops, int bots,
                  uint32_t symbol, bool named, bool name_above) {
    n.role = role;
    n.ring = ui::block(parent, theme::TRACK);
    lv_obj_set_size(n.ring, d_, d_);
    lv_obj_set_pos(n.ring, cx - d_ / 2, cy - d_ / 2);
    lv_obj_set_style_radius(n.ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(n.ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(n.ring, ring_w, 0);
    lv_obj_set_style_border_color(n.ring, ui::color(role), 0);

    // 0 all rows; 1 the top row, the icon and one row below; 2 the icon and one row; 3 the one row alone.
    int mode = 0;
    if (!fits(tops + bots)) {
      if (tops > 0 && bots > 1 && fits(tops + 1))
        mode = 1;
      else
        mode = fits(1) ? 2 : 3;
    }
    const int rows_top = mode <= 1 ? tops : 0;
    const int rows_bot = mode == 0 ? bots : 1;
    const bool has_icon = mode <= 2;
    const int total = (rows_top + rows_bot) * th_ + (has_icon ? ih_ : 0);
    int y = cy - total / 2;
    const int tw = d_ * 92 / 100;
    auto row = [&](Font f, int hgt) -> lv_obj_t * {
      lv_obj_t *l = ui::label(parent, f, theme::INK);
      lv_obj_set_size(l, tw, hgt);
      lv_obj_set_pos(l, cx - tw / 2, y);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
      y += hgt;
      return l;
    };
    if (rows_top >= 1) n.top = row(Font::BODY, th_);
    if (has_icon) {
      n.icon = row(icon_font_, ih_);
      ui::set_text(n.icon, ui::icon(symbol));
    }
    n.b1 = row(Font::BODY, th_);
    if (rows_bot >= 2) n.b2 = row(Font::BODY, th_);

    if (named) {
      const int nw = d_ * 3 / 2;
      n.name = ui::label(parent, Font::BODY, theme::MUTED);
      lv_obj_set_size(n.name, nw, th_);
      lv_obj_set_pos(n.name, cx - nw / 2, name_above ? cy - d_ / 2 - th_ : cy + d_ / 2);
      lv_obj_set_style_text_align(n.name, LV_TEXT_ALIGN_CENTER, 0);
    }
  }

  // The dots of a line, spread evenly along its path, from the first point toward the last.
  // A dot is moved only when its place changed.
  void place(Line &l) {
    const int segs = l.n - 1;
    for (int i = 0; i < dots_; i++) {
      float t = l.phase + static_cast<float>(i) / static_cast<float>(dots_);
      t -= std::floor(t);
      if (!animate_) t = 0.5f;
      if (l.dir < 0) t = 1.0f - t;
      float s = t * l.total;
      int k = 0;
      if (segs > 1 && s > l.seg_len[0]) {
        s -= l.seg_len[0];
        k = 1;
      }
      float f = l.seg_len[k] > 0.0f ? s / l.seg_len[k] : 0.0f;
      f = std::min(1.0f, std::max(0.0f, f));
      const int x = l.px[k] + static_cast<int>(std::lround((l.px[k + 1] - l.px[k]) * f)) - dot_ / 2;
      const int y = l.py[k] + static_cast<int>(std::lround((l.py[k + 1] - l.py[k]) * f)) - dot_ / 2;
      if (x == l.last_x[i] && y == l.last_y[i]) continue;
      l.last_x[i] = x;
      l.last_y[i] = y;
      lv_obj_set_pos(l.dots[i], x, y);
    }
  }

  void flow(Line &l, float watt, int dir) {
    l.watt = std::isnan(watt) ? 0.0f : std::fabs(watt);
    l.dir = l.watt >= MIN_WATT ? dir : 0;
    for (int i = 0; i < dots_; i++) {
      const bool show = l.dir != 0 && (animate_ || i == 0);
      if (show == l.shown[i]) continue;
      l.shown[i] = show;
      lv_obj_set_style_opa(l.dots[i], show ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    if (l.dir != 0) place(l);
  }

  // The text a person typed for a circle, or the translated standard name when the field is empty.
  std::string name(const std::string &custom, const char *key) const {
    return custom.empty() ? std::string(plugin_->text(key)) : custom;
  }

  // The battery label may hold the placeholder {soc}; it is replaced by the charge level and follows it.
  static bool has_soc_tag(const std::string &t) { return t.find("{soc}") != std::string::npos; }
  static std::string with_soc(std::string t, const std::string &soc_text) {
    for (size_t at = t.find("{soc}"); at != std::string::npos; at = t.find("{soc}", at + soc_text.size()))
      t.replace(at, 5, soc_text);
    return t;
  }

  static void put(lv_obj_t *label, const std::string &text) {
    if (label) ui::set_text(label, text);
  }

  void refresh() {
    EnergyFlow *p = plugin_;
    const float solar = read(p->solar());
    const float inverter = read(p->inverter());
    const float grid_raw = read(p->grid());
    const float battery_raw = read(p->battery());
    const float soc = read(p->soc());
    const float home_raw = read(p->home());
    const float wallbox = read(p->wallbox());
    const float storage2_raw = read(p->storage2());

    // Every 10 s the raw values of all sensors go to the log, so a wrong or swapped sensor shows up there.
    const uint32_t now = millis();
    if (now - last_log_ >= 10000u || last_log_ == 0) {
      last_log_ = now;
      ESP_LOGI(TAG, "raw: solar=%.1f inverter=%.1f grid=%.1f battery=%.1f soc=%.1f home=%.1f wallbox=%.1f storage2=%.1f",
               solar, inverter, grid_raw, battery_raw, soc, home_raw, wallbox, storage2_raw);
    }

    // After the inversion a positive grid value is a draw from the grid, a positive battery value a discharge,
    // a positive Speicher2 or wallbox value a consumption (the way power-flow-card-plus reads them).
    const float grid = invert_grid_ ? -grid_raw : grid_raw;
    const float battery = invert_battery_ ? -battery_raw : battery_raw;
    const float storage2 = invert_storage2_ ? -storage2_raw : storage2_raw;

    // "Calculate home from the balance": everything that comes in (solar, grid draw, battery discharge, Speicher2
    // discharge) is used by the house and the wallbox. The Speicher2 is positive while it takes power, so it is subtracted.
    // A missing battery or Speicher2 value counts as 0; without solar or grid there is no balance.
    float home = home_raw;
    if (calc_home_) {
      if (std::isnan(solar) || std::isnan(grid))
        home = NAN;
      else
        home = std::max(0.0f, solar + grid + nz(battery) - nz(storage2));
    }

    const float grid_in = std::max(0.0f, nz(grid)), grid_out = std::max(0.0f, -nz(grid));
    const float batt_dis = std::max(0.0f, nz(battery)), batt_chg = std::max(0.0f, -nz(battery));

    char soc_text[16];
    if (std::isnan(soc))
      snprintf(soc_text, sizeof soc_text, "--");
    else
      snprintf(soc_text, sizeof soc_text, "%.0f %%", soc);
    const bool soc_in_label = has_soc_tag(lbl_battery_);

    if (compact_) {
      std::string battery_name = name(lbl_battery_, "battery");
      if (soc_in_label) {
        battery_name = with_soc(battery_name, soc_text);
      } else if (!std::isnan(soc)) {
        char buf[16];
        snprintf(buf, sizeof buf, " %.0f%%", soc);
        battery_name += buf;
      }
      const std::string lines_text[4] = {
          name(lbl_pv_, "solar") + " " + watts(p, solar),
          std::string(p->text("home")) + " " + watts(p, home),
          (lbl_grid_.empty() ? std::string(p->text(grid < 0 ? "grid_export" : "grid_import")) : lbl_grid_) + " " + watts(p, grid),
          battery_name + " " + watts(p, battery),
      };
      for (int i = 0; i < 4; i++) ui::set_text(text_[i], fits_[i] ? lines_text[i] : std::string());
      return;
    }

    // Who feeds whom (the split of power-flow-card-plus): the battery is charged by the sun first, then by the grid;
    // an export comes from the sun first, then from the battery; what is left of the sun goes to the home.
    const float s = std::max(0.0f, nz(solar));
    const float grid_to_batt = std::min(grid_in, std::max(0.0f, batt_chg - s));
    const float solar_to_batt = std::min(s, batt_chg - grid_to_batt);
    const float solar_avail = s - solar_to_batt;
    const float solar_to_grid = std::min(solar_avail, grid_out);
    const float batt_to_grid = std::min(batt_dis, std::max(0.0f, grid_out - solar_to_grid));
    const float solar_to_home = std::max(0.0f, solar_avail - solar_to_grid);
    const float batt_to_home = std::max(0.0f, batt_dis - batt_to_grid);
    const float grid_to_home = std::max(0.0f, grid_in - grid_to_batt);

    flow(l_gh_, grid_to_home, +1);
    flow(l_sb_, solar_to_batt, +1);
    flow(l_sh_, solar_to_home, +1);
    flow(l_bh_, batt_to_home, +1);
    flow(l_sg_, solar_to_grid, +1);
    flow(l_gb_, std::max(grid_to_batt, batt_to_grid), grid_to_batt >= batt_to_grid ? +1 : -1);
    flow(l_hw_, wallbox, wallbox >= 0 ? +1 : -1);
    flow(l_hm_, storage2, storage2 >= 0 ? +1 : -1);

    // PV: the inverter input on top, the total below the symbol.
    put(pv_.top, watts(p, inverter));
    put(pv_.b1, watts(p, solar));
    put(pv_.name, name(lbl_pv_, "solar"));

    // Grid: "<" feed-in, ">" draw. A circle with one row shows whichever is active.
    if (grid_.b2) {
      put(grid_.b1, "< " + watts(p, grid_out));
      put(grid_.b2, "> " + watts(p, grid_in));
    } else {
      put(grid_.b1, grid_out >= MIN_WATT ? "< " + watts(p, grid_out) : "> " + watts(p, grid_in));
    }
    put(grid_.name, name(lbl_grid_, "grid"));

    put(home_.b1, watts(p, home));

    put(wall_.b1, watts(p, wallbox));
    put(wall_.name, name(lbl_wallbox_, "wallbox"));

    // Battery: the charge level on top, "+" charging and "-" discharging below the symbol.
    // With {soc} in the label (and a label shown) the level is in the label and the top row stays empty.
    const bool soc_shown_in_label = soc_in_label && batt_.name;
    put(batt_.top, soc_shown_in_label ? std::string() : std::string(soc_text));
    if (!batt_.top && !soc_shown_in_label) {
      put(batt_.b1, soc_text);  // too small for the top row: the level takes the one row
    } else if (batt_.b2) {
      put(batt_.b1, "+ " + watts(p, batt_chg));
      put(batt_.b2, "- " + watts(p, batt_dis));
    } else {
      put(batt_.b1, batt_chg >= MIN_WATT ? "+ " + watts(p, batt_chg)
                                         : (batt_dis >= MIN_WATT ? "- " + watts(p, batt_dis) : watts(p, 0.0f)));
    }
    put(batt_.name, with_soc(name(lbl_battery_, "battery"), soc_text));

    const std::string mar_prefix = std::fabs(nz(storage2)) < MIN_WATT ? "" : (storage2 >= 0 ? "+ " : "- ");
    put(mar_.b1, mar_prefix + watts(p, storage2));
    put(mar_.name, name(lbl_storage2_, "storage2"));
  }

  EnergyFlow *plugin_;
  Node pv_, wall_, grid_, home_, batt_, mar_;
  Line l_gh_, l_sb_, l_sh_, l_bh_, l_sg_, l_gb_, l_hw_, l_hm_;
  lv_obj_t *text_[4]{};
  theme::Role text_role_[4]{theme::INK, theme::INK, theme::INK, theme::INK};
  bool fits_[4]{};
  Font icon_font_ = Font::ICON_SMALL;
  int th_ = 0, ih_ = 0, d_ = 0, dot_ = 0, thick_ = 0, fps_ = 2, dots_ = 2;
  float max_power_ = 2000.0f;
  bool invert_grid_ = true, invert_battery_ = true, invert_storage2_ = false, animate_ = true, compact_ = false,
       calc_home_ = false;
  std::string lbl_pv_, lbl_wallbox_, lbl_grid_, lbl_battery_, lbl_storage2_;
  uint32_t last_ms_ = 0, last_log_ = 0;
};

void EnergyFlow::setup() {
  add_tile("flow", [this]() { return new FlowTile(this); });
}

void EnergyFlow::attach(FlowTile *tile) { tiles_.push_back(tile); }

void EnergyFlow::detach(FlowTile *tile) { tiles_.erase(std::remove(tiles_.begin(), tiles_.end(), tile), tiles_.end()); }

void EnergyFlow::on_interval(uint32_t now_ms) {
  for (FlowTile *tile : tiles_) tile->animate(now_ms);
}

}  // namespace esphome::energy_flow
