/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XPathStep.h"

#include "Attr.h"
#include "CommonAtomStrings.h"
#include "Document.h"
#include "ElementInlines.h"
#include "HTMLElement.h"
#include "NodeTraversal.h"
#include "XMLNSNames.h"
#include "XPathParser.h"
#include "XPathUtil.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace XPath {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Step);
WTF_MAKE_TZONE_ALLOCATED_IMPL(Step::NodeTest);

Step::Step(Axis axis, NodeTest nodeTest)
    : m_axis(axis)
    , m_nodeTest(WTFMove(nodeTest))
{
}

Step::Step(Axis axis, NodeTest nodeTest, Vector<std::unique_ptr<Expression>> predicates)
    : m_axis(axis)
    , m_nodeTest(WTFMove(nodeTest))
    , m_predicates(WTFMove(predicates))
{
}

Step::~Step() = default;

void Step::optimize()
{
    // Evaluate predicates as part of node test if possible to avoid building unnecessary NodeSets.
    // E.g., there is no need to build a set of all "foo" nodes to evaluate "foo[@bar]", we can check the predicate while enumerating.
    // This optimization can be applied to predicates that are not context node list sensitive, or to first predicate that is only context position sensitive, e.g. foo[position() mod 2 = 0].
    Vector<std::unique_ptr<Expression>> remainingPredicates;
    for (auto& predicate : m_predicates) {
        if ((!predicateIsContextPositionSensitive(*predicate) || m_nodeTest.m_mergedPredicates.isEmpty()) && !predicate->isContextSizeSensitive() && remainingPredicates.isEmpty())
            m_nodeTest.m_mergedPredicates.append(WTFMove(predicate));
        else
            remainingPredicates.append(WTFMove(predicate));
    }
    m_predicates = WTFMove(remainingPredicates);
}

void optimizeStepPair(Step& first, Step& second, bool& dropSecondStep)
{
    dropSecondStep = false;

    if (first.m_axis != Step::DescendantOrSelfAxis)
        return;

    if (first.m_nodeTest.m_kind != Step::NodeTest::AnyNodeTest)
        return;

    if (!first.m_predicates.isEmpty())
        return;

    if (!first.m_nodeTest.m_mergedPredicates.isEmpty())
        return;

    ASSERT(first.m_nodeTest.m_data.isEmpty());
    ASSERT(first.m_nodeTest.m_namespaceURI.isEmpty());

    // Optimize the common case of "//" AKA /descendant-or-self::node()/child::NodeTest to /descendant::NodeTest.
    if (second.m_axis != Step::ChildAxis)
        return;

    if (!second.predicatesAreContextListInsensitive())
        return;

    first.m_axis = Step::DescendantAxis;
    first.m_nodeTest = WTFMove(second.m_nodeTest);
    first.m_predicates = WTFMove(second.m_predicates);
    first.optimize();
    dropSecondStep = true;
}

bool Step::predicatesAreContextListInsensitive() const
{
    for (auto& predicate : m_predicates) {
        if (predicateIsContextPositionSensitive(*predicate) || predicate->isContextSizeSensitive())
            return false;
    }

    for (auto& predicate : m_nodeTest.m_mergedPredicates) {
        if (predicateIsContextPositionSensitive(*predicate) || predicate->isContextSizeSensitive())
            return false;
    }

    return true;
}

void Step::evaluate(Node& context, NodeSet& nodes) const
{
    EvaluationContext& evaluationContext = Expression::evaluationContext();
    evaluationContext.position = 0;

    nodesInAxis(context, nodes);

    // Check predicates that couldn't be merged into node test.
    for (auto& predicate : m_predicates) {
        NodeSet newNodes;
        if (!nodes.isSorted())
            newNodes.markSorted(false);

        for (unsigned j = 0; j < nodes.size(); j++) {
            RefPtr node = nodes[j];

            evaluationContext.node = node;
            evaluationContext.size = nodes.size();
            evaluationContext.position = j + 1;
            if (evaluatePredicate(*predicate))
                newNodes.append(WTFMove(node));
        }

        nodes = WTFMove(newNodes);
    }
}

#if ASSERT_ENABLED
static inline Node::NodeType primaryNodeType(Step::Axis axis)
{
    switch (axis) {
        case Step::AttributeAxis:
            return Node::ATTRIBUTE_NODE;
        default:
            return Node::ELEMENT_NODE;
    }
}
#endif // ASSERT_ENABLED

