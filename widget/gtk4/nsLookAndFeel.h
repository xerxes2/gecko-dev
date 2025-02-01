/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "X11UndefineNone.h"
#include "nsXPLookAndFeel.h"
#include "nsCOMPtr.h"
#include "gfxFont.h"

#define THEME_DEFAULT_BASE "#ffffff"
#define THEME_DEFAULT_TEXT "#000000"
#define THEME_DEFAULT_BG "#f6f5f4"
#define THEME_DEFAULT_FG "#2e3436"
#define THEME_DEFAULT_FG_SEL "#ffffff"
#define THEME_DEFAULT_BG_SEL "#3584e4"
#define THEME_DEFAULT_BORD "darken($bg_color, 18%)"
#define THEME_DEFAULT_BORD_SEL "darken($selected_bg_color, 15%)"
#define THEME_DEFAULT_BORD_EDGE "transparentize(white, 0.2)"
#define THEME_DEFAULT_BORD_ALT "darken($bg_color, 24%)"
#define THEME_DEFAULT_LINK "darken($selected_bg_color, 10%)"
#define THEME_DEFAULT_LINK_VIS "darken($selected_bg_color, 20%)"
#define THEME_DEFAULT_HB_BG "lighten($bg_color, 5%)"
#define THEME_DEFAULT_MENU THEME_DEFAULT_BASE
#define THEME_DEFAULT_MENU_SEL "darken($bg_color, 6%)"
#define THEME_DEFAULT_SCROLL_BG "mix($bg_color, $fg_color, 80%)"
#define THEME_DEFAULT_SCROLL_SL "mix($fg_color, $bg_color, 60%)"
#define THEME_DEFAULT_SCROLL_SL_HO "mix($fg_color, $bg_color, 80%)"
#define THEME_DEFAULT_SCROLL_SL_AC "darken($selected_bg_color, 10%)"
#define THEME_DEFAULT_WARN "#f57900"
#define THEME_DEFAULT_ERR "#cc0000"
#define THEME_DEFAULT_SUCC "#33d17a"
#define THEME_DEFAULT_DEST "#e01b24"
#define THEME_DEFAULT_DARK_FILL "mix($borders_color, $bg_color, 50%)"
#define THEME_DEFAULT_SIDE_BG "mix($bg_color, $base_color, 50%)"
#define THEME_DEFAULT_SHADOW "transparentize(black, 0.9)"
#define THEME_DEFAULT_BG_INS "mix($bg_color, $base_color, 60%)"
#define THEME_DEFAULT_FG_INS "mix($fg_color, $bg_color, 50%)"
#define THEME_DEFAULT_BORD_INS "mix($borders_color, $bg_color, 80%)"
#define THEME_DEFAULT_BASE_BACK "darken($base_color, 1%)"
#define THEME_DEFAULT_TEXT_BACK "mix($text_color, $backdrop_base_color, 80%)"
#define THEME_DEFAULT_BG_BACK THEME_DEFAULT_BG
#define THEME_DEFAULT_FG_BACK "mix($fg_color, $backdrop_bg_color, 50%)"
#define THEME_DEFAULT_BACK_INS "darken($backdrop_bg_color, 15%)"
#define THEME_DEFAULT_FG_BACK_SEL "$backdrop_base_color"
#define THEME_DEFAULT_BORD_BACK "mix($borders_color, $bg_color, 80%)"
#define THEME_DEFAULT_DARK_FILL_BACK "mix($backdrop_borders_color, $backdrop_bg_color, 35%)"
#define THEME_DEFAULT_DROP "#2ec27e"
#define THEME_DEFAULT_TOOLT_BORD "transparentize(white, 0.9)"

