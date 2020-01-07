#include <sync_manager.h>
#include "sync-adapter-app.h"


typedef struct app_data {
	Evas_Object *win;
	Evas_Object *layout;
	Evas_Object *conform;
	Evas_Object *nf;
	/* Object for circular UI */
	Eext_Circle_Surface *circle_surface;
} app_data_s;

/* Declare global app_data object */
app_data_s *g_ad = NULL;

typedef struct list_item_data {
	int index;
	int type;
	Elm_Object_Item *item;
	Eina_Bool expanded;
} list_item_data_s;

/*
 * Declare global list_itme_data object
 * depending on each list view
 */
list_item_data_s *main_id;
list_item_data_s *setting_id;
list_item_data_s *data_id;
list_item_data_s *foreach_id;

typedef struct list_menu_data {
	Evas_Object *nf;
	Evas_Object *popup;
	Evas_Object *genlist;
} list_menu_data_s;

/*
 * Declare global list_menu_data object
 * depending on each list menu which can be selected
 * - Sync types
 * - Period interval types
 * - Capability types
 * - Option types
 */
list_menu_data_s *sync_job_md;
list_menu_data_s *interval_md;
list_menu_data_s *capability_md;
list_menu_data_s *option_md;

/* Account handle and ID for operating sync job related with Account */
account_h account;
int account_id = -1;

/* Flag for verifying accountless case */
bool is_accountless = false;

/* The number of sync jobs which being operated */
int cnt_sync_jobs = 0;

/*
 * Get selected text for displaying it on each button
 * Each button can display below text which is used to set up sync parameters
 */
static char *sync_job_items[] = {"On Demand Sync", "Periodic Sync", "Data Change Sync"};
static char *sync_interval_items[] = {"every 30 minutes", "every an hour", "every 2 hours", "every 3 hours", "every 6 hours", "every 12 hours", "every a day"};
static char *sync_capability_items[] = {"Contact", "Image", "Music", "Sound", "Video"};
static char *sync_option_items[] = {"Option None", "Sync with Priority", "Sync Once", "Sync Once with Priority"};

/* To display selected parameters */
char displayed_job[MAX_SIZE] = "On Demand Sync";
char displayed_interval[MAX_SIZE] = "every 30 minutes";
char displayed_capability[MAX_SIZE] = "Contact";
char displayed_option[MAX_SIZE] = "Option None";

/* Variables as parameter for calling Sync Manager API */
sync_period_e sync_interval = SYNC_PERIOD_INTERVAL_30MIN;
char *sync_capability = SYNC_SUPPORTS_CAPABILITY_CONTACT;
sync_option_e sync_option = SYNC_OPTION_NONE;

/* Below sync job IDs are required for managing each sync job */
int on_demand_sync_job_id = -1;
int periodic_sync_job_id = -1;
int data_change_sync_job_id = -1;
int data_change_id[NUM_OF_CAPABILITY] = {-1, };

/* Store sync job ID to be managed */
int remove_sync_job[MAX_NUM] = {-1, };
/* Store whether sync jobs to be removed is checked */
bool list_of_remove_sync_job[MAX_NUM] = {false, };
/* Store text for showing that on "Manage sync jobs" */
char list_of_sync_jobs[MAX_NUM][MAX_SIZE];

/* Flag for verifying whether user already uses a menu */
int is_enter = 0;


/*
 * Window delete request callback
 */
static Eina_Bool
win_delete_request_cb(void *data, Elm_Object_Item *it)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Delete main view");
	/* Terminate the Sync Adapter sample */
	ui_app_exit();
	return EINA_TRUE;
}


/*
 * Naviframe pop callback for "Start sync with settings"
 */
static Eina_Bool
sync_job_naviframe_pop_cb(void *data, Elm_Object_Item *it)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Return to previous view");
	/* Delete "Start Sync" button when naviframe is popped up */
	evas_object_del((Evas_Object *)data);
	/* Initialize flag to check whether user already uses the menu, "Start sync with settings" */
	is_enter--;
	return EINA_TRUE;
}


/*
 * Naviframe pop callback for "Manage sync jobs"
 */
static Eina_Bool
foreach_naviframe_pop_cb(void *data, Elm_Object_Item *it)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Return to previous view");
	/* Delete "Remove" button when naviframe is popped up */
	evas_object_del((Evas_Object *)data);
	/* Initialize list to show it on the menu "Manage sync jobs" */
	memset(list_of_sync_jobs, '\0', sizeof(list_of_sync_jobs));
	/* Initialize flag to check whether user already uses the menu, "Manage sync jobs" */
	is_enter--;
	/* Initialize variable which is used to count the number of registered sync jobs */
	cnt_sync_jobs = 0;
	return EINA_TRUE;
}


/*
 * Get title of the main view
 */
static char *
get_main_menu_title(void *data, Evas_Object *obj, const char *part)
{
	return strdup("Sync Adapter");
}


/*
 * Get title of the menu, "Start sync with settings"
 */
static char *
get_setting_menu_title(void *data, Evas_Object *obj, const char *part)
{
	return strdup("Start sync with settings");
}


/*
 * Get title of the menu, "Manage sync jobs"
 */
static char *
get_manage_menu_title(void *data, Evas_Object *obj, const char *part)
{
	return strdup("Manage sync jobs");
}


/*
 * Get text which can represent main menu
 */
static char*
get_main_menu_text(void *data, Evas_Object *obj, const char *part)
{
	/* Item data for main UI */
	main_id = (list_item_data_s *)data;

	/*
	 * Below code will make text for main list
	 * - "Create an account"
	 * - "Set as accountless"
	 * - "Start sync with settings"
	 * - "Manage sync jobs"
	 */
	switch (main_id->index) {
	case 0:
		if (!strcmp("elm.text", part)) return strdup("Create an account");
		else return NULL;
	case 1:
		if (!strcmp("elm.text", part)) return strdup("Set as accountless");
		else return NULL;
	case 2:
		if (!strcmp("elm.text", part)) return strdup("Start sync with settings");
		else return NULL;
	case 3:
		if (!strcmp("elm.text", part)) return strdup("Manage sync jobs");
		else return NULL;
	}

	return NULL;
}


/*
 * Get text which can represent menu to select specific sync condition
 * Below menu provides 4 items to set parameters for sync jobs
 * "Sync types" will show On Demand, Periodic, and Data Change sync jobs
 * "Period interval types" will show intervals from 30 minutes to a day
 * "Capability types" will show Contact, Image, Music, Sound, and Video
 * "Option types" will show Option None, Sync with Priority, Sync Once, and Sync Once with Priority
 * Depending on the sync types, list which can be used to select each menu will be enabling
 */
static char *
get_setting_menu_text(void *data, Evas_Object *obj, const char *part)
{
	/* Item data for setting UI */
	setting_id = (list_item_data_s *)data;
	char buf[MAX_SIZE];

	/*
	 * Change the state of genlist depending on selected "Sync types"
	 * Each list represents title, setting value, and description for the menu
	 * In the case of enabled list, setting value is blue
	 * In the opposite case, the value is shown as dim grey for indicating disabled
	 * The description text is changed depending on the state of selection
	 */
	switch (setting_id->index) {
	case 0:
		if (!strcmp("elm.text", part))
			return strdup("Sync types");
		else if (!strcmp("elm.text.1", part))
			snprintf(buf, sizeof(buf), "<font color=#3DB9CCFF>%s</font>", displayed_job);
		break;
	case 1:
		if (!strcmp("elm.text", part)) {
			return strdup("Period interval types");
		} else if (!strcmp("elm.text.1", part)) {
			if (!strcmp(displayed_job, "Periodic Sync"))
				snprintf(buf, sizeof(buf), "<font color=#3DB9CCFF>%s</font>", displayed_interval);
			else
				snprintf(buf, sizeof(buf), "<font color=#88888888>%s</font>", displayed_interval);
		}
		break;
	case 2:
		if (!strcmp("elm.text", part)) {
			return strdup("Capability types");
		} else if (!strcmp("elm.text.1", part)) {
			if (!strcmp(displayed_job, "Data Change Sync"))
				snprintf(buf, sizeof(buf), "<font color=#3DB9CCFF>%s</font>", displayed_capability);
			else
				snprintf(buf, sizeof(buf), "<font color=#88888888>%s</font>", displayed_capability);
		}
		break;
	case 3:
		if (!strcmp("elm.text", part))
			return strdup("Option types");
		else if (!strcmp("elm.text.1", part))
			snprintf(buf, sizeof(buf), "<font color=#3DB9CCFF>%s</font>", displayed_option);
		break;
	}

	return strdup(buf);
}


/*
 * Get text of registered sync jobs
 * It is mainly used to contain text on each line
 */
