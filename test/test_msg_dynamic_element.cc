#include "catch_amalgamated.hpp"
/*
 * test_msg_dynamic_element.cc — MsgDynamicElement JSON serialization tests
 *
 * Tests to_json/from_json for MsgDynamicKeyValue and MsgDynamicElement,
 * covering various element types and conditional serialization.
 */
#include "nlohmann/json.hpp"
#include "util/MsgDynamicElement.h"

using json = nlohmann::json;

TEST_CASE("MsgDynamicKeyValue: JSON roundtrip", "[msg-dynamic-element]") {
    cosmo::MsgDynamicKeyValue kv;
    kv.key    = "test_key";
    kv.value  = "test_value";
    kv.keys   = {"k1", "k2"};
    kv.values = {"v1", "v2"};

    SECTION("Serialize to JSON") {
        json j;
        to_json(j, kv);
        REQUIRE(j.contains("key"));
        REQUIRE(j["key"] == "test_key");
    }

    SECTION("Roundtrip") {
        json j;
        to_json(j, kv);

        cosmo::MsgDynamicKeyValue kv2;
        from_json(j, kv2);
        REQUIRE(kv2.key.ToString() == "test_key");
        REQUIRE(kv2.value.ToString() == "test_value");
    }
}

TEST_CASE("MsgDynamicElement: JSON serialization basic fields", "[msg-dynamic-element]") {
    cosmo::MsgDynamicElement elem;
    elem.key          = "brightness";
    elem.value        = "50";
    elem.name         = "Brightness";
    elem.defaultValue = "50";
    elem.description  = "Adjust brightness";
    elem.type         = "slider";
    elem.min          = 0.0f;
    elem.max          = 100.0f;
    elem.step         = 1.0f;

    json j;
    to_json(j, elem);

    SECTION("Contains name field") {
        REQUIRE(j.contains("name"));
        REQUIRE(j["name"] == "Brightness");
    }

    SECTION("Contains type field") {
        REQUIRE(j.contains("type"));
        REQUIRE(j["type"] == "slider");
    }
}

TEST_CASE("MsgDynamicElement: from_json deserialization", "[msg-dynamic-element]") {
    json j = {{"key", "contrast"},
              {"value", "70"},
              {"name", "Contrast"},
              {"defaultValue", "50"},
              {"description", "Adjust contrast"},
              {"type", "slider"},
              {"min", 0.0f},
              {"max", 100.0f},
              {"step", 1.0f}};

    cosmo::MsgDynamicElement elem;
    from_json(j, elem);

    REQUIRE(elem.key.ToString() == "contrast");
    REQUIRE(elem.name == "Contrast");
    REQUIRE(elem.min == Catch::Approx(0.0f));
    REQUIRE(elem.max == Catch::Approx(100.0f));
}

TEST_CASE("MsgDynamicElement: Option serialization", "[msg-dynamic-element]") {
    cosmo::MsgDynamicElement::Option opt;
    opt.name  = "High";
    opt.value = "3";

    json j;
    to_json(j, opt);
    REQUIRE(j["name"] == "High");
    REQUIRE(j["value"] == "3");

    cosmo::MsgDynamicElement::Option opt2;
    from_json(j, opt2);
    REQUIRE(opt2.name == "High");
    REQUIRE(opt2.value == "3");
}

TEST_CASE("MsgDynamicElement: DependsOn serialization", "[msg-dynamic-element]") {
    cosmo::MsgDynamicElement::DependsOn dep;
    dep.key   = "mode";
    dep.value = "advanced";

    json j;
    to_json(j, dep);
    REQUIRE(j["key"] == "mode");
    REQUIRE(j["value"] == "advanced");

    cosmo::MsgDynamicElement::DependsOn dep2;
    from_json(j, dep2);
    REQUIRE(dep2.key == "mode");
    REQUIRE(dep2.value == "advanced");
}

TEST_CASE("MsgDynamicElement: element with options", "[msg-dynamic-element]") {
    cosmo::MsgDynamicElement elem;
    elem.key   = "quality";
    elem.value = "2";
    elem.name  = "Quality";
    elem.type  = "select";

    cosmo::MsgDynamicElement::Option o1, o2;
    o1.name      = "Low";
    o1.value     = "1";
    o2.name      = "High";
    o2.value     = "2";
    elem.options = {o1, o2};

    json j;
    to_json(j, elem);

    cosmo::MsgDynamicElement elem2;
    from_json(j, elem2);
    REQUIRE(elem2.options.size() == 2);
    REQUIRE(elem2.options[0].name == "Low");
    REQUIRE(elem2.options[1].name == "High");
}
