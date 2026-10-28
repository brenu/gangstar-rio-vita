#!/usr/bin/env python3
"""Install or update the Gangstar Rio diagnostic and collect VitaCompanion evidence."""
import argparse
import ftplib
import hashlib
import io
import json
from pathlib import Path
import socket
import struct
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TITLE = "G4HM00001"
MOUNT = "/ux0:"
APP = MOUNT + "/app/" + TITLE
DATA = MOUNT + "/data/gangstarrio"


def command(host, port, text):
    with socket.create_connection((host, port), timeout=10) as sock:
        sock.settimeout(20 if text.startswith("promote ") else 5)
        sock.sendall((text + "\n").encode())
        chunks = []
        while True:
            try:
                part = sock.recv(8192)
            except socket.timeout:
                if chunks:
                    break
                raise
            if not part:
                break
            chunks.append(part)
            if b"\n" in part:
                break
        result = b"".join(chunks).decode(errors="replace").strip()
        if not result:
            raise RuntimeError("No reply to " + text)
        print(text + ": " + result)
        return result


def download(ftp, remote):
    output = io.BytesIO()
    last_report = time.monotonic()

    def receive(block):
        nonlocal last_report
        output.write(block)
        now = time.monotonic()
        if now - last_report >= 15:
            print("Reading:", remote, output.tell(), "bytes", flush=True)
            last_report = now

    ftp.retrbinary("RETR " + remote, receive, blocksize=65536)
    return output.getvalue()


def mkdirs(ftp, remote):
    current = ""
    for part in remote.strip("/").split("/"):
        current += "/" + part
        try:
            ftp.mkd(current)
        except ftplib.error_perm:
            try:
                ftp.nlst(current)
            except ftplib.error_perm as exc:
                raise RuntimeError("Cannot create or inspect directory " + current) from exc


def upload_verified(ftp, remote, data):
    temporary = remote + ".upload"
    ftp.storbinary("STOR " + temporary, io.BytesIO(data), blocksize=65536)
    if download(ftp, temporary) != data:
        raise RuntimeError("Transfer verification failed: " + remote)
    try:
        ftp.delete(remote)
    except ftplib.error_perm:
        pass
    ftp.rename(temporary, remote)
    if download(ftp, remote) != data:
        raise RuntimeError("Installed verification failed: " + remote)
    print("Uploaded and verified:", remote, len(data), "bytes")


def install_probe(ftp, args, folder):
    package = args.vpk.resolve()
    raw = package.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("Corrupt VPK")
        names = archive.namelist()
        if any(name.endswith((".apk", ".obb", ".so")) for name in names):
            raise RuntimeError("Diagnostic VPK unexpectedly contains game data")
        required = {"eboot.bin", "sce_sys/param.sfo"}
        if not required.issubset(names):
            raise RuntimeError("VPK is missing eboot.bin or param.sfo")
        if sfo_string(archive.read("sce_sys/param.sfo"), "TITLE_ID") != TITLE:
            raise RuntimeError("Package is not the Gangstar Rio diagnostic")
        for mount in ("/ux0:", "/uma0:"):
            try:
                installed = download(ftp, mount + "/app/" + TITLE + "/sce_sys/param.sfo")
            except ftplib.error_perm:
                continue
            if sfo_string(installed, "TITLE_ID") == TITLE:
                raise RuntimeError("Gangstar title already exists on " + mount + "; use deploy after verifying its mount")

        app_stage = "/ux0:/data/gangstarrio-install-" + digest[:10]
        data_dir = "/ux0:/data/gangstarrio"
        mkdirs(ftp, app_stage)
        mkdirs(ftp, data_dir)
        for name in names:
            if name.endswith("/"):
                continue
            if name.startswith("/") or ".." in Path(name).parts:
                raise RuntimeError("Unsafe VPK path")
            parent = str(Path(app_stage, name).parent)
            mkdirs(ftp, parent)
            upload_verified(ftp, app_stage + "/" + name, archive.read(name))

    # The loader needs the game's own ARM ELF at boot. Keep it in the Vita
    # data folder, outside the VPK, and do not replace a conflicting file.
    library = ROOT / "work/libgl2jni.so"
    if not library.is_file():
        raise RuntimeError("Run tools/analyze.py first to extract libgl2jni.so")
    remote_library = data_dir + "/libgl2jni.so"
    ftp.voidcmd("TYPE I")
    try:
        existing_size = ftp.size(remote_library)
    except ftplib.error_perm:
        existing_size = None
    library_bytes = library.read_bytes()
    if existing_size is not None:
        if existing_size != len(library_bytes) or download(ftp, remote_library) != library_bytes:
            raise RuntimeError("A different libgl2jni.so already exists at " + remote_library)
        print("Existing game library matches APK extraction")
    else:
        upload_verified(ftp, remote_library, library_bytes)

    reply = command(args.host, args.command_port, "promote " + app_stage.lstrip("/"))
    if reply != "Promoted.":
        raise RuntimeError("Promotion failed: " + reply)
    (folder / "deployment.json").write_text(json.dumps({
        "vpk": str(package), "sha256": digest, "title_id": TITLE,
        "app_stage": app_stage, "mount": "/ux0:",
        "game_library": remote_library,
        "note": "Only the core .so was staged; APK and OBB files are not on the Vita yet."
    }, indent=2) + "\n")
    reply = command(args.host, args.command_port, "launch " + TITLE)
    if reply != "Launched.":
        raise RuntimeError("Launch was not confirmed")


