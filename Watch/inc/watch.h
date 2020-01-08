#ifndef __watch_H__
#define __watch_H__

#include <watch_app.h>
#include <watch_app_efl.h>
#include <Elementary.h>
#include <dlog.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "watch"

#if !defined(PACKAGE)
#define PACKAGE "org.example.watch"
#endif

#endif /* __watch_H__ */
