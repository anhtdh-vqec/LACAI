#!/usr/bin/env python3
"""Validate the LACAI three-team contract registry using only the standard library."""

import argparse
import copy
import json
import pathlib
import re
import sys


g_max_document_bytes = 2 * 1024 * 1024
g_identifier_pattern = re.compile(r"^[A-Za-z0-9_.-]{1,128}$")
g_sha256_pattern = re.compile(r"^[a-f0-9]{64}$")
g_utc_timestamp_pattern = re.compile(
    r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")
g_version_pattern = re.compile(
    r"^#define VQEC_VISION_AI_BASELINE_SCHEMA_VERSION ([0-9]+)U$", re.MULTILINE)
g_contract_ids = tuple(f"C{index:02d}" for index in range(1, 11))
g_team_ids = ("bsp_fw", "ai_app", "ai_model")
g_expected_owners = {
    "C01": "bsp_fw", "C02": "bsp_fw", "C03": "ai_model", "C04": "ai_app",
    "C05": "ai_app", "C06": "ai_app", "C07": "ai_app", "C08": "ai_app",
    "C09": "ai_app", "C10": "bsp_fw",
}
g_expected_producers = {
    "C01": ("bsp_fw",), "C02": ("bsp_fw",), "C03": ("ai_model",),
    "C04": ("ai_app", "bsp_fw"), "C05": ("ai_app", "bsp_fw"),
    "C06": ("ai_app",), "C07": ("ai_app", "bsp_fw"), "C08": ("ai_app",),
    "C09": ("ai_app", "bsp_fw"), "C10": ("bsp_fw",),
}
g_usecase_ids = {
    "S01": "security.restricted_area_smoking",
    "S02": "security.suspicious_weapon",
    "S03": "security.ppe_compliance",
    "S04": "security.fire_smoke_detection",
    "S05": "security.blacklist_person_alert",
    "S06": "security.attendance_recognition",
    "S07": "security.demographic_estimation",
    "S08": "security.people_density_heatmap",
    "S09": "security.unauthorized_intrusion",
    "S10": "security.people_entry_exit_count",
    "S11": "security.person_tracking",
    "S12": "security.vlm_context_alert",
    "S13": "security.abandoned_or_removed_object",
    "S14": "security.lost_item_trace",
    "S15": "security.luggage_cart_tracking",
    "S16": "security.vehicle_plate_recognition",
    "S17": "security.crowd_gathering",
    "S18": "security.abnormal_fight_conflict",
}
g_contract_keys = {
    "contract_id", "contract_version", "name", "owner_team", "producer_teams", "consumer_teams",
    "authority_document", "max_message_bytes", "max_messages_per_second", "clock",
    "units", "ownership", "delivery", "security", "required_fields", "error_codes",
}
g_usecase_keys = {
    "catalog_code", "usecase_id", "version", "capability_status", "status_reason",
    "model_dependencies", "data_dependencies", "query_dependencies", "output_dependencies",
}


class contract_validation_error(ValueError):
    """Raised when a registry or conformance-case invariant is violated."""


def vqec_vision_ai_tcont_icchk_require(_condition, _message):
    if not _condition:
        raise contract_validation_error(_message)


def vqec_vision_ai_tcont_icchk_load_json(_path):
    path = pathlib.Path(_path)
    size = path.stat().st_size
    vqec_vision_ai_tcont_icchk_require(
        0 < size <= g_max_document_bytes,
        f"document size is outside bounds: {path}")
    with path.open("r", encoding="utf-8") as stream:
        value = json.load(stream)
    vqec_vision_ai_tcont_icchk_require(isinstance(value, dict), f"root must be object: {path}")
    return value


def vqec_vision_ai_tcont_icchk_read_baseline(_path):
    text = pathlib.Path(_path).read_text(encoding="utf-8")
    match = g_version_pattern.search(text)
    vqec_vision_ai_tcont_icchk_require(match is not None, "baseline schema version is missing")
    return int(match.group(1))


def vqec_vision_ai_tcont_icchk_validate_string_list(_value, _field, _maximum=64):
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_value, list) and 1 <= len(_value) <= _maximum,
        f"{_field} must be a bounded nonempty list")
    vqec_vision_ai_tcont_icchk_require(
        len(_value) == len(set(_value)), f"{_field} contains duplicates")
    for item in _value:
        vqec_vision_ai_tcont_icchk_require(
            isinstance(item, str) and 0 < len(item) <= 256,
            f"{_field} contains an invalid string")


