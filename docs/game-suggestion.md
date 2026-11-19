---
title: Game Suggestions
description: Recommended window rules and troubleshooting for games running under mangowm.
---

Most game-specific issues can be addressed with [window rules](/docs/window-management/rules#window-rules).
Rules can match a Wayland application by `app_id` or a window by `title`. When both
are present, both must match.

The configuration below is a starting point for Worms Reloaded. Replace `app_id`
with the game's own application ID, and remove options you do not need.

```ini
# Keep the game tiled, prevent idle while focused, and keep its rendering loop
# alive when it is hidden or scrolled off-screen.
window_rule=is_floating:0,idle_inhibit_when_focus:1,force_render:1,app_id:WormsReloaded.bin.x86
```

> **Note:** `vrr_only_fullscreen:1` can be added to the rule if you use variable
> refresh rate (VRR). It requires `vrr:0` in the matching monitor rule.

## FAQ

### Why is a game slower under mangowm than on X11?

Start with the game's built-in VSync option. If it causes poor frame pacing and
you accept tearing artifacts, enable tearing globally and force it for the game
window:

```ini
allow_tearing=1
window_rule=force_tearing:1,title:Counter-Strike 2
```

If the game offers working VSync, prefer that setting instead of forcing
tearing. `force_tearing=1` is useful for reducing latency, but it can introduce
visible tearing. See [Tearing (Game Mode)](/docs/configuration/monitors#tearing-game-mode)
for the full behavior matrix and graphics card compatibility notes.

Use an exact `app_id` or window `title` so the rule does not affect unrelated
windows. See [Window Rules](/docs/window-management/rules#window-rules) for
matching details.

### Why does a game freeze when its window is hidden?

Some games stop advancing their frame loop when their window is scrolled
off-screen, moved to another tag, or otherwise hidden. Enable `force_render` for
that game:

```ini
window_rule=force_render:1,app_id:WormsReloaded.bin.x86
```

This keeps sending frame callbacks to the hidden game surface so it can continue
rendering. It does not make a hidden window visible. The option can increase
GPU usage, so apply it only to games affected by this problem. See the
[window rule parameters](/docs/window-management/rules#state--behavior-parameters)
for related behavior options.

### Why does a game stutter only in fullscreen?

Fullscreen games normally use direct scanout, which can bypass the compositor
and significantly reduce rendering overhead. On some GPUs, direct scanout has
compatibility problems that appear as stuttering or other fullscreen issues.
Disable it by adding this environment variable to your configuration:

```ini
env=WLR_SCENE_DISABLE_DIRECT_SCANOUT,1
```

Log out and back in after changing it so the environment is applied to the new
mangowm session. Keep this workaround disabled unless you are affected: direct
scanout is an important performance optimization. See
[Environment Variables](/docs/configuration/basics#environment-variables) for
how `env` entries are loaded.

### Why does the display flicker when VRR is enabled?

If VRR causes flickering on the whole display, disable it in the matching
[monitor rule](/docs/configuration/monitors#monitor-rules) and enable it only
for fullscreen game windows:

```ini
# Add vrr:0 to the existing rule for the affected monitor.
monitor_rule=name:^DP-1$,vrr:0

# Replace the app ID with your game's application ID.
window_rule=vrr_only_fullscreen:1,force_render:1,app_id:WormsReloaded.bin.x86
```

`vrr_only_fullscreen:1` enables VRR only while the matching window is visible
and fullscreen. The monitor's global `vrr` option must remain `0`; otherwise VRR
stays enabled on the whole output.

> **Important:** Monitor rules are matched from top to bottom, and only the
> first matching rule is applied. Add `vrr:0` to your existing monitor rule
> instead of creating a conflicting rule for the same monitor.

For complete syntax and examples, see [Window Rules](/docs/window-management/rules)
and [Monitor Rules](/docs/configuration/monitors#monitor-rules).
