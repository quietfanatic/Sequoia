#include <string>

#include "tap.h"
#include "tests.h"
#include "../util/json.h"

using namespace json;
using namespace std;

void t (const string& s) {
    catch_fail([&]{
        auto v = parse(s);
        auto s2 = stringify(v);
        return is(s2, s, s);
    }, s);
}

Test json_test {"json", []{
    plan(15);
    t("null");
    t("true");
    t("false");
    t("0");
    t("12343");
    t("35.324");
    t("-45.5");
    t("1.3241e-54");
    t("\"foo\"");
    t("\"a \\n b \\r c \\t c \\\\ \\\" \"");
    t("[]");
    t("[2,4,6,null,true,\"asdf\"]");
    t("{}");
    t("{\"asdf\":\"ghjk\"}");
    t("{\"foo\":[4,5,6],\"agfd\":[[[[4]]],{},{\"gtre\":null}],\"goo\":{}}");
}};

