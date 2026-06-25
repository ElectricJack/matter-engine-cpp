#include "part_graph.h"
#include "part_asset_v2.h"   // SP-1 (MatterEngine3, via -I../include): compute_resolved_hash,
                            //   cache_path_resolved; pulls in v1 part_asset.h for fnv1a64
#include <cstdio>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

namespace part_graph {

std::string serialize_params(const Params& params) {
    std::string out;
    for (const auto& kv : params) {          // std::map iterates in sorted key order
        out += kv.first;
        out += '=';
        const ParamValue& v = kv.second;
        switch (v.kind) {
            case ParamValue::Kind::Number: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.17g", v.num);
                out += buf;
                break;
            }
            case ParamValue::Kind::Bool:
                out += (v.boolean ? "true" : "false");
                break;
            case ParamValue::Kind::Str:
                out += v.str;
                break;
        }
        out += ';';
    }
    return out;
}

PartGraph::PartGraph(ModuleResolver& resolver, Baker& baker)
    : resolver_(resolver), baker_(baker) {}

InstallResult PartGraph::install(const std::vector<ChildRequest>&) {
    InstallResult r;
    r.ok = false;
    r.error = "not implemented";   // Task 4+ implements resolve/topo/bake
    return r;
}

} // namespace part_graph
