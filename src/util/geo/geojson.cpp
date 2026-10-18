#include "termchart/util/geo/geojson.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace termchart::util::geo {

namespace {

// 极简 JSON 解析器（只为 GeoJSON 写）：仅支持 string/number/true/false/null/object/array。
// 目的：零依赖；不追求通用、不追求 RFC 7159 完整。覆盖本仓库用到的 GeoJSON 子集。
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    double number_value = 0.0;
    bool   bool_value = false;
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::vector<std::pair<std::string, JsonValue>> object_value;
};

struct Parser {
    const std::string& s;
    std::size_t i = 0;

    void skip_ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }

    bool eat(char c) {
        skip_ws();
        if (i < s.size() && s[i] == c) { ++i; return true; }
        return false;
    }

    bool peek(char c) {
        skip_ws();
        return i < s.size() && s[i] == c;
    }

    [[noreturn]] void fail(const std::string& msg) {
        std::ostringstream o;
        o << "geojson parse error at " << i << ": " << msg;
        throw std::runtime_error(o.str());
    }

    JsonValue parse_value() {
        skip_ws();
        if (i >= s.size()) fail("unexpected end");
        char c = s[i];
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        fail(std::string("unexpected char '") + c + "'");
    }

    JsonValue parse_object() {
        JsonValue v; v.type = JsonValue::Type::Object;
        ++i;  // {
        while (true) {
            skip_ws();
            if (i < s.size() && s[i] == '}') { ++i; break; }
            JsonValue key = parse_string();
            if (!eat(':')) fail("expected ':'");
            JsonValue val = parse_value();
            v.object_value.emplace_back(std::move(key.string_value), std::move(val));
            skip_ws();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == '}') { ++i; break; }
            fail("expected ',' or '}'");
        }
        return v;
    }

    JsonValue parse_array() {
        JsonValue v; v.type = JsonValue::Type::Array;
        ++i;  // [
        while (true) {
            skip_ws();
            if (i < s.size() && s[i] == ']') { ++i; break; }
            v.array_value.push_back(parse_value());
            skip_ws();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == ']') { ++i; break; }
            fail("expected ',' or ']'");
        }
        return v;
    }

    JsonValue parse_string() {
        JsonValue v; v.type = JsonValue::Type::String;
        if (s[i] != '"') fail("expected '\"'");
        ++i;
        std::string out;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char esc = s[i + 1];
                switch (esc) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u':
                    if (i + 5 < s.size()) {
                        unsigned cp = 0;
                        std::sscanf(s.c_str() + i + 2, "%4x", &cp);
                        if (cp < 0x80) out += static_cast<char>(cp);
                        else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        i += 4;
                    }
                    break;
                default: out += esc; break;
                }
                i += 2;
            } else {
                out += s[i++];
            }
        }
        if (i >= s.size()) fail("unterminated string");
        ++i;
        v.string_value = std::move(out);
        return v;
    }

    JsonValue parse_number() {
        JsonValue v; v.type = JsonValue::Type::Number;
        std::size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) ++i;
        v.number_value = std::stod(s.substr(start, i - start));
        return v;
    }

    JsonValue parse_bool() {
        JsonValue v; v.type = JsonValue::Type::Bool;
        if (s.compare(i, 4, "true") == 0) { v.bool_value = true; i += 4; return v; }
        if (s.compare(i, 5, "false") == 0) { v.bool_value = false; i += 5; return v; }
        fail("expected true/false");
    }

    JsonValue parse_null() {
        JsonValue v;
        if (s.compare(i, 4, "null") == 0) { i += 4; return v; }
        fail("expected null");
    }
};

const JsonValue* find(const JsonValue& obj, const std::string& key) {
    if (obj.type != JsonValue::Type::Object) return nullptr;
    for (const auto& [k, val] : obj.object_value) {
        if (k == key) return &val;
    }
    return nullptr;
}

void parse_coord(const JsonValue& v, double& lon, double& lat) {
    if (v.type != JsonValue::Type::Array || v.array_value.size() < 2)
        throw std::runtime_error("coordinate must be [lon, lat]");
    lon = v.array_value[0].number_value;
    lat = v.array_value[1].number_value;
}

