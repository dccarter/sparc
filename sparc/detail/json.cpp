//
// Created by dc on 11/27/16.
//

#include <cmath>
#include "json.h"
#include "kore.h"

namespace sparc {

    static JsonNode *JsonValue_find(JsonValue *obj, cc_string p, JsonTag t) {
        if (obj->node != NULL) {
            if (p == NULL) {
                if (obj->node->tag == t)
                    return obj->node;
            } else {
                JsonNode *tmp = json_find_member(obj->node, p);
                if (tmp && tmp->tag == t)
                    return tmp;
            }
        }

        return NULL;
    }

    JsonValue::JsonValue(JsonNode *n)
            : node(n) {}
    JsonValue::JsonValue(Json *j)
            :node(((detail::CcanJson *)j)->root_) {}

    JsonValue JsonValue::a(cc_string p) {
        return JsonValue(JsonValue_find(this, p, JSON_ARRAY));
    }

    JsonValue JsonValue::o(cc_string p) {
        return JsonValue(JsonValue_find(this, p, JSON_OBJECT));
    }

    bool JsonValue::b(cc_string p) {
        JsonNode *n = JsonValue_find(this, p, JSON_BOOL);
        return n ? n->bool_ : false;
    }

    double JsonValue::n(cc_string p) {
        JsonNode *n = JsonValue_find(this, p, JSON_NUMBER);
        return n ? n->number_ : NAN;
    }

    cc_string JsonValue::s(cc_string p) {
        JsonNode *n = JsonValue_find(this, p, JSON_STRING);
        return n ? n->string_ : NULL;
    }

    void JsonValue::each(JsonEach hh) {
        if (hh && node && (node->tag == JSON_ARRAY || node->tag == JSON_OBJECT)) {
            JsonNode *jn;
            json_foreach(jn, node) {
                JsonValue jobj(jn);
                if (!hh(jobj))
                    break;
            }
        }
    }

    bool JsonValue::isNull() const {
        return node == NULL || node->tag == JSON_NULL;
    }

    bool JsonValue::operator!=(const JsonValue &other) {
        return other.node != node;
    }

    bool JsonValue::operator==(const JsonValue &other) {
        return other.node == node;
    }

    JsonType JsonValue::type() const {
        return node ? (JsonType) node->tag : J_NULL;
    }

    JsonValue JsonValue::operator[](int index) {
        JsonNode *jn = NULL;
        if (node && node->tag == JSON_ARRAY)
            jn = json_find_element(node, index);
        return JsonValue(jn);
    }

    JsonValue JsonValue::operator[](cc_string p) {
        JsonNode *jn = NULL;
        if (p != NULL)
            jn = json_find_member(node, p);

        return JsonValue(jn);
    }

    JsonObject::JsonObject(JsonNode *jn, JsonType t)
        : JsonValue(jn),
          type_(jn? (JsonType) t : J_NULL)
    {}

    JsonObject::JsonObject(Json *json)
        : JsonValue(json),
          type_(J_OBJECT)
    {}

    JsonObject::JsonObject(JsonValue jv)
        : JsonValue(jv.node)
    {}

    JsonObject JsonObject::addb(bool b) {
        if (node && node->tag == JSON_ARRAY) {
            JsonNode *jn = json_mkbool(b);
            json_append_element(node, jn);
            return JsonObject(jn, J_BOOL);
        }
        return JsonObject(NULL);
    }

    JsonObject JsonObject::add(cc_string s) {
        if (node && node->tag == JSON_ARRAY) {
            JsonNode *jn = json_mkstring(s);
            json_append_element(node, jn);
            return JsonObject(jn, J_STRING);
        }
        return JsonObject(NULL);
    }

    JsonObject JsonObject::add(double d) {
        if (node && node->tag == JSON_ARRAY) {
            JsonNode *jn = json_mknumber(d);
            json_append_element(node, jn);
            return JsonObject(jn, J_NUMBER);

        }
        return JsonObject(NULL);
    }
    JsonObject JsonObject::add() {
        if (node && node->tag == JSON_ARRAY) {
            JsonNode *jn = json_mkobject();
            json_append_element(node, jn);
            return JsonObject(jn, J_OBJECT);
        }
        return JsonObject(NULL);
    }

    JsonObject JsonObject::set(cc_string name) {
        if (node && node->tag == JSON_OBJECT) {
            JsonNode *jn = json_mkobject();
            json_append_member(node, name, jn);
            return JsonObject(jn, J_STRING);
        }
        return JsonObject(NULL);
    }

    JsonObject JsonObject::setb(cc_string name, bool b) {
        if (node && node->tag == JSON_OBJECT) {
            JsonNode *jn = json_mkbool(b);
            json_append_member(node, name, jn);
            return JsonObject(jn, J_BOOL);
        }
        return JsonObject(NULL);
    }

    JsonObject JsonObject::set(cc_string name, double d) {
        if (node && node->tag == JSON_OBJECT) {
            JsonNode *jn = json_mknumber(d);
            json_append_member(node, name, jn);
            return JsonObject(jn, J_NUMBER);
        }
        return JsonObject(NULL);
    }

    JsonObject JsonObject::set(cc_string name, cc_string v) {
        if (node && node->tag == JSON_OBJECT) {
            JsonNode *jn = json_mkstring(v);
            json_append_member(node, name, jn);
            return JsonObject(jn, J_STRING);
        }
        return JsonObject(NULL);
    }

    JsonObject JsonObject::mkarr(cc_string name) {
        if (node && node->tag == JSON_OBJECT) {
            JsonNode *jn = json_mkarray();
            json_append_member(node, name, jn);
            return JsonObject(jn, J_ARRAY);
        }
        return JsonObject(NULL);
    }


    namespace detail {

        CcanJson::CcanJson(JsonNode *node)
                : root_(node)
        {}

        CcanJson::~CcanJson() {
            if (root_) {
                json_delete(root_);
                root_ = NULL;
            }
        }

        c_string CcanJson::encode(size_t &len, char *space) {
            if (root_)
                return json_stringify(root_, space, &len);
            return NULL;
        }
    }

    Json* Json::create() {
        JsonNode *jn = json_mkobject();
        return new detail::CcanJson(jn);
    }

    Json *Json::decode(cc_string jstr) {
        JsonNode *jnode = json_decode(jstr);
        if (jnode == NULL) {
            $warn("decoding json failed: %s", errno_s);
            return NULL;
        }
        return new detail::CcanJson(jnode);
    }
}