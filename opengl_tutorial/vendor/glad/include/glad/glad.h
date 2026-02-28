// Compatibility shim: forward GLAD1-style #include <glad/glad.h> to GLAD2's gl.h
// and provide GLAD1 typedefs/macros that map to the GLAD2 API.

#ifndef GLAD_COMPAT_H_
#define GLAD_COMPAT_H_

#include <glad/gl.h>

typedef GLADloadfunc GLADloadproc;

#ifndef gladLoadGLLoader
#define gladLoadGLLoader(func) gladLoadGL((GLADloadfunc)(func))
#endif

#endif