#define THEME_DEFAULT_DARK_BASE "lighten(desaturate(#241f31, 100%), 2%)"
#define THEME_DEFAULT_DARK_TEXT "#ffffff"
#define THEME_DEFAULT_DARK_BG "darken(desaturate(#3d3846, 100%), 4%)"
#define THEME_DEFAULT_DARK_FG "#eeeeec"
#define THEME_DEFAULT_DARK_FG_SEL "#ffffff"
#define THEME_DEFAULT_DARK_BG_SEL "darken(#3584e4, 20%)"
#define THEME_DEFAULT_DARK_BORD "darken($bg_color, 10%)"
#define THEME_DEFAULT_DARK_BORD_SEL "darken($selected_bg_color, 30%))"
#define THEME_DEFAULT_DARK_BORD_EDGE "transparentize($fg_color, 0.93))"
#define THEME_DEFAULT_DARK_BORD_ALT "darken($bg_color, 18%)"
#define THEME_DEFAULT_DARK_LINK "lighten($selected_bg_color, 20%)"
#define THEME_DEFAULT_DARK_LINK_VIS "lighten($selected_bg_color, 10%)"
#define THEME_DEFAULT_DARK_HB_BG "darken($bg_color, 3%))"
#define THEME_DEFAULT_DARK_MENU THEME_DEFAULT_DARK_BASE
#define THEME_DEFAULT_DARK_MENU_SEL "darken($bg_color, 8%)"
#define THEME_DEFAULT_DARK_SCROLL_BG "mix($base_color, $bg_color, 50%))"
#define THEME_DEFAULT_DARK_SCROLL_SL "mix($fg_color, $bg_color, 60%)"
#define THEME_DEFAULT_DARK_SCROLL_SL_HO "mix($fg_color, $bg_color, 80%)"
#define THEME_DEFAULT_DARK_SCROLL_SL_AC "lighten($selected_bg_color, 10%)"
#define THEME_DEFAULT_DARK_WARN "#f57900"
#define THEME_DEFAULT_DARK_ERR "#cc0000"
#define THEME_DEFAULT_DARK_SUCC "darken(#33d17a, 10%)"
#define THEME_DEFAULT_DARK_DEST "darken(#e01b24, 10%)"
#define THEME_DEFAULT_DARK_DARK_FILL "mix($borders_color, $bg_color, 50%)"
#define THEME_DEFAULT_DARK_SIDE_BG "mix($bg_color, $base_color, 50%)"
#define THEME_DEFAULT_DARK_SHADOW "transparentize(black, 0.9)"
#define THEME_DEFAULT_DARK_BG_INS "mix($bg_color, $base_color, 60%)"
#define THEME_DEFAULT_DARK_FG_INS "mix($fg_color, $bg_color, 50%)"
#define THEME_DEFAULT_DARK_BORD_INS "mix($borders_color, $bg_color, 80%)"
#define THEME_DEFAULT_DARK_BASE_BACK "lighten($base_color, 1%)"
#define THEME_DEFAULT_DARK_TEXT_BACK "mix($text_color, $backdrop_base_color, 80%)"
#define THEME_DEFAULT_DARK_BG_BACK THEME_DEFAULT_DARK_BG
#define THEME_DEFAULT_DARK_FG_BACK "mix($fg_color, $backdrop_bg_color, 50%)"
#define THEME_DEFAULT_DARK_BACK_INS "lighten($backdrop_bg_color, 15%))"
#define THEME_DEFAULT_DARK_FG_BACK_SEL "$backdrop_text_color"
#define THEME_DEFAULT_DARK_BORD_BACK "mix($borders_color, $bg_color, 80%)"
#define THEME_DEFAULT_DARK_DARK_FILL_BACK "mix($backdrop_borders_color, $backdrop_bg_color, 35%)"
#define THEME_DEFAULT_DARK_DROP "#26a269"
#define THEME_DEFAULT_DARK_TOOLT_BORD "transparentize(white, 0.9)"

enum WidgetNodeType : int;
struct _GtkStyle;
typedef struct _GDBusProxy GDBusProxy;
typedef struct _GtkCssProvider GtkCssProvider;
typedef struct _GFile GFile;
typedef struct _GFileMonitor GFileMonitor;
typedef struct _GVariant GVariant;

