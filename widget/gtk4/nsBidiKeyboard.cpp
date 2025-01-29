/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlink.h"

#include "nsBidiKeyboard.h"
#include "nsIWidget.h"
#include <gtk/gtk.h>

NS_IMPL_ISUPPORTS(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard() { Reset(); }

NS_IMETHODIMP
nsBidiKeyboard::Reset() {
  // NB: The default keymap can be null (e.g. in xpcshell). In that case,
  // simply assume that we don't have bidi keyboards.
  mHaveBidiKeyboards = false;

  GdkDisplay* display = gdk_display_get_default();
  if (!display) return NS_OK;

  GdkSeat* seat = gdk_display_get_default_seat(display);
  if (!seat) return NS_OK;

  GdkDevice* keyboard = gdk_seat_get_keyboard(seat);
  mHaveBidiKeyboards = keyboard && gdk_device_has_bidi_layouts(keyboard);
  return NS_OK;
}

nsBidiKeyboard::~nsBidiKeyboard() = default;

NS_IMETHODIMP
nsBidiKeyboard::IsLangRTL(bool* aIsRTL) {
  if (!mHaveBidiKeyboards) return NS_ERROR_FAILURE;

  GdkDisplay* display = gdk_display_get_default();
  if (!display) return NS_ERROR_FAILURE;

  GdkSeat* seat = gdk_display_get_default_seat(display);
  if (!seat) return NS_ERROR_FAILURE;

  GdkDevice* keyboard = gdk_seat_get_keyboard(seat);

  *aIsRTL = (gdk_device_get_direction(keyboard) ==
             PANGO_DIRECTION_RTL);

  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::GetHaveBidiKeyboards(bool* aResult) {
  // not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}

// static
already_AddRefed<nsIBidiKeyboard> nsIWidget::CreateBidiKeyboardInner() {
  return do_AddRef(new nsBidiKeyboard());
}
