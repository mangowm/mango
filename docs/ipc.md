---
title: IPC
description: Control mangowm programmatically using mmsg.
---

# mmsg(1) - User Manual

`mmsg` is the command-line interface for the Mango compositor's Inter-Process Communication (IPC) system. It allows users and scripts to query the state of the compositor or subscribe to real-time events.

## SYNOPSIS
`mmsg <command> [arguments...]`

## DESCRIPTION
`mmsg` acts as a client that connects to the Mango compositor via a Unix domain socket defined by the `MANGO_INSTANCE_SIGNATURE` environment variable. It supports two primary modes of operation:
1. **One-shot Request (`get`)**: Sends a query to the compositor, receives a single JSON response, and terminates.
2. **Persistent Stream (`watch`)**: Subscribes to a specific state, receiving continuous JSON updates whenever that state changes.

## ENVIRONMENT VARIABLES
* **`MANGO_INSTANCE_SIGNATURE`**: Must be set to the path of the Unix socket created by the running Mango instance. This is typically handled automatically when running `mmsg` from within a terminal spawned by the compositor.

## COMMANDS

### GET (One-Shot Queries)
| Command | Description |
| :--- | :--- |
| `get version` | Returns the current version of the compositor. |
| `get cursorpos` | Returns the global pointer position (`x`, `y`) and the monitor under it. |
| `get keymode` | Returns the current active keyboard mode (e.g., normal, insert). |
| `get keyboardlayout` | Returns the active XKB layout (abbreviated). |
| `get monitor <name>` | Returns full JSON details for a specific monitor. |
| `get focusing-client` | Returns full JSON details for the client currently in focus. |
| `get client <id>` | Returns full JSON details for a client with the given ID. |
| `get tag <mon> <idx>` | Queries status of a specific tag on a monitor. |
| `get tags <mon>` | Returns a JSON object containing the status of all tags on a monitor. |
| `get all-clients` | Returns a JSON array of all active clients. |
| `get all-monitors` | Returns a JSON array of all connected monitors. |
| `get all-devices` | Returns a JSON array of all physical input devices, grouped by libinput device group (`name`, `types`, `identifier`, `vendor`, `product`, `interfaces`, `matched`). |
| `get all-layers` | Returns a JSON array of all open layer surfaces (`monitor`, `layer`, `name`). |
| `get all-tags` | Returns a JSON object containing the status of all tags. |
| `get last_open_surface [<mon>]` | Returns the last focused surface name for a monitor,if the mon not set, it will get current monitor. |
| `get layouts` | Returns the layout list (`symbol`, `name`). |
| `get option <key>` | Returns a single option's configured literal value. |
| `get options` | Returns all options seen this session (option and bind-family keys). |
| `get binds` | Returns all key/mouse/axis/switch/gesture binds grouped by keymode. |
| `get rules` | Returns all rules and their raw specs. |

*Example:*
```bash
mmsg get monitor eDP-1
mmsg get all-clients
mmsg get all-monitors
mmsg get all-devices
mmsg get all-layers
mmsg get cursorpos
```

### SETOPTION
`setoption <key> <value>` sets a config option at runtime. The value extends to the end of the line, so it may contain literal commas and spaces:

```bash
mmsg setoption animations 0
mmsg setoption bind Alt,space,setlayout,monocle
mmsg setoption windowrule title=Steam,isfloating:true
```

Reply is `{"success": true}`, or `{"error":"usage: setoption <key> <value>"}` / `{"error":"unknown option or invalid value"}`. Changes are session-only; reloading the config file restores the file values. Binds and rules added this way show up in `get binds` / `get rules` and can be removed with `unset`. Read back the current literal with `get option <key>` / `get options`.

### GET BINDS
`get binds` enumerates every bind, grouped by keymode:

```json
{"binds":[{"keymode":"default","binds":[
  {"family":"bind","mod":"Alt","keysym":"space","spec":"Alt,space,setlayout,monocle"}
]}],"mouse":[...],"axis":[...],"switch":[...],"gesture":[...]}
```

`family` is `bind` plus one suffix letter per flag (`s` = sym bind, `l` = lock, `r` = release, `p` = pass, `c` = allowconflict); `keysym` is the XKB key name for symbol binds or `code:<n>` for explicit keycodes. Every entry carries its raw `spec`.

### GET RULES
`get rules` enumerates the active rules:

```bash
mmsg get rules
```

Returns `{"rules":[{"rule":"windowrule","spec":"..."},...]}`. `rule` is one of `windowrule`, `windowrule-once`, `layerrule`, `monitorrule`, `tagrule`, `devicerule`, and `spec` is the raw value after the config key.

### UNSET
Removes a bind or rule for the current session only (a config reload rebuilds everything from the file):

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

Binds match by identity; pass the fields exactly as `get binds` reported them (the `s` in the trailing `family` for the key binds selects sym vs keycode matching). Rules match by exact spec, so paste them unchanged from `get rules` (commas are preserved). Reply is `{"success":true}` or `{"error":"not found"}`.

### WATCH (Event Subscription)
Subscribes the client to real-time updates. When the state changes, the server pushes a new JSON object to the output stream.

* `watch monitor <name>`
* `watch focusing-client`
* `watch client <id>`
* `watch tags <mon_name>`
* `watch all-monitors`
* `watch all-tags`
* `watch all-clients`
* `watch all-devices` — streams the last input device (`name`, `type`) that triggered an event
* `watch keymode`
* `watch keyboardlayout`
* `watch last_open_surface [<mon_name>]`

*Example:*
```bash
# watch all monitors
mmsg watch all-monitors
# watch all tags
mmsg watch all-tags
```

### DISPATCH
Allows sending commands to the compositor to alter its state.
* `dispatch <func_name>,[args...] [client,<id>]`

*Example:* 
```bash   
# operate specific client by id
mmsg dispatch exchange_client,left client,375
# operate current client
mmsg dispatch exchange_client,left
````
