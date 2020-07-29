# glLibGen - an OpenGL wrapper class code generator for C++
Downloads and parses OpenGL-related headers and extracts as well as online documentation, from which it further extracts function descriptions to be placed in comments. With intellisense, you can scroll through all gl functions, see which version spec they were introduced in, and get a link to the online docs.

glLibGen, Copyright ©2016-2020, Keelan Stuart (hereafter referenced as AUTHOR). All Rights Reserved. Permission to use, copy, modify, and distribute this software is hereby granted, without fee and without a signed licensing agreement, provided that the above copyright notice appears in all copies, modifications, and distributions. Furthermore, AUTHOR assumes no responsibility for any damages caused either directly or indirectly by the use of this software, nor vouches for any fitness of purpose of this software. All other copyrighted material contained herein is noted and rights attributed to individual copyright holders.

glLibTest (part of the main sln) uses glLibGen as a pre-build event to generate its OpenGL code. It then uses it to render a very simple scene in an MFC window. Warning: it takes quite a while to parse gl.h (et al) and download documentation for each function, subsequently parsing out the parameters, etc... but a CRC-containing comment line is appended to the generated .h file that will keep subsequent re-builds of the wrapper from happening, so unless something changes, you will only have to sit through it one time.

Note: build Release first... the pre-build step in the glLibTest project uses the non-debug version of glLibGen.

***

## USAGE: glLibGen64.exe [[-paramname[:]] [-paramname[:]] ...]

### DATA PARAMETERS (you need a ":" after the "-param" on the command line):
```glh                   ; local or remote (HTTP) file path to "gl.h"```
```glexth                ; local or remote (HTTP) file path to "glext.h"```
```khrh                  ; local or remote (HTTP) file path to "khrplatform.h"```
```wglexth               ; local or remote (HTTP) file path to "wglext.h"```
```class                 ; name of the class that will be generated```
```basefile              ; base filename that the C++ code will go into (file.cpp and file.h)```
```outdir                ; directory where the code will be generated and gl headers copied```
```ver                   ; determines the maximum version of OpenGL to support```

### STAND-ALONE PARAMETERS (just the "-param" by itself):
```arb                   ; includes ARB extensions (default on)```
```ext                   ; includes EXT extensions```
```nv                    ; includes nVidia extensions```
```amd                   ; includes AMD extensions```
```ati                   ; includes ATI extensions```
```intel                 ; includes Intel extensions```
```sgi                   ; includes Silicon Graphics extensions```
```sun                   ; includes Sun Microsystems extensions```
```apple                 ; includes Apple extensions```
```oes                   ; includes OES extensions```
```ingr                  ; includes Intergraph extensions```
```khr                   ; includes Khronos extensions```
```afx                   ; adds "#include <stdafx.h>"```
```pch                   ; adds "#include <pch.h>"```

***

### EXAMPLES:

```glLibGen64.exe -ver:4.5 -class:"COpenGL" -basefile:"gl_wrapper" -outdir:"c:\myproj" -glh:"C:\Program Files (x86)\Windows Kits\10\Include\10.0.16299.0\um\gl\GL.h" -glexth:"https://www.khronos.org/registry/OpenGL/api/GL/glext.h" -wglexth:"https://www.khronos.org/registry/OpenGL/api/GL/wglext.h" -khrh:"https://www.khronos.org/registry/EGL/api/KHR/khrplatform.h"```
_Creates gl_wrapper.h and gl_wrapper.cpp in c:\myproj and copies the relevant .h files for OpenGL there, too. COpenGL will include all 4.5 spec functions_

```glLibGen64.exe -ver:3.2 -khr -pch -class:"Cogl" -basefile:"Cogl" -outdir:"d:\somewhere" -glh:"C:\Program Files (x86)\Windows Kits\10\Include\10.0.16299.0\um\gl\GL.h" -glexth:"https://www.khronos.org/registry/OpenGL/api/GL/glext.h" -wglexth:"https://www.khronos.org/registry/OpenGL/api/GL/wglext.h" -khrh:"https://www.khronos.org/registry/EGL/api/KHR/khrplatform.h"```
_Creates Cogl.h and Cogl.cpp in d:\somewhere and copies the relevant .h files for OpenGL there, too. COpenGL will include all 3.2 spec functions, as well as all specifically-marked KHRONOS extensions. Will include <pch.h> at the top>_