def sfo_string(data, requested):
    magic, version, keys, values, count = struct.unpack_from("<5I", data)
    if magic != 0x46535000 or count > 256:
        raise ValueError("Invalid SFO header")
    for index in range(count):
        key, fmt, length, maximum, value = struct.unpack_from("<HHIII", data, 20 + index * 16)
        name = data[keys + key:].split(b"\0", 1)[0].decode()
        if name == requested:
            return data[values + value:values + value + length].rstrip(b"\0").decode()
    raise ValueError("Missing SFO field: " + requested)


def display_capture(ftp, args, folder):
    root = MOUNT + "/picture"
    def inventory(path, depth=0):
        files = set()
        try:
            entries = ftp.nlst(path + "/")
        except ftplib.error_perm:
            return files
        for entry in entries:
            name = entry.rsplit("/", 1)[-1]
            if name in (".", ".."): continue
            remote = path + "/" + name
            if name.lower().endswith((".png", ".jpg", ".jpeg")):
                files.add(remote)
            elif depth < 3:
                files.update(inventory(remote, depth + 1))
        return files
    before = inventory(root)
    try:
        command(args.host, args.command_port, "press ps; press start; wait 200ms; release start; release ps")
    finally:
        command(args.host, args.command_port, "release start; release ps")
    for attempt in range(10):
        time.sleep(0.5)
        added = inventory(root) - before
        if added:
            for index, remote in enumerate(sorted(added)):
                target = folder / ("display-" + str(index) + Path(remote).suffix)
                target.write_bytes(download(ftp, remote))
                print("Display capture:", target, "from", remote)
            return
    print("No new system screenshot found under", root)


