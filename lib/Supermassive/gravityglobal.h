#ifndef GRAVITY_GLOBAL_H
#define GRAVITY_GLOBAL_H

#include <QtCore/QtGlobal>

#ifdef BUILDING_HEMERA_GRAVITY
#  define HEMERA_GRAVITY_EXPORT Q_DECL_EXPORT
#else
#  define HEMERA_GRAVITY_EXPORT Q_DECL_IMPORT
#endif

#if !defined(Q_OS_WIN) && defined(QT_VISIBILITY_AVAILABLE)
#  define HEMERA_GRAVITY_NO_EXPORT __attribute__((visibility("hidden")))
#endif

#ifndef HEMERA_GRAVITY_NO_EXPORT
#  define HEMERA_GRAVITY_NO_EXPORT
#endif

#endif