#!/usr/bin/env python3
"""Exercise the target service's live usecase D-Bus boundary under eSDK QEMU."""

import copy
import json
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import gi

gi.require_version("Gio", "2.0")
gi.require_version("GLib", "2.0")
from gi.repository import Gio, GLib


g_service_name = "com.vqec.UsecaseRuntimeTest"
g_peer_name = "com.vqec.UsecaseRuntimeTestPeer"
g_object_path = "/com/vqec/AiVision/UsecaseRuntimeTest"
g_interface_name = "com.vqec.AiVision.UsecaseControl1"
g_source_id = "camera_front"
g_first_usecase = "first_workload"
g_second_usecase = "second_workload"
g_rpc_timeout_ms = 10000
g_startup_timeout_seconds = 20
g_transition_timeout_seconds = 20
g_poll_seconds = 0.05


def vqec_vision_ai_tools_ucrtst_write_fixture(_directory, _source_directory):
    deployment = json.loads((_source_directory / "service_smoke_deployment.json").read_text())
    catalog = json.loads((_source_directory / "service_smoke_catalog.json").read_text())
    deployment["sources"] = deployment["sources"][:1]
    second = copy.deepcopy(catalog["models"][0])
    second["model_id"] = "second_detector"
    second["artifact_ref"] = "second_detector_artifact"
    second["output_manifest_ref"] = "second_detector_outputs"
    second["graph_name"] = "second_detector_graph"
    second["decoder_contract"] = "second_detector.decoder.v1"
    catalog["models"].append(second)
    deployment["sources"][0]["model_ids"].append(second["model_id"])
    deployment["sources"][0]["max_tensor_bytes"] *= 2
    snapshot = {
        "schema_version": 1,
        "control_revision": 1,
        "entitlement_revision": 1,
        "deployment_revision": deployment["revision"],
        "catalog": {
            "schema_version": 1,
            "revision": 1,
            "catalog_id": "runtime_test_usecases",
            "model_catalog_ref": catalog["catalog_id"],
            "usecases": [
                {"usecase_id": g_first_usecase, "usecase_version": "1.0",
                 "root_model_ids": [catalog["models"][0]["model_id"]], "feature_ids": []},
                {"usecase_id": g_second_usecase, "usecase_version": "1.0",
                 "root_model_ids": [second["model_id"]], "feature_ids": []},
            ],
        },
        "associations": [
            {"source_id": g_source_id, "usecase_id": identifier, "desired": True,
             "installed": True, "entitled": True, "supported": True,
             "compatible": True, "admitted": True}
            for identifier in (g_first_usecase, g_second_usecase)
        ],
    }
    paths = []
    for name, value in (("deployment.json", deployment),
                        ("catalog.json", catalog), ("snapshot.json", snapshot)):
        path = _directory / name
        path.write_text(json.dumps(value), encoding="utf-8")
        paths.append(path)
    return paths


def vqec_vision_ai_tools_ucrtst_call(_bus, _method, _parameters, _output):
    return _bus.call_sync(g_service_name, g_object_path, g_interface_name, _method,
                         _parameters, GLib.VariantType.new(_output),
                         Gio.DBusCallFlags.NONE, g_rpc_timeout_ms, None).unpack()


def vqec_vision_ai_tools_ucrtst_status(_bus):
    revision, entitlement, generation, entries = vqec_vision_ai_tools_ucrtst_call(
        _bus, "GetUsecaseStatus", GLib.Variant("()", ()),
        "(ttta(ssbbbbbbbbss))")
    return revision, entitlement, generation, {
        entry[1]: entry for entry in entries
    }


def vqec_vision_ai_tools_ucrtst_wait_generation(_bus, _process, _revision, _generation, _expected):
    deadline = time.monotonic() + g_transition_timeout_seconds
    while time.monotonic() < deadline:
        if _process.poll() is not None:
            raise AssertionError(f"service exited during generation {_generation}")
        current = vqec_vision_ai_tools_ucrtst_status(_bus)
        if current[0] == _revision and current[2] == _generation:
            for identifier, running in _expected.items():
                entry = current[3][identifier]
                if (entry[4] != running or entry[8] != running or
                        entry[9] != running or
                        entry[10] != ("running" if running else "disabled")):
                    raise AssertionError(f"wrong runtime state for {identifier}: {entry}")
            return
        time.sleep(g_poll_seconds)
    raise AssertionError(f"generation {_generation} did not publish")


def vqec_vision_ai_tools_ucrtst_apply(_bus, _request_id, _revision, _entries):
    parameters = GLib.Variant("(sta(ssb))", (_request_id, _revision, _entries))
    return vqec_vision_ai_tools_ucrtst_call(_bus, "ApplyDesiredPlan", parameters, "(btsu)")