def collect(ftp, folder, log_only=False, screenshots=False, latest_core=False, log_tail=0):
    (folder / "runtime-data-inventory.json").write_text(
        json.dumps(ftp.nlst(DATA + "/"), indent=2) + "\n")
    try:
        recipes = ftp.nlst(DATA + "/shader-warmup-v1/")
    except ftplib.error_perm:
        recipes = []
    (folder / "shader-warmup-inventory.json").write_text(json.dumps(recipes, indent=2) + "\n")
    print("Recorded shader pairs:", sum(entry.endswith(".rsw") for entry in recipes))
    try:
        (folder / "port.log").write_bytes(download(ftp, DATA + "/port.log"))
    except ftplib.error_perm as exc:
        print("Log unavailable:", exc)
    if screenshots:
        current_log = (folder / "port.log").read_text(errors="replace") if (folder / "port.log").exists() else ""
        for frame in (1, 30, 300):
            name = "frame-" + str(frame) + ".png"
            if not any("Gangstar framebuffer captured:" in line and line.endswith("/" + name)
                       for line in current_log.splitlines()):
                print("No capture confirmed in this run:", name)
                continue
            try:
                (folder / name).write_bytes(download(ftp, DATA + "/" + name))
                print("Screenshot:", folder / name)
            except ftplib.error_perm:
                print("Screenshot not available:", name)
    if log_only:
        log = (folder / "port.log").read_text(errors="replace")
        print("\n".join(log.splitlines()[-log_tail:]) if log_tail else log)
        print("Evidence:", folder)
        return
    # GPU faults can implicate a system module too. Keep recent complete
    # cores, then inspect their identity rather than inferring it from names.
    names = ftp.nlst(MOUNT + "/data/")
    (folder / "core-inventory.json").write_text(json.dumps(
        [name for name in names if name.rsplit("/", 1)[-1].startswith("psp2core-")],
        indent=2) + "\n")
    if latest_core:
        candidates = [n for n in names if n.rsplit("/", 1)[-1].startswith("psp2core-")]
        names = sorted(candidates, key=lambda n: int(n.rsplit("/", 1)[-1].split("-")[1]))[-1:]
    for name in names:
        basename = name.rsplit("/", 1)[-1]
        if basename.startswith("psp2core-") and (basename.endswith(".psp2dmp") or
                (latest_core and basename.endswith(".psp2dmp.tmp"))):
            try:
                created = int(basename.split("-", 2)[1])
            except (ValueError, IndexError):
                continue
            # Don't repeatedly download years of unrelated crash history.
            if created < time.time() - 7 * 86400:
                continue
            # Cores are immutable. Reuse an earlier download during boot loops.
            cached = next((p for p in (ROOT / "work/hardware").glob("*/" + basename)
                           if p.parent != folder and p.stat().st_size), None)
            if cached and not basename.endswith(".tmp"):
                (folder / basename).write_bytes(cached.read_bytes())
                continue
            (folder / basename).write_bytes(download(ftp, MOUNT + "/data/" + basename))
    print("Evidence:", folder)


def deploy(ftp, args, folder):
    package = args.vpk.resolve()
    raw = package.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("Corrupt VPK")
        names = archive.namelist()
        if any(name.endswith((".apk", ".obb", ".so")) for name in names):
            raise RuntimeError("Package unexpectedly contains game data")
        if any(name.startswith("/") or ".." in Path(name).parts for name in names):
            raise RuntimeError("Unsafe package path")
        # Fixed code-only deployment scope; refuse silently missing resources.
        required = {"eboot.bin", "sce_sys/param.sfo"}
        if not required.issubset(names):
            raise RuntimeError("Missing executable or metadata")
        sfo = archive.read("sce_sys/param.sfo")
        if sfo_string(sfo, "TITLE_ID") != TITLE:
            raise RuntimeError("Package is not the Gangstar Rio diagnostic")
        changed = []
        backup = folder / "installed-backup"
        for name in names:
            if name.endswith("/"):
                continue
            new = archive.read(name)
            old = download(ftp, APP + "/" + name)
            if old != new:
                target = backup / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(old)
                changed.append((name, new))
        (folder / "deployment.json").write_text(json.dumps({"vpk": str(package), "sha256": digest, "version": sfo_string(sfo, "APP_VER"), "ftp_mount": MOUNT, "changed": [name for name, _ in changed]}, indent=2) + "\n")
        # A retry must also stop an unchanged installed build: sending launch
        # alone can leave the same stalled process resident.
        if args.close_test_session or not args.app_closed:
            stop = "quit all" if args.close_test_session else "quit " + TITLE
            reply = command(args.host, args.command_port, stop)
            if reply not in ("Quit.", "Apps quit."):
                raise RuntimeError("Cannot confirm Gangstar Rio stopped; close it manually, then use --app-closed")
            time.sleep(1)
        if changed:
            for name, data in changed:
                remote = APP + "/" + name
                if args.direct_write:
                    print("Direct update with local backup:", remote, flush=True)
                    ftp.storbinary("STOR " + remote, io.BytesIO(data), blocksize=65536)
                    if download(ftp, remote) != data:
                        raise RuntimeError("Direct update verification failed; backup: " + str(backup / name))
                    print("Updated and verified:", name, len(data), "bytes")
                    continue
                # Stage outside app0's mounted directory; both locations are
                # on the selected partition, so the final rename stays atomic.
                temporary = DATA + "/deploy-" + digest[:12] + "-" + name.replace("/", "_") + ".upload"
                print("Uploading:", temporary, len(data), "bytes", flush=True)
                ftp.storbinary("STOR " + temporary, io.BytesIO(data))
                print("Verifying upload:", temporary, flush=True)
                if download(ftp, temporary) != data:
                    raise RuntimeError("Transfer verification failed: " + name)
                # This FTP server refuses rename over an existing file.
                # Preserve the old remote file and restore it if commit fails.
                previous = remote + ".previous-" + folder.name
                print("Committing:", remote, flush=True)
                ftp.rename(remote, previous)
                try:
                    ftp.rename(temporary, remote)
                except Exception:
                    ftp.rename(previous, remote)
                    raise
                if download(ftp, remote) != data:
                    raise RuntimeError("Installed verification failed: " + name)
                print("Updated and verified:", name, len(data), "bytes")
        else:
            print("Installed files already match package")
    reply = command(args.host, args.command_port, "launch " + TITLE)
    if reply != "Launched.":
        raise RuntimeError("Launch was not confirmed")


