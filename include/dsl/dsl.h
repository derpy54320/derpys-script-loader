/* DERPY'S SCRIPT LOADER
	
	this is the main header for dsl
	it includes headers for all main components
	define DSL_SERVER_VERSION before including for the server version
	
	define DSL_DISABLE_SYSTEM_ACCESS to not support system access
	define DSL_DISABLE_ZIP_FILES to not support zip file loading (and not need libzip / dependencies)
	
	the client can only be compiled on Windows (obviously, because the game is for Windows)
	the server can be compiled on Windows or using GCC
	
*/

#ifndef DSL_MAIN_H
#define DSL_MAIN_H

// DSL VERSION
#define DSL_VERSION 9
#define DSL_VERSION_LUA 9.0f
#define DSL_VERSION_NET "dss9dev"
#define DSL_VERSION_TEXT "version 9 - dev"

// VERSION NAME
#ifdef DSL_DISABLE_SYSTEM_ACCESS
#define DSL_VERSION_TEXT_2 DSL_VERSION_TEXT " - S"
#else
#define DSL_VERSION_TEXT_2 DSL_VERSION_TEXT
#endif

// LUA SETTINGS
#define DSL_REGISTRY_KEY "dsl"

// SIGNATURE TEXT
#ifdef DSL_SERVER_VERSION
#define DSL_SIGNATURE_TEXT "derpy's script server: " DSL_VERSION_TEXT_2 // *
#else
#define DSL_SIGNATURE_TEXT "derpy's script loader: " DSL_VERSION_TEXT_2 // *
#endif

// SYSTEM COMMAMDS
#ifdef _WIN32
#define DSL_SYSTEM_CLS "CLS" // *
#define DSL_SYSTEM_PAUSE "PAUSE" // *
#else
#define DSL_SYSTEM_CLS "clear" // *
//#define DSL_SYSTEM_PAUSE "read -p \"Press any key to continue . . . \"" // *
#endif

// ROOT PATH
#ifdef DSL_SERVER_VERSION
#define DSL_ROOT_PREFIX ""
#else
#define DSL_ROOT_PREFIX "_derpy_script_loader/"
#endif

// FILE PATHS
#define DSL_CACHE_PATH DSL_ROOT_PREFIX "cache/" // just for cleanup
#define DSL_LOGGING_PATH DSL_ROOT_PREFIX "logs/" // created as needed
#define DSL_PACKAGE_PATH DSL_ROOT_PREFIX "packages/"
#define DSL_SCRIPTS_PATH DSL_ROOT_PREFIX "scripts/"
#ifdef DSL_SERVER_VERSION
#define DSL_CLIENT_SCRIPTS_PATH DSL_ROOT_PREFIX "cache/" // created as needed
#else
#define DSL_CONTENT_PATH DSL_ROOT_PREFIX "cache/" // created as needed
#define DSL_SERVER_SCRIPTS_PATH DSL_ROOT_PREFIX "cache/server/" // created as needed
#endif

// FILE NAMES
// TODO: rename the main config file since people can't read
#define DSL_CONFIG_FILE DSL_ROOT_PREFIX "config.txt" // *
#define DSL_SCRIPT_CONFIG "config.txt" // collection relative
#ifndef DSL_SERVER_VERSION
#define DSL_SIGNATURE_PNG DSL_ROOT_PREFIX "signature.png" // *
#define DSL_SERVER_ICON_FILE DSL_ROOT_PREFIX "cache/server.png"
#endif

// COMMON HEADERS
#include <stdio.h> // standard
#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN // windows
#include <windows.h>
#include <winsock2.h>
#else
#include "wcompatibility.h" // linux (also includes unistd.h)
#include <linux/limits.h>
#include <sys/socket.h>
#include <errno.h>
#endif
#ifndef DSL_SERVER_VERSION
#define DIRECTINPUT_VERSION 0x0800 // directx
#include <dinput.h>
#include <d3d9.h>
#endif
#ifdef __cplusplus // lua
#define LUA_API extern "C"
#endif
#define LUA_NUMBER float
#include <lua.h>
#include <lauxlib.h>
#include <zip.h> // miscellaneous

// DSL KEYBOARD
#define DSL_KEY_RELEASED 0
#define DSL_KEY_PRESSED 1 // bitwise & to check if pressed or being pressed
#define DSL_KEY_JUST_RELEASED 2
#define DSL_KEY_JUST_PRESSED 3

// DSL FLAGS
#define DSL_SYSTEM_ACCESS 1
#define DSL_SERVER_ACCESS 2
#define DSL_ADD_DEBUG_FUNCS 4
#define DSL_ADD_REBUILD_FUNC 8
#define DSL_SHOWN_ERROR_WINDOW 16
#define DSL_CONNECTED_TO_SERVER 32 // disables some stuff permanently
#define DSL_USED_TEXT_REGISTER 64 // show a warning since this feature is buggy
#define DSL_WARN_SYSTEM_S 128 // system access was enabled but is not supported
#define DSL_WARN_SYSTEM_N 256 // system access was enabled but is not allowed
#define DSL_SHOW_TICK_WARNINGS 512
#define DSL_RUN_MAIN_LOOP 1024

// DSL STATE
typedef struct dsl_state{
	// components
	void *game; // NULL during init stage
	lua_State *lua;
	struct config_file *config;
	#ifndef DSL_SERVER_VERSION
	struct render_state *render; // NULL during pre-init stage
	struct script_content *content; // NULL once no longer needed
	#endif
	struct script_events *events;
	struct script_console *console;
	struct script_command **cmdlist;
	struct script_manager *manager;
	struct script_loader *loader;
	struct network_state *network;
	
	// input
	#ifndef DSL_SERVER_VERSION
	char keyboard[0x100];
	DIMOUSESTATE mouse;
	DIMOUSESTATE mouse2; // last mouse update
	#endif
	
	// timing
	#ifdef DSL_SERVER_VERSION
	DWORD frame_time;
	DWORD last_frame;
	DWORD t_wrapped;
	#else
	unsigned frame_time;
	unsigned last_frame;
	#endif
	
	// miscellaneous
	int flags;
	#ifndef DSL_SERVER_VERSION
	int dwrite_cycle;
	int show_console;
	int console_key; // directinput key
	int save_data; // lua registry reference
	#endif
	int dsl_data; // lua registry reference for passing data between pre-init scripts and main scripts
}dsl_state;

// GLOBAL STATE
#ifdef DSL_SERVER_VERSION
extern dsl_state *g_dsl; // getGameTimer needs this
#endif

// DSL COMPONENTS
#include "text.h"
#include "utility.h"
#include "serialize.h"
#include "config.h"
#include "events.h"
#include "command.h"
#include "manager.h"
#include "loader.h"
#include "content.h"
#include "network.h"
#ifdef DSL_SERVER_VERSION
#include "server/sv_game.h"
#include "server/sv_console.h"
#else
#include "client/game.h"
#include "client/render.h"
#include "client/console.h"
#endif

// DSL DEBUGGING
//#include "memdebug.h"

#endif
