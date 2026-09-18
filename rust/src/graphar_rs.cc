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

#include "arrow/c/bridge.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>

#include "arrow/api.h"
#include "graphar/api/arrow_reader.h"

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

template <typename T>
T value_or_throw(graphar::Result<T> result) {
  if (!result) {
    throw std::runtime_error(result.error().message());
  }
  return std::move(result).value();
}

void status_or_throw(const graphar::Status& status) {
  if (!status.ok()) {
    throw std::runtime_error(status.message());
  }
}

template <typename T>
T arrow_value_or_throw(arrow::Result<T> result) {
  if (!result.ok()) {
    throw std::runtime_error(result.status().ToString());
  }
  return std::move(result).ValueUnsafe();
}

void arrow_status_or_throw(const arrow::Status& status) {
  if (!status.ok()) {
    throw std::runtime_error(status.ToString());
  }
}

std::pair<std::shared_ptr<arrow::Array>, int64_t> locate_array_value(
    const std::shared_ptr<arrow::ChunkedArray>& column, int64_t row) {
  for (const auto& chunk : column->chunks()) {
    if (row < chunk->length()) {
      return {chunk, row};
    }
    row -= chunk->length();
  }
  throw std::runtime_error("Arrow row is outside the selected column");
}

std::string read_string_value(
    const std::shared_ptr<arrow::ChunkedArray>& column, int64_t row,
    bool& valid) {  // NOLINT(runtime/references)
  auto [array, local_row] = locate_array_value(column, row);
  valid = !array->IsNull(local_row);
  if (!valid) {
    return {};
  }
  if (array->type_id() == arrow::Type::STRING) {
    return std::dynamic_pointer_cast<arrow::StringArray>(array)->GetString(
        local_row);
  }
  if (array->type_id() == arrow::Type::LARGE_STRING) {
    return std::dynamic_pointer_cast<arrow::LargeStringArray>(array)->GetString(
        local_row);
  }
  throw std::runtime_error("Requested GraphAr property is not UTF-8");
}

int64_t read_int64_value(const std::shared_ptr<arrow::ChunkedArray>& column,
                         int64_t row) {
  auto [array, local_row] = locate_array_value(column, row);
  if (array->IsNull(local_row) || array->type_id() != arrow::Type::INT64) {
    throw std::runtime_error("GraphAr adjacency endpoint is not a valid int64");
  }
  return std::dynamic_pointer_cast<arrow::Int64Array>(array)->Value(local_row);
}

std::vector<std::shared_ptr<arrow::ChunkedArray>> select_columns(
    const std::vector<std::shared_ptr<arrow::Table>>& tables,
    const std::vector<std::string>& names) {
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns;
  columns.reserve(names.size());
  for (const auto& name : names) {
    std::shared_ptr<arrow::ChunkedArray> column;
    for (const auto& table : tables) {
      column = table->GetColumnByName(name);
      if (column != nullptr) {
        break;
      }
    }
    if (column == nullptr) {
      throw std::runtime_error("Property with name " + name +
                               " does not exist in the selected chunks");
    }
    columns.push_back(std::move(column));
  }
  return columns;
}
}  // namespace

graphar::VertexStringBatch read_vertex_string_batch(
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
  auto vertex_info = graph_info->GetVertexInfo(type);
  std::vector<graphar::VertexPropertyArrowChunkReader> readers;
  std::vector<graphar::GetChunkVersion> reader_versions;
  readers.reserve(vertex_info->GetPropertyGroups().size());
  reader_versions.reserve(vertex_info->GetPropertyGroups().size());
  for (const auto& property_group : vertex_info->GetPropertyGroups()) {
    std::vector<std::string> selected;
    for (const auto& property : property_group->GetProperties()) {
      if (std::find(names.begin(), names.end(), property.name) != names.end()) {
        selected.push_back(property.name);
      }
    }
    if (!selected.empty()) {
      readers.emplace_back(vertex_info, property_group, selected,
                           graph_info->GetPrefix());
      reader_versions.push_back(
          property_group->GetFileType() == graphar::FileType::PARQUET
              ? graphar::GetChunkVersion::V2
              : graphar::GetChunkVersion::V1);
    }
  }
  graphar::VertexStringBatch batch;
  batch.column_count = names.size();
  batch.ids.reserve(collection->size());
  batch.values.reserve(collection->size() * names.size());
  batch.valid.reserve(collection->size() * names.size());
  const auto vertex_count = static_cast<int64_t>(collection->size());
  const auto chunk_size = vertex_info->GetChunkSize();
  for (int64_t chunk_start = 0; chunk_start < vertex_count;
       chunk_start += chunk_size) {
    std::vector<std::shared_ptr<arrow::Table>> tables;
    tables.reserve(readers.size());
    for (size_t index = 0; index < readers.size(); ++index) {
      status_or_throw(readers[index].seek(chunk_start));
      tables.push_back(
          value_or_throw(readers[index].GetChunk(reader_versions[index])));
    }
    const auto columns = select_columns(tables, names);
    const auto rows = std::min(chunk_size, vertex_count - chunk_start);
    for (int64_t row = 0; row < rows; ++row) {
      batch.ids.push_back(chunk_start + row);
      for (const auto& column : columns) {
        bool valid = false;
        batch.values.push_back(read_string_value(column, row, valid));
        batch.valid.push_back(valid);
      }
    }
  }
  return batch;
}

