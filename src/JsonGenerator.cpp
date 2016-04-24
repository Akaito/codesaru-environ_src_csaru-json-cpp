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

#include "../include/JsonGenerator.hpp"

#if _MSC_VER > 1000
    #pragma warning(push)
    // unsafe functions warning, such as fopen()
    #pragma warning(disable:4996)
#endif

namespace CSaruJson {

//=========================================================================
bool JsonGenerator::WriteToFile (CSaruContainer::DataMapReader * reader, char const * filename) {
    // check for NULL reader
    if (reader == NULL) {
        #ifdef _DEBUG
            fprintf(stderr, "JsonGenerator::WriteToFile() called, but reader == NULL.\n");
        #endif
        return false;
    }
    // check for NULL filename
    if (filename == NULL) {
        #ifdef _DEBUG
            fprintf(stderr, "JsonGenerator::WriteToFile() called, but filename == NULL.\n");
        #endif
        return false;
    }

    std::FILE * file = fopen(filename, "wt");
    // check for successful fopen
    if (file == NULL) {
        #ifdef _DEBUG
            fprintf(stderr, "JsonGenerator::WriteToFile() failed to open desired file.  File was [%s].\n", filename);
        #endif
        return false;
    }

    const bool writeResult = WriteToStream(reader, file);

    fclose(file);
    return writeResult;
}

//=========================================================================
bool JsonGenerator::WriteToStream (CSaruContainer::DataMapReader * reader, std::FILE * file) {
    // check for NULL reader
    if (reader == NULL) {
        #ifdef _DEBUG
            fprintf(stderr, "JsonGenerator::WriteToFile() called, but reader == NULL.\n");
        #endif
        return false;
    }

    // check for successful fopen
    if (file == NULL) {
        #ifdef _DEBUG
            fprintf(stderr, "JsonGenerator::WriteToStream() was given a NULL file pointer.\n");
        #endif
        return false;
    }

    const bool writeResult = WriteJsonToFile(file, reader, false);
    return writeResult;
}

//=========================================================================
bool JsonGenerator::WriteIndent (std::FILE * file, int indentAmount) {
    for (int i = 0;  i < indentAmount;  ++i)
        fprintf(file, " ");
    return true;
}

//=========================================================================
bool JsonGenerator::WriteJsonToFile (
    std::FILE *                     file,
    CSaruContainer::DataMapReader * reader,
    bool                            currentNodeWritesName
) {
    // indent
    WriteIndent(file, reader->GetCurrentDepth() * 2);
    // write name if node isn't root, and its parent isn't an array
    if (currentNodeWritesName) {
        //fprintf(file, "\"%s\": ", reader->ReadName());
        fprintf(file, "\"");
        WriteEscapedStringToFile(file, reader->ReadName());
        fprintf(file, "\": ");
    }
    // write data based on current node type
    switch (reader->GetCurrentNode()->GetType()) {
		case CSaruContainer::DataNode::Type::Unused: // may want to error here instead
        case CSaruContainer::DataNode::Type::Null: {
            fprintf(file, "null");
        } break;

        case CSaruContainer::DataNode::Type::Object: {
            fprintf(file, "{\n");
            // objects tend to have children, print them if this one has any
            if (reader->GetCurrentNode()->HasChildren()) {
                reader->ToFirstChild();
                /*bool result =*/ WriteJsonToFile(file, reader, true);
                reader->PopNode();
            }
            // terminate object
            WriteIndent(file, reader->GetCurrentDepth() * 2);
        fprintf(file, "}");
        } break;

        case CSaruContainer::DataNode::Type::Array: {
            fprintf(file, "[\n");
            // arrays tend to have children, print them if this one has any
            if (reader->GetCurrentNode()->HasChildren()) {
                reader->ToFirstChild();
                /*bool result =*/ WriteJsonToFile(file, reader, false);
                reader->PopNode();
            }
            // terminate array
            WriteIndent(file, reader->GetCurrentDepth() * 2);
            fprintf(file, "]");
        } break;

        case CSaruContainer::DataNode::Type::Bool: {
            if (reader->ReadBool())
                fprintf(file, "true");
            else
                fprintf(file, "false");
        } break;

        case CSaruContainer::DataNode::Type::Int: {
            fprintf(file, "%d", reader->ReadInt());
        } break;

        case CSaruContainer::DataNode::Type::Float: {
            fprintf(file, "%f", reader->ReadFloat());
        } break;

        case CSaruContainer::DataNode::Type::String: {
            fprintf(file, "\"");
            WriteEscapedStringToFile(file, reader->ReadString());
            fprintf(file, "\"");
        } break;
    }

    // write siblings, if there are any
    if (reader->ToNextSibling().IsValid()) {
        fprintf(file, ",\n");
        if (!WriteJsonToFile(file, reader, currentNodeWritesName))
            return false;
    }
    // otherwise, just terminate the current line
    else {
        fprintf(file, "\n");
    }

    return true;
}

//=========================================================================
void JsonGenerator::WriteEscapedStringToFile (std::FILE * file, const char * string) {
    while (*string) {
        switch (*string) {
            case '"':  fprintf(file, "\\\"");        break;
            case '\\': fprintf(file, "\\\\");        break;
            // backspace
            case 0x08: fprintf(file, "\\b");         break;
            // formfeed
            case 0x0C: fprintf(file, "\\f");         break;
            // newline
            case 0x0A: fprintf(file, "\\n");         break;
            // carriage return
            case 0x0D: fprintf(file, "\\r");         break;
            // horizontal tab
            case 0x09: fprintf(file, "\\t");         break;

            default:   fprintf(file, "%c", *string); break;
        }

        ++string;
    }
}

} // namespace CSaruJson

#if _MSC_VER > 1000
    #pragma warning(pop)
#endif
