/*
Copyright (c) 2011 Christopher Higgins Barrett

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

#pragma once

#include <csaru-container-cpp/DataMapMutator.hpp>

#include "JsonParser.hpp"

namespace CSaruJson {

class JsonParserCallbackForDataMap : public JsonParser::CallbackInterface {
private:
    // Data
    CSaruContainer::DataMapMutator m_mutator;

public:
    // Methods
    JsonParserCallbackForDataMap (const CSaruContainer::DataMapMutator & mutator);

    // Commands
    void SetMutator (const CSaruContainer::DataMapMutator & mutator);

    // CallbackInterface implementations
    virtual void BeginObject (const char * name, size_t nameLen);
    virtual void EndObject (void);
    virtual void BeginArray (const char * name, size_t nameLen);
    virtual void EndArray (void);
    virtual void GotString (const char * name, size_t nameLen, const char * value, size_t valueLen);
    virtual void GotFloat (const char * name, size_t nameLen, float value);
    virtual void GotInteger (const char * name, size_t nameLen, int value);
    virtual void GotBoolean (const char * name, size_t nameLen, bool value);
    virtual void GotNull (const char * name, size_t nameLen);
};

} // namespace CSaruJson
