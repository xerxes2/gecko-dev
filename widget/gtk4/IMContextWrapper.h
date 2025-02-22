/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IMContextWrapper_h_
#define IMContextWrapper_h_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIWidget.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/ContentData.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Maybe.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/WritingModes.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/widget/IMEData.h"

class nsWindow;

namespace mozilla {
namespace widget {

/**
 * KeyHandlingState is result of IMContextWrapper::OnKeyEvent().
 */
enum class KeyHandlingState {
  // The native key event has not been handled by IMContextWrapper.
  eNotHandled,
  // The native key event was handled by IMContextWrapper.
  eHandled,
  // The native key event has not been handled by IMContextWrapper,
  // but eKeyDown or eKeyUp event has been dispatched.
  eNotHandledButEventDispatched,
  // The native key event has not been handled by IMContextWrapper,
  // but eKeyDown or eKeyUp event has been dispatched and consumed.
  eNotHandledButEventConsumed,
};

class IMContextWrapper final : public TextEventDispatcherListener {
 public:
  // TextEventDispatcherListener implementation
  NS_DECL_ISUPPORTS

  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) override;
  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;
  NS_IMETHOD_(void)
  OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) override;
  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher* aTextEventDispatcher,
                            WidgetKeyboardEvent& aKeyboardEvent,
                            uint32_t aIndexOfKeypress, void* aData) override;

 public:
  // aOwnerWindow is a pointer of the owner window.  When aOwnerWindow is
  // destroyed, the related IME contexts are released (i.e., IME cannot be
  // used with the instance after that).
  explicit IMContextWrapper(nsWindow* aOwnerWindow);

  // Called when the process is being shut down.
  static void Shutdown();

  // "Enabled" means the users can use all IMEs.
  // I.e., the focus is in the normal editors.
  bool IsEnabled() const;

  // OnFocusWindow is a notification that aWindow is going to be focused.
  void OnFocusWindow(nsWindow* aWindow);
  // OnBlurWindow is a notification that aWindow is going to be unfocused.
  void OnBlurWindow(nsWindow* aWindow);
  // OnDestroyWindow is a notification that aWindow is going to be destroyed.
  void OnDestroyWindow(nsWindow* aWindow);
  // OnFocusChangeInGecko is a notification that an editor gets focus.
  void OnFocusChangeInGecko(bool aFocus);
  // OnSelectionChange is a notification that selection (caret) is changed
  // in the focused editor.
  void OnSelectionChange(nsWindow* aCaller,
                         const IMENotification& aIMENotification);
  // OnThemeChanged is called when desktop theme is changed.
  static void OnThemeChanged();

  /**
   * OnKeyEvent() is called when aWindow gets a native key press event or a
   * native key release event.  If this returns true, the key event was
   * filtered by IME.  Otherwise, this returns false.
   * NOTE: When the native key press event starts composition, this returns
   *       true but dispatches an eKeyDown event or eKeyUp event before
   *       dispatching composition events or content command event.
   *
   * @param aWindow                       A window on which user operate the
   *                                      key.
   * @param aEvent                        A native key press or release
   *                                      event.
   * @param aKeyboardEventWasDispatched   true if eKeyDown or eKeyUp event
   *                                      for aEvent has already been
   *                                      dispatched.  In this case,
   *                                      this class doesn't dispatch
   *                                      keyboard event anymore.
   */
  KeyHandlingState OnKeyEvent(nsWindow* aWindow, GdkEvent* aEvent,
                              bool aKeyboardEventWasDispatched = false);

  // IME related nsIWidget methods.
  nsresult EndIMEComposition(nsWindow* aCaller);
  void SetInputContext(nsWindow* aCaller, const InputContext* aContext,
                       const InputContextAction* aAction);
  InputContext GetInputContext();
  void OnUpdateComposition();
  void OnLayoutChange();

  TextEventDispatcher* GetTextEventDispatcher();

  // TODO: Typically, new IM comes every several years.  And now, our code
  //       becomes really IM behavior dependent.  So, perhaps, we need prefs
  //       to control related flags for IM developers.
  enum class IMContextID : uint8_t {
    Fcitx,  // 4.x or earlier
    Fcitx5,
    IBus,
    IIIMF,
    Scim,
    Uim,
    Wayland,
    Unknown,
  };

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const IMContextID& aIMContextID) {
    switch (aIMContextID) {
      case IMContextID::Fcitx:
        return aStream << "Fcitx";
      case IMContextID::Fcitx5:
        return aStream << "Fcitx5";
      case IMContextID::IBus:
        return aStream << "IBus";
      case IMContextID::IIIMF:
        return aStream << "IIIMF";
      case IMContextID::Scim:
        return aStream << "Scim";
      case IMContextID::Uim:
        return aStream << "Uim";
      case IMContextID::Wayland:
        return aStream << "Wayland";
      case IMContextID::Unknown:
        return aStream << "Unknown";
    }
    MOZ_ASSERT_UNREACHABLE("Add new case for the new IM support");
    return aStream << "Unknown";
  }

  /**
   * GetIMName() returns IM name associated with mContext.  If the context is
   * xim, this look for actual engine from XMODIFIERS environment variable.
   */
  nsDependentCSubstring GetIMName() const;

  /**
   * GetWaitingSynthesizedKeyPressHardwareKeyCode() returns hardware_keycode
   * value of last handled GDK_KEY_PRESS event which is probable handled by
   * IME asynchronously and we have not received synthesized GDK_KEY_PRESS
   * event yet.
   */
  static guint16 GetWaitingSynthesizedKeyPressHardwareKeyCode() {
    return sWaitingSynthesizedKeyPressHardwareKeyCode;
  }

 protected:
  ~IMContextWrapper();

  /**
   * SetInputPurposeAndInputHints() sets input-purpose and input-hints of
   * current IM context to the values computed with mInputContext.
   */
  void SetInputPurposeAndInputHints();

  // Owner of an instance of this class. This should be top level window.
  // The owner window must release the contexts when it's destroyed because
  // the IME contexts need the native window.  If OnDestroyWindow() is called
  // with the owner window, it'll release IME contexts.  Otherwise, it'll
  // just clean up any existing composition if it's related to the destroying
  // child window.
  nsWindow* mOwnerWindow;

  // A last focused window in this class's context.
  nsWindow* mLastFocusedWindow;

  // Actual context. This is used for handling the user's input.
  GtkIMContext* mContext;

  // mSimpleContext is used for the password field and
  // the |ime-mode: disabled;| editors if sUseSimpleContext is true.
  // These editors disable IME.  But dead keys should work.  Fortunately,
  // the simple IM context of GTK2 support only them.
  GtkIMContext* mSimpleContext;

  // mDummyContext is a dummy context and will be used in Focus()
  // when the state of mEnabled means disabled.  This context's IME state is
  // always "closed", so it closes IME forcedly.
  GtkIMContext* mDummyContext;

  // mComposingContext is not nullptr while one of mContext, mSimpleContext
  // and mDummyContext has composition.
  // XXX: We don't assume that two or more context have composition same time.
  GtkIMContext* mComposingContext;

  // IME enabled state and other things defined in InputContext.
  // Use following helper methods if you don't need the detail of the status.
  InputContext mInputContext;

  // mCompositionStart is the start offset of the composition string in the
  // current content.  When <textarea> or <input> have focus, it means offset
  // from the first character of them.  When a HTML editor has focus, it
  // means offset from the first character of the root element of the editor.
  uint32_t mCompositionStart;

  // mDispatchedCompositionString is the latest composition string which
  // was dispatched by compositionupdate event.
  nsString mDispatchedCompositionString;

  // mSelectedStringRemovedByComposition is the selected string which was
  // removed by first compositionchange event.
  nsString mSelectedStringRemovedByComposition;

  // OnKeyEvent() temporarily sets mProcessingKeyEvent to the given native
  // event.
  GdkEvent* mProcessingKeyEvent;

  /**
   * GdkEventKeyQueue stores *copy* of GdkEventKey instances.  However, this
   * must be safe to our usecase since it has |time| and the value should not
   * be same as older event.
   */
  class GdkEventKeyQueue final {
   public:
    ~GdkEventKeyQueue() { Clear(); }

    void Clear() { mEvents.Clear(); }

    /**
     * PutEvent() puts new event into the queue.
     */
    void PutEvent(GdkEvent* aEvent) {
     // GdkKeyEvent* newEvent = reinterpret_cast<GdkKeyEvent*>(
      //    gdk_event_copy(reinterpret_cast<const GdkEvent*>(aEvent)));
      //newEvent->state &= GDK_MODIFIER_MASK;
      //mEvents.AppendElement(newEvent);
      mEvents.AppendElement(aEvent);
    }

    /**
     * RemoveEvent() removes oldest same event and its preceding events
     * from the queue.
     */
    void RemoveEvent(GdkEvent* aEvent) {
      size_t index = IndexOf(aEvent);
      if (NS_WARN_IF(index == GdkEventKeyQueue::NoIndex())) {
        return;
      }
      mEvents.RemoveElementAt(index);
      gdk_event_unref(GDK_EVENT(aEvent));
    }

    /**
     * Return corresponding GDK_KEY_PRESS event for aEvent.  aEvent must be a
     * GDK_KEY_RELEASE event.
     */
    GdkEvent* GetCorrespondingKeyPressEvent(
         GdkEvent* aEvent) const {
      //MOZ_ASSERT(aEvent->type == GDK_KEY_RELEASE);
      MOZ_ASSERT(gdk_event_get_event_type(GDK_EVENT(aEvent)) == GDK_KEY_RELEASE);
      //for (const GUniquePtr<GdkKeyEvent>& pendingKeyEvent : mEvents) {
      for (GdkEvent* pendingKeyEvent : mEvents) {
      //if (pendingKeyEvent->type == GDK_KEY_PRESS &&
        if (gdk_event_get_event_type(GDK_EVENT(pendingKeyEvent)) == GDK_KEY_PRESS &&
          //aEvent->hardware_keycode == pendingKeyEvent->hardware_keycode) {
            gdk_key_event_get_keycode(GDK_EVENT(aEvent)) == gdk_key_event_get_keycode(GDK_EVENT(pendingKeyEvent))) {
          return pendingKeyEvent;
        }
      }
      return nullptr;
    }

    /**
     * FirstEvent() returns oldest event in the queue.
     */
    GdkEvent* GetFirstEvent() const {
      if (mEvents.IsEmpty()) {
        return nullptr;
      }
      return mEvents[0];
    }

    bool IsEmpty() const { return mEvents.IsEmpty(); }

    static size_t NoIndex() { return nsTArray<GdkEvent*>::NoIndex; }
    size_t Length() const { return mEvents.Length(); }
    size_t IndexOf(GdkEvent* aEvent) const {
      static_assert(!(GDK_MODIFIER_MASK & (1 << 24)),
                    "We assumes 25th bit is used by some IM, but used by GDK");
      static_assert(!(GDK_MODIFIER_MASK & (1 << 25)),
                    "We assumes 26th bit is used by some IM, but used by GDK");
      for (size_t i = 0; i < mEvents.Length(); i++) {
        GdkEvent* event = mEvents[i];
        // It must be enough to compare only type, time, keyval and
        // part of state.   Note that we cannot compaire two events
        // simply since IME may have changed unused bits of state.
        if (gdk_event_get_time(GDK_EVENT(event)) == gdk_event_get_time(GDK_EVENT(aEvent))) {
          if (NS_WARN_IF(gdk_event_get_event_type(GDK_EVENT(event)) != gdk_event_get_event_type(GDK_EVENT(aEvent))) ||
              NS_WARN_IF(gdk_key_event_get_keyval(GDK_EVENT(event)) != gdk_key_event_get_keyval(GDK_EVENT(aEvent))) ||
              NS_WARN_IF(gdk_event_get_modifier_state(GDK_EVENT(event)) != (gdk_event_get_modifier_state(GDK_EVENT(aEvent)) & GDK_MODIFIER_MASK))) {
            continue;
          }
        }
        return i;
      }
      return GdkEventKeyQueue::NoIndex();
    }

   private:
    nsTArray<GdkEvent*> mEvents;
  };
  // OnKeyEvent() append mPostingKeyEvents when it believes that a key event
  // is posted to other IME process.
  GdkEventKeyQueue mPostingKeyEvents;

  static guint16 sWaitingSynthesizedKeyPressHardwareKeyCode;

  struct Range {
    uint32_t mOffset;
    uint32_t mLength;

    Range() : mOffset(UINT32_MAX), mLength(UINT32_MAX) {}

    bool IsValid() const { return mOffset != UINT32_MAX; }
    void Clear() {
      mOffset = UINT32_MAX;
      mLength = UINT32_MAX;
    }
  };

  // current target offset and length of IME composition
  Range mCompositionTargetRange;

  // mCompositionState indicates current status of composition.
  enum eCompositionState : uint8_t {
    eCompositionState_NotComposing,
    eCompositionState_CompositionStartDispatched,
    eCompositionState_CompositionChangeEventDispatched
  };
  eCompositionState mCompositionState;

  bool IsComposing() const {
    return (mCompositionState != eCompositionState_NotComposing);
  }

  bool IsComposingOn(GtkIMContext* aContext) const {
    return IsComposing() && mComposingContext == aContext;
  }

  bool IsComposingOnCurrentContext() const {
    return IsComposingOn(GetCurrentContext());
  }

  bool EditorHasCompositionString() {
    return (mCompositionState ==
            eCompositionState_CompositionChangeEventDispatched);
  }

  /**
   * Checks if aContext is valid context for handling composition.
   *
   * @param aContext          An IM context which is specified by native
   *                          composition events.
   * @return                  true if the context is valid context for
   *                          handling composition.  Otherwise, false.
   */
  bool IsValidContext(GtkIMContext* aContext) const;

  const char* GetCompositionStateName() {
    switch (mCompositionState) {
      case eCompositionState_NotComposing:
        return "NotComposing";
      case eCompositionState_CompositionStartDispatched:
        return "CompositionStartDispatched";
      case eCompositionState_CompositionChangeEventDispatched:
        return "CompositionChangeEventDispatched";
      default:
        return "InvaildState";
    }
  }

  // mIMContextID indicates the ID of mContext.  This is actually indicates
  // IM which user selected.
  IMContextID mIMContextID;

  // If mContentSelection is Nothing, it means that
  // EnsureToCacheContentSelection failed to get selection or just not caching
  // the selection.
  Maybe<ContentSelection> mContentSelection;

  /**
   * Return true if mContentSelection is set to some.  Otherwise, false.
   */
  bool EnsureToCacheContentSelection(nsAString* aSelectedString = nullptr);

  enum class IMEFocusState : uint8_t {
    // IME has focus
    Focused,
    // IME was blurred
    Blurred,
    // IME was blurred without a focus change
    BlurredWithoutFocusChange,
  };
  friend std::ostream& operator<<(std::ostream& aStream, IMEFocusState aState) {
    switch (aState) {
      case IMEFocusState::Focused:
        return aStream << "IMEFocusState::Focused";
      case IMEFocusState::Blurred:
        return aStream << "IMEFocusState::Blurred";
      case IMEFocusState::BlurredWithoutFocusChange:
        return aStream << "IMEFocusState::BlurredWithoutFocusChange";
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid value");
        return aStream << "<illegal value>";
    }
  }
  IMEFocusState mIMEFocusState = IMEFocusState::Blurred;

  // mFallbackToKeyEvent is set to false when this class starts to handle
  // a native key event (at that time, mProcessingKeyEvent is set to the
  // native event).  If active IME just commits composition with a character
  // which is produced by the key with current keyboard layout, this is set
  // to true.
  bool mFallbackToKeyEvent;
  // mKeyboardEventWasDispatched is used by OnKeyEvent() and
  // MaybeDispatchKeyEventAsProcessedByIME().
  // MaybeDispatchKeyEventAsProcessedByIME() dispatches an eKeyDown or
  // eKeyUp event event if the composition is caused by a native
  // key press event.  If this is true, a keyboard event has been dispatched
  // for the native event.  If so, MaybeDispatchKeyEventAsProcessedByIME()
  // won't dispatch keyboard event anymore.
  bool mKeyboardEventWasDispatched;
  // Whether the keyboard event which as dispatched at setting
  // mKeyboardEventWasDispatched to true was consumed or not.
  bool mKeyboardEventWasConsumed;
  // mIsDeletingSurrounding is true while OnDeleteSurroundingNative() is
  // trying to delete the surrounding text.
  bool mIsDeletingSurrounding;
  // mLayoutChanged is true after OnLayoutChange() is called.  This is reset
  // when eCompositionChange is being dispatched.
  bool mLayoutChanged;
  // mSetCursorPositionOnKeyEvent true when caret rect or position is updated
  // with no composition.  If true, we update candidate window position
  // before key down
  bool mSetCursorPositionOnKeyEvent;
  // mPendingResettingIMContext becomes true if selection change notification
  // is received during composition but the selection change occurred before
  // starting the composition.  In such case, we cannot notify IME of
  // selection change during composition because we don't want to commit
  // the composition in such case.  However, we should notify IME of the
  // selection change after the composition is committed.
  bool mPendingResettingIMContext;
  // mRetrieveSurroundingSignalReceived is true after "retrieve_surrounding"
  // signal is received until selection is changed in Gecko.
  bool mRetrieveSurroundingSignalReceived;
  // mMaybeInDeadKeySequence is set to true when we detect a dead key press
  // and set to false when we're sure dead key sequence has been finished.
  // Note that we cannot detect which key event causes ending a dead key
  // sequence.  For example, when you press dead key grave with ibus Spanish
  // keyboard layout, it just consumes the key event when we call
  // gtk_im_context_filter_keypress().  Then, pressing "Escape" key cancels
  // the dead key sequence but we don't receive any signal and it's consumed
  // by gtk_im_context_filter_keypress() normally.  On the other hand, when
  // pressing "Shift" key causes exactly same behavior but dead key sequence
  // isn't finished yet.
  bool mMaybeInDeadKeySequence;
  // mIsIMInAsyncKeyHandlingMode is set to true if we know that IM handles
  // key events asynchronously.  I.e., filtered key event may come again
  // later.
  bool mIsIMInAsyncKeyHandlingMode;
  // mIsKeySnooped is set to true if IM uses key snooper to listen key events.
  // In such case, we won't receive key events if IME consumes the event.
  bool mIsKeySnooped;
  // mSetInputPurposeAndInputHints is set if `SetInputContext` wants `Focus`
  // to set input-purpose and input-hints.
  bool mSetInputPurposeAndInputHints;

  // sLastFocusedContext is a pointer to the last focused instance of this
  // class.  When a instance is destroyed and sLastFocusedContext refers it,
  // this is cleared.  So, this refers valid pointer always.
  static IMContextWrapper* sLastFocusedContext;

  // sUseSimpleContext indeicates if password editors and editors with
  // |ime-mode: disabled;| should use GtkIMContextSimple.
  // If true, they use GtkIMContextSimple.  Otherwise, not.
  static bool sUseSimpleContext;

  // Callback methods for native IME events.  These methods should call
  // the related instance methods simply.
  static gboolean OnRetrieveSurroundingCallback(GtkIMContext* aContext,
                                                IMContextWrapper* aModule);
  static gboolean OnDeleteSurroundingCallback(GtkIMContext* aContext,
                                              gint aOffset, gint aNChars,
                                              IMContextWrapper* aModule);
  static void OnCommitCompositionCallback(GtkIMContext* aContext,
                                          const gchar* aString,
                                          IMContextWrapper* aModule);
  static void OnChangeCompositionCallback(GtkIMContext* aContext,
                                          IMContextWrapper* aModule);
  static void OnStartCompositionCallback(GtkIMContext* aContext,
                                         IMContextWrapper* aModule);
  static void OnEndCompositionCallback(GtkIMContext* aContext,
                                       IMContextWrapper* aModule);

  // The instance methods for the native IME events.
  gboolean OnRetrieveSurroundingNative(GtkIMContext* aContext);
  gboolean OnDeleteSurroundingNative(GtkIMContext* aContext, gint aOffset,
                                     gint aNChars);
  void OnCommitCompositionNative(GtkIMContext* aContext, const gchar* aString);
  void OnChangeCompositionNative(GtkIMContext* aContext);
  void OnStartCompositionNative(GtkIMContext* aContext);
  void OnEndCompositionNative(GtkIMContext* aContext);

  /**
   * GetCurrentContext() returns current IM context which is chosen with the
   * enabled state.
   * WARNING:
   *     When this class receives some signals for a composition after focus
   *     is moved in Gecko, the result of this may be different from given
   *     context by the signals.
   */
  GtkIMContext* GetCurrentContext() const;

  /**
   * GetActiveContext() returns a composing context or current context.
   */
  GtkIMContext* GetActiveContext() const {
    return mComposingContext ? mComposingContext : GetCurrentContext();
  }

  // If the owner window and IM context have been destroyed, returns TRUE.
  bool IsDestroyed() { return !mOwnerWindow; }

  void NotifyIMEOfFocusChange(IMEFocusState aIMEFocusState);

  // Initializes the instance.
  void Init();

  /**
   * Reset the active context, i.e., if there is mComposingContext, reset it.
   * Otherwise, reset current context.  Note that all native composition
   * events during calling this will be ignored.
   */
  void ResetIME();

  // Gets the current composition string by the native APIs.
  void GetCompositionString(GtkIMContext* aContext,
                            nsAString& aCompositionString);

  /**
   * Generates our text range array from current composition string.
   *
   * @param aContext              A GtkIMContext which is being handled.
   * @param aCompositionString    The data to be dispatched with
   *                              compositionchange event.
   */
  already_AddRefed<TextRangeArray> CreateTextRangeArray(
      GtkIMContext* aContext, const nsAString& aCompositionString);

  /**
   * SetTextRange() initializes aTextRange with aPangoAttrIter.
   *
   * @param aPangoAttrIter            An iter which represents a clause of the
   *                                  composition string.
   * @param aUTF8CompositionString    The whole composition string (UTF-8).
   * @param aUTF16CaretOffset         The caret offset in the composition
   *                                  string encoded as UTF-16.
   * @param aTextRange                The result.
   * @return                          true if this initializes aTextRange.
   *                                  Otherwise, false.
   */
  bool SetTextRange(PangoAttrIterator* aPangoAttrIter,
                    const gchar* aUTF8CompositionString,
                    uint32_t aUTF16CaretOffset, TextRange& aTextRange) const;

  /**
   * ToNscolor() converts the PangoColor in aPangoAttrColor to nscolor.
   */
  static nscolor ToNscolor(PangoAttrColor* aPangoAttrColor);

  /**
   * Move the candidate window with "fake" cursor position.
   *
   * @param aContext              A GtkIMContext which is being handled.
   */
  void SetCursorPosition(GtkIMContext* aContext);

  // Queries the current selection offset of the window.
  uint32_t GetSelectionOffset(nsWindow* aWindow);

  // Get current paragraph text content and cursor position
  nsresult GetCurrentParagraph(nsAString& aText, uint32_t& aCursorPos);

  /**
   * Delete text portion
   *
   * @param aContext              A GtkIMContext which is being handled.
   * @param aOffset               Start offset of the range to delete.
   * @param aNChars               Count of characters to delete.  It depends
   *                              on |g_utf8_strlen()| what is one character.
   */
  nsresult DeleteText(GtkIMContext* aContext, int32_t aOffset,
                      uint32_t aNChars);

  // Called before destroying the context to work around some platform bugs.
  void PrepareToDestroyContext(GtkIMContext* aContext);

  /**
   *  WARNING:
   *    Following methods dispatch gecko events.  Then, the focused widget
   *    can be destroyed, and also it can be stolen focus.  If they returns
   *    FALSE, callers cannot continue the composition.
   *      - MaybeDispatchKeyEventAsProcessedByIME
   *      - DispatchCompositionStart
   *      - DispatchCompositionChangeEvent
   *      - DispatchCompositionCommitEvent
   */

  /**
   * Dispatch an eKeyDown or eKeyUp event whose mKeyCode value is
   * NS_VK_PROCESSKEY and mKeyNameIndex is KEY_NAME_INDEX_Process if
   * we're not in a dead key sequence, mProcessingKeyEvent is nullptr
   * but mPostingKeyEvents is not empty or mProcessingKeyEvent is not
   * nullptr and mKeyboardEventWasDispatched is still false.  If this
   * dispatches a keyboard event, this sets mKeyboardEventWasDispatched
   * to true.
   *
   * @param aFollowingEvent       The following event message.
   * @return                      If the caller can continue to handle
   *                              composition, returns true.  Otherwise,
   *                              false.  For example, if focus is moved
   *                              by dispatched keyboard event, returns
   *                              false.
   */
  bool MaybeDispatchKeyEventAsProcessedByIME(EventMessage aFollowingEvent);

  /**
   * Dispatches a composition start event.
   *
   * @param aContext              A GtkIMContext which is being handled.
   * @return                      true if the focused widget is neither
   *                              destroyed nor changed.  Otherwise, false.
   */
  bool DispatchCompositionStart(GtkIMContext* aContext);

  /**
   * Dispatches a compositionchange event.
   *
   * @param aContext              A GtkIMContext which is being handled.
   * @param aCompositionString    New composition string.
   * @return                      true if the focused widget is neither
   *                              destroyed nor changed.  Otherwise, false.
   */
  bool DispatchCompositionChangeEvent(GtkIMContext* aContext,
                                      const nsAString& aCompositionString);

  /**
   * Dispatches a compositioncommit event or compositioncommitasis event.
   *
   * @param aContext              A GtkIMContext which is being handled.
   * @param aCommitString         If this is nullptr, the composition will
   *                              be committed with last dispatched data.
   *                              Otherwise, the composition will be
   *                              committed with this value.
   * @return                      true if the focused widget is neither
   *                              destroyed nor changed.  Otherwise, false.
   */
  bool DispatchCompositionCommitEvent(GtkIMContext* aContext,
                                      const nsAString* aCommitString = nullptr);
};

}  // namespace widget
}  // namespace mozilla

#endif  // #ifndef IMContextWrapper_h_
