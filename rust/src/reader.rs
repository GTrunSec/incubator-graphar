// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

//! Bounded high-level readers backed by GraphAr collections.

use arrow_array::ffi_stream::{ArrowArrayStreamReader, FFI_ArrowArrayStream};
use cxx::let_cxx_string;

use crate::{ffi, info::AdjListType, info::GraphInfo};

/// A vertex and its requested UTF-8 property values.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VertexStringRecord {
    id: i64,
    values: Vec<Option<String>>,
}

impl VertexStringRecord {
    /// Return the GraphAr-internal vertex ID.
    pub fn id(&self) -> i64 {
        self.id
    }

    /// Return values in the same order as the requested property names.
    pub fn values(&self) -> &[Option<String>] {
        &self.values
    }
}

/// An edge and its requested UTF-8 property values.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EdgeStringRecord {
    source: i64,
    destination: i64,
    values: Vec<Option<String>>,
}

/// Columnar vertex strings transferred with one allocation per field vector.
#[derive(Debug)]
pub struct VertexStringBatch {
    ids: Vec<i64>,
    values: Vec<String>,
    valid: Vec<bool>,
    column_count: usize,
}

impl VertexStringBatch {
    /// Number of vertex rows in the batch.
    pub fn row_count(&self) -> usize {
        self.ids.len()
    }

    /// Number of requested property columns in the batch.
    pub fn column_count(&self) -> usize {
        self.column_count
    }

    /// GraphAr-internal vertex ID for `row`.
    pub fn id(&self, row: usize) -> Option<i64> {
        self.ids.get(row).copied()
    }

    /// UTF-8 property at `(row, column)`, or `None` for null/out-of-bounds.
    pub fn value(&self, row: usize, column: usize) -> Option<&str> {
        let index = flat_index(row, column, self.column_count)?;
        self.valid
            .get(index)
            .copied()
            .unwrap_or(false)
            .then(|| self.values.get(index).map(String::as_str))
            .flatten()
    }

    fn into_records(self) -> Vec<VertexStringRecord> {
        let mut values = self
            .values
            .into_iter()
            .zip(self.valid)
            .map(|(value, valid)| valid.then_some(value));
        self.ids
            .into_iter()
            .map(|id| VertexStringRecord {
                id,
                values: values.by_ref().take(self.column_count).collect(),
            })
            .collect()
    }
}

/// Columnar edge strings transferred with one allocation per field vector.
#[derive(Debug)]
pub struct EdgeStringBatch {
    sources: Vec<i64>,
    destinations: Vec<i64>,
    values: Vec<String>,
    valid: Vec<bool>,
    column_count: usize,
    row_count: usize,
}

impl EdgeStringBatch {
    /// Number of edge rows in the batch.
    pub fn row_count(&self) -> usize {
        self.row_count
    }

    /// Number of requested property columns in the batch.
    pub fn column_count(&self) -> usize {
        self.column_count
    }

    /// GraphAr-internal source vertex ID for `row`.
    pub fn source(&self, row: usize) -> Option<i64> {
        self.sources.get(row).copied()
    }

    /// GraphAr-internal destination vertex ID for `row`.
    pub fn destination(&self, row: usize) -> Option<i64> {
        self.destinations.get(row).copied()
    }

    /// UTF-8 property at `(row, column)`, or `None` for null/out-of-bounds.
    pub fn value(&self, row: usize, column: usize) -> Option<&str> {
        let index = flat_index(row, column, self.column_count)?;
        self.valid
            .get(index)
            .copied()
            .unwrap_or(false)
            .then(|| self.values.get(index).map(String::as_str))
            .flatten()
    }

    fn into_records(self) -> Vec<EdgeStringRecord> {
        let mut values = self
            .values
            .into_iter()
            .zip(self.valid)
            .map(|(value, valid)| valid.then_some(value));
        self.sources
            .into_iter()
            .zip(self.destinations)
            .map(|(source, destination)| EdgeStringRecord {
                source,
                destination,
                values: values.by_ref().take(self.column_count).collect(),
            })
            .collect()
    }
}

fn flat_index(row: usize, column: usize, column_count: usize) -> Option<usize> {
    (column < column_count)
        .then(|| row.checked_mul(column_count)?.checked_add(column))
        .flatten()
}

impl EdgeStringRecord {
    /// Return the GraphAr-internal source vertex ID.
    pub fn source(&self) -> i64 {
        self.source
    }

    /// Return the GraphAr-internal destination vertex ID.
    pub fn destination(&self) -> i64 {
        self.destination
    }

    /// Return values in the same order as the requested property names.
    pub fn values(&self) -> &[Option<String>] {
        &self.values
    }
}