def vqec_vision_ai_tcont_icchk_validate_contract(_contract, _baseline):
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_contract, dict) and set(_contract) == g_contract_keys,
        "contract fields do not match the v1 envelope")
    contract_id = _contract["contract_id"]
    vqec_vision_ai_tcont_icchk_require(contract_id in g_contract_ids, "unknown contract_id")
    vqec_vision_ai_tcont_icchk_require(
        _contract["contract_version"] == _baseline, f"{contract_id} version mismatch")
    vqec_vision_ai_tcont_icchk_require(
        _contract["owner_team"] == g_expected_owners[contract_id],
        f"{contract_id} has the wrong schema owner")
    producers = _contract["producer_teams"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(producers, list) and tuple(producers) == g_expected_producers[contract_id],
        f"{contract_id} has the wrong producer teams")
    consumers = _contract["consumer_teams"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(consumers, list) and consumers and len(consumers) == len(set(consumers)),
        f"{contract_id} consumers are invalid")
    vqec_vision_ai_tcont_icchk_require(
        all(team in g_team_ids and team != _contract["owner_team"] for team in consumers),
        f"{contract_id} consumer must be another known team")
    document = _contract["authority_document"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(document, str) and document.startswith("docs/") and document.endswith(".md")
        and ".." not in pathlib.PurePosixPath(document).parts,
        f"{contract_id} authority document is invalid")
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_contract["max_message_bytes"], int)
        and 1 <= _contract["max_message_bytes"] <= 16 * 1024 * 1024,
        f"{contract_id} message bound is invalid")
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_contract["max_messages_per_second"], int)
        and 1 <= _contract["max_messages_per_second"] <= 10000,
        f"{contract_id} rate bound is invalid")

    clock = _contract["clock"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(clock, dict)
        and set(clock) == {"domain", "timestamp_unit", "mapping_required"}
        and clock["domain"] in {"none", "monotonic", "source_monotonic", "realtime_utc"}
        and clock["timestamp_unit"] in {"none", "ns", "us", "ms"}
        and isinstance(clock["mapping_required"], bool),
        f"{contract_id} clock contract is invalid")
    if clock["domain"] == "none":
        vqec_vision_ai_tcont_icchk_require(
            clock["timestamp_unit"] == "none" and not clock["mapping_required"],
            f"{contract_id} none clock cannot carry mapping")
    else:
        vqec_vision_ai_tcont_icchk_require(
            clock["timestamp_unit"] != "none", f"{contract_id} clock needs a unit")

    vqec_vision_ai_tcont_icchk_validate_string_list(_contract["units"], f"{contract_id}.units")
    vqec_vision_ai_tcont_icchk_require(
        all(":" in unit for unit in _contract["units"]),
        f"{contract_id} units must name field and unit")
    ownership = _contract["ownership"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(ownership, dict)
        and set(ownership) == {"producer_retains_until", "consumer_release_signal", "timeout_is_completion", "disconnect_is_completion"}
        and ownership["timeout_is_completion"] is False
        and ownership["disconnect_is_completion"] is False,
        f"{contract_id} completion safety is invalid")
    for field in ("producer_retains_until", "consumer_release_signal"):
        vqec_vision_ai_tcont_icchk_require(
            isinstance(ownership[field], str) and g_identifier_pattern.fullmatch(ownership[field]),
            f"{contract_id} ownership signal is invalid")

    delivery = _contract["delivery"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(delivery, dict)
        and set(delivery) == {"reliability", "idempotency_key", "retry_policy", "ordering"}
        and delivery["reliability"] in {"bounded_latest", "revisioned_snapshot", "immutable_package", "transactional_snapshot", "snapshot_paged", "at_least_once"},
        f"{contract_id} delivery contract is invalid")
    for field in ("idempotency_key", "retry_policy", "ordering"):
        vqec_vision_ai_tcont_icchk_require(
            isinstance(delivery[field], str) and g_identifier_pattern.fullmatch(delivery[field]),
            f"{contract_id} delivery {field} is invalid")

    security = _contract["security"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(security, dict)
        and set(security) == {"authenticated_principal", "authorization_scopes", "sensitive_data", "deny_by_default"}
        and security["deny_by_default"] is True
        and isinstance(security["authenticated_principal"], str)
        and g_identifier_pattern.fullmatch(security["authenticated_principal"]),
        f"{contract_id} security contract is invalid")
    vqec_vision_ai_tcont_icchk_validate_string_list(
        security["authorization_scopes"], f"{contract_id}.authorization_scopes")
    vqec_vision_ai_tcont_icchk_validate_string_list(
        security["sensitive_data"], f"{contract_id}.sensitive_data")
    vqec_vision_ai_tcont_icchk_validate_string_list(
        _contract["required_fields"], f"{contract_id}.required_fields")
    vqec_vision_ai_tcont_icchk_validate_string_list(
        _contract["error_codes"], f"{contract_id}.error_codes")
    vqec_vision_ai_tcont_icchk_require(
        "invalid_argument" in _contract["error_codes"],
        f"{contract_id} must define invalid_argument")


