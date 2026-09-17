/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "graphar-rs/src/ffi.rs.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

namespace graphar_rs {
rust::String to_type_name(const graphar::DataType& type) {
  return rust::String(type.ToTypeName());
}

std::shared_ptr<graphar::ConstInfoVersion> new_const_info_version(
    int32_t version) {
  // Let any upstream exceptions propagate to Rust via `cxx::Exception`.
  return std::make_shared<graphar::InfoVersion>(static_cast<int>(version));
}

std::unique_ptr<graphar::Property> new_property(
    const std::string& name, std::shared_ptr<graphar::DataType> type,
    bool is_primary, bool is_nullable, graphar::Cardinality cardinality) {
  return std::make_unique<graphar::Property>(name, type, is_primary,
                                             is_nullable, cardinality);
}
const std::string& property_get_name(const graphar::Property& prop) {
  return prop.name;
}
const std::shared_ptr<graphar::DataType>& property_get_type(
    const graphar::Property& prop) {
  return prop.type;
}
bool property_is_primary(const graphar::Property& prop) {
  return prop.is_primary;
}
bool property_is_nullable(const graphar::Property& prop) {
  return prop.is_nullable;
}
graphar::Cardinality property_get_cardinality(const graphar::Property& prop) {
  return prop.cardinality;
}
std::unique_ptr<graphar::Property> property_clone(
    const graphar::Property& prop) {
  return std::make_unique<graphar::Property>(prop);
}

void property_vec_push_property(std::vector<graphar::Property>& properties,
                                std::unique_ptr<graphar::Property> prop) {
  properties.emplace_back(*prop);
}

void property_vec_emplace_property(std::vector<graphar::Property>& properties,
                                   const std::string& name,
                                   std::shared_ptr<graphar::DataType> type,
                                   bool is_primary, bool is_nullable,
                                   graphar::Cardinality cardinality) {
  properties.emplace_back(name, type, is_primary, is_nullable, cardinality);
}

std::unique_ptr<std::vector<graphar::Property>> property_vec_clone(
    const std::vector<graphar::Property>& properties) {
  return std::make_unique<std::vector<graphar::Property>>(properties);
}

void property_group_vec_push_property_group(
    std::vector<graphar::SharedPropertyGroup>& property_groups,
    std::shared_ptr<graphar::PropertyGroup> property_group) {
  property_groups.emplace_back(std::move(property_group));
}

std::unique_ptr<std::vector<graphar::SharedPropertyGroup>>
property_group_vec_clone(
    const std::vector<graphar::SharedPropertyGroup>& property_groups) {
  return std::make_unique<std::vector<graphar::SharedPropertyGroup>>(
      property_groups);
}

std::shared_ptr<graphar::VertexInfo> create_vertex_info(
    const std::string& type, graphar::IdType chunk_size,
    const std::vector<graphar::SharedPropertyGroup>& property_groups,
    const rust::Vec<rust::String>& labels, const std::string& prefix,
    std::shared_ptr<graphar::ConstInfoVersion> version) {
  if (type.empty()) {
    throw std::runtime_error("CreateVertexInfo: type must not be empty");
  }
  if (chunk_size <= 0) {
    throw std::runtime_error("CreateVertexInfo: chunk_size must be > 0");
  }

  std::vector<std::string> label_vec;
  label_vec.reserve(labels.size());
  for (size_t i = 0; i < labels.size(); ++i) {
    label_vec.emplace_back(std::string(labels[i]));
  }

  auto vertex_info = graphar::CreateVertexInfo(
      type, chunk_size, property_groups, label_vec, prefix, std::move(version));
  if (vertex_info == nullptr) {
    throw std::runtime_error("CreateVertexInfo: returned nullptr");
  }
  return vertex_info;
}

std::shared_ptr<graphar::EdgeInfo> create_edge_info(
    const std::string& src_type, const std::string& edge_type,
    const std::string& dst_type, graphar::IdType chunk_size,
    graphar::IdType src_chunk_size, graphar::IdType dst_chunk_size,
    bool directed, const graphar::AdjacentListVector& adjacent_lists,
    const std::vector<graphar::SharedPropertyGroup>& property_groups,
    const std::string& prefix,
    std::shared_ptr<graphar::ConstInfoVersion> version) {
  if (src_type.empty()) {
    throw std::runtime_error("CreateEdgeInfo: src_type must not be empty");
  }
  if (edge_type.empty()) {
    throw std::runtime_error("CreateEdgeInfo: edge_type must not be empty");
  }
  if (dst_type.empty()) {
    throw std::runtime_error("CreateEdgeInfo: dst_type must not be empty");
  }
  if (chunk_size <= 0) {
    throw std::runtime_error("CreateEdgeInfo: chunk_size must be > 0");
  }
  if (src_chunk_size <= 0) {
    throw std::runtime_error("CreateEdgeInfo: src_chunk_size must be > 0");
  }
  if (dst_chunk_size <= 0) {
    throw std::runtime_error("CreateEdgeInfo: dst_chunk_size must be > 0");
  }
  if (adjacent_lists.empty()) {
    throw std::runtime_error(
        "CreateEdgeInfo: adjacent_lists must not be empty");
  }

  auto edge_info = graphar::CreateEdgeInfo(
      src_type, edge_type, dst_type, chunk_size, src_chunk_size, dst_chunk_size,
      directed, adjacent_lists, property_groups, prefix, std::move(version));
  if (edge_info == nullptr) {
    throw std::runtime_error("CreateEdgeInfo: returned nullptr");
  }
  return edge_info;
}

std::shared_ptr<graphar::GraphInfo> load_graph_info(const std::string& path) {
  auto loaded = graphar::GraphInfo::Load(path);
  if (!loaded) {
    throw std::runtime_error(loaded.error().message());
  }
  return std::move(loaded).value();
}

std::shared_ptr<graphar::GraphInfo> create_graph_info(
    const std::string& name,
    const std::vector<graphar::SharedVertexInfo>& vertex_infos,
    const std::vector<graphar::SharedEdgeInfo>& edge_infos,
    const rust::Vec<rust::String>& labels, const std::string& prefix,
    std::shared_ptr<graphar::ConstInfoVersion> version) {
  if (name.empty()) {
    throw std::runtime_error("CreateGraphInfo: name must not be empty");
  }

  std::vector<std::string> label_vec;
  label_vec.reserve(labels.size());
  for (size_t i = 0; i < labels.size(); ++i) {
    label_vec.emplace_back(std::string(labels[i]));
  }

  auto graph_info = graphar::CreateGraphInfo(name, vertex_infos, edge_infos,
                                             label_vec, prefix, version);
  if (graph_info == nullptr) {
    throw std::runtime_error("CreateGraphInfo: returned nullptr");
  }
  // if (!graph_info->IsValidated()) {
  //   throw std::runtime_error("CreateGraphInfo: graph info is not validated");
  // }
  return graph_info;
}

namespace {
std::vector<std::string> to_std_strings(
    const rust::Vec<rust::String>& strings) {
  std::vector<std::string> values;
  values.reserve(strings.size());
  for (const auto& value : strings) {
    values.emplace_back(std::string(value));
  }
  return values;
}

void enforce_row_budget(size_t rows, size_t max_rows,
                        const char* collection_name) {
  if (rows > max_rows) {
    throw std::runtime_error(
        std::string(collection_name) + " contains " + std::to_string(rows) +
        " rows, exceeding max_rows=" + std::to_string(max_rows));
  }
}
}  // namespace

rust::Vec<graphar::VertexStringRecord> read_vertex_string_records(
    const std::shared_ptr<graphar::GraphInfo>& graph_info,
    const std::string& type, const rust::Vec<rust::String>& properties,
    size_t max_rows) {
  if (graph_info == nullptr) {
    throw std::runtime_error("VerticesCollection: graph_info must not be null");
  }
  auto collection_result = graphar::VerticesCollection::Make(graph_info, type);
  if (!collection_result) {
    throw std::runtime_error(collection_result.error().message());
  }
  auto collection = std::move(collection_result).value();
  enforce_row_budget(collection->size(), max_rows, "VerticesCollection");

  const auto names = to_std_strings(properties);
  rust::Vec<graphar::VertexStringRecord> records;
  records.reserve(collection->size());
  for (auto iter = collection->begin(); iter != collection->end(); ++iter) {
    auto vertex = *iter;
    graphar::VertexStringRecord record;
    record.id = vertex.id();
    record.values.reserve(names.size());
    record.valid.reserve(names.size());
    for (const auto& name : names) {
      const bool valid = vertex.IsValid(name);
      record.valid.push_back(valid);
      if (valid) {
        auto value = vertex.property<std::string>(name);
        if (!value) {
          throw std::runtime_error(value.error().message());
        }
        record.values.push_back(std::move(value).value());
      } else {
        record.values.push_back("");
      }
    }
    records.push_back(std::move(record));
  }
  return records;
}

rust::Vec<graphar::EdgeStringRecord> read_edge_string_records(
    const std::shared_ptr<graphar::GraphInfo>& graph_info,
    const std::string& src_type, const std::string& edge_type,
    const std::string& dst_type, graphar::AdjListType adjacency,
    const rust::Vec<rust::String>& properties, size_t max_rows) {
  if (graph_info == nullptr) {
    throw std::runtime_error("EdgesCollection: graph_info must not be null");
  }
  auto collection_result = graphar::EdgesCollection::Make(
      graph_info, src_type, edge_type, dst_type, adjacency);
  if (!collection_result) {
    throw std::runtime_error(collection_result.error().message());
  }
  auto collection = std::move(collection_result).value();
  enforce_row_budget(collection->size(), max_rows, "EdgesCollection");

  const auto names = to_std_strings(properties);
  rust::Vec<graphar::EdgeStringRecord> records;
  records.reserve(collection->size());
  for (auto iter = collection->begin(); iter != collection->end(); ++iter) {
    auto edge = *iter;
    graphar::EdgeStringRecord record;
    record.source = edge.source();
    record.destination = edge.destination();
    record.values.reserve(names.size());
    record.valid.reserve(names.size());
    for (const auto& name : names) {
      const bool valid = edge.IsValid(name);
      record.valid.push_back(valid);
      if (valid) {
        auto value = edge.property<std::string>(name);
        if (!value) {
          throw std::runtime_error(value.error().message());
        }
        record.values.push_back(std::move(value).value());
      } else {
        record.values.push_back("");
      }
    }
    records.push_back(std::move(record));
  }
  return records;
}

static graphar::MaybeIndex optional_to_maybe_index(std::optional<size_t> opt) {
  if (opt) {
    return graphar::MaybeIndex{true, *opt};
  } else {
    return graphar::MaybeIndex{false, 0};
  }
}

graphar::MaybeIndex graph_info_vertex_info_index(
    const graphar::GraphInfo& graph_info, const std::string& type) {
  return optional_to_maybe_index(graph_info.GetVertexInfoIndex(type));
}

graphar::MaybeIndex graph_info_edge_info_index(
    const graphar::GraphInfo& graph_info, const std::string& src_type,
    const std::string& edge_type, const std::string& dst_type) {
  return optional_to_maybe_index(
      graph_info.GetEdgeInfoIndex(src_type, edge_type, dst_type));
}

void vertex_info_vec_push_vertex_info(
    std::vector<graphar::SharedVertexInfo>& vertex_infos,
    std::shared_ptr<graphar::VertexInfo> vertex_info) {
  vertex_infos.emplace_back(std::move(vertex_info));
}

void edge_info_vec_push_edge_info(
    std::vector<graphar::SharedEdgeInfo>& edge_infos,
    std::shared_ptr<graphar::EdgeInfo> edge_info) {
  edge_infos.emplace_back(std::move(edge_info));
}

void vertex_info_save(const graphar::VertexInfo& vertex_info,
                      const std::string& path) {
  auto status = vertex_info.Save(path);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}

std::unique_ptr<std::string> vertex_info_dump(
    const graphar::VertexInfo& vertex_info) {
  auto dumped = vertex_info.Dump();
  if (!dumped) {
    throw std::runtime_error(dumped.error().message());
  }
  return std::make_unique<std::string>(std::move(dumped).value());
}

std::unique_ptr<graphar::AdjacentListVector> new_adjacent_list_vec() {
  return std::make_unique<graphar::AdjacentListVector>();
}

void push_adjacent_list(graphar::AdjacentListVector& v,
                        std::shared_ptr<graphar::AdjacentList> adjacent_list) {
  v.emplace_back(std::move(adjacent_list));
}

void edge_info_save(const graphar::EdgeInfo& edge_info,
                    const std::string& path) {
  auto status = edge_info.Save(path);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}

std::unique_ptr<std::string> edge_info_dump(
    const graphar::EdgeInfo& edge_info) {
  auto r = edge_info.Dump();
  if (!r) {
    throw std::runtime_error(r.error().message());
  }
  return std::make_unique<std::string>(std::move(r).value());
}

void graph_info_save(const graphar::GraphInfo& graph_info,
                     const std::string& path) {
  auto status = graph_info.Save(path);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}

std::unique_ptr<std::string> graph_info_dump(
    const graphar::GraphInfo& graph_info) {
  auto dumped = graph_info.Dump();
  if (!dumped) {
    throw std::runtime_error(dumped.error().message());
  }
  return std::make_unique<std::string>(std::move(dumped).value());
}

// =========================== Builder ===========================
// `Vertex`
std::unique_ptr<graphar::builder::Vertex> new_vertex_builder() {
  return std::make_unique<graphar::builder::Vertex>();
}

void add_vertex(graphar::builder::VerticesBuilder& builder,
                graphar::builder::Vertex& v) {
  auto status = builder.AddVertex(v);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}

std::unique_ptr<graphar::builder::VerticesBuilder> new_vertices_builder(
    const std::shared_ptr<graphar::VertexInfo>& vertex_info,
    const std::string& path_prefix, i64 start_idx) {
  if (vertex_info == nullptr) {
    throw std::runtime_error("VerticesBuilder: vertex_info must not be null");
  }
  if (start_idx < 0) {
    throw std::runtime_error("VerticesBuilder: start_idx must be >= 0");
  }

  return std::make_unique<graphar::builder::VerticesBuilder>(
      vertex_info, path_prefix, static_cast<graphar::IdType>(start_idx));
}

void vertices_dump(graphar::builder::VerticesBuilder& builder) {
  auto status = builder.Dump();
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}

// `Edge`
std::unique_ptr<graphar::builder::Edge> new_edge_builder(i64 source,
                                                         i64 destination) {
  if (source < 0 || destination < 0) {
    throw std::runtime_error("Edge: vertex IDs must be >= 0");
  }
  return std::make_unique<graphar::builder::Edge>(source, destination);
}

i64 edge_builder_source(const graphar::builder::Edge& edge) {
  return edge.GetSource();
}

i64 edge_builder_destination(const graphar::builder::Edge& edge) {
  return edge.GetDestination();
}

bool edge_builder_is_empty(const graphar::builder::Edge& edge) {
  return edge.Empty();
}

bool edge_builder_contains_property(const graphar::builder::Edge& edge,
                                    const std::string& name) {
  return edge.ContainProperty(name);
}

// `EdgesBuilder`
std::unique_ptr<graphar::builder::EdgesBuilder> new_edges_builder(
    const std::shared_ptr<graphar::EdgeInfo>& edge_info,
    const std::string& path_prefix, graphar::AdjListType adjacency,
    i64 num_vertices) {
  if (edge_info == nullptr) {
    throw std::runtime_error("EdgesBuilder: edge_info must not be null");
  }
  if (!edge_info->HasAdjacentListType(adjacency)) {
    throw std::runtime_error(
        "EdgesBuilder: adjacency list type is absent from edge_info");
  }
  if (num_vertices < 0) {
    throw std::runtime_error("EdgesBuilder: num_vertices must be >= 0");
  }
  return std::make_unique<graphar::builder::EdgesBuilder>(
      edge_info, path_prefix, adjacency, num_vertices, nullptr,
      graphar::ValidateLevel::strong_validate);
}

void add_edge(graphar::builder::EdgesBuilder& builder,
              graphar::builder::Edge& edge) {
  auto status = builder.AddEdge(edge);
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}

i64 edges_len(const graphar::builder::EdgesBuilder& builder) {
  return builder.GetNum();
}

void edges_clear(graphar::builder::EdgesBuilder& builder) { builder.Clear(); }

void edges_dump(graphar::builder::EdgesBuilder& builder) {
  auto status = builder.Dump();
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}
}  // namespace graphar_rs
