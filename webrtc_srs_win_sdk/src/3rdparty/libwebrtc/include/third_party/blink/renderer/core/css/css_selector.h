/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class CSSParserContext;
class CSSSelectorList;

// This class represents a simple selector for a StyleRule.

// CSS selector representation is somewhat complicated and subtle. A
// representative list of selectors is in CSSSelectorTest; run it in a debug
// build to see useful debugging output.
//
// ** TagHistory() and Relation():
//
// Selectors are represented as an array of simple selectors (defined more
// or less according to
// http://www.w3.org/TR/css3-selectors/#simple-selectors-dfn). The tagHistory()
// method returns the next simple selector in the list. The relation() method
// returns the relationship of the current simple selector to the one in
// tagHistory(). For example, the CSS selector .a.b #c is represented as:
//
// SelectorText(): .a.b #c
// --> (relation == kDescendant)
//   SelectorText(): .a.b
//   --> (relation == kSubSelector)
//     SelectorText(): .b
//
// The order of tagHistory() varies depending on the situation.
// * Relations using combinators
//   (http://www.w3.org/TR/css3-selectors/#combinators), such as descendant,
//   sibling, etc., are parsed right-to-left (in the example above, this is why
//   #c is earlier in the tagHistory() chain than .a.b).
// * SubSelector relations are parsed left-to-right, such as the .a.b example
//   above.
// * ShadowPseudo relations are parsed right-to-left. Example:
//   summary::-webkit-details-marker is parsed as: selectorText():
//   summary::-webkit-details-marker --> (relation == ShadowPseudo)
//   selectorText(): summary
//
// ** match():
//
// The match of the current simple selector tells us the type of selector, such
// as class, id, tagname, or pseudo-class. Inline comments in the Match enum
// give examples of when each type would occur.
//
// ** value(), attribute():
//
// value() tells you the value of the simple selector. For example, for class
// selectors, value() will tell you the class string, and for id selectors it
// will tell you the id(). See below for the special case of attribute
// selectors.
//
// ** Attribute selectors.
//
// Attribute selectors return the attribute name in the attribute() method. The
// value() method returns the value matched against in case of selectors like
// [attr="value"].
//
class CORE_EXPORT CSSSelector {
  USING_FAST_MALLOC_WITH_TYPE_NAME(blink::CSSSelector);

 public:
  CSSSelector();
  CSSSelector(const CSSSelector&);
  explicit CSSSelector(const QualifiedName&, bool tag_is_implicit = false);

  ~CSSSelector();

  String SelectorText() const;

  bool operator==(const CSSSelector&) const;

  static constexpr unsigned kIdSpecificity = 0x010000;
  static constexpr unsigned kClassLikeSpecificity = 0x000100;
  static constexpr unsigned kTagSpecificity = 0x000001;

  // http://www.w3.org/TR/css3-selectors/#specificity
  // We use 256 as the base of the specificity number system.
  unsigned Specificity() const;

  /* how the attribute value has to match.... Default is Exact */
  enum MatchType {
    kUnknown,
    kTag,               // Example: div
    kId,                // Example: #id
    kClass,             // example: .class
    kPseudoClass,       // Example:  :nth-child(2)
    kPseudoElement,     // Example: ::first-line
    kPagePseudoClass,   // ??
    kAttributeExact,    // Example: E[foo="bar"]
    kAttributeSet,      // Example: E[foo]
    kAttributeHyphen,   // Example: E[foo|="bar"]
    kAttributeList,     // Example: E[foo~="bar"]
    kAttributeContain,  // css3: E[foo*="bar"]
    kAttributeBegin,    // css3: E[foo^="bar"]
    kAttributeEnd,      // css3: E[foo$="bar"]
    kFirstAttributeSelectorMatch = kAttributeExact,
  };

  enum RelationType {
    // No combinator. Used between simple selectors within the same compound.
    kSubSelector,
    // "Space" combinator
    kDescendant,
    // > combinator
    kChild,
    // + combinator
    kDirectAdjacent,
    // ~ combinator
    kIndirectAdjacent,
    // The relation types below are implicit combinators inserted at parse time
    // before pseudo elements which match another flat tree element than the
    // rest of the compound.
    //
    // Implicit combinator inserted before pseudo elements matching an element
    // inside a UA shadow tree. This combinator allows the selector matching to
    // cross a shadow root.
    //
    // Examples:
    // input::placeholder, video::cue(i), video::--webkit-media-controls-panel
    kUAShadow,
    // Implicit combinator inserted before ::slotted() selectors.
    kShadowSlot,
    // Implicit combinator inserted before ::part() selectors which allows
    // matching a ::part in shadow-including descendant tree for #host in
    // "#host::part(button)".
    kShadowPart,