def main():
    global MOUNT, APP, DATA
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["ping", "reboot", "tap", "status", "collect", "deploy", "install-probe", "mount-check"])
    parser.add_argument("host")
    parser.add_argument("--ftp-port", type=int, default=1337)
    parser.add_argument("--command-port", type=int, default=1338)
    parser.add_argument("--ftp-mount", choices=["ux0", "uma0"], help="Select only after checking the mount in VitaShell")
    parser.add_argument("--app-closed", action="store_true", help="Use only after confirming Gangstar Rio is closed on the Vita")
    parser.add_argument("--close-test-session", action="store_true", help="Close foreground apps; use only during a dedicated Gangstar test session")
    parser.add_argument("--direct-write", action="store_true", help="Update installed files directly after saving local backups, then verify")
    parser.add_argument("--vpk", type=Path, help="VPK used by deploy or install-probe")
    parser.add_argument("--log-only", action="store_true", help="Read startup log without downloading cores")
    parser.add_argument("--hud-textures", action="store_true", help="Collect ETC1 atlas dumps from a HUD trace")
    parser.add_argument("--hud-trace", action="store_true", help="Request and collect one frame of HUD draw inputs")
    parser.add_argument("--shaders", action="store_true", help="Collect dumped original shader sources")
    parser.add_argument("--display-screenshot", action="store_true", help="Capture current screen with Vita PS+Start and collect the new photo")
    parser.add_argument("--screenshots", action="store_true", help="Collect startup framebuffer PNGs")
    parser.add_argument("--latest-core", action="store_true", help="Download only newest core, including an incomplete .tmp")
    parser.add_argument("--log-tail", type=int, default=0, help="Print only the final N log lines; always save the complete log")
    parser.add_argument("--x", type=int, default=480, help="Tap x in 960x544 game coordinates")
    parser.add_argument("--y", type=int, default=272, help="Tap y in 960x544 game coordinates")
    args = parser.parse_args()
    if args.action == "tap":
        if not (0 <= args.x < 960 and 0 <= args.y < 544):
            raise RuntimeError("Tap coordinates outside the game screen")
        try:
            reply = command(args.host, args.command_port,
                            f"press front-touch 0 {args.x * 2} {args.y * 2}")
            if reply != "Input pressed.":
                raise RuntimeError("Touch injection failed: " + reply)
            time.sleep(0.15)
        finally:
            command(args.host, args.command_port, "release front-touch 0")
        return
    if args.action in ("deploy", "install-probe") and args.vpk is None:
        args.vpk = ROOT / "work/build/gangstar_rio_wip.vpk"
    if args.action == "reboot":
        command(args.host, args.command_port, "reboot")
        return
    if args.action in ("ping", "status"):
        command(args.host, args.command_port, "version")
        if args.action == "ping":
            return
    with ftplib.FTP() as ftp:
        # Wi-Fi/storage can pause during app teardown or a file transfer.
        # Commands run in a resumable terminal so this does not block updates.
        ftp.connect(args.host, args.ftp_port, timeout=60)
        ftp.login()
        found = []
        for mount in ["/ux0:", "/uma0:"]:
            try:
                sfo = download(ftp, mount + "/app/" + TITLE + "/sce_sys/param.sfo")
            except ftplib.error_perm:
                continue
            if sfo_string(sfo, "TITLE_ID") == TITLE:
                found.append(mount)
        if not found:
            if args.action == "install-probe":
                folder = ROOT / "work/hardware" / time.strftime("companion-%Y%m%d-%H%M%S")
                folder.mkdir(parents=True, exist_ok=False)
                install_probe(ftp, args, folder)
                return
            raise RuntimeError("No Gangstar Rio installation found")
        if args.ftp_mount:
            MOUNT = "/" + args.ftp_mount + ":"
            if MOUNT not in found:
                raise RuntimeError("No Gangstar Rio app on selected FTP mount")
        elif len(found) == 1:
            MOUNT = found[0]
        else:
            raise RuntimeError("Multiple Gangstar Rio copies: explicitly select the verified --ftp-mount")
        if args.action == "install-probe":
            raise RuntimeError("Gangstar Rio is already installed; use deploy for an update")
        APP = MOUNT + "/app/" + TITLE
        DATA = MOUNT + "/data/gangstarrio"
        print("FTP storage view:", MOUNT)
        if args.action == "mount-check":
            marker = "/uma0:/data/gangstarrio/VITACOMPANION_MOUNT_CHECK.txt"
            payload = b"Written through VitaCompanion FTP /uma0:/data/gangstarrio/. Compare this file's location in VitaShell.\n"
            ftp.storbinary("STOR " + marker, io.BytesIO(payload))
            if download(ftp, marker) != payload:
                raise RuntimeError("Marker verification failed")
            print("Mount-check marker:", marker)
            return
        if args.action == "status":
            print("Installed app:", ftp.nlst(APP + "/"))
            entries = ftp.nlst(DATA + "/")
            print("Game directory entries:", entries)
            ftp.voidcmd("TYPE I")
            for entry in entries:
                try:
                    print("Game file:", entry, "size:", ftp.size(DATA + "/" + entry))
                except ftplib.error_perm as exc:
                    print("Game file:", entry, str(exc))
            print("Dump directory:", ftp.nlst(MOUNT + "/data/"))
            for mount in ["/ux0:", "/uma0:"]:
                for suffix in ["", ".upload"]:
                    remote = mount + "/data/gangstarrio/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb" + suffix
                    try:
                        print("OBB path:", remote, "size:", ftp.size(remote))
                    except ftplib.error_perm as exc:
                        print("OBB path:", remote, str(exc))
            return
        folder = ROOT / "work/hardware" / time.strftime("companion-%Y%m%d-%H%M%S")
        folder.mkdir(parents=True, exist_ok=False)
        if args.hud_trace:
            ftp.storbinary("STOR " + DATA + "/hud-trace.request", io.BytesIO(b"one frame\n"))
            for attempt in range(30):
                time.sleep(0.5)
                if "hud-trace.request" not in ftp.nlst(DATA + "/"):
                    time.sleep(0.5)
                    (folder / "hud-trace.log").write_bytes(download(ftp, DATA + "/hud-trace.log"))
                    print("HUD trace:", folder / "hud-trace.log")
                    break
            else:
                raise RuntimeError("HUD trace request was not consumed")
        if args.hud_textures:
            target = folder / "hud-textures"
            target.mkdir()
            for entry in ftp.nlst(DATA + "/hud-textures/"):
                name = entry.rsplit("/", 1)[-1]
                if name.startswith("tex-") and name.endswith(".etc1") and all(c in "0123456789-" for c in name[4:-5]):
                    (target / name).write_bytes(download(ftp, DATA + "/hud-textures/" + name))
            print("HUD texture dumps:", target)
        if args.display_screenshot:
            display_capture(ftp, args, folder)
        if args.shaders:
            remote = DATA + "/shader-diagnostics"
            target = folder / "shader-diagnostics"
            target.mkdir()
            try:
                entries = ftp.nlst(remote + "/")
            except ftplib.error_perm:
                entries = []
            for entry in entries:
                name = entry.rsplit("/", 1)[-1]
                if len(name) != 45 or not name.endswith(".glsl") or any(c not in "0123456789abcdefABCDEF" for c in name[:-5]):
                    continue
                (target / name).write_bytes(download(ftp, remote + "/" + name))
            print("Shader sources:", len(list(target.glob("*.glsl"))), target)
        collect(ftp, folder, args.log_only, args.screenshots, args.latest_core, args.log_tail)
        if args.action == "deploy":
            deploy(ftp, args, folder)


if __name__ == "__main__":
    try:
        main()
    except (OSError, EOFError, ValueError, RuntimeError, ftplib.Error) as exc:
        print("Vita test stopped:", exc, file=sys.stderr)
        sys.exit(1)