void parse_ring(const JsonValue& ring_val, std::vector<Point>& ring) {
    if (ring_val.type != JsonValue::Type::Array) throw std::runtime_error("ring not array");
    ring.clear();
    ring.reserve(ring_val.array_value.size());
    for (const auto& pt : ring_val.array_value) {
        double lon = 0, lat = 0;
        parse_coord(pt, lon, lat);
        ring.push_back({lon, lat});
    }
}

void parse_geometry(const JsonValue& geom, std::vector<GeoPolygon>& polys) {
    const JsonValue* type_v = find(geom, "type");
    if (!type_v || type_v->type != JsonValue::Type::String)
        throw std::runtime_error("geometry.type missing");
    const std::string type = type_v->string_value;
    const JsonValue* coords = find(geom, "coordinates");
    if (!coords) throw std::runtime_error("geometry.coordinates missing");

    if (type == "Polygon") {
        GeoPolygon poly;
        for (const auto& ring_val : coords->array_value) {
            poly.rings.emplace_back();
            parse_ring(ring_val, poly.rings.back());
        }
        polys.push_back(std::move(poly));
    } else if (type == "MultiPolygon") {
        for (const auto& poly_val : coords->array_value) {
            GeoPolygon poly;
            for (const auto& ring_val : poly_val.array_value) {
                poly.rings.emplace_back();
                parse_ring(ring_val, poly.rings.back());
            }
            polys.push_back(std::move(poly));
        }
    } else {
        throw std::runtime_error("unsupported geometry type: " + type);
    }
}

void parse_properties(const JsonValue& props, GeoPolygon& poly) {
    if (props.type != JsonValue::Type::Object) return;
    for (const auto& [k, v] : props.object_value) {
        std::string val;
        switch (v.type) {
        case JsonValue::Type::String: val = v.string_value; break;
        case JsonValue::Type::Number: val = std::to_string(v.number_value); break;
        case JsonValue::Type::Bool:   val = v.bool_value ? "true" : "false"; break;
        default: continue;
        }
        poly.properties.emplace_back(k, val);
    }
}

}  // namespace

bool parse_geojson(const std::string& text, GeoFeatureCollection& out,
                   std::string& out_error) noexcept {
    try {
        Parser p{text};
        JsonValue root = p.parse_value();
        const JsonValue* type_v = find(root, "type");
        if (!type_v || type_v->string_value != "FeatureCollection") {
            out_error = "root.type must be FeatureCollection";
            return false;
        }
        const JsonValue* feats = find(root, "features");
        if (!feats || feats->type != JsonValue::Type::Array) {
            out_error = "features missing or not array";
            return false;
        }
        out.polygons.clear();
        for (const auto& f : feats->array_value) {
            const JsonValue* geom = find(f, "geometry");
            const JsonValue* props = find(f, "properties");
            std::vector<GeoPolygon> polys;
            if (geom) parse_geometry(*geom, polys);
            for (auto& poly : polys) {
                if (props) parse_properties(*props, poly);
                out.polygons.push_back(std::move(poly));
            }
        }
        return true;
    } catch (const std::exception& e) {
        out_error = e.what();
        return false;
    }
}

bool parse_geojson_file(const std::string& path, GeoFeatureCollection& out,
                        std::string& out_error) noexcept {
    std::ifstream in(path);
    if (!in) {
        out_error = "cannot open: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse_geojson(ss.str(), out, out_error);
}

GeoBounds compute_bounds(const GeoFeatureCollection& fc) noexcept {
    GeoBounds b{90, -90, 180, -180};
    bool any = false;
    for (const auto& poly : fc.polygons) {
        for (const auto& ring : poly.rings) {
            for (const auto& p : ring) {
                if (std::isnan(p.x) || std::isnan(p.y)) continue;
                if (!any) { b.min_lat = b.max_lat = p.y; b.min_lon = b.max_lon = p.x; any = true; }
                else {
                    if (p.y < b.min_lat) b.min_lat = p.y;
                    if (p.y > b.max_lat) b.max_lat = p.y;
                    if (p.x < b.min_lon) b.min_lon = p.x;
                    if (p.x > b.max_lon) b.max_lon = p.x;
                }
            }
        }
    }
    return b;
}

}  // namespace termchart::util::geo