    // leftmost "Space" combinator of relative selector
    kRelativeDescendant,
    // leftmost > combinator of relative selector
    kRelativeChild,
    // leftmost + combinator of relative selector
    kRelativeDirectAdjacent,
    // leftmost ~ combinator of relative selector
    kRelativeIndirectAdjacent
  };

  enum PseudoType {
    kPseudoUnknown,
    kPseudoEmpty,
    kPseudoFirstChild,
    kPseudoFirstOfType,
    kPseudoLastChild,
    kPseudoLastOfType,
    kPseudoOnlyChild,
    kPseudoOnlyOfType,
    kPseudoFirstLine,
    kPseudoFirstLetter,
    kPseudoNthChild,
    kPseudoNthOfType,
    kPseudoNthLastChild,
    kPseudoNthLastOfType,
    kPseudoPart,
    kPseudoState,
    kPseudoLink,
    kPseudoVisited,
    kPseudoAny,
    kPseudoIs,
    kPseudoWhere,
    kPseudoAnyLink,
    kPseudoWebkitAnyLink,
    kPseudoAutofill,
    kPseudoWebKitAutofill,
    kPseudoAutofillPreviewed,
    kPseudoAutofillSelected,
    kPseudoHover,
    kPseudoDrag,
    kPseudoFocus,
    kPseudoFocusVisible,
    kPseudoFocusWithin,
    kPseudoActive,
    kPseudoChecked,
    kPseudoEnabled,
    kPseudoFullPageMedia,
    kPseudoDefault,
    kPseudoDisabled,
    kPseudoOptional,
    kPseudoPlaceholderShown,
    kPseudoRequired,
    kPseudoReadOnly,
    kPseudoReadWrite,
    kPseudoValid,
    kPseudoInvalid,
    kPseudoIndeterminate,
    kPseudoTarget,
    kPseudoBefore,
    kPseudoAfter,
    kPseudoMarker,
    kPseudoModal,
    kPseudoBackdrop,
    kPseudoLang,
    kPseudoNot,
    kPseudoPlaceholder,
    kPseudoFileSelectorButton,
    kPseudoResizer,
    kPseudoRoot,
    kPseudoScope,
    kPseudoScrollbar,
    kPseudoScrollbarButton,
    kPseudoScrollbarCorner,
    kPseudoScrollbarThumb,
    kPseudoScrollbarTrack,
    kPseudoScrollbarTrackPiece,
    kPseudoWindowInactive,
    kPseudoCornerPresent,
    kPseudoDecrement,
    kPseudoIncrement,
    kPseudoHorizontal,
    kPseudoVertical,
    kPseudoStart,
    kPseudoEnd,
    kPseudoDoubleButton,
    kPseudoSingleButton,
    kPseudoNoButton,
    kPseudoSelection,
    kPseudoLeftPage,
    kPseudoRightPage,
    kPseudoFirstPage,
    // TODO(foolip): When the unprefixed Fullscreen API is enabled, merge
    // kPseudoFullScreen and kPseudoFullscreen into one. (kPseudoFullscreen is
    // controlled by the FullscreenUnprefixed REF, but is otherwise an alias.)
    kPseudoFullScreen,
    kPseudoFullScreenAncestor,
    kPseudoFullscreen,
    kPseudoPaused,
    kPseudoPictureInPicture,
    kPseudoPlaying,
    kPseudoInRange,
    kPseudoOutOfRange,
    kPseudoXrOverlay,
    // Pseudo elements in UA ShadowRoots. Available in any stylesheets.
    kPseudoWebKitCustomElement,
    // Pseudo elements in UA ShadowRoots. Available only in UA stylesheets.
    kPseudoBlinkInternalElement,
    kPseudoCue,
    kPseudoFutureCue,
    kPseudoPastCue,
    kPseudoDefined,
    kPseudoHasDatalist,
    kPseudoHost,
    kPseudoHostContext,
    kPseudoSpatialNavigationFocus,
    kPseudoSpatialNavigationInterest,
    kPseudoIsHtml,
    kPseudoListBox,
    kPseudoMultiSelectFocus,
    kPseudoHostHasAppearance,
    kPseudoPopupOpen,
    kPseudoSlotted,
    kPseudoVideoPersistent,
    kPseudoVideoPersistentAncestor,
    kPseudoTargetText,
    kPseudoDir,
    kPseudoHighlight,
    kPseudoSpellingError,
    kPseudoGrammarError,
    kPseudoHas,

