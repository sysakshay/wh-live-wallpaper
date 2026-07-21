---
name: windhawk_mod_creation
description: Cheatsheet and reference guide for creating, structuring, and developing C++ mods for the Windhawk engine, including metadata, settings, lifecycle callbacks, and API functions.
---

# Windhawk Mod Creation Guide & API Reference

Each Windhawk mod is a single C++ file (`.cpp`) compiled into a dynamic library using Clang (C++23 mode, mingw-w64 toolchain). The mod is loaded directly into the target processes specified in its metadata.

---

## 1. Mod Structure Overview

A complete Windhawk mod consists of up to four main sections:
1. **Metadata Block** (`// ==WindhawkMod==` ... `// ==/WindhawkMod==`)
2. **Readme Block** (*Optional*, `// ==WindhawkModReadme==` ... `// ==/WindhawkModReadme==`)
3. **Settings Block** (*Optional*, `// ==WindhawkModSettings==` ... `// ==/WindhawkModSettings==`)
4. **C++ Code** (Callback implementations and custom logic)

---

## 2. Mod Metadata Block (`// ==WindhawkMod==`)

Every mod must begin with the metadata block defining identifiers, targets, and compiler options.

```cpp
// ==WindhawkMod==
// @id              unique-mod-id
// @name            Your Awesome Mod
// @description     Short description of what the mod does
// @version         1.0
// @author          Your Name
// @github          https://github.com/username
// @twitter         https://twitter.com/username
// @homepage        https://your-website.example.com/
// @donateUrl       https://your-website.example.com/sponsor
// @include         notepad.exe
// @include         %SystemRoot%\explorer.exe
// @exclude         C:\unwanted\path\*.exe
// @architecture    x86-64
// @compilerOptions -lcomctl32 -lgdi32 -luxtheme
// @license         MIT
// ==/WindhawkMod==
```

### Key Metadata Directives
- **`@id`**: Unique identifier containing only `0-9`, `a-z`, and `-`.
- **`@include` / `@exclude`**: Target executable filenames/paths. Supports wildcards (`*`, `?`) and environment variables (`%SystemRoot%`, `%ProgramFiles%`, `%ProgramFiles(X86)%`). Multiple entries allowed.
- **`@architecture`**: Supported targets (`x86`, `amd64`, `arm64`, `x86-64`). Omitting is equivalent to specifying both `x86` and `x86-64`.
- **`@compilerOptions`**: Extra flags passed to the Clang compiler (e.g., `-lcomdlg32 -ldwmapi`).
- **Localization**: Supports language suffixes, e.g., `// @name:fr-FR Exemple de mod`.

---

## 3. Readme & Settings Blocks

### Readme (`// ==WindhawkModReadme==`)
Contains Markdown documentation displayed to the user. Embedded images must be hosted on `i.imgur.com` or `raw.githubusercontent.com`.

```cpp
// ==WindhawkModReadme==
/*
# Your Awesome Mod
Describe the mod features, usage instructions, and formatting here.
*/
// ==/WindhawkModReadme==
```

### Settings (`// ==WindhawkModSettings==`)
Defined in YAML inside a C block comment (`/* ... */`). Supports types: `boolean`, `number`, `string`, arrays (`[1, 2, 3]` or `[a, b, c]`), and nested structures.

```cpp
// ==WindhawkModSettings==
/*
- EnableFeature: true
  $name: Enable Feature
  $description: Enables the core functionality of the mod
- Opacity: 80
- Theme: dark
  $options:
    - light: Light Theme
    - dark: Dark Theme
- Advanced:
    - BufferSize: 1024
*/
// ==/WindhawkModSettings==
```

---

## 4. Lifecycle Callback Functions

Implement these functions in your C++ code to integrate with Windhawk's engine:

| Function | Signature | Description |
| :--- | :--- | :--- |
| **`Wh_ModInit`** *(Required)* | `BOOL Wh_ModInit()` | First callback when loading into target process. Initialize resources and register hooks here. Return `TRUE` on success, `FALSE` to abort loading. |
| **`Wh_ModAfterInit`** | `void Wh_ModAfterInit()` | Called after `Wh_ModInit` and after hooks are applied. |
| **`Wh_ModBeforeUninit`** | `void Wh_ModBeforeUninit()` | Called before hooks are removed during unloading. |
| **`Wh_ModUninit`** | `void Wh_ModUninit()` | Called after hooks are removed during unloading. Clean up resources. |
| **`Wh_ModSettingsChanged`** | `void Wh_ModSettingsChanged()`<br>*or*<br>`BOOL Wh_ModSettingsChanged(BOOL* bReload)` | Called when user updates settings. If using Variant 2, return `TRUE` and set `*bReload = TRUE` to trigger a full mod reload. Return `FALSE` to unload until next setting change. |

