#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// States, animations, days
// ---------------------------------------------------------------------------
const int GREEN = 0;
const int AMBER = 1;
const int RED = 2;
const int NAP = 3;

const int ANIM_NONE = 0;
const int ANIM_SUNSET = 1;
const int ANIM_SUNRISE = 2;
const int ANIM_NAP_START = 3;
const int ANIM_NAP_END = 4;
const int ANIM_DAY_START = 5;

// Matches ESPTime::day_of_week (1 = Sunday).
const int SUN = 1;
const int MON = 2;
const int TUE = 3;
const int WED = 4;
const int THU = 5;
const int FRI = 6;
const int SAT = 7;

inline int NextDay(int day) { return (day % 7) + 1; }
inline int PrevDay(int day) { return day == SUN ? SAT : day - 1; }

struct RGBColor { int r; int g; int b; };

inline int ClampByte(int v) { return std::max(0, std::min(255, v)); }

inline int SafePercent(float value, int fallback) {
  if (std::isnan(value)) return fallback;
  return std::max(0, std::min(100, (int) std::lround(value)));
}

inline int Lerp(int start, int end, float progress) {
  progress = std::max(0.0f, std::min(1.0f, progress));
  return (int) std::lround(start + (end - start) * progress);
}

inline RGBColor LerpColor(RGBColor start, RGBColor end, float progress) {
  return RGBColor{Lerp(start.r, end.r, progress), Lerp(start.g, end.g, progress), Lerp(start.b, end.b, progress)};
}

inline RGBColor RoundColor(float r, float g, float b) {
  return RGBColor{ClampByte((int) std::lround(r)), ClampByte((int) std::lround(g)), ClampByte((int) std::lround(b))};
}

// Displayed values of the original hues under gamma 2.8
// (originals: green 0,255,75  amber 255,143,0  nap 209,0,255  horizon 0,90,255).
const RGBColor GREEN_HUE = {0, 255, 8};
const RGBColor AMBER_HUE = {255, 50, 0};
const RGBColor RED_HUE = {255, 0, 0};
const RGBColor NAP_HUE = {146, 0, 255};
const RGBColor HORIZON_HUE = {0, 14, 255};

// Day/Night controls are percentages. Gamma is applied here because the
// ESPHome light itself uses gamma_correct: 1.0.
const float PERCEPTUAL_GAMMA = 2.2f;

// Convert a user-facing 0–100% brightness value to a raw WS2812 value.
// Zero is off; raw value 1 is valid on this matrix.
inline int PercentToRaw(int percent) {
  percent = std::max(0, std::min(100, percent));
  if (percent == 0) return 0;

  const float normalized = percent / 100.0f;
  const int raw = (int) std::lround(
    std::pow(normalized, PERCEPTUAL_GAMMA) * 255.0f
  );

  return std::max(1, std::min(255, raw));
}

// Color Test values are direct raw WS2812 channel values.
inline int TestLevelToRaw(int level) {
  return ClampByte(level);
}

inline RGBColor ScaleColor(RGBColor hue, int brightness) {
  brightness = ClampByte(brightness);
  const int max_channel = std::max({hue.r, hue.g, hue.b});
  if (brightness == 0 || max_channel == 0) {
    return RGBColor{0, 0, 0};
  }

  const float scale = (float) brightness / (float) max_channel;
  return RoundColor(hue.r * scale, hue.g * scale, hue.b * scale);
}

inline RGBColor DimColor(RGBColor c, float factor) {
  factor = std::max(0.0f, std::min(1.0f, factor));
  if (factor <= 0.0f) return RGBColor{0, 0, 0};

  auto ch = [factor](int v) {
    return std::max(0, (int) std::lround(v * factor));
  };

  return RGBColor{ch(c.r), ch(c.g), ch(c.b)};
}

