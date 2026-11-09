#!/usr/bin/env python3

import argparse
import json
import os
from pathlib import Path
import selectors
import shlex
import shutil
import socket
import subprocess
import tempfile
import time


def rectangle(client):
    return tuple(client[key] for key in ("x", "y", "width", "height"))


def assert_floating_state(client, explicit, effective):
    assert client["is_floating"] is explicit
    assert client["is_effectively_floating"] is effective


def build_drag_client(directory, protocol_directory):
    wayland_protocols = Path(subprocess.check_output(
        ["pkg-config", "--variable=pkgdatadir", "wayland-protocols"], text=True,
    ).strip())
    protocols = {
        "xdg-shell": wayland_protocols / "stable/xdg-shell/xdg-shell.xml",
        "virtual-pointer": protocol_directory / "unstable/wlr-virtual-pointer-unstable-v1.xml",
    }
    sources = []
    for name, xml in protocols.items():
        source = directory / (name + ".c")
        subprocess.run(["wayland-scanner", "client-header", str(xml),
                        str(directory / (name + "-client.h"))], check=True)
        subprocess.run(["wayland-scanner", "private-code", str(xml), str(source)], check=True)
        sources.append(str(source))
    flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "wayland-client"], text=True,
    ))
    has_xcb = subprocess.run(["pkg-config", "--exists", "xcb"]).returncode == 0
    if has_xcb:
        flags.extend(["-DHAVE_XCB", *shlex.split(subprocess.check_output(
            ["pkg-config", "--cflags", "--libs", "xcb"], text=True,
        ))])
    executable = directory / "floating-drag-client"
    subprocess.run([os.environ.get("CC", "cc"), "-Wall", "-Wextra",
                    "-Wno-unused-parameter", "-I", str(directory),
                    str(Path(__file__).with_name("floating-drag-client.c")),
                    *sources, "-o", str(executable), *flags], check=True)
    return executable, has_xcb


