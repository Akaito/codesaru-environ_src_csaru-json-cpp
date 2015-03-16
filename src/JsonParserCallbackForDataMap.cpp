/*
Copyright (c) 2015 Christopher Higgins Barrett

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "JsonParserCallbackForDataMap.hpp"
#include <DataNode.hpp>

namespace CSaruJson {

//=========================================================================
JsonParserCallbackForDataMap::JsonParserCallbackForDataMap (const CSaruContainer::DataMapMutator & mutator)
    : m_mutator(mutator)
{}

//=========================================================================
void JsonParserCallbackForDataMap::BeginObject(const char * name, size_t nameLen) {
    m_mutator.WriteNameSecure(name, nameLen);
    m_mutator.SetToObjectType();
    m_mutator.CreateAndGotoChildSafe("", 0);
}

//=========================================================================
void JsonParserCallbackForDataMap::EndObject() {
    // we only want to clean the last child if it was a temporary write-location
    //  created while parsing.  If we've just bubbled back up from lower nodes,
    //  the last child should _not_ be deleted.
    if (m_mutator.GetCurrentNode()->GetType() == CSaruContainer::DataNode::Type::Unused) {
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
    m_mutator.WriteNameSecure(name, nameLen);
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
    if (m_mutator.GetCurrentNode()->GetType() == CSaruContainer::DataNode::Type::Unused) {
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
    m_mutator.WriteNameSecure(name, nameLen);
    m_mutator.Write(value);
    m_mutator.Walk(1);
}

//=========================================================================
void JsonParserCallbackForDataMap::GotInteger(const char * name, size_t nameLen, int value) {
    m_mutator.WriteNameSecure(name, nameLen);
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
void JsonParserCallbackForDataMap::SetMutator(const CSaruContainer::DataMapMutator & mutator) {
    m_mutator = mutator;
}

} // namespace CSaruJson