inline RGBColor GetStateColor(int state, int dayPercent, int nightPercent) {
  dayPercent = std::max(0, std::min(100, dayPercent));
  nightPercent = std::max(0, std::min(100, nightPercent));

  switch (state) {
    case GREEN:
      return ScaleColor(GREEN_HUE, PercentToRaw(dayPercent));
    case AMBER:
      return ScaleColor(
        AMBER_HUE,
        PercentToRaw((dayPercent + nightPercent) / 2)
      );
    case RED:
      return ScaleColor(RED_HUE, PercentToRaw(nightPercent));
    case NAP:
      return ScaleColor(NAP_HUE, PercentToRaw(nightPercent));
    default:
      return RGBColor{0, 0, 0};
  }
}

// ---------------------------------------------------------------------------
// Animation, fading, pulsing
// ---------------------------------------------------------------------------
const uint32_t TRANSITION_ANIM_DURATION_MS = 20000;

inline float SmoothStep(float p) {
  p = std::max(0.0f, std::min(1.0f, p));
  return p * p * (3.0f - 2.0f * p);
}

// Time constant of the exponential color fade. Frame-rate independent.
const float FADE_TAU_MS = 10000.0f;

inline float FadeAlpha(uint32_t dt_ms) {
  if (dt_ms == 0) return 0.0f;
  if (dt_ms > 60000) return 1.0f;
  return 1.0f - std::exp(-(float) dt_ms / FADE_TAU_MS);
}

// The colon blinks on/off rather than dimming. At normal night levels amber
// is about (6, 1, 0) raw: green is already at its minimum, so any dimmer
// "amber" would really be red or yellow. Off has no hue, so nothing shifts.
const float PULSE_WARN_THRESHOLD_MIN = 10.0f;
const float PULSE_URGENT_THRESHOLD_MIN = 5.0f;
const uint32_t PULSE_PERIOD_SLOW_MS = 2000;
const uint32_t PULSE_PERIOD_FAST_MS = 900;

inline bool BlinkOn(uint32_t now_ms, uint32_t period_ms) {
  return (now_ms % period_ms) < period_ms / 2;
}

// ---------------------------------------------------------------------------
// Time helpers
// ---------------------------------------------------------------------------
inline int TimeAsMinutes(int hour, int minute) { return hour * 60 + minute; }

// Minutes going forward from `from` to `to`, wrapping at midnight (0..1439).
inline int MinutesUntil(int from, int to) {
  return ((to - from) % 1440 + 1440) % 1440;
}

struct WindowSpan { bool inWindow; int totalMinutes; int elapsedMinutes; };

// Time-of-day window [start, end), wrapping past midnight if end < start.
inline WindowSpan ResolveWindow(int currentMinutes, int startMinutes, int endMinutes) {
  if (startMinutes == endMinutes) return WindowSpan{false, 0, 0};
  int total = MinutesUntil(startMinutes, endMinutes);
  int elapsed = MinutesUntil(startMinutes, currentMinutes);
  if (elapsed >= total) return WindowSpan{false, 0, 0};
  return WindowSpan{true, total, elapsed};
}

inline float Percent(float elapsed, float total) {
  if (total <= 0.0f) return 0.0f;
  return std::max(0.0f, std::min(100.0f, 100.0f * elapsed / total));
}

template <typename SntpTime, typename HaTime>
inline ESPTime GetBestTime(SntpTime sntp, HaTime ha) {
  auto now = sntp->now();

  if (now.is_valid()) {
    return now;
  }

  return ha->now();
}
// ---------------------------------------------------------------------------
// Schedule
//
// Weekend *days* (Sat/Sun) use the weekend wake/go times.
// Weekend *nights* (Fri/Sat) use the weekend bedtime.
// Holiday mode treats every day and night as weekend.
// ---------------------------------------------------------------------------
const int WIND_DOWN_MINUTES = 30;

struct Schedule {
  int weekdayWake = 0;
  int weekdayGo = 0;
  int weekdayBedtime = 0;
  int weekendWake = 0;
  int weekendGo = 0;
  int weekendBedtime = 0;
  bool holiday = false;
  bool napEnabled = false;
  int napStart = 0;
  int napEnd = 0;
};

