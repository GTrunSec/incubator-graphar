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

//! GraphAr edge writer builders.

use std::pin::Pin;

use cxx::{UniquePtr, let_cxx_string};

use crate::{ffi, info::AdjListType, info::EdgeInfo};

/// An edge record being constructed for use with [`EdgesBuilder`].
///
/// Source and destination are GraphAr-internal vertex IDs, not application
/// identities. Applications should persist their own stable IDs as properties.
pub struct Edge(pub(crate) UniquePtr<ffi::graphar::BuilderEdge>);

impl Edge {
    /// Create an edge from non-negative GraphAr vertex IDs.
    ///
    /// # Errors
    ///
    /// Returns an error when either internal vertex ID is negative.
    pub fn try_new(source: i64, destination: i64) -> crate::Result<Self> {
        if source < 0 {
            return Err(crate::Error::InvalidArgument {
                name: "source",
                reason: "source must be >= 0".to_string(),
            });
        }
        if destination < 0 {
            return Err(crate::Error::InvalidArgument {
                name: "destination",
                reason: "destination must be >= 0".to_string(),
            });
        }
        Ok(Self(ffi::graphar::new_edge_builder(source, destination)?))
    }

    /// Create an edge from GraphAr vertex IDs.
    ///
    /// # Panics
    ///
    /// Panics if either ID is negative. Prefer [`Edge::try_new`] for untrusted input.
    pub fn new(source: i64, destination: i64) -> Self {
        Self::try_new(source, destination).unwrap()
    }

    /// Return the GraphAr-internal source vertex ID.
    pub fn source(&self) -> i64 {
        ffi::graphar::edge_builder_source(self.as_ref())
    }

    /// Return the GraphAr-internal destination vertex ID.
    pub fn destination(&self) -> i64 {
        ffi::graphar::edge_builder_destination(self.as_ref())
    }

    /// Return true when this edge has no properties.
    pub fn is_empty(&self) -> bool {
        ffi::graphar::edge_builder_is_empty(self.as_ref())
    }

    /// Return true when this edge contains `name`.
    pub fn contains_property<S: AsRef<str>>(&self, name: S) -> bool {
        let_cxx_string!(name = name.as_ref());
        ffi::graphar::edge_builder_contains_property(self.as_ref(), &name)
    }

    /// Add a boolean property.
    pub fn add_property_bool<S: AsRef<str>>(&mut self, name: S, value: bool) {
        let_cxx_string!(name = name.as_ref());
        ffi::graphar::edge_builder_add_property_bool(self.pin_mut(), &name, value);
    }

    /// Add a signed 32-bit integer property.
    pub fn add_property_i32<S: AsRef<str>>(&mut self, name: S, value: i32) {
        let_cxx_string!(name = name.as_ref());
        ffi::graphar::edge_builder_add_property_i32(self.pin_mut(), &name, value);
    }

    /// Add a signed 64-bit integer property.
    pub fn add_property_i64<S: AsRef<str>>(&mut self, name: S, value: i64) {
        let_cxx_string!(name = name.as_ref());
        ffi::graphar::edge_builder_add_property_i64(self.pin_mut(), &name, value);
    }

    /// Add a 32-bit floating-point property.
    pub fn add_property_f32<S: AsRef<str>>(&mut self, name: S, value: f32) {
        let_cxx_string!(name = name.as_ref());
        ffi::graphar::edge_builder_add_property_f32(self.pin_mut(), &name, value);
    }

    /// Add a 64-bit floating-point property.
    pub fn add_property_f64<S: AsRef<str>>(&mut self, name: S, value: f64) {
        let_cxx_string!(name = name.as_ref());
        ffi::graphar::edge_builder_add_property_f64(self.pin_mut(), &name, value);
    }

    /// Add a UTF-8 string property.
    pub fn add_property_string<S: AsRef<str>, V: AsRef<str>>(&mut self, name: S, value: V) {
        let_cxx_string!(name = name.as_ref());
        let_cxx_string!(value = value.as_ref());
        ffi::graphar::edge_builder_add_property_string(self.pin_mut(), &name, &value);
    }

    fn as_ref(&self) -> &ffi::graphar::BuilderEdge {
        self.0.as_ref().expect("edge should be valid")
    }

    fn pin_mut(&mut self) -> Pin<&mut ffi::graphar::BuilderEdge> {
        self.0.as_mut().expect("edge should be valid")
    }
}

/// A high-level builder for writing a collection of edges.
pub struct EdgesBuilder(pub(crate) UniquePtr<ffi::graphar::EdgesBuilder>);

impl EdgesBuilder {
    /// Create an edge builder backed by upstream GraphAr.
    ///
    /// The `prefix` must end with `/`, and `num_vertices` is the number of
    /// vertices for the selected source- or destination-oriented adjacency.
    ///
    /// # Errors
    ///
    /// Returns an error for an invalid prefix, negative vertex count, missing
    /// adjacency-list type, or an error from upstream GraphAr. Added edges are
    /// checked with GraphAr's strong schema validation.
    pub fn try_new<P: AsRef<str>>(
        edge_info: &EdgeInfo,
        prefix: P,
        adjacency: AdjListType,
        num_vertices: i64,
    ) -> crate::Result<Self> {
        let prefix = prefix.as_ref();
        if !prefix.ends_with('/') {
            return Err(crate::Error::InvalidArgument {
                name: "prefix",
                reason: "prefix must end with '/'".to_string(),
            });
        }
        if num_vertices < 0 {
            return Err(crate::Error::InvalidArgument {
                name: "num_vertices",
                reason: "num_vertices must be >= 0".to_string(),
            });
        }
        let_cxx_string!(prefix = prefix);
        let inner =
            ffi::graphar::new_edges_builder(&edge_info.0, &prefix, adjacency, num_vertices)?;
        Ok(Self(inner))
    }

