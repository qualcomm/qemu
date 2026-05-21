// SPDX-License-Identifier: BSD-3-Clause-Clear
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// Library interface for the QEMU PGO profile converter.

pub mod elf_index;
pub mod native;

mod autofdo;
mod bolt;
mod cgprof;
mod gcc_autofdo;
mod temporal;

pub use autofdo::emit as emit_autofdo;
pub use bolt::emit as emit_bolt;
pub use cgprof::emit as emit_cgprof;
pub use gcc_autofdo::emit as emit_gcc_autofdo;
pub use temporal::emit as emit_temporal;
