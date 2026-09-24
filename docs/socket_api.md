---
title: Socket API
description: The wire protocol reference for building libraries and clients against the Mango IPC socket.
---

# Mango Socket API - Protocol Reference

This document describes the Mango compositor IPC socket at the wire level. It is the reference a programmer needs to implement a client or library in any language. The `mmsg` binary (`mmsg/mmsg.c`) is the reference implementation; its argv joins into a single space-separated command string and forwards it to the socket.

## Socket and Environment

* **Socket type**: Unix domain socket, `AF_UNIX`, `SOCK_STREAM`.
* **Path**: `$XDG_RUNTIME_DIR/mango-<pid>.sock`, where `<pid>` is the compositor PID.
* The running instance exports its socket path in the environment variable `MANGO_INSTANCE_SIGNATURE`. Prefer reading this variable over reconstructing the path; it is set by the compositor (`src/ipc/ipc.c:1419`) and is inherited by processes spawned from it.
* To control a specific instance from outside, set `MANGO_INSTANCE_SIGNATURE=<path>` explicitly for your client process.

## Framing

* Every request is a single line terminated by `\n`. Everything before the first `\n` is the command. No binary framing, no length prefix, no null padding.
* Responses to non-`watch` commands are a single JSON object terminated by `\n`, after which the server closes the connection.
* `watch` commands do not close the connection. The server first sends a snapshot of the current state (one JSON line), then sends a JSON line per event. Keep reading until EOF or your own timeout.
* The server converts every comma in a `get` command into a space before matching (`src/ipc/ipc.c:628`). Commas and single spaces can therefore be used interchangeably in `get` command arguments:
  `get monitor eDP-1` and `get,monitor,eDP-1` are equivalent.
* `setoption` and `unset <rule>` parse the raw line, so commas inside a value or rule spec are literal and can be pasted back unchanged from `get binds` / `get rules`. Inside `dispatch`, a literal comma inside an argument is quoted with `\,` (below).
* Any unrecognized command answers `{"error":"unknown command"}`.

## Commands

### `get`

One-shot queries. Reply object shape per branch; errors are `{"error":"<reason>"}`.

| Command | Reply |
| :--- | :--- |
| `get version` | `{"version":"..."}` |
| `get cursorpos` | `{"x":n,"y":n,"monitor":"..."\|null}` (monitor under cursor, or null) |
| `get keymode` | `{"keymode":"..."}` |
| `get keyboardlayout` | `{"layout":"..."}` (active XKB layout, empty string if none) |
| `get last_open_surface [<mon>]` | `{"monitor":"...","last_open_surface":"..."}`; without arg uses the selected monitor; `{"error":"monitor not found"}` |
| `get monitor <name>` | Full monitor object (schema below); `{"error":"monitor not found"}` |
| `get focusing-client` | Client object of the focused client; `{"error":"no focused client"}` |
| `get client <id>` | Client object by numeric id; `{"error":"client not found"}` |
| `get tag <mon> <idx>` | `{"monitor","tag_index","is_active","is_urgent","client_count","focused_client"}`; `tag_index` is 1-based; errors on malformed usage or bad monitor/index |
| `get all-clients` | `{"clients":[client...]}` |
| `get all-monitors` | `{"monitors":[monitor...]}` |
| `get all-devices` | `{"devices":[{...device}]}` (one entry per physical libinput device group) |
| `get all-layers` | `{"layers":[{"monitor","layer","name"}]}` (mapped layer surfaces only) |
| `get all-tags` | `{"all_tags":[{"monitor","tags":[...]}]}` (one entry per monitor) |
| `get tags <mon>` | `{"monitor","tags":[...],"active_tags":[...]}`; `{"error":"monitor not found"}` |
| `get layouts` | `{"layouts":[{"symbol","name"}]}` |
| `get option <key>` | `{"option":"<key>","value":"<configured literal>"}`; `{"error":"option not found"}` |
| `get options` | `{"options":[{"option":"...","value":"..."}]}` (set of keys ever configured this session) |
| `get binds` | `{"binds":[{keymode,binds:[...]}],"mouse":[...],"axis":[...],"switch":[...],"gesture":[...]}` (schema below) |
| `get rules` | `{"rules":[{"rule":"...","spec":"..."},...]}` (schema below) |

