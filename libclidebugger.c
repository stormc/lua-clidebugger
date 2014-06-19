/************************************************************************
* Readline integration for the Lua clidebugger
* (https://github.com/ToddWegner/clidebugger)
*
* (c) 2014 Christian Storm <Christian.Storm@tngtech.com>
*
* Based on the Lua readline completion for the Lua standalone interpreter
* (https://github.com/rrthomas/lua-rlcompleter)
*
* (c) 2011 Reuben Thomas <rrt@sc3d.org>
* (c) 2007 Steve Donovan
* (c) 2004 Jay Carlson
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
************************************************************************/
#define _XOPEN_SOURCE 500 // Make POSIX' fileno() available
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <readline/readline.h>
#include <readline/history.h>

// silence gcc's warnings about unused variables via gcc-specific attribute.
#ifdef __GNUC__
    #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
    #define UNUSED(x) UNUSED_ ## x
#endif

// compatibility to Lua 5.1
#if LUA_VERSION_NUM == 501
    #define lua_rawlen(L, index) lua_objlen(L, index)
    #define luaL_setfuncs(L, lib, nup) luaL_register(L, NULL, lib)
#endif

#define LUA_WORD_BREAK_CHARACTERS " \t\n\"\\'><=;:+-*/%^~#{}()[].,"
#define DEF_WORD_BREAK_CHARACTERS " \t\n\"\\'`@$><=;|&{("

// static copy of Lua state, as readline has no per-use state
static lua_State *storedL;

static int f_readline(lua_State* L) {
    const char* prompt = lua_tostring(L, 1);
    char* line = readline(prompt);
    lua_pushstring(L, line);
    if (line != NULL) { free(line); } // free readline's return value
    return 1;
}

// this function is called repeatedly by rl_completion_matches() inside
// do_completion(), each time returning one element from the Lua table
static char* iterator(const char* UNUSED(text), int state) {
    size_t len;
    const char *str;
    char *result;
    lua_rawgeti(storedL, -1, state + 1);
    if (lua_isnil(storedL, -1)) { return NULL; }
    str = lua_tolstring(storedL, -1, &len);
    result = malloc(len + 1);
    strcpy(result, str);
    lua_pop(storedL, 1);
    return result;
}

// awkward construct to convert function to void* in order to silence gcc's
// "warning: ISO C forbids conversion of object pointer to function pointer type".
typedef char* iterator_t(const char* text, int state);
union iterator_t_union {
    void *ptr;
    iterator_t* func;
};
static void* iterator_func_to_ptr(iterator_t * func) {
    union iterator_t_union x;
    x.func = func;
    return (x.ptr);
}

// this function is called by readline() whenever the user wants a completion
static char **do_completion(const char *text, int start, int end) {
    int oldtop = lua_gettop(storedL);
    char **matches = NULL;

    lua_pushlightuserdata(storedL, iterator_func_to_ptr(iterator));
    lua_gettable(storedL, LUA_REGISTRYINDEX);

    // rl_completion_append_character is not appended to a single
    // completion alternative at the end of the input line,
    // this is done by Lua returning according completions
    rl_completion_suppress_append = 1;

    if (lua_isfunction(storedL, -1)) {
        lua_pushstring(storedL, text);
        lua_pushstring(storedL, rl_line_buffer);
        lua_pushinteger(storedL, start + 1);
        lua_pushinteger(storedL, end + 1);
        if (!lua_pcall(storedL, 4, 1, 0) && lua_istable(storedL, -1)) {
            matches = rl_completion_matches(text, iterator);
        }
    }
    lua_settop(storedL, oldtop);
    return matches;
}

static int f_setcompleter(lua_State *L) {
    if (!lua_isfunction(L, 1)) {
        const char *msg = lua_pushfstring(L, "function expected, got %s", luaL_typename(L, 1));
        luaL_argerror(L, 1, msg);
    }
    lua_pushlightuserdata(L, iterator_func_to_ptr((iterator)));
    lua_pushvalue(L, 1);
    lua_settable(L, LUA_REGISTRYINDEX);
    return 0;
}

static int f_parse_and_bind(lua_State *L) {
    const char* line = lua_tostring(L, 1);
    rl_parse_and_bind((char *)line);
    return 0;
}

static int f_redisplay(lua_State *UNUSED(L)) {
    rl_forced_update_display();
    return 0;
}

static int f_add_history(lua_State* L) {
    if (lua_rawlen(L, 1) > 0) { add_history(lua_tostring(L, 1)); }
    return 0;
}

static int f_read_history(lua_State *L) {
    const char* file = lua_tostring(L, 1);
    if (read_history(file) != 0) { luaL_error(L, "reading history from file %s failed", file); }
    lua_pushboolean(L, 1);
    return 1;
}