/// Read UTF-8 properties for all vertices of `vertex_type`.
///
/// `max_rows` is checked by the native collection before allocating result
/// records. Property values preserve the order of `properties`; null values
/// are returned as `None`.
pub fn read_vertex_strings<S: AsRef<str>>(
    graph_info: &GraphInfo,
    vertex_type: S,
    properties: &[String],
    max_rows: usize,
) -> crate::Result<Vec<VertexStringRecord>> {
    read_vertex_string_batch(graph_info, vertex_type, properties, max_rows)
        .map(VertexStringBatch::into_records)
}

/// Read vertex UTF-8 properties as one flattened row-major batch.
///
/// This preserves Arrow chunk order while avoiding one nested values vector
/// allocation per vertex row.
pub fn read_vertex_string_batch<S: AsRef<str>>(
    graph_info: &GraphInfo,
    vertex_type: S,
    properties: &[String],
    max_rows: usize,
) -> crate::Result<VertexStringBatch> {
    let_cxx_string!(vertex_type = vertex_type.as_ref());
    let properties = properties.to_vec();
    ffi::graphar::read_vertex_string_batch(&graph_info.0, &vertex_type, &properties, max_rows)
        .map(|batch| VertexStringBatch {
            ids: batch.ids,
            values: batch.values,
            valid: batch.valid,
            column_count: batch.column_count,
        })
        .map_err(Into::into)
}

/// Read UTF-8 properties for all edges of an edge triplet and adjacency type.
///
/// `max_rows` is checked by the native collection before allocating result
/// records. Property values preserve the order of `properties`; null values
/// are returned as `None`.
#[allow(clippy::too_many_arguments)]
pub fn read_edge_strings<S1, S2, S3>(
    graph_info: &GraphInfo,
    src_type: S1,
    edge_type: S2,
    dst_type: S3,
    adjacency: AdjListType,
    properties: &[String],
    max_rows: usize,
) -> crate::Result<Vec<EdgeStringRecord>>
where
    S1: AsRef<str>,
    S2: AsRef<str>,
    S3: AsRef<str>,
{
    read_edge_string_batch(
        graph_info, src_type, edge_type, dst_type, adjacency, properties, max_rows,
    )
    .map(EdgeStringBatch::into_records)
}

/// Read edge endpoints and UTF-8 properties as one flattened row-major batch.
///
/// This preserves Arrow chunk order while avoiding one nested values vector
/// allocation per edge row.
#[allow(clippy::too_many_arguments)]
pub fn read_edge_string_batch<S1, S2, S3>(
    graph_info: &GraphInfo,
    src_type: S1,
    edge_type: S2,
    dst_type: S3,
    adjacency: AdjListType,
    properties: &[String],
    max_rows: usize,
) -> crate::Result<EdgeStringBatch>
where
    S1: AsRef<str>,
    S2: AsRef<str>,
    S3: AsRef<str>,
{
    let_cxx_string!(src_type = src_type.as_ref());
    let_cxx_string!(edge_type = edge_type.as_ref());
    let_cxx_string!(dst_type = dst_type.as_ref());
    let properties = properties.to_vec();
    ffi::graphar::read_edge_string_batch(
        &graph_info.0,
        &src_type,
        &edge_type,
        &dst_type,
        adjacency,
        &properties,
        max_rows,
    )
    .map(|batch| EdgeStringBatch {
        sources: batch.sources,
        destinations: batch.destinations,
        values: batch.values,
        valid: batch.valid,
        column_count: batch.column_count,
        row_count: batch.row_count,
    })
    .map_err(Into::into)
}

/// Scan the same upstream Arrow chunks as [`read_edge_string_batch`] without
/// materializing endpoint or UTF-8 values across the Rust bridge.
///
/// This is the native GraphAr reference path for measuring bridge overhead on
/// an identical graph, adjacency representation, and property projection.
#[allow(clippy::too_many_arguments)]
pub fn scan_edge_arrow_chunks<S1, S2, S3>(
    graph_info: &GraphInfo,
    src_type: S1,
    edge_type: S2,
    dst_type: S3,
    adjacency: AdjListType,
    properties: &[String],
    max_rows: usize,
) -> crate::Result<usize>
where
    S1: AsRef<str>,
    S2: AsRef<str>,
    S3: AsRef<str>,
{
    let_cxx_string!(src_type = src_type.as_ref());
    let_cxx_string!(edge_type = edge_type.as_ref());
    let_cxx_string!(dst_type = dst_type.as_ref());
    let properties = properties.to_vec();
    ffi::graphar::scan_edge_arrow_chunks(
        &graph_info.0,
        &src_type,
        &edge_type,
        &dst_type,
        adjacency,
        &properties,
        max_rows,
    )
    .map_err(Into::into)
}