def vqec_vision_ai_tcont_icchk_validate_usecase(_usecase, _baseline):
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_usecase, dict) and set(_usecase) == g_usecase_keys,
        "usecase fields do not match the v1 envelope")
    code = _usecase["catalog_code"]
    vqec_vision_ai_tcont_icchk_require(
        code in g_usecase_ids and _usecase["usecase_id"] == g_usecase_ids[code],
        "usecase stable identity mismatch")
    vqec_vision_ai_tcont_icchk_require(
        _usecase["version"] == _baseline, f"{code} version mismatch")
    vqec_vision_ai_tcont_icchk_require(
        _usecase["capability_status"] in {"supported", "partial", "unsupported"}
        and isinstance(_usecase["status_reason"], str)
        and 0 < len(_usecase["status_reason"]) <= 256,
        f"{code} capability status is invalid")
    for field in ("model_dependencies", "data_dependencies", "query_dependencies", "output_dependencies"):
        vqec_vision_ai_tcont_icchk_validate_string_list(_usecase[field], f"{code}.{field}", 30)
    vqec_vision_ai_tcont_icchk_require(
        all(re.fullmatch(r"Q(0[1-9]|[12][0-9]|30)", query)
            for query in _usecase["query_dependencies"]),
        f"{code} query dependency is invalid")


