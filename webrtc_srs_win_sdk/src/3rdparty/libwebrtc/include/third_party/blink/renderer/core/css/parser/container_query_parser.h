// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CONTAINER_QUERY_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CONTAINER_QUERY_PARSER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"

namespace blink {

class CSSParserContext;

class CORE_EXPORT ContainerQueryParser {
  STACK_ALLOCATED();

 public:
  explicit ContainerQueryParser(const CSSParserContext&);

  absl::optional<ContainerSelector> ConsumeSelector(CSSParserTokenRange&);

  // https://drafts.csswg.org/css-contain-3/#typedef-container-query
  std::unique_ptr<MediaQueryExpNode> ParseQuery(String);
  std::unique_ptr<MediaQueryExpNode> ParseQuery(CSSParserTokenRange);

 private:
  using FeatureSet = MediaQueryParser::FeatureSet;

  std::unique_ptr<MediaQueryExpNode> ConsumeContainerQuery(
      CSSParserTokenRange&);
  std::unique_ptr<MediaQueryExpNode> ConsumeContainerCondition(
      CSSParserTokenRange&);
  std::unique_ptr<MediaQueryExpNode> ConsumeFeatureQuery(CSSParserTokenRange&,
                                                         const FeatureSet&);
  std::unique_ptr<MediaQueryExpNode> ConsumeFeatureQueryInParens(
      CSSParserTokenRange&,
      const FeatureSet&);
  std::unique_ptr<MediaQueryExpNode> ConsumeFeatureCondition(
      CSSParserTokenRange&,
      const FeatureSet&);
  std::unique_ptr<MediaQueryExpNode> ConsumeFeature(CSSParserTokenRange&,
                                                    const FeatureSet&);

  const CSSParserContext& context_;
  MediaQueryParser media_query_parser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CONTAINER_QUERY_PARSER_H_