namespace mozilla {
enum class StyleGtkThemeFamily : uint8_t;
}

class nsLookAndFeel final : public nsXPLookAndFeel {
 public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  void RefreshImpl() override;
  nsresult NativeGetInt(IntID aID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID aID, float& aResult) override;
  nsresult NativeGetColor(ColorID, ColorScheme, nscolor& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;

  char16_t GetPasswordCharacterImpl() override;
  bool GetEchoPasswordImpl() override;

  bool GetDefaultDrawInTitlebar() override;

  nsXPLookAndFeel::TitlebarAction GetTitlebarAction(
      TitlebarEvent aEvent) override;

  void GetThemeInfo(nsACString&) override;

  nsresult GetKeyboardLayoutImpl(nsACString& aLayout) override;

  static const nscolor kBlack = NS_RGB(0, 0, 0);
  static const nscolor kWhite = NS_RGB(255, 255, 255);
  // Returns whether any setting changed.
  bool RecomputeDBusSettings();
  // Returns whether the setting really changed.
  bool RecomputeDBusAppearanceSetting(const nsACString& aKey, GVariant* aValue);

  struct ColorPair {
    nscolor mBg = kWhite;
    nscolor mFg = kBlack;

    bool operator==(const ColorPair& aOther) const {
      return mBg == aOther.mBg && mFg == aOther.mFg;
    }
    bool operator!=(const ColorPair& aOther) const {
      return !(*this == aOther);
    }
  };

  using ThemeFamily = mozilla::StyleGtkThemeFamily;

 protected:
  static bool WidgetUsesImage(WidgetNodeType aNodeType);
  void RecordLookAndFeelSpecificTelemetry() override;
  static bool ShouldHonorThemeScrollbarColors();
  mozilla::Maybe<ColorScheme> ComputeColorSchemeSetting();

  void WatchDBus();
  void UnwatchDBus();

  // We use up to two themes (one light, one dark), which might have different
  // sets of fonts and colors.
  struct PerThemeData {
    nsCString mName;
    bool mIsDark = false;
    bool mHighContrast = false;
    bool mPreferDarkTheme = false;

    ThemeFamily mFamily{0};

    // Cached fonts
    nsString mDefaultFontName;
    nsString mButtonFontName;
    nsString mFieldFontName;
    nsString mMenuFontName;
    gfxFontStyle mDefaultFontStyle;
    gfxFontStyle mButtonFontStyle;
    gfxFontStyle mFieldFontStyle;
    gfxFontStyle mMenuFontStyle;

    // Cached colors
    nscolor mGrayText = kBlack;
    ColorPair mInfo;
    ColorPair mMenu;
    ColorPair mMenuHover;
    ColorPair mHeaderBar;
    ColorPair mHeaderBarInactive;
    ColorPair mButton;
    ColorPair mButtonHover;
    ColorPair mButtonActive;
    nscolor mButtonBorder = kBlack;
    nscolor mThreeDHighlight = kBlack;
    nscolor mThreeDShadow = kBlack;
    nscolor mOddCellBackground = kWhite;
    nscolor mNativeHyperLinkText = kBlack;
    nscolor mNativeVisitedHyperLinkText = kBlack;
    // FIXME: This doesn't seem like it'd be sound since we use Window for
    // -moz-Combobox... But I guess we rely on chrome code not setting
    // appearance: none on selects or overriding the color if they do.
    nscolor mComboBoxText = kBlack;
    ColorPair mField;
    ColorPair mWindow;
    ColorPair mDialog;
    ColorPair mSidebar;
    nscolor mSidebarBorder = kBlack;

    nscolor mMozWindowActiveBorder = kBlack;
    nscolor mMozWindowInactiveBorder = kBlack;

    ColorPair mCellHighlight;
    ColorPair mSelectedText;
    ColorPair mAccent;
    ColorPair mSelectedItem;

    ColorPair mMozColHeader;
    ColorPair mMozColHeaderHover;
    ColorPair mMozColHeaderActive;

    ColorPair mTitlebar;
    ColorPair mTitlebarInactive;

    nscolor mThemedScrollbar = kWhite;
    nscolor mThemedScrollbarInactive = kWhite;
    nscolor mThemedScrollbarThumb = kBlack;
    nscolor mThemedScrollbarThumbHover = kBlack;
    nscolor mThemedScrollbarThumbActive = kBlack;
    nscolor mThemedScrollbarThumbInactive = kBlack;

    float mCaretRatio = 0.0f;
    int32_t mTitlebarRadius = 0;
    int32_t mTitlebarButtonSpacing = 0;
    char16_t mInvisibleCharacter = 0;
    bool mMenuSupportsDrag = false;

    void Init(const char*, bool);
    nsresult GetColor(ColorID, nscolor&) const;
    bool GetFont(FontID, nsString& aFontName, gfxFontStyle&) const;
    void InitCellHighlightColors();
  };

  PerThemeData mThemeDefault;
  PerThemeData mThemeDefaultDark;

  const PerThemeData& LightTheme() const {
    return mThemeDefault.mIsDark ? mThemeDefaultDark : mThemeDefault;
  }

  const PerThemeData& DarkTheme() const {
    return mThemeDefault.mIsDark ? mThemeDefault : mThemeDefaultDark;
  }

  const PerThemeData& EffectiveTheme() const {
    return mSystemUsesDarkTheme ? mThemeDefaultDark : mThemeDefault;
  }

  uint32_t mDBusID = 0;
  RefPtr<GFile> mKdeColors;
  RefPtr<GFileMonitor> mKdeColorsMonitor;

  mozilla::Maybe<ColorScheme> mColorSchemePreference;
  RefPtr<GDBusProxy> mDBusSettingsProxy;
  // DBus settings from:
  // https://github.com/flatpak/xdg-desktop-portal/blob/main/data/org.freedesktop.portal.Settings.xml
  struct DBusSettings {
    mozilla::Maybe<ColorScheme> mColorScheme;
    bool mPrefersContrast = false;
    // Transparent means no accent-color. Note that the real accent color cannot
    // have transparency.
    ColorPair mAccentColor{NS_TRANSPARENT, NS_TRANSPARENT};

    bool HasAccentColor() const { return NS_GET_A(mAccentColor.mBg); }
  } mDBusSettings;
  int32_t mCaretBlinkTime = 0;
  int32_t mCaretBlinkCount = -1;
  bool mCSDMaximizeButton = false;
  bool mCSDMinimizeButton = false;
  bool mCSDCloseButton = false;
  bool mCSDReversedPlacement = false;
  bool mPrefersReducedMotion = false;
  bool mInitialized = false;
  bool mSystemUsesDarkTheme = false;
  int32_t mCSDMaximizeButtonPosition = 0;
  int32_t mCSDMinimizeButtonPosition = 0;
  int32_t mCSDCloseButtonPosition = 0;
  TitlebarAction mDoubleClickAction = TitlebarAction::None;
  TitlebarAction mMiddleClickAction = TitlebarAction::None;

  RefPtr<GtkCssProvider> mRoundedCornerProvider;
  void UpdateRoundedBottomCornerStyles();

  void ClearRoundedCornerProvider();

  void EnsureInit() {
    if (mInitialized) {
      return;
    }
    Initialize();
  }

  void Initialize();

  void RestoreSystemTheme();
  void InitializeGlobalSettings();
  // Returns whether we found an alternative theme.
  bool ConfigureAltTheme();
  void ConfigureAndInitializeAltTheme();
  void ConfigureFinalEffectiveTheme();
  void MaybeApplyAdwaitaOverrides();
};

#endif