    // TODO(blee@igalia.com) Need to clarify the :scope dependency in relative
    // selector definition.
    // - spec : https://www.w3.org/TR/selectors-4/#relative
    // - csswg issue : https://github.com/w3c/csswg-drafts/issues/6399
    kPseudoRelativeLeftmost,

    // The following selectors are used to target pseudo elements created for
    // DocumentTransition.
    // See
    // https://github.com/WICG/shared-element-transitions/blob/main/explainer.md
    // for details.
    kPseudoTransition,
    kPseudoTransitionContainer,
    kPseudoTransitionOldContent,
    kPseudoTransitionNewContent,
  };

  enum class AttributeMatchType {
    kCaseSensitive,
    kCaseInsensitive,
    kCaseSensitiveAlways,
  };

  PseudoType GetPseudoType() const {
    return static_cast<PseudoType>(pseudo_type_);
  }

  void UpdatePseudoType(const AtomicString&,
                        const CSSParserContext&,
                        bool has_arguments,
                        CSSParserMode);
  void UpdatePseudoPage(const AtomicString&);
  static PseudoType NameToPseudoType(const AtomicString&, bool has_arguments);
  static PseudoId GetPseudoId(PseudoType);

  // Selectors are kept in an array by CSSSelectorList. The next component of
  // the selector is the next item in the array.
  const CSSSelector* TagHistory() const {
    return is_last_in_tag_history_ ? nullptr : this + 1;
  }

  static const AtomicString& UniversalSelectorAtom() { return g_null_atom; }
  const QualifiedName& TagQName() const;
  const AtomicString& Value() const;
  const AtomicString& SerializingValue() const;

  // WARNING: Use of QualifiedName by attribute() is a lie.
  // attribute() will return a QualifiedName with prefix and namespaceURI
  // set to g_star_atom to mean "matches any namespace". Be very careful
  // how you use the returned QualifiedName.
  // http://www.w3.org/TR/css3-selectors/#attrnmsp
  const QualifiedName& Attribute() const;
  AttributeMatchType AttributeMatch() const;
  // Returns the argument of a parameterized selector. For example, :lang(en-US)
  // would have an argument of en-US.
  // Note that :nth-* selectors don't store an argument and just store the
  // numbers.
  const AtomicString& Argument() const {
    return has_rare_data_ ? data_.rare_data_->argument_ : g_null_atom;
  }
  const CSSSelectorList* SelectorList() const {
    return has_rare_data_ ? data_.rare_data_->selector_list_.get() : nullptr;
  }
  const Vector<AtomicString>* PartNames() const {
    return has_rare_data_ ? data_.rare_data_->part_names_.get() : nullptr;
  }
  bool ContainsPseudoInsideHasPseudoClass() const {
    return has_rare_data_ ? data_.rare_data_->bits_
                                .contains_pseudo_inside_has_pseudo_class_
                          : false;
  }

#ifndef NDEBUG
  void Show() const;
  void Show(int indent) const;
#endif

  bool IsASCIILower(const AtomicString& value);
  void SetValue(const AtomicString&, bool match_lower_case);
  void SetAttribute(const QualifiedName&, AttributeMatchType);
  void SetArgument(const AtomicString&);
  void SetSelectorList(std::unique_ptr<CSSSelectorList>);
  void SetPartNames(std::unique_ptr<Vector<AtomicString>>);
  void SetContainsPseudoInsideHasPseudoClass();

  void SetNth(int a, int b);
  bool MatchNth(unsigned count) const;

  bool IsAdjacentSelector() const {
    return relation_ == kDirectAdjacent || relation_ == kIndirectAdjacent;
  }
  bool IsUAShadowSelector() const { return relation_ == kUAShadow; }
  bool IsAttributeSelector() const {
    return match_ >= kFirstAttributeSelectorMatch;
  }
  bool IsHostPseudoClass() const {
    return pseudo_type_ == kPseudoHost || pseudo_type_ == kPseudoHostContext;
  }
  bool IsUserActionPseudoClass() const;
  bool IsIdClassOrAttributeSelector() const;

  RelationType Relation() const { return static_cast<RelationType>(relation_); }
  void SetRelation(RelationType relation) {
    relation_ = relation;
    DCHECK_EQ(static_cast<RelationType>(relation_),
              relation);  // using a bitfield.
  }

  MatchType Match() const { return static_cast<MatchType>(match_); }
  void SetMatch(MatchType match) {
    match_ = match;
    DCHECK_EQ(static_cast<MatchType>(match_), match);  // using a bitfield.
  }

  bool IsLastInSelectorList() const { return is_last_in_selector_list_; }
  void SetLastInSelectorList(bool is_last) {
    is_last_in_selector_list_ = is_last;
  }

