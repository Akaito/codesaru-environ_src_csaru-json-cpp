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
