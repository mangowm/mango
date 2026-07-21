---
name: Bug report
about: Something in mango isn't working correctly
title: ""
labels: "A: bug"
assignees: ""
---

## Checklist

- [ ] I have tested this on the latest mango version (main branch) and the required wlroots / 我已使用最新版本测试过此问题

## Info

mango version(mango -v):
wlroots version:

## Crash track
1. you need to build mango by enabling the asan flag.
```bash
meson build -Dprefix=/usr -Dasan=true