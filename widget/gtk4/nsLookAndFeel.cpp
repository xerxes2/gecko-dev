/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// for strtod()
#include <stdlib.h>
#include <dlfcn.h>

#include "nsLookAndFeel.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <pango/pango.h>
#include <pango/pango-fontmap.h>
#include <fontconfig/fontconfig.h>

#include "GRefPtr.h"
#include "GUniquePtr.h"
#include "nsGtkUtils.h"
#include "gfxPlatformGtk.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/WidgetUtilsGtk.h"
#include "ScreenHelperGTK.h"
#include "ScrollbarDrawing.h"

//#include "gtkdrawing.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "gfxFontConstants.h"
#include "WidgetUtils.h"
#include "nsWindow.h"

#include "mozilla/gfx/2D.h"

#include <cairo-gobject.h>
#include <dlfcn.h>
//#include "WidgetStyleCache.h"
#include "prenv.h"
#include "nsCSSColorUtils.h"
#include "mozilla/Preferences.h"

#ifdef MOZ_X11
#  include <X11/XKBlib.h>
#endif

#ifdef MOZ_WAYLAND
#  include <xkbcommon/xkbcommon.h>
#endif

using namespace mozilla;
using namespace mozilla::widget;

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
static LazyLogModule gLnfLog("LookAndFeel");
#  define LOGLNF(...) MOZ_LOG(gLnfLog, LogLevel::Debug, (__VA_ARGS__))
#  define LOGLNF_ENABLED() MOZ_LOG_TEST(gLnfLog, LogLevel::Debug)
#else
#  define LOGLNF(args)
#  define LOGLNF_ENABLED() false
#endif /* MOZ_LOGGING */

#define GDK_COLOR_TO_NS_RGB(c) \
  ((nscolor)NS_RGB(c.red >> 8, c.green >> 8, c.blue >> 8))
#define GDK_RGBA_TO_NS_RGBA(c)                                    \
  ((nscolor)NS_RGBA((int)((c).red * 255), (int)((c).green * 255), \
                    (int)((c).blue * 255), (int)((c).alpha * 255)))

static bool sIgnoreChangedSettings = false;

static void OnSettingsChange() {
  if (sIgnoreChangedSettings) {
    return;
  }
  // TODO: We could be more granular here, but for now assume everything
  // changed.
  LookAndFeel::NotifyChangedAllWindows(widget::ThemeChangeKind::StyleAndLayout);
  widget::IMContextWrapper::OnThemeChanged();
}

static void settings_changed_cb(GtkSettings*, GParamSpec*, void*) {
  OnSettingsChange();
}

static bool sCSDAvailable;

static nsCString GVariantToString(GVariant* aVariant) {
  nsCString ret;
  gchar* s = g_variant_print(aVariant, TRUE);
  if (s) {
    ret.Assign(s);
    g_free(s);
  }
  return ret;
}

static nsDependentCString GVariantGetString(GVariant* aVariant) {
  gsize len = 0;
  const gchar* v = g_variant_get_string(aVariant, &len);
  return nsDependentCString(v, len);
}

static void UnboxVariant(RefPtr<GVariant>& aVariant) {
  while (aVariant && g_variant_is_of_type(aVariant, G_VARIANT_TYPE_VARIANT)) {
    // Unbox the return value.
    aVariant = dont_AddRef(g_variant_get_variant(aVariant));
  }
}

static void settings_changed_signal_cb(GDBusProxy* proxy, gchar* sender_name,
                                       gchar* signal_name, GVariant* parameters,
                                       gpointer user_data) {
  LOGLNF("Settings Change sender=%s signal=%s params=%s\n", sender_name,
         signal_name, GVariantToString(parameters).get());
  if (strcmp(signal_name, "SettingChanged")) {
    NS_WARNING(
        nsPrintfCString("Unknown change signal for settings: %s", signal_name)
            .get());
    return;
  }
  RefPtr<GVariant> ns = dont_AddRef(g_variant_get_child_value(parameters, 0));
  RefPtr<GVariant> key = dont_AddRef(g_variant_get_child_value(parameters, 1));
  RefPtr<GVariant> value =
      dont_AddRef(g_variant_get_child_value(parameters, 2));
  // Third parameter is the value, but we don't care about it.
  if (!ns || !key || !value ||
      !g_variant_is_of_type(ns, G_VARIANT_TYPE_STRING) ||
      !g_variant_is_of_type(key, G_VARIANT_TYPE_STRING)) {
    MOZ_ASSERT(false, "Unexpected setting change signal parameters");
    return;
  }

  auto* lnf = static_cast<nsLookAndFeel*>(user_data);
  auto nsStr = GVariantGetString(ns);
  if (!nsStr.Equals("org.freedesktop.appearance"_ns)) {
    return;
  }

  UnboxVariant(value);

  auto keyStr = GVariantGetString(key);
  if (lnf->RecomputeDBusAppearanceSetting(keyStr, value)) {
    OnSettingsChange();
  }
}

bool nsLookAndFeel::RecomputeDBusAppearanceSetting(const nsACString& aKey,
                                                   GVariant* aValue) {
  LOGLNF("RecomputeDBusAppearanceSetting(%s, %s)",
         PromiseFlatCString(aKey).get(), GVariantToString(aValue).get());
  if (aKey.EqualsLiteral("contrast")) {
    const bool old = mDBusSettings.mPrefersContrast;
    mDBusSettings.mPrefersContrast = g_variant_get_uint32(aValue) == 1;
    return mDBusSettings.mPrefersContrast != old;
  }
  if (aKey.EqualsLiteral("color-scheme")) {
    const auto old = mDBusSettings.mColorScheme;
    mDBusSettings.mColorScheme = [&] {
      switch (g_variant_get_uint32(aValue)) {
        default:
          MOZ_FALLTHROUGH_ASSERT("Unexpected color-scheme query return value");
        case 0:
          break;
        case 1:
          return Some(ColorScheme::Dark);
        case 2:
          return Some(ColorScheme::Light);
      }
      return Maybe<ColorScheme>{};
    }();
    return mDBusSettings.mColorScheme != old;
  }
  if (aKey.EqualsLiteral("accent-color")) {
    auto old = mDBusSettings.mAccentColor;
    mDBusSettings.mAccentColor.mBg = mDBusSettings.mAccentColor.mFg =
        NS_TRANSPARENT;
    gdouble r = -1.0, g = -1.0, b = -1.0;
    g_variant_get(aValue, "(ddd)", &r, &g, &b);
    if (r >= 0.0f && g >= 0.0f && b >= 0.0f) {
      mDBusSettings.mAccentColor.mBg = gfx::sRGBColor(r, g, b, 1.0).ToABGR();
      mDBusSettings.mAccentColor.mFg =
          ThemeColors::ComputeCustomAccentForeground(
              mDBusSettings.mAccentColor.mBg);
    }
    return mDBusSettings.mAccentColor != old;
  }
  return false;
}

