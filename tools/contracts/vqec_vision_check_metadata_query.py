#!/usr/bin/env python3
"""Validate the version-1 metadata/query catalog using only the standard library."""

import argparse
import copy
import json
import pathlib
import re
import sys


g_max_document_bytes = 2 * 1024 * 1024
g_version_pattern = re.compile(
    r"^#define VQEC_VISION_AI_BASELINE_SCHEMA_VERSION ([0-9]+)U$", re.MULTILINE)
g_record_ids = tuple(f"D{index:02d}" for index in range(1, 19))
g_query_ids = tuple(f"Q{index:02d}" for index in range(1, 31))
g_usecase_codes = tuple(f"S{index:02d}" for index in range(1, 19))
g_outcomes = ("positive", "negative", "unknown", "unsupported", "expired")
g_sensitivities = {
    "aggregate", "object", "visual_attribute", "trajectory", "plate", "identity",
    "biometric", "media", "operational", "audit",
}


class metadata_query_validation_error(ValueError):
    """Raised when a metadata/query catalog invariant is violated."""


def vqec_vision_ai_tcont_mdqck_require(_condition, _message):
    if not _condition:
        raise metadata_query_validation_error(_message)


def vqec_vision_ai_tcont_mdqck_load_json(_path):
    path = pathlib.Path(_path)
    size = path.stat().st_size
    vqec_vision_ai_tcont_mdqck_require(
        0 < size <= g_max_document_bytes, f"catalog size is outside bounds: {path}")
    with path.open("r", encoding="utf-8") as stream:
        document = json.load(stream)
    vqec_vision_ai_tcont_mdqck_require(isinstance(document, dict), "catalog root must be object")
    return document


def vqec_vision_ai_tcont_mdqck_read_baseline(_path):
    text = pathlib.Path(_path).read_text(encoding="utf-8")
    match = g_version_pattern.search(text)
    vqec_vision_ai_tcont_mdqck_require(match is not None, "baseline schema version is missing")
    return int(match.group(1))


def vqec_vision_ai_tcont_mdqck_validate_ids(_items, _field, _expected):
    values = [item[_field] for item in _items]
    vqec_vision_ai_tcont_mdqck_require(
        tuple(sorted(values)) == _expected and len(values) == len(set(values)),
        f"{_field} catalog is incomplete or duplicated")


def vqec_vision_ai_tcont_mdqck_validate_references(_values, _allowed, _label):
    vqec_vision_ai_tcont_mdqck_require(
        isinstance(_values, list) and _values and len(_values) == len(set(_values))
        and all(value in _allowed for value in _values),
        f"{_label} references are empty, duplicated or unknown")


