/*
* TeamSpeak 3 demo plugin
*
* Copyright (c) 2008-2017 TeamSpeak Systems GmbH
*/

#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "teamspeak/clientlib_publicdefinitions.h"
#include "ts3_functions.h"
#include "plugin.h"

#include "LogitechGkeyLib.h"
#pragma comment(lib, "LogitechGkeyLib.lib")

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 22

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
    int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
    *result = (char*)malloc(outlen);
    if (WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
        *result = NULL;
        return -1;
    }
    return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
* If any of these required functions is not implemented, TS3 will refuse to load the plugin
*/

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
    /* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
    static char* result = NULL;  /* Static variable so it's allocated only once */
    if (!result) {
        const wchar_t* name = L"G-Key Plugin";
        if (wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
            result = "G-Key Plugin";  /* Conversion failed, fallback here */
        }
    }
    return result;
#else
    return "G-Key Plugin";
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
    return "1.0";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
    return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
    /* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Armada";
}

/* Plugin description */
const char* ts3plugin_description() {
    /* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "This plugin provides support for Logitech devices with G-Key support for hotkeys.";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

void __cdecl GkeySDKCallback(GkeyCode gkeyCode, wchar_t* gkeyOrButtonString, void* context)
{
    // Notify Teamspeak of the G-Key event
    char* id = NULL;
    if (wcharToUtf8(gkeyOrButtonString, &id) == 0) {
        // For the up_down parameter 1 = up and 0 = down, so invert it
        ts3Functions.notifyKeyEvent(pluginID, id, !gkeyCode.keyDown);
    }
    free(id);
}

/*
* Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
* If the function returns 1 on failure, the plugin will be unloaded again.
*/
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
    char pluginPath[PATH_BUFSIZE];

    /* Your plugin init code here */
    printf("PLUGIN: init\n");
    logiGkeyCBContext gkeyContext;
    ZeroMemory(&gkeyContext, sizeof(gkeyContext));
    gkeyContext.gkeyCallBack = (logiGkeyCB)GkeySDKCallback;
    gkeyContext.gkeyContext = NULL;

    LogiGkeyInit(&gkeyContext);

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
    ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

    printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
               /* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
               * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
               * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");
    LogiGkeyShutdown();

    /*
    * Note:
    * If your plugin implements a settings dialog, it must be closed and deleted here, else the
    * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
    */

    /* Free pluginID if we registered it */
    if (pluginID) {
        free(pluginID);
        pluginID = NULL;
    }
}

/****************************** Optional functions ********************************/
/*
* Following functions are optional, if not needed you don't need to implement them.
*/

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
    printf("PLUGIN: offersConfigure\n");
    /*
    * Return values:
    * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
    * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
    * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
    */
    return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
    printf("PLUGIN: configure\n");
}

/*
* If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
* automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
* Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
*/
void ts3plugin_registerPluginID(const char* id) {
    const size_t sz = strlen(id) + 1;
    pluginID = (char*)malloc(sz * sizeof(char));
    _strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
    printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
    return NULL;
}