// Evaluate NodeTest without considering merged predicates.
inline bool nodeMatchesBasicTest(Node& node, Step::Axis axis, const Step::NodeTest& nodeTest)
{
    switch (nodeTest.m_kind) {
        case Step::NodeTest::TextNodeTest:
            return node.nodeType() == Node::TEXT_NODE || node.nodeType() == Node::CDATA_SECTION_NODE;
        case Step::NodeTest::CommentNodeTest:
            return node.nodeType() == Node::COMMENT_NODE;
        case Step::NodeTest::ProcessingInstructionNodeTest: {
            const AtomString& name = nodeTest.m_data;
            return node.nodeType() == Node::PROCESSING_INSTRUCTION_NODE && (name.isEmpty() || node.nodeName() == name);
        }
        case Step::NodeTest::AnyNodeTest:
            return true;
        case Step::NodeTest::NameTest: {
            const AtomString& name = nodeTest.m_data;
            const AtomString& namespaceURI = nodeTest.m_namespaceURI;

            if (axis == Step::AttributeAxis) {
                ASSERT(is<Attr>(node));

                // In XPath land, namespace nodes are not accessible on the attribute axis.
                if (node.namespaceURI() == XMLNSNames::xmlnsNamespaceURI)
                    return false;

                if (name == starAtom())
                    return namespaceURI.isEmpty() || node.namespaceURI() == namespaceURI;

                auto& attr = downcast<Attr>(node);
                if (attr.document().isHTMLDocument() && attr.ownerElement() && attr.ownerElement()->isHTMLElement()
                    && namespaceURI.isNull() && attr.namespaceURI().isNull()) {
                    return equalIgnoringASCIICase(attr.localName(), name);
                }

                return node.localName() == name && node.namespaceURI() == namespaceURI;
            }

            // Node test on the namespace axis is not implemented yet, the caller has a check for it.
            ASSERT(axis != Step::NamespaceAxis);

            // For other axes, the principal node type is element.
            ASSERT(primaryNodeType(axis) == Node::ELEMENT_NODE);
            RefPtr element = dynamicDowncast<Element>(node);
            if (!element)
                return false;

            if (name == starAtom())
                return namespaceURI.isEmpty() || namespaceURI == node.namespaceURI();

            if (node.document().isHTMLDocument()) {
                if (is<HTMLElement>(*element)) {
                    // Paths without namespaces should match HTML elements in HTML documents despite those having an XHTML namespace. Names are compared case-insensitively.
                    return equalIgnoringASCIICase(element->localName(), name) && (namespaceURI.isNull() || namespaceURI == node.namespaceURI());
                }
                // An expression without any prefix shouldn't match no-namespace nodes (because HTML5 says so).
                return element->hasLocalName(name) && namespaceURI == node.namespaceURI() && !namespaceURI.isNull();
            }
            return element->hasLocalName(name) && namespaceURI == node.namespaceURI();
        }
    }
    ASSERT_NOT_REACHED();
    return false;
}

inline bool nodeMatches(Node& node, Step::Axis axis, const Step::NodeTest& nodeTest)
{
    if (!nodeMatchesBasicTest(node, axis, nodeTest))
        return false;

    EvaluationContext& evaluationContext = Expression::evaluationContext();

    // Only the first merged predicate may depend on position.
    ++evaluationContext.position;

    for (auto& predicate : nodeTest.m_mergedPredicates) {
        // No need to set context size - we only get here when evaluating predicates that do not depend on it.
        evaluationContext.node = &node;
        if (!evaluatePredicate(*predicate))
            return false;
    }

    return true;
}

