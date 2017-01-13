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

#pragma once

#include <csaru-datamap-cpp/DataMapMutator.hpp>

#include "JsonParser.hpp"

namespace CSaruJson {

class JsonParserCallbackForDataMap : public JsonParser::CallbackInterface {
private:
    // Data
    CSaruDataMap::DataMapMutator m_mutator;

public:
    // Methods
    JsonParserCallbackForDataMap (const CSaruDataMap::DataMapMutator & mutator);

    // Commands
    void SetMutator (const CSaruDataMap::DataMapMutator & mutator);

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
