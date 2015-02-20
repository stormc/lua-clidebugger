# clidebugger #

A simple command line interface debugger with readline support for Lua 5.1 and 5.2 inspired by [RemDebug](http://www.keplerproject.org/remdebug/index.html) neglecting its remote facilities. Except for readline support, it's not dependent on anything other than the standard Lua libraries.


## Features ##

### Debugger Commands ###

The following debugger commands are available (as shown by the ``help`` command):
```
help [command]      -- show this list or help for command [F1]
listb               -- lists breakpoints [CTRL+F8]
setb [line file]    -- set a breakpoint to line/file
delb [line file]    -- removes a breakpoint
delallb             -- removes all breakpoints
listw               -- lists watch expressions
setw <exp>          -- adds a new watch expression
delw <index>        -- removes the watch expression at index
delallw             -- removes all watch expressions
ups                 -- list all the upvalue names
locs                -- list all the locals names
vars [depth]        -- list context locals to depth, omitted=1
glob [depth]        -- list globals to depth, omitted=1
what <func>         -- show where <func> is defined (if known)
fenv [depth]        -- list context function env to depth, omitted=1
trace               -- dumps a stack trace
run                 -- run until next breakpoint or watch expression [F9]
over [N]            -- run next N lines, stepping over function calls [F8]
step [N]            -- run next N lines, stepping into function calls [F7]
out [N]             -- run until stepped out of N functions [SHIFT+F8]
exit                -- exits debugger, re-start it using pause()
set [level]         -- set context to stack level, omitted=show
gotoo [line file]   -- step to line in file
show line file X Y  -- show X lines before and Y after line in file [CTRL+RET]
tron [crl]          -- turn trace on for (c)alls, (r)eturns, (l)lines
dump <var> [depth]  -- dump all fields of variable to depth [ALT+F7]
info                -- dumps the complete debug info captured
eval <statement>    -- execute a statement in the current context [ALT+F8]
pon                 -- turn on pause() command
poff                -- turn off pause() command
```

### Readline Support ###

#### clidebugger's readline support offers... ####

  * the line editing features of readline.
  * completion of debugger commands and (some of) its parameters.
  * completion of Lua locals, globals, and upvalues.
  * function and table completions postfixed with ``(`` and ``[`` or ``.``, respectively.
  * completion of filenames via readline's default filename completion, for convenience restricted to directories and files ending in ``.lua``.
  * some command shortcuts if running in a ([u]rxvt-like) terminal, e.g., ``F8`` for ``step over``. See the square bracket-enclosed keys in the [Debugger Commands](#debugger-commands) section above for reference.
  * ANSI escape sequence output coloring in some places if running in a terminal, e.g., the ``show`` command's source listing indicates the current line of execution by a green ``***`` marker and shows line numbers in an unobtrusive black color.

#### Future improvements to readline support ####

  * The ``what`` command completion does not (yet) offer completion of local functions as their type cannot be easily inferred at completion time. Implementing this feature requires quite a bit of refactoring (see the source code comments). For the same reason, locals are not postfixed with ``(``, ``[``, or ``.`` .
  * A command's optional number or count parameter is generally not offered for completion.
  * Command shortcuts are currently available for [u]rxvt terminals only. Adding support for other terminals is merely a matter of inserting the according key escape sequences for your terminal.


## Installation and Usage##

### Installation ###

Just place clidebugger in some directory and add that directory to the ``LUA_PATH`` environment variable, e.g.
```sh
mkdir $HOME/clidebugger
cd $HOME/clidebugger
git clone https://github.com/stormc/lua-clidebugger.git .
export LUA_PATH=;;$HOME/clidebugger/?.lua
```
You may want to put the ``export LUA_PATH=...`` statement somewhere appropriate in your shell's startup files.

Enjoying readline support requires to compile the Lua C-module ``libclidebugger.so`` by running ``make`` in ``$HOME/clidebugger``. 


### Usage Example ###

Then, insert the following lines into the Lua file to debug
```lua
require("debugger")
...
pause("debug time!")
```
and run the debuggee Lua file which will drop to the debugger shell once the control flow reaches the ``pause(...)`` line, e.g.
```
Lua Debugger
Paused at file ./debugger.lua line 22 (0)
Message: debug time!
Type 'help' for commands
>>
```

