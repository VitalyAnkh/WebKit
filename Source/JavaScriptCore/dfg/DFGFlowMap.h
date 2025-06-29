/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(DFG_JIT)

#include "DFGFlowIndexing.h"
#include "DFGGraph.h"
#include "DFGNode.h"
#include <wtf/TZoneMalloc.h>

namespace JSC { namespace DFG {

// This is a mapping from nodes to values that is useful for flow-sensitive analysis. In such an
// analysis, at every point in the program we need to consider the values of nodes plus the shadow
// values of Phis. This makes it easy to do both of those things.
template<typename T>
class FlowMap {
    WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE(FlowMap);
public:
    FlowMap(Graph& graph)
        : m_graph(graph)
    {
        resize();
    }
    
    // Call this if the number of nodes in the graph has changed. Note that this does not reset any
    // entries.
    void resize()
    {
        m_map.resize(m_graph.maxNodeCount());
        if (m_graph.m_form == SSA)
            m_shadowMap.resize(m_graph.maxNodeCount());
    }
    
    Graph& graph() const { return m_graph; }
    
    ALWAYS_INLINE T& at(unsigned nodeIndex)
    {
        return m_map[nodeIndex];
    }
    
    ALWAYS_INLINE T& at(Node* node)
    {
        return at(node->index());
    }
    
    ALWAYS_INLINE T& atShadow(unsigned nodeIndex)
    {
        return m_shadowMap[nodeIndex];
    }
    
    ALWAYS_INLINE T& atShadow(Node* node)
    {
        return atShadow(node->index());
    }
    
    ALWAYS_INLINE T& at(unsigned nodeIndex, NodeFlowProjection::Kind kind)
    {
        switch (kind) {
        case NodeFlowProjection::Primary:
            return at(nodeIndex);
        case NodeFlowProjection::Shadow:
            return atShadow(nodeIndex);
        }
        RELEASE_ASSERT_NOT_REACHED();
        return *std::bit_cast<T*>(nullptr);
    }
    
    ALWAYS_INLINE T& at(Node* node, NodeFlowProjection::Kind kind)
    {
        return at(node->index(), kind);
    }
    
    ALWAYS_INLINE T& at(NodeFlowProjection projection)
    {
        return at(projection.node(), projection.kind());
    }
    
    ALWAYS_INLINE const T& at(unsigned nodeIndex) const { return const_cast<FlowMap*>(this)->at(nodeIndex); }
    ALWAYS_INLINE const T& at(Node* node) const { return const_cast<FlowMap*>(this)->at(node); }
    ALWAYS_INLINE const T& atShadow(unsigned nodeIndex) const { return const_cast<FlowMap*>(this)->atShadow(nodeIndex); }
    ALWAYS_INLINE const T& atShadow(Node* node) const { return const_cast<FlowMap*>(this)->atShadow(node); }
    ALWAYS_INLINE const T& at(unsigned nodeIndex, NodeFlowProjection::Kind kind) const { return const_cast<FlowMap*>(this)->at(nodeIndex, kind); }
    ALWAYS_INLINE const T& at(Node* node, NodeFlowProjection::Kind kind) const { return const_cast<FlowMap*>(this)->at(node, kind); }
    ALWAYS_INLINE const T& at(NodeFlowProjection projection) const { return const_cast<FlowMap*>(this)->at(projection); }

    ALWAYS_INLINE void clear()
    {
        m_map.clear();
        m_shadowMap.clear();
        resize();
    }

private:
    Graph& m_graph;
    Vector<T, 0, UnsafeVectorOverflow> m_map;
    Vector<T, 0, UnsafeVectorOverflow> m_shadowMap;
};

WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED_TEMPLATE_IMPL(template<typename T>, FlowMap<T>);

} } // namespace JSC::DFG

namespace WTF {

template<typename T>
void printInternal(PrintStream& out, const JSC::DFG::FlowMap<T>& map)
{
    CommaPrinter comma;
    for (unsigned i = 0; i < map.graph().maxNodeCount(); ++i) {
        if (JSC::DFG::Node* node = map.graph().nodeAt(i)) {
            if (const T& value = map.at(node))
                out.print(comma, node, "=>"_s, value);
        }
    }
    for (unsigned i = 0; i < map.graph().maxNodeCount(); ++i) {
        if (JSC::DFG::Node* node = map.graph().nodeAt(i)) {
            if (const T& value = map.atShadow(node))
                out.print(comma, "shadow("_s, node, ")=>"_s, value);
        }
    }
}

} // namespace WTF

#endif // ENABLE(DFG_JIT)

