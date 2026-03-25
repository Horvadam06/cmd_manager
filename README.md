# cmd — Command Manager Library
Created by Horváth Ádám József, early 2026.

A lightweight, context-aware command manager library for C++ CLI applications. Supports command registration, aliases, context stacking, interactive input with tab completion, and cross-platform raw input handling.

---

## Features

- **Context-aware command execution** - commands are registered per context, looked up in a prioritized stack at runtime
- **Global context**                  - commands like `help` are always available without per-context registration
- **Alias support**                   - register secondary keys that resolve to the same handler (`e` → `exit`)
- **Tab completion**                  - cycles through prefix-matched commands, works mid-line with correct cursor positioning
- **Conflict detection**              - asserts on duplicate command names across combined contexts by default
- **Cross-platform**                  - Windows (`conio.h`) and Linux (`termios`) supported via `#ifdef`
- **Singleton instance**              - one manager for the lifetime of the program

---

## File Structure

```
cmd/
├── command_manager.hpp   # Public API, types, CommandManager class
├── command_manager.cpp   # CommandManager implementation
├── input.hpp             # KeyPress, Key enum
└── input.cpp             # readKey(), enableRawMode(), disableRawMode()
```

---

## Quick Start

```cpp
#include "command_manager.hpp"

static constexpr char my_ctx_key = 0;
cmd::ContextId my_ctx = &my_ctx_key;

auto& mgr = cmd::CommandManager::instance();

mgr.registerCommand(my_ctx, "exit", "Quit the program",
    [](const std::vector<std::string>&, const cmd::ContextList&) {
        std::exit(0);
    });

mgr.registerAlias(my_ctx, "e", "exit");

mgr.runInteractive({ my_ctx });
```

---

## API Reference

### Types

```cpp
using ContextId   = const void*;
using ContextList = std::vector<ContextId>;

enum class ConflictPolicy {
    Error,     // default — asserts on duplicate names across combined contexts
    FirstWins, // explicit opt-in, first context in the list wins
};
```

### CommandManager::instance()
Returns the singleton instance.
```cpp
CommandManager& instance();
```

### registerCommand()
Registers a command in a given context. Returns `nullptr` if the name is already taken.
```cpp
const Command* registerCommand(
    ContextId ctx,
    const std::string& name,
    const std::string& description,
    std::function<void(const std::vector<std::string>&, const ContextList&)> handler
);
```
The handler receives parsed arguments and the active context list. Commands with a `nullptr` handler are valid — they act as named tokens for pointer comparison.

### registerAlias()
Registers a secondary name pointing to an existing command's handler. Aliases of aliases are not supported.
```cpp
bool registerAlias(ContextId ctx, const std::string& alias, const std::string& target);
```

### execute()
Parses input, resolves the command through the context stack (plus global), and calls its handler.
```cpp
const Command* execute(
    const ContextList& contexts,
    const std::string& input,
    ConflictPolicy policy = ConflictPolicy::Error
);
```
Returns the matched `Command*`, or `nullptr` on parse failure or unknown command. The returned pointer can be compared directly:
```cpp
const Command* cmd_yes = mgr.registerCommand(ctx, "yes", "", nullptr);
const Command* result  = mgr.execute({ ctx }, input);
if (result == cmd_yes) { ... }
```

### runInteractive()
Starts an interactive prompt loop with raw input, tab completion, and cursor movement.
```cpp
void runInteractive(
    const ContextList& contexts,
    bool modal = false,
    ConflictPolicy policy = ConflictPolicy::Error
);
```
- `modal = false` — exits after the first valid non-help command
- `modal = true` — loops until an `exit` command or EOF

### printHelp()
Prints all non-alias commands for the given context stack. Pass `showAliases = true` or type `help aliases` at the prompt to include inline alias annotations (`exit (e) - Quit the program`).
```cpp
void printHelp(
    const ContextList& contexts,
    bool showAliases = false,
    ConflictPolicy policy = ConflictPolicy::Error
) const;
```

---

## Context Stacking

Commands are searched in order through the provided `ContextList`, with the global context always appended last. The first match wins.

```cpp
// yes/no only
mgr.runInteractive({ &yes_no_ctx });

// yes/no available, plus main commands
mgr.runInteractive({ &yes_no_ctx, &main_ctx });
```

If the same command name appears in two contexts in the same list, `ConflictPolicy::Error` will assert. Use `ConflictPolicy::FirstWins` when overlap is intentional:

```cpp
mgr.runInteractive({ &yes_no_ctx, &main_ctx }, true, cmd::ConflictPolicy::FirstWins);
```

---

## Built-in Commands

| Command | Description |
|---|---|
| `help` | Lists all commands in the active context stack |
| `help aliases` | Same, with aliases shown inline |

`help` is registered automatically in the global context — no per-context setup needed.

---

## Tab Completion

While in `runInteractive`, pressing `Tab` cycles through commands that match the current input prefix. Multiple matches cycle on repeated `Tab` presses. Completion respects the active `ContextList` and search order.

---

## Platform Notes

Raw input and ANSI cursor control are handled in `input.cpp`:

| Feature | Windows | Linux |
|---|---|---|
| Raw keypresses | `conio.h / _getch()` | `termios` |
| ANSI escape codes | Requires `ENABLE_VIRTUAL_TERMINAL_PROCESSING` | Native |
| Terminal restore on exit | `disableRawMode()` | `atexit` + `disableRawMode()` |

On Linux, the terminal is restored automatically via `atexit` even if the program crashes or is interrupted.

---

## Debug Mode

Set `Debug_mode = true` inside `CommandManager` to enable diagnostic messages prefixed with `[CMD_MNGR]`. These surface duplicate registrations, missing alias targets, and other registration errors at runtime.