def vqec_vision_ai_tcont_icchk_validate_registry(_registry, _baseline):
    expected_keys = {"schema_version", "registry_id", "revision", "authority_team", "teams", "contracts", "usecases"}
    vqec_vision_ai_tcont_icchk_require(
        set(_registry) == expected_keys, "registry root fields do not match v1")
    vqec_vision_ai_tcont_icchk_require(
        _registry["schema_version"] == _baseline, "registry schema version mismatch")
    vqec_vision_ai_tcont_icchk_require(
        _registry["registry_id"] == "lacai.integration_contract_registry"
        and _registry["authority_team"] == "ai_app"
        and isinstance(_registry["revision"], int) and _registry["revision"] > 0,
        "registry authority or revision is invalid")
    teams = _registry["teams"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(teams, list) and len(teams) == len(g_team_ids), "team catalog is incomplete")
    team_ids = []
    for team in teams:
        vqec_vision_ai_tcont_icchk_require(
            isinstance(team, dict) and set(team) == {"team_id", "owns", "must_not_own"},
            "team envelope is invalid")
        team_ids.append(team["team_id"])
        vqec_vision_ai_tcont_icchk_validate_string_list(team["owns"], "team.owns")
        vqec_vision_ai_tcont_icchk_validate_string_list(team["must_not_own"], "team.must_not_own")
        vqec_vision_ai_tcont_icchk_require(
            set(team["owns"]).isdisjoint(team["must_not_own"]), "team ownership conflicts")
    vqec_vision_ai_tcont_icchk_require(
        set(team_ids) == set(g_team_ids) and len(team_ids) == len(set(team_ids)),
        "team IDs are incomplete or duplicated")

    contracts = _registry["contracts"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(contracts, list) and len(contracts) == len(g_contract_ids),
        "contract catalog must contain C01-C10")
    for contract in contracts:
        vqec_vision_ai_tcont_icchk_validate_contract(contract, _baseline)
    contract_ids = [contract["contract_id"] for contract in contracts]
    vqec_vision_ai_tcont_icchk_require(
        tuple(sorted(contract_ids)) == g_contract_ids and len(contract_ids) == len(set(contract_ids)),
        "contract IDs are incomplete or duplicated")

    usecases = _registry["usecases"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(usecases, list) and len(usecases) == len(g_usecase_ids),
        "usecase catalog must contain S01-S18")
    for usecase in usecases:
        vqec_vision_ai_tcont_icchk_validate_usecase(usecase, _baseline)
    usecase_codes = [usecase["catalog_code"] for usecase in usecases]
    usecase_ids = [usecase["usecase_id"] for usecase in usecases]
    vqec_vision_ai_tcont_icchk_require(
        set(usecase_codes) == set(g_usecase_ids)
        and len(usecase_codes) == len(set(usecase_codes))
        and len(usecase_ids) == len(set(usecase_ids)),
        "usecase IDs are incomplete or duplicated")


def vqec_vision_ai_tcont_icchk_validate_cases(_cases, _registry, _baseline):
    vqec_vision_ai_tcont_icchk_require(
        set(_cases) == {"schema_version", "registry_id", "registry_revision", "cases"},
        "case root fields do not match v1")
    vqec_vision_ai_tcont_icchk_require(
        _cases["schema_version"] == _baseline
        and _cases["registry_id"] == _registry["registry_id"]
        and _cases["registry_revision"] == _registry["revision"],
        "case registry identity mismatch")
    contracts = {contract["contract_id"]: contract for contract in _registry["contracts"]}
    entries = _cases["cases"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(entries, list) and len(entries) == len(g_contract_ids) * 2,
        "each contract requires one valid and one rejected case")
    case_ids = set()
    coverage = {contract_id: set() for contract_id in g_contract_ids}
    for entry in entries:
        vqec_vision_ai_tcont_icchk_require(
            isinstance(entry, dict)
            and set(entry) == {"case_id", "contract_id", "outcome", "scenario", "expected_reason_code"},
            "case envelope is invalid")
        contract_id = entry["contract_id"]
        vqec_vision_ai_tcont_icchk_require(contract_id in contracts, "case contract is unknown")
        vqec_vision_ai_tcont_icchk_require(
            isinstance(entry["case_id"], str) and entry["case_id"].startswith(contract_id + "_")
            and entry["case_id"] not in case_ids,
            "case ID is invalid or duplicated")
        case_ids.add(entry["case_id"])
        outcome = entry["outcome"]
        reason = entry["expected_reason_code"]
        vqec_vision_ai_tcont_icchk_require(
            outcome in {"valid", "rejected"}
            and isinstance(entry["scenario"], str) and 0 < len(entry["scenario"]) <= 256,
            "case outcome or scenario is invalid")
        if outcome == "valid":
            vqec_vision_ai_tcont_icchk_require(reason == "ok", "valid case must expect ok")
        else:
            vqec_vision_ai_tcont_icchk_require(
                reason in contracts[contract_id]["error_codes"],
                f"{contract_id} rejected case reason is not declared")
        coverage[contract_id].add(outcome)
    vqec_vision_ai_tcont_icchk_require(
        all(outcomes == {"valid", "rejected"} for outcomes in coverage.values()),
        "case coverage is incomplete")


