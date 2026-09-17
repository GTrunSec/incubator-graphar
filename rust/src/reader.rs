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

fn decode_values(values: Vec<String>, valid: Vec<bool>) -> Vec<Option<String>> {
    debug_assert_eq!(values.len(), valid.len());
    values
        .into_iter()
        .zip(valid)
        .map(|(value, valid)| valid.then_some(value))
        .collect()
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
    let_cxx_string!(vertex_type = vertex_type.as_ref());
    let properties = properties.to_vec();
    ffi::graphar::read_vertex_string_records(&graph_info.0, &vertex_type, &properties, max_rows)
        .map(|records| {
            records
                .into_iter()
                .map(|record| VertexStringRecord {
                    id: record.id,
                    values: decode_values(record.values, record.valid),
                })
                .collect()
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
    let_cxx_string!(src_type = src_type.as_ref());
    let_cxx_string!(edge_type = edge_type.as_ref());
    let_cxx_string!(dst_type = dst_type.as_ref());
    let properties = properties.to_vec();
    ffi::graphar::read_edge_string_records(
        &graph_info.0,
        &src_type,
        &edge_type,
        &dst_type,
        adjacency,
        &properties,
        max_rows,
    )
    .map(|records| {
        records
            .into_iter()
            .map(|record| EdgeStringRecord {
                source: record.source,
                destination: record.destination,
                values: decode_values(record.values, record.valid),
            })
            .collect()
    })
    .map_err(Into::into)
}

#[cfg(test)]
mod tests {
    use super::{decode_values, read_edge_strings, read_vertex_strings};
    use crate::builder::{Edge, EdgesBuilder, Vertex, VerticesBuilder};
    use crate::info::{AdjListType, AdjacentList, EdgeInfo, GraphInfo, VertexInfo};
    use crate::property::{Property, PropertyGroup, PropertyGroupVector, PropertyVec};
    use crate::types::{Cardinality, DataType, FileType};
    use tempfile::tempdir;

    #[test]
    fn nullable_values_preserve_property_order() {
        assert_eq!(
            decode_values(
                vec!["first".to_string(), String::new(), "third".to_string()],
                vec![true, false, true],
            ),
            vec![Some("first".to_string()), None, Some("third".to_string())]
        );
    }

    fn string_properties() -> PropertyGroupVector {
        let mut properties = PropertyVec::new();
        properties.push(Property::new(
            "key",
            DataType::string(),
            true,
            false,
            Cardinality::Single,
        ));
        properties.push(Property::new(
            "note",
            DataType::string(),
            false,
            true,
            Cardinality::Single,
        ));
        let mut groups = PropertyGroupVector::new();
        groups.push(PropertyGroup::new(properties, FileType::Csv, "property/"));
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
        vertices.dump().unwrap();

        let mut edges =
            EdgesBuilder::try_new(&edge_info, &prefix, AdjListType::UnorderedBySource, 2).unwrap();
        let mut edge = Edge::new(0, 1);
        edge.add_property_string("key", "edge-0");
        edge.add_property_string("note", "evidence");
        edges.add_edge(edge).unwrap();
        edges.dump().unwrap();

        let properties = vec!["key".to_string(), "note".to_string()];
        let read_vertices = read_vertex_strings(&graph_info, "entity", &properties, 2).unwrap();
        assert_eq!(read_vertices.len(), 2);
        assert_eq!(read_vertices[0].id(), 0);
        assert_eq!(
            read_vertices[0].values(),
            &[Some("entity-0".to_string()), Some("present".to_string())]
        );
        assert_eq!(
            read_vertices[1].values(),
            &[Some("entity-1".to_string()), None]
        );

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
}