  bool IsLastInOriginalList() const { return is_last_in_original_list_; }
  void SetLastInOriginalList(bool is_last) {
    is_last_in_original_list_ = is_last;
  }

  bool IsLastInTagHistory() const { return is_last_in_tag_history_; }
  void SetLastInTagHistory(bool is_last) { is_last_in_tag_history_ = is_last; }

  // https://drafts.csswg.org/selectors/#compound
  bool IsCompound() const;

  enum LinkMatchMask {
    kMatchLink = 1,
    kMatchVisited = 2,
    kMatchAll = kMatchLink | kMatchVisited
  };

  // True if :link or :visited pseudo-classes are found anywhere in
  // the selector.
  bool HasLinkOrVisited() const;

  bool IsForPage() const { return is_for_page_; }
  void SetForPage() { is_for_page_ = true; }

  bool MatchesPseudoElement() const;
  bool IsTreeAbidingPseudoElement() const;
  bool IsAllowedAfterPart() const;

  // Returns true if the immediately preceeding simple selector is ::part.
  bool FollowsPart() const;
  // Returns true if the immediately preceeding simple selector is ::slotted.
  bool FollowsSlotted() const;

  static String FormatPseudoTypeForDebugging(PseudoType);

 private:
  unsigned relation_ : 4;     // enum RelationType
  unsigned match_ : 4;        // enum MatchType
  unsigned pseudo_type_ : 8;  // enum PseudoType
  unsigned is_last_in_selector_list_ : 1;
  unsigned is_last_in_tag_history_ : 1;
  unsigned has_rare_data_ : 1;
  unsigned is_for_page_ : 1;
  unsigned tag_is_implicit_ : 1;
  unsigned is_last_in_original_list_ : 1;

  void SetPseudoType(PseudoType pseudo_type) {
    pseudo_type_ = pseudo_type;
    DCHECK_EQ(static_cast<PseudoType>(pseudo_type_),
              pseudo_type);  // using a bitfield.
  }

  unsigned SpecificityForOneSelector() const;
  unsigned SpecificityForPage() const;
  const CSSSelector* SerializeCompound(StringBuilder&) const;

  // Hide.
  CSSSelector& operator=(const CSSSelector&) = delete;

  struct RareData : public RefCounted<RareData> {
    static scoped_refptr<RareData> Create(const AtomicString& value) {
      return base::AdoptRef(new RareData(value));
    }
    ~RareData();

    bool MatchNth(unsigned count);
    int NthAValue() const { return bits_.nth_.a_; }
    int NthBValue() const { return bits_.nth_.b_; }

    AtomicString matching_value_;
    AtomicString serializing_value_;
    union {
      struct {
        int a_;  // Used for :nth-*
        int b_;  // Used for :nth-*
      } nth_;
      AttributeMatchType
          attribute_match_;  // used for attribute selector (with value)

      // Used for :has() with pseudos in its argument. e.g. :has(:hover)
      bool contains_pseudo_inside_has_pseudo_class_;
    } bits_;
    QualifiedName attribute_;  // used for attribute selector
    AtomicString argument_;    // Used for :contains, :lang, :nth-*
    std::unique_ptr<CSSSelectorList>
        selector_list_;  // Used for :-webkit-any and :not
    std::unique_ptr<Vector<AtomicString>>
        part_names_;  // Used for ::part() selectors.

   private:
    RareData(const AtomicString& value);
  };
  void CreateRareData();

  // The type tag for DataUnion is actually inferred from multiple state variables in the
  // containing CSSSelector using the following rules.
  //
  //  if (match_ == kTag) {
  //     /* data_.tag_q_name_ is valid */
  //  } else if (has_rare_data_) {
  //     /* data_.rare_data_ is valid */
  //  } else {
  //     /* data_.value_ is valid */
  //  }
  //
  // Note that it is important to placement-new and explicitly destruct the fields when
  // shifting between types tags for a DataUnion! Otherwise there will be undefined
  // behavior! This luckily only happens when transitioning from a normal |value_| to
  // a |rare_data_|.
  union DataUnion {
    enum ConstructUninitializedTag { kConstructUninitialized };
    explicit DataUnion(ConstructUninitializedTag) {}

    enum ConstructEmptyValueTag { kConstructEmptyValue };
    explicit DataUnion(ConstructEmptyValueTag) : value_() {}

    explicit DataUnion(const QualifiedName& tag_q_name)
        : tag_q_name_(tag_q_name) {}

    ~DataUnion() {}

    AtomicString value_;
    QualifiedName tag_q_name_;
    scoped_refptr<RareData> rare_data_;
  } data_;
};

