#!/usr/bin/env python3

import argparse
import json
import os
from pathlib import Path
import selectors
import shutil
import socket
import subprocess
import tempfile
import time


def rectangle(client):
    return tuple(client[key] for key in ("x", "y", "width", "height"))


def run(build_dir, terminal, pointer_tool):
    processes = []
    with tempfile.TemporaryDirectory(prefix="mango-floating-") as temporary:
        directory = Path(temporary)
        config = directory / "config.conf"
        config.write_text(
            "animations=0\nlayer_animations=0\nblur=0\nshadows=0\n"
            "enable_floating_snap=0\nfocus_on_activate=0\n"
            "circle_layout=tile,floating\n"
            "tag_rule=id:3,layout_name:floating\n",
            encoding="ascii",
        )
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
                assert not client_state(first)["is_floating"]

                dispatch("setlayout,floating")
                assert all(item["is_floating"] for item in clients())
                dispatch("movewin,80,90", first)
                dispatch("resizewin,360,240", first)
                saved = rectangle(client_state(first))
                assert saved[:2] == (80, 90), saved
                dispatch("setlayout,floating")
                assert rectangle(client_state(first)) == saved
                dispatch("setlayout,tile")
                assert not client_state(first)["is_floating"]
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
                    assert not client_state(first)["is_floating"]
                    dispatch("setlayout,floating")
                    assert rectangle(client_state(first)) == saved
                print("PASS: fullscreen/maximize restore without permanent floating")

                dispatch("toggleoverview")
                dispatch("toggleoverview")
                assert rectangle(client_state(first)) == saved
                print("PASS: overview preserves floating geometry")

                third = launch("mango-floating-new")
                assert client_state(third)["is_floating"]
                assert rectangle(client_state(first)) == saved
                dispatch("togglefullscreen", first)
                launch("mango-floating-fullscreen-peer")
                assert client_state(first)["is_fullscreen"]
                dispatch("togglefullscreen", first)
                assert rectangle(client_state(first)) == saved
                dispatch("togglefloating", second)
                dispatch("setlayout,tile")
                assert client_state(second)["is_floating"]
                assert not client_state(first)["is_floating"]
                assert not client_state(third)["is_floating"]
                dispatch("setlayout,floating")
                assert rectangle(client_state(first)) == saved
                print("PASS: new windows float; explicit floating intent survives")

                dispatch("view,2")
                dispatch("setlayout,tile")
                dispatch("view,3")
                ruled = request("get all-monitors")
                if isinstance(ruled, dict):
                    ruled = ruled["monitors"]
                assert next(item for item in ruled if item["active"])["layout_symbol"] == "FL"
                dispatch("view,1")
                assert client_state(first)["is_floating"]
                assert rectangle(client_state(first)) == saved
                active = request("get all-monitors")
                if isinstance(active, dict):
                    active = active["monitors"]
                assert sum(item["layout_symbol"] == "FL" for item in active) == 1
                dispatch("switch_layout")
                assert not client_state(first)["is_floating"]
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
                    assert not client_state(first)["is_floating"]
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
                assert not client_state(first)["is_floating"]
                dispatch("tagmon," + source, first)
                assert client_state(first)["is_floating"]
                dispatch("setlayout,tile")
                assert not client_state(first)["is_floating"]
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
    parser = argparse.ArgumentParser(description="Isolated floating-layout IPC regression checks")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--terminal", default=shutil.which("foot"))
    parser.add_argument("--pointer-tool", default=shutil.which("wlrctl"))
    arguments = parser.parse_args()
    if not arguments.terminal:
        parser.error("foot is required; pass --terminal /path/to/foot")
    run(arguments.build_dir.resolve(), arguments.terminal, arguments.pointer_tool)