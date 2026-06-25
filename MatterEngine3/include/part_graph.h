#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace part_graph {

struct ParamValue {
    enum class Kind { Number, Bool, Str } kind = Kind::Number;
    double      num = 0.0;
    bool        boolean = false;
    std::string str;
    static ParamValue number(double v) { ParamValue p; p.kind = Kind::Number; p.num = v; return p; }
    static ParamValue boolean_(bool v) { ParamValue p; p.kind = Kind::Bool; p.boolean = v; return p; }
    static ParamValue string_(std::string v) { ParamValue p; p.kind = Kind::Str; p.str = std::move(v); return p; }
};
using Params = std::map<std::string, ParamValue>;

std::string serialize_params(const Params& params);

} // namespace part_graph