def vqec_vision_ai_tcont_icchk_validate_schema_version(_schema, _baseline, _label):
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_schema.get("properties"), dict)
        and _schema["properties"].get("schema_version", {}).get("const") == _baseline,
        f"{_label} does not use the canonical baseline version")


def vqec_vision_ai_tcont_icchk_validate_receipt(_receipt, _registry, _cases, _baseline):
    expected_keys = {
        "schema_version", "registry_id", "registry_revision", "receipt_id",
        "receipt_revision", "contract_id", "contract_version", "producer_team",
        "produced_at_utc", "expires_at_utc", "subject", "evidence", "consumer_disposition",
    }
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_receipt, dict) and set(_receipt) == expected_keys,
        "receipt root fields do not match v1")
    vqec_vision_ai_tcont_icchk_require(
        _receipt["schema_version"] == _baseline
        and _receipt["registry_id"] == _registry["registry_id"]
        and _receipt["registry_revision"] == _registry["revision"]
        and _receipt["contract_version"] == _baseline,
        "receipt registry or contract version mismatch")
    vqec_vision_ai_tcont_icchk_require(
        isinstance(_receipt["receipt_id"], str)
        and g_identifier_pattern.fullmatch(_receipt["receipt_id"])
        and isinstance(_receipt["receipt_revision"], int)
        and _receipt["receipt_revision"] > 0,
        "receipt identity is invalid")
    contracts = {contract["contract_id"]: contract for contract in _registry["contracts"]}
    contract_id = _receipt["contract_id"]
    vqec_vision_ai_tcont_icchk_require(contract_id in contracts, "receipt contract is unknown")
    vqec_vision_ai_tcont_icchk_require(
        _receipt["producer_team"] in contracts[contract_id]["producer_teams"],
        f"{contract_id} receipt producer is not authoritative")
    produced_at = _receipt["produced_at_utc"]
    expires_at = _receipt["expires_at_utc"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(produced_at, str) and g_utc_timestamp_pattern.fullmatch(produced_at),
        "receipt produced_at_utc is invalid")
    vqec_vision_ai_tcont_icchk_require(
        expires_at is None
        or (isinstance(expires_at, str) and g_utc_timestamp_pattern.fullmatch(expires_at)
            and expires_at > produced_at),
        "receipt expires_at_utc is invalid")

    subject = _receipt["subject"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(subject, dict)
        and set(subject) == {"subject_id", "subject_version", "target_id", "artifact_digests"},
        "receipt subject is invalid")
    for field in ("subject_id", "subject_version", "target_id"):
        vqec_vision_ai_tcont_icchk_require(
            isinstance(subject[field], str) and g_identifier_pattern.fullmatch(subject[field]),
            f"receipt subject {field} is invalid")
    digests = subject["artifact_digests"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(digests, dict) and 1 <= len(digests) <= 64,
        "receipt artifact digests are invalid")
    for name, digest in digests.items():
        vqec_vision_ai_tcont_icchk_require(
            isinstance(name, str) and g_identifier_pattern.fullmatch(name)
            and isinstance(digest, str) and g_sha256_pattern.fullmatch(digest),
            "receipt artifact digest entry is invalid")

    evidence = _receipt["evidence"]
    evidence_keys = {
        "environment_reference", "command_reference", "case_report_digest", "case_ids",
        "deviations", "owner_signature_reference",
    }
    vqec_vision_ai_tcont_icchk_require(
        isinstance(evidence, dict) and set(evidence) == evidence_keys,
        "receipt evidence is invalid")
    vqec_vision_ai_tcont_icchk_require(
        isinstance(evidence["environment_reference"], str)
        and g_identifier_pattern.fullmatch(evidence["environment_reference"])
        and isinstance(evidence["command_reference"], str)
        and 0 < len(evidence["command_reference"]) <= 512
        and isinstance(evidence["case_report_digest"], str)
        and g_sha256_pattern.fullmatch(evidence["case_report_digest"])
        and isinstance(evidence["owner_signature_reference"], str)
        and 0 < len(evidence["owner_signature_reference"]) <= 256,
        "receipt evidence references are invalid")
    deviations = evidence["deviations"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(deviations, list) and len(deviations) <= 64
        and len(deviations) == len(set(deviations))
        and all(isinstance(item, str) and 0 < len(item) <= 256 for item in deviations),
        "receipt deviations are invalid")
    expected_cases = {
        entry["case_id"] for entry in _cases["cases"] if entry["contract_id"] == contract_id}
    case_ids = evidence["case_ids"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(case_ids, list) and set(case_ids) == expected_cases
        and len(case_ids) == len(set(case_ids)),
        f"{contract_id} receipt case coverage is incomplete")

    disposition = _receipt["consumer_disposition"]
    vqec_vision_ai_tcont_icchk_require(
        isinstance(disposition, dict)
        and set(disposition) == {"status", "consumer_team", "decided_at_utc", "reason_code"}
        and disposition["consumer_team"] == "ai_app",
        "receipt consumer disposition is invalid")
    status = disposition["status"]
    decided_at = disposition["decided_at_utc"]
    reason = disposition["reason_code"]
    vqec_vision_ai_tcont_icchk_require(
        status in {"pending", "accepted", "rejected", "accepted_with_deviation"},
        "receipt disposition status is invalid")
    if status == "pending":
        vqec_vision_ai_tcont_icchk_require(
            decided_at is None and reason is None, "pending receipt cannot be decided")
    else:
        vqec_vision_ai_tcont_icchk_require(
            isinstance(decided_at, str) and g_utc_timestamp_pattern.fullmatch(decided_at)
            and isinstance(reason, str) and re.fullmatch(r"[a-z][a-z0-9_]+", reason),
            "decided receipt requires timestamp and reason")
        if status == "accepted":
            vqec_vision_ai_tcont_icchk_require(reason == "ok", "accepted receipt reason must be ok")
        if status == "rejected":
            vqec_vision_ai_tcont_icchk_require(
                reason in contracts[contract_id]["error_codes"],
                "rejected receipt reason is not declared")
        if status == "accepted_with_deviation":
            vqec_vision_ai_tcont_icchk_require(
                bool(deviations), "accepted_with_deviation requires a declared deviation")


