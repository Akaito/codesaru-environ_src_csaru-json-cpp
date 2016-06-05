/*
Copyright (c) 2015 Christopher Higgins Barrett

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#include <csaru-datamap-cpp/DataNode.hpp>

#include "../include/JsonParserCallbackForDataMap.hpp"

namespace CSaruJson {

//=========================================================================
JsonParserCallbackForDataMap::JsonParserCallbackForDataMap (const CSaruDataMap::DataMapMutator & mutator)
    : m_mutator(mutator)
{}

//=========================================================================
void JsonParserCallbackForDataMap::BeginObject(const char * name, size_t nameLen) {
    m_mutator.WriteNameSecure(name, int(nameLen));
    m_mutator.SetToObjectType();
    m_mutator.CreateAndGotoChildSafe("", 0);
}

//=========================================================================
void JsonParserCallbackForDataMap::EndObject() {
    // we only want to clean the last child if it was a temporary write-location
    //  created while parsing.  If we've just bubbled back up from lower nodes,
    //  the last child should _not_ be deleted.
    if (m_mutator.GetCurrentNode()->GetType() == CSaruDataMap::DataNode::Type::Unused) {
        m_mutator.ToParent();
        m_mutator.DeleteLastChildren(1);
    }
    else
        m_mutator.ToParent();
    
    // if in a child object, prepare for more data to be written out
    if (m_mutator.GetCurrentDepth() >= 2)
        m_mutator.Walk(1);
}

//=========================================================================
void JsonParserCallbackForDataMap::BeginArray(const char * name, size_t nameLen) {
    m_mutator.WriteNameSecure(name, int(nameLen));
    m_mutator.SetToArrayType();
    m_mutator.CreateAndGotoChildSafe("", 0);
}

//=========================================================================
void JsonParserCallbackForDataMap::EndArray()
{
    //m_mutator.ToParent();
    //m_mutator.DeleteLastChildren(1);

    // we only want to clean the last child if it was a temporary write-location
    //  created while parsing.  If we've just bubbled back up from lower nodes,
    //  the last child should _not_ be deleted.
    if (m_mutator.GetCurrentNode()->GetType() == CSaruDataMap::DataNode::Type::Unused) {
        m_mutator.ToParent();
        m_mutator.DeleteLastChildren(1);
    }
    else
        m_mutator.ToParent();
    
    m_mutator.Walk(1);
}

//=========================================================================
void JsonParserCallbackForDataMap::GotString(const char * name, size_t nameLen, const char * value, size_t valueLen) {
  m_mutator.WriteWalkSafe(name, static_cast<int>(nameLen), value, static_cast<int>(valueLen));
}

//=========================================================================
void JsonParserCallbackForDataMap::GotFloat(const char * name, size_t nameLen, float value) {
    m_mutator.WriteNameSecure(name, int(nameLen));
    m_mutator.Write(value);
    m_mutator.Walk(1);
}

//=========================================================================
void JsonParserCallbackForDataMap::GotInteger(const char * name, size_t nameLen, int value) {
    m_mutator.WriteNameSecure(name, int(nameLen));
    m_mutator.Write(value);
    m_mutator.Walk(1);
}

//=========================================================================
void JsonParserCallbackForDataMap::GotBoolean(const char * name, size_t nameLen, bool value) {
    m_mutator.WriteWalkSafeBooleanValue(name, static_cast<int>(nameLen), value);
}

//=========================================================================
void JsonParserCallbackForDataMap::GotNull(const char * name, size_t nameLen) {
    m_mutator.WriteWalkSafeNullValue(name, static_cast<int>(nameLen));
}

//=========================================================================
void JsonParserCallbackForDataMap::SetMutator(const CSaruDataMap::DataMapMutator & mutator) {
    m_mutator = mutator;
}

} // namespace CSaruJson