static int f_write_history(lua_State *L) {
    const char* file = lua_tostring(L, 1);
    if (write_history(file) != 0) { luaL_error(L, "writing history from file %s failed", file); }
    lua_pushboolean(L, 1);
    return 1;
}

static int f_setprompt(lua_State* L) {
    const char* prompt = lua_tostring(L, 1);
    rl_set_prompt(prompt);
    return 0;
}

static int f_isatty(lua_State *L) {
    FILE **fp = (FILE **) luaL_checkudata(L, -1, LUA_FILEHANDLE);
    lua_pushboolean(L, isatty(fileno(*fp)));
    return 1;
}

static char *_lua_compl_word_breaks() {
    return strdup(LUA_WORD_BREAK_CHARACTERS);
}

static int f_lua_compl(lua_State *UNUSED(L)) {
    rl_attempted_completion_over = 1;
    rl_completion_word_break_hook = _lua_compl_word_breaks;
    return 0;
}

static char *_default_completion_word_breaks() {
    return strdup(DEF_WORD_BREAK_CHARACTERS);
}

static int f_filename_compl(lua_State *UNUSED(L)) {
    rl_attempted_completion_over = 0;
    rl_completion_word_break_hook = _default_completion_word_breaks;
    return 0;
}

static inline int strendswith(const char *str, const char *suffix) {
    if (!str || !suffix) { return 0; }
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr) { return 0; }
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

static inline int is_directory(const char *fname) {
    struct stat sb;
    return ( (stat(fname, &sb) == 0) && S_ISDIR(sb.st_mode) );
}

// when using readline's file completion, ignore any completion matches
// that are neither files having the extension '.lua' nor directories
static int filename_completion_ignore(char **matches) {
    unsigned int midx, fidx;
    char **filteredmatches;

    if (matches[1] == (char *)0) {
        // single completion case as matches[1] is of length 0,
        // the sole completion match is in matches[0]
        if ( !strendswith(matches[0], ".lua") && !is_directory(matches[0]) ) {
            free(matches[0]);
            matches[0] = (char *)NULL;
        }
        return 0;
    }

    for (fidx = 1; matches[fidx]; fidx++);
    filteredmatches = malloc((fidx+1) * sizeof (char *));
    filteredmatches[0] = matches[0];
    for (midx = fidx = 1; matches[midx]; midx++) {
        if ( strendswith(matches[midx], ".lua") || is_directory(matches[midx]) ) {
            filteredmatches[fidx++] = matches[midx];
        } else {
            free(matches[midx]);
            matches[midx] = NULL;
        }
    }
    filteredmatches[fidx] = (char *)NULL;

    if (fidx == 1) {
        // after matches[] is filtered, no completion matches remain
        free(filteredmatches);
        free(matches[0]);
        matches[0] = (char *)NULL;
        return 0;
    }

    if (fidx == 2) {
        // after matches[] is filtered, exactly one completion match remains
        free(matches[0]);
        matches[0] = filteredmatches[1];
        matches[1] = (char *)NULL;
        free(filteredmatches);
        return 0;
    }

    // more than one completion match remains, copy the references back
    // to the original matches[] and terminate it properly
    for (fidx = 1; filteredmatches[fidx]; fidx++) {
        matches[fidx] = filteredmatches[fidx];
    }
    matches[fidx] = (char *)NULL;
    free(filteredmatches);
    return 0;
}

static const luaL_Reg lib[] = {
    { "_set",          f_setcompleter   },
    { "setprompt",     f_setprompt      },
    { "readline",      f_readline       },
    { "redisplay",     f_redisplay      },
    { "add_history",   f_add_history    },
    { "read_history",  f_read_history   },
    { "write_history", f_write_history  },
    { "bindkey",       f_parse_and_bind },
    { "isatty",        f_isatty         },
    { "filecompl",     f_filename_compl },
    { "luacompl",      f_lua_compl      },
    { NULL, NULL},
};

int luaopen_libclidebugger(lua_State *L) {
    const char rl_prompt_start_ignore = RL_PROMPT_START_IGNORE;
    const char rl_prompt_end_ignore   = RL_PROMPT_END_IGNORE;
    lua_createtable(L, 0, sizeof(lib)/sizeof((lib)[0]) - 1 + 2);
    lua_pushstring(L, "rlpsi");
    lua_pushlstring(L, &rl_prompt_start_ignore, 1);
    lua_rawset(L, -3);
    lua_pushstring(L, "rlpei");
    lua_pushlstring(L, &rl_prompt_end_ignore, 1);
    lua_rawset(L, -3);
    luaL_setfuncs(L, lib, 0);

    storedL = L;

    rl_ignore_some_completions_function = filename_completion_ignore;
    rl_attempted_completion_function = do_completion;
    return 1;
}
