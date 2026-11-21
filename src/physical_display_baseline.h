#pragma once

#include "remote_display_topology.h"

#include <set>

namespace remote_display_topology {
  // A Full enumeration represents disabled outputs with info:null. Reject
  // malformed observations atomically so they cannot replace a saved baseline
  // with a partial list. An empty enumeration can also be a transient outage.
  inline std::optional<std::vector<node_t>> parse_physical_baseline(const nlohmann::json &devices) {
    if (!devices.is_array() || devices.empty()) return std::nullopt;
    std::vector<node_t> nodes;
    std::set<std::string> identities;
    for (const auto &device : devices) {
      if (!device.is_object() || !device.contains("device_id") || !device["device_id"].is_string()) return std::nullopt;
      node_t node;
      node.id = device["device_id"].get<std::string>();
      if (node.id.empty() || !identities.insert(node.id).second) return std::nullopt;
      node.label = node.id;
      for (const auto *key : {"display_name", "friendly_name"}) {
        if (device.contains(key) && device[key].is_string() && !device[key].get_ref<const std::string &>().empty()) {
          node.label = device[key].get<std::string>();
        }
      }
      node.physical = true;
      if (!device.contains("info")) return std::nullopt;
      const auto &info = device["info"];
      if (!info.is_null()) {
        if (!info.is_object()) return std::nullopt;
        for (const auto *key : {"active", "primary"}) {
          if (info.contains(key) && !info[key].is_boolean()) return std::nullopt;
        }
        node.active = info.value("active", true);
        node.primary = node.active && info.value("primary", false);
      }
      nodes.push_back(std::move(node));
    }
    return nodes;
  }
}