static void print_and_free_bookmarks_list(struct PluginBookmarkList* list)
{
    int i;
    for (i = 0; i < list->itemcount; ++i) {
        if (list->items[i].isFolder) {
            printf("Folder: name=%s\n", list->items[i].name);
            print_and_free_bookmarks_list(list->items[i].folder);
            ts3Functions.freeMemory(list->items[i].name);
        }
        else {
            printf("Bookmark: name=%s uuid=%s\n", list->items[i].name, list->items[i].uuid);
            ts3Functions.freeMemory(list->items[i].name);
            ts3Functions.freeMemory(list->items[i].uuid);
        }
    }
    ts3Functions.freeMemory(list);
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
    char buf[COMMAND_BUFSIZE];
    char *s, *param1 = NULL, *param2 = NULL;
    int i = 0;
    enum { CMD_NONE = 0, CMD_JOIN, CMD_COMMAND, CMD_SERVERINFO, CMD_CHANNELINFO, CMD_AVATAR, CMD_ENABLEMENU, CMD_SUBSCRIBE, CMD_UNSUBSCRIBE, CMD_SUBSCRIBEALL, CMD_UNSUBSCRIBEALL, CMD_BOOKMARKSLIST } cmd = CMD_NONE;
#ifdef _WIN32
    char* context = NULL;
#endif

    printf("PLUGIN: process command: '%s'\n", command);

    _strcpy(buf, COMMAND_BUFSIZE, command);
#ifdef _WIN32
    s = strtok_s(buf, " ", &context);
#else
    s = strtok(buf, " ");
#endif
    while (s != NULL) {
        if (i == 0) {
            if (!strcmp(s, "join")) {
                cmd = CMD_JOIN;
            }
            else if (!strcmp(s, "command")) {
                cmd = CMD_COMMAND;
            }
            else if (!strcmp(s, "serverinfo")) {
                cmd = CMD_SERVERINFO;
            }
            else if (!strcmp(s, "channelinfo")) {
                cmd = CMD_CHANNELINFO;
            }
            else if (!strcmp(s, "avatar")) {
                cmd = CMD_AVATAR;
            }
            else if (!strcmp(s, "enablemenu")) {
                cmd = CMD_ENABLEMENU;
            }
            else if (!strcmp(s, "subscribe")) {
                cmd = CMD_SUBSCRIBE;
            }
            else if (!strcmp(s, "unsubscribe")) {
                cmd = CMD_UNSUBSCRIBE;
            }
            else if (!strcmp(s, "subscribeall")) {
                cmd = CMD_SUBSCRIBEALL;
            }
            else if (!strcmp(s, "unsubscribeall")) {
                cmd = CMD_UNSUBSCRIBEALL;
            }
            else if (!strcmp(s, "bookmarkslist")) {
                cmd = CMD_BOOKMARKSLIST;
            }
        }
        else if (i == 1) {
            param1 = s;
        }
        else {
            param2 = s;
        }
#ifdef _WIN32
        s = strtok_s(NULL, " ", &context);
#else
        s = strtok(NULL, " ");
#endif
        i++;
    }

    switch (cmd) {
    case CMD_NONE:
        return 1;  /* Command not handled by plugin */
    case CMD_JOIN:  /* /test join <channelID> [optionalCannelPassword] */
        if (param1) {
            uint64 channelID = (uint64)atoi(param1);
            char* password = param2 ? param2 : "";
            char returnCode[RETURNCODE_BUFSIZE];
            anyID myID;

            /* Get own clientID */
            if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
                ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                break;
            }

            /* Create return code for requestClientMove function call. If creation fails, returnCode will be NULL, which can be
            * passed into the client functions meaning no return code is used.
            * Note: To use return codes, the plugin needs to register a plugin ID using ts3plugin_registerPluginID */
            ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);

            /* In a real world plugin, the returnCode should be remembered (e.g. in a dynamic STL vector, if it's a C++ plugin).
            * onServerErrorEvent can then check the received returnCode, compare with the remembered ones and thus identify
            * which function call has triggered the event and react accordingly. */

            /* Request joining specified channel using above created return code */
            if (ts3Functions.requestClientMove(serverConnectionHandlerID, myID, channelID, password, returnCode) != ERROR_ok) {
                ts3Functions.logMessage("Error requesting client move", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
            }
        }
        else {
            ts3Functions.printMessageToCurrentTab("Missing channel ID parameter.");
        }
        break;
    case CMD_COMMAND:  /* /test command <command> */
        if (param1) {
            /* Send plugin command to all clients in current channel. In this case targetIds is unused and can be NULL. */
            if (pluginID) {
                /* See ts3plugin_registerPluginID for how to obtain a pluginID */
                printf("PLUGIN: Sending plugin command to current channel: %s\n", param1);
                ts3Functions.sendPluginCommand(serverConnectionHandlerID, pluginID, param1, PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);
            }
            else {
                printf("PLUGIN: Failed to send plugin command, was not registered.\n");
            }
        }
        else {
            ts3Functions.printMessageToCurrentTab("Missing command parameter.");
        }
        break;
    case CMD_SERVERINFO: {  /* /test serverinfo */
                            /* Query host, port and server password of current server tab.
                            * The password parameter can be NULL if the plugin does not want to receive the server password.
                            * Note: Server password is only available if the user has actually used it when connecting. If a user has
                            * connected with the permission to ignore passwords (b_virtualserver_join_ignore_password) and the password,
                            * was not entered, it will not be available.
                            * getServerConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
        char host[SERVERINFO_BUFSIZE];
        /*char password[SERVERINFO_BUFSIZE];*/
        char* password = NULL;  /* Don't receive server password */
        unsigned short port;
        if (!ts3Functions.getServerConnectInfo(serverConnectionHandlerID, host, &port, password, SERVERINFO_BUFSIZE)) {
            char msg[SERVERINFO_BUFSIZE];
            snprintf(msg, sizeof(msg), "Server Connect Info: %s:%d", host, port);
            ts3Functions.printMessageToCurrentTab(msg);
        }
        else {
            ts3Functions.printMessageToCurrentTab("No server connect info available.");
        }
        break;
    }
    case CMD_CHANNELINFO: {  /* /test channelinfo */
                             /* Query channel path and password of current server tab.
                             * The password parameter can be NULL if the plugin does not want to receive the channel password.
                             * Note: Channel password is only available if the user has actually used it when entering the channel. If a user has
                             * entered a channel with the permission to ignore passwords (b_channel_join_ignore_password) and the password,
                             * was not entered, it will not be available.
                             * getChannelConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
        char path[CHANNELINFO_BUFSIZE];
        /*char password[CHANNELINFO_BUFSIZE];*/
        char* password = NULL;  /* Don't receive channel password */

                                /* Get own clientID and channelID */
        anyID myID;
        uint64 myChannelID;
        if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            break;
        }
        /* Get own channel ID */
        if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &myChannelID) != ERROR_ok) {
            ts3Functions.logMessage("Error querying channel ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            break;
        }

        /* Get channel connect info of own channel */
        if (!ts3Functions.getChannelConnectInfo(serverConnectionHandlerID, myChannelID, path, password, CHANNELINFO_BUFSIZE)) {
            char msg[CHANNELINFO_BUFSIZE];
            snprintf(msg, sizeof(msg), "Channel Connect Info: %s", path);
            ts3Functions.printMessageToCurrentTab(msg);
        }
        else {
            ts3Functions.printMessageToCurrentTab("No channel connect info available.");
        }
        break;
    }
    case CMD_AVATAR: {  /* /test avatar <clientID> */
        char avatarPath[PATH_BUFSIZE];
        anyID clientID = (anyID)atoi(param1);
        unsigned int error;

        memset(avatarPath, 0, PATH_BUFSIZE);
        error = ts3Functions.getAvatar(serverConnectionHandlerID, clientID, avatarPath, PATH_BUFSIZE);
        if (error == ERROR_ok) {  /* ERROR_ok means the client has an avatar set. */
            if (strlen(avatarPath)) {  /* Avatar path contains the full path to the avatar image in the TS3Client cache directory */
                printf("Avatar path: %s\n", avatarPath);
            }
            else { /* Empty avatar path means the client has an avatar but the image has not yet been cached. The TeamSpeak
                   * client will automatically start the download and call onAvatarUpdated when done */
                printf("Avatar not yet downloaded, waiting for onAvatarUpdated...\n");
            }
        }
        else if (error == ERROR_database_empty_result) {  /* Not an error, the client simply has no avatar set */
            printf("Client has no avatar\n");
        }
        else { /* Other error occured (invalid server connection handler ID, invalid client ID, file io error etc) */
            printf("Error getting avatar: %d\n", error);
        }
        break;
    }
    case CMD_ENABLEMENU:  /* /test enablemenu <menuID> <0|1> */
        if (param1) {
            int menuID = atoi(param1);
            int enable = param2 ? atoi(param2) : 0;
            ts3Functions.setPluginMenuEnabled(pluginID, menuID, enable);
        }
        else {
            ts3Functions.printMessageToCurrentTab("Usage is: /test enablemenu <menuID> <0|1>");
        }
        break;
    case CMD_SUBSCRIBE:  /* /test subscribe <channelID> */
        if (param1) {
            char returnCode[RETURNCODE_BUFSIZE];
            uint64 channelIDArray[2];
            channelIDArray[0] = (uint64)atoi(param1);
            channelIDArray[1] = 0;
            ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
            if (ts3Functions.requestChannelSubscribe(serverConnectionHandlerID, channelIDArray, returnCode) != ERROR_ok) {
                ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
            }
        }
        break;
    case CMD_UNSUBSCRIBE:  /* /test unsubscribe <channelID> */
        if (param1) {
            char returnCode[RETURNCODE_BUFSIZE];
            uint64 channelIDArray[2];
            channelIDArray[0] = (uint64)atoi(param1);
            channelIDArray[1] = 0;
            ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
            if (ts3Functions.requestChannelUnsubscribe(serverConnectionHandlerID, channelIDArray, NULL) != ERROR_ok) {
                ts3Functions.logMessage("Error unsubscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
            }
        }
        break;
    case CMD_SUBSCRIBEALL: {  /* /test subscribeall */
        char returnCode[RETURNCODE_BUFSIZE];
        ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
        if (ts3Functions.requestChannelSubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
            ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
        }
        break;
    }
    case CMD_UNSUBSCRIBEALL: {  /* /test unsubscribeall */
        char returnCode[RETURNCODE_BUFSIZE];
        ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
        if (ts3Functions.requestChannelUnsubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
            ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
        }
        break;
    }
    case CMD_BOOKMARKSLIST: {  /* test bookmarkslist */
        struct PluginBookmarkList* list;
        unsigned int error = ts3Functions.getBookmarkList(&list);
        if (error == ERROR_ok) {
            print_and_free_bookmarks_list(list);
        }
        else {
            ts3Functions.logMessage("Error getting bookmarks list", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        }
        break;
    }
    }

    return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
    printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}

