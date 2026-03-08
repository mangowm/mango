---
name: Bug report
about: Something in mango isn't working correctly
title: ""
labels: "A: bug"
assignees: ""
---

## Info

<!--Paste mango version from running "mango -v"-->
<!--
Wlroots library needs to be built from this repository to avoid crashes
https://github.com/DreamMaoMao/wlroots.git
-->

mango version:
wlroots version:

## Crash track
1.you need to build mango by enable asan flag.
```bash
meson build -Dprefix=/usr -Dasan=true
``
2.run mango in tty.
```bash
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:log_path=/home/xxx/asan.log"
mango

```

3.after mango crash,paste the log file `/home/xxx/asan.log` here.

## Description

<!--
Only report bugs that can be reproduced on the main line
-->