namespace {
using SteadyClock = std::chrono::steady_clock;

uint64_t elapsed_ns(const SteadyClock::time_point& started) {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(SteadyClock::now() -
                                                          started)
          .count());
}

std::shared_ptr<arrow::Table> read_edge_arrow_table(
    const std::shared_ptr<graphar::GraphInfo>& graph_info,
    const std::string& src_type, const std::string& edge_type,
    const std::string& dst_type, graphar::AdjListType adjacency,
    const rust::Vec<rust::String>& properties, size_t max_rows,
    bool preserve_physical_types = false,
    graphar::EdgeArrowReadTimings* timings = nullptr) {
  const auto native_read_started = SteadyClock::now();
  if (timings != nullptr) {
    *timings = graphar::EdgeArrowReadTimings{};
  }
  if (graph_info == nullptr) {
    throw std::runtime_error("EdgesCollection: graph_info must not be null");
  }
  const auto collection_started = SteadyClock::now();
  auto collection_result = graphar::EdgesCollection::Make(
      graph_info, src_type, edge_type, dst_type, adjacency);
  if (!collection_result) {
    throw std::runtime_error(collection_result.error().message());
  }
  auto collection = std::move(collection_result).value();
  enforce_row_budget(collection->size(), max_rows, "EdgesCollection");
  if (timings != nullptr) {
    timings->collection_lookup_ns = elapsed_ns(collection_started);
  }

  const auto reader_setup_started = SteadyClock::now();
  const auto names = to_std_strings(properties);
  auto edge_info = graph_info->GetEdgeInfo(src_type, edge_type, dst_type);
  graphar::AdjListArrowChunkReader adjacency_reader(
      edge_info, adjacency, graph_info->GetPrefix());
  std::vector<graphar::AdjListPropertyArrowChunkReader> property_readers;
  property_readers.reserve(edge_info->GetPropertyGroups().size());
  graphar::util::FilterOptions property_options;
  property_options.preserve_physical_types = preserve_physical_types;
  for (const auto& property_group : edge_info->GetPropertyGroups()) {
    if (std::any_of(property_group->GetProperties().begin(),
                    property_group->GetProperties().end(),
                    [&names](const auto& property) {
                      return std::find(names.begin(), names.end(),
                                       property.name) != names.end();
                    })) {
      property_readers.emplace_back(edge_info, property_group, adjacency,
                                    graph_info->GetPrefix(), property_options);
    }
  }
  if (timings != nullptr) {
    timings->reader_setup_ns = elapsed_ns(reader_setup_started);
  }
  std::vector<std::shared_ptr<arrow::Table>> chunk_tables;
  size_t rows_read = 0;
  while (rows_read < collection->size()) {
    const auto adjacency_read_started = SteadyClock::now();
    auto adjacency_table = value_or_throw(adjacency_reader.GetChunk());
    if (timings != nullptr) {
      timings->adjacency_read_ns += elapsed_ns(adjacency_read_started);
    }
    if (adjacency_table == nullptr) {
      const auto chunk_advance_started = SteadyClock::now();
      status_or_throw(adjacency_reader.next_chunk());
      for (auto& reader : property_readers) {
        status_or_throw(reader.next_chunk());
      }
      if (timings != nullptr) {
        timings->chunk_advance_ns += elapsed_ns(chunk_advance_started);
      }
      continue;
    }
    const auto property_read_started = SteadyClock::now();
    std::vector<std::shared_ptr<arrow::Table>> property_tables;
    property_tables.reserve(property_readers.size());
    for (auto& reader : property_readers) {
      property_tables.push_back(value_or_throw(reader.GetChunk()));
    }
    if (timings != nullptr) {
      timings->property_read_ns += elapsed_ns(property_read_started);
    }
    const auto column_projection_started = SteadyClock::now();
    const auto columns = select_columns(property_tables, names);
    if (timings != nullptr) {
      timings->column_projection_ns += elapsed_ns(column_projection_started);
    }
    const auto table_assembly_started = SteadyClock::now();
    const auto sources = adjacency_table->column(0);
    const auto destinations = adjacency_table->column(1);
    std::vector<std::shared_ptr<arrow::Field>> fields{
        arrow::field("__source", sources->type(), false),
        arrow::field("__destination", destinations->type(), false)};
    std::vector<std::shared_ptr<arrow::ChunkedArray>> combined_columns{
        sources, destinations};
    fields.reserve(2 + names.size());
    combined_columns.reserve(2 + names.size());
    for (size_t index = 0; index < names.size(); ++index) {
      fields.push_back(arrow::field(names[index], columns[index]->type(), true));
      combined_columns.push_back(columns[index]);
    }
    chunk_tables.push_back(arrow::Table::Make(
        arrow::schema(std::move(fields)), std::move(combined_columns)));
    if (timings != nullptr) {
      timings->table_assembly_ns += elapsed_ns(table_assembly_started);
    }
    rows_read += static_cast<size_t>(adjacency_table->num_rows());
    if (rows_read < collection->size()) {
      const auto chunk_advance_started = SteadyClock::now();
      status_or_throw(adjacency_reader.next_chunk());
      for (auto& reader : property_readers) {
        status_or_throw(reader.next_chunk());
      }
      if (timings != nullptr) {
        timings->chunk_advance_ns += elapsed_ns(chunk_advance_started);
      }
    }
  }
  if (chunk_tables.empty()) {
    throw std::runtime_error(
        "Arrow edge scan cannot export an empty collection yet");
  }
  const auto concatenate_started = SteadyClock::now();
  auto table = arrow_value_or_throw(arrow::ConcatenateTables(chunk_tables));
  if (timings != nullptr) {
    timings->concatenate_ns = elapsed_ns(concatenate_started);
    timings->native_read_ns = elapsed_ns(native_read_started);
  }
  return table;
}