inline bool IsEffectiveWeekend(int dayOfWeek, bool holidayMode) {
  return holidayMode || dayOfWeek == SAT || dayOfWeek == SUN;
}

inline bool IsEffectiveWeekendNight(int dayOfWeek, bool holidayMode) {
  return holidayMode || dayOfWeek == FRI || dayOfWeek == SAT;
}

inline int WakeFor(const Schedule &s, int day) {
  return IsEffectiveWeekend(day, s.holiday) ? s.weekendWake : s.weekdayWake;
}
inline int GoFor(const Schedule &s, int day) {
  return IsEffectiveWeekend(day, s.holiday) ? s.weekendGo : s.weekdayGo;
}
inline int BedtimeFor(const Schedule &s, int day) {
  return IsEffectiveWeekendNight(day, s.holiday) ? s.weekendBedtime : s.weekdayBedtime;
}

// One "clock day" runs from that day's wake time to the next day's wake time.
// All values are minutes after midnight at the start of that day, so they
// can exceed 1440 (e.g. a bedtime of 00:30 is 1470, the next wake is 1440+).
struct DayTimeline { int wake; int go; int windDown; int bedtime; int nextWake; };

inline DayTimeline BuildTimeline(const Schedule &s, int day) {
  DayTimeline t;
  t.wake = WakeFor(s, day);
  t.go = t.wake + MinutesUntil(t.wake, GoFor(s, day));
  t.bedtime = t.wake + MinutesUntil(t.wake, BedtimeFor(s, day));
  t.windDown = t.bedtime - WIND_DOWN_MINUTES;
  t.nextWake = 1440 + WakeFor(s, NextDay(day));
  return t;
}

