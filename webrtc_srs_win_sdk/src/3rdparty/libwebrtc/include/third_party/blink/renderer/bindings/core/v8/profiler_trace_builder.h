// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PROFILER_TRACE_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PROFILER_TRACE_BUILDER_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_marker.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "v8/include/v8-profiler.h"

namespace blink {

class ProfilerFrame;
class ProfilerSample;
class ProfilerStack;
class ProfilerTrace;
class ScriptState;

// A hash uniquely identified by the substack associated with the node.
struct ProfilerNodeStackHash {
  STATIC_ONLY(ProfilerNodeStackHash);

  static bool Equal(const v8::CpuProfileNode* a, const v8::CpuProfileNode* b) {
    return a->GetNodeId() == b->GetNodeId();
  }

  static unsigned GetHash(const v8::CpuProfileNode* node) {
    return node->GetNodeId();
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

// A hash uniquely identified by the stack frame associated with the node.
struct ProfilerNodeFrameHash {
  STATIC_ONLY(ProfilerNodeFrameHash);

  static bool Equal(const v8::CpuProfileNode* a, const v8::CpuProfileNode* b) {
    return a->GetFunctionName() == b->GetFunctionName() &&
           a->GetScriptResourceName() == b->GetScriptResourceName() &&
           a->GetLineNumber() == b->GetLineNumber() &&
           a->GetColumnNumber() == b->GetColumnNumber();
  }

  static unsigned GetHash(const v8::CpuProfileNode* node) {
    return StringHash::GetHash(node->GetFunctionNameStr()) ^
           StringHash::GetHash(node->GetScriptResourceNameStr()) ^
           DefaultHash<unsigned>::Hash().GetHash(node->GetLineNumber()) ^
           DefaultHash<unsigned>::Hash().GetHash(node->GetColumnNumber());
  }

  static const bool safe_to_compare_to_empty_or_deleted = false;
};

// Produces a structurally compressed trace from a v8::CpuProfile relative to a
// time origin, and omits frames from cross-origin scripts that do not
// participate in CORS.
//
// The trace format is described at:
// https://wicg.github.io/js-self-profiling/#the-profilertrace-dictionary
class CORE_EXPORT ProfilerTraceBuilder final
    : public GarbageCollected<ProfilerTraceBuilder> {
 public:
  static ProfilerTrace* FromProfile(ScriptState*,
                                    const v8::CpuProfile* profile,
                                    const SecurityOrigin* allowed_origin,
                                    base::TimeTicks time_origin);

  explicit ProfilerTraceBuilder(ScriptState*,
                                const SecurityOrigin* allowed_origin,
                                base::TimeTicks time_origin);

  ProfilerTraceBuilder(const ProfilerTraceBuilder&) = delete;
  ProfilerTraceBuilder& operator=(const ProfilerTraceBuilder&) = delete;

  void Trace(Visitor*) const;

 private:
  // Adds a stack sample from V8 to the trace, performing necessary filtering
  // and coalescing.
  void AddSample(const v8::CpuProfileNode* node,
                 base::TimeTicks timestamp,
                 const v8::StateTag);
  // Obtains the stack ID of the substack with the given node as its leaf,
  // performing origin-based filtering.
  absl::optional<wtf_size_t> GetOrInsertStackId(const v8::CpuProfileNode* node);
  // Obtains the frame ID of the stack frame represented by the given node.
  wtf_size_t GetOrInsertFrameId(const v8::CpuProfileNode* node);
  // Obtains the resource ID for the given resource name.
  wtf_size_t GetOrInsertResourceId(const char* resource_name);

  ProfilerTrace* GetTrace() const;

  inline absl::optional<V8ProfilerMarker> VMStateToMarker(v8::StateTag state) {
    switch (state) {
      case v8::GC:
        return V8ProfilerMarker(V8ProfilerMarker::Enum::kGc);
      case v8::JS:
      case v8::ATOMICS_WAIT:
        return V8ProfilerMarker(V8ProfilerMarker::Enum::kScript);
      default:
        return absl::optional<V8ProfilerMarker>();
    }
  }

  // Discards metadata frames and performs an origin check on the given stack
  // frame, returning true if it either has the same origin as the profiler, or
  // if it should be shared cross origin.
  bool ShouldIncludeStackFrame(const v8::CpuProfileNode* node);

  Member<ScriptState> script_state_;

  const SecurityOrigin* allowed_origin_;
  const base::TimeTicks time_origin_;

  Vector<String> resources_;
  HeapVector<Member<ProfilerFrame>> frames_;
  HeapVector<Member<ProfilerStack>> stacks_;
  HeapVector<Member<ProfilerSample>> samples_;

  // Maps V8-managed resource strings to their indices in the resources table.
  HashMap<const char*, wtf_size_t> resource_map_;
  HashMap<const v8::CpuProfileNode*, wtf_size_t, ProfilerNodeStackHash>
      node_to_stack_map_;
  HashMap<const v8::CpuProfileNode*, wtf_size_t, ProfilerNodeFrameHash>
      node_to_frame_map_;

  // A mapping from a V8 internal script ID to whether or not it passes the
  // same-origin policy for the ScriptState that the trace belongs to.
  HashMap<int, bool> script_same_origin_cache_;

  FRIEND_TEST_ALL_PREFIXES(ProfilerTraceBuilderTest, AddVMStateMarker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_PROFILER_TRACE_BUILDER_H_