def vqec_vision_ai_tcont_mdqck_validate_catalog(_catalog, _baseline, _integration):
    expected_root = {
        "schema_version", "catalog_id", "revision", "authority_team", "record_families",
        "queries", "usecases", "traffic_extensions", "required_fixture_outcomes",
    }
    vqec_vision_ai_tcont_mdqck_require(set(_catalog) == expected_root, "catalog root mismatch")
    vqec_vision_ai_tcont_mdqck_require(
        _catalog["schema_version"] == _baseline and _catalog["catalog_id"] ==
        "lacai.metadata_query_catalog" and _catalog["authority_team"] == "ai_app"
        and isinstance(_catalog["revision"], int) and _catalog["revision"] > 0,
        "catalog authority, identity or version mismatch")
    vqec_vision_ai_tcont_mdqck_require(
        tuple(_catalog["required_fixture_outcomes"]) == g_outcomes,
        "required fixture outcome coverage mismatch")

    records = _catalog["record_families"]
    vqec_vision_ai_tcont_mdqck_require(isinstance(records, list), "record catalog must be list")
    vqec_vision_ai_tcont_mdqck_validate_ids(records, "record_family_id", g_record_ids)
    record_names = set()
    for record in records:
        vqec_vision_ai_tcont_mdqck_require(
            set(record) == {"record_family_id", "name", "class", "sensitivity",
                            "retention_profile", "required_fields"},
            "record family envelope mismatch")
        vqec_vision_ai_tcont_mdqck_require(
            isinstance(record["name"], str) and record["name"] not in record_names
            and re.fullmatch(r"[a-z][a-z0-9_]+", record["name"])
            and record["sensitivity"] in g_sensitivities
            and isinstance(record["required_fields"], list)
            and len(record["required_fields"]) >= 4
            and len(record["required_fields"]) == len(set(record["required_fields"])),
            f"{record['record_family_id']} semantics are invalid")
        record_names.add(record["name"])

    queries = _catalog["queries"]
    vqec_vision_ai_tcont_mdqck_require(isinstance(queries, list), "query catalog must be list")
    vqec_vision_ai_tcont_mdqck_validate_ids(queries, "query_id", g_query_ids)
    for query in queries:
        vqec_vision_ai_tcont_mdqck_require(
            set(query) == {"query_id", "requires", "level", "sensitivity",
                           "baseline_support", "oracle"},
            "query envelope mismatch")
        vqec_vision_ai_tcont_mdqck_validate_references(
            query["requires"], set(g_record_ids), f"{query['query_id']}.requires")
        vqec_vision_ai_tcont_mdqck_require(
            query["sensitivity"] in g_sensitivities
            and query["baseline_support"] in {"supported", "unsupported"}
            and isinstance(query["oracle"], str) and 20 <= len(query["oracle"]) <= 256,
            f"{query['query_id']} policy or oracle is invalid")

    usecases = _catalog["usecases"]
    vqec_vision_ai_tcont_mdqck_require(isinstance(usecases, list), "usecase map must be list")
    vqec_vision_ai_tcont_mdqck_validate_ids(usecases, "catalog_code", g_usecase_codes)
    integration_usecases = {
        item["catalog_code"]: item for item in _integration.get("usecases", [])}
    for usecase in usecases:
        vqec_vision_ai_tcont_mdqck_require(
            set(usecase) == {"catalog_code", "usecase_id", "produces", "requires", "queries",
                             "retention_profiles", "capacity_profile"},
            "usecase mapping envelope mismatch")
        code = usecase["catalog_code"]
        vqec_vision_ai_tcont_mdqck_require(
            code in integration_usecases
            and usecase["usecase_id"] == integration_usecases[code]["usecase_id"],
            f"{code} stable usecase identity mismatch")
        vqec_vision_ai_tcont_mdqck_validate_references(
            usecase["produces"], set(g_record_ids), f"{code}.produces")
        vqec_vision_ai_tcont_mdqck_validate_references(
            usecase["requires"], set(g_record_ids), f"{code}.requires")
        vqec_vision_ai_tcont_mdqck_validate_references(
            usecase["queries"], set(g_query_ids), f"{code}.queries")
        vqec_vision_ai_tcont_mdqck_require(
            set(usecase["queries"]) == set(integration_usecases[code]["query_dependencies"]),
            f"{code} query mapping drifted from the integration registry")
        vqec_vision_ai_tcont_mdqck_require(
            isinstance(usecase["retention_profiles"], list)
            and usecase["retention_profiles"] and usecase["capacity_profile"] == "unqualified",
            f"{code} retention/capacity state is ambiguous")

    traffic = _catalog["traffic_extensions"]
    vqec_vision_ai_tcont_mdqck_require(
        isinstance(traffic, list) and len(traffic) >= 9, "traffic extension coverage is incomplete")
    profile_ids = set()
    traffic_queries = set()
    for profile in traffic:
        vqec_vision_ai_tcont_mdqck_require(
            set(profile) == {"profile_id", "records", "queries"}
            and isinstance(profile["profile_id"], str)
            and profile["profile_id"].startswith("traffic.")
            and profile["profile_id"] not in profile_ids,
            "traffic profile identity is invalid")
        profile_ids.add(profile["profile_id"])
        vqec_vision_ai_tcont_mdqck_validate_references(
            profile["records"], set(g_record_ids), f"{profile['profile_id']}.records")
        vqec_vision_ai_tcont_mdqck_validate_references(
            profile["queries"], set(g_query_ids), f"{profile['profile_id']}.queries")
        traffic_queries.update(profile["queries"])
    vqec_vision_ai_tcont_mdqck_require(
        {f"Q{index:02d}" for index in range(18, 26)}.issubset(traffic_queries),
        "traffic profiles do not cover Q18-Q25")

    mapped_queries = {query for item in usecases for query in item["queries"]} | traffic_queries
    administrative = {f"Q{index:02d}" for index in range(26, 31)}
    vqec_vision_ai_tcont_mdqck_require(
        mapped_queries | administrative == set(g_query_ids),
        "usecase, traffic and administrative mapping does not cover Q01-Q30")


def vqec_vision_ai_tcont_mdqck_run_self_test(_catalog, _baseline, _integration):
    mutations = []
    missing_record = copy.deepcopy(_catalog)
    missing_record["record_families"].pop()
    mutations.append(("missing D18", missing_record))
    duplicate_query = copy.deepcopy(_catalog)
    duplicate_query["queries"][1]["query_id"] = "Q01"
    mutations.append(("duplicate Q01", duplicate_query))
    missing_outcome = copy.deepcopy(_catalog)
    missing_outcome["required_fixture_outcomes"].pop()
    mutations.append(("missing expired fixture class", missing_outcome))
    stale_mapping = copy.deepcopy(_catalog)
    stale_mapping["usecases"][0]["queries"].pop()
    mutations.append(("stale S01 query mapping", stale_mapping))
    for label, mutation in mutations:
        try:
            vqec_vision_ai_tcont_mdqck_validate_catalog(mutation, _baseline, _integration)
        except metadata_query_validation_error:
            continue
        raise metadata_query_validation_error(f"negative self-test was accepted: {label}")


def vqec_vision_ai_tcont_mdqck_run(_arguments=None):
    parser = argparse.ArgumentParser(description="Validate LACAI metadata/query catalog v1")
    parser.add_argument("--catalog", required=True)
    parser.add_argument("--integration-registry", required=True)
    parser.add_argument("--version-registry", required=True)
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args(_arguments)
    baseline = vqec_vision_ai_tcont_mdqck_read_baseline(arguments.version_registry)
    catalog = vqec_vision_ai_tcont_mdqck_load_json(arguments.catalog)
    integration = vqec_vision_ai_tcont_mdqck_load_json(arguments.integration_registry)
    vqec_vision_ai_tcont_mdqck_validate_catalog(catalog, baseline, integration)
    if arguments.self_test:
        vqec_vision_ai_tcont_mdqck_run_self_test(catalog, baseline, integration)
    print(
        f"PASS: {len(catalog['usecases'])} usecases, "
        f"{len(catalog['record_families'])} record families, "
        f"{len(catalog['queries'])} queries, schema v{baseline}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(vqec_vision_ai_tcont_mdqck_run())
    except (OSError, TypeError, KeyError, json.JSONDecodeError,
            metadata_query_validation_error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(1)