The option store (`get option` / `get options`) reports the raw literal value as it appeared in the config file or the most recent `setoption`, before clamping. Keys not seen this session are not present; bind-family keys (`bind`, `mousebind`, `axisbind`, `switchbind`, `gesturebind`) are listed as options too, while rules are served by `get rules`. See the scriptability notes below.

### `setoption`

`setoption <key> <value>` - set a single config option at runtime. The value extends to the end of the line, so it may contain literal commas and spaces:

* OK: `setoption animations 0`
* OK: `setoption bind MOD,xkey,spawn_shell,foot,--flag`

Reply: `{"success": true}`. Unless the key is unknown or the value invalid:

* `setoption animations` (no value) -> `{"error":"usage: setoption <key> <value>"}`
* `setoption nosuchkey 1` -> `{"error":"unknown option or invalid value"}`

Effects: `setoption` writes the value, clamps and re-applies runtime state, and does **not** re-run `exec=` hooks. To re-run autostart hooks, reload the config file (this is outside the IPC surface).

Commas inside a value that an option parser would split on (binds, rule specs, curve values) are escaped as `\,`; the server collapses the pair to `,` before parsing. `get rules` and `get binds` emit `spec` raw, so re-escape any literal comma with `\,` when sending a rule spec back through `setoption`.

### `get binds`

Sibling arrays per bind kind, each grouped by keymode. Example shape:

```json
{
  "binds":   [{"keymode":"default","binds":[
    {"family":"bind","mod":"Super","keysym":"space","spec":"bind,Super,space,spawn_shell,foot"}
  ]}],
  "mouse":   [{"keymode":"default","binds":[
    {"mod":"Super","button":"code:272","spec":"mousebind,Super,code:272,togglefloating"}
  ]}],
  "axis":    [{"keymode":"default","binds":[
    {"mod":"","dir":"up","spec":"axisbind,...,up,..."}
  ]}],
  "switch":  [{"keymode":"default","binds":[
    {"fold":"fold","spec":"switchbind,..."}
  ]}],
  "gesture": [{"keymode":"default","binds":[
    {"mod":"","motion":"up","fingers_count":"3","spec":"gesturebind,...,up,3,..."}
  ]}]
}
```

Per-entry fields:

* key binds: `family` is the base `bind` plus one suffix letter per flag: `s` = sym bind (reparses as keysym), `l` = lock, `r` = release, `p` = pass, `c` = allowconflict. `mod` is accepted by `parse_mod`; `keysym` is the XKB key name for binds written as a symbol (including symbol binds that mango resolves to a keycode), or `code:<n>` when the config used an explicit `code:` prefix.
* mouse: `mod`, `button` (`code:<n>`).
* axis: `mod`, `dir` (`up`/`down`/`left`/`right`/`alldir`/`undir`).
* switch: `fold` (`fold`/`unfold`).
* gesture: `mod`, `motion` (direction), `fingers_count`.
* every entry carries its raw `spec`, or `""` for built-in defaults. The reported fields feed the `unset` command unchanged.

### `get rules`

```json
{"rules":[{"rule":"windowrule","spec":"title=^Steam$"},{"rule":"monitorrule","spec":"eDP-1,1920x1080@60,1"}]}
```

`rule` is one of `windowrule`, `windowrule-once`, `layerrule`, `monitorrule`, `tagrule`, `devicerule`, matching the config key spelling. `spec` is the raw value after the key.

### `unset`

Remove a bind or rule for the current session only; no config-file write, and a config reload rebuilds everything from the file.

```text
unset bind <keymode> <mod> <keysym> <family>
unset mousebind <keymode> <mod> <button>
unset axisbind <keymode> <mod> <dir>
unset switchbind <keymode> <fold>
unset gesturebind <keymode> <mod> <motion> <fingers>
unset windowrule <spec>
unset layerrule <spec>
unset monitorrule <spec>
unset tagrule <spec>
unset devicerule <spec>
```