graphar::EdgeStringBatch materialize_edge_string_batch(
    const std::shared_ptr<arrow::Table>& table, size_t property_count) {
  graphar::EdgeStringBatch batch;
  batch.column_count = property_count;
  batch.row_count = static_cast<size_t>(table->num_rows());
  batch.sources.reserve(batch.row_count);
  batch.destinations.reserve(batch.row_count);
  batch.values.reserve(batch.row_count * property_count);
  batch.valid.reserve(batch.row_count * property_count);
  const auto sources = table->column(0);
  const auto destinations = table->column(1);
  for (int64_t row = 0; row < table->num_rows(); ++row) {
    batch.sources.push_back(read_int64_value(sources, row));
    batch.destinations.push_back(read_int64_value(destinations, row));
    for (size_t column_index = 0; column_index < property_count;
         ++column_index) {
      bool valid = false;
      batch.values.push_back(
          read_string_value(table->column(column_index + 2), row, valid));
      batch.valid.push_back(valid);
    }
  }
  return batch;
}
}  // namespace

graphar::EdgeStringBatch read_edge_string_batch(
    const std::shared_ptr<graphar::GraphInfo>& graph_info,
    const std::string& src_type, const std::string& edge_type,
    const std::string& dst_type, graphar::AdjListType adjacency,
    const rust::Vec<rust::String>& properties, size_t max_rows) {
  return materialize_edge_string_batch(
      read_edge_arrow_table(graph_info, src_type, edge_type, dst_type, adjacency,
                            properties, max_rows),
      properties.size());
}

size_t scan_edge_arrow_chunks(
    const std::shared_ptr<graphar::GraphInfo>& graph_info,
    const std::string& src_type, const std::string& edge_type,
    const std::string& dst_type, graphar::AdjListType adjacency,
    const rust::Vec<rust::String>& properties, size_t max_rows) {
  return static_cast<size_t>(
      read_edge_arrow_table(graph_info, src_type, edge_type, dst_type, adjacency,
                            properties, max_rows)
          ->num_rows());
}

void export_edge_arrow_stream(
    const std::shared_ptr<graphar::GraphInfo>& graph_info,
    const std::string& src_type, const std::string& edge_type,
    const std::string& dst_type, graphar::AdjListType adjacency,
    const rust::Vec<rust::String>& properties, size_t max_rows,
    size_t stream_address) {
  auto table = read_edge_arrow_table(graph_info, src_type, edge_type, dst_type,
                                     adjacency, properties, max_rows);
  auto reader = std::make_shared<arrow::TableBatchReader>(std::move(table));
  auto* stream = reinterpret_cast<ArrowArrayStream*>(stream_address);
  arrow_status_or_throw(
      arrow::ExportRecordBatchReader(std::move(reader), stream));
}

graphar::EdgeArrowReadTimings export_edge_arrow_stream_observed(
    const std::shared_ptr<graphar::GraphInfo>& graph_info,
    const std::string& src_type, const std::string& edge_type,
    const std::string& dst_type, graphar::AdjListType adjacency,
    const rust::Vec<rust::String>& properties, size_t max_rows,
    size_t stream_address) {
  graphar::EdgeArrowReadTimings timings{};
  auto table = read_edge_arrow_table(graph_info, src_type, edge_type, dst_type,
                                     adjacency, properties, max_rows, true,
                                     &timings);
  auto reader = std::make_shared<arrow::TableBatchReader>(std::move(table));
  auto* stream = reinterpret_cast<ArrowArrayStream*>(stream_address);
  const auto stream_export_started = SteadyClock::now();
  arrow_status_or_throw(
      arrow::ExportRecordBatchReader(std::move(reader), stream));
  timings.stream_export_ns = elapsed_ns(stream_export_started);
  return timings;
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