def vqec_vision_ai_tcont_icchk_run_self_test(_registry, _cases, _baseline):
    mutations = []
    missing_contract = copy.deepcopy(_registry)
    missing_contract["contracts"].pop()
    mutations.append(("missing C10", missing_contract, _cases))
    duplicate_usecase = copy.deepcopy(_registry)
    duplicate_usecase["usecases"][1]["usecase_id"] = duplicate_usecase["usecases"][0]["usecase_id"]
    mutations.append(("duplicate usecase", duplicate_usecase, _cases))
    unsafe_completion = copy.deepcopy(_registry)
    unsafe_completion["contracts"][0]["ownership"]["disconnect_is_completion"] = True
    mutations.append(("unsafe completion", unsafe_completion, _cases))
    wrong_owner = copy.deepcopy(_registry)
    wrong_owner["contracts"][2]["owner_team"] = "ai_app"
    mutations.append(("wrong C03 owner", wrong_owner, _cases))
    missing_case = copy.deepcopy(_cases)
    missing_case["cases"].pop()
    mutations.append(("missing rejected case", _registry, missing_case))
    for label, registry, cases in mutations:
        try:
            vqec_vision_ai_tcont_icchk_validate_registry(registry, _baseline)
            vqec_vision_ai_tcont_icchk_validate_cases(cases, registry, _baseline)
        except contract_validation_error:
            continue
        raise contract_validation_error(f"negative self-test was accepted: {label}")
    receipt = {
        "schema_version": _baseline,
        "registry_id": _registry["registry_id"],
        "registry_revision": _registry["revision"],
        "receipt_id": "fixture.c01.receipt",
        "receipt_revision": 1,
        "contract_id": "C01",
        "contract_version": _baseline,
        "producer_team": "bsp_fw",
        "produced_at_utc": "2026-09-18T00:00:00Z",
        "expires_at_utc": None,
        "subject": {
            "subject_id": "fixture.raw_source",
            "subject_version": "1.0",
            "target_id": "qcs6490.fixture",
            "artifact_digests": {"report": "0" * 64},
        },
        "evidence": {
            "environment_reference": "fixture.environment",
            "command_reference": "fixture command reference",
            "case_report_digest": "1" * 64,
            "case_ids": [
                entry["case_id"] for entry in _cases["cases"] if entry["contract_id"] == "C01"],
            "deviations": [],
            "owner_signature_reference": "fixture-signature-reference",
        },
        "consumer_disposition": {
            "status": "pending", "consumer_team": "ai_app",
            "decided_at_utc": None, "reason_code": None,
        },
    }
    vqec_vision_ai_tcont_icchk_validate_receipt(receipt, _registry, _cases, _baseline)
    invalid_receipt = copy.deepcopy(receipt)
    invalid_receipt["producer_team"] = "ai_model"
    try:
        vqec_vision_ai_tcont_icchk_validate_receipt(
            invalid_receipt, _registry, _cases, _baseline)
    except contract_validation_error:
        return
    raise contract_validation_error("negative self-test accepted an unauthorized receipt producer")


