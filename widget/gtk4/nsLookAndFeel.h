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

#define THEME_DEFAULT_BASE NS_RGBA(255, 255, 255, 255)
#define THEME_DEFAULT_TEXT NS_RGBA(0, 0, 0, 255)
#define THEME_DEFAULT_BG NS_RGBA(246, 245, 244, 255)
#define THEME_DEFAULT_FG NS_RGBA(46, 52, 54, 255)
#define THEME_DEFAULT_BG_SEL NS_RGBA(53, 132, 228, 255)
#define THEME_DEFAULT_FG_SEL NS_RGBA(255, 255, 255, 255)
#define THEME_DEFAULT_BORD NS_RGBA(205, 199, 194, 255)
#define THEME_DEFAULT_BORD_SEL NS_RGBA(24, 95, 180, 255)
#define THEME_DEFAULT_BORD_EDGE NS_RGBA(255, 255, 255, 204)
#define THEME_DEFAULT_BORD_ALT NS_RGBA(191, 184, 177, 255)
#define THEME_DEFAULT_LINK NS_RGBA(27, 106, 203, 255)
#define THEME_DEFAULT_LINK_VIS NS_RGBA(21, 83, 158, 255)
#define THEME_DEFAULT_HB_BG NS_RGBA(255, 255, 255, 255)
#define THEME_DEFAULT_MENU THEME_DEFAULT_BASE
#define THEME_DEFAULT_MENU_SEL NS_RGBA(232, 230, 227, 255)
#define THEME_DEFAULT_SCROLL_BG NS_RGBA(206, 206, 206, 255)
#define THEME_DEFAULT_SCROLL_SL NS_RGBA(126, 129, 130, 255)
#define THEME_DEFAULT_SCROLL_SL_HO NS_RGBA(86, 91, 92, 255)
#define THEME_DEFAULT_SCROLL_SL_AC NS_RGBA(27, 106, 203, 255)
#define THEME_DEFAULT_WARN NS_RGBA(245, 121, 0, 255)
#define THEME_DEFAULT_ERR NS_RGBA(204, 0, 0, 255)
#define THEME_DEFAULT_SUCC NS_RGBA(51, 209, 122, 255)
#define THEME_DEFAULT_DEST NS_RGBA(224, 27, 36, 255)
#define THEME_DEFAULT_DARK_FILL NS_RGBA(226, 223, 220, 255)
#define THEME_DEFAULT_SIDE_BG NS_RGBA(251, 250, 250, 255)
#define THEME_DEFAULT_SHADOW NS_RGBA(0, 0, 0, 25)
#define THEME_DEFAULT_BG_INS NS_RGBA(250, 249, 248, 255)
#define THEME_DEFAULT_FG_INS NS_RGBA(146, 149, 149, 255)
#define THEME_DEFAULT_BORD_INS NS_RGBA(214, 210, 205, 255)
#define THEME_DEFAULT_BASE_BACK NS_RGBA(252, 252, 252, 255)
#define THEME_DEFAULT_TEXT_BACK NS_RGBA(50, 50, 50, 255)
#define THEME_DEFAULT_BG_BACK THEME_DEFAULT_BG
#define THEME_DEFAULT_FG_BACK NS_RGBA(146, 149, 149, 255)
#define THEME_DEFAULT_BACK_INS NS_RGBA(212, 207, 202, 255)
#define THEME_DEFAULT_FG_BACK_SEL THEME_DEFAULT_BASE_BACK
#define THEME_DEFAULT_SCROLL_BG_BACK NS_RGBA(239, 237, 236, 255)
#define THEME_DEFAULT_SCROLL_SL_BACK NS_RGBA(146, 146, 145, 255)
#define THEME_DEFAULT_BORD_BACK NS_RGBA(214, 210, 205, 255)
#define THEME_DEFAULT_DARK_FILL_BACK NS_RGBA(235, 233, 230, 255)
#define THEME_DEFAULT_DROP NS_RGBA(46, 194, 126, 255)
#define THEME_DEFAULT_TOOLT_BG NS_RGBA(20, 20, 20, 255)
#define THEME_DEFAULT_TOOLT_FG NS_RGBA(255, 255, 255, 255)
#define THEME_DEFAULT_TOOLT_BORD NS_RGBA(255, 255, 255, 25)