bool nsLookAndFeel::RecomputeDBusSettings() {
  if (!mDBusSettingsProxy) {
    return false;
  }

  GVariantBuilder namespacesBuilder;
  g_variant_builder_init(&namespacesBuilder, G_VARIANT_TYPE("as"));
  g_variant_builder_add(&namespacesBuilder, "s", "org.freedesktop.appearance");

  GUniquePtr<GError> error;
  RefPtr<GVariant> variant = dont_AddRef(g_dbus_proxy_call_sync(
      mDBusSettingsProxy, "ReadAll", g_variant_new("(as)", &namespacesBuilder),
      G_DBUS_CALL_FLAGS_NONE,
      StaticPrefs::widget_gtk_settings_portal_timeout_ms(), nullptr,
      getter_Transfers(error)));
  if (!variant) {
    LOGLNF("dbus settings query error: %s\n", error->message);
    return false;
  }

  LOGLNF("dbus settings query result: %s\n", GVariantToString(variant).get());
  variant = dont_AddRef(g_variant_get_child_value(variant, 0));
  UnboxVariant(variant);
  LOGLNF("dbus settings query result after unbox: %s\n",
         GVariantToString(variant).get());
  if (!variant || !g_variant_is_of_type(variant, G_VARIANT_TYPE_DICTIONARY)) {
    MOZ_ASSERT(false, "Unexpected dbus settings query return value");
    return false;
  }

  bool changed = false;
  // We expect one dictionary with (right now) one namespace for appearance,
  // with another dictionary inside for the actual values.
  {
    gchar* ns;
    GVariantIter outerIter;
    GVariantIter* innerIter;
    g_variant_iter_init(&outerIter, variant);
    while (g_variant_iter_loop(&outerIter, "{sa{sv}}", &ns, &innerIter)) {
      LOGLNF("Got namespace %s", ns);
      if (!strcmp(ns, "org.freedesktop.appearance")) {
        gchar* appearanceKey;
        GVariant* innerValue;
        while (g_variant_iter_loop(innerIter, "{sv}", &appearanceKey,
                                   &innerValue)) {
          LOGLNF(" > %s: %s", appearanceKey,
                 GVariantToString(innerValue).get());
          changed |= RecomputeDBusAppearanceSetting(
              nsDependentCString(appearanceKey), innerValue);
        }
      }
    }
  }
  return changed;
}

void nsLookAndFeel::WatchDBus() {
  LOGLNF("nsLookAndFeel::WatchDBus");
  GUniquePtr<GError> error;
  mDBusSettingsProxy = dont_AddRef(g_dbus_proxy_new_for_bus_sync(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr,
      "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.Settings", nullptr, getter_Transfers(error)));
  if (!mDBusSettingsProxy) {
    LOGLNF("Can't create DBus proxy for settings: %s\n", error->message);
    return;
  }

  g_signal_connect(mDBusSettingsProxy, "g-signal",
                   G_CALLBACK(settings_changed_signal_cb), this);

  // DBus interface was started after L&F init so we need to load our settings
  // from DBus explicitly.
  if (RecomputeDBusSettings()) {
    OnSettingsChange();
  }
}

void nsLookAndFeel::UnwatchDBus() {
  if (!mDBusSettingsProxy) {
    return;
  }
  LOGLNF("nsLookAndFeel::UnwatchDBus");
  g_signal_handlers_disconnect_by_func(
      mDBusSettingsProxy, FuncToGpointer(settings_changed_signal_cb), this);
  mDBusSettingsProxy = nullptr;
}

nsLookAndFeel::nsLookAndFeel() {
  static constexpr nsLiteralCString kObservedSettings[] = {
      // Affects system font sizes.
      "notify::gtk-xft-dpi"_ns,
      // Affects mSystemTheme and mAltTheme as expected.
      "notify::gtk-theme-name"_ns,
      "notify::gtk-application-prefer-dark-theme"_ns,
      // System fonts?
      "notify::gtk-font-name"_ns,
      // prefers-reduced-motion
      "notify::gtk-enable-animations"_ns,
      // CSD media queries, etc.
      "notify::gtk-decoration-layout"_ns,
      // Text resolution affects system font and widget sizes.
      "notify::resolution"_ns,
      // These three Affect mCaretBlinkTime
      "notify::gtk-cursor-blink"_ns,
      "notify::gtk-cursor-blink-time"_ns,
      "notify::gtk-cursor-blink-timeout"_ns,
      // Affects SelectTextfieldsOnKeyFocus
      "notify::gtk-entry-select-on-focus"_ns,
      // Affects ScrollToClick
      "notify::gtk-primary-button-warps-slider"_ns,
      // Affects SubmenuDelay
      //"notify::gtk-menu-popup-delay"_ns,
      // Affects DragThresholdX/Y
      "notify::gtk-dnd-drag-threshold"_ns,
      // Affects titlebar actions loaded at moz_gtk_refresh().
      "notify::gtk-titlebar-double-click"_ns,
      "notify::gtk-titlebar-middle-click"_ns,
  };

  GtkSettings* settings = gtk_settings_get_default();
  for (const auto& setting : kObservedSettings) {
    g_signal_connect_after(settings, setting.get(),
                           G_CALLBACK(settings_changed_cb), nullptr);
  }

  sCSDAvailable =
      nsWindow::GetSystemGtkWindowDecoration() != nsWindow::GTK_DECORATION_NONE;

  if (ShouldUsePortal(PortalKind::Settings)) {
    mDBusID = g_bus_watch_name(
        G_BUS_TYPE_SESSION, "org.freedesktop.portal.Desktop",
        G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
        [](GDBusConnection*, const gchar*, const gchar*,
           gpointer data) -> void {
          auto* lnf = static_cast<nsLookAndFeel*>(data);
          lnf->WatchDBus();
        },
        [](GDBusConnection*, const gchar*, gpointer data) -> void {
          auto* lnf = static_cast<nsLookAndFeel*>(data);
          lnf->UnwatchDBus();
        },
        this, nullptr);
  }
  if (IsKdeDesktopEnvironment()) {
    GUniquePtr<gchar> path(
        g_strconcat(g_get_user_config_dir(), "/gtk-3.0/colors.css", NULL));
    mKdeColors = dont_AddRef(g_file_new_for_path(path.get()));
    mKdeColorsMonitor = dont_AddRef(
        g_file_monitor_file(mKdeColors.get(), G_FILE_MONITOR_NONE, NULL, NULL));
    if (mKdeColorsMonitor) {
      g_signal_connect(mKdeColorsMonitor.get(), "changed",
                       G_CALLBACK(settings_changed_cb), NULL);
    }
  }
}

nsLookAndFeel::~nsLookAndFeel() {
  ClearRoundedCornerProvider();
  if (mDBusID) {
    g_bus_unwatch_name(mDBusID);
    mDBusID = 0;
  }
  UnwatchDBus();
  g_signal_handlers_disconnect_by_func(
      gtk_settings_get_default(), FuncToGpointer(settings_changed_cb), nullptr);
}

// Modifies color |*aDest| as if a pattern of color |aSource| was painted with
// CAIRO_OPERATOR_OVER to a surface with color |*aDest|.
static void ApplyColorOver(const GdkRGBA& aSource, GdkRGBA* aDest) {
  gdouble sourceCoef = aSource.alpha;
  gdouble destCoef = aDest->alpha * (1.0 - sourceCoef);
  gdouble resultAlpha = sourceCoef + destCoef;
  if (resultAlpha != 0.0) {  // don't divide by zero
    destCoef /= resultAlpha;
    sourceCoef /= resultAlpha;
    aDest->red = sourceCoef * aSource.red + destCoef * aDest->red;
    aDest->green = sourceCoef * aSource.green + destCoef * aDest->green;
    aDest->blue = sourceCoef * aSource.blue + destCoef * aDest->blue;
    aDest->alpha = resultAlpha;
  }
}

static void GetLightAndDarkness(const GdkRGBA& aColor, double* aLightness,
                                double* aDarkness) {
  double sum = aColor.red + aColor.green + aColor.blue;
  *aLightness = sum * aColor.alpha;
  *aDarkness = (3.0 - sum) * aColor.alpha;
}