Binds match by identity (`keymode` plus the fields `get binds` reported); the `s` in `family` selects keysym vs keycode matching. Rules match by exact `spec` read from the raw line, so a spec pasted unchanged from `get rules` matches directly (literal commas are preserved). Built-in default key binds report `spec` as `""` and are removed by identity like any other. Reply is `{"success":true}` or `{"error":"not found"}`.

### `dispatch`

`dispatch <func>[,<arg>...] [client,<id>]` - invoke a dispatcher function (the `mmsg dispatch` surface):

* `dispatch exchange_client,left`
* `dispatch window_move_to_specific_monitor,eDP-1 client,375`

Tokenization rules:

* The string after `dispatch ` is split on `,` into tokens; tokens 0-5 are forwarded (function name plus up to 5 args). Empty tokens and surrounding whitespace are trimmed.
* A literal comma inside a value is escaped as `\,`; the backslash is stripped and the comma kept. `dispatch setoption,bind,MOD,xkey,spawn_shell,foot\,--flag` yields the arg `foot,--flag`.
* Token cap is 16; extra tokens are dropped.
* The function name is resolved via `parse_func_name` (parse_config.c); the tokenizer is in `src/ipc/ipc.c`.
* Optional trailing `client,<id>` cookie routes the call at a target client. It may appear at the start, end, or any boundary point of the token stream; the scanner (`src/ipc/ipc.c:901`) pulls it out wherever `client,<id>,`, `client,<id>` or `client,<id> ` occurs.

Replies: `{"success":true}` on invocation (including "no-op" dispatchers); `{"error":"no client found"}` if the `client,<id>` cookie references a dead client; `{"error":"unknown function"}` if the token is not a known dispatcher.

### `watch`

`watch <subject>` - subscribe to a stream. The connection becomes the delivery channel; send exactly one watch command, do not terminate the connection after the first reply.

| Command | Initial snapshot | Subsequent payloads |
| :--- | :--- | :--- |
| `watch monitor <name>` | monitor object | monitor object on any change to that output |
| `watch focusing-client` | focused client object, or `{"id":null,"title":null,"appid":null}` when nothing focused | focused client object |
| `watch client <id>` | client object, or nothing if the id does not exist | client object updates |
| `watch tags <mon>` | `{"monitor","tags","active_tags"}` | same shape |
| `watch all-monitors` | `{"monitors":[...]}` | `{"monitors":[...]}` |
| `watch all-tags` | `{"all_tags":[{"monitor","tags"}]}` | same shape |
| `watch all-clients` | `{"clients":[...]}` | `{"clients":[...]}` |
| `watch all-devices` | `{"name":null,"type":null}` | `{"name","type"}` of the last device that triggered an event |
| `watch keymode` | `{"keymode":"..."}` | `{"keymode":"..."}` |
| `watch keyboardlayout` | `{"layout":"..."}` | `{"layout":"..."}` |
| `watch last_open_surface [<mon>]` | `{"monitor","last_open_surface"}` (default selected monitor) | same shape |

Each payload is a separate JSON line. The server treats any read on a watch connection only as a keepalive; a closed connection unsubscribes it (`src/ipc/ipc.c:277`).

## Object Schemas

### Client

`get all-clients`, `get focusing-client`, `get client <id>`, and client watches emit this object.

```json
{
  "id": 3,
  "pid": 12831,
  "foreign_toplevel_id": "fd5d...",
  "title": "firefox",
  "appid": "firefox",
  "monitor": "eDP-1",
  "tags": [1],
  "is_xwayland": false,
  "is_swallowing": false,
  "is_swallowedby": false,
  "is_group": false,
  "is_visible": true,
  "is_focused": true,
  "is_fullscreen": false,
  "is_floating": false,
  "is_maximized": false,
  "is_global": false,
  "is_unglobal": false,
  "is_overlay": false,
  "is_fakefullscreen": false,
  "is_minimized": false,
  "is_urgent": false,
  "is_scratchpad": false,
  "is_namedscratchpad": false,
  "x": 0, "y": 0, "width": 1920, "height": 1080,
  "scroller_proportion": 1.0
}
```