def vqec_vision_ai_tools_ucrtst_run(_command, _source_directory):
    with tempfile.TemporaryDirectory(prefix="vqec_vision_usecase_runtime_") as root:
        directory = Path(root)
        deployment, catalog, snapshot = vqec_vision_ai_tools_ucrtst_write_fixture(directory, _source_directory)
        bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        acquired = bus.call_sync(
            "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
            "RequestName", GLib.Variant("(su)", (g_peer_name, 4)),
            GLib.VariantType.new("(u)"), Gio.DBusCallFlags.NONE,
            g_rpc_timeout_ms, None).unpack()[0]
        if acquired != 1:
            raise AssertionError("cannot own trusted peer name")
        log_path = directory / "service.log"
        with log_path.open("w+") as log:
            service = subprocess.Popen(_command + [
                "--mode", "production", "--platform", "fake",
                "--deployment", str(deployment), "--model-catalog", str(catalog),
                "--usecase-snapshot", str(snapshot), "--usecase-dbus-session",
                "--usecase-service-name", g_service_name,
                "--usecase-object-path", g_object_path,
                "--usecase-peer-name", g_peer_name,
                "--usecase-rpc-timeout-ms", str(g_rpc_timeout_ms),
                "--usecase-callbacks-per-poll", "8",
            ], stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + g_startup_timeout_seconds
                while time.monotonic() < deadline:
                    if service.poll() is not None:
                        raise AssertionError("service exited before DBus startup")
                    owned = bus.call_sync(
                        "org.freedesktop.DBus", "/org/freedesktop/DBus",
                        "org.freedesktop.DBus", "NameHasOwner",
                        GLib.Variant("(s)", (g_service_name,)),
                        GLib.VariantType.new("(b)"), Gio.DBusCallFlags.NONE,
                        g_rpc_timeout_ms, None).unpack()[0]
                    if owned:
                        break
                    time.sleep(g_poll_seconds)
                else:
                    raise AssertionError("service did not own control DBus name")
                vqec_vision_ai_tools_ucrtst_wait_generation(bus, service, 1, 1,
                                {g_first_usecase: True, g_second_usecase: True})
                capabilities = vqec_vision_ai_tools_ucrtst_call(bus, "GetCapabilities", GLib.Variant("()", ()),
                                    "(ta(ss))")
                if len(capabilities[1]) != 2:
                    raise AssertionError("capability catalog is incomplete")
                cases = [
                    ("only_first", [(g_source_id, g_first_usecase, True)],
                     {g_first_usecase: True, g_second_usecase: False}),
                    ("all_off", [],
                     {g_first_usecase: False, g_second_usecase: False}),
                    ("only_second", [(g_source_id, g_second_usecase, True)],
                     {g_first_usecase: False, g_second_usecase: True}),
                    ("both_on", [(g_source_id, g_first_usecase, True),
                                 (g_source_id, g_second_usecase, True)],
                     {g_first_usecase: True, g_second_usecase: True}),
                ]
                for generation, (request_id, entries, expected) in enumerate(cases, 2):
                    receipt = vqec_vision_ai_tools_ucrtst_apply(bus, request_id, generation - 1, entries)
                    if not receipt[0] or receipt[1] != generation or receipt[2] != "reconciling":
                        raise AssertionError(f"desired plan rejected: {receipt}")
                    vqec_vision_ai_tools_ucrtst_wait_generation(bus, service, generation, generation, expected)
                    if vqec_vision_ai_tools_ucrtst_apply(bus, request_id, generation - 1, entries) != receipt:
                        raise AssertionError("idempotent retry changed receipt")
                stale = vqec_vision_ai_tools_ucrtst_apply(bus, "stale", 1, [])
                if stale[0]:
                    raise AssertionError("stale control revision was accepted")
                unknown = vqec_vision_ai_tools_ucrtst_apply(bus, "unknown", 5,
                                [(g_source_id, "not_installed", True)])
                if unknown[0]:
                    raise AssertionError("unknown usecase was accepted")
                print("live DBus usecase transitions passed: both, first, off, second, both")
            except Exception:
                log.flush()
                print(log_path.read_text()[-5000:], file=sys.stderr)
                raise
            finally:
                service.send_signal(signal.SIGTERM)
                try:
                    service.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    service.kill()
                    service.wait(timeout=5)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise SystemExit("usage: test.py <golden-dir> <target-runner...>")
    vqec_vision_ai_tools_ucrtst_run(sys.argv[2:], Path(sys.argv[1]))
