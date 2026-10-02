# Third-party components

## SDL2 2.32.10

Used by the `OpenHover` executable for window creation, input, and OpenGL context creation. It is
dynamically linked as a system dependency and is not included in this source tree.

SDL2 is available under the zlib License:

```text
This software is provided 'as-is', without any express or implied warranty.  In no event
will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial
applications, and to alter it and redistribute it freely, subject to the following
restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you
	wrote the original software. If you use this software in a product, an acknowledgment
	in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented
	as being the original software.
3. This notice may not be removed or altered from any source distribution.
```

## OpenGL system library

Used by the `OpenHover` executable for its 3D rendering API. OpenHover dynamically links the
system `libGL` and does not include an OpenGL implementation, graphics driver, or its headers
in this source tree. The installed libglvnd dispatch implementation is MIT-licensed; a release
must retain the notices for the platform-provided implementation and driver it ships with.

Add each dependency here in the same commit that introduces it: name, version, licence,
where it is used, and whether it ships in a release. See [licensing](docs/licensing.md) for
which licences are acceptable.