static char*
get_text_registered_sync_jobs(void *data, Evas_Object *obj, const char *part)
{
	/* Item data for foreach UI */
	foreach_id = (list_item_data_s *)data;

	char buf[MAX_SIZE];
	snprintf(buf, sizeof(buf), "%s", list_of_sync_jobs[foreach_id->index]);

	/* The contents of text in buf is included on each line */
	switch (foreach_id->type % 3) {
	case 0:
		if (!strcmp("elm.text", part)) return strdup(buf);
		else return NULL;
	case 1:
		if (!strcmp("elm.text", part)) return strdup(buf);
		else return NULL;
	case 2:
		if (!strcmp("elm.text", part)) return strdup(buf);
		else if (!strcmp("elm.text.end", part)) return strdup("Sub text");
		else return NULL;
	}

	return NULL;
}


/*
 * Release list item data structure
 */
static void
gl_del_cb(void *data, Evas_Object *obj)
{
	list_item_data_s *lid = data;
	free(lid);
}


/*
 * Dismiss pop-up for timeout
 */
static void
timeout_cb(void *data, Evas_Object *obj, void *event_info)
{
	if (!obj) return;
	elm_popup_dismiss(obj);
}


/*
 * Dismiss pop-up for hiding it
 */
static void
popup_hide_cb(void *data, Evas_Object *obj, void *event_info)
{
	if (!obj) return;
	elm_popup_dismiss(obj);
}


/*
 * Dismiss pop-up for finishing it
 */
static void
popup_finished_cb(void *data, Evas_Object *obj, void *event_info)
{
	if (!obj) return;
	evas_object_del(obj);
}


/*
 * Notify whether account mode is selected
 * Neither "Create an account" nor "Set as accountless" is not pressed before starting a sync job,
 * "Select account mode first" is shown on the pop-up
 */
void
notify_by_using_popup(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : select account mode first - create an account or use accountless mode");

	Evas_Object *win = data, *popup;

	popup = elm_popup_add(win);
	elm_object_style_set(popup, "toast/circle");
	elm_popup_orient_set(popup, ELM_POPUP_ORIENT_BOTTOM);
	eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, popup_hide_cb, NULL);
	evas_object_smart_callback_add(popup, "dismissed", popup_finished_cb, NULL);

	elm_object_part_text_set(popup, "elm.text", "Select account mode first");

	/* Pop-up is maintained for 2 seconds */
	elm_popup_timeout_set(popup, TIME_FOR_POPUP);
	evas_object_smart_callback_add(popup, "timeout", timeout_cb, NULL);

	evas_object_show(popup);
}


/*
 * Query account callback for getting account ID
 */
bool query_account_cb(account_h account, void *user_data)
{
	account_get_account_id(account, &account_id);
	return false;
}


/*
 * Callback for operating on demand sync job
 * This function is a process of calling sync_manager_on_demand_sync_job()
 * Press "Start Sync" button with "Sync types" as "On Demand Sync"
 */
static void
cb_add_on_demand_sync(void *pData, Evas_Object *pObj, void *pEvent_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : Request manual sync");

	/*
	 * Create bundle and use set sync option as sync_job_user_data
	 * displayed_interval will be used to show requested on demand sync job
	 * when it does not be operated properly due to network disconnected
	 */
	bundle *sync_job_user_data = NULL;
	sync_job_user_data = bundle_create();
	bundle_add_str(sync_job_user_data, "option", displayed_option);

	/*
	 * Depending whether account is set, method of calling the API is decided
	 * Below process includes getting account which is set and using it
	 */
	int ret = SYNC_ERROR_NONE;
	if (is_accountless) {
		dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : request accountless On Demand Sync");
		ret = sync_manager_on_demand_sync_job(NULL, "OnDemand", sync_option, sync_job_user_data, &on_demand_sync_job_id);
	} else {
		dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : request On Demand Sync with an account");
		dlog_print(DLOG_INFO, LOG_TAG, "Account Id : [%d]", account_id);
		/* Query account by using account ID */
		ret = account_query_account_by_account_id(account_id, &account);
		if (ret != ACCOUNT_ERROR_NONE)
			dlog_print(DLOG_INFO, LOG_TAG, "account_query_account_by_account_id() is failed [%d : %s]", ret, get_error_message(ret));
		ret = sync_manager_on_demand_sync_job(account, "OnDemand", sync_option, sync_job_user_data, &on_demand_sync_job_id);
	}

	if (ret != SYNC_ERROR_NONE)
		dlog_print(DLOG_INFO, LOG_TAG, "sync_manager_on_demand_sync_job() is failed [%d : %s]", ret, get_error_message(ret));
	else
		dlog_print(DLOG_INFO, LOG_TAG, "Sync manager added on demand sync id [%d]", on_demand_sync_job_id);

	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : Exit the cb_add_on_demand_sync()");
}


/*
 * Callback for operating periodic sync job
 * This function is a process of calling sync_manager_add_periodic_sync_job()
 * Press "Start Sync" button with "Sync types" as "Periodic Sync"
 */
static void
cb_add_periodic_sync(void *pData, Evas_Object *pObj, void *pEvent_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : Request periodic sync");

	/*
	 * Create bundle and use set sync interval as sync_job_user_data
	 * displayed_interval will be used to show registered periodic sync job
	 */
	bundle *sync_job_user_data = NULL;
	sync_job_user_data = bundle_create();
	bundle_add_str(sync_job_user_data, "interval", displayed_interval);

	/*
	 * Depending whether account is set, method of calling the API is decided
	 * Below process includes getting account which is set and using it
	 */
	int ret = SYNC_ERROR_NONE;
	if (is_accountless) {
		dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : request accountless Periodic Sync");
		ret = sync_manager_add_periodic_sync_job(NULL, "Periodic", sync_interval, sync_option, sync_job_user_data, &periodic_sync_job_id);
	} else {
		dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : request Periodic Sync with an account");
		dlog_print(DLOG_INFO, LOG_TAG, "Account Id : [%d]", account_id);
		/* Query account by using account ID */
		ret = account_query_account_by_account_id(account_id, &account);
		if (ret != ACCOUNT_ERROR_NONE)
			dlog_print(DLOG_INFO, LOG_TAG, "account_query_account_by_account_id() is failed [%d : %s]", ret, get_error_message(ret));
		ret = sync_manager_add_periodic_sync_job(account, "Periodic", sync_interval, sync_option, sync_job_user_data, &periodic_sync_job_id);
	}

	if (ret != SYNC_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "sync_manager_add_periodic_sync_job() is failed [%d : %s]", ret, get_error_message(ret));
	} else {
		dlog_print(DLOG_INFO, LOG_TAG, "Sync manager added periodic sync id [%d]", periodic_sync_job_id);

		/*
		 * Adding periodic sync job will not send response immediately without expedited option
		 * So, below pop-up messages show the result of adding periodic sync job
		 */
		if ((sync_option == SYNC_OPTION_NONE) | (sync_option == SYNC_OPTION_NO_RETRY)) {
			Evas_Object *win = g_ad->nf, *popup;
			popup = elm_popup_add(win);
			elm_object_style_set(popup, "toast/circle");
			elm_popup_orient_set(popup, ELM_POPUP_ORIENT_BOTTOM);
			eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, popup_hide_cb, NULL);
			evas_object_smart_callback_add(popup, "dismissed", popup_finished_cb, NULL);

			if (sync_interval == SYNC_PERIOD_INTERVAL_30MIN)
				elm_object_part_text_set(popup, "elm.text", "It is expected in 30mins");
			else if (sync_interval == SYNC_PERIOD_INTERVAL_1H)
				elm_object_part_text_set(popup, "elm.text", "It is expected in an hour");
			else if (sync_interval == SYNC_PERIOD_INTERVAL_2H)
				elm_object_part_text_set(popup, "elm.text", "It is expected in 2 hours");
			else if (sync_interval == SYNC_PERIOD_INTERVAL_3H)
				elm_object_part_text_set(popup, "elm.text", "It is expected in 3 hours");
			else if (sync_interval == SYNC_PERIOD_INTERVAL_6H)
				elm_object_part_text_set(popup, "elm.text", "It is expected in 6 hours");
			else if (sync_interval == SYNC_PERIOD_INTERVAL_12H)
				elm_object_part_text_set(popup, "elm.text", "It is expected in 12 hours");
			else
				elm_object_part_text_set(popup, "elm.text", "It is expected in a day");

			/* Pop-up is maintained for 2 seconds */
			elm_popup_timeout_set(popup, TIME_FOR_POPUP);
			evas_object_smart_callback_add(popup, "timeout", timeout_cb, NULL);

			evas_object_show(popup);
		}
	}

	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : Exit the cb_add_periodic_sync()");
}