def run(build_dir, terminal, pointer_tool, drag_protocols=None, top_drag_zone=None):
    processes = []
    effective_top_drag_zone = 0 if top_drag_zone is None else top_drag_zone
    with tempfile.TemporaryDirectory(prefix="mango-floating-") as temporary:
        directory = Path(temporary)
        config = directory / ("config.toml" if top_drag_zone == 12 else "config.conf")
        conf_text = (
            "animations=0\nlayer_animations=0\nblur=0\nshadows=0\n"
            "enable_floating_snap=0\nfocus_on_activate=0\n"
            "circle_layout=tile,floating\n"
            "tag_rule=id:3,layout_name:floating\n"
        )
        if top_drag_zone == 12:
            config_text = (
                "[global]\nanimations=0\nlayer_animations=0\nblur=0\nshadows=0\n"
                "enable_floating_snap=0\nfocus_on_activate=0\n"
                'circle_layout=["tile","floating"]\n'
                f"top_drag_zone={top_drag_zone}\n"
                '[[rule.tag_rule]]\nid=3\nlayout_name="floating"\n'
            )
        elif top_drag_zone == 0:
            config_text = conf_text + "top_drag_zone=0\n"
        else:
            config_text = conf_text
        config.write_text(config_text, encoding="ascii")
        environment = os.environ.copy()
        for key in ("DBUS_SESSION_BUS_ADDRESS", "DISPLAY", "WAYLAND_DISPLAY",
                    "MANGO_INSTANCE_SIGNATURE"):
            environment.pop(key, None)
        environment.update(XDG_RUNTIME_DIR=temporary, WLR_BACKENDS="headless",
                           WLR_HEADLESS_OUTPUTS="2")
        read_fd, write_fd = os.pipe()
        with (directory / "session.log").open("w+") as log:
            compositor = subprocess.Popen(
                [str(build_dir / "mango"), "-c", str(config), "-r", str(write_fd)],
                env=environment, pass_fds=(write_fd,), stdout=log, stderr=log,
            )
            processes.append(compositor)
            os.close(write_fd)
            try:
                with selectors.DefaultSelector() as selector:
                    selector.register(read_fd, selectors.EVENT_READ)
                    assert selector.select(15), "compositor startup timed out"
                    assert os.read(read_fd, 1) == b"\n", "compositor failed to start"
                ipc_path = directory / f"mango-{compositor.pid}.sock"
                displays = [path for path in directory.glob("wayland-*")
                            if path.is_socket()]
                assert len(displays) == 1, displays
                environment["WAYLAND_DISPLAY"] = displays[0].name

                def request(command):
                    with socket.socket(socket.AF_UNIX) as connection:
                        connection.settimeout(5)
                        connection.connect(str(ipc_path))
                        connection.sendall((command + "\n").encode())
                        with connection.makefile("r") as stream:
                            return json.loads(stream.readline())

                def dispatch(command, client=None):
                    suffix = f" client,{client}" if client is not None else ""
                    response = request("dispatch " + command + suffix)
                    assert not response.get("error"), response

                def clients():
                    result = request("get all-clients")
                    return result if isinstance(result, list) else result["clients"]

                def client_state(client):
                    return next(item for item in clients() if item["id"] == client)

                if drag_protocols:
                    executable, has_xcb = build_drag_client(directory, drag_protocols)
                    dispatch("setlayout,floating")
                    fixture = subprocess.Popen(
                        [str(executable)], env=environment, stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=log, text=True, bufsize=1,
                    )
                    processes.append(fixture)

                    def fixture_response():
                        with selectors.DefaultSelector() as selector:
                            selector.register(fixture.stdout, selectors.EVENT_READ)
                            assert selector.select(10), "drag fixture timed out"
                        line = fixture.stdout.readline()
                        assert line, "drag fixture exited"
                        return json.loads(line)

                    assert fixture_response()["ready"]
                    fixture_id = next(item["id"] for item in clients()
                                      if item["appid"] == "mango-floating-drag-test")

                    def inject(command):
                        fixture.stdin.write(command + "\n")
                        fixture.stdin.flush()
                        return fixture_response()

                    def warp(x, y):
                        current = request("get cursorpos")
                        inject(f"move {x - current['x']} {y - current['y']}")

                    dispatch("movewin,80,90", fixture_id)
                    dispatch("resizewin,420,280", fixture_id)
                    warp(120, 130)
                    inject("mode 1")
                    inject("press 272")
                    warp(170, 170)
                    assert rectangle(client_state(fixture_id))[:2] == (130, 130)
                    inject("release 272")
                    dropped = rectangle(client_state(fixture_id))
                    warp(200, 200)
                    assert rectangle(client_state(fixture_id)) == dropped
                    dispatch("setlayout,tile")
                    assert_floating_state(client_state(fixture_id), False, False)
                    dispatch("setlayout,floating")
                    assert rectangle(client_state(fixture_id)) == dropped
                    print("PASS: native titlebar press-motion-release and geometry persistence")
                    inject("mode 2")
                    warp(170, 170)
                    inject("press 272")
                    warp(190, 190)
                    assert rectangle(client_state(fixture_id)) == dropped
                    inject("release 272")
                    inject("stale")
                    warp(210, 210)
                    assert rectangle(client_state(fixture_id)) == dropped
                    print("PASS: invalid and released pointer serials cannot start a move")

                    dispatch("setlayout,tile")
                    tiled = rectangle(client_state(fixture_id))
                    inject("mode 1")
                    warp(tiled[0] + 50, tiled[1] + 50)
                    inject("press 272")
                    inject("move 30 20")
                    inject("release 272")
                    assert rectangle(client_state(fixture_id)) == tiled
                    dispatch("togglefloating", fixture_id)
                    dispatch("movewin,80,90", fixture_id)
                    warp(120, 130)
                    inject("press 272")
                    warp(170, 170)
                    inject("release 272")
                    assert rectangle(client_state(fixture_id))[:2] == (130, 130)
                    dispatch("togglefloating", fixture_id)
                    dispatch("setlayout,floating")
                    print("PASS: native dragging respects tiled and explicit-floating state")

                    dispatch("movewin,80,90", fixture_id)
                    dispatch("resizewin,420,280", fixture_id)
                    warp(120, 130)
                    inject("mode 1")
                    inject("press 272")
                    inject("move 20 20")
                    inject("press 273")
                    inject("release 273")
                    inject("move 10 10")
                    inject("release 272")
                    assert rectangle(client_state(fixture_id))[:2] == (110, 120)
                    released = rectangle(client_state(fixture_id))
                    inject("move 10 10")
                    assert rectangle(client_state(fixture_id)) == released
                    inject("mode 0")
                    counts = inject("state")
                    inject("press 272")
                    next_click = inject("release 272")
                    assert next_click["presses"] == counts["presses"] + 1
                    assert next_click["releases"] == counts["releases"] + 1
                    print("PASS: extra buttons do not interrupt a native drag; next click works")

                    inject("mode 1")
                    inject("press 272")
                    inject("move 10 10")
                    dispatch("setlayout,tile")
                    canceled = rectangle(client_state(fixture_id))
                    inject("move 20 20")
                    inject("release 272")
                    assert rectangle(client_state(fixture_id)) == canceled
                    dispatch("setlayout,floating")
                    inject("mode 0")
                    print("PASS: changing to a tiled layout cancels the active native drag")

                    if has_xcb:
                        display_socket = directory / "display.sock"
                        with socket.socket(socket.AF_UNIX) as listener:
                            listener.settimeout(10)
                            listener.bind(str(display_socket))
                            listener.listen(1)
                            dispatch(f"spawn,{executable} --display-socket {display_socket}")
                            connection, _ = listener.accept()
                            with connection:
                                connection.settimeout(5)
                                xdisplay = connection.recv(64).decode()
                        if xdisplay:
                            inject("x11 " + xdisplay)
                            deadline = time.monotonic() + 10
                            xclient = None
                            while time.monotonic() < deadline:
                                xclient = next((item for item in clients()
                                                if item["title"] == "mango-floating-x11-test"), None)
                                if xclient:
                                    break
                            assert xclient, ("XWayland test window did not map", clients())
                            xclient_id = xclient["id"]
                            dispatch("movewin,80,90", xclient_id)
                            dispatch("resizewin,420,280", xclient_id)
                            warp(120, 130)
                            inject("press 272")
                            inject("xmove")
                            inject("move 50 40")
                            inject("release 272")
                            xrect = rectangle(client_state(xclient_id))
                            assert xrect[:2] == (130, 130), xrect
                            inject("move 20 20")
                            assert rectangle(client_state(xclient_id)) == xrect
                            inject("xmove")
                            inject("move 20 20")
                            assert rectangle(client_state(xclient_id)) == xrect
                            dispatch("setlayout,tile")
                            assert_floating_state(client_state(xclient_id), False, False)
                            dispatch("setlayout,floating")
                            assert rectangle(client_state(xclient_id)) == xrect
                            inject("xclose")
                            print("PASS: XWayland titlebar move requests and release validation")
                        else:
                            print("SKIP: XWayland runtime (compositor has no DISPLAY)")
                    else:
                        print("SKIP: XWayland drag coverage (xcb development files unavailable)")

                    dispatch("movewin,80,90", fixture_id)
                    dispatch("resizewin,420,280", fixture_id)
                    before_strip = rectangle(client_state(fixture_id))
                    strip_x = before_strip[0] + 40
                    strip_y = before_strip[1] + 13
                    warp(strip_x, strip_y)
                    counts = inject("state")
                    inject("press 272")
                    inject("move 2 1")
                    assert rectangle(client_state(fixture_id)) == before_strip, (
                        before_strip, rectangle(client_state(fixture_id)), strip_x, strip_y
                    )
                    inject("release 272")
                    after_click = inject("state")
                    expected_clicks = 0 if effective_top_drag_zone else 1
                    assert after_click["presses"] == counts["presses"] + expected_clicks
                    assert after_click["releases"] == counts["releases"] + expected_clicks
                    warp(strip_x, strip_y)
                    inject("press 272")
                    inject("move 50 40")
                    inject("release 272")
                    after_strip = rectangle(client_state(fixture_id))
                    if effective_top_drag_zone:
                        assert after_strip[:2] == (130, 130), after_strip
                        assert inject("state") == after_click
                        inject("move 30 20")
                        assert rectangle(client_state(fixture_id)) == after_strip
                        dispatch("setlayout,tile")
                        assert_floating_state(client_state(fixture_id), False, False)
                        dispatch("setlayout,floating")
                        assert rectangle(client_state(fixture_id)) == after_strip
                        print("PASS: invisible strip threshold, paired input capture and release")

                        dispatch("movewin,80,90", fixture_id)
                        dispatch("resizewin,420,280", fixture_id)
                        warp(82, strip_y)
                        inject("press 272")
                        inject("frame")
                        inject("move 20 0")
                        inject("release 272")
                        border_resize = rectangle(client_state(fixture_id))
                        assert border_resize[0] == 100, border_resize
                        assert border_resize[2] == 400, border_resize
                        print("PASS: border resizing takes priority over the invisible strip")

                        dispatch("movewin,80,90", fixture_id)
                        warp(strip_x, strip_y)
                        inject("press 272")
                        dispatch("setoption,top_drag_zone,0")
                        disabled = rectangle(client_state(fixture_id))
                        inject("move 30 20")
                        inject("release 272")
                        assert rectangle(client_state(fixture_id)) == disabled
                        dispatch(f"setoption,top_drag_zone,{effective_top_drag_zone}")
                        print("PASS: disabling the strip cancels its pending gesture")
                    else:
                        assert after_strip == before_strip
                        forwarded = inject("state")
                        assert forwarded["presses"] == after_click["presses"] + 1
                        assert forwarded["releases"] == after_click["releases"] + 1
                        print("PASS: disabled strip leaves ordinary top-content input untouched")

                    if effective_top_drag_zone:
                        dispatch("movewin,80,90", fixture_id)
                        warp(strip_x, strip_y)
                        inject("press 272")
                        inject("unmap")
                        inject("move 30 20")
                        inject("release 272")
                        print("PASS: unmapping a pending strip window safely cancels the gesture")
                    inject("unmap")
                    fixture.terminate()
                    fixture.wait(timeout=5)

                def pointer_move_to(x, y):
                    current = request("get cursorpos")
                    subprocess.run(
                        [pointer_tool, "pointer", "move", str(x - current["x"]),
                         str(y - current["y"])],
                        env=environment, check=True, stdout=log, stderr=log,
                        timeout=5,
                    )

                def pointer_release():
                    subprocess.run(
                        [pointer_tool, "pointer", "click", "left"],
                        env=environment, check=True, stdout=log, stderr=log,
                        timeout=5,
                    )

                def launch(name):
                    process = subprocess.Popen(
                        [terminal, "--app-id=" + name, "--title=" + name,
                         "--override=initial-window-size-pixels=420x280",
                         "--", "cat"],
                        env=environment, stdin=subprocess.PIPE, stdout=log, stderr=log,
                    )
                    processes.append(process)
                    deadline = time.monotonic() + 10
                    while time.monotonic() < deadline:
                        assert process.poll() is None, "test terminal exited"
                        for item in clients():
                            if item["appid"] == name:
                                return item["id"]
                    raise AssertionError("terminal did not map: " + name)

                monitors = request("get all-monitors")
                if isinstance(monitors, dict):
                    monitors = monitors["monitors"]
                assert len(monitors) == 2, monitors
                dispatch("setlayout,tile")
                first = launch("mango-floating-first")
                second = launch("mango-floating-second")
                assert_floating_state(client_state(first), False, False)

                dispatch("setlayout,floating")
                assert all(not item["is_floating"] and
                           item["is_effectively_floating"] for item in clients())
                dispatch("movewin,80,90", first)
                dispatch("resizewin,360,240", first)
                saved = rectangle(client_state(first))
                assert saved[:2] == (80, 90), saved
                dispatch("setlayout,floating")
                assert rectangle(client_state(first)) == saved
                dispatch("setlayout,tile")
                assert_floating_state(client_state(first), False, False)
                assert rectangle(client_state(first)) != saved
                dispatch("setlayout,floating")
                assert rectangle(client_state(first)) == saved
                print("PASS: selection, keyboard move/resize, geometry round-trip")

                for command, flag in (("togglefullscreen", "is_fullscreen"),
                                      ("togglemaximizescreen", "is_maximized")):
                    dispatch(command, first)
                    assert client_state(first)[flag]
                    dispatch(command, first)
                    assert not client_state(first)[flag]
                    assert rectangle(client_state(first)) == saved
                    dispatch("setlayout,tile")
                    assert_floating_state(client_state(first), False, False)
                    dispatch("setlayout,floating")
                    assert rectangle(client_state(first)) == saved
                print("PASS: fullscreen/maximize restore without permanent floating")

                dispatch("toggleoverview")
                dispatch("toggleoverview")
                assert rectangle(client_state(first)) == saved
                print("PASS: overview preserves floating geometry")

                third = launch("mango-floating-new")
                assert_floating_state(client_state(third), False, True)
                assert rectangle(client_state(first)) == saved
                dispatch("togglefullscreen", first)
                launch("mango-floating-fullscreen-peer")
                assert client_state(first)["is_fullscreen"]
                dispatch("togglefullscreen", first)
                assert rectangle(client_state(first)) == saved
                dispatch("togglefloating", second)
                dispatch("setlayout,tile")
                assert_floating_state(client_state(second), True, True)
                assert_floating_state(client_state(first), False, False)
                assert_floating_state(client_state(third), False, False)
                dispatch("setlayout,floating")
                assert rectangle(client_state(first)) == saved
                print("PASS: new windows float; explicit floating intent survives")

                dispatch("view,2")
                dispatch("setlayout,tile")
                assert_floating_state(client_state(first), False, True)
                dispatch("view,3")
                ruled = request("get all-monitors")
                if isinstance(ruled, dict):
                    ruled = ruled["monitors"]
                assert next(item for item in ruled if item["active"])["layout_symbol"] == "FL"
                dispatch("view,1")
                assert_floating_state(client_state(first), False, True)
                assert rectangle(client_state(first)) == saved
                active = request("get all-monitors")
                if isinstance(active, dict):
                    active = active["monitors"]
                assert sum(item["layout_symbol"] == "FL" for item in active) == 1
                dispatch("switch_layout")
                assert_floating_state(client_state(first), False, False)
                dispatch("switch_layout")
                assert rectangle(client_state(first)) == saved
                print("PASS: tag rules, tag/monitor isolation and layout cycling")

                if pointer_tool:
                    dispatch("movewin,80,90", first)
                    dispatch("resizewin,360,240", first)
                    pointer_move_to(120, 130)
                    dispatch("moveresize,curmove")
                    pointer_move_to(170, 170)
                    pointer_release()
                    moved = rectangle(client_state(first))
                    assert moved[:2] == (130, 130), moved
                    pointer_move_to(180, 180)
                    dispatch("moveresize,curresize")
                    current = request("get cursorpos")
                    pointer_move_to(current["x"] + 45, current["y"] + 35)
                    pointer_release()
                    resized = rectangle(client_state(first))
                    assert resized[2:] != moved[2:], (moved, resized)
                    dispatch("setlayout,tile")
                    assert_floating_state(client_state(first), False, False)
                    dispatch("setlayout,floating")
                    assert rectangle(client_state(first)) == resized
                    print("PASS: pointer move/resize preserve reversible floating state")
                else:
                    print("SKIP: pointer grabs (pass --pointer-tool /path/to/wlrctl)")

                source = client_state(first)["monitor"]
                destination = next(item["name"] for item in monitors
                                   if item["name"] != source)
                dispatch("tagmon," + destination, first)
                assert client_state(first)["monitor"] == destination
                assert_floating_state(client_state(first), False, False)
                dispatch("tagmon," + source, first)
                assert_floating_state(client_state(first), False, True)
                dispatch("setlayout,tile")
                assert_floating_state(client_state(first), False, False)
                print("PASS: floating/tiled monitor transfers preserve explicit intent")
            except BaseException:
                log.flush()
                log.seek(0)
                print(log.read())
                raise
            finally:
                os.close(read_fd)
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Isolated floating-layout IPC regression checks",
        epilog=("Without --drag-protocols, native drag, drag-strip, and XWayland "
                "coverage is skipped."),
    )
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--terminal", default=shutil.which("foot"))
    parser.add_argument("--pointer-tool", default=shutil.which("wlrctl"))
    parser.add_argument("--drag-protocols", type=Path,
                        help="wlr-protocols data directory; enables real native drag tests")
    arguments = parser.parse_args()
    if not arguments.terminal:
        parser.error("foot is required; pass --terminal /path/to/foot")
    if not arguments.drag_protocols:
        print("SKIP: native drag, drag-strip, and XWayland coverage "
              "(pass --drag-protocols /path/to/wlr-protocols)")
    for zone in ((None, 0, 12) if arguments.drag_protocols else (None, 0)):
        if arguments.drag_protocols:
            print(f"Testing with top_drag_zone={0 if zone is None else zone}")
        run(arguments.build_dir.resolve(), arguments.terminal, arguments.pointer_tool,
            arguments.drag_protocols, zone)