static bool GetGradientColors(const GValue* aValue, GdkRGBA* aLightColor,
                              GdkRGBA* aDarkColor) {
  if (!G_TYPE_CHECK_VALUE_TYPE(aValue, CAIRO_GOBJECT_TYPE_PATTERN)) {
    return false;
  }

  auto pattern = static_cast<cairo_pattern_t*>(g_value_get_boxed(aValue));
  if (!pattern) {
    return false;
  }

  // Just picking the lightest and darkest colors as simple samples rather
  // than trying to blend, which could get messy if there are many stops.
  if (CAIRO_STATUS_SUCCESS !=
      cairo_pattern_get_color_stop_rgba(pattern, 0, nullptr, reinterpret_cast<double*>(&aDarkColor->red),
                                        reinterpret_cast<double*>(&aDarkColor->green),
                                        reinterpret_cast<double*>(&aDarkColor->blue),
                                        reinterpret_cast<double*>(&aDarkColor->alpha))) {
    return false;
  }

  double maxLightness, maxDarkness;
  GetLightAndDarkness(*aDarkColor, &maxLightness, &maxDarkness);
  *aLightColor = *aDarkColor;

  GdkRGBA stop;
  for (int index = 1;
       CAIRO_STATUS_SUCCESS ==
       cairo_pattern_get_color_stop_rgba(pattern, index, nullptr, reinterpret_cast<double*>(&aDarkColor->red),
                                         reinterpret_cast<double*>(&aDarkColor->green),
                                         reinterpret_cast<double*>(&aDarkColor->blue),
                                         reinterpret_cast<double*>(&aDarkColor->alpha));
       ++index) {
    double lightness, darkness;
    GetLightAndDarkness(stop, &lightness, &darkness);
    if (lightness > maxLightness) {
      maxLightness = lightness;
      *aLightColor = stop;
    }
    if (darkness > maxDarkness) {
      maxDarkness = darkness;
      *aDarkColor = stop;
    }
  }

  return true;
}

static bool GetColorFromImagePattern(const GValue* aValue, nscolor* aColor) {
  if (!G_TYPE_CHECK_VALUE_TYPE(aValue, CAIRO_GOBJECT_TYPE_PATTERN)) {
    return false;
  }

  auto* pattern = static_cast<cairo_pattern_t*>(g_value_get_boxed(aValue));
  if (!pattern) {
    return false;
  }

  cairo_surface_t* surface;
  if (cairo_pattern_get_surface(pattern, &surface) != CAIRO_STATUS_SUCCESS) {
    return false;
  }

  cairo_format_t format = cairo_image_surface_get_format(surface);
  if (format == CAIRO_FORMAT_INVALID) {
    return false;
  }
  int width = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);
  int stride = cairo_image_surface_get_stride(surface);
  if (!width || !height) {
    return false;
  }

  // Guesstimate the central pixel would have a sensible color.
  int x = width / 2;
  int y = height / 2;

  unsigned char* data = cairo_image_surface_get_data(surface);
  switch (format) {
    // Most (all?) GTK images / patterns / etc use ARGB32.
    case CAIRO_FORMAT_ARGB32: {
      size_t offset = x * 4 + y * stride;
      uint32_t* pixel = reinterpret_cast<uint32_t*>(data + offset);
      *aColor = gfx::sRGBColor::UnusualFromARGB(*pixel).ToABGR();
      return true;
    }
    default:
      break;
  }

  return false;
}

// Finds ideal cell highlight colors used for unfocused+selected cells distinct
// from both Highlight, used as focused+selected background, and the listbox
// background which is assumed to be similar to -moz-field
void nsLookAndFeel::PerThemeData::InitCellHighlightColors() {
  int32_t minLuminosityDifference = NS_SUFFICIENT_LUMINOSITY_DIFFERENCE_BG;
  int32_t backLuminosityDifference =
      NS_LUMINOSITY_DIFFERENCE(mWindow.mBg, mField.mBg);
  if (backLuminosityDifference >= minLuminosityDifference) {
    mCellHighlight = mWindow;
    return;
  }

  uint16_t hue, sat, luminance;
  uint8_t alpha;
  mCellHighlight = mField;

  NS_RGB2HSV(mCellHighlight.mBg, hue, sat, luminance, alpha);

  uint16_t step = 30;
  // Lighten the color if the color is very dark
  if (luminance <= step) {
    luminance += step;
  }
  // Darken it if it is very light
  else if (luminance >= 255 - step) {
    luminance -= step;
  }
  // Otherwise, compute what works best depending on the text luminance.
  else {
    uint16_t textHue, textSat, textLuminance;
    uint8_t textAlpha;
    NS_RGB2HSV(mCellHighlight.mFg, textHue, textSat, textLuminance, textAlpha);
    // Text is darker than background, use a lighter shade
    if (textLuminance < luminance) {
      luminance += step;
    }
    // Otherwise, use a darker shade
    else {
      luminance -= step;
    }
  }
  NS_HSV2RGB(mCellHighlight.mBg, hue, sat, luminance, alpha);
}

void nsLookAndFeel::NativeInit() { EnsureInit(); }

void nsLookAndFeel::RefreshImpl() {
  mInitialized = false;
  
  nsXPLookAndFeel::RefreshImpl();
}

nsresult nsLookAndFeel::NativeGetColor(ColorID aID, ColorScheme aScheme,
                                       nscolor& aColor) {
  EnsureInit();

  const auto& theme =
      aScheme == ColorScheme::Light ? LightTheme() : DarkTheme();
  return theme.GetColor(aID, aColor);
}

static bool ShouldUseColorForActiveDarkScrollbarThumb(nscolor aColor) {
  auto IsDifferentEnough = [](int32_t aChannel, int32_t aOtherChannel) {
    return std::abs(aChannel - aOtherChannel) > 10;
  };
  return IsDifferentEnough(NS_GET_R(aColor), NS_GET_G(aColor)) ||
         IsDifferentEnough(NS_GET_R(aColor), NS_GET_B(aColor));
}

static bool ShouldUseThemedScrollbarColor(StyleSystemColor aID, nscolor aColor,
                                          bool aIsDark) {
  if (!aIsDark) {
    return true;
  }
  if (StaticPrefs::widget_non_native_theme_scrollbar_dark_themed()) {
    return true;
  }
  return aID == StyleSystemColor::ThemedScrollbarThumbActive &&
         StaticPrefs::widget_non_native_theme_scrollbar_active_always_themed();
}

nsresult nsLookAndFeel::PerThemeData::GetColor(ColorID aID,
                                               nscolor& aColor) const {
  nsresult res = NS_OK;

  switch (aID) {
      // These colors don't seem to be used for anything anymore in Mozilla
      // The CSS2 colors below are used.
    case ColorID::Appworkspace:  // MDI background color
    case ColorID::Background:    // desktop background
    case ColorID::Window:
    case ColorID::Windowframe:
    case ColorID::MozCombobox:
      aColor = mWindow.mBg;
      break;
    case ColorID::Windowtext:
      aColor = mWindow.mFg;
      break;
    case ColorID::MozDialog:
      aColor = mDialog.mBg;
      break;
    case ColorID::MozDialogtext:
      aColor = mDialog.mFg;
      break;
    case ColorID::IMESelectedRawTextBackground:
    case ColorID::IMESelectedConvertedTextBackground:
    case ColorID::Highlight:  // preference selected item,
      aColor = mSelectedText.mBg;
      break;
    case ColorID::Highlighttext:
      if (NS_GET_A(mSelectedText.mBg) < 155) {
        aColor = NS_SAME_AS_FOREGROUND_COLOR;
        break;
      }
      [[fallthrough]];
    case ColorID::IMESelectedRawTextForeground:
    case ColorID::IMESelectedConvertedTextForeground:
      aColor = mSelectedText.mFg;
      break;
    case ColorID::Selecteditem:
      aColor = mSelectedItem.mBg;
      break;
    case ColorID::Selecteditemtext:
      aColor = mSelectedItem.mFg;
      break;
    case ColorID::Accentcolor:
      aColor = mAccent.mBg;
      break;
    case ColorID::Accentcolortext:
      aColor = mAccent.mFg;
      break;
    case ColorID::MozCellhighlight:
      aColor = mCellHighlight.mBg;
      break;
    case ColorID::MozCellhighlighttext:
      aColor = mCellHighlight.mFg;
      break;
    case ColorID::IMERawInputBackground:
    case ColorID::IMEConvertedTextBackground:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::IMERawInputForeground:
    case ColorID::IMEConvertedTextForeground:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMERawInputUnderline:
    case ColorID::IMEConvertedTextUnderline:
      aColor = NS_SAME_AS_FOREGROUND_COLOR;
      break;
    case ColorID::IMESelectedRawTextUnderline:
    case ColorID::IMESelectedConvertedTextUnderline:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::Scrollbar:
      aColor = mThemedScrollbar;
      break;
    case ColorID::ThemedScrollbar:
      aColor = mThemedScrollbar;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarInactive:
      aColor = mThemedScrollbarInactive;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumb:
      aColor = mThemedScrollbarThumb;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumbHover:
      aColor = mThemedScrollbarThumbHover;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumbActive:
      aColor = mThemedScrollbarThumbActive;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;
    case ColorID::ThemedScrollbarThumbInactive:
      aColor = mThemedScrollbarThumbInactive;
      if (!ShouldUseThemedScrollbarColor(aID, aColor, mIsDark)) {
        return NS_ERROR_FAILURE;
      }
      break;

      // css2  http://www.w3.org/TR/REC-CSS2/ui.html#system-colors
    case ColorID::Activeborder:
      // active window border
      aColor = mMozWindowActiveBorder;
      break;
    case ColorID::Inactiveborder:
      // inactive window border
      aColor = mMozWindowInactiveBorder;
      break;
    case ColorID::Graytext:  // disabled text in windows, menus, etc.
      aColor = mGrayText;
      break;
    case ColorID::Activecaption:
      aColor = mTitlebar.mBg;
      break;
    case ColorID::Captiontext:  // text in active window caption (titlebar)
      aColor = mTitlebar.mFg;
      break;
    case ColorID::Inactivecaption:
      // inactive window caption
      aColor = mTitlebarInactive.mBg;
      break;
    case ColorID::Inactivecaptiontext:
      aColor = mTitlebarInactive.mFg;
      break;
    case ColorID::Infobackground:
      aColor = mInfo.mBg;
      break;
    case ColorID::Infotext:
      aColor = mInfo.mFg;
      break;
    case ColorID::Menu:
      aColor = mMenu.mBg;
      break;
    case ColorID::Menutext:
      aColor = mMenu.mFg;
      break;
    case ColorID::MozHeaderbar:
      aColor = mHeaderBar.mBg;
      break;
    case ColorID::MozHeaderbartext:
      aColor = mHeaderBar.mFg;
      break;
    case ColorID::MozHeaderbarinactive:
      aColor = mHeaderBarInactive.mBg;
      break;
    case ColorID::MozHeaderbarinactivetext:
      aColor = mHeaderBarInactive.mFg;
      break;
    case ColorID::Threedface:
    case ColorID::Buttonface:
    case ColorID::MozButtondisabledface:
      // 3-D face color
      aColor = mWindow.mBg;
      break;

    case ColorID::Buttontext:
      // text on push buttons
      aColor = mButton.mFg;
      break;

    case ColorID::Buttonhighlight:
      // 3-D highlighted edge color
    case ColorID::Threedhighlight:
      // 3-D highlighted outer edge color
      aColor = mThreeDHighlight;
      break;

    case ColorID::Buttonshadow:
      // 3-D shadow edge color
    case ColorID::Threedshadow:
      // 3-D shadow inner edge color
      aColor = mThreeDShadow;
      break;
    case ColorID::Buttonborder:
      aColor = mButtonBorder;
      break;
    case ColorID::Threedlightshadow:
    case ColorID::MozDisabledfield:
      aColor = mIsDark ? *GenericDarkColor(aID) : NS_RGB(0xE0, 0xE0, 0xE0);
      break;
    case ColorID::Threeddarkshadow:
      aColor = mIsDark ? *GenericDarkColor(aID) : NS_RGB(0xDC, 0xDC, 0xDC);
      break;

    case ColorID::MozEventreerow:
    case ColorID::Field:
      aColor = mField.mBg;
      break;
    case ColorID::Fieldtext:
      aColor = mField.mFg;
      break;
    case ColorID::MozSidebar:
      aColor = mSidebar.mBg;
      break;
    case ColorID::MozSidebartext:
      aColor = mSidebar.mFg;
      break;
    case ColorID::MozSidebarborder:
      aColor = mSidebarBorder;
      break;
    case ColorID::MozButtonhoverface:
      aColor = mButtonHover.mBg;
      break;
    case ColorID::MozButtonhovertext:
      aColor = mButtonHover.mFg;
      break;
    case ColorID::MozButtonactiveface:
      aColor = mButtonActive.mBg;
      break;
    case ColorID::MozButtonactivetext:
      aColor = mButtonActive.mFg;
      break;
    case ColorID::MozMenuhover:
      aColor = mMenuHover.mBg;
      break;
    case ColorID::MozMenuhovertext:
      aColor = mMenuHover.mFg;
      break;
    case ColorID::MozMenuhoverdisabled:
      aColor = NS_TRANSPARENT;
      break;
    case ColorID::MozOddtreerow:
      aColor = mOddCellBackground;
      break;
    case ColorID::MozNativehyperlinktext:
      aColor = mNativeHyperLinkText;
      break;
    case ColorID::MozNativevisitedhyperlinktext:
      aColor = mNativeVisitedHyperLinkText;
      break;
    case ColorID::MozComboboxtext:
      aColor = mComboBoxText;
      break;
    case ColorID::MozColheader:
      aColor = mMozColHeader.mBg;
      break;
    case ColorID::MozColheadertext:
      aColor = mMozColHeader.mFg;
      break;
    case ColorID::MozColheaderhover:
      aColor = mMozColHeaderHover.mBg;
      break;
    case ColorID::MozColheaderhovertext:
      aColor = mMozColHeaderHover.mFg;
      break;
    case ColorID::MozColheaderactive:
      aColor = mMozColHeaderActive.mBg;
      break;
    case ColorID::MozColheaderactivetext:
      aColor = mMozColHeaderActive.mFg;
      break;
    case ColorID::SpellCheckerUnderline:
    case ColorID::Mark:
    case ColorID::Marktext:
    case ColorID::MozAutofillBackground:
    case ColorID::TargetTextBackground:
    case ColorID::TargetTextForeground:
      aColor = GetStandinForNativeColor(
          aID, mIsDark ? ColorScheme::Dark : ColorScheme::Light);
      break;
    case ColorID::Canvastext:
      aColor = mWindow.mFg;
      break;
    case ColorID::Canvas:
      aColor = mWindow.mBg;
      break;
    case ColorID::MozMenubarhovertext:
      aColor = mMenuHover.mBg;
      break;
    case ColorID::MozMacDefaultbuttontext: 
      aColor = mWindow.mFg;
      break;
     case ColorID::TextSelectAttentionBackground:
     case ColorID::TextSelectDisabledBackground:
     case ColorID::TextHighlightBackground:
      aColor = mInfo.mBg;
      break;
    default:
      /* default color is BLACK */
      aColor = 0;
      res = NS_ERROR_FAILURE;
      break;
  }

  return res;
}

