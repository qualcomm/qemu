// Copyright 2024, Linaro Limited
// Author(s): Manos Pitsidianakis <manos.pitsidianakis@linaro.org>
// SPDX-License-Identifier: GPL-2.0-or-later

//! PL011 QEMU Device Model
//!
//! This library implements a device model for the PrimeCell® UART (PL011)
//! device in QEMU.
//!
//! # Library crate
//!
//! See [`PL011State`](crate::device::PL011State) for the device model type and
//! the [`registers`] module for register types.

// #[cfg(feature="gen8-2-1")]
pub mod adreno840v2;
pub use adreno840v2::*;