---

## 5. API Functions Reference

### Logging & Constants
- **`Wh_Log(PCWSTR format, ...)`**: Logs `printf`-style messages to the Windhawk editor log output window (evaluated only when logging is active).
- **Constants**: `WH_MOD_ID` (`PCWSTR`), `WH_MOD_VERSION` (`PCWSTR`).

### Function Hooking
- **`BOOL Wh_SetFunctionHook(void* targetFunction, void* hookFunction, void** originalFunction)`**: Registers a detour hook. Set in `Wh_ModInit`. `originalFunction` receives the trampoline pointer.
- **`BOOL Wh_RemoveFunctionHook(void* targetFunction)`**: Registers a hook for removal.
- **`BOOL Wh_ApplyHookOperations()`**: Manually applies pending hooks/unhooks. Note: Automatically called after `Wh_ModInit`; avoid manual calls when possible for performance.

### User Settings (`// ==WindhawkModSettings==` values)
- **`int Wh_GetIntSetting(PCWSTR valueName, ...)`**: Retrieves an integer setting (supports `printf`-style formatting for key names). Returns `0` if not found.
- **`PCWSTR Wh_GetStringSetting(PCWSTR valueName, ...)`**: Retrieves a string setting. Must be freed when done using `Wh_FreeStringSetting(string)`.
- **`void Wh_FreeStringSetting(PCWSTR string)`**: Frees memory from `Wh_GetStringSetting`.

### Persistent Local Storage & Paths
- **`int Wh_GetIntValue(PCWSTR valueName, int defaultValue)`** / **`BOOL Wh_SetIntValue(PCWSTR valueName, int value)`**
- **`size_t Wh_GetStringValue(PCWSTR valueName, PWSTR buffer, size_t bufferChars)`** / **`BOOL Wh_SetStringValue(PCWSTR valueName, PCWSTR value)`**
- **`size_t Wh_GetBinaryValue(PCWSTR valueName, void* buffer, size_t bufferSize)`** / **`BOOL Wh_SetBinaryValue(PCWSTR valueName, const void* buffer, size_t bufferSize)`**
- **`BOOL Wh_DeleteValue(PCWSTR valueName)`**: Deletes a stored value.
- **`size_t Wh_GetModStoragePath(PWSTR pathBuffer, size_t bufferChars)`**: Gets the mod's dedicated storage directory path (removed when the mod is uninstalled).

### Symbol Enumeration (`Wh_FindFirstSymbol`)
```cpp
typedef struct tagWH_FIND_SYMBOL_OPTIONS {
    size_t optionsSize;          // Set to sizeof(WH_FIND_SYMBOL_OPTIONS)
    PCWSTR symbolServer;         // NULL for Microsoft public symbol server
    BOOL noUndecoratedSymbols;   // TRUE for faster enumeration of decorated symbols only
} WH_FIND_SYMBOL_OPTIONS;

typedef struct tagWH_FIND_SYMBOL {
    void* address;
    PCWSTR symbol;
    PCWSTR symbolDecorated;
} WH_FIND_SYMBOL;

HANDLE Wh_FindFirstSymbol(HMODULE hModule, const WH_FIND_SYMBOL_OPTIONS* options, WH_FIND_SYMBOL* findData);
BOOL Wh_FindNextSymbol(HANDLE symSearch, WH_FIND_SYMBOL* findData);
void Wh_FindCloseSymbol(HANDLE symSearch);
```

### Disassembly (`Wh_Disasm`)
```cpp
typedef struct tagWH_DISASM_RESULT {
    size_t length;
    char text[96];
} WH_DISASM_RESULT;

BOOL Wh_Disasm(void* address, WH_DISASM_RESULT* result);
```

### HTTP Requests (`Wh_GetUrlContent`)
```cpp
typedef struct tagWH_GET_URL_CONTENT_OPTIONS {
    size_t optionsSize;     // Set to sizeof(WH_GET_URL_CONTENT_OPTIONS)
    PCWSTR targetFilePath;  // Optional file path to write directly to disk
} WH_GET_URL_CONTENT_OPTIONS;

typedef struct tagWH_URL_CONTENT {
    const char* data;
    size_t length;
    int statusCode;
} WH_URL_CONTENT;

const WH_URL_CONTENT* Wh_GetUrlContent(PCWSTR url, const WH_GET_URL_CONTENT_OPTIONS* options);
void Wh_FreeUrlContent(const WH_URL_CONTENT* content);
```
