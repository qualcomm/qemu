// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Edge storage implementations: Top-N bounded heap and streaming
// HashMap. Selected via plugin argument.

use rustc_hash::FxHashMap;

/// A branch edge between two addresses.
#[derive(Hash, Eq, PartialEq, Clone, Copy, Debug)]
pub struct Edge {
    pub from: u64,
    pub to: u64,
}

/// Per-vCPU edge accumulator. Supports two modes:
/// - Top-N: bounded-capacity storage that keeps the hottest edges
/// - Streaming: unbounded HashMap, flushed periodically
pub struct EdgeAccumulator {
    storage: EdgeStorage,
    pub total_tb_execs: u64,
}

enum EdgeStorage {
    TopN(TopNTable),
    Streaming(StreamingMap),
}

impl EdgeAccumulator {
    pub fn new(capacity: usize, streaming: bool) -> Self {
        let storage = if streaming {
            EdgeStorage::Streaming(StreamingMap::new())
        } else {
            EdgeStorage::TopN(TopNTable::new(capacity))
        };
        Self {
            storage,
            total_tb_execs: 0,
        }
    }

    /// Record a branch edge.
    pub fn record(&mut self, from: u64, to: u64) {
        match &mut self.storage {
            EdgeStorage::TopN(table) => table.record(from, to),
            EdgeStorage::Streaming(map) => map.record(from, to),
        }
    }

    /// Drain all edges into a Vec for output.
    pub fn drain(&mut self) -> Vec<(u64, u64, u64)> {
        match &mut self.storage {
            EdgeStorage::TopN(table) => table.drain(),
            EdgeStorage::Streaming(map) => map.drain(),
        }
    }

    /// Number of unique edges currently stored.
    pub fn len(&self) -> usize {
        match &self.storage {
            EdgeStorage::TopN(table) => table.edges.len(),
            EdgeStorage::Streaming(map) => map.edges.len(),
        }
    }

    /// Whether streaming mode should trigger a flush.
    pub fn should_flush(&self, threshold: usize) -> bool {
        match &self.storage {
            EdgeStorage::TopN(_) => false,
            EdgeStorage::Streaming(map) => map.edges.len() >= threshold,
        }
    }
}

/// Top-N table: bounded HashMap that keeps the N hottest edges.
///
/// Implementation: a HashMap with periodic pruning. When the map
/// exceeds 2*N entries, we sort by count and keep only the top N.
/// This amortizes the sort cost across many insertions.
struct TopNTable {
    edges: FxHashMap<Edge, u64>,
    capacity: usize,
}

impl TopNTable {
    fn new(capacity: usize) -> Self {
        Self {
            edges: FxHashMap::with_capacity_and_hasher(capacity, Default::default()),
            capacity,
        }
    }

    fn record(&mut self, from: u64, to: u64) {
        let edge = Edge { from, to };
        *self.edges.entry(edge).or_insert(0) += 1;

        // Prune when we exceed 2x capacity
        if self.edges.len() > self.capacity * 2 {
            self.prune();
        }
    }

    fn prune(&mut self) {
        if self.edges.len() <= self.capacity {
            return;
        }
        let mut entries: Vec<(Edge, u64)> = self.edges.drain().collect();
        entries.sort_unstable_by(|a, b| b.1.cmp(&a.1));
        entries.truncate(self.capacity);
        self.edges = entries.into_iter().collect();
    }

    fn drain(&mut self) -> Vec<(u64, u64, u64)> {
        self.edges.drain().map(|(e, c)| (e.from, e.to, c)).collect()
    }
}

/// Streaming map: unbounded HashMap that is flushed periodically.
struct StreamingMap {
    edges: FxHashMap<Edge, u64>,
}

impl StreamingMap {
    fn new() -> Self {
        Self {
            edges: FxHashMap::default(),
        }
    }

    fn record(&mut self, from: u64, to: u64) {
        let edge = Edge { from, to };
        *self.edges.entry(edge).or_insert(0) += 1;
    }

    fn drain(&mut self) -> Vec<(u64, u64, u64)> {
        self.edges.drain().map(|(e, c)| (e.from, e.to, c)).collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn top_n_basic_recording() {
        let mut acc = EdgeAccumulator::new(100, false);
        acc.record(0x1000, 0x2000);
        acc.record(0x1000, 0x2000);
        acc.record(0x3000, 0x4000);
        assert_eq!(acc.len(), 2);

        let edges = acc.drain();
        assert_eq!(edges.len(), 2);

        let e1 = edges
            .iter()
            .find(|e| e.0 == 0x1000 && e.1 == 0x2000)
            .unwrap();
        assert_eq!(e1.2, 2);

        let e2 = edges
            .iter()
            .find(|e| e.0 == 0x3000 && e.1 == 0x4000)
            .unwrap();
        assert_eq!(e2.2, 1);
    }

    #[test]
    fn top_n_pruning() {
        // Capacity=2, prune threshold=4 (2*capacity)
        let mut acc = EdgeAccumulator::new(2, false);

        // Record 3 different edges: a(10), b(5), c(1)
        for _ in 0..10 {
            acc.record(0xA, 0xB);
        }
        for _ in 0..5 {
            acc.record(0xC, 0xD);
        }
        for _ in 0..1 {
            acc.record(0xE, 0xF);
        }
        // 3 edges < 2*2=4, no pruning yet
        assert_eq!(acc.len(), 3);

        // Add 2 more cold edges to trigger pruning at len > 4
        acc.record(0x10, 0x11);
        acc.record(0x12, 0x13);
        // Now 5 > 4, pruning keeps top 2

        assert!(acc.len() <= 2);
        let edges = acc.drain();
        // The two hottest edges should survive
        let counts: Vec<u64> = edges.iter().map(|e| e.2).collect();
        assert!(counts.contains(&10));
        assert!(counts.contains(&5));
    }

    #[test]
    fn streaming_basic() {
        let mut acc = EdgeAccumulator::new(100, true);
        acc.record(0x1000, 0x2000);
        acc.record(0x1000, 0x2000);
        acc.record(0x3000, 0x4000);
        assert_eq!(acc.len(), 2);
        assert!(!acc.should_flush(10));
        assert!(acc.should_flush(2));

        let edges = acc.drain();
        assert_eq!(edges.len(), 2);
        assert_eq!(acc.len(), 0);
    }

    #[test]
    fn drain_empties_accumulator() {
        let mut acc = EdgeAccumulator::new(100, false);
        acc.record(0x1, 0x2);
        assert_eq!(acc.len(), 1);
        let _ = acc.drain();
        assert_eq!(acc.len(), 0);
    }

    #[test]
    fn edge_hash_distinguishes_direction() {
        let mut acc = EdgeAccumulator::new(100, false);
        acc.record(0x1000, 0x2000);
        acc.record(0x2000, 0x1000);
        assert_eq!(acc.len(), 2);
    }
}