Note: `foreign_toplevel_id` is the identifier string of the client's foreign-toplevel handle. `tags` is an array of 0-based tag indices (0 is the scratchpad tag array). `monitor` is empty string when unmapped.

### Monitor

`get monitor`, `get all-monitors`, and monitor watches emit this object.

```json
{
  "name": "eDP-1",
  "active": true,
  "is_hdr": false,
  "is_vrr": false,
  "x": 0, "y": 0, "width": 1920, "height": 1080,
  "scale": 1.0,
  "layout_index": 0,
  "layout_symbol": "tile",
  "last_open_surface": "firefox",
  "tag_num": 8,
  "hide_clients": 0,
  "tags": [{"index":1,"is_active":true,"is_urgent":false,"layout":"tile","client_count":2}],
  "active_tags": [1],
  "active_client": {"id":3,"title":"firefox","appid":"firefox"}
}
```

`active_client` is `{"id":null,"title":null,"appid":null}` when nothing is focused on that output. `layout_index` is the index into the layouts array of `get layouts`.

### Tag entry

Each element of a monitor's `tags` array (`get tags`, `get all-tags`, `get tag`):

```json
{"index": 1, "is_active": true, "is_urgent": false, "layout": "tile", "client_count": 2}
```

Indices are 1-based in tag entries; `active_tags` arrays and client `tags` arrays use 0-based indices. `active_tags` is `[0]` when the monitor is in overview mode.

### Device

Each element of `get all-devices` / `get all-layers`:

```json
{
  "name": "Logitech MX Master",
  "types": ["pointer", "keyboard"],
  "vendor": 1133,
  "product": 40686,
  "identifier": "1133:40686:Logitech MX Master",
  "interfaces": 2,
  "matched": true,
  "matched_rule": "mymouse"
}
```

`matched_rule` is present only when a config device rule matched. Devices not handled by libinput have vendor/product `0`. `types` values: `keyboard`, `pointer`, `trackpad`, `touch`, `switch`, `tablet`, `pad`, `unknown`.

### Layer

```json
{"monitor": "eDP-1", "layer": "top", "name": "waybar"}
```

`layer` values: `background`, `bottom`, `top`, `overlay`.

### Layout

```json
{"symbol": "tile", "name": "The tile layout"}
```

## Error Handling

* Errors are always `{"error":"<message>"}`. Known messages: `monitor not found`, `no focused client`, `client not found`, `option not found`, `usage: setoption <key> <value>`, `unknown option or invalid value`, `not found`, `no client found`, `unknown function`, `unknown command`.
* Replies are single `\n`-terminated JSON values. For non-`watch` commands exactly one reply arrives before close; do not block waiting for more.
* Dispatch replies carry no per-function payload; query state afterward with `get` or a `watch`.

## Scriptability Notes

* `setoption` combined with `get option` / `get options` is the runtime config surface intended for scriptable configuration: query the current literal values, set them, and share the existing parsing/clamping/apply pipeline. `exec` hooks are intentionally not re-run by `setoption`, so it is safe to call for every key your library exposes.
* `get binds` / `get rules` / `unset` extend the surface to bindings and rules: enumerate current state, then `setoption` to add or replace, or `unset` to remove. Removal and `setoption` are session-only; reloading the config file resets runtime state to the file contents.
* `dispatch` covers the non-config command surface (layout operations, focus, monitors, keybinds, spawning); see `docs/ipc.md` for the `mmsg` usage of the same surface.
* The config file remains the source of truth for values not set this session; `get option` reports only keys the running instance has seen. A library that wants a complete view should first load the config file or drive configuration entirely through `setoption`.

## Minimal Interactive Probe

```sh
# from a shell spawned by the compositor:
mmsg get option animations        # or: cat <<< 'get option animations' | nc -U "$MANGO_INSTANCE_SIGNATURE"
mmsg setoption animations 0
mmsg get options
mmsg get binds
mmsg get rules
mmsg unset bind default Super space bind   # remove from the current session
mmsg dispatch cycle_layout,next client,375
```