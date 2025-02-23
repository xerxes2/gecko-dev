/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncGtkClipboardRequest.h"

namespace mozilla {

AsyncGtkClipboardRequest::AsyncGtkClipboardRequest(ClipboardDataType aDataType,
                                                   int32_t aWhichClipboard,
                                                   const char* aMimeType) {
  GdkClipboard* clipboard = nullptr;
  if (GetSelectionInt(aWhichClipboard)) {
    clipboard = gdk_display_get_primary_clipboard(gdk_display_get_default());
  } else {
    clipboard = gdk_display_get_clipboard(gdk_display_get_default());
  }

  mRequest = MakeUnique<Request>(aDataType);

  switch (aDataType) {
    case ClipboardDataType::Data:
      MOZ_CLIPBOARD_LOG("  getting DATA MIME %s\n", aMimeType);
      //gtk_clipboard_request_contents(clipboard,
      //                               gdk_atom_intern(aMimeType, FALSE),
      //                               OnDataReceived, mRequest.get());
      gdk_clipboard_read_async(clipboard, &aMimeType, G_PRIORITY_DEFAULT, nullptr,
                               OnDataReceived, mRequest.get());
      break;
    case ClipboardDataType::Text:
      MOZ_CLIPBOARD_LOG("  getting TEXT\n");
      //gtk_clipboard_request_text(clipboard, OnTextReceived, mRequest.get());
      gdk_clipboard_read_text_async(clipboard, nullptr, OnTextReceived,
                                    mRequest.get());
      break;
    case ClipboardDataType::Targets:
      MOZ_CLIPBOARD_LOG("  getting TARGETS\n");
      //gtk_clipboard_request_contents(clipboard,
      //                               gdk_atom_intern("TARGETS", FALSE),
      //                               OnDataReceived, mRequest.get());
      break;
  }
}

void AsyncGtkClipboardRequest::OnDataReceived(GObject* clipboard,
                                              GAsyncResult* aResult,
                                              gpointer data) {
  auto whichClipboard = GetGeckoClipboardType(GDK_CLIPBOARD(clipboard));
  MOZ_CLIPBOARD_LOG("OnDataReceived(%s) callback\n",
                    whichClipboard == Some(nsClipboard::kSelectionClipboard)
                        ? "primary"
                        : "clipboard");
  static_cast<Request*>(data)->Complete(aResult);
}

void AsyncGtkClipboardRequest::OnTextReceived(GObject* clipboard,
                                              GAsyncResult* aResult,
                                              gpointer data) {
  auto whichClipboard = GetGeckoClipboardType(GDK_CLIPBOARD(clipboard));
  MOZ_CLIPBOARD_LOG("OnTextReceived(%s) callback\n",
                    whichClipboard == Some(nsClipboard::kSelectionClipboard)
                        ? "primary"
                        : "clipboard");
  char* aData = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(clipboard), aResult, nullptr);

  static_cast<Request*>(data)->Complete(aData);
}

void AsyncGtkClipboardRequest::Request::Complete(const void* aData) {
  MOZ_CLIPBOARD_LOG("Request::Complete(), aData = %p, timedOut = %d\n", aData,
                    mTimedOut);

  if (mTimedOut) {
    delete this;
    return;
  }

  mData.emplace();

  gint dataLength = 0;
  if (mDataType == ClipboardDataType::Targets ||
      mDataType == ClipboardDataType::Data) {
   // dataLength = gtk_selection_data_get_length((GtkSelectionData*)aData);
  } else {
    dataLength = aData ? strlen((const char*)aData) : 0;
  }

  // Negative size means no data or data error.
  if (dataLength <= 0) {
    MOZ_CLIPBOARD_LOG("    zero dataLength, quit.\n");
    return;
  }

  switch (mDataType) {
    case ClipboardDataType::Targets: {
      MOZ_CLIPBOARD_LOG("    getting %d bytes of clipboard targets.\n",
                        dataLength);
      gint n_targets = 0;
      //GdkAtom* targets = nullptr;
      //if (!gtk_selection_data_get_targets((GtkSelectionData*)aData, &targets,
      //                                    &n_targets) ||
      //    !n_targets) {
        // We failed to get targets
      //  return;
      //}
      //mData->SetTargets(
      //    ClipboardTargets{GUniquePtr<GdkAtom>(targets), uint32_t(n_targets)});
      break;
    }
    case ClipboardDataType::Text: {
      MOZ_CLIPBOARD_LOG("    getting %d bytes of text.\n", dataLength);
      mData->SetText(Span(static_cast<const char*>(aData), dataLength));
      MOZ_CLIPBOARD_LOG("    done, mClipboardData = %p\n",
                        mData->AsSpan().data());
      break;
    }
    case ClipboardDataType::Data: {
      MOZ_CLIPBOARD_LOG("    getting %d bytes of data.\n", dataLength);
   //   mData->SetData(Span(gtk_selection_data_get_data((GtkSelectionData*)aData),
   //                       dataLength));
      MOZ_CLIPBOARD_LOG("    done, mClipboardData = %p\n",
                        mData->AsSpan().data());
      break;
    }
  }
}

AsyncGtkClipboardRequest::~AsyncGtkClipboardRequest() {
  if (mRequest && mRequest->mData.isNothing()) {
    mRequest->mTimedOut = true;
    Unused << mRequest.release();
  }
}

}  // namespace mozilla