    /// Create an edge builder backed by upstream GraphAr.
    ///
    /// # Panics
    ///
    /// Panics when an argument or the edge metadata is rejected. Prefer
    /// [`EdgesBuilder::try_new`] for untrusted input.
    pub fn new<P: AsRef<str>>(
        edge_info: &EdgeInfo,
        prefix: P,
        adjacency: AdjListType,
        num_vertices: i64,
    ) -> Self {
        Self::try_new(edge_info, prefix, adjacency, num_vertices).unwrap()
    }

    /// Add an edge to the collection.
    pub fn add_edge(&mut self, mut edge: Edge) -> crate::Result<()> {
        ffi::graphar::add_edge(self.pin_mut(), edge.pin_mut())?;
        Ok(())
    }

    /// Return the number of buffered edges.
    pub fn len(&self) -> i64 {
        ffi::graphar::edges_len(self.as_ref())
    }

    /// Return true when no edges are buffered.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Remove all buffered edges.
    pub fn clear(&mut self) {
        ffi::graphar::edges_clear(self.pin_mut());
    }

    /// Dump all currently buffered edges into GraphAr chunk files.
    pub fn dump(&mut self) -> crate::Result<()> {
        ffi::graphar::edges_dump(self.pin_mut())?;
        Ok(())
    }

    fn as_ref(&self) -> &ffi::graphar::EdgesBuilder {
        self.0.as_ref().expect("edges builder should be valid")
    }

    fn pin_mut(&mut self) -> Pin<&mut ffi::graphar::EdgesBuilder> {
        self.0.as_mut().expect("edges builder should be valid")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    use crate::info::{AdjacentList, InfoVersion};
    use crate::property::{Property, PropertyGroup, PropertyGroupVector, PropertyVec};
    use crate::types::{Cardinality, DataType, FileType};
    use tempfile::tempdir;

    fn make_edge_info() -> EdgeInfo {
        let mut properties = PropertyVec::new();
        properties.push(Property::new(
            "fact_id",
            DataType::string(),
            true,
            false,
            Cardinality::Single,
        ));
        let mut groups = PropertyGroupVector::new();
        groups.push(PropertyGroup::new(properties, FileType::Csv, "property/"));

        EdgeInfo::builder("entity", "knows", "entity", 4, 4, 4)
            .directed(true)
            .push_adjacent_list(AdjacentList::new(
                AdjListType::UnorderedBySource,
                FileType::Csv,
                Some("unordered_by_source/"),
            ))
            .property_groups(groups)
            .prefix("edge/entity_knows_entity/")
            .version(InfoVersion::new(1).unwrap())
            .try_build()
            .unwrap()
    }

    #[test]
    fn edge_properties_and_endpoints_are_exposed() {
        let mut edge = Edge::new(1, 2);
        assert_eq!(edge.source(), 1);
        assert_eq!(edge.destination(), 2);
        assert!(edge.is_empty());
        edge.add_property_string("fact_id", "fact-1");
        assert!(!edge.is_empty());
        assert!(edge.contains_property("fact_id"));
    }

    #[test]
    fn negative_internal_ids_are_rejected() {
        assert!(Edge::try_new(-1, 0).is_err());
        assert!(Edge::try_new(0, -1).is_err());
    }

    #[test]
    fn edges_builder_writes_with_upstream_graphar() {
        let directory = tempdir().unwrap();
        let prefix = format!("{}/", directory.path().display());
        let mut builder =
            EdgesBuilder::try_new(&make_edge_info(), prefix, AdjListType::UnorderedBySource, 3)
                .unwrap();

        let mut edge = Edge::new(0, 1);
        edge.add_property_string("fact_id", "fact-1");
        builder.add_edge(edge).unwrap();
        assert_eq!(builder.len(), 1);
        builder.dump().unwrap();
    }

    #[test]
    fn builder_rejects_missing_adjacency_type() {
        let error =
            EdgesBuilder::try_new(&make_edge_info(), "/tmp/", AdjListType::OrderedByDest, 3)
                .err()
                .unwrap();
        assert!(error.to_string().contains("adjacency list type"));
    }

    #[test]
    fn builder_strongly_validates_edge_properties() {
        let directory = tempdir().unwrap();
        let prefix = format!("{}/", directory.path().display());
        let mut builder =
            EdgesBuilder::try_new(&make_edge_info(), prefix, AdjListType::UnorderedBySource, 3)
                .unwrap();
        let mut edge = Edge::new(0, 1);
        edge.add_property_string("unknown", "value");
        assert!(builder.add_edge(edge).is_err());
        assert!(builder.is_empty());
    }
}
