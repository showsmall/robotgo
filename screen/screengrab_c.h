#include "../base/bitmap_free_c.h"
#include <stdlib.h> /* malloc() */

#if defined(IS_MACOSX)
	#include <OpenGL/OpenGL.h>
	#include <OpenGL/gl.h>
	#include <ApplicationServices/ApplicationServices.h>
#elif defined(USE_X11)
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
	#include "../base/xdisplay_c.h"
#elif defined(IS_WINDOWS)
	#include <string.h>
#endif

MMBitmapRef copyMMBitmapFromDisplayInRect(MMRectInt32 rect, int32_t display_id) {
#if defined(IS_MACOSX)
	MMBitmapRef bitmap = NULL;
	uint8_t *buffer = NULL;
	size_t bufferSize = 0;

	CGDirectDisplayID displayID = (CGDirectDisplayID) display_id;
	if (displayID == -1) {
		displayID = CGMainDisplayID();
	}

	CGImageRef image = CGDisplayCreateImageForRect(displayID,
		CGRectMake(rect.origin.x, rect.origin.y, rect.size.w, rect.size.h));
	if (!image) { return NULL; }

	CFDataRef imageData = CGDataProviderCopyData(CGImageGetDataProvider(image));
	if (!imageData) { return NULL; }

	bufferSize = CFDataGetLength(imageData);
	buffer = malloc(bufferSize);
	CFDataGetBytes(imageData, CFRangeMake(0, bufferSize), buffer);

	bitmap = createMMBitmap_c(buffer, 
			CGImageGetWidth(image), CGImageGetHeight(image), CGImageGetBytesPerRow(image), 
			CGImageGetBitsPerPixel(image), CGImageGetBitsPerPixel(image) / 8);

	CFRelease(imageData);
	CGImageRelease(image);

	return bitmap;
#elif defined(USE_X11)
	MMBitmapRef bitmap;
	Display *display;
	if (display_id == -1) {
		display = XOpenDisplay(NULL);
	} else {
		display = XGetMainDisplay();
	}

	XImage *image = XGetImage(display, XDefaultRootWindow(display), 
							(int)rect.origin.x, (int)rect.origin.y,
	                        (unsigned int)rect.size.w, (unsigned int)rect.size.h, AllPlanes, ZPixmap);
	XCloseDisplay(display);
	if (image == NULL) { return NULL; }

	bitmap = createMMBitmap_c((uint8_t *)image->data, 
				rect.size.w, rect.size.h, (size_t)image->bytes_per_line, 
				(uint8_t)image->bits_per_pixel, (uint8_t)image->bits_per_pixel / 8);
	image->data = NULL; /* Steal ownership of bitmap data so we don't have to copy it. */
	XDestroyImage(image);

	return bitmap;
#elif defined(IS_WINDOWS)
	MMBitmapRef bitmap;
	void *data;
	HDC screen = NULL, screenMem = NULL;
	HBITMAP dib;
	BITMAPINFO bi;

	/* Initialize bitmap info. */
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
   	bi.bmiHeader.biWidth = (long)rect.size.w;
   	bi.bmiHeader.biHeight = -(long)rect.size.h; /* Non-cartesian, please */
   	bi.bmiHeader.biPlanes = 1;
   	bi.bmiHeader.biBitCount = 32;
   	bi.bmiHeader.biCompression = BI_RGB;
   	bi.bmiHeader.biSizeImage = (DWORD)(4 * rect.size.w * rect.size.h);
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	if (display_id == -1) {
		screen = GetDC(NULL); /* Get entire screen */
	} else {
		screen = GetDC((HWND) (uintptr) display_id);
	}
	if (screen == NULL) { return NULL; }

	/* Get screen data in display device context. */
   	dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &data, NULL, 0);

	/* Copy the data into a bitmap struct. */
	BOOL smem = (screenMem = CreateCompatibleDC(screen)) == NULL;
	BOOL bitb = BitBlt(screenMem, (int)0, (int)0, (int)rect.size.w, (int)rect.size.h, 
				screen, rect.origin.x, rect.origin.y, SRCCOPY);
	if (smem || SelectObject(screenMem, dib) == NULL || !bitb) {
		/* Error copying data. */
		ReleaseDC(NULL, screen);
		DeleteObject(dib);
		if (screenMem != NULL) { DeleteDC(screenMem); }

		return NULL;
	}

	bitmap = createMMBitmap_c(NULL, rect.size.w, rect.size.h, 4 * rect.size.w,
	                        	(uint8_t)bi.bmiHeader.biBitCount, 4);

	/* Copy the data to our pixel buffer. */
	if (bitmap != NULL) {
		bitmap->imageBuffer = malloc(bitmap->bytewidth * bitmap->height);
		memcpy(bitmap->imageBuffer, data, bitmap->bytewidth * bitmap->height);
	}

	ReleaseDC(NULL, screen);
	DeleteObject(dib);
	DeleteDC(screenMem);

	return bitmap;
#endif
}
