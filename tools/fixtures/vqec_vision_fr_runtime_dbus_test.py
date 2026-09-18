#!/usr/bin/env python3
"""Native FW D-Bus simulation; reads private deployment fixtures outside Git."""

import argparse
import json
import time
import uuid
from pathlib import Path

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib

# D-Bus/FR v1 contract spellings and wire ordinals, not deployment choices.
g_usecase_interface = "com.vqec.AiVision.UsecaseControl1"
g_enrollment_interface = "com.vqec.Lacai.FaceEnrollment1"
g_dbus_name = "org.freedesktop.DBus"
g_dbus_path = "/org/freedesktop/DBus"
g_name_acquired = 1
g_do_not_queue = 4
g_terminal_states = {2, 3, 4}
g_completed_state = 2
g_failed_state = 4
g_fixture_max_bytes = 65536
g_max_test_templates = 16


def vqec_vision_ai_tools_frdbt_call(_bus, _config, _domain, _method, _parameters=None):
    endpoint = _config[_domain]
    interface = g_usecase_interface if _domain == "control" else g_enrollment_interface
    return _bus.call_sync(
        endpoint["service"], endpoint["object"], interface, _method, _parameters,
        None, Gio.DBusCallFlags.NONE, _config["rpc_timeout_ms"], None).unpack()


def vqec_vision_ai_tools_frdbt_wait(_bus, _config, _method, _predicate, _parameters=None):
    deadline = time.monotonic() + _config["transition_timeout_seconds"]
    domain = "control" if _method == "GetUsecaseStatus" else "enrollment"
    while time.monotonic() < deadline:
        try:
            result = vqec_vision_ai_tools_frdbt_call(
                _bus, _config, domain, _method, _parameters)
            if _predicate(result):
                return result
        except GLib.Error as error:
            # Endpoint startup/rebuild is asynchronous; authorization errors are fatal.
            if "AccessDenied" in str(error):
                raise
        time.sleep(_config["poll_seconds"])
    raise AssertionError(f"timeout waiting for {_method}")


def vqec_vision_ai_tools_frdbt_check_models(_config, _expected):
    executable = str(Path(_config["service_executable"]).resolve())
    pids = []
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if str((entry / "exe").resolve()) == executable:
                pids.append(entry.name)
        except OSError:
            continue
    if len(pids) != 1:
        raise AssertionError("expected exactly one configured AI service process")
    maps = Path("/proc", pids[0], "maps").read_text()
    actual = {key: value in maps for key, value in _config["model_libraries"].items()}
    if actual != _expected:
        raise AssertionError(f"resident model libraries differ: {actual}")
    return pids[0]


def vqec_vision_ai_tools_frdbt_reject(_callback, _access_denied=False):
    try:
        _callback()
    except GLib.Error as error:
        if _access_denied and "AccessDenied" not in str(error):
            raise AssertionError("unauthorized call failed for an unrelated reason") from error
        return
    raise AssertionError("invalid D-Bus operation was accepted")


def vqec_vision_ai_tools_frdbt_begin(_bus, _config, _request, _subject, _revision,
                                   _path=None, _source=None, _camera=None, _samples=1):
    return vqec_vision_ai_tools_frdbt_call(
        _bus, _config, "enrollment", "BeginEnrollment",
        GLib.Variant("(ssssuutut)", (
            _request, _subject, _config["image_path"] if _path is None else _path,
            _config["source_id"] if _source is None else _source,
            _config["camera_id"] if _camera is None else _camera,
            _config["channel_id"], 0, _samples, _revision)))