inline const QualifiedName& CSSSelector::Attribute() const {
  DCHECK(IsAttributeSelector());
  DCHECK(has_rare_data_);
  return data_.rare_data_->attribute_;
}

inline CSSSelector::AttributeMatchType CSSSelector::AttributeMatch() const {
  DCHECK(IsAttributeSelector());
  DCHECK(has_rare_data_);
  return data_.rare_data_->bits_.attribute_match_;
}

inline bool CSSSelector::IsASCIILower(const AtomicString& value) {
  for (wtf_size_t i = 0; i < value.length(); ++i) {
    if (IsASCIIUpper(value[i]))
      return false;
  }
  return true;
}

inline void CSSSelector::SetValue(const AtomicString& value,
                                  bool match_lower_case = false) {
  DCHECK_NE(match_, static_cast<unsigned>(kTag));
  if (match_lower_case && !has_rare_data_ && !IsASCIILower(value)) {
    CreateRareData();
  }

  if (!has_rare_data_) {
    data_.value_ = value;
    return;
  }
  data_.rare_data_->matching_value_ =
      match_lower_case ? value.LowerASCII() : value;
  data_.rare_data_->serializing_value_ = value;
}

inline CSSSelector::CSSSelector()
    : relation_(kSubSelector),
      match_(kUnknown),
      pseudo_type_(kPseudoUnknown),
      is_last_in_selector_list_(false),
      is_last_in_tag_history_(true),
      has_rare_data_(false),
      is_for_page_(false),
      tag_is_implicit_(false),
      is_last_in_original_list_(false),
      data_(DataUnion::kConstructEmptyValue) {}

inline CSSSelector::CSSSelector(const QualifiedName& tag_q_name,
                                bool tag_is_implicit)
    : relation_(kSubSelector),
      match_(kTag),
      pseudo_type_(kPseudoUnknown),
      is_last_in_selector_list_(false),
      is_last_in_tag_history_(true),
      has_rare_data_(false),
      is_for_page_(false),
      tag_is_implicit_(tag_is_implicit),
      is_last_in_original_list_(false),
      data_(tag_q_name) {}

inline CSSSelector::CSSSelector(const CSSSelector& o)
    : relation_(o.relation_),
      match_(o.match_),
      pseudo_type_(o.pseudo_type_),
      is_last_in_selector_list_(o.is_last_in_selector_list_),
      is_last_in_tag_history_(o.is_last_in_tag_history_),
      has_rare_data_(o.has_rare_data_),
      is_for_page_(o.is_for_page_),
      tag_is_implicit_(o.tag_is_implicit_),
      is_last_in_original_list_(o.is_last_in_original_list_),
      data_(DataUnion::kConstructUninitialized) {
  if (o.match_ == kTag) {
    new (&data_.tag_q_name_) QualifiedName(o.data_.tag_q_name_);
  } else if (o.has_rare_data_) {
    new (&data_.rare_data_) scoped_refptr<RareData>(o.data_.rare_data_);
  } else {
    new (&data_.value_) AtomicString(o.data_.value_);
  }
}

inline CSSSelector::~CSSSelector() {
  if (match_ == kTag)
    data_.tag_q_name_.~QualifiedName();
  else if (has_rare_data_)
    data_.rare_data_.~scoped_refptr<RareData>();
  else
    data_.value_.~AtomicString();
}

inline const QualifiedName& CSSSelector::TagQName() const {
  DCHECK_EQ(match_, static_cast<unsigned>(kTag));
  return data_.tag_q_name_;
}

inline const AtomicString& CSSSelector::Value() const {
  DCHECK_NE(match_, static_cast<unsigned>(kTag));
  if (has_rare_data_)
    return data_.rare_data_->matching_value_;
  return data_.value_;
}

inline const AtomicString& CSSSelector::SerializingValue() const {
  DCHECK_NE(match_, static_cast<unsigned>(kTag));
  if (has_rare_data_)
    return data_.rare_data_->serializing_value_;
  return data_.value_;
}

inline bool CSSSelector::IsUserActionPseudoClass() const {
  return pseudo_type_ == kPseudoHover || pseudo_type_ == kPseudoActive ||
         pseudo_type_ == kPseudoFocus || pseudo_type_ == kPseudoDrag ||
         pseudo_type_ == kPseudoFocusWithin ||
         pseudo_type_ == kPseudoFocusVisible;
}

inline bool CSSSelector::IsIdClassOrAttributeSelector() const {
  return IsAttributeSelector() || Match() == CSSSelector::kId ||
         Match() == CSSSelector::kClass;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SELECTOR_H_