def vqec_vision_ai_tcont_icchk_run(_arguments=None):
    parser = argparse.ArgumentParser(description="Validate LACAI integration contract v1")
    parser.add_argument("--registry", required=True)
    parser.add_argument("--cases", required=True)
    parser.add_argument("--schema", required=True)
    parser.add_argument("--cases-schema", required=True)
    parser.add_argument("--receipt-schema", required=True)
    parser.add_argument("--receipt", action="append", default=[])
    parser.add_argument("--version-registry", required=True)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args(_arguments)
    baseline = vqec_vision_ai_tcont_icchk_read_baseline(arguments.version_registry)
    registry = vqec_vision_ai_tcont_icchk_load_json(arguments.registry)
    cases = vqec_vision_ai_tcont_icchk_load_json(arguments.cases)
    schema = vqec_vision_ai_tcont_icchk_load_json(arguments.schema)
    cases_schema = vqec_vision_ai_tcont_icchk_load_json(arguments.cases_schema)
    receipt_schema = vqec_vision_ai_tcont_icchk_load_json(arguments.receipt_schema)
    vqec_vision_ai_tcont_icchk_validate_schema_version(schema, baseline, "registry schema")
    vqec_vision_ai_tcont_icchk_validate_schema_version(cases_schema, baseline, "cases schema")
    vqec_vision_ai_tcont_icchk_validate_schema_version(
        receipt_schema, baseline, "receipt schema")
    vqec_vision_ai_tcont_icchk_validate_registry(registry, baseline)
    vqec_vision_ai_tcont_icchk_validate_cases(cases, registry, baseline)
    for receipt_path in arguments.receipt:
        receipt = vqec_vision_ai_tcont_icchk_load_json(receipt_path)
        vqec_vision_ai_tcont_icchk_validate_receipt(receipt, registry, cases, baseline)
    if arguments.self_test:
        vqec_vision_ai_tcont_icchk_run_self_test(registry, cases, baseline)
    print(
        f"PASS: {len(registry['contracts'])} contracts, "
        f"{len(registry['usecases'])} usecases, {len(cases['cases'])} cases, "
        f"{len(arguments.receipt)} receipts, schema v{baseline}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(vqec_vision_ai_tcont_icchk_run())
    except (OSError, TypeError, KeyError, json.JSONDecodeError, contract_validation_error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