/*
 * Callback for operating data change sync job
 * This function is a process of calling sync_manager_add_data_change_sync_job()
 * Press "Start Sync" button with "Sync types" as "Data Change Sync"
 */
static void
cb_add_data_change_sync(void *pData, Evas_Object *pObj, void *pEvent_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : Request data change sync");

	/*
	 * Depending whether account is set, method of calling the API is decided
	 * Below process includes getting account which is set and using it
	 */
	int ret = SYNC_ERROR_NONE, idx = 0;
	if (is_accountless) {
		dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : request accountless Data Change Sync");

		/*
		 * This logic is used to store data change sync job id
		 * Because sync capabilities are six kinds, they should be stored
		 * for managing the multiple of registered data change sync jobs
		 */
		for (idx = 0; idx < NUM_OF_CAPABILITY; idx++) {
			if (data_change_id[idx] == -1) {
				ret = sync_manager_add_data_change_sync_job(NULL, sync_capability, sync_option, NULL, &data_change_sync_job_id);
				data_change_id[idx] = data_change_sync_job_id;
				dlog_print(DLOG_INFO, LOG_TAG, "[accountless case] restored data_change_id[%d] = %d", idx, data_change_id[idx]);
				break;
			} else {
				if (idx == NUM_OF_CAPABILITY - 1) {
					dlog_print(DLOG_INFO, LOG_TAG, "data_change_id[idx] is full");
					break;
				}
				continue;
			}
		}
	} else {
		dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : request Data Change Sync with an account");
		dlog_print(DLOG_INFO, LOG_TAG, "Account Id : [%d]", account_id);
		/* Query account by using account ID */
		ret = account_query_account_by_account_id(account_id, &account);
		if (ret != ACCOUNT_ERROR_NONE)
			dlog_print(DLOG_INFO, LOG_TAG, "account_query_account_by_account_id() is failed [%d : %s]", ret, get_error_message(ret));

		/*
		 * This logic is used to store data change sync job id
		 * Because sync capabilities are six kinds and they should be stored
		 * for managing the multiple of registered data change sync jobs
		 */
		for (idx = 0; idx < NUM_OF_CAPABILITY; idx++) {
			if (data_change_id[idx] == -1) {
				ret = sync_manager_add_data_change_sync_job(account, sync_capability, sync_option, NULL, &data_change_sync_job_id);
				data_change_id[idx] = data_change_sync_job_id;
				dlog_print(DLOG_INFO, LOG_TAG, "[account case] restored data_change_id[%d] = %d", idx, data_change_id[idx]);
				break;
			} else {
				if (idx == NUM_OF_CAPABILITY - 1) {
					dlog_print(DLOG_INFO, LOG_TAG, "data_change_id[idx] is full");
					break;
				}
				continue;
			}
		}
	}

	if (ret != SYNC_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "sync_manager_add_data_change_sync_job() is failed [%d : %s]", ret, get_error_message(ret));
	} else {
		if (data_change_sync_job_id != -1)
			dlog_print(DLOG_INFO, LOG_TAG, "Sync manager added data change sync id [%d]", data_change_sync_job_id);
	}

	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : Exit the cb_add_data_change_sync()");
}


/*
 * Callback for operating sync jobs when "Start Sync" button is pressed
 */
static void
cb_start_sync(void *data, Evas_Object *obj, void *event_info)
{
	/* app_data */
	app_data_s *ad = NULL;
	ad = (app_data_s *) g_ad;
	if (ad == NULL) return;

	/*
	 * Depending on displayed_job, one of below callback functions will be invoked
	 * or pop-up will be shown for Data Changed Sync
	 * - cb_add_on_demand_sync() for operating On Demand Sync
	 * - cb_add_periodic_sync() for operating Periodic Sync
	 */
	if (!strcmp(displayed_job, "On Demand Sync")) {
		cb_add_on_demand_sync(ad, NULL, NULL);
	} else if (!strcmp(displayed_job, "Periodic Sync")) {
		cb_add_periodic_sync(ad, NULL, NULL);
	} else {
		cb_add_data_change_sync(ad, NULL, NULL);
		Evas_Object *nf = data, *popup, *layout;

		popup = elm_popup_add(nf);
		elm_object_style_set(popup, "circle");
		eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, popup_hide_cb, NULL);
		evas_object_smart_callback_add(popup, "dismissed", popup_finished_cb, NULL);

		layout = elm_layout_add(popup);
		elm_layout_theme_set(layout, "layout", "popup", "content/circle");

		elm_object_part_text_set(layout, "elm.text", "Press home button."
													" Change data in your device."
													" Pop up will come after change."
													" Check the pop up message.");
		elm_object_content_set(popup, layout);

		evas_object_show(popup);
	}
}


/*
 * Create an account handle for registering and managing sync job with account
 */