nsresult nsLookAndFeel::NativeGetInt(IntID aID, int32_t& aResult) {
  nsresult res = NS_OK;

  // We use delayed initialization by EnsureInit() here
  // to make sure mozilla::Preferences is available (Bug 115807).
  // IntID::UseAccessibilityTheme is requested before user preferences
  // are read, and so EnsureInit(), which depends on preference values,
  // is deliberately delayed until required.
  switch (aID) {
    case IntID::ScrollButtonLeftMouseButtonAction:
      aResult = 0;
      break;
    case IntID::ScrollButtonMiddleMouseButtonAction:
      aResult = 1;
      break;
    case IntID::ScrollButtonRightMouseButtonAction:
      aResult = 2;
      break;
    case IntID::CaretBlinkTime:
      EnsureInit();
      aResult = mCaretBlinkTime;
      break;
    case IntID::CaretBlinkCount:
      EnsureInit();
      aResult = mCaretBlinkCount;
      break;
    case IntID::CaretWidth:
      aResult = 1;
      break;
    case IntID::SelectTextfieldsOnKeyFocus: {
      GtkSettings* settings;
      gboolean select_on_focus;

      settings = gtk_settings_get_default();
      g_object_get(settings, "gtk-entry-select-on-focus", &select_on_focus,
                   nullptr);

      if (select_on_focus)
        aResult = 1;
      else
        aResult = 0;

    } break;
    case IntID::ScrollToClick: {
      GtkSettings* settings;
      gboolean warps_slider = FALSE;

      settings = gtk_settings_get_default();
      if (g_object_class_find_property(G_OBJECT_GET_CLASS(settings),
                                       "gtk-primary-button-warps-slider")) {
        g_object_get(settings, "gtk-primary-button-warps-slider", &warps_slider,
                     nullptr);
      }

      if (warps_slider)
        aResult = 1;
      else
        aResult = 0;
    } break;
    //case IntID::SubmenuDelay: {
    //  GtkSettings* settings;
    //  gint delay;
    //  settings = gtk_settings_get_default();
    //  g_object_get(settings, "gtk-menu-popup-delay", &delay, nullptr);
    //  aResult = (int32_t)delay;
    //  break;
    //}
    case IntID::MenusCanOverlapOSBar:
      aResult = 0;
      break;
    case IntID::SkipNavigatingDisabledMenuItem:
      aResult = 1;
      break;
    case IntID::DragThresholdX:
    case IntID::DragThresholdY: {
      gint threshold = 0;
      g_object_get(gtk_settings_get_default(), "gtk-dnd-drag-threshold",
                   &threshold, nullptr);

      aResult = threshold;
    } break;
    case IntID::TreeOpenDelay:
      aResult = 1000;
      break;
    case IntID::TreeCloseDelay:
      aResult = 1000;
      break;
    case IntID::TreeLazyScrollDelay:
      aResult = 150;
      break;
    case IntID::TreeScrollDelay:
      aResult = 100;
      break;
    case IntID::TreeScrollLinesMax:
      aResult = 3;
      break;
    case IntID::AlertNotificationOrigin:
      aResult = NS_ALERT_TOP;
      break;
    case IntID::IMERawInputUnderlineStyle:
    case IntID::IMEConvertedTextUnderlineStyle:
      aResult = static_cast<int32_t>(StyleTextDecorationStyle::Solid);
      break;
    case IntID::IMESelectedRawTextUnderlineStyle:
    case IntID::IMESelectedConvertedTextUnderline:
      aResult = static_cast<int32_t>(StyleTextDecorationStyle::None);
      break;
    case IntID::SpellCheckerUnderlineStyle:
      aResult = static_cast<int32_t>(StyleTextDecorationStyle::Wavy);
      break;
    case IntID::MenuBarDrag:
      EnsureInit();
      aResult = mThemeDefault.mMenuSupportsDrag;
      break;
    case IntID::ScrollbarButtonAutoRepeatBehavior:
      aResult = 1;
      break;
    case IntID::SwipeAnimationEnabled:
      aResult = 1;
      break;
    case IntID::ContextMenuOffsetVertical:
    case IntID::ContextMenuOffsetHorizontal:
      aResult = 2;
      break;
    case IntID::GTKCSDAvailable:
      aResult = sCSDAvailable;
      break;
    case IntID::GTKCSDTransparencyAvailable: {
      aResult = ScreenHelperGTK::IsComposited();
      break;
    }
    case IntID::GTKCSDMaximizeButton:
      EnsureInit();
      aResult = mCSDMaximizeButton;
      break;
    case IntID::GTKCSDMinimizeButton:
      EnsureInit();
      aResult = mCSDMinimizeButton;
      break;
    case IntID::GTKCSDCloseButton:
      EnsureInit();
      aResult = mCSDCloseButton;
      break;
    case IntID::GTKCSDReversedPlacement:
      EnsureInit();
      aResult = mCSDReversedPlacement;
      break;
    case IntID::PrefersReducedMotion: {
      EnsureInit();
      aResult = mPrefersReducedMotion;
      break;
    }
    case IntID::SystemUsesDarkTheme: {
      EnsureInit();
      if (mColorSchemePreference) {
        aResult = *mColorSchemePreference == ColorScheme::Dark;
      } else {
        aResult = mThemeDefault.mIsDark;
      }
      break;
    }
    case IntID::GTKCSDMaximizeButtonPosition:
      aResult = mCSDMaximizeButtonPosition;
      break;
    case IntID::GTKCSDMinimizeButtonPosition:
      aResult = mCSDMinimizeButtonPosition;
      break;
    case IntID::GTKCSDCloseButtonPosition:
      aResult = mCSDCloseButtonPosition;
      break;
    case IntID::GTKThemeFamily: {
      EnsureInit();
      aResult = int32_t(EffectiveTheme().mFamily);
      break;
    }
    case IntID::UseAccessibilityTheme:
    // If high contrast is enabled, enable prefers-reduced-transparency media
    // query as well as there is no dedicated option.
    case IntID::PrefersReducedTransparency:
      EnsureInit();
      aResult = mDBusSettings.mPrefersContrast || mThemeDefault.mHighContrast;
      break;
    case IntID::InvertedColors:
      // No GTK API for checking if inverted colors is enabled
      aResult = 0;
      break;
    case IntID::TitlebarRadius: {
      EnsureInit();
      aResult = EffectiveTheme().mTitlebarRadius;
      break;
    }
    case IntID::TitlebarButtonSpacing: {
      EnsureInit();
      aResult = EffectiveTheme().mTitlebarButtonSpacing;
      break;
    }
    case IntID::AllowOverlayScrollbarsOverlap: {
      aResult = 1;
      break;
    }
    case IntID::ScrollbarFadeBeginDelay: {
      aResult = 1000;
      break;
    }
    case IntID::ScrollbarFadeDuration: {
      aResult = 400;
      break;
    }
    case IntID::ScrollbarDisplayOnMouseMove: {
      aResult = 1;
      break;
    }
    case IntID::PanelAnimations:
      aResult = [&]() -> bool {
        if (!sCSDAvailable) {
          // Disabled on systems without CSD, see bug 1385079.
          return false;
        }
        if (GdkIsWaylandDisplay()) {
          // Disabled on wayland, see bug 1800442 and bug 1800368.
          return false;
        }
        return true;
      }();
      break;
    case IntID::UseOverlayScrollbars: {
      aResult = StaticPrefs::widget_gtk_overlay_scrollbars_enabled();
      break;
    }
    case IntID::HideCursorWhileTyping: {
      aResult = StaticPrefs::widget_gtk_hide_pointer_while_typing_enabled();
      break;
    }
    case IntID::TouchDeviceSupportPresent:
      aResult = widget::WidgetUtilsGTK::IsTouchDeviceSupportPresent();
      break;
    default:
      aResult = 0;
      res = NS_ERROR_FAILURE;
  }

  return res;
}

nsresult nsLookAndFeel::NativeGetFloat(FloatID aID, float& aResult) {
  nsresult rv = NS_OK;
  switch (aID) {
    case FloatID::IMEUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::SpellCheckerUnderlineRelativeSize:
      aResult = 1.0f;
      break;
    case FloatID::CaretAspectRatio:
      EnsureInit();
      aResult = mThemeDefault.mCaretRatio;
      break;
    case FloatID::TextScaleFactor:
      aResult = gfxPlatformGtk::GetFontScaleFactor();
      break;
    default:
      aResult = -1.0;
      rv = NS_ERROR_FAILURE;
  }
  return rv;
}

