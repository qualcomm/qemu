// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: GPL-2.0-or-later

//! Qualcomm QUP GENI UART Device Model
//!
//! This library implements a device model for the Qualcomm QUP GENI UART
//! device in QEMU. The device implements a UART interface using the
//! Generic Interface (GENI) Serial Engine framework.
//!
//! # Features
//! - 256-byte FIFO depth for both TX and RX
//! - M interrupt registers for UART mode
//! - Simplified clock handling
//! - Loopback mode support
//! - Compatible with existing Linux drivers

mod device;
mod registers;

pub use device::qup_geni_uart_create;

pub const TYPE_QUP_GENI_UART: &::std::ffi::CStr = c"qup-geni-uart";