static void
create_account()
{
	/* Create an account handle */
	int ret = account_create(&account);
	if (ret != ACCOUNT_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "account_create() is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}

	/* Set user name in the account handle */
	ret = account_set_user_name(account, USER_NAME);
	if (ret != ACCOUNT_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "account_set_user_name() is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}

	/* Set package name in the account handle */
	ret = account_set_package_name(account, PKG_NAME);
	if (ret != ACCOUNT_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "account_set_package_name() is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}

	/* Set Account Capabilities in the account handle for Contact */
	ret = account_set_capability(account, ACCOUNT_SUPPORTS_CAPABILITY_CONTACT, ACCOUNT_CAPABILITY_ENABLED);
	if (ret != ACCOUNT_ERROR_NONE)
		dlog_print(DLOG_INFO, LOG_TAG, "account_set_capability() for contact is failed [%d : %s]", ret, get_error_message(ret));

	/* Set sync support on the account handle */
	ret = account_set_sync_support(account, ACCOUNT_SYNC_STATUS_IDLE);
	if (ret != ACCOUNT_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "account_set_sync_support() is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}

	/*
	 * Insert the account handle into Account DB
	 * If there is an account handle for the Sync Adapter already,
	 * query it by using user name
	 */
	ret = account_insert_to_db(account, &account_id);
	if (ret == ACCOUNT_ERROR_DUPLICATED) {
		dlog_print(DLOG_INFO, LOG_TAG, "account is already exist and set properly");
		/* Query account id with user name "SyncAdapter" */
		account_query_account_by_user_name(query_account_cb, USER_NAME, NULL);
	} else if (ret != ACCOUNT_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "account_insert_to_db() is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}

	/* After creating account, is_accountless flag is set as false */
	is_accountless = false;
}


/*
 * Callback for creating or setting account handle
 * It is invoked when "Create an account" button is pressed on main menu
 */
void
on_create_account_sync_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Enter the create_account()");

	Evas_Object *nf = data, *popup;
	Evas_Object *win = nf;

	popup = elm_popup_add(win);
	elm_object_style_set(popup, "toast/circle");
	elm_popup_orient_set(popup, ELM_POPUP_ORIENT_BOTTOM);
	eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, popup_hide_cb, NULL);
	evas_object_smart_callback_add(popup, "dismissed", popup_finished_cb, NULL);

	/*
	 * If there is no account ID is set, create_account() is called
	 * Also, "Account creation completed" is popped up after creating an account
	 * In the opposite case, "Account is already created" rises on bottom of the view
	 */
	if (account_id == -1) {
		create_account();
		elm_object_part_text_set(popup, "elm.text", "Account creation completed");
	} else {
		elm_object_part_text_set(popup, "elm.text", "Account is already created");
	}

	/* Pop-up is maintained for 2 seconds */
	elm_popup_timeout_set(popup, TIME_FOR_POPUP);
	evas_object_smart_callback_add(popup, "timeout", timeout_cb, NULL);

	evas_object_show(popup);

	dlog_print(DLOG_INFO, LOG_TAG, "Setting account complete, Account ID = [%d]", account_id);
}


/*
 * Set accountless case for calling sync-manager APIs without Account
 */
static void
set_accountless()
{
	dlog_print(DLOG_INFO, LOG_TAG, "Account ID is initialized");
	account_id = -1;
	is_accountless = true;
}


/*
 * Callback for unsetting account handle
 * It is invoked when "Set as accountless" button is pressed on main menu
 */
void
on_select_accountless_sync_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Enter the select_accountless()");

	Evas_Object *nf = data, *popup;
	Evas_Object *win = nf;

	popup = elm_popup_add(win);
	elm_object_style_set(popup, "toast/circle");
	elm_popup_orient_set(popup, ELM_POPUP_ORIENT_BOTTOM);
	eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, popup_hide_cb, NULL);
	evas_object_smart_callback_add(popup, "dismissed", popup_finished_cb, NULL);

	if (!is_accountless) {
		set_accountless();
		elm_object_part_text_set(popup, "elm.text", "Setting accountless completed");
	} else {
		elm_object_part_text_set(popup, "elm.text", "Accountless is already set");
	}

	/* Pop-up is maintained for 2 seconds */
	elm_popup_timeout_set(popup, TIME_FOR_POPUP);
	evas_object_smart_callback_add(popup, "timeout", timeout_cb, NULL);

	evas_object_show(popup);
}


/*
 * Callback for storing and setting selected "Sync types"
 */
static void
sync_job_select_cb(void *data, Evas_Object *obj, void *event_info)
{
	Evas_Object *genlist = ((list_menu_data_s *)data)->genlist;
	Evas_Object *popup = ((list_menu_data_s *)data)->popup;
	Elm_Object_Item *item = (Elm_Object_Item *)event_info;

	int idx = (int)elm_object_item_data_get(item);
	dlog_print(DLOG_INFO, LOG_TAG, "Selected text sync_job_items[%d] : [%s]\n", idx, sync_job_items[idx]);
	/* Selected text is copied to displayable variable */
	snprintf(displayed_job, MAX_SIZE, "%s", sync_job_items[idx]);

	item = elm_genlist_first_item_get(genlist);
	elm_genlist_item_update(item);

	/*
	 * Initialize index which is used to draw a new genlist view
	 * as many as the number of genlist items
	 */
	idx = 0;
	while (idx != (int)elm_genlist_items_count(genlist)) {
		item = elm_genlist_item_next_get(item);
		elm_genlist_item_update(item);
		idx++;
	}

	evas_object_del(popup);
}


/*
 * Callback for storing and setting selected "Period interval types"
 */
static void
sync_interval_select_cb(void *data, Evas_Object *obj, void *event_info)
{
	Evas_Object *genlist = ((list_menu_data_s *)data)->genlist;
	Evas_Object *popup = ((list_menu_data_s *)data)->popup;
	Elm_Object_Item *item = (Elm_Object_Item *)event_info;

	int idx = (int)elm_object_item_data_get(item);
	dlog_print(DLOG_INFO, LOG_TAG, "Selected text sync_interval_items[%d] : [%s]\n", idx, sync_interval_items[idx]);
	/* Selected text is copied to displayable variable */
	snprintf(displayed_interval, MAX_SIZE, "%s", sync_interval_items[idx]);

	/*
	 * The sync_interval will be set depending on the text
	 * It will be used to call sync_manager_add_periodic_sync_job() as parameter
	 */
	if (!strcmp("every 30 minutes", displayed_interval))
		sync_interval = SYNC_PERIOD_INTERVAL_30MIN;
	else if (!strcmp("every an hour", displayed_interval))
		sync_interval = SYNC_PERIOD_INTERVAL_1H;
	else if (!strcmp("every 2 hours", displayed_interval))
		sync_interval = SYNC_PERIOD_INTERVAL_2H;
	else if (!strcmp("every 3 hours", displayed_interval))
		sync_interval = SYNC_PERIOD_INTERVAL_3H;
	else if (!strcmp("every 6 hours", displayed_interval))
		sync_interval = SYNC_PERIOD_INTERVAL_6H;
	else if (!strcmp("every 12 hours", displayed_interval))
		sync_interval = SYNC_PERIOD_INTERVAL_12H;
	else
		sync_interval = SYNC_PERIOD_INTERVAL_1DAY;

	item = elm_genlist_first_item_get(genlist);
	elm_genlist_item_update(item);

	/*
	 * Initialize index which is used to draw a new genlist view
	 * as many as the number of genlist items
	 */
	idx = 0;
	while (idx != (int)elm_genlist_items_count(genlist)) {
		item = elm_genlist_item_next_get(item);
		elm_genlist_item_update(item);
		idx++;
	}

	evas_object_del(popup);
}


/*
 * Callback for storing and setting selected "Capability types"
 */
static void
sync_capability_select_cb(void *data, Evas_Object *obj, void *event_info)
{
	Evas_Object *genlist = ((list_menu_data_s *)data)->genlist;
	Evas_Object *popup = ((list_menu_data_s *)data)->popup;
	Elm_Object_Item *item = (Elm_Object_Item *)event_info;

	int idx = (int)elm_object_item_data_get(item);
	dlog_print(DLOG_INFO, LOG_TAG, "Selected text sync_capability_items[%d] : [%s]\n", idx, sync_capability_items[idx]);
	/* Selected text is copied to displayable variable */
	snprintf(displayed_capability, MAX_SIZE, "%s", sync_capability_items[idx]);

	/*
	 * The sync_capability will be set depending on the text
	 * It will be used to call sync_manager_add_data_change_sync_job() as parameter
	 */
	if (!strcmp("Contact", displayed_capability))
		sync_capability = SYNC_SUPPORTS_CAPABILITY_CONTACT;
	else if (!strcmp("Image", displayed_capability))
		sync_capability = SYNC_SUPPORTS_CAPABILITY_IMAGE;
	else if (!strcmp("Music", displayed_capability))
		sync_capability = SYNC_SUPPORTS_CAPABILITY_MUSIC;
	else if (!strcmp("Sound", displayed_capability))
		sync_capability = SYNC_SUPPORTS_CAPABILITY_SOUND;
	else
		sync_capability = SYNC_SUPPORTS_CAPABILITY_VIDEO;

	item = elm_genlist_first_item_get(genlist);
	elm_genlist_item_update(item);

	/*
	 * Initialize index which is used to draw a new genlist view
	 * as many as the number of genlist items
	 */
	idx = 0;
	while (idx != (int)elm_genlist_items_count(genlist)) {
		item = elm_genlist_item_next_get(item);
		elm_genlist_item_update(item);
		idx++;
	}

	evas_object_del(popup);
}


/*
 * Callback for storing and setting selected "Option types"
 */
static void
sync_option_select_cb(void *data, Evas_Object *obj, void *event_info)
{
	Evas_Object *genlist = ((list_menu_data_s *)data)->genlist;
	Evas_Object *popup = ((list_menu_data_s *)data)->popup;
	Elm_Object_Item *item = (Elm_Object_Item *)event_info;

	int idx = 0;
	idx = (int)elm_object_item_data_get(item);
	dlog_print(DLOG_INFO, LOG_TAG, "Selected text sync_option_items[%d] : [%s]\n", idx, sync_option_items[idx]);
	/* Selected text is copied to displayable variable */
	snprintf(displayed_option, MAX_SIZE, "%s", sync_option_items[idx]);

	if (!strcmp("Option None", displayed_option))
		sync_option = SYNC_OPTION_NONE;
	else if (!strcmp("Sync with Priority", displayed_option))
		sync_option = SYNC_OPTION_EXPEDITED;
	else if (!strcmp("Sync Once", displayed_option))
		sync_option = SYNC_OPTION_NO_RETRY;
	else
		sync_option = SYNC_OPTION_EXPEDITED | SYNC_OPTION_NO_RETRY;

	item = elm_genlist_first_item_get(genlist);
	elm_genlist_item_update(item);

	/*
	 * Initialize index which is used to draw a new genlist view
	 * as many as the number of genlist items
	 */
	idx = 0;
	while (idx != (int)elm_genlist_items_count(genlist)) {
		item = elm_genlist_item_next_get(item);
		elm_genlist_item_update(item);
		idx++;
	}

	evas_object_del(popup);
}


/*
 * Callback for getting text about the list of "Sync types"
 */
static char*
sync_job_text_get_cb(void *data, Evas_Object *obj, const char *part)
{
	int idx = (int)data;
	return strdup(sync_job_items[idx]);
}


/*
 * Callback for getting text about the list of "Period interval types"
 */
static char*
sync_interval_text_get_cb(void *data, Evas_Object *obj, const char *part)
{
	int idx = (int)data;
	return strdup(sync_interval_items[idx]);
}


/*
 * Callback for getting text about the list of "Capability types"
 */
static char*
sync_capability_text_get_cb(void *data, Evas_Object *obj, const char *part)
{
	int idx = (int)data;
	return strdup(sync_capability_items[idx]);
}


/*
 * Callback for getting text about the list of "Option types"
 */
static char*
sync_option_text_get_cb(void *data, Evas_Object *obj, const char *part)
{
	int idx = (int)data;
	return strdup(sync_option_items[idx]);
}


/*
 * Callback for showing pop-up list of "Sync types"
 */
static void
on_sync_job_list_popup_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Select Sync Job types");

	static Elm_Genlist_Item_Class itc;
	Evas_Object *box, *genlist, *popup;
	Evas_Object *nf = ((list_menu_data_s *)data)->nf;
	((list_menu_data_s *)data)->genlist = (Evas_Object *)obj;

	/* Pop-up object and its title is set as "Sync types" */
	popup = elm_popup_add(nf);
	elm_popup_align_set(popup, ELM_NOTIFY_ALIGN_FILL, 1.0);
	eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, eext_popup_back_cb, NULL);
	elm_object_part_text_set(popup, "title,text", "Sync types");
	evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	/* Box object for including genlist */
	box = elm_box_add(popup);
	evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	genlist = elm_genlist_add(box);
	evas_object_size_hint_weight_set(genlist, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);

	/*
	 * Get text which are included on the list like below
	 * - "On Demand Sync"
	 * - "Periodic Sync"
	 * - "Data Change Sync"
	 */
	itc.item_style = "default";
	itc.func.text_get = sync_job_text_get_cb;
	itc.func.content_get = NULL;
	itc.func.state_get = NULL;
	itc.func.del = NULL;

	((list_menu_data_s *)data)->popup = popup;

	/*
	 * Once each item on the list is pressed,
	 * sync_job_select_cb() is invoked and the value is set depending on its text
	 */
	int idx;
	for (idx = 0; idx < NUM_OF_SYNC_JOB; idx++)
		elm_genlist_item_append(genlist, &itc, (void *)idx, NULL, ELM_GENLIST_ITEM_GROUP, sync_job_select_cb, sync_job_md);

	evas_object_show(genlist);
	elm_box_pack_end(box, genlist);
	evas_object_size_hint_min_set(box, -1, SIZE_OF_POPUP);
	elm_object_content_set(popup, box);

	evas_object_show(popup);
}