// Result nodes are ordered in axis order. Node test (including merged predicates) is applied.
void Step::nodesInAxis(Node& context, NodeSet& nodes) const
{
    ASSERT(nodes.isEmpty());
    switch (m_axis) {
        case ChildAxis:
            if (is<Attr>(context)) // In XPath model, attribute nodes do not have children.
                return;
            for (RefPtr node = context.firstChild(); node; node = node->nextSibling()) {
                if (nodeMatches(*node, ChildAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            return;
        case DescendantAxis:
            if (is<Attr>(context)) // In XPath model, attribute nodes do not have children.
                return;
            for (RefPtr node = context.firstChild(); node; node = NodeTraversal::next(*node, &context)) {
                if (nodeMatches(*node, DescendantAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            return;
        case ParentAxis:
            if (RefPtr attr = dynamicDowncast<Attr>(context)) {
                RefPtr node = attr->ownerElement();
                if (node && nodeMatches(*node, ParentAxis, m_nodeTest))
                    nodes.append(node.get());
            } else {
                RefPtr node = context.parentNode();
                if (node && nodeMatches(*node, ParentAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            return;
        case AncestorAxis: {
            RefPtr node = context;
            if (RefPtr attr = dynamicDowncast<Attr>(context)) {
                node = attr->ownerElement();
                if (!node)
                    return;
                if (nodeMatches(*node, AncestorAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            for (node = node->parentNode(); node; node = node->parentNode()) {
                if (nodeMatches(*node, AncestorAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            nodes.markSorted(false);
            return;
        }
        case FollowingSiblingAxis:
            if (is<Attr>(context))
                return;
            for (RefPtr node = context.nextSibling(); node; node = node->nextSibling()) {
                if (nodeMatches(*node, FollowingSiblingAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            return;
        case PrecedingSiblingAxis:
            if (is<Attr>(context))
                return;
            for (RefPtr node = context.previousSibling(); node; node = node->previousSibling()) {
                if (nodeMatches(*node, PrecedingSiblingAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            nodes.markSorted(false);
            return;
        case FollowingAxis:
            if (RefPtr attr = dynamicDowncast<Attr>(context)) {
                RefPtr<Node> node = attr->ownerElement();
                if (!node)
                    return;
                while ((node = NodeTraversal::next(*node))) {
                    if (nodeMatches(*node, FollowingAxis, m_nodeTest))
                        nodes.append(node.get());
                }
            } else {
                for (RefPtr parent = &context; !isRootDomNode(parent.get()); parent = parent->parentNode()) {
                    for (RefPtr node = parent->nextSibling(); node; node = node->nextSibling()) {
                        if (nodeMatches(*node, FollowingAxis, m_nodeTest))
                            nodes.append(node.get());
                        for (RefPtr child = node->firstChild(); child; child = NodeTraversal::next(*child, node.get())) {
                            if (nodeMatches(*child, FollowingAxis, m_nodeTest))
                                nodes.append(child.get());
                        }
                    }
                }
            }
            return;
        case PrecedingAxis: {
            RefPtr<Node> node;
            if (RefPtr attr = dynamicDowncast<Attr>(context)) {
                node = attr->ownerElement();
                if (!node)
                    return;
            } else
                node = &context;
            while (RefPtr parent = node->parentNode()) {
                for (node = NodeTraversal::previous(*node); node != parent; node = NodeTraversal::previous(*node)) {
                    if (nodeMatches(*node, PrecedingAxis, m_nodeTest))
                        nodes.append(node.get());
                }
                node = parent;
            }
            nodes.markSorted(false);
            return;
        }
        case AttributeAxis: {
            RefPtr contextElement = dynamicDowncast<Element>(context);
            if (!contextElement)
                return;

            // Avoid lazily creating attribute nodes for attributes that we do not need anyway.
            if (m_nodeTest.m_kind == NodeTest::NameTest && m_nodeTest.m_data != starAtom()) {
                RefPtr<Attr> attr;
                // We need this branch because getAttributeNodeNS() doesn't do
                // ignore-case matching even for an HTML element in an HTML document.
                if (m_nodeTest.m_namespaceURI.isNull())
                    attr = contextElement->getAttributeNode(m_nodeTest.m_data);
                else
                    attr = contextElement->getAttributeNodeNS(m_nodeTest.m_namespaceURI, m_nodeTest.m_data);
                if (attr && attr->namespaceURI() != XMLNSNames::xmlnsNamespaceURI) { // In XPath land, namespace nodes are not accessible on the attribute axis.
                    if (nodeMatches(*attr, AttributeAxis, m_nodeTest)) // Still need to check merged predicates.
                        nodes.append(WTFMove(attr));
                }
                return;
            }
            
            if (!contextElement->hasAttributes())
                return;

            for (auto& attribute : contextElement->attributes()) {
                auto attr = contextElement->ensureAttr(attribute.name());
                if (nodeMatches(attr.get(), AttributeAxis, m_nodeTest))
                    nodes.append(WTFMove(attr));
            }
            return;
        }
        case NamespaceAxis:
            // XPath namespace nodes are not implemented yet.
            return;
        case SelfAxis:
            if (nodeMatches(context, SelfAxis, m_nodeTest))
                nodes.append(&context);
            return;
        case DescendantOrSelfAxis:
            if (nodeMatches(context, DescendantOrSelfAxis, m_nodeTest))
                nodes.append(&context);
            if (is<Attr>(context)) // In XPath model, attribute nodes do not have children.
                return;
            for (RefPtr node = context.firstChild(); node; node = NodeTraversal::next(*node, &context)) {
                if (nodeMatches(*node, DescendantOrSelfAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            return;
        case AncestorOrSelfAxis: {
            if (nodeMatches(context, AncestorOrSelfAxis, m_nodeTest))
                nodes.append(&context);
            RefPtr node = context;
            if (RefPtr attr = dynamicDowncast<Attr>(context)) {
                node = attr->ownerElement();
                if (!node)
                    return;
                if (nodeMatches(*node, AncestorOrSelfAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            for (node = node->parentNode(); node; node = node->parentNode()) {
                if (nodeMatches(*node, AncestorOrSelfAxis, m_nodeTest))
                    nodes.append(node.get());
            }
            nodes.markSorted(false);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

}
}
