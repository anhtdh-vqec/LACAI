// Device-free tests for the strict decoder package loader: required policy fields, unknown
// key rejection, type/range validation, cross-stage tensor uniqueness and failure
// preservation.

#include <iostream>
#include <sstream>
#include <string>

#include "vqec_vision_decoder_package.hpp"

using namespace vqec::vision::ai;

namespace {

status load(const std::string& _document, decoder_package& _package) {
    std::istringstream stream(_document);
    return vqec_vision_ai_mreg_dcpkg_load(stream, _package);
}

std::string yolov8_document() {
    return R"json({
      "schema_version": 1,
      "model_id": "yolov8n_person",
      "decoder_contract": "yolo.v8.detect",
      "class_count": 1,
      "labels_ref": "labels.txt",
      "confidence_threshold": 0.25,
      "iou_threshold": 0.45,
      "box_tensor": "boxes_out",
      "score_tensor": "conf_out",
      "strides": [8, 16, 32],
      "grids": [{"stride": 8, "width": 80, "height": 80}],
      "anchors": 8400,
      "box_layout": "channel_first",
      "box_input_size": [640, 640],
      "notes": "loader test"
    })json";
}

std::string anchor_document() {
    return R"json({
      "kind": "anchor_distance",
      "decoder_contract": "face.detect.scrfd",
      "class_id": "face",
      "landmark_schema_id": "scrfd.landmark",
      "landmark_schema_version": "1",
      "landmark_count": 5,
      "anchor_offset_cells": 0.5,
      "confidence_threshold": 0.5,
      "iou_threshold": 0.4,
      "max_candidates": 1000,
      "stages": [
        {"score_tensor": "s8", "box_tensor": "b8", "landmark_tensor": "k8",
         "stride": 8, "grid_width": 80, "grid_height": 80, "anchors_per_cell": 2},
        {"score_tensor": "s16", "box_tensor": "b16", "landmark_tensor": "k16",
         "stride": 16, "grid_width": 40, "grid_height": 40, "anchors_per_cell": 2}
      ]
    })json";
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    decoder_package yoke;
    check(load(yolov8_document(), yoke).code_ == status_code::ok);
    check(yoke.kind_ == decoder_package_kind::yolov8);
    check(yoke.decoder_contract_ == "yolo.v8.detect");
    check(yoke.box_tensor_ == "boxes_out" && yoke.score_tensor_ == "conf_out");
    check(yoke.class_count_ == 1U && yoke.labels_ref_ == "labels.txt");
    check(yoke.confidence_threshold_ == 0.25F && yoke.iou_threshold_ == 0.45F);
    // The whole package is replaced only on success; a referenced label file is resolved
    // later by the platform, not by the loader.
    check(yoke.labels_.empty());

    decoder_package anchor;
    check(load(anchor_document(), anchor).code_ == status_code::ok);
    check(anchor.kind_ == decoder_package_kind::anchor_distance);
    check(anchor.class_id_ == "face" && anchor.landmark_count_ == 5U);
    check(anchor.max_candidates_ == 1000U && anchor.stages_.size() == 2U);
    check(anchor.stages_[1].stride_ == 16U && anchor.stages_[1].anchors_per_cell_ == 2U);

    const auto expect_invalid = [&check](const std::string& _document) {
        decoder_package package;
        check(load(_document, package).code_ == status_code::invalid_argument);
    };

    // Missing required policy fields are rejected instead of defaulted.
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","confidence_threshold":0.25,
        "iou_threshold":0.45,"box_tensor":"boxes_out","score_tensor":"conf_out"})json");
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "iou_threshold":0.45,"box_tensor":"boxes_out","score_tensor":"conf_out"})json");
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "confidence_threshold":0.25,"iou_threshold":0.45,"score_tensor":"conf_out"})json");
    // Unknown keys are rejected.
    {
        std::string document = yolov8_document();
        document.pop_back();
        document += ",\"unexpected\":true}";
        expect_invalid(document);
    }
    // Wrong types and out-of-range values are rejected.
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":"one",
        "confidence_threshold":0.25,"iou_threshold":0.45,"box_tensor":"boxes_out",
        "score_tensor":"conf_out"})json");
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "confidence_threshold":1.5,"iou_threshold":0.45,"box_tensor":"boxes_out",
        "score_tensor":"conf_out"})json");
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "confidence_threshold":0.25,"iou_threshold":0,"box_tensor":"boxes_out",
        "score_tensor":"conf_out"})json");
    // Inline and referenced labels are mutually exclusive.
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "confidence_threshold":0.25,"iou_threshold":0.45,"box_tensor":"boxes_out",
        "score_tensor":"conf_out","labels":["person"],"labels_ref":"labels.txt"})json");
    // Inline label count must match class_count.
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "confidence_threshold":0.25,"iou_threshold":0.45,"box_tensor":"boxes_out",
        "score_tensor":"conf_out","labels":["person","vehicle"]})json");
    // A dot label reference is not a package filename.
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "confidence_threshold":0.25,"iou_threshold":0.45,"box_tensor":"boxes_out",
        "score_tensor":"conf_out","labels_ref":".."})json");
    // Box and score tensors must be distinct.
    expect_invalid(R"json({"decoder_contract":"yolo.v8.detect","class_count":1,
        "confidence_threshold":0.25,"iou_threshold":0.45,"box_tensor":"same",
        "score_tensor":"same"})json");
    // An unknown explicit kind fails.
    expect_invalid(R"json({"kind":"resnet","decoder_contract":"x","class_count":1,
        "confidence_threshold":0.25,"iou_threshold":0.45,"box_tensor":"a",
        "score_tensor":"b"})json");

    // Anchor-distance required fields, ranges and cross-stage uniqueness.
    expect_invalid(R"json({"kind":"anchor_distance","decoder_contract":"face.detect.scrfd",
        "class_id":"face","landmark_schema_id":"s","landmark_schema_version":"1",
        "landmark_count":5,"anchor_offset_cells":0.5,"confidence_threshold":0.5,
        "iou_threshold":0.4,"max_candidates":1000})json");
    expect_invalid(R"json({"kind":"anchor_distance","decoder_contract":"face.detect.scrfd",
        "class_id":"face","landmark_schema_id":"s","landmark_schema_version":"1",
        "landmark_count":5,"anchor_offset_cells":0.5,"confidence_threshold":0.5,
        "iou_threshold":0.4,"max_candidates":1000,"stages":[
          {"score_tensor":"s8","box_tensor":"b8","landmark_tensor":"k8","stride":8,
           "grid_width":80,"grid_height":80,"anchors_per_cell":2}],"extra":1})json");
    expect_invalid(R"json({"kind":"anchor_distance","decoder_contract":"face.detect.scrfd",
        "class_id":"face","landmark_schema_id":"s","landmark_schema_version":"1",
        "landmark_count":0,"anchor_offset_cells":0.5,"confidence_threshold":0.5,
        "iou_threshold":0.4,"max_candidates":1000,"stages":[
          {"score_tensor":"s8","box_tensor":"b8","landmark_tensor":"k8","stride":8,
           "grid_width":80,"grid_height":80,"anchors_per_cell":2}]})json");
    expect_invalid(R"json({"kind":"anchor_distance","decoder_contract":"face.detect.scrfd",
        "class_id":"face","landmark_schema_id":"s","landmark_schema_version":"1",
        "landmark_count":5,"anchor_offset_cells":0.5,"confidence_threshold":0.5,
        "iou_threshold":0.4,"max_candidates":1000,"stages":[
          {"score_tensor":"dup","box_tensor":"b8","landmark_tensor":"k8","stride":8,
           "grid_width":80,"grid_height":80,"anchors_per_cell":2},
          {"score_tensor":"dup","box_tensor":"b16","landmark_tensor":"k16","stride":16,
           "grid_width":40,"grid_height":40,"anchors_per_cell":2}]})json");

    // Malformed and duplicate-key documents fail without disturbing the caller's package.
    expect_invalid("{ not json");
    decoder_package preserved = yoke;
    std::istringstream duplicate(
        R"json({"decoder_contract":"yolo.v8.detect","decoder_contract":"yolo.v8.detect",
        "class_count":1,"confidence_threshold":0.25,"iou_threshold":0.45,
        "box_tensor":"a","score_tensor":"b"})json");
    check(vqec_vision_ai_mreg_dcpkg_load(duplicate, yoke).code_ ==
        status_code::invalid_argument);
    check(yoke.decoder_contract_ == preserved.decoder_contract_ &&
        yoke.box_tensor_ == preserved.box_tensor_ && yoke.class_count_ == 1U);

    std::cout << "decoder package loader failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
