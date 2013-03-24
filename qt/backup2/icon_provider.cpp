// Copyright (C) 2013 Cory Maccarrone
// Author: Cory Maccarrone <darkstar6262@gmail.com>
#include "icon_provider.h"

#include <QIcon>
#include <QPixmap>

#if defined(Q_OS_WIN)
#define _WIN32_IE 0x0500
#include <qt_windows.h>
#include <commctrl.h>
#include <objbase.h>
#endif

#ifdef Q_OS_WIN32
static QImage qt_fromWinHBITMAP(HDC hdc, HBITMAP bitmap, int w, int h) {
  BITMAPINFO bmi;
  memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth       = w;
  bmi.bmiHeader.biHeight      = -h;
  bmi.bmiHeader.biPlanes      = 1;
  bmi.bmiHeader.biBitCount    = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage   = w * h * 4;

  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return image;
  }

  // Get bitmap bits
  uchar *data = (uchar *)malloc(bmi.bmiHeader.biSizeImage);

  if (GetDIBits(hdc, bitmap, 0, h, data, &bmi, DIB_RGB_COLORS)) {
    // Create image and copy data into image.
    for (int y = 0; y < h; ++y) {
      void *dest = (void *) image.scanLine(y);
      void *src = data + y * image.bytesPerLine();
      memcpy(dest, src, image.bytesPerLine());
    }
  } else {
    qWarning("qt_fromWinHBITMAP(), failed to get bitmap bits");
  }

  free(data);
  return image;
}

QPixmap convertHIconToPixmap(const HICON icon) {
  bool foundAlpha = false;
  HDC screenDevice = GetDC(0);
  HDC hdc = CreateCompatibleDC(screenDevice);
  ReleaseDC(0, screenDevice);

  ICONINFO iconinfo;
  // x and y Hotspot describes the icon center.
  bool result = GetIconInfo(icon, &iconinfo);
  if (!result) {
    qWarning("convertHIconToPixmap(), failed to GetIconInfo()");
  }

  int w = iconinfo.xHotspot * 2;
  int h = iconinfo.yHotspot * 2;

  BITMAPINFOHEADER bitmapInfo;
  bitmapInfo.biSize = sizeof(BITMAPINFOHEADER);
  bitmapInfo.biWidth = w;
  bitmapInfo.biHeight = h;
  bitmapInfo.biPlanes = 1;
  bitmapInfo.biBitCount = 32;
  bitmapInfo.biCompression = BI_RGB;
  bitmapInfo.biSizeImage = 0;
  bitmapInfo.biXPelsPerMeter = 0;
  bitmapInfo.biYPelsPerMeter = 0;
  bitmapInfo.biClrUsed = 0;
  bitmapInfo.biClrImportant = 0;
  DWORD* bits;

  HBITMAP winBitmap = CreateDIBSection(
      hdc, reinterpret_cast<BITMAPINFO*>(&bitmapInfo),
      DIB_RGB_COLORS, (VOID**)&bits, NULL, 0);
  HGDIOBJ oldhdc = static_cast<HBITMAP>(SelectObject(hdc, winBitmap));
  DrawIconEx(hdc, 0, 0, icon, iconinfo.xHotspot * 2,
             iconinfo.yHotspot * 2, 0, 0, DI_NORMAL);
  QImage image = qt_fromWinHBITMAP(hdc, winBitmap, w, h);

  for (int y = 0; y < h && !foundAlpha; y++) {
    QRgb* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < w ; x++) {
      if (qAlpha(scanLine[x]) != 0) {
        foundAlpha = true;
        break;
      }
    }
  }
  if (!foundAlpha) {
    // If no alpha was found, we use the mask to set alpha values
    DrawIconEx(hdc, 0, 0, icon, w, h, 0, 0, DI_MASK);
    QImage mask = qt_fromWinHBITMAP(hdc, winBitmap, w, h);

    for (int y = 0; y < h; y++){
      QRgb *scanlineImage = reinterpret_cast<QRgb*>(image.scanLine(y));
      QRgb *scanlineMask = mask.isNull() ?
                               0 : reinterpret_cast<QRgb*>(mask.scanLine(y));
      for (int x = 0; x < w; x++){
        if (scanlineMask && qRed(scanlineMask[x]) != 0) {
          scanlineImage[x] = 0;  // Mask out this pixel
        } else {
          scanlineImage[x] |= 0xff000000;  // Set the alpha channel to 255
        }
      }
    }
  }

  // Dispose resources created by iconinfo call
  DeleteObject(iconinfo.hbmMask);
  DeleteObject(iconinfo.hbmColor);

  SelectObject(hdc, oldhdc);  // Restore state
  DeleteObject(winBitmap);
  DeleteDC(hdc);
  return QPixmap::fromImage(image);
}
#endif  // Q_OS_WIN32

IconProvider::IconProvider() {
}

QIcon IconProvider::FileIcon(const QString &filename) {
  QFileInfo file_info(filename);
  QPixmap pixmap;

#ifdef Q_OS_WIN32

  if (file_info.suffix().isEmpty()) {
    return icon_provider_.icon(QFileIconProvider::File);
  }

  /*
  if (file_info.suffix() == "exe" && file_info.exists()) {
    return icon_provider_.icon(file_info);
  }
  */

  if (!icon_cache_.find(file_info.suffix(), &pixmap)) {
    // We don't use the variable, but by storing it statically, we
    // ensure CoInitialize is only called once.
    static HRESULT com_init = CoInitialize(NULL);
    Q_UNUSED(com_init);

    SHFILEINFO sh_file_info;
    unsigned long val = 0;

    val = SHGetFileInfo(
        reinterpret_cast<const wchar_t*>(("foo." + file_info.suffix()).utf16()),
        0, &sh_file_info, sizeof(sh_file_info),
        SHGFI_ICON | SHGFI_USEFILEATTRIBUTES);

    // Even if GetFileInfo returns a valid result, hIcon can be empty in some
    // cases
    if (val && sh_file_info.hIcon) {
      pixmap = convertHIconToPixmap(sh_file_info.hIcon);
      if (!pixmap.isNull()) {
        icon_cache_.insert(file_info.suffix(), pixmap);
      }
      DestroyIcon(sh_file_info.hIcon);
    } else {
      return icon_provider_.icon(QFileIconProvider::File);
    }
  }

#else
  // Default icon for Linux and Mac OS X for now
  return icon_provider_.icon(file_info);
#endif

  return QIcon(pixmap);
}