/// Export the selected upstream edge Arrow chunks through the Arrow C Stream
/// Interface without copying their buffers.
///
/// The resulting Rust reader owns the exported C stream and releases the
/// native Arrow buffers after the last record batch is dropped.
#[allow(clippy::too_many_arguments)]
pub fn read_edge_arrow_batches<S1, S2, S3>(
    graph_info: &GraphInfo,
    src_type: S1,
    edge_type: S2,
    dst_type: S3,
    adjacency: AdjListType,
    properties: &[String],
    max_rows: usize,
) -> crate::Result<ArrowArrayStreamReader>
where
    S1: AsRef<str>,
    S2: AsRef<str>,
    S3: AsRef<str>,
{
    let_cxx_string!(src_type = src_type.as_ref());
    let_cxx_string!(edge_type = edge_type.as_ref());
    let_cxx_string!(dst_type = dst_type.as_ref());
    let properties = properties.to_vec();
    let mut stream = FFI_ArrowArrayStream::empty();
    let stream_address = std::ptr::addr_of_mut!(stream) as usize;
    ffi::graphar::export_edge_arrow_stream(
        &graph_info.0,
        &src_type,
        &edge_type,
        &dst_type,
        adjacency,
        &properties,
        max_rows,
        stream_address,
    )?;
    ArrowArrayStreamReader::try_new(stream).map_err(|error| crate::Error::InvalidArgument {
        name: "arrow_stream",
        reason: error.to_string(),
    })
}

#[cfg(test)]
mod tests {
    use super::{
        read_edge_arrow_batches, read_edge_string_batch, read_edge_strings,
        read_vertex_string_batch, read_vertex_strings, scan_edge_arrow_chunks,
    };
    use crate::builder::{Edge, EdgesBuilder, Vertex, VerticesBuilder};
    use crate::info::{AdjListType, AdjacentList, EdgeInfo, GraphInfo, VertexInfo};
    use crate::property::{Property, PropertyGroup, PropertyGroupVector, PropertyVec};
    use crate::types::{Cardinality, DataType, FileType};
    use tempfile::tempdir;

    fn string_properties() -> PropertyGroupVector {
        let mut key = PropertyVec::new();
        key.push(Property::new(
            "key",
            DataType::string(),
            true,
            false,
            Cardinality::Single,
        ));
        let mut note = PropertyVec::new();
        note.push(Property::new(
            "note",
            DataType::string(),
            false,
            true,
            Cardinality::Single,
        ));
        let mut groups = PropertyGroupVector::new();
        groups.push(PropertyGroup::new(key, FileType::Csv, "key/"));
        groups.push(PropertyGroup::new(note, FileType::Csv, "note/"));
        groups
    }