// Valid when, for every day of the week (with the current holiday setting):
//   wake < go < wind-down start < bedtime < next morning's wake
// and, if the nap is enabled, it lies inside that day's green period.
inline bool IsScheduleValid(const Schedule &s) {
  for (int day = SUN; day <= SAT; day++) {
    DayTimeline t = BuildTimeline(s, day);
    if (!(t.wake < t.go && t.go < t.windDown && t.bedtime < t.nextWake)) return false;
    if (s.napEnabled) {
      int goOfDay = t.go % 1440;
      int napStart = t.go + MinutesUntil(goOfDay, s.napStart);
      int napEnd = t.go + MinutesUntil(goOfDay, s.napEnd);
      if (!(napStart > t.go && napEnd > napStart && napEnd <= t.windDown)) return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Evaluation: one call gives everything the sensors and display need.
// ---------------------------------------------------------------------------
struct ClockStatus {
  int state = RED;
  float progress = 0.0f;           // % of the current period that has elapsed
  bool windDownActive = false;
  float windDownProgress = 0.0f;   // 0..1, reaches 1 when the urgent blink starts
  float remainingMinutes = 999.0f; // only meaningful for AMBER
  bool scheduleValid = true;
};

inline ClockStatus EvaluateClock(const Schedule &s, int dayOfWeek, int hour, int minute, int second) {
  ClockStatus st;
  if (dayOfWeek < SUN || dayOfWeek > SAT) return st;

  st.scheduleValid = IsScheduleValid(s);
  if (!st.scheduleValid) return st;

  const int nowMinutes = TimeAsMinutes(hour, minute);
  const float secondFrac = (float) second / 60.0f;

  // Before today's wake time we are still in yesterday's night.
  int day = dayOfWeek;
  int t = nowMinutes;
  if (nowMinutes < WakeFor(s, dayOfWeek)) {
    day = PrevDay(dayOfWeek);
    t += 1440;
  }
  const DayTimeline tl = BuildTimeline(s, day);
  const float tf = (float) t + secondFrac;

  if (t < tl.go) {
    st.state = AMBER;
    st.progress = Percent(tf - tl.wake, (float) (tl.go - tl.wake));
    st.remainingMinutes = (float) tl.go - tf;
  } else if (t < tl.windDown) {
    WindowSpan nap = s.napEnabled ? ResolveWindow(nowMinutes, s.napStart, s.napEnd) : WindowSpan{false, 0, 0};
    if (nap.inWindow) {
      st.state = NAP;
      st.progress = Percent(nap.elapsedMinutes + secondFrac, (float) nap.totalMinutes);
    } else {
      st.state = GREEN;
      st.progress = Percent(tf - tl.go, (float) (tl.bedtime - tl.go));
    }
  } else if (t < tl.bedtime) {
    st.state = AMBER;
    st.windDownActive = true;
    st.progress = Percent(tf - tl.go, (float) (tl.bedtime - tl.go));
    // Finish the green->amber blend when the urgent blink begins, so the
    // last minutes before bed are pure amber.
    const float ramp = std::max(1.0f, (float) WIND_DOWN_MINUTES - PULSE_URGENT_THRESHOLD_MIN);
    st.windDownProgress = std::max(0.0f, std::min(1.0f, (tf - tl.windDown) / ramp));
    st.remainingMinutes = (float) tl.bedtime - tf;
  } else {
    st.state = RED;
    st.progress = Percent(tf - tl.bedtime, (float) (tl.nextWake - tl.bedtime));
  }
  return st;
}

// ---------------------------------------------------------------------------
// Night / nap view (rows 1..7; row 0 is the progress bar)
// ---------------------------------------------------------------------------
struct Icon { int width; int height; const char *rows[7]; };

// Sleep view (night and nap): small time on the left (bottom-aligned),
// three Zs on the right going low, high, middle. Only the color differs.
const Icon SLEEP_ICON = {13, 7, {
  ".....###.....",
  ".......#.####",
  "####..#....##",
  "..##.###..##.",
  ".##......##..",
  "##.......####",
  "####.........",
}};
const int SLEEP_X = 19;
const int SLEEP_TIME_X = 0;
const int SLEEP_TIME_Y = 3;  // display row of the digits' top edge (rows 3..7)

// 3x5 pixel digits (same shapes as the Home Assistant card).
const char *const SMALL_DIGITS[10][5] = {
  {"###", "#.#", "#.#", "#.#", "###"},
  {".#.", "##.", ".#.", ".#.", "###"},
  {"###", "..#", "###", "#..", "###"},
  {"###", "..#", "###", "..#", "###"},
  {"#.#", "#.#", "###", "..#", "..#"},
  {"###", "#..", "###", "..#", "###"},
  {"###", "#..", "###", "#.#", "###"},
  {"###", "..#", ".#.", ".#.", ".#."},
  {"###", "#.#", "###", "#.#", "###"},
  {"###", "#.#", "###", "..#", "###"},
};

// Draws "HH:MM" in 3x5 digits: digit, 1px gap, digit, 1px gap, colon,
// 1px gap, digit, 1px gap, digit (17 pixels wide). `set(x, y)` lights one pixel.
template <typename SetPixel>
inline void DrawSmallTime(int hour, int minute, int x0, int y0, SetPixel set) {
  const int digits[4] = {(hour / 10) % 10, hour % 10, (minute / 10) % 10, minute % 10};
  int x = x0;
  for (int i = 0; i < 4; i++) {
    for (int row = 0; row < 5; row++)
      for (int col = 0; col < 3; col++)
        if (SMALL_DIGITS[digits[i]][row][col] == '#') set(x + col, y0 + row);
    x += 4;
    if (i == 1) {
      set(x, y0 + 1);
      set(x, y0 + 3);
      x += 2;
    }
  }
}

// The progress bar is drawn at this fraction of the state color at night.
const float NIGHT_BAR_DIM = 0.5f;

// ---------------------------------------------------------------------------
// Home Assistant state label
// ---------------------------------------------------------------------------
inline const char *StateLabel(const ClockStatus &st) {
  if (!st.scheduleValid) return "Invalid schedule";
  switch (st.state) {
    case GREEN: return "Awake";
    case AMBER: return st.windDownActive ? "Wind down" : "Waking up";
    case RED: return "Sleep";
    case NAP: return "Nap";
    default: return "Unknown";
  }
}