static void GetSystemFontInfo(GtkStyleContext* aStyle, nsString* aFontName,
                              gfxFontStyle* aFontStyle) {
  aFontStyle->style = FontSlantStyle::NORMAL;

  // As in
  // https://git.gnome.org/browse/gtk+/tree/gtk/gtkwidget.c?h=3.22.19#n10333
  const char* fontName;
  g_object_get(gtk_settings_get_default(), "gtk-font-name", fontName, nullptr);
  
  PangoFontDescription* desc = pango_font_description_from_string(fontName);
  aFontStyle->systemFont = true;

  constexpr auto quote = u"\""_ns;
  NS_ConvertUTF8toUTF16 family(pango_font_description_get_family(desc));
  *aFontName = quote + family + quote;

  aFontStyle->weight =
      FontWeight::FromInt(pango_font_description_get_weight(desc));

  // FIXME: Set aFontStyle->stretch correctly!
  aFontStyle->stretch = FontStretch::NORMAL;

  float size = float(pango_font_description_get_size(desc)) / PANGO_SCALE;

  // |size| is now either pixels or pango-points (not Mozilla-points!)

  if (!pango_font_description_get_size_is_absolute(desc)) {
    // |size| is in pango-points, so convert to pixels.
    size *= float(gfxPlatformGtk::GetFontScaleDPI()) / POINTS_PER_INCH_FLOAT;
  }

  // |size| is now pixels but not scaled for the hidpi displays,
  aFontStyle->size = size;

  pango_font_description_free(desc);
}

bool nsLookAndFeel::NativeGetFont(FontID aID, nsString& aFontName,
                                  gfxFontStyle& aFontStyle) {
  return mThemeDefault.GetFont(aID, aFontName, aFontStyle);
}

bool nsLookAndFeel::PerThemeData::GetFont(FontID aID, nsString& aFontName,
                                          gfxFontStyle& aFontStyle) const {
  switch (aID) {
    case FontID::Menu:             // css2
    case FontID::MozPullDownMenu:  // css3
      aFontName = mMenuFontName;
      aFontStyle = mMenuFontStyle;
      break;

    case FontID::MozField:  // css3
    case FontID::MozList:   // css3
      aFontName = mFieldFontName;
      aFontStyle = mFieldFontStyle;
      break;

    case FontID::MozButton:  // css3
      aFontName = mButtonFontName;
      aFontStyle = mButtonFontStyle;
      break;

    case FontID::Caption:       // css2
    case FontID::Icon:          // css2
    case FontID::MessageBox:    // css2
    case FontID::SmallCaption:  // css2
    case FontID::StatusBar:     // css2
    default:
      aFontName = mDefaultFontName;
      aFontStyle = mDefaultFontStyle;
      break;
  }

  // Convert GDK pixels to CSS pixels.
  // When "layout.css.devPixelsPerPx" > 0, this is not a direct conversion.
  // The difference produces a scaling of system fonts in proportion with
  // other scaling from the change in CSS pixel sizes.
  aFontStyle.size /= LookAndFeel::GetTextScaleFactor();
  return true;
}

static nsCString GetGtkSettingsStringKey(const char* aKey) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
  nsCString ret;
  GtkSettings* settings = gtk_settings_get_default();
  char* value = nullptr;
  g_object_get(settings, aKey, &value, nullptr);
  if (value) {
    ret.Assign(value);
    g_free(value);
  }
  return ret;
}

static nsCString GetGtkTheme() {
  auto theme = GetGtkSettingsStringKey("gtk-theme-name");
  if (theme.IsEmpty()) {
    theme.AssignLiteral("Adwaita");
  }
  return theme;
}

static bool GetPreferDarkTheme() {
  GtkSettings* settings = gtk_settings_get_default();
  gboolean preferDarkTheme = FALSE;
  g_object_get(settings, "gtk-application-prefer-dark-theme", &preferDarkTheme,
               nullptr);
  return preferDarkTheme == TRUE;
}

static bool AnyColorChannelIsDifferent(nscolor aColor) {
  return NS_GET_R(aColor) != NS_GET_G(aColor) ||
         NS_GET_R(aColor) != NS_GET_B(aColor);
}


void nsLookAndFeel::ClearRoundedCornerProvider() {
  if (!mRoundedCornerProvider) {
    return;
  }
  //gtk_style_context_remove_provider_for_display(
  //    gdk_display_get_default(),
  //    GTK_STYLE_PROVIDER(mRoundedCornerProvider.get()));
  mRoundedCornerProvider = nullptr;
}

void nsLookAndFeel::UpdateRoundedBottomCornerStyles() {
  ClearRoundedCornerProvider();
  if (!StaticPrefs::widget_gtk_rounded_bottom_corners_enabled()) {
    return;
  }
  int32_t radius = EffectiveTheme().mTitlebarRadius;
  if (!radius) {
    return;
  }
  mRoundedCornerProvider = dont_AddRef(gtk_css_provider_new());
  nsPrintfCString string(
      "window.csd decoration {"
      "border-bottom-right-radius: %dpx;"
      "border-bottom-left-radius: %dpx;"
      "}\n",
      radius, radius);
  //GUniquePtr<GError> error;
  /*
  gtk_css_provider_load_from_data(mRoundedCornerProvider.get(),
                                       string.get(), string.Length());
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(),
      GTK_STYLE_PROVIDER(mRoundedCornerProvider.get()),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  */
}

Maybe<ColorScheme> nsLookAndFeel::ComputeColorSchemeSetting() {
  {
    // Check the pref explicitly here. Usually this shouldn't be needed, but
    // since we can only load one GTK theme at a time, and the pref will
    // override the effective value that the rest of gecko assumes for the
    // "system" color scheme, we need to factor it in our GTK theme decisions.
    int32_t pref = 0;
    if (NS_SUCCEEDED(Preferences::GetInt("ui.systemUsesDarkTheme", &pref))) {
      return Some(pref ? ColorScheme::Dark : ColorScheme::Light);
    }
  }

  return mDBusSettings.mColorScheme;
}

void nsLookAndFeel::Initialize() {
  LOGLNF("nsLookAndFeel::Initialize");
  MOZ_DIAGNOSTIC_ASSERT(!mInitialized);
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread(),
                        "LookAndFeel init should be done on the main thread");

  mInitialized = true;

  GtkSettings* settings = gtk_settings_get_default();
  if (MOZ_UNLIKELY(!settings)) {
    NS_WARNING("EnsureInit: No settings");
    return;
  }

  AutoRestore<bool> restoreIgnoreSettings(sIgnoreChangedSettings);
  sIgnoreChangedSettings = true;

  // First initialize global settings.
  InitializeGlobalSettings();

  // Record our default theme settings now.
  mThemeDefault.Init("Default", FALSE);
  mThemeDefaultDark.Init("Default-dark", TRUE);

  LOGLNF("Theme: %s. Theme Dark: %s\n", mThemeDefault.mName.get(),
         mThemeDefaultDark.mName.get());

  ConfigureFinalEffectiveTheme();

  RecordTelemetry();
}

