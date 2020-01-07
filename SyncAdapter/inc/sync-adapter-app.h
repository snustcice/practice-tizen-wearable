#ifndef __SYNC_ADAPTER_H__
#define __SYNC_ADAPTER_H__

#include <tizen.h>
#include <account.h>
#include <app.h>
#include <bundle.h>
#include <dlog.h>
#include <efl_extension.h>
#include <system_settings.h>

/* Sync Adapter application ID for removing account ID */
#define SYNC_ADAPTER_APP_ID "org.example.syncadapter.ui"

/*
 * Sync Adapter Service application ID for registering
 * the Sync Adapter Service as a sync-adapter
 */
#define SYNC_ADAPTER_SERVICE_APP_ID "org.example.syncadapter.service"

/*
 * Package name can coupling the applications,
 * Sync Adapter and Sync Adapter Service
 */
#if !defined(PKG_NAME)
#define PKG_NAME "org.example.syncadapter"
#endif

/* User name for querying account handle */
#define USER_NAME "SyncAdapter"

/* Log tag for printing logs of Sync Adapter */
#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "SyncAdapter"

/* Set timer for showing pop-up as 2 seconds */
#define TIME_FOR_POPUP 2

/* Set pop-up size */
#define SIZE_OF_POPUP 256

/* The line of genlist */
#define NUM_OF_ITEMS 4

/* The number of sync jobs */
#define NUM_OF_SYNC_JOB 3

/* The number of periodic intervals */
#define NUM_OF_INTERVAL 7

/* The number of capabilities for data change sync */
#define NUM_OF_CAPABILITY 5

/* The number of sync options */
#define NUM_OF_OPTION 4

/* Maximum number of genlist lines */
#define MAX_NUM_LINE 10

/* Maximum number of sync jobs which can be registered */
#define MAX_NUM 10

/* Maximum size of the name of displayed menu */
#define MAX_SIZE 50

#endif /* __SYNC_ADAPTER_H__ */