/*
 * Callback for showing pop-up list of "Period interval types"
 */
static void
on_sync_interval_list_popup_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Select interval types");

	/*
	 * Below if block is for enabling "Period interval types"
	 * It is only enabled when the "Sync types" is set as "Periodic Sync"
	 * On the other hand, "Sync types" is set as the other types,
	 * then, this pop-up list is disabled to press
	 */
	if (!strcmp(displayed_job, "Periodic Sync")) {
		static Elm_Genlist_Item_Class itc;
		Evas_Object *box, *genlist, *popup;
		Evas_Object *nf = ((list_menu_data_s *)data)->nf;
		((list_menu_data_s *)data)->genlist = (Evas_Object *)obj;

		/* Pop-up object and its title is set as "Period interval types" */
		popup = elm_popup_add(nf);
		elm_popup_align_set(popup, ELM_NOTIFY_ALIGN_FILL, 1.0);
		eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, eext_popup_back_cb, NULL);
		elm_object_part_text_set(popup, "title,text", "Period interval types");
		evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

		/* Box object for including genlist */
		box = elm_box_add(popup);
		evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

		genlist = elm_genlist_add(box);
		evas_object_size_hint_weight_set(genlist, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);

		/*
		 * Get text which are included on the list like below
		 * - "every 30 minutes"
		 * - "every an hour"
		 * - "every 2 hours"
		 * - "every 3 hours"
		 * - "every 6 hours"
		 * - "every 12 hours"
		 * - "every a day"
		 */
		itc.item_style = "default";
		itc.func.text_get = sync_interval_text_get_cb;
		itc.func.content_get = NULL;
		itc.func.state_get = NULL;
		itc.func.del = NULL;

		((list_menu_data_s *)data)->popup = popup;

		/*
		 * Once each item on the list is pressed,
		 * sync_interval_select_cb() is invoked and the value is set depending on its text
		 */
		int idx;
		for (idx = 0; idx < NUM_OF_INTERVAL; idx++)
			elm_genlist_item_append(genlist, &itc, (void *)idx, NULL, ELM_GENLIST_ITEM_GROUP, sync_interval_select_cb, interval_md);

		evas_object_show(genlist);
		elm_box_pack_end(box, genlist);
		evas_object_size_hint_min_set(box, -1, SIZE_OF_POPUP);
		elm_object_content_set(popup, box);

		evas_object_show(popup);
	}
}


/*
 * Callback for showing pop-up list of "Capability types"
 */
static void
on_sync_capability_list_popup_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Select capability types");

	/*
	 * Below if block is for enabling "Capability types"
	 * It is only enabled when the "Sync types" is set as "Data Change Sync"
	 * On the other hand, "Sync types" is set as the other types,
	 * then, this pop-up list is disabled to press
	 */
	if (!strcmp(displayed_job, "Data Change Sync")) {
		static Elm_Genlist_Item_Class itc;
		Evas_Object *box, *genlist, *popup;
		Evas_Object *nf = ((list_menu_data_s *)data)->nf;
		((list_menu_data_s *)data)->genlist = (Evas_Object *)obj;

		/* Pop-up object and its title is set as "Capability types" */
		popup = elm_popup_add(nf);
		elm_popup_align_set(popup, ELM_NOTIFY_ALIGN_FILL, 1.0);
		eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, eext_popup_back_cb, NULL);
		elm_object_part_text_set(popup, "title,text", "Capability types");
		evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

		/* Box object for including genlist */
		box = elm_box_add(popup);
		evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

		genlist = elm_genlist_add(box);
		evas_object_size_hint_weight_set(genlist, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
		evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);

		/*
		 * Get text which are included on the list like below
		 * - "Contact"
		 * - "Image"
		 * - "Music"
		 * - "Sound"
		 * - "Video"
		 */
		itc.item_style = "default";
		itc.func.text_get = sync_capability_text_get_cb;
		itc.func.content_get = NULL;
		itc.func.state_get = NULL;
		itc.func.del = NULL;

		((list_menu_data_s *)data)->popup = popup;

		/*
		 * Once each item on the list is pressed,
		 * sync_capability_select_cb() is invoked and the value is set depending on its text
		 */
		int idx;
		for (idx = 0; idx < NUM_OF_CAPABILITY; idx++)
			elm_genlist_item_append(genlist, &itc, (void *)idx, NULL, ELM_GENLIST_ITEM_GROUP, sync_capability_select_cb, capability_md);

		evas_object_show(genlist);
		elm_box_pack_end(box, genlist);
		evas_object_size_hint_min_set(box, -1, SIZE_OF_POPUP);
		elm_object_content_set(popup, box);

		evas_object_show(popup);
	}
}


/*
 * Callback for showing pop-up list of "Option types"
 */
static void
on_sync_option_list_popup_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Select option types");

	static Elm_Genlist_Item_Class itc;
	Evas_Object *box, *genlist, *popup;
	Evas_Object *nf = ((list_menu_data_s *)data)->nf;
	((list_menu_data_s *)data)->genlist = (Evas_Object *)obj;

	/* Pop-up object and its title is set as "Option types" */
	popup = elm_popup_add(nf);
	elm_popup_align_set(popup, ELM_NOTIFY_ALIGN_FILL, 1.0);
	eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, eext_popup_back_cb, NULL);
	elm_object_part_text_set(popup, "title,text", "Option types");
	evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	/* Box object for including genlist */
	box = elm_box_add(popup);
	evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	genlist = elm_genlist_add(box);
	evas_object_size_hint_weight_set(genlist, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(genlist, EVAS_HINT_FILL, EVAS_HINT_FILL);

	/*
	 * Get text which are included on the list like below
	 * - "Option None"
	 * - "Sync with Priority"
	 * - "Sync Once"
	 * - "Sync Once with Priority"
	 */
	itc.item_style = "default";
	itc.func.text_get = sync_option_text_get_cb;
	itc.func.content_get = NULL;
	itc.func.state_get = NULL;
	itc.func.del = NULL;

	((list_menu_data_s *)data)->popup = popup;

	/*
	 * Once each item on the list is pressed,
	 * sync_option_select_cb() is invoked and the value is set depending on its text
	 */
	int idx;
	for (idx = 0; idx < NUM_OF_OPTION; idx++)
		elm_genlist_item_append(genlist, &itc, (void *)idx, NULL, ELM_GENLIST_ITEM_GROUP, sync_option_select_cb, option_md);

	evas_object_show(genlist);
	elm_box_pack_end(box, genlist);
	evas_object_size_hint_min_set(box, -1, SIZE_OF_POPUP);
	elm_object_content_set(popup, box);

	evas_object_show(popup);
}


/*
 * Callback for drawing setting menu
 * It is invoked when "Start sync with settings" is pressed on main UI
 * It also includes a button for starting sync jobs,
 * "Start Sync" is written on the button
 * Once the button is pressed, the Sync Adapter conducts using sync-manager APIs
 * depending on the "Sync types" which is set
 */
