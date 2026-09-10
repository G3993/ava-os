// Runtime loader for the GL entry points declared in glplat.h (Windows).
#ifndef __APPLE__
#include "glplat.h"
#include <GLFW/glfw3.h>

#define GLPLAT_DEF(ret, name, args) ret(GLPLAT_APIENTRY* name) args = nullptr;
GLPLAT_FUNCS(GLPLAT_DEF)
#undef GLPLAT_DEF

void glplatInit() {
#define GLPLAT_LOAD(ret, name, args)                                           \
    name = (ret(GLPLAT_APIENTRY*) args)glfwGetProcAddress(#name);
    GLPLAT_FUNCS(GLPLAT_LOAD)
#undef GLPLAT_LOAD
}
#endif
