#ifndef WAYLANDBLURHELPER_H
#define WAYLANDBLURHELPER_H

class QWindow;

namespace Wayland {

class BlurHelper {
public:
    static bool isWayland();
    static void applyBlur(QWindow *window);
};

}

#endif // WAYLANDBLURHELPER_H