void nsLookAndFeel::InitializeGlobalSettings() {
  GtkSettings* settings = gtk_settings_get_default();

  mColorSchemePreference = ComputeColorSchemeSetting();

  gboolean enableAnimations = false;
  g_object_get(settings, "gtk-enable-animations", &enableAnimations, nullptr);
  mPrefersReducedMotion = !enableAnimations;

  gint blink_time = 0;     // In milliseconds
  gint blink_timeout = 0;  // in seconds
  gboolean blink;
  g_object_get(settings, "gtk-cursor-blink-time", &blink_time,
               "gtk-cursor-blink-timeout", &blink_timeout, "gtk-cursor-blink",
               &blink, nullptr);
  // From
  // https://docs.gtk.org/gtk3/property.Settings.gtk-cursor-blink-timeout.html:
  //
  //     Setting this to zero has the same effect as setting
  //     GtkSettings:gtk-cursor-blink to FALSE.
  //
  mCaretBlinkTime = blink && blink_timeout ? (int32_t)blink_time : 0;

  if (mCaretBlinkTime) {
    // blink_time * 2 because blink count is a full blink cycle.
    mCaretBlinkCount =
        std::max(1, int32_t(std::ceil(float(blink_timeout * 1000) /
                                      (float(blink_time) * 2.0f))));
  } else {
    mCaretBlinkCount = -1;
  }

  mCSDCloseButton = true;
  mCSDMinimizeButton = false;
  mCSDMaximizeButton = false;
  mCSDCloseButtonPosition = 0;
  mCSDMinimizeButtonPosition = 0;
  mCSDMaximizeButtonPosition = 0;

  struct actionMapping {
    TitlebarAction action;
    char name[100];
  } ActionMapping[] = {
      {TitlebarAction::None, "none"},
      {TitlebarAction::WindowLower, "lower"},
      {TitlebarAction::WindowMenu, "menu"},
      {TitlebarAction::WindowMinimize, "minimize"},
      {TitlebarAction::WindowMaximize, "maximize"},
      {TitlebarAction::WindowMaximizeToggle, "toggle-maximize"},
  };

  auto GetWindowAction = [&](const char* eventName) -> TitlebarAction {
    gchar* action = nullptr;
    g_object_get(settings, eventName, &action, nullptr);
    if (!action) {
      return TitlebarAction::None;
    }
    auto free = mozilla::MakeScopeExit([&] { g_free(action); });
    for (auto const& mapping : ActionMapping) {
      if (!strncmp(action, mapping.name, strlen(mapping.name))) {
        return mapping.action;
      }
    }
    return TitlebarAction::None;
  };

  mDoubleClickAction = GetWindowAction("gtk-titlebar-double-click");
  mMiddleClickAction = GetWindowAction("gtk-titlebar-middle-click");
}

void nsLookAndFeel::ConfigureFinalEffectiveTheme() {
  //MOZ_ASSERT(mSystemThemeOverridden,
  //           "By this point, the alt theme should be configured");
  const bool shouldUseDarkTheme = [&] {
    using ChromeSetting = PreferenceSheet::ChromeColorSchemeSetting;
    // NOTE: We can't call ColorSchemeForChrome directly because this might run
    // while we're computing it.
    switch (PreferenceSheet::ColorSchemeSettingForChrome()) {
      case ChromeSetting::Light:
        return false;
      case ChromeSetting::Dark:
        return true;
      case ChromeSetting::System:
        break;
    };
    // Check xdg-portal.
    /*
    if (*mDBusSettings.mColorScheme == ColorScheme::Dark) {
      return true;
    } else if (*mDBusSettings.mColorScheme == ColorScheme::Light) {
      return false;
    }
    */
    // Last resort is to check gtksettings.
    return GetPreferDarkTheme();
  }();

  mSystemUsesDarkTheme = shouldUseDarkTheme;
  UpdateRoundedBottomCornerStyles();
  //moz_gtk_refresh();
}
/*
using ColorPair = nsLookAndFeel::ColorPair;
static ColorPair GetColorPair(GdkRGBA* colFg, GdkRGBA* colBg) {
  ColorPair result;
  result.mFg = GDK_RGBA_TO_NS_RGBA(*colFg);
  result.mBg = GDK_RGBA_TO_NS_RGBA(*colBg);
  return result;
}
*/

struct nsLookAndFeel::ThemeProto ThemeDefault = {
  THEME_DEFAULT_BASE,
  THEME_DEFAULT_TEXT,
  THEME_DEFAULT_BG,
  THEME_DEFAULT_FG,
  THEME_DEFAULT_BG_SEL,
  THEME_DEFAULT_FG_SEL,
  THEME_DEFAULT_BORD,
  THEME_DEFAULT_BORD_SEL,
  THEME_DEFAULT_BORD_EDGE,
  THEME_DEFAULT_BORD_ALT,
  THEME_DEFAULT_LINK,
  THEME_DEFAULT_LINK_VIS,
  THEME_DEFAULT_HB_BG,
  THEME_DEFAULT_MENU,
  THEME_DEFAULT_MENU_SEL,
  THEME_DEFAULT_SCROLL_BG,
  THEME_DEFAULT_SCROLL_SL,
  THEME_DEFAULT_SCROLL_SL_HO,
  THEME_DEFAULT_SCROLL_SL_AC,
  THEME_DEFAULT_WARN,
  THEME_DEFAULT_ERR,
  THEME_DEFAULT_SUCC,
  THEME_DEFAULT_DEST,
  THEME_DEFAULT_DARK_FILL,
  THEME_DEFAULT_SIDE_BG,
  THEME_DEFAULT_SHADOW,
  THEME_DEFAULT_BG_INS,
  THEME_DEFAULT_FG_INS,
  THEME_DEFAULT_BORD_INS,
  THEME_DEFAULT_BASE_BACK,
  THEME_DEFAULT_TEXT_BACK,
  THEME_DEFAULT_BG_BACK,
  THEME_DEFAULT_FG_BACK,
  THEME_DEFAULT_BACK_INS,
  THEME_DEFAULT_FG_BACK_SEL,
  THEME_DEFAULT_SCROLL_BG_BACK,
  THEME_DEFAULT_SCROLL_SL_BACK,
  THEME_DEFAULT_BORD_BACK,
  THEME_DEFAULT_DARK_FILL_BACK,
  THEME_DEFAULT_DROP,
  THEME_DEFAULT_TOOLT_BG,
  THEME_DEFAULT_TOOLT_FG,
  THEME_DEFAULT_TOOLT_BORD
};

struct nsLookAndFeel::ThemeProto ThemeDefaultDark = {
  THEME_DEFAULT_DARK_BASE,
  THEME_DEFAULT_DARK_TEXT,
  THEME_DEFAULT_DARK_BG,
  THEME_DEFAULT_DARK_FG,
  THEME_DEFAULT_DARK_BG_SEL,
  THEME_DEFAULT_DARK_FG_SEL,
  THEME_DEFAULT_DARK_BORD,
  THEME_DEFAULT_DARK_BORD_SEL,
  THEME_DEFAULT_DARK_BORD_EDGE,
  THEME_DEFAULT_DARK_BORD_ALT,
  THEME_DEFAULT_DARK_LINK,
  THEME_DEFAULT_DARK_LINK_VIS,
  THEME_DEFAULT_DARK_HB_BG,
  THEME_DEFAULT_DARK_MENU,
  THEME_DEFAULT_DARK_MENU_SEL,
  THEME_DEFAULT_DARK_SCROLL_BG,
  THEME_DEFAULT_DARK_SCROLL_SL,
  THEME_DEFAULT_DARK_SCROLL_SL_HO,
  THEME_DEFAULT_DARK_SCROLL_SL_AC,
  THEME_DEFAULT_DARK_WARN,
  THEME_DEFAULT_DARK_ERR,
  THEME_DEFAULT_DARK_SUCC,
  THEME_DEFAULT_DARK_DEST,
  THEME_DEFAULT_DARK_DARK_FILL,
  THEME_DEFAULT_DARK_SIDE_BG,
  THEME_DEFAULT_DARK_SHADOW,
  THEME_DEFAULT_DARK_BG_INS,
  THEME_DEFAULT_DARK_FG_INS,
  THEME_DEFAULT_DARK_BORD_INS,
  THEME_DEFAULT_DARK_BASE_BACK,
  THEME_DEFAULT_DARK_TEXT_BACK,
  THEME_DEFAULT_DARK_BG_BACK,
  THEME_DEFAULT_DARK_FG_BACK,
  THEME_DEFAULT_DARK_BACK_INS,
  THEME_DEFAULT_DARK_FG_BACK_SEL,
  THEME_DEFAULT_DARK_SCROLL_BG_BACK,
  THEME_DEFAULT_DARK_SCROLL_SL_BACK,
  THEME_DEFAULT_DARK_BORD_BACK,
  THEME_DEFAULT_DARK_DARK_FILL_BACK,
  THEME_DEFAULT_DARK_DROP,
  THEME_DEFAULT_DARK_TOOLT_BG,
  THEME_DEFAULT_DARK_TOOLT_FG,
  THEME_DEFAULT_DARK_TOOLT_BORD
};

