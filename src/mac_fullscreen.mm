// Native macOS fullscreen helpers (green-button fullscreen is Cocoa-side,
// invisible to GLFW's monitor API).
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#import <Cocoa/Cocoa.h>

extern "C" bool macWindowIsFullscreen(GLFWwindow* w) {
    NSWindow* nsw = glfwGetCocoaWindow(w);
    return nsw && (nsw.styleMask & NSWindowStyleMaskFullScreen);
}

extern "C" void macWindowExitFullscreen(GLFWwindow* w) {
    NSWindow* nsw = glfwGetCocoaWindow(w);
    if (nsw && (nsw.styleMask & NSWindowStyleMaskFullScreen))
        [nsw toggleFullScreen:nil];
}
