TEMPLATE = app
QT += core gui widgets
CONFIG += c++11 link_pkgconfig
PKGCONFIG += gxde-bernese-qt$${QT_MAJOR_VERSION}

include(widgets.pri)
