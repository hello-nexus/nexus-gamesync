# Third-Party Notices

## Aurora-Wrappers

The ABI structure of the LightFX (`src/lightfx.c`) and Logitech
(`src/logiled.c`) capture shims derives from Aurora-Wrappers
(https://github.com/Aurora-RGB/Aurora-Wrappers), which is MIT-licensed.

```
MIT License

Copyright (c) 2016 Anton Pupkov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Vendor SDK headers

The SDK surfaces these shims impersonate are defined by vendor headers, which
are NOT redistributed here; the export ABIs were reimplemented for interop:

- `LFX2.h` / `LFXDecl.h` (Dell LightFX 2.0) are Dell's, Copyright (c) 2007 Dell, Inc.
- `LogitechLEDLib.h` (Logitech Gaming LED SDK) is Logitech's, Copyright (c) 2011-2014 Logitech.
- `RzChromaSDK*.h` (Razer Chroma SDK) is Razer's.

These shims link no vendor code; they are clean-room reimplementations of the
documented export surfaces.