def vqec_vision_ai_tools_frdbt_run(_config):
    bus_type = Gio.BusType.SESSION if _config["session_bus"] else Gio.BusType.SYSTEM
    bus = Gio.bus_get_sync(bus_type, None)
    owned = bus.call_sync(
        g_dbus_name, g_dbus_path, g_dbus_name, "RequestName",
        GLib.Variant("(su)", (_config["peer_name"], g_do_not_queue)),
        GLib.VariantType.new("(u)"), Gio.DBusCallFlags.NONE,
        _config["rpc_timeout_ms"], None).unpack()[0]
    if owned != g_name_acquired:
        raise AssertionError("configured FW test peer is already owned")
    current = vqec_vision_ai_tools_frdbt_wait(
        bus, _config, "GetUsecaseStatus", lambda _reply: _reply[2] != 0)
    initial_models = _config["initial_models"]
    service_pid = vqec_vision_ai_tools_frdbt_check_models(_config, initial_models)
    baseline = vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "GetGalleryStatus")
    address = Gio.dbus_address_get_for_bus_sync(bus_type, None)
    intruder = Gio.DBusConnection.new_for_address_sync(
        address, Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT |
        Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
    for domain, method, parameters in (
        ("control", "GetUsecaseStatus", None),
        ("control", "GetCapabilities", None),
        ("control", "ApplyDesiredPlan", GLib.Variant("(sta(ssb))", ("intruder", current[0], []))),
        ("enrollment", "GetGalleryStatus", None),
        ("enrollment", "GetEnrollmentStatus", GLib.Variant("(s)", ("intruder",))),
        ("enrollment", "CancelEnrollment", GLib.Variant("(s)", ("intruder",))),
        ("enrollment", "RemoveSubject", GLib.Variant("(st)", ("intruder", baseline[0]))),
        ("enrollment", "BeginEnrollment", GLib.Variant("(ssssuutut)", (
            "intruder", "intruder", "", _config["source_id"], _config["camera_id"],
            _config["channel_id"], 0, 1, baseline[0]))),
    ):
        vqec_vision_ai_tools_frdbt_reject(
            lambda: vqec_vision_ai_tools_frdbt_call(intruder, _config, domain, method, parameters), True)
    intruder.close_sync(None)
    run_id = "fr_test_" + uuid.uuid4().hex
    for index, case in enumerate(_config["transitions"]):
        parameters = GLib.Variant("(sta(ssb))", (run_id + str(index), current[0],
            [(_config["source_id"], key, value) for key, value in case["desired"].items()]))
        receipt = vqec_vision_ai_tools_frdbt_call(bus, _config, "control", "ApplyDesiredPlan", parameters)
        if not receipt[0] or receipt[2] != "reconciling":
            raise AssertionError("test transition must change the desired plan")
        generation = current[2] + 1
        current = vqec_vision_ai_tools_frdbt_wait(bus, _config, "GetUsecaseStatus",
            lambda _reply: _reply[0] == receipt[1] and _reply[2] == generation)
        for entry in current[3]:
            if entry[0] != _config["source_id"]:
                continue
            expected = case["desired"].get(entry[1], False)
            if entry[8] != expected or entry[9] != expected:
                raise AssertionError("loaded/running state differs from desired fixture")
        if vqec_vision_ai_tools_frdbt_check_models(_config, case["models"]) != service_pid:
            raise AssertionError("service process changed during live transition")
        if vqec_vision_ai_tools_frdbt_call(bus, _config, "control", "ApplyDesiredPlan", parameters) != receipt:
            raise AssertionError("retry changed the desired-plan receipt")
        fr_enabled = case["desired"].get(_config["fr_usecase_id"], False)
        if "index_collection_path" in _config:
            if Path(_config["index_collection_path"]).exists() != fr_enabled:
                raise AssertionError("derived index files differ from FR runtime state")
        if fr_enabled:
            if vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "GetGalleryStatus") != baseline:
                raise AssertionError("runtime transition changed the protected gallery")
        print(f"PASS runtime transition {index + 1}", flush=True)
    subject = run_id + "_subject"
    revision = baseline[0]
    for index, path in enumerate(_config["failure_images"]):
        request = run_id + "_failure_" + str(index)
        vqec_vision_ai_tools_frdbt_begin(bus, _config, request, subject, revision, _path=path)
        terminal = vqec_vision_ai_tools_frdbt_wait(bus, _config, "GetEnrollmentStatus",
            lambda _reply: _reply[2] in g_terminal_states, GLib.Variant("(s)", (request,)))
        if terminal[2] != g_failed_state or vqec_vision_ai_tools_frdbt_call(
                bus, _config, "enrollment", "GetGalleryStatus") != baseline:
            raise AssertionError("invalid image changed the gallery")
    for sample in range(_config["template_samples"]):
        request = run_id + "_sample_" + str(sample)
        before = vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "GetGalleryStatus")
        vqec_vision_ai_tools_frdbt_begin(bus, _config, request, subject, before[0])
        terminal = vqec_vision_ai_tools_frdbt_wait(bus, _config, "GetEnrollmentStatus",
            lambda _reply: _reply[2] in g_terminal_states, GLib.Variant("(s)", (request,)))
        after = vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "GetGalleryStatus")
        if terminal[2] != g_completed_state or after[0] != before[0] + 1 or after[2] != before[2] + 1:
            raise AssertionError("template enrollment did not commit exactly once")
        if vqec_vision_ai_tools_frdbt_begin(bus, _config, request, subject, before[0]) != terminal:
            raise AssertionError("terminal enrollment retry changed status")
        vqec_vision_ai_tools_frdbt_reject(lambda: vqec_vision_ai_tools_frdbt_begin(
            bus, _config, request, subject + "_conflict", before[0]))
        if vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "GetGalleryStatus") != after:
            raise AssertionError("retry/conflict added another template")
    before = vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "GetGalleryStatus")
    vqec_vision_ai_tools_frdbt_reject(lambda: vqec_vision_ai_tools_frdbt_call(
        bus, _config, "enrollment", "RemoveSubject", GLib.Variant("(st)", (subject, baseline[0]))))
    vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "RemoveSubject",
        GLib.Variant("(st)", (subject, before[0])))
    after = vqec_vision_ai_tools_frdbt_call(bus, _config, "enrollment", "GetGalleryStatus")
    if after[1:] != baseline[1:] or after[0] != before[0] + _config["template_samples"]:
        raise AssertionError("temporary subject removal did not preserve the original gallery")
    print("PASS FR enrollment/retry/delete and runtime model residency", flush=True)
    # Optional demo mode retains the trusted unique sender. No biometric values are logged.
    while _config.get("keep_peer_alive", False):
        time.sleep(_config["poll_seconds"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, required=True)
    arguments = parser.parse_args()
    if arguments.fixture.stat().st_size > g_fixture_max_bytes:
        raise SystemExit("fixture exceeds test-tool safety bound")
    fixture = json.loads(arguments.fixture.read_text())
    for key in ("rpc_timeout_ms", "transition_timeout_seconds", "poll_seconds", "template_samples"):
        if not isinstance(fixture[key], (int, float)) or fixture[key] <= 0:
            raise SystemExit(f"invalid positive fixture setting: {key}")
    if (not isinstance(fixture["template_samples"], int) or
            not 2 <= fixture["template_samples"] <= g_max_test_templates):
        raise SystemExit("template_samples must be between 2 and the test safety ceiling 16")
    vqec_vision_ai_tools_frdbt_run(fixture)