/*
* Implement the following three functions when the plugin should display a line in the server/channel/client info.
* If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
*/

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
    return "Test plugin info";
}

/*
* Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
* function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
* Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
* "data" to NULL to have the client ignore the info data.
*/
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) {
    char* name;

    /* For demonstration purpose, display the name of the currently selected server, channel or client. */
    switch (type) {
    case PLUGIN_SERVER:
        if (ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name) != ERROR_ok) {
            printf("Error getting virtual server name\n");
            return;
        }
        break;
    case PLUGIN_CHANNEL:
        if (ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, id, CHANNEL_NAME, &name) != ERROR_ok) {
            printf("Error getting channel name\n");
            return;
        }
        break;
    case PLUGIN_CLIENT:
        if (ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_NICKNAME, &name) != ERROR_ok) {
            printf("Error getting client nickname\n");
            return;
        }
        break;
    default:
        printf("Invalid item type: %d\n", type);
        data = NULL;  /* Ignore */
        return;
    }

    *data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));  /* Must be allocated in the plugin! */
    snprintf(*data, INFODATA_BUFSIZE, "The nickname is [I]\"%s\"[/I]", name);  /* bbCode is supported. HTML is not supported */
    ts3Functions.freeMemory(name);
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
    free(data);
}

/*
* Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
* the user manually disabled it in the plugin dialog.
* This function is optional. If missing, no autoload is assumed.
*/
int ts3plugin_requestAutoload() {
    return 0;  /* 1 = request autoloaded, 0 = do not request autoload */
}

/************************** TeamSpeak callbacks ***************************/
/*
* Following functions are optional, feel free to remove unused callbacks.
* See the clientlib documentation for details on each function.
*/

/* Clientlib */

// This function receives your key Identifier you send to notifyKeyEvent and should return
// the friendly device name of the device this hotkey originates from. Used for display in UI.
const char* ts3plugin_keyDeviceName(const char* keyIdentifier) {
    if (strstr(keyIdentifier, "Mouse"))
        return "Logitech Mouse";
    else
        return "Logitech Keyboard";
}

// This function translates the given key identifier to a friendly key name for display in the UI
const char* ts3plugin_displayKeyText(const char* keyIdentifier) {
    // The Logitech G-Key SDK already uses a friendly name
    return keyIdentifier;
}

// This is used internally as a prefix for hotkeys so we can store them without collisions.
// Should be unique across plugins.
const char* ts3plugin_keyPrefix() {
    return "gkey";
}
