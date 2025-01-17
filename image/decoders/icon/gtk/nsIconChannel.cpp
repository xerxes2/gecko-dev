/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconChannel.h"

#include <stdlib.h>
#include <unistd.h>

#include "mozilla/DebugOnly.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/ipc/ByteBuf.h"
#include <algorithm>

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include <gtk/gtk.h>

#include "nsMimeTypes.h"
#include "nsIMIMEService.h"

#include "nsServiceManagerUtils.h"

#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsIStringStream.h"
#include "nsServiceManagerUtils.h"
#include "nsIURL.h"
#include "nsIPipe.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "prlink.h"
#include "gfxPlatform.h"

using mozilla::CheckedInt32;
using mozilla::ipc::ByteBuf;

NS_IMPL_ISUPPORTS(nsIconChannel, nsIRequest, nsIChannel)

static nsresult MozGdkPixbufToByteBuf(GdkPixbuf* aPixbuf, ByteBuf* aByteBuf) {
  int width = gdk_pixbuf_get_width(aPixbuf);
  int height = gdk_pixbuf_get_height(aPixbuf);
  NS_ENSURE_TRUE(height < 256 && width < 256 && height > 0 && width > 0 &&
                     gdk_pixbuf_get_colorspace(aPixbuf) == GDK_COLORSPACE_RGB &&
                     gdk_pixbuf_get_bits_per_sample(aPixbuf) == 8 &&
                     gdk_pixbuf_get_has_alpha(aPixbuf) &&
                     gdk_pixbuf_get_n_channels(aPixbuf) == 4,
                 NS_ERROR_UNEXPECTED);

  const int n_channels = 4;
  CheckedInt32 buf_size =
      4 + n_channels * CheckedInt32(height) * CheckedInt32(width);
  if (!buf_size.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint8_t* const buf = (uint8_t*)moz_xmalloc(buf_size.value());
  uint8_t* out = buf;

  *(out++) = width;
  *(out++) = height;
  *(out++) = uint8_t(mozilla::gfx::SurfaceFormat::OS_RGBA);

  // Set all bits to ensure in nsIconDecoder we color manage and premultiply.
  *(out++) = 0xFF;

  const guchar* const pixels = gdk_pixbuf_get_pixels(aPixbuf);
  int instride = gdk_pixbuf_get_rowstride(aPixbuf);
  int outstride = width * n_channels;

  // encode the RGB data and the A data and adjust the stride as necessary.
  mozilla::gfx::SwizzleData(pixels, instride,
                            mozilla::gfx::SurfaceFormat::R8G8B8A8, out,
                            outstride, mozilla::gfx::SurfaceFormat::OS_RGBA,
                            mozilla::gfx::IntSize(width, height));

  *aByteBuf = ByteBuf(buf, buf_size.value(), buf_size.value());
  return NS_OK;
}

static nsresult ByteBufToStream(ByteBuf&& aBuf, nsIInputStream** aStream) {
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream =
      do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // stream takes ownership of buf and will free it on destruction.
  // This function cannot fail.
  rv = stream->AdoptData(reinterpret_cast<char*>(aBuf.mData), aBuf.mLen);
  MOZ_ASSERT(CheckedInt32(aBuf.mLen).isValid(),
             "aBuf.mLen should fit in int32_t");
  aBuf.mData = nullptr;

  // If this no longer holds then re-examine buf's lifetime.
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(aStream);
  return NS_OK;
}

static nsresult StreamToChannel(already_AddRefed<nsIInputStream> aStream,
                                nsIURI* aURI, nsIChannel** aChannel) {
  // nsIconProtocolHandler::NewChannel will provide the correct loadInfo for
  // this iconChannel. Use the most restrictive security settings for the
  // temporary loadInfo to make sure the channel can not be opened.
  nsCOMPtr<nsIPrincipal> nullPrincipal =
      mozilla::NullPrincipal::CreateWithoutOriginAttributes();
  return NS_NewInputStreamChannel(
      aChannel, aURI, std::move(aStream), nullPrincipal,
      nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
      nsIContentPolicy::TYPE_INTERNAL_IMAGE, nsLiteralCString(IMAGE_ICON_MS));
}

// This widget is only used for getting the scale factor and
// could possibly be removed later.
static GtkWidget* gProtoWidget = nullptr;

static void ensure_stock_image_widget() {
  if (!gProtoWidget) {
    gProtoWidget = gtk_image_new();
  }
}

static uint32_t moz_gtk_icon_size(const char* name) {
  if (strcmp(name, "toolbar") == 0) {
    return 24;
  }
  if (strcmp(name, "dnd") == 0) {
    return 32;
  }
  if (strcmp(name, "dialog") == 0) {
    return 48;
  }
  return 16;
}

static int32_t GetIconSize(nsIMozIconURI* aIconURI) {
  nsAutoCString iconSizeString;

  aIconURI->GetIconSize(iconSizeString);
  if (iconSizeString.IsEmpty()) {
    uint32_t size;
    mozilla::DebugOnly<nsresult> rv = aIconURI->GetImageSize(&size);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetImageSize failed");
    return size;
  }

  uint32_t size = moz_gtk_icon_size(iconSizeString.get());
  return size;
}

/* static */
nsresult nsIconChannel::GetIconWithGIO(nsIMozIconURI* aIconURI,
                                       ByteBuf* aDataOut) {
  GIcon* icon = nullptr;
  nsCOMPtr<nsIURL> fileURI;

  // Read icon content
  aIconURI->GetIconURL(getter_AddRefs(fileURI));

  // Get icon for file specified by URI
  if (fileURI) {
    nsAutoCString spec;
    fileURI->GetAsciiSpec(spec);
    if (fileURI->SchemeIs("file")) {
      GFile* file = g_file_new_for_uri(spec.get());
      GFileInfo* fileInfo =
          g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON,
                            G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
      g_object_unref(file);
      if (fileInfo) {
        // icon from g_content_type_get_icon doesn't need unref
        icon = g_file_info_get_icon(fileInfo);
        if (icon) {
          g_object_ref(icon);
        }
        g_object_unref(fileInfo);
      }
    }
  } else {
    // From the moz-icon://appId?size=... get the appId
    nsAutoCString appId;
    aIconURI->GetAsciiSpec(appId);

    if (appId.Find("?size=")) {
      appId.Truncate(appId.Find("?size="));
    }

    appId = Substring(appId, sizeof("moz-icon:/"));

    GDesktopAppInfo* app_info = g_desktop_app_info_new(appId.get());
    if (app_info) {
      icon = g_app_info_get_icon((GAppInfo*)app_info);
      if (icon) {
        g_object_ref(icon);
      }
      g_object_unref(app_info);
    }
  }

  // Try to get icon by using MIME type
  if (!icon) {
    nsAutoCString type;
    aIconURI->GetContentType(type);
    // Try to get MIME type from file extension by using nsIMIMEService
    if (type.IsEmpty()) {
      nsCOMPtr<nsIMIMEService> ms(do_GetService("@mozilla.org/mime;1"));
      if (ms) {
        nsAutoCString fileExt;
        aIconURI->GetFileExtension(fileExt);
        ms->GetTypeFromExtension(fileExt, type);
      }
    }
    char* ctype = nullptr;  // character representation of content type
    if (!type.IsEmpty()) {
      ctype = g_content_type_from_mime_type(type.get());
    }
    if (ctype) {
      icon = g_content_type_get_icon(ctype);
      g_free(ctype);
    }
  }

  GtkTextDirection direction = gtk_widget_get_default_direction();
  #ifdef MOZ_GTK4
  GdkDisplay* display = gdk_display_get_default();
  GtkIconTheme* iconTheme = gtk_icon_theme_get_for_display(display);
  GtkIconPaintable* iconPaint = nullptr;
  #else
  GtkIconTheme* iconTheme = gtk_icon_theme_get_default();
  GtkIconInfo* iconInfo = nullptr;
  GtkIconLookupFlags iconFlags;
  if (direction == GTK_TEXT_DIR_LTR) {
    iconFlags = GTK_ICON_LOOKUP_DIR_LTR;
  } else {
    iconFlags = GTK_ICON_LOOKUP_DIR_RTL;
  }
  #endif

  ensure_stock_image_widget();
  uint32_t scaleFactor = gtk_widget_get_scale_factor(gProtoWidget);
  uint32_t iconSize = GetIconSize(aIconURI);
  uint32_t iconSizeBuf = iconSize * scaleFactor;
  bool iconExists = false;

  if (icon) {
    // Use the GIcon to find an icon.
    #ifdef MOZ_GTK4
    iconPaint = gtk_icon_theme_lookup_by_gicon(iconTheme, icon, iconSize,
      scaleFactor, direction, (GtkIconLookupFlags)0);
    const char* name = gtk_icon_paintable_get_icon_name(iconPaint);
    if (name) {
      iconExists = true;
    }
    #else
    iconInfo = gtk_icon_theme_lookup_by_gicon_for_scale(iconTheme, icon,
      iconSize, scaleFactor, iconFlags);
    if (iconInfo) {
      iconExists = true;
    }
    #endif
    g_object_unref(icon);
  }

  if (!iconExists) {
    // Mozilla's mimetype lookup failed. Try the "unknown" icon.
    #ifdef MOZ_GTK4
    iconPaint = gtk_icon_theme_lookup_icon(iconTheme, "unknown", NULL,
      iconSize, scaleFactor, direction, (GtkIconLookupFlags)0);
    const char* name = gtk_icon_paintable_get_icon_name(iconPaint);
    if (name) {
      iconExists = true;
    } else {
      g_object_unref(iconPaint);
    }
    #else
    iconInfo = gtk_icon_theme_lookup_icon_for_scale(iconTheme, "unknown",
      iconSize, scaleFactor, iconFlags);
    if (iconInfo) {
      iconExists = true;
    }
    #endif
    if (!iconExists) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  #ifdef MOZ_GTK4
  GFile* gFile = gtk_icon_paintable_get_file(iconPaint);
  char* path = g_file_get_path(gFile);
  #else
  const gchar* path = gtk_icon_info_get_filename(iconInfo);
  #endif

  GdkPixbuf* iconBuf = gdk_pixbuf_new_from_file_at_size(path, iconSizeBuf,
    iconSizeBuf, nullptr);

  #ifdef MOZ_GTK4
  g_object_unref(iconPaint);
  if (gFile) {
    g_object_unref(gFile);
    g_free(path);
  }
  #else
  g_object_unref(iconInfo);
  #endif

  if (!iconBuf) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = MozGdkPixbufToByteBuf(iconBuf, aDataOut);
  NS_ENSURE_SUCCESS(rv, rv);
  g_object_unref(iconBuf);
  return rv;
}

/* static */
nsresult nsIconChannel::GetIcon(nsIURI* aURI, ByteBuf* aDataOut) {
  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  if (!iconURI) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (gfxPlatform::IsHeadless()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString stockIcon;
  iconURI->GetStockIcon(stockIcon);
  if (stockIcon.IsEmpty()) {
    return GetIconWithGIO(iconURI, aDataOut);
  }

  // Search for stockIcon
  nsAutoCString iconSizeString;
  iconURI->GetIconSize(iconSizeString);

  ensure_stock_image_widget();
  uint32_t scaleFactor = gtk_widget_get_scale_factor(gProtoWidget);
  uint32_t iconSize = moz_gtk_icon_size(iconSizeString.get());
  uint32_t iconSizeBuf = iconSize * scaleFactor;
  bool iconExists = false;

  #ifdef MOZ_GTK4
  GdkDisplay* display = gdk_display_get_default();
  GtkIconTheme* iconTheme = gtk_icon_theme_get_for_display(display);
  #else
  GtkIconTheme* iconTheme = gtk_icon_theme_get_default();
  #endif

  GtkTextDirection direction;
  if (StringEndsWith(stockIcon, "-ltr"_ns)) {
    direction = GTK_TEXT_DIR_LTR;
    stockIcon.Truncate(stockIcon.Length() - 4);
  } else if (StringEndsWith(stockIcon, "-rtl"_ns)) {
    direction = GTK_TEXT_DIR_RTL;
    stockIcon.Truncate(stockIcon.Length() - 4);
  } else {
    direction = gtk_widget_get_default_direction();
  }

  #ifdef MOZ_GTK4
  GtkIconPaintable* iconPaint = gtk_icon_theme_lookup_icon(iconTheme,
    stockIcon.get(), NULL, iconSize, scaleFactor, direction,
    GTK_ICON_LOOKUP_FORCE_SYMBOLIC);
  GFile* gFile = gtk_icon_paintable_get_file(iconPaint);
  char* path;
  if (gFile) {
    path = g_file_get_path(gFile);
    iconExists = true;
  }
  #else
  GtkIconLookupFlags iconFlags;
  if (direction == GTK_TEXT_DIR_LTR) {
    iconFlags = GtkIconLookupFlags(GTK_ICON_LOOKUP_DIR_LTR |
      GTK_ICON_LOOKUP_FORCE_SYMBOLIC);
  } else {
    iconFlags = GtkIconLookupFlags(GTK_ICON_LOOKUP_DIR_RTL |
      GTK_ICON_LOOKUP_FORCE_SYMBOLIC);
  }
  GtkIconInfo* iconInfo = gtk_icon_theme_lookup_icon_for_scale(iconTheme,
    stockIcon.get(), iconSize, scaleFactor, iconFlags);
  const gchar* path;
  if (iconInfo) {
    path = gtk_icon_info_get_filename(iconInfo);
    iconExists = true;
  }
  #endif

  GdkPixbuf* iconBuf = nullptr;
  if (iconExists) {
    iconBuf = gdk_pixbuf_new_from_file_at_size(path, iconSizeBuf, iconSizeBuf,
      nullptr);
    if (!iconBuf) {
      iconExists = false;
    }
  }

  #ifdef MOZ_GTK4
  g_object_unref(iconPaint);
  if (gFile) {
    g_object_unref(gFile);
    g_free(path);
  }
  #else
  if (iconInfo) {
    g_object_unref(iconInfo);
  }
  #endif

  if (!iconExists) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = MozGdkPixbufToByteBuf(iconBuf, aDataOut);
  g_object_unref(iconBuf);
  return rv;
}

nsresult nsIconChannel::Init(nsIURI* aURI) {
  nsCOMPtr<nsIInputStream> stream;

  using ContentChild = mozilla::dom::ContentChild;
  if (auto* contentChild = ContentChild::GetSingleton()) {
    // Get the icon via IPC and translate the promise of a ByteBuf
    // into an actually-existing channel.
    RefPtr<ContentChild::GetSystemIconPromise> icon =
        contentChild->SendGetSystemIcon(aURI);
    if (!icon) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIAsyncInputStream> inputStream;
    nsCOMPtr<nsIAsyncOutputStream> outputStream;
    NS_NewPipe2(getter_AddRefs(inputStream), getter_AddRefs(outputStream), true,
                false, 0, UINT32_MAX);

    // FIXME: Bug 1718324
    // The GetSystemIcon() call will end up on the parent doing GetIcon()
    // and by using ByteBuf we might not be immune to some deadlock, at least
    // on paper. From analysis in
    // https://phabricator.services.mozilla.com/D118596#3865440 we should be
    // safe in practice, but it would be nicer to just write that differently.

    icon->Then(
        mozilla::GetCurrentSerialEventTarget(), __func__,
        [outputStream](std::tuple<nsresult, mozilla::Maybe<ByteBuf>>&& aArg) {
          nsresult rv = std::get<0>(aArg);
          mozilla::Maybe<ByteBuf> bytes = std::move(std::get<1>(aArg));

          if (NS_SUCCEEDED(rv)) {
            MOZ_RELEASE_ASSERT(bytes);
            uint32_t written;
            rv = outputStream->Write(reinterpret_cast<char*>(bytes->mData),
                                     static_cast<uint32_t>(bytes->mLen),
                                     &written);
            if (NS_SUCCEEDED(rv)) {
              const bool wroteAll = static_cast<size_t>(written) == bytes->mLen;
              MOZ_ASSERT(wroteAll);
              if (!wroteAll) {
                rv = NS_ERROR_UNEXPECTED;
              }
            }
          } else {
            MOZ_ASSERT(!bytes);
          }

          if (NS_FAILED(rv)) {
            outputStream->CloseWithStatus(rv);
          }
        },
        [outputStream](mozilla::ipc::ResponseRejectReason) {
          outputStream->CloseWithStatus(NS_ERROR_FAILURE);
        });

    stream = inputStream.forget();
  } else {
    // Get the icon directly.
    ByteBuf bytebuf;
    nsresult rv = GetIcon(aURI, &bytebuf);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ByteBufToStream(std::move(bytebuf), getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return StreamToChannel(stream.forget(), aURI, getter_AddRefs(mRealChannel));
}

void nsIconChannel::Shutdown() {
  if (gProtoWidget) {
    g_object_unref(gProtoWidget);
    gProtoWidget = nullptr;
  }
}
