// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_PROCESS_INFO_H_
#define BASE_PROCESS_PROCESS_INFO_H_

#include "base/base_export.h"
#include "build/build_config.h"

namespace base {

#if BUILDFLAG(IS_WIN)
enum IntegrityLevel {
  INTEGRITY_UNKNOWN,
  UNTRUSTED_INTEGRITY,
  LOW_INTEGRITY,
  MEDIUM_INTEGRITY,
  HIGH_INTEGRITY,
};

// Returns the integrity level of the process. Returns INTEGRITY_UNKNOWN in the
// case of an underlying system failure.
BASE_EXPORT IntegrityLevel GetCurrentProcessIntegrityLevel();

// Determines whether the current process is elevated. Note: in some
// configurations this may be true for processes launched without using
// base::LaunchElevatedProcess().
BASE_EXPORT bool IsCurrentProcessElevated();

// Determines whether the current process is running within an App Container.
BASE_EXPORT bool IsCurrentProcessInAppContainer();

#endif  // BUILDFLAG(IS_WIN)

}  // namespace base

#endif  // BASE_PROCESS_PROCESS_INFO_H_