void nsLookAndFeel::PerThemeData::Init(const char* themeName, bool isDark) {
  //mHighContrast = StaticPrefs::widget_content_gtk_high_contrast_enabled() &&
  //                mName.Find("HighContrast"_ns) >= 0;

  mName = themeName;
  mIsDark = isDark;

  if (strcmp(themeName, "Default-dark") == 0) {
    mTheme = &ThemeDefaultDark;
  } else {
    mTheme = &ThemeDefault;
  }

  // Window
  mWindow.mFg = mTheme->pFg;
  mWindow.mBg = mTheme->pBg;
  mDialog = mWindow;
  mMozWindowActiveBorder = mTheme->pBord;
  mMozWindowInactiveBorder = mTheme->pBordBack;

  // Titlebar
  mTitlebar.mFg = mTheme->pText;
  mTitlebar.mBg =  mTheme->pBg;
  mTitlebarInactive.mFg = mTheme->pFgBack;
  mTitlebarInactive.mBg = mTheme->pBgBack;
  //mTitlebarRadius = IsSolidCSDStyleUsed() ? 0 : GetBorderRadius(style);
  //mTitlebarButtonSpacing = moz_gtk_get_titlebar_button_spacing();
  mHeaderBar = mTitlebar;
  mHeaderBarInactive = mTitlebarInactive;

  // Scrollbar
  mThemedScrollbar = mTheme->pScrollBg;
  mThemedScrollbarInactive = mTheme->pScrollBgBack;
  mThemedScrollbarInactive =
      NS_ComposeColors(mThemedScrollbarInactive, mTheme->pScrollBg);
  mThemedScrollbarThumb = mTheme->pScrollSl;
  mThemedScrollbarThumbHover = mTheme->pScrollSlHo;
  mThemedScrollbarThumbActive = mTheme->pScrollSlAc;
  mThemedScrollbarThumbInactive = mTheme->pScrollSlBack;

  // Tooltip
  mInfo.mFg = mTheme->pTooltFg;
  mInfo.mBg = mTheme->pTooltBg;

  // Menu
  mMenu.mBg = mTheme->pMenu;
  mMenu.mFg = mTheme->pFg;
  mMenuHover.mFg = mTheme->pFg;
  mMenuHover.mBg = NS_ComposeColors(mMenu.mBg, mMenu.mFg);
  mMenuHover.mBg = mTheme->pBg;

  // Text
  //ApplyColorOver(col0, &col1);
  mField.mBg = mTheme->pBg;
  mField.mFg = mTheme->pText;
  mSidebar = mField;
  mSidebarBorder = mTheme->pFgBack;
  mGrayText = NS_RGB(120, 120, 120);
  mThreeDShadow = mGrayText;
  mThreeDHighlight = mGrayText;

  // Selected text and background
  mSelectedText.mBg = mTheme->pBgSel;
  mSelectedText.mFg = mTheme->pFgSel;
  mSelectedItem = mSelectedText;
  mCellHighlight.mBg = mTheme->pBgSel;
  mCellHighlight.mFg = mTheme->pBgSel;
  mAccent.mBg = mSelectedItem.mBg;
  mAccent.mFg = mSelectedItem.mFg;
  mOddCellBackground = mTheme->pBg;
  //EnsureColorPairIsOpaque(mAccent);
  //PreferDarkerBackground(mAccent);

  // Button
  mButtonBorder = mTheme->pBordBack;
  mButton.mFg = mTheme->pText;
  mButton.mBg = mTheme->pBg;
  mButtonHover.mFg = mTheme->pText;
  mButtonHover.mBg = mTheme->pBg;
  mButtonActive.mFg = mTheme->pText;
  mButtonActive.mBg = mTheme->pBg;

  // Combobox
  mComboBoxText = mTheme->pText;

  // Column header
  mMozColHeader.mFg = mTheme->pFg;
  mMozColHeader.mBg = mTheme->pBg;
  mMozColHeaderHover.mFg = mTheme->pFg;
  mMozColHeaderHover.mBg = mTheme->pBg;
  mMozColHeaderActive.mFg = mTheme->pFg;
  mMozColHeaderActive.mBg = mTheme->pBg;

  // Some themes have a unified menu bar, and support window dragging on it
  gboolean supports_menubar_drag = FALSE;
  mMenuSupportsDrag = supports_menubar_drag;

  // Links
  mNativeHyperLinkText = mTheme->pLink;
  mNativeVisitedHyperLinkText = mTheme->pLinkVis;

  // invisible character styles
  guint value = 42;
  mInvisibleCharacter = char16_t(value);

  if (LOGLNF_ENABLED()) {
    LOGLNF("Initialized theme %s \n", mName.get());
    for (auto id : MakeEnumeratedRange(ColorID::End)) {
      nscolor color;
      nsresult rv = GetColor(id, color);
      LOGLNF(" * color %d: pref=%s success=%d value=%x\n", int(id),
             GetColorPrefName(id), NS_SUCCEEDED(rv),
             NS_SUCCEEDED(rv) ? color : 0);
    }
    LOGLNF(" * titlebar-radius: %d\n", mTitlebarRadius);
  }
}

// virtual
char16_t nsLookAndFeel::GetPasswordCharacterImpl() {
  EnsureInit();
  return mThemeDefault.mInvisibleCharacter;
}

bool nsLookAndFeel::GetEchoPasswordImpl() { return false; }

bool nsLookAndFeel::GetDefaultDrawInTitlebar() { return sCSDAvailable; }

nsXPLookAndFeel::TitlebarAction nsLookAndFeel::GetTitlebarAction(
    TitlebarEvent aEvent) {
  return aEvent == TitlebarEvent::Double_Click ? mDoubleClickAction
                                               : mMiddleClickAction;
}

void nsLookAndFeel::GetThemeInfo(nsACString& aInfo) {
  aInfo.Append(mThemeDefault.mName);
  aInfo.Append(" / ");
  aInfo.Append(mThemeDefaultDark.mName);
}

nsresult nsLookAndFeel::GetKeyboardLayoutImpl(nsACString& aLayout) {
  if (mozilla::widget::GdkIsX11Display()) {
#if defined(MOZ_X11)
    Display* display = gdk_x11_get_default_xdisplay();
    if (!display) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    XkbDescRec* kbdDesc = XkbAllocKeyboard();
    if (!kbdDesc) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    auto cleanup = MakeScopeExit([&] { XkbFreeKeyboard(kbdDesc, 0, true); });

    XkbStateRec state;
    XkbGetState(display, XkbUseCoreKbd, &state);
    uint32_t group = state.group;

    XkbGetNames(display, XkbGroupNamesMask, kbdDesc);

    if (!kbdDesc->names || !kbdDesc->names->groups[group]) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    char* layout = XGetAtomName(display, kbdDesc->names->groups[group]);

    aLayout.Assign(layout);
#endif
  } else {
#if defined(MOZ_WAYLAND)
    struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    auto cleanupContext = MakeScopeExit([&] { xkb_context_unref(context); });

    struct xkb_keymap* keymap = xkb_keymap_new_from_names(
        context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    auto cleanupKeymap = MakeScopeExit([&] { xkb_keymap_unref(keymap); });

    const char* layout = xkb_keymap_layout_get_name(keymap, 0);

    if (layout) {
      aLayout.Assign(layout);
    }
#endif
  }

  return NS_OK;
}

void nsLookAndFeel::RecordLookAndFeelSpecificTelemetry() {
  // Gtk version we're on.
  nsCString version;
  version.AppendPrintf("%d.%d", GTK_MAJOR_VERSION, GTK_MINOR_VERSION);
  glean::widget::gtk_version.Set(version);
}

#undef LOGLNF
#undef LOGLNF_ENABLED
