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
#define _strtok(cntx, delim) strtok_s(cntx, delim, &cntx)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#define _strtok(cntx, delim) strtok(cntx, delim)
#endif

#define PLUGIN_API_VERSION 26

#define GKEY_ID_BUFSIZE 64
#define GKEY_MOUSE_ID "mouse"
#define GKEY_KEYBOARD_ID "keybd"

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
    return "G-Key Plugin";
}

/* Plugin version */
const char* ts3plugin_version() {
    return "1.1";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
    return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
    return "Jules Blok";
}

/* Plugin description */
const char* ts3plugin_description() {
    /* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "This plugin provides support for Logitech devices with G-Keys for hotkeys.";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

void __cdecl GkeySDKCallback(GkeyCode gkeyCode, wchar_t* gkeyOrButtonString, void* context)
{
    // Construct our own consistent identifier
    char keyId[GKEY_ID_BUFSIZE];
    snprintf(keyId, GKEY_ID_BUFSIZE, "%s-g%d-m%d", gkeyCode.mouse ? "mouse" : "keybd", gkeyCode.keyIdx, gkeyCode.mState);

    // Notify Teamspeak of the G-Key event
    // For the up_down parameter 1 = up and 0 = down, so invert it
    ts3Functions.notifyKeyEvent(pluginID, keyId, !gkeyCode.keyDown);
}

/*
* Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
* If the function returns 1 on failure, the plugin will be unloaded again.
*/
int ts3plugin_init() {
    logiGkeyCBContext gkeyContext;
    ZeroMemory(&gkeyContext, sizeof(gkeyContext));
    gkeyContext.gkeyCallBack = (logiGkeyCB)GkeySDKCallback;
    gkeyContext.gkeyContext = NULL;

    LogiGkeyInit(&gkeyContext);

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
               /* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
               * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
               * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
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
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
    return NULL;
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) {
    return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
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
    if (strstr(keyIdentifier, GKEY_MOUSE_ID))
        return "Logitech Mouse";
    else
        return "Logitech Keyboard";
}

GkeyCode GkeyIdentifierToCode(const char* keyIdentifier) {
    // Allocate a string we can modify
    size_t len = strlen(keyIdentifier) + 1;
    char* str = (char*)malloc(len);
    _strcpy(str, len, keyIdentifier);

    // Tokenize the string
    char* cntx = str;
    char* device = _strtok(cntx, "-");
    char* gkey = _strtok(cntx, "-");
    char* mkey = _strtok(cntx, "-");

    // Construct the gkey code
    GkeyCode code = { 0 };
    if (device)
        code.mouse = (strcmp(device, GKEY_MOUSE_ID) == 0) ? 1 : 0;
    if (gkey)
        code.keyIdx = atoi(gkey + 1);
    if (mkey)
        code.mState = atoi(mkey + 1);

    // Cleanup and return
    free(str);
    return code;
}

// This function translates the given key identifier to a friendly key name for display in the UI
const char* ts3plugin_displayKeyText(const char* keyIdentifier) {
    GkeyCode code = GkeyIdentifierToCode(keyIdentifier);

    wchar_t* text = NULL;
    if (code.mouse)
        text = LogiGkeyGetMouseButtonString(code.keyIdx);
    else
        text = LogiGkeyGetKeyboardGkeyString(code.keyIdx, code.mState);

    /* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
    static char* result = NULL;  /* Static variable so it's allocated only once */
    if (result)
        free(result);
    if (wcharToUtf8(text, &result) == -1) /* Convert name into UTF-8 encoded result */
        return keyIdentifier;  /* Conversion failed, fallback here */
    return result;
}

// This is used internally as a prefix for hotkeys so we can store them without collisions.
// Should be unique across plugins.
const char* ts3plugin_keyPrefix() {
    return "gkey";
}