#define THEME_DEFAULT_DARK_BASE NS_RGBA(45, 45, 45, 255)
#define THEME_DEFAULT_DARK_TEXT NS_RGBA(255, 255, 255, 255)
#define THEME_DEFAULT_DARK_BG NS_RGBA(53, 53, 53, 255)
#define THEME_DEFAULT_DARK_FG NS_RGBA(238, 238, 236, 255)
#define THEME_DEFAULT_DARK_BG_SEL NS_RGBA(21, 83, 158, 255)
#define THEME_DEFAULT_DARK_FG_SEL NS_RGBA(255, 255, 255, 255)
#define THEME_DEFAULT_DARK_BORD NS_RGBA(28, 28, 28, 255)
#define THEME_DEFAULT_DARK_BORD_SEL NS_RGBA(3, 12, 23, 255)
#define THEME_DEFAULT_DARK_BORD_EDGE NS_RGBA(238, 238, 236, 18)
#define THEME_DEFAULT_DARK_BORD_ALT NS_RGBA(7, 7, 7, 255)
#define THEME_DEFAULT_DARK_LINK NS_RGBA(53, 132, 228, 255)
#define THEME_DEFAULT_DARK_LINK_VIS NS_RGBA(27, 107, 203, 255)
#define THEME_DEFAULT_DARK_HB_BG NS_RGBA(45, 45, 45, 255)
#define THEME_DEFAULT_DARK_MENU THEME_DEFAULT_DARK_BASE
#define THEME_DEFAULT_DARK_MENU_SEL NS_RGBA(33, 33, 33, 255)
#define THEME_DEFAULT_DARK_SCROLL_BG NS_RGBA(49, 49, 49, 255)
#define THEME_DEFAULT_DARK_SCROLL_SL NS_RGBA(164, 164, 163, 255)
#define THEME_DEFAULT_DARK_SCROLL_SL_HO NS_RGBA(201, 201, 199, 255)
#define THEME_DEFAULT_DARK_SCROLL_SL_AC NS_RGBA(27, 107, 203, 255)
#define THEME_DEFAULT_DARK_WARN NS_RGBA(245, 121, 0, 255)
#define THEME_DEFAULT_DARK_ERR NS_RGBA(204, 0, 0, 255)
#define THEME_DEFAULT_DARK_SUCC NS_RGBA(38, 171, 98, 255)
#define THEME_DEFAULT_DARK_DEST NS_RGBA(178, 22, 29, 255)
#define THEME_DEFAULT_DARK_DARK_FILL NS_RGBA(41, 41, 41, 255)
#define THEME_DEFAULT_DARK_SIDE_BG NS_RGBA(49, 49, 49, 255)
#define THEME_DEFAULT_DARK_SHADOW NS_RGBA(0, 0, 0, 26)
#define THEME_DEFAULT_DARK_BG_INS NS_RGBA(50, 50, 50, 255)
#define THEME_DEFAULT_DARK_FG_INS NS_RGBA(146, 146, 145, 255)
#define THEME_DEFAULT_DARK_BORD_INS NS_RGBA(33, 33, 3, 255)
#define THEME_DEFAULT_DARK_BASE_BACK NS_RGBA(48, 48, 48, 255)
#define THEME_DEFAULT_DARK_TEXT_BACK NS_RGBA(214, 214, 214, 255)
#define THEME_DEFAULT_DARK_BG_BACK THEME_DEFAULT_DARK_BG
#define THEME_DEFAULT_DARK_FG_BACK NS_RGBA(146, 146, 145, 255)
#define THEME_DEFAULT_DARK_BACK_INS NS_RGBA(91, 91, 91, 255)
#define THEME_DEFAULT_DARK_FG_BACK_SEL THEME_DEFAULT_DARK_TEXT_BACK
#define THEME_DEFAULT_DARK_SCROLL_BG_BACK NS_RGBA(45, 45, 45, 255)
#define THEME_DEFAULT_DARK_SCROLL_SL_BACK NS_RGBA(53, 53, 53, 255)
#define THEME_DEFAULT_DARK_BORD_BACK NS_RGBA(33, 33, 33, 255)
#define THEME_DEFAULT_DARK_DARK_FILL_BACK NS_RGBA(46, 46, 46, 255)
#define THEME_DEFAULT_DARK_DROP NS_RGBA(38, 162, 105, 255)
#define THEME_DEFAULT_DARK_TOOLT_BG NS_RGBA(20, 20, 20, 255)
#define THEME_DEFAULT_DARK_TOOLT_FG NS_RGBA(255, 255, 255, 255)
#define THEME_DEFAULT_DARK_TOOLT_BORD NS_RGBA(255, 255, 255, 26)

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
  struct ThemeProto {
    nscolor pBase;
    nscolor pText;
    nscolor pBg;
    nscolor pFg;
    nscolor pBgSel;
    nscolor pFgSel;
    nscolor pBord;
    nscolor pBordSel;
    nscolor pBordEdge;
    nscolor pBordAlt;
    nscolor pLink;
    nscolor pLinkVis;
    nscolor pHbBg;
    nscolor pMenu;
    nscolor pMenuSel;
    nscolor pScrollBg;
    nscolor pScrollSl;
    nscolor pScrollSlHo;
    nscolor pScrollSlAc;
    nscolor pWarn;
    nscolor pErr;
    nscolor pSucc;
    nscolor pDest;
    nscolor pDarkFill;
    nscolor pSideBg;
    nscolor pShadow;
    nscolor pBgIns;
    nscolor pFgIns;
    nscolor pBordIns;
    nscolor pBaseBack;
    nscolor pTextBack;
    nscolor pBgBack;
    nscolor pFgBack;
    nscolor pBackIns;
    nscolor pFgBackSel;
    nscolor pScrollBgBack;
    nscolor pScrollSlBack;
    nscolor pBordBack;
    nscolor pDarkFillBack;
    nscolor pDrop;
    nscolor pTooltBg;
    nscolor pTooltFg;
    nscolor pTooltBord;
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
    ThemeProto* mTheme;

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