    #[test]
    fn native_collections_read_written_vertices_and_edges() {
        let directory = tempdir().unwrap();
        let prefix = format!("{}/", directory.path().display());
        let vertex_info = VertexInfo::builder("entity", 4)
            .property_groups(string_properties())
            .prefix("vertex/entity/")
            .try_build()
            .unwrap();
        let edge_info = EdgeInfo::builder("entity", "relates", "entity", 4, 4, 4)
            .directed(true)
            .push_adjacent_list(AdjacentList::new(
                AdjListType::UnorderedBySource,
                FileType::Csv,
                Some("unordered_by_source/"),
            ))
            .property_groups(string_properties())
            .prefix("edge/entity_relates_entity/")
            .try_build()
            .unwrap();
        let graph_info = GraphInfo::try_new(
            "fixture",
            vec![vertex_info.clone()],
            vec![edge_info.clone()],
            vec![],
            &prefix,
            None,
        )
        .unwrap();

        let mut vertices = VerticesBuilder::try_new(&vertex_info, &prefix, 0).unwrap();
        let mut first = Vertex::new();
        first.add_property_string("key", "entity-0");
        first.add_property_string("note", "present");
        vertices.add_vertex(first).unwrap();
        let mut second = Vertex::new();
        second.add_property_string("key", "entity-1");
        vertices.add_vertex(second).unwrap();
        let mut third = Vertex::new();
        third.add_property_string("key", "entity-2");
        third.add_property_string("note", "");
        vertices.add_vertex(third).unwrap();
        vertices.dump().unwrap();

        let mut edges =
            EdgesBuilder::try_new(&edge_info, &prefix, AdjListType::UnorderedBySource, 2).unwrap();
        let mut edge = Edge::new(0, 1);
        edge.add_property_string("key", "edge-0");
        edge.add_property_string("note", "evidence");
        edges.add_edge(edge).unwrap();
        edges.dump().unwrap();

        let properties = vec!["key".to_string(), "note".to_string()];
        let vertex_batch = read_vertex_string_batch(&graph_info, "entity", &properties, 3).unwrap();
        assert_eq!(vertex_batch.row_count(), 3);
        assert_eq!(vertex_batch.column_count(), 2);
        assert_eq!(vertex_batch.id(2), Some(2));
        assert_eq!(vertex_batch.value(0, 1), Some("present"));
        assert_eq!(vertex_batch.value(1, 1), None);
        assert_eq!(vertex_batch.value(2, 1), Some(""));

        let read_vertices = read_vertex_strings(&graph_info, "entity", &properties, 3).unwrap();
        assert_eq!(read_vertices.len(), 3);
        assert_eq!(read_vertices[0].id(), 0);
        assert_eq!(
            read_vertices[0].values(),
            &[Some("entity-0".to_string()), Some("present".to_string())]
        );
        assert_eq!(
            read_vertices[1].values(),
            &[Some("entity-1".to_string()), None]
        );
        assert_eq!(
            read_vertices[2].values(),
            &[Some("entity-2".to_string()), Some(String::new())]
        );

        let edge_batch = read_edge_string_batch(
            &graph_info,
            "entity",
            "relates",
            "entity",
            AdjListType::UnorderedBySource,
            &properties,
            1,
        )
        .unwrap();
        assert_eq!(edge_batch.row_count(), 1);
        assert_eq!(edge_batch.column_count(), 2);
        assert_eq!(edge_batch.source(0), Some(0));
        assert_eq!(edge_batch.destination(0), Some(1));
        assert_eq!(edge_batch.value(0, 0), Some("edge-0"));
        assert_eq!(
            scan_edge_arrow_chunks(
                &graph_info,
                "entity",
                "relates",
                "entity",
                AdjListType::UnorderedBySource,
                &properties,
                1,
            )
            .unwrap(),
            1
        );
        let arrow_batches = read_edge_arrow_batches(
            &graph_info,
            "entity",
            "relates",
            "entity",
            AdjListType::UnorderedBySource,
            &properties,
            1,
        )
        .unwrap()
        .collect::<Result<Vec<_>, _>>()
        .unwrap();
        assert_eq!(
            arrow_batches
                .iter()
                .map(|batch| batch.num_rows())
                .sum::<usize>(),
            1
        );
        assert_eq!(arrow_batches[0].num_columns(), 4);

        let read_edges = read_edge_strings(
            &graph_info,
            "entity",
            "relates",
            "entity",
            AdjListType::UnorderedBySource,
            &properties,
            1,
        )
        .unwrap();
        assert_eq!(read_edges.len(), 1);
        assert_eq!(
            (read_edges[0].source(), read_edges[0].destination()),
            (0, 1)
        );
        assert_eq!(
            read_edges[0].values(),
            &[Some("edge-0".to_string()), Some("evidence".to_string())]
        );

        let error = read_vertex_strings(&graph_info, "entity", &properties, 1).unwrap_err();
        assert!(error.to_string().contains("exceeding max_rows=1"));
    }

    #[test]
    fn edge_iteration_crosses_exact_chunk_boundaries() {
        let directory = tempdir().unwrap();
        let prefix = format!("{}/", directory.path().display());
        let edge_info = EdgeInfo::builder("entity", "relates", "entity", 4, 4, 4)
            .directed(true)
            .push_adjacent_list(AdjacentList::new(
                AdjListType::UnorderedBySource,
                FileType::Csv,
                Some("unordered_by_source/"),
            ))
            .property_groups(string_properties())
            .prefix("edge/entity_relates_entity/")
            .try_build()
            .unwrap();
        let graph_info = GraphInfo::try_new(
            "exact-boundary",
            vec![],
            vec![edge_info.clone()],
            vec![],
            &prefix,
            None,
        )
        .unwrap();
        let mut edges =
            EdgesBuilder::try_new(&edge_info, &prefix, AdjListType::UnorderedBySource, 8).unwrap();
        for source in 0..8 {
            let mut edge = Edge::new(source, (source + 1) % 8);
            edge.add_property_string("key", format!("edge-{source}"));
            edges.add_edge(edge).unwrap();
        }
        edges.dump().unwrap();

        let read_edges = read_edge_strings(
            &graph_info,
            "entity",
            "relates",
            "entity",
            AdjListType::UnorderedBySource,
            &["key".to_string()],
            8,
        )
        .unwrap();

        assert_eq!(read_edges.len(), 8);
        assert_eq!(read_edges[0].source(), 0);
        assert_eq!(read_edges[4].source(), 4);
        assert_eq!(read_edges[7].source(), 7);
    }
}