void
on_select_settings_sync_cb(void *data, Evas_Object *obj, void *event_info)
{
	/* Flag for verifying whether user already uses a menu */
	if (is_enter) return;
	is_enter++;

	dlog_print(DLOG_INFO, LOG_TAG, "Enter the Start sync with settings");

	/* app_data */
	app_data_s *ad = NULL;
	ad = (app_data_s *) g_ad;
	if (ad == NULL) return;

	Elm_Object_Item *nf_it, *it;
	Evas_Object *nf = data, *syncBtn, *genlist, *circle_genlist;
	ad->nf = nf;

	/* Elm_Genlist_Item_Class for title, list, padding */
	Elm_Genlist_Item_Class *ttc = elm_genlist_item_class_new();
	Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
	Elm_Genlist_Item_Class *ptc = elm_genlist_item_class_new();

	genlist = elm_genlist_add(ad->layout);
	elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);

	/* genlist for circular view */
	circle_genlist = eext_circle_object_genlist_add(genlist, ad->circle_surface);
	eext_circle_object_genlist_scroller_policy_set(circle_genlist, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
	eext_rotary_object_event_activated_set(circle_genlist, EINA_TRUE);

	ttc->item_style = "title";
	ttc->func.text_get = get_setting_menu_title;

	itc->item_style = "2text";
	itc->func.text_get = get_setting_menu_text;
	itc->func.del = gl_del_cb;

	ptc->item_style = "padding";

	/* Append genlist item for title */
	elm_genlist_item_append(genlist, ttc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

	/*
	 * This logic is for appending genlist items.
	 * Each line of the list is linked to callback functions for showing respective pop-up list
	 * Each pop-up list is based on respective menu data
	 */
	int idx;
	for (idx = 0; idx < NUM_OF_ITEMS; idx++) {
		setting_id = calloc(sizeof(list_item_data_s), 1);
		setting_id->type = 3;
		setting_id->index = idx;
		setting_id->item = it;

		switch (idx) {
		case 0:
			sync_job_md = calloc(sizeof(list_menu_data_s), 1);
			sync_job_md->nf = nf;
			it = elm_genlist_item_append(genlist, itc, setting_id, NULL, ELM_GENLIST_ITEM_NONE, on_sync_job_list_popup_cb, sync_job_md);
			break;
		case 1:
			interval_md = calloc(sizeof(list_menu_data_s), 1);
			interval_md->nf = nf;
			it = elm_genlist_item_append(genlist, itc, setting_id, NULL, ELM_GENLIST_ITEM_NONE, on_sync_interval_list_popup_cb, interval_md);
			break;
		case 2:
			capability_md = calloc(sizeof(list_menu_data_s), 1);
			capability_md->nf = nf;
			it = elm_genlist_item_append(genlist, itc, setting_id, NULL, ELM_GENLIST_ITEM_NONE, on_sync_capability_list_popup_cb, capability_md);
			break;
		case 3:
			option_md = calloc(sizeof(list_menu_data_s), 1);
			option_md->nf = nf;
			it = elm_genlist_item_append(genlist, itc, setting_id, NULL, ELM_GENLIST_ITEM_NONE, on_sync_option_list_popup_cb, option_md);
			break;
		default:
			break;
		}

		setting_id->item = it;
	}

	/* Append genlist item for padding */
	elm_genlist_item_append(genlist, ptc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

	elm_genlist_item_class_free(ttc);
	elm_genlist_item_class_free(itc);
	elm_genlist_item_class_free(ptc);

	/* Button for stating sync jobs */
	elm_layout_theme_set(ad->layout, "layout", "bottom_button", "default");
	syncBtn = elm_button_add(ad->nf);
	elm_object_style_set(syncBtn, "bottom");
	elm_object_text_set(syncBtn, "Start Sync");
	elm_object_part_content_set(ad->layout, "elm.swallow.button", syncBtn);

	/*
	 * When you start sync job without setting for Account,
	 * any sync job won't be operated and
	 * pop-up message will be shown as "Select account mode first"
	 */
	if (is_accountless || (account_id != -1))
		evas_object_smart_callback_add(syncBtn, "clicked", cb_start_sync, nf);
	else
		evas_object_smart_callback_add(syncBtn, "clicked", notify_by_using_popup, nf);

	evas_object_show(syncBtn);

	nf_it = elm_naviframe_item_push(nf, "Start sync with settings", NULL, NULL, genlist, "empty");
	elm_naviframe_item_pop_cb_set(nf_it, sync_job_naviframe_pop_cb, syncBtn);
}


/*
 * Callback for receiving the result of sync_manager_foreach_sync_job()
 * This function can receive the result to search sync jobs respectively
 * If there is sync_job_name in the result, On Demand or
 * Periodic sync job ID is stored for managing them
 * In the other case, Data Change sync job ID which is based on
 * the value of sync_capability is stored
 * Stored sync_job_id will be used to manage each sync job
 */
bool
sync_adapter_sample_foreach_sync_job_cb(account_h account, const char *sync_job_name, const char *sync_capability, int sync_job_id, bundle* sync_job_user_data, void *user_data)
{
	char sync_job_info[MAX_SIZE], *value = NULL;
	memset(sync_job_info, 0, sizeof(sync_job_info));

	/*
	 * Below text are stored in sync_job_info is used to represent their list
	 * When you press "Manage sync jobs" on main UI with registering sync job, the list is shown
	 * This function is called repeatedly until all of the sync jobs are printed
	 */
	if (sync_job_name) {
		if (!strcmp(sync_job_name, "OnDemand")) {
			on_demand_sync_job_id = sync_job_id;
			sprintf(sync_job_info, "[%d] %s", sync_job_id, strdup(sync_job_name));
		} else if (!strcmp(sync_job_name, "Periodic")) {
			bundle_get_str(sync_job_user_data, "interval", &value);
			periodic_sync_job_id = sync_job_id;
			sprintf(sync_job_info, "[%d] %s", sync_job_id, strdup(value));
		}
	} else if (sync_capability) {
		if (!strcmp(sync_capability, SYNC_SUPPORTS_CAPABILITY_CONTACT))
			sprintf(sync_job_info, "[%d] for %s", sync_job_id, strdup("Contact"));
		else if (!strcmp(sync_capability, SYNC_SUPPORTS_CAPABILITY_IMAGE))
			sprintf(sync_job_info, "[%d] for %s", sync_job_id, strdup("Image"));
		else if (!strcmp(sync_capability, SYNC_SUPPORTS_CAPABILITY_MUSIC))
			sprintf(sync_job_info, "[%d] for %s", sync_job_id, strdup("Music"));
		else if (!strcmp(sync_capability, SYNC_SUPPORTS_CAPABILITY_SOUND))
			sprintf(sync_job_info, "[%d] for %s", sync_job_id, strdup("Sound"));
		else if (!strcmp(sync_capability, SYNC_SUPPORTS_CAPABILITY_VIDEO))
			sprintf(sync_job_info, "[%d] for %s", sync_job_id, strdup("Video"));
	} else if (cnt_sync_jobs == 0) {
		return true;
	}

	int idx, temp_idx = 0;
	for (idx = 0; idx < MAX_NUM; idx++) {
		if (list_of_sync_jobs[idx][0] == '\0') {
			remove_sync_job[idx] = sync_job_id;
			temp_idx = idx;
			break;
		}
	}
	strcpy(list_of_sync_jobs[temp_idx], sync_job_info);

	/* Count the entire number of registered sync jobs */
	cnt_sync_jobs++;

	return true;
}


/*
 * Declare on_manage_sync_jobs_cb() for calling it
 * in on_select_manage_sync_job_cb()
 */
void on_manage_sync_jobs_cb(void *data, Evas_Object *obj, void *event_info);


/*
 * Callback is invoked when "Manage sync jobs" is pressed on main UI
 */
static void
on_select_manage_sync_jobs_cb(void *data, Evas_Object *obj, void *event_info)
{
	/* Flag for verifying whether user already uses a menu */
	if (is_enter) return;
	is_enter++;

	/* list_of_remove_sync_job should be initialized for storing renewed sync job list */
	memset(list_of_remove_sync_job, false, sizeof(list_of_remove_sync_job));

	/*
	 * Call below function for using sync_manager_foreach_sync_job()
	 * to search all of the registered sync jobs.
	 */
	on_manage_sync_jobs_cb(data, obj, event_info);
}


/*
 * Callback for removing all sync jobs
 */
void
on_remove_all_sync_jobs_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Enter remove all sync jobs");

	Evas_Object *nf = (Evas_Object *)data;

	/*
	 * Registered sync jobs can be removed by using sync_manager_remove_sync_job()
	 * with stored Sync Job ID in advance
	 * After removing a sync job, variable which is used to store Sync Job ID should be initialized
	 */
	int idx, idx2;
	for (idx = 0; idx < cnt_sync_jobs; idx++) {
		list_of_remove_sync_job[idx] = true;
		if (list_of_remove_sync_job[idx]) {
			sync_manager_remove_sync_job(remove_sync_job[idx]);
			list_of_remove_sync_job[idx] = false;
			dlog_print(DLOG_INFO, LOG_TAG, "Removed sync job: [%d]", remove_sync_job[idx]);
		}
		for (idx2 = 0; idx2 < NUM_OF_CAPABILITY; idx2++) {
			if (data_change_id[idx2] == remove_sync_job[idx]) {
				data_change_id[idx2] = -1;
				break;
			}
		}
		remove_sync_job[idx] = -1;
	}

	/*
	 * If "Remove" button is pressed with all item checked,
	 * all of the Data Change Sync Job IDs should be initialized
	 */
	memset(data_change_id, -1, sizeof(data_change_id));
	/*
	 * It would be good to remove account information also
	 * when all of the sync jobs which are registered through the Sync Adapter are removed
	 * Because this application can change account mode freely
	 */
	int ret = account_delete_from_db_by_package_name(SYNC_ADAPTER_APP_ID);
	if (ret == ACCOUNT_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Account DB is deleted successfully");
		if (account) {
			ret = account_destroy(account);
			if (ret == ACCOUNT_ERROR_NONE)
				dlog_print(DLOG_INFO, LOG_TAG, "Account handle is removed successfully");
		}
	}
	/* Initialize account ID which is no longer used */
	account_id = -1;

	/* Initialize Sync Job ID */
	on_demand_sync_job_id = -1;
	periodic_sync_job_id = -1;

	elm_naviframe_item_pop(nf);

	/* Call below function again to draw renewed UI after removing sync jobs */
	on_select_manage_sync_jobs_cb(data, obj, event_info);
}


/*
 * Callback for calling sync_manager_foreach_sync_job()
 * and drawing UI to contain the result on genlist.
 */
void
on_manage_sync_jobs_cb(void *data, Evas_Object *obj, void *event_info)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Enter the Manage sync jobs");

	/* app_data */
	app_data_s *ad = NULL;
	ad = (app_data_s *) g_ad;
	if (ad == NULL) return;

	Evas_Object *nf = data, *removeBtn, *genlist, *circle_genlist;
	Elm_Object_Item *nf_it, *it;
	ad->nf = nf;

	/* Elm_Genlist_Item_Class for title, list, padding */
	Elm_Genlist_Item_Class *ttc = elm_genlist_item_class_new();
	Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
	Elm_Genlist_Item_Class *ptc = elm_genlist_item_class_new();

	genlist = elm_genlist_add(ad->layout);
	elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);

	/* genlist for circular view */
	circle_genlist = eext_circle_object_genlist_add(genlist, ad->circle_surface);
	eext_circle_object_genlist_scroller_policy_set(circle_genlist, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
	eext_rotary_object_event_activated_set(circle_genlist, EINA_TRUE);

	ttc->item_style = "title";
	ttc->func.text_get = get_manage_menu_title;

	/* Append genlist item for title */
	elm_genlist_item_append(genlist, ttc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

	memset(remove_sync_job, -1, sizeof(remove_sync_job));

	int ret = sync_manager_foreach_sync_job(sync_adapter_sample_foreach_sync_job_cb, NULL);

	int idx, n_items = cnt_sync_jobs;
	for (idx = 0; idx < MAX_NUM; idx++) {
		if (list_of_sync_jobs[idx][0] != '\0')
			dlog_print(DLOG_INFO, LOG_TAG, "list_of_sync_jobs[%d] : %s", idx, list_of_sync_jobs[idx]);
	}

	itc = elm_genlist_item_class_new();
	itc->item_style = "2text";
	itc->func.text_get = get_text_registered_sync_jobs;
	itc->func.del = gl_del_cb;

	for (idx = 0; idx < n_items; idx++) {
		foreach_id = calloc(sizeof(list_item_data_s), 1);
		foreach_id->type = 6;
		foreach_id->index = idx;
		it = elm_genlist_item_append(genlist, itc, foreach_id, NULL, ELM_GENLIST_ITEM_TREE, NULL, nf);
		foreach_id->item = it;
	}

	ptc->item_style = "padding";

	/* Append genlist item for padding */
	elm_genlist_item_append(genlist, ptc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
	if (ret == SYNC_ERROR_NONE)
		evas_object_show(genlist);
	else
		dlog_print(DLOG_INFO, LOG_TAG, "sync_manager_foreach_sync_job() is failed [%d : %s]", ret, get_error_message(ret));

	elm_genlist_item_class_free(ttc);
	elm_genlist_item_class_free(itc);
	elm_genlist_item_class_free(ptc);

	/* Button for stating sync jobs */
	elm_layout_theme_set(ad->layout, "layout", "bottom_button", "default");
	removeBtn = elm_button_add(ad->nf);
	elm_object_style_set(removeBtn, "bottom");
	elm_object_text_set(removeBtn, "Remove");
	elm_object_part_content_set(ad->layout, "elm.swallow.button", removeBtn);

	evas_object_smart_callback_add(removeBtn, "clicked", on_remove_all_sync_jobs_cb, nf);

	/*
	 * Set "Remove" button for removing all items
	 * The button is enabled only when there is sync job
	 * In the opposite case, it is shown as disabled
	 */
	if (cnt_sync_jobs > 0)
		elm_object_disabled_set(removeBtn, EINA_FALSE);
	else
		elm_object_disabled_set(removeBtn, EINA_TRUE);

	evas_object_show(removeBtn);

	nf_it = elm_naviframe_item_push(nf, "Manage sync jobs", NULL, NULL, genlist, "empty");
	elm_naviframe_item_pop_cb_set(nf_it, foreach_naviframe_pop_cb, removeBtn);
}


/*
 * Draw the Sync Adapter's main menu
 */
static void
create_sync_main_menu(app_data_s *ad)
{
	/*
	 * The Sync Adapter should send request to launch the Sync Adapter Service
	 * Then, the Sync Adapter Service will set callback functions by using sync-adapter API
	 * Without below steps, the Sync Adapter can't receive the result of sync operation
	 * through the Sync Adapter Service
	 */
	app_control_h app_control;
	int ret = app_control_create(&app_control);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Creating app_control handle is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}
	/* Set the Sync Adapter Service App ID for launching it */
	ret = app_control_set_app_id(app_control, SYNC_ADAPTER_SERVICE_APP_ID);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Setting app id is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}
	/* Send application launch request */
	ret = app_control_send_launch_request(app_control, NULL, NULL);
	if (ret == APP_CONTROL_ERROR_NONE)
		dlog_print(DLOG_INFO, LOG_TAG, "Launching sync service app successfully [%s]", SYNC_ADAPTER_SERVICE_APP_ID);
	else {
		dlog_print(DLOG_INFO, LOG_TAG, "Launching sync service app is failed [%d : %s]", ret, get_error_message(ret));
		return;
	}

	/* Initialize list to show it on the menu "Manage sync jobs" */
	memset(list_of_sync_jobs, '\0', sizeof(list_of_sync_jobs));

	/* Initialize list to store data change sync job IDs */
	memset(data_change_id, -1, sizeof(data_change_id));

	Evas_Object *nf = ad->nf, *genlist, *circle_genlist;
	Elm_Object_Item *nf_it, *it;

	/* Elm_Genlist_Item_Class for title, list, padding */
	Elm_Genlist_Item_Class *ttc = elm_genlist_item_class_new();
	Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
	Elm_Genlist_Item_Class *ptc = elm_genlist_item_class_new();

	genlist = elm_genlist_add(nf);
	elm_genlist_mode_set(genlist, ELM_LIST_COMPRESS);

	/* genlist for circular view */
	circle_genlist = eext_circle_object_genlist_add(genlist, ad->circle_surface);
	eext_circle_object_genlist_scroller_policy_set(circle_genlist, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
	eext_rotary_object_event_activated_set(circle_genlist, EINA_TRUE);

	ttc->item_style = "title";
	ttc->func.text_get = get_main_menu_title;

	itc->item_style = "2text";
	itc->func.text_get = get_main_menu_text;
	itc->func.del = gl_del_cb;

	ptc->item_style = "padding";

	/* Append genlist item for title */
	elm_genlist_item_append(genlist, ttc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

	/* This logic is for appending genlist items */
	int idx;
	for (idx = 0; idx < NUM_OF_ITEMS; idx++) {
		main_id = calloc(sizeof(list_item_data_s), 1);
		main_id->index = idx;

		switch (idx) {
		case 0:
			it = elm_genlist_item_append(genlist, itc, main_id, NULL, ELM_GENLIST_ITEM_NONE, on_create_account_sync_cb, nf);
			break;
		case 1:
			it = elm_genlist_item_append(genlist, itc, main_id, NULL, ELM_GENLIST_ITEM_NONE, on_select_accountless_sync_cb, nf);
			break;
		case 2:
			it = elm_genlist_item_append(genlist, itc, main_id, NULL, ELM_GENLIST_ITEM_NONE, on_select_settings_sync_cb, nf);
			break;
		case 3:
			it = elm_genlist_item_append(genlist, itc, main_id, NULL, ELM_GENLIST_ITEM_NONE, on_select_manage_sync_jobs_cb, nf);
			break;
		default:
			break;
		}

		main_id->item = it;
	}

	/* Append genlist item for padding */
	elm_genlist_item_append(genlist, ptc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

	elm_genlist_item_class_free(ttc);
	elm_genlist_item_class_free(itc);
	elm_genlist_item_class_free(ptc);

	nf_it = elm_naviframe_item_push(nf, "Sync Adapter", NULL, NULL, genlist, "empty");
	elm_naviframe_item_pop_cb_set(nf_it, win_delete_request_cb, ad->win);
}


/*
 * Create base GUI to include main menu
 */
static void
create_base_gui(app_data_s *ad)
{
	/* Window */
	ad->win = elm_win_util_standard_add(PKG_NAME, PKG_NAME);
	elm_win_conformant_set(ad->win, EINA_TRUE);
	elm_win_autodel_set(ad->win, EINA_TRUE);

	if (elm_win_wm_rotation_supported_get(ad->win)) {
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}

	/* Conformant */
	ad->conform = elm_conformant_add(ad->win);
	elm_win_indicator_mode_set(ad->win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(ad->win, ELM_WIN_INDICATOR_OPAQUE);
	evas_object_size_hint_weight_set(ad->conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(ad->win, ad->conform);
	evas_object_show(ad->conform);

	/* Eext Circle Surface Creation */
	ad->circle_surface = eext_circle_surface_conformant_add(ad->conform);

	/* Base Layout */
	ad->layout = elm_layout_add(ad->conform);
	evas_object_size_hint_weight_set(ad->layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_layout_theme_set(ad->layout, "layout", "application", "default");
	evas_object_show(ad->layout);
	elm_object_content_set(ad->conform, ad->layout);

	/* Naviframe */
	ad->nf = elm_naviframe_add(ad->layout);
	elm_object_part_content_set(ad->layout, "elm.swallow.content", ad->nf);
	elm_naviframe_event_enabled_set(ad->nf, EINA_TRUE);
	eext_object_event_callback_add(ad->nf, EEXT_CALLBACK_BACK, eext_naviframe_back_cb, NULL);
	eext_object_event_callback_add(ad->nf, EEXT_CALLBACK_MORE, eext_naviframe_more_cb, NULL);

	/* Add sync main menu */
	create_sync_main_menu(ad);

	/* Show window after base GUI is set up */
	evas_object_show(ad->win);
}


/*
 * App create callback
 */
static bool
app_create(void *data)
{
	/*
	 * Hook to take necessary actions before main event loop starts
	 * Initialize UI resources and application's data
	 * If this function returns true, the main loop of application starts
	 * If this function returns false, the application is terminated
	 */
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : app_create is invoked");

	app_data_s *ad = data;
	create_base_gui(ad);

	return true;
}


/*
 * App terminate callback
 */
static void
app_terminate(void *data)
{
	/* Release all resources */
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : app_terminate is invoked");
	account_destroy(account);
}


/*
 * App control callback
 */
static void
app_control(app_control_h app_control, void *data)
{
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : app_control is invoked");

	/* Get app control operation */
	char *operation;
	int ret = app_control_get_operation(app_control, &operation);
	if (ret != APP_CONTROL_ERROR_NONE) {
		dlog_print(DLOG_INFO, LOG_TAG, "Get app control operation is failed [%d : %s]", ret, get_error_message(ret));
		return;
	} else {
		dlog_print(DLOG_INFO, LOG_TAG, "App Control operation [%s]", operation);
	}

	/*
	 * In the case of non-default App Control operation,
	 * it has a respective operation depending on its kind of sync jobs
	 */
	if (strcmp(operation, "http://tizen.org/appcontrol/operation/default")) {
		/*
		 * Below popup object for displaying the sort of sync jobs
		 * Pop-up is maintained during 2 seconds
		 */
		Evas_Object *win = g_ad->nf, *popup;
		popup = elm_popup_add(win);
		elm_object_style_set(popup, "toast/circle");
		elm_popup_orient_set(popup, ELM_POPUP_ORIENT_BOTTOM);
		eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, popup_hide_cb, NULL);
		evas_object_smart_callback_add(popup, "dismissed", popup_finished_cb, NULL);

		/*
		 * Sync complete message will be popped up depending on the kind of sync jobs,
		 * On demand, Periodic, Data change sync job, which come from the Sync Adapter Service
		 */
		if (operation && !strcmp(operation, "http://tizen.org/appcontrol/operation/on_demand_sync_complete"))
			elm_object_part_text_set(popup, "elm.text", "On Demand Sync is completed");
		else if (operation && !strcmp(operation, "http://tizen.org/appcontrol/operation/periodic_sync_complete"))
			elm_object_part_text_set(popup, "elm.text", "Periodic Sync is completed");
		else if (operation && !strcmp(operation, "http://tizen.org/appcontrol/operation/data_change_sync_complete"))
			elm_object_part_text_set(popup, "elm.text", "Data Change Sync is completed");

		/* Pop-up is maintained for 2 seconds */
		if (strcmp(operation, "http://tizen.org/appcontrol/operation/account/add")
			&& strcmp(operation, "http://tizen.org/appcontrol/operation/account/configure")) {
			elm_popup_timeout_set(popup, TIME_FOR_POPUP);
			evas_object_smart_callback_add(popup, "timeout", timeout_cb, NULL);
			evas_object_show(popup);
		}
	}

	dlog_print(DLOG_INFO, LOG_TAG, "Sync operation complete");
}


/*
 * App pause callback
 */
static void
app_pause(void *data)
{
	/* Take necessary actions when application becomes invisible */
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : app_pause is invoked");
}


/*
 * App resume callback
 */
static void
app_resume(void *data)
{
	/* Take necessary actions when application becomes visible */
	dlog_print(DLOG_INFO, LOG_TAG, "Sync Adapter : app_resume is invoked");
}


/*
 * App low battery callback
 */
static void
ui_app_low_battery(app_event_info_h event_info, void *user_data)
{
	/* APP_EVENT_LOW_BATTERY */
}


/*
 * App low memory callback
 */
static void
ui_app_low_memory(app_event_info_h event_info, void *user_data)
{
	/* APP_EVENT_LOW_MEMORY */
}


/*
 * App orientation changed callback
 */
static void
ui_app_orient_changed(app_event_info_h event_info, void *user_data)
{
	/* APP_EVENT_DEVICE_ORIENTATION_CHANGED */
	return;
}


/*
 * App language changed callback
 */
static void
ui_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/* APP_EVENT_LANGUAGE_CHANGED */
	char *locale = NULL;
	system_settings_get_value_string(SYSTEM_SETTINGS_KEY_LOCALE_LANGUAGE, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}


/*
 * App region format changed callback
 */
static void
ui_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/* APP_EVENT_REGION_FORMAT_CHANGED */
}


/*
 * Sync Adapter main()
 */
int main(int argc, char *argv[])
{
	app_data_s ad = {0,};
	int ret = 0;

	/* Life cycle callback structure */
	ui_app_lifecycle_callback_s event_callback = {0,};
	/* Event handler */
	app_event_handler_h handlers[5] = {NULL, };

	/* Set event callback functions */
	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;

	/* Set low battery event */
	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_BATTERY], APP_EVENT_LOW_BATTERY, ui_app_low_battery, &ad);
	/* Set low memory event */
	ui_app_add_event_handler(&handlers[APP_EVENT_LOW_MEMORY], APP_EVENT_LOW_MEMORY, ui_app_low_memory, &ad);
	/* Set orientation changed event */
	ui_app_add_event_handler(&handlers[APP_EVENT_DEVICE_ORIENTATION_CHANGED], APP_EVENT_DEVICE_ORIENTATION_CHANGED, ui_app_orient_changed, &ad);
	/* Set language changed event */
	ui_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED], APP_EVENT_LANGUAGE_CHANGED, ui_app_lang_changed, &ad);
	/* Set region format changed event */
	ui_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED], APP_EVENT_REGION_FORMAT_CHANGED, ui_app_region_changed, &ad);
	ui_app_remove_event_handler(handlers[APP_EVENT_LOW_MEMORY]);

	g_ad = &ad;
	ret = ui_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "ui_app_main() is failed = [%d : %s]", ret, get_error_message(ret));

	return ret;
}
