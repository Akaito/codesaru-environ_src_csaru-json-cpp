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

#include <cstdio>
#include <cstdlib> // atoi()
#include <cstring> // memcpy()

// GetSystemPageSize()
#include <csaru-core-cpp/csaru-core-cpp.h>

#include "../include/JsonParser.hpp"

#ifdef _MSC_VER
    #pragma warning(push)
    // Using sprintf() in some places.  Not sprintf_s; for cross-platform
    //   compatibility.
    #pragma warning(disable: 4996)
#endif 

namespace CSaruJson {

//=========================================================================
JsonParser::JsonParser () {
    Reset();
}

//=========================================================================
bool JsonParser::ParseEntireFile (
    std::FILE *         file,
    char *              freadBuffer,
    size_t              freadBufferSizeInElements,
    CallbackInterface * dataCallback
) {
    Reset();

    // check for no file given
    if (file == nullptr) {
        m_errorStatus = ErrorStatus::Error_CantAccessData;
        NotifyOfError(
            "ParseEntireFile() was given a NULL file pointer.  "
                "Please give a file with read access and in translate mode to be parsed."
        );
        return false;
    }
    // check for no storage destination
    if (dataCallback == nullptr) {
        m_errorStatus = ErrorStatus::Error_CantAccessData;
        NotifyOfError(
            "ParseEntireFile() was given a NULL CallbackInterface pointer to store its result in.  "
                "Please provide a valid CallbackInterface."
        );
        return false;
    }

    // if no working buffer was given, make one
    bool mustDeleteBufferAfter = (freadBuffer == nullptr);
    if (mustDeleteBufferAfter) {
        freadBufferSizeInElements = CSaruCore::GetSystemPageSize();
        freadBuffer = new char[freadBufferSizeInElements];
    }

    m_errorStatus = ErrorStatus::NotFinished;
    while (m_parserStatus < ParserStatus::Done && m_errorStatus <  ErrorStatus::Error_Unspecified) {
        size_t charsThisRead = fread(freadBuffer, sizeof(char), freadBufferSizeInElements, file);
        // if we didn't read in as much as we wanted, check why
        if (charsThisRead != freadBufferSizeInElements) {
            // file reading error?
            if (ferror(file)) {
                m_errorStatus = ErrorStatus::Error_BadFileRead;
                NotifyOfError(NULL);
                break;
            }
            // otherwise, we reached the end of the file.
            //   No special action here. (right?)
        }

        // pass our chunk of memory down to the worker function for parsing
        ParseBuffer(freadBuffer, charsThisRead, dataCallback);
    }

    // clean up our buffer, if the user didn't give us one
    if (mustDeleteBufferAfter)
        delete [] freadBuffer;

    return m_errorStatus < ErrorStatus::Error_Unspecified;
}

//=========================================================================
bool JsonParser::ParseBuffer (const char * buffer, size_t bufferSize, CallbackInterface * dataCallback) {
    // check for no buffer given
    if (buffer == nullptr) {
        m_errorStatus = ErrorStatus::Error_CantAccessData;
        NotifyOfError("ParseBuffer() was given a NULL buffer pointer.");
        return false;
    }
    // check for no storage destination
    if (dataCallback == nullptr) {
        m_errorStatus = ErrorStatus::Error_CantAccessData;
        NotifyOfError(
            "ParseBuffer() was given a NULL CallbackInterface pointer to store its result in.  "
                "Please provide a valid CallbackInterface."
        );
        return false;
    }

    m_source       = buffer;
    m_sourceSize   = bufferSize;
    m_sourceIndex  = 0;
    m_dataCallback = dataCallback;

    while (
        m_errorStatus < ErrorStatus::Error_Unspecified  &&
        m_parserStatus != ParserStatus::Done            &&
        m_parserStatus != ParserStatus::FinishedAllData &&
        m_sourceIndex < m_sourceSize
    ) {
        // walk through buffer, parsing data
        switch (m_parserStatus) {
            // nothing parsed yet.  Only valid thing is the root object's start.
            case ParserStatus::NotStarted: {
                SkipWhitespace(true);
                if (m_sourceIndex >= m_sourceSize)
                    break;
                // should have root object
                if (m_source[m_sourceIndex] == '{')
                    BeginObject();
                // if we didn't begin the root object, error
                else {
                    m_errorStatus = ErrorStatus::ParseError_ExpectedBeginObject;
                    m_parserStatus = ParserStatus::Done;
                    NotifyOfError("All valid JSON data begins with the opening curly brace of the root, unnamed object.");
                }
            } break;

            // An object has already begun.  Only valid things are the name of the
            //   first name-value pair, or an object-terminating curly brace.
            case ParserStatus::BeganObject: {
                SkipWhitespace(true);
                if (m_sourceIndex >= m_sourceSize)
                    break;
                // all object fields have names
                if (m_source[m_sourceIndex] == '"')
                    BeginName();
                // objects can be empty
                else if (m_source[m_sourceIndex] == '}')
                    EndObject();
                // nothing else is valid
                else {
                    m_errorStatus  = ErrorStatus::ParseError_ExpectedString;
                    m_parserStatus = ParserStatus::Done;
                    NotifyOfError(
                        "Every field in an object is made up of a name-value pair.  "
                            "Like this: { \"answer\" : 42 }\n"
                            "Other possible error: Didn't terminate your empty object properly.  Do like this: { }"
                    );
                }
                break;

                // An array has already just begun, or an array value-separating comma
                //   was encoutnered after a valid value in the same array.
                //   Only valid things are...
                //    - Skippable whitespace.
                //    - string value-starting double quote.
                //    - Numeric value-starting digit (or negative sign).
                //    - true-starting 't'.
                //    - false-starting 'f'.
                //    - null-starting 'n'.
                case ParserStatus::BeganArray:
                case ParserStatus::NeedAnotherDataElement_InArray: {
                    SkipWhitespace(true);
                    if (m_sourceIndex >= m_sourceSize)
                        break;

                    switch (m_source[m_sourceIndex]) {
                        // string value?
                        case '"': {
                            BeginStringValue();
                        } break;
                        // number opening negative sign?
                        case '-': {
                            m_parserStatus = ParserStatus::NumberSawLeadingNegativeSign;
                            BeginNumberValue_AtLeadingNegative();
                        } break;
                        // number leading zero?
                        case '0': {
                            m_parserStatus = ParserStatus::NumberSawLeadingZero;
                            BeginNumberValue_AtLeadingZero();
                        } break;
                        // number leading 1-9 digit?
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9': {
                            m_parserStatus = ParserStatus::NumberReadingWholeDigits;
                            //BeginNumberValue_AtNormalDigit();
                            ContinueNumberValue_ReadingWholeDigits();
                        } break;
                        // bad attempt to start a fractional number?
                        case '.': {
                            m_parserStatus = ParserStatus::Done;
                            m_errorStatus  = ErrorStatus::ParserError_PrematureDecimalPoint;
                            NotifyOfError(
                                "Numbers cannot start with a decimal point.  Begin them with a zero first (0.123)"
                            );
                        } break;
                        // 'true' value?
                        case 't': {
                            BeginTrueValue();
                        } break;
                        // 'false' value?
                        case 'f': {
                            BeginFalseValue();
                        } break;
                        // 'null' value?
                        case 'n': {
                            BeginNullValue();
                        } break;
                        // child object value?
                        case '{': {
                            BeginObject();
                        } break;
                        // child array value?
                        case '[': {
                            BeginArray();
                        } break;
                        // arrays can be empty
                        case ']': {
                            if (m_parserStatus == ParserStatus::NeedAnotherDataElement_InArray) {
                                m_parserStatus = ParserStatus::Done;
                                m_errorStatus  = ErrorStatus::ParseError_ExpectedValue;
                                NotifyOfError(
                                    "Expected another value in Array.  Got end-of-array square bracket instead.  "
                                        "Either give another value, or remove the last comma in the array."
                                );
                                break;
                            }
                            EndArray();
                        } break;
                        // TODO: Implement other value types.
                        default: {
                            m_errorStatus  = ErrorStatus::ParseError_ExpectedValue;
                            m_parserStatus = ParserStatus::Done;
                            NotifyOfError(
                                "Saw an object's element's name, then the name-value separator.  "
                                    "But no valid value came after that."
                            );
                        } break;
                    } break; // end switch (m_source[m_sourceIndex])
                } // end case ParserStatus::NeedAnotherDataElement_InArray:
                // ^^^ intentional fall-through!

                // We've already seen a name's opening double-quote.  Since we're
                //   currently reading a name, keep reading in the name.  Until
                //   double-quotes are again encountered.
                case ParserStatus::ReadingName: {
                    if (m_source[m_sourceIndex] == '"')
                        FinishName();
                    else
                        ContinueName();
                } break;

                // After a name (which only occurs in name-value pairs), the only thing
                //   we should see is a name-value-separating colon.
                case ParserStatus::FinishedName: {
                    SkipWhitespace(true);
                    if (m_sourceIndex >= m_sourceSize)
                        break;
                    if (m_source[m_sourceIndex] == ':') {
                        m_parserStatus = ParserStatus::SawNameValueSeparator;
                        ++m_sourceIndex;
                        ++m_currentColumn;
                    }
                    // otherwise, we have malformed data
                    else {
                        m_errorStatus  = ErrorStatus::ParseError_ExpectedNameValueSeparator;
                        m_parserStatus = ParserStatus::Done;
                        NotifyOfError(
                            "Every name must be followed by the name-value separator (a colon).  "
                                "Like this: { \"name\" : \"value\" }"
                        );
                    }
                } break;

                // After the name-value separater, we should see...
                //   double-quote to begin a string value
                //   digit 0-9 or the negative sign (no positive sign!) to begin a number
                //   't' in 'true'  (must be lowercase)
                //   'f' in 'false'  (must be lowercase)
                //   'n' in 'null'  (must be lowercase)
                case ParserStatus::SawNameValueSeparator: {
                    SkipWhitespace(true);
                    if (m_sourceIndex >= m_sourceSize)
                        break;
                    switch (m_source[m_sourceIndex]) {
                        // string value?
                        case '"': {
                            BeginStringValue();
                        } break;
                        // number opening negative sign?
                        case '-': {
                            m_parserStatus = ParserStatus::NumberSawLeadingNegativeSign;
                            BeginNumberValue_AtLeadingNegative();
                        } break;
                        // number leading zero?
                        case '0': {
                            m_parserStatus = ParserStatus::NumberSawLeadingZero;
                            BeginNumberValue_AtLeadingZero();
                        } break;
                        // number leading 1-9 digit?
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9': {
                            m_parserStatus = ParserStatus::NumberReadingWholeDigits;
                            //BeginNumberValue_AtNormalDigit();
                            ContinueNumberValue_ReadingWholeDigits();
                        } break;
                        // bad attempt to start a fractional number?
                        case '.': {
                            m_parserStatus = ParserStatus::Done;
                            m_errorStatus = ErrorStatus::ParserError_PrematureDecimalPoint;
                            NotifyOfError(
                                "Numbers cannot start with a decimal point.  Begin them with a zero first (0.123)."
                            );
                        } break;
                        // 'true' value?
                        case 't': {
                            BeginTrueValue();
                        } break;
                        // 'false' value?
                        case 'f': {
                            BeginFalseValue();
                        } break;
                        // 'null' value?
                        case 'n': {
                            BeginNullValue();
                        } break;
                        // child object value?
                        case '{': {
                            BeginObject();
                        } break;
                        // child array value?
                        case '[': {
                            BeginArray();
                        } break;
                        // TODO: Implement other value types.
                        default: {
                            m_errorStatus  = ErrorStatus::ParseError_ExpectedValue;
                            m_parserStatus = ParserStatus::Done;
                            NotifyOfError(
                                "Saw an object's element's name, then the name-value separator.  "
                                    "But no valid value came after that."
                            );
                        } break;
                    } break; // end switch (m_source[m_sourceIndex])
                } // end case ParserStatus::SawNameValueSeparator:
                // ^^^ intentional fall-through!

                // We've already seen a string value's opening double-quote.  Since we're
                //   currently reading a string, keep reading in the string.  Until
                //   double-quotes are encountered again.  This is broken up in this way
                //   because the string may span a number of buffer-parse calls.
                case ParserStatus::ReadingStringValue: {
                    if (m_source[m_sourceIndex] == '"')
                        FinishStringValue();
                    else
                        ContinueStringValue();
                } break;

                case ParserStatus::ReadingName_EscapedChar:
                case ParserStatus::ReadingStringValue_EscapedChar: {
                    HandleEscapedCharacter();
                } break;

                case ParserStatus::NumberSawLeadingNegativeSign: {
                    ContinueNumberValue_AfterLeadingNegative();
                } break;

                case ParserStatus::NumberSawLeadingZero: {
                    ContinueNumberValue_AfterLeadingZero();
                    /*
                    // begin floating-pointer number?
                    if (m_source[m_sourceIndex] == '.') {
                    }
                    // end number at just zero?
                    else if (
                        m_source[m_sourceIndex] == ',' ||
                        m_source[m_sourceIndex] == '}' ||
                        m_source[m_sourceIndex] == ']' ||
                        IsWhitespace(m_source[m_sourceIndex], true)
                    ) {
                    }
                    else {
                    }
                    //*/
                } break;

                case ParserStatus::NumberReadingWholeDigits: {
                    switch (m_source[m_sourceIndex]) {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9': {
                            ContinueNumberValue_ReadingWholeDigits();
                        } break;

                        case '.': {
                            m_parserStatus = ParserStatus::NumberSawDecimalPoint;
                            m_tempData[m_tempDataIndex] = '.';
                            ++m_tempDataIndex;
                            ++m_sourceIndex;
                            ++m_currentColumn;
                        } break;

                        case ',':
                        case '}':
                        case ']': {
                            m_parserStatus = ParserStatus::FinishedValue;
                            FinishNumberValueIntegral();
                        } break;

                        case 'e':
                        case 'E': {
                            m_errorStatus  = ErrorStatus::ParseError_ExpectedDigitOrDecimalOrEndOfNumber;
                            m_parserStatus = ParserStatus::Done;
                            NotifyOfError("Exponents are not supported.");
                            break;
                        }

                        default: {
                            if (IsWhitespace(m_source[m_sourceIndex], true)) {
                                m_parserStatus = ParserStatus::FinishedValue;
                                FinishNumberValueIntegral();
                            }
                            else {
                                m_errorStatus  = ErrorStatus::ParseError_ExpectedDigitOrDecimalOrEndOfNumber;
                                m_parserStatus = ParserStatus::Done;
                                NotifyOfError(
                                    "Was reading integral digits in a number.  "
                                        "Expected more digits, decimal point, or "
                                        "end of number by \'}\', \']\', or \',\'."
                                );
                            }
                        } break;
                    } break; // end sub-switch
                } // end case

                case ParserStatus::NumberSawDecimalPoint: {
                    if (m_source[m_sourceIndex] < '0' || m_source[m_sourceIndex] > '9') {
                        m_errorStatus = ErrorStatus::ParserError_UnfinishedFractionalNumber;
                        m_parserStatus = ParserStatus::Done;
                        NotifyOfError(
                            "Fractional numbers must have digits after the decimal point.  "
                                "So \"0.\" is not valid, but \"0.0\" is."
                        );
                        break;
                    }

                    ContinueNumberValue_ReadingFractionalDigits();
                    m_parserStatus = ParserStatus::NumberReadingFractionalDigits;
                } break;

                case ParserStatus::NumberReadingFractionalDigits: {
                    if (IsWhitespace(m_source[m_sourceIndex], true) || m_source[m_sourceIndex]) {
                        FinishNumberValueWithFractional();
                        break;
                    }
                    else if (
                        (m_source[m_sourceIndex] < '0' || m_source[m_sourceIndex] > '9') &&
                        m_source[m_sourceIndex] != ']' &&
                        m_source[m_sourceIndex] != '}' &&
                        m_source[m_sourceIndex] != ','
                    ) {
                        m_errorStatus  = ErrorStatus::ParseError_ExpectedDigitOrEndOfNumber;
                        m_parserStatus = ParserStatus::Done;
                        NotifyOfError(
                            "Fractional portion of number terminated incorrectly.  "
                                "Should end in whitespace, array-finishing ']', object-finishing '}', "
                                "or value-separating ','."
                        );
                        break;
                    }
                    ContinueNumberValue_ReadingFractionalDigits();
                } break;

                case ParserStatus::ReadingTrueValue: {
                    if (m_tempDataIndex < 4)
                        ContinueTrueValue();
                    else
                        FinishTrueValue();
                } break;

                case ParserStatus::ReadingFalseValue: {
                    if (m_tempDataIndex < 4)
                        ContinueFalseValue();
                    else
                        FinishFalseValue();
                } break;

                //case ParserStatus::ReadingFalseValue:
                //break;

                case ParserStatus::ReadingNullValue: {
                    if (m_tempDataIndex < 4)
                        ContinueNullValue();
                    else
                        FinishNullValue();
                } break;

                // Just finished a value (of any kind).  We should see...
                //   If currently in an object:
                //    - whitespace that can be ignored.
                //    - value-separating comma.
                //    - object-terminating curly brace.
                //   If currently in an array:
                //    - whitespace that can be ignored.
                //    - value-separating comma.
                //    - array-terminating square bracket.
                case ParserStatus::FinishedValue: {
                    SkipWhitespace(true);
                    if (m_sourceIndex >= m_sourceSize)
                        break;
                    // value-separating comma?
                    if (m_source[m_sourceIndex] == ',') {
                        ClearNameAndDataBuffers();
                        ++m_sourceIndex;
                        ++m_currentColumn;
                        if (m_objectTypeStack[m_objectTypeStackIndex - 1] == true)
                            m_parserStatus = ParserStatus::NeedAnotherDataElement_InObject;
                        else
                            m_parserStatus = ParserStatus::NeedAnotherDataElement_InArray;
                    }
                    // object-terminating curly brace?
                    else if (m_source[m_sourceIndex] == '}') {
                        EndObject();
                    }
                    // array-terminating square bracket?
                    else if (m_source[m_sourceIndex] == ']') {
                        EndArray();
                    }
                    // otherwise, unexpected invalid data
                    else {
                        m_errorStatus  = ErrorStatus::ParseError_ExpectedValueSeparatorOrEndOfContainer;
                        m_parserStatus = ParserStatus::Done;
                        // were we in an object?
                        if (m_objectTypeStack[m_objectTypeStackIndex - 1] == true) {
                            NotifyOfError(
                                "Every name-value pair in an object must be followed by either the name-value "
                                    "separating comma (,), or the termination of the containing object (})."
                            );
                        }
                        // otherwise, we were in an array
                        else {
                            NotifyOfError(
                                "Every value in an array must be followed by either the value separating "
                                    "comma (,), or the termination of the containing array (])"
                            );
                        }
                    }
                } break;

                // Just got a value-separating comma.  Only valid things are...
                //    - Skippable whitespace.
                //    - Name-starting double quotes.
                case ParserStatus::NeedAnotherDataElement_InObject: {
                    SkipWhitespace(true);
                    if (m_sourceIndex >= m_sourceSize)
                        break;
                    // all object fields have names, or it could be an array's string
                    if (m_source[m_sourceIndex] == '"')
                        BeginName();
                    // can't end the object when we're looking for another value
                    else if (m_source[m_sourceIndex] == '}') {
                        m_parserStatus = ParserStatus::Done;
                        m_errorStatus  = ErrorStatus::ParseError_ExpectedString;
                        NotifyOfError(
                            "Ended object too early.  Already saw a comma, which means another item is expected.  "
                                "Like this: { \"item1\" : 1, \"item2\" : 2 }"
                        );
                    }
                    // nothing else is valid
                    else {
                        m_parserStatus = ParserStatus::Done;
                        m_errorStatus = ErrorStatus::ParseError_ExpectedString;
                        NotifyOfError(
                            "After a value-separating comma in an object, the next element must be a name-value pair.  "
                                "Like this: { \"foo\" : 1, \"bar\" : 2 }"
                        );
                    }
                } break;

                // TODO: Check for more data, and error if more is encountered.
                //   More data after all is finished probably means the user has
                //   mis-matching braces.  Or more than one root object.
                case ParserStatus::FinishedAllData: {
                    m_sourceIndex  = m_sourceSize;
                    m_parserStatus = ParserStatus::Done;
                } break;
            } // end case ParserStatus::BeganObject:
        } // end switch (m_parserStatus)
    } // end while (parser status, etc.)

    if (m_errorStatus == ErrorStatus::NotStarted) {
        m_errorStatus  = ErrorStatus::Done;
        m_parserStatus = ParserStatus::Done;
    }

    return m_errorStatus < ErrorStatus::Error_Unspecified;
}

//=========================================================================
void JsonParser::Reset () {
    m_errorStatus   = ErrorStatus::NotStarted;
    m_parserStatus  = ParserStatus::NotStarted;
    m_tempName[0]   = '\0';
    m_tempData[0]   = '\0';
    m_tempNameIndex = 0;
    m_tempDataIndex = 0;
    //m_dataCallback  = nullptr;
    m_sourceSize    = 0;
    m_sourceIndex   = 0;

    m_currentRow    = 1;
    m_currentColumn = 1;

    m_objectTypeStackIndex = 0;
}

//=========================================================================
void JsonParser::NotifyOfError (const char * message) {
    fprintf(stderr, "  JsonParser error: (row %d, col %d)\n", m_currentRow, m_currentColumn);
    // self-described error
    if (message)
        fprintf(stderr, "%s\nJsonParser Status: ", message);
    // status-based message
    switch (m_errorStatus) {
        case ErrorStatus::Error_CantAccessData: {
            fprintf(stderr, "Can't access data.");
        } break;
    }
    fprintf(stderr, "\n\n");
}

//=========================================================================
bool JsonParser::IsWhitespace (char c, bool newlinesCount) const {
    if (newlinesCount) {
        switch (c) {
            case ' ':
            case 0x09: // TAB (horizontal tab)
            case 0x0A: // LF  (NL line feed, new line)
            case 0x0D: // CR  (carriage return)
                return true;
        }
        // break not needed
    }
    // if newlines don't count, only space and TAB are accepted
    else if (c == ' ' || c == 0x09) {
        return true;
    }

    return false;
}

//=========================================================================
void JsonParser::SkipWhitespace (bool alsoSkipNewlines) {
    while (m_sourceIndex < m_sourceSize && IsWhitespace(m_source[m_sourceIndex], alsoSkipNewlines)) {
        if (m_source[m_sourceIndex] == '\n') {
            ++m_currentRow;
            m_currentColumn = 1;
        }
        else
            ++m_currentColumn;

        ++m_sourceIndex;
    }
}

//=========================================================================
void JsonParser::BeginObject () {
    //SkipWhitespace(true);
    // if we have the right character, it's okay to begin the object
    //if (m_source[m_sourceIndex] == '{') {
        // update internal status
        m_parserStatus = ParserStatus::BeganObject;
        ++m_sourceIndex;
        ++m_currentColumn;
        // object stack tracking
        m_objectTypeStack[m_objectTypeStackIndex] = true;
        ++m_objectTypeStackIndex;
        // callback, if such is available
        if (m_dataCallback)
            m_dataCallback->BeginObject(m_tempName, m_tempNameIndex);
        //return true;
    //}

    //return false;
}

//=========================================================================
void JsonParser::EndObject () {
    // if we're not in an object, someone ended an array with the wrong thing.
    if (m_objectTypeStack[m_objectTypeStackIndex - 1] == false) {
        m_parserStatus = ParserStatus::Done;
        m_errorStatus  = ErrorStatus::ParseError_ExpectedEndOfArray;
        NotifyOfError(
            "Array terminated improperly (used curly brace).  Use the square bracket to do so instead.  "
                "Like this: [ 8, 16 ]"
        );
        return;
    }

    --m_objectTypeStackIndex;
    // if we've run the stack out, all data is now finished.  We have a special
    //   state for this, other than kDone.  This is so if more data is
    //   encountered after, we can warn the user of mis-matching braces.
    if (m_objectTypeStackIndex == 0) {
        m_parserStatus = ParserStatus::FinishedAllData;
        m_errorStatus = ErrorStatus::Done;
    }
    else
        m_parserStatus = ParserStatus::FinishedValue;

    ++m_sourceIndex;
    ++m_currentColumn;

    // callback, if such is available
    if (m_dataCallback)
        m_dataCallback->EndObject();
}

//=========================================================================
void JsonParser::BeginArray () {
    // update internal status
    m_parserStatus = ParserStatus::BeganArray;
    ++m_sourceIndex;
    ++m_currentColumn;
    // object stack tracking
    m_objectTypeStack[m_objectTypeStackIndex] = false;
    ++m_objectTypeStackIndex;
    // callback, if such is available
    if (m_dataCallback)
        m_dataCallback->BeginArray(m_tempName, m_tempNameIndex);

    ClearNameAndDataBuffers();
}

//=========================================================================
void JsonParser::EndArray () {
    // if we're not in an array, someone ended an object with the wrong thing.
    if (m_objectTypeStack[m_objectTypeStackIndex - 1] == true) {
        m_parserStatus = ParserStatus::Done;
        m_errorStatus  = ErrorStatus::ParseError_ExpectedEndOfObject;
        NotifyOfError(
            "Object terminated improperly (used square bracket).  Use the curly brace to do so instead.  "
                "Like this: { \"foo\": 8 }"
        );
        return;
    }

    --m_objectTypeStackIndex;
    // if we've run the stack out, all data is now finished, but something is
    //   very wrong.  The root container must be an object, not an array.
    if (m_objectTypeStackIndex == 0) {
        m_parserStatus = ParserStatus::Done;
        m_errorStatus  = ErrorStatus::ParseError_BadStructure;
        NotifyOfError(
            "Encountered end of all data, but root-most object was an array.  Should have been an object.  "
                "How did you even get to this state?"
        );
        return;
    }
    else
        m_parserStatus = ParserStatus::FinishedValue;

    ++m_sourceIndex;
    ++m_currentColumn;

    // callback, if such is available
    if (m_dataCallback)
        m_dataCallback->EndArray();
}

//=========================================================================
void JsonParser::BeginName () { 
    // update internal status
    m_parserStatus  = ParserStatus::ReadingName;
    m_tempNameIndex = 0;

    // get past the opening double-quote
    ++m_sourceIndex;
    ++m_currentColumn;
}

//=========================================================================
void JsonParser::ContinueName () {
    size_t nameLen = 0;
    // read name, while watching for both end of buffer, and end of string
    while (
        m_sourceIndex + nameLen < m_sourceSize    &&
        m_source[m_sourceIndex + nameLen] != '\\' &&
        m_source[m_sourceIndex + nameLen] != '"'
    ) {
        ++nameLen;
    }

    // copy found name into temp buffer for holding.
    //   Being careful not to overflow our internal buffer.
    size_t copyAmount = nameLen;
    if (m_tempNameIndex + copyAmount >= s_maxNameLength)
        copyAmount = s_maxNameLength - m_tempNameIndex;
    // now copy
    memcpy(m_tempName + m_tempNameIndex, m_source + m_sourceIndex, copyAmount);
    m_tempNameIndex += copyAmount;

    if (m_source[m_sourceIndex + nameLen] == '\\') {
        m_parserStatus = ParserStatus::ReadingName_EscapedChar;
        // skip past the escape sequence-initiating backslash
        ++m_sourceIndex;
    }

    m_sourceIndex += nameLen;
    m_currentColumn += nameLen;
}

//=========================================================================
void JsonParser::FinishName () {
    m_tempName[m_tempNameIndex] = '\0';
    m_parserStatus = ParserStatus::FinishedName;
    ++m_sourceIndex;
    ++m_currentColumn;
}

//=========================================================================
void JsonParser::BeginStringValue () {
    // update internal status
    m_parserStatus  = ParserStatus::ReadingStringValue;
    m_tempDataIndex = 0;
    // get past the opening double-quote
    ++m_sourceIndex;
    ++m_currentColumn;
}

//=========================================================================
void JsonParser::ContinueStringValue () {
    size_t dataLen = 0;
    // read string value, while watching for both end of buffer,
    //   and end of string
    while (
        m_sourceIndex + dataLen < m_sourceSize    &&
        m_source[m_sourceIndex + dataLen] != '\\' &&
        m_source[m_sourceIndex + dataLen] != '"'
    ) {
        ++dataLen;
    }

    // copy found string into temp buffer for holding
    //   Being careful not to overflow our internal buffer.
    size_t copy_amount = dataLen;
    if (m_tempDataIndex + copy_amount >= s_maxStringLength)
        copy_amount = s_maxStringLength - m_tempDataIndex;

    memcpy(m_tempData + m_tempDataIndex, m_source + m_sourceIndex, copy_amount);
    m_tempDataIndex += copy_amount;

    if (m_source[m_sourceIndex + dataLen] == '\\') {
        m_parserStatus = ParserStatus::ReadingStringValue_EscapedChar;
        // skip past the escape sequence-initiating backslash
        ++m_sourceIndex;
    }

    m_sourceIndex += dataLen;
    m_currentColumn += dataLen;
}

//=========================================================================
void JsonParser::HandleEscapedCharacter () {
    char special_char = '\0';

    switch (m_source[m_sourceIndex]) {
        case '"':
        case '\\':
        case '/': special_char = m_source[m_sourceIndex]; break;

        // backspace
        case 'b': special_char = 0x08; break;
        // formfeed
        case 'f': special_char = 0x0C; break;

        // newline
        case 'n': special_char = 0x0A; break;
        // carriage return
        case 'r': special_char = 0x0D; break;

        // horizontal tab
        case 't': special_char = 0x09; break;

        // unicode (u is followed by 4 hexadecimal digits
        case 'u': {
            m_parserStatus = ParserStatus::Done;
            m_errorStatus  = ErrorStatus::ParseError_SixCharacterEscapeSequenceNotYetSupported;
            NotifyOfError("Six-character escape sequences are not yet supported.  An example of this is \"\\u005C\".");
        } return;

        default: {
            m_parserStatus = ParserStatus::Done;
            m_errorStatus  = ErrorStatus::ParseError_InvalidEscapedCharacter;
            NotifyOfError(
                "Invalid escaped character.  "
                    "The only valid ones are \\\", \\\\, \\/, \\b, \\f, \\n, \\r, \\t.  "
                    "\\uXXXX is also not yet supported."
            );
        } return;
    }

    if (m_parserStatus == ParserStatus::ReadingName_EscapedChar) {
        if (m_tempNameIndex + 1 < s_maxNameLength) {
            *(m_tempName + m_tempNameIndex) = special_char;
            ++m_tempNameIndex;
        }

        m_parserStatus = ParserStatus::ReadingName;
    }
    // reading string value with an escaped character
    else {
        if (m_tempDataIndex + 1 < s_maxStringLength) {
            *(m_tempData + m_tempDataIndex) = special_char;
            ++m_tempDataIndex;
        }

        m_parserStatus = ParserStatus::ReadingStringValue;
    }

    ++m_sourceIndex;
    ++m_currentColumn;
}

//=========================================================================
void JsonParser::FinishStringValue () {
    m_tempData[m_tempDataIndex] = '\0';
    m_parserStatus = ParserStatus::FinishedValue;
    ++m_sourceIndex;
    ++m_currentColumn;
    // notify user of new data.  Doesn't matter if we're in an object or an
    //   array, since m_tempName will appropriately be pointing at an empty
    //   string (not NULL pointer, but empty string) iff we're in an array.
    if (m_dataCallback)
        m_dataCallback->GotString(m_tempName, m_tempNameIndex, m_tempData, m_tempDataIndex);
}

//=========================================================================
void JsonParser::BeginNumberValue_AtLeadingNegative () {
    // internal status already updated by caller (parse buffer)
    m_tempData[0] = '-';
    m_tempDataIndex = 1;
    //ContinueNumberValue_AfterLeadingNegative();
    ++m_sourceIndex;
    ++m_currentColumn;
}

//=========================================================================
void JsonParser::BeginNumberValue_AtLeadingZero () {
    // internal status already updated by caller (parse buffer)
    m_tempData[0] = '0';
    m_tempDataIndex = 1;
    //ContinueNumberValue_AfterLeadingZero();
    ++m_sourceIndex;
    ++m_currentColumn;
}

//=========================================================================
void JsonParser::BeginNumberValue_AtNormalDigit () {
    // internal status already updated by caller (parse buffer)
    m_tempData[0] = m_source[m_sourceIndex];
    m_tempDataIndex = 1;
    //ContinueNumberValue_ReadingWholeDigits();
}

//=========================================================================
void JsonParser::ContinueNumberValue_AfterLeadingNegative () {
    switch (m_source[m_sourceIndex]) {
        case '0': {
            m_parserStatus  = ParserStatus::NumberSawLeadingZero;
            m_tempData[1]   = '0';
            m_tempDataIndex = 2;
            ++m_sourceIndex;
            ++m_currentColumn;
        } break;

        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            m_parserStatus  =  ParserStatus::NumberReadingWholeDigits;
            m_tempData[1]   = m_source[m_sourceIndex];
            m_tempDataIndex = 2;
            ++m_sourceIndex;
            ++m_currentColumn;
        } break;

        default: {
            m_parserStatus = ParserStatus::Done;
            m_errorStatus  = ErrorStatus::ParseError_ExpectedDigit;
            NotifyOfError("Expected 0-9 digit while reading number (just read leading '-' sign).");
        } break;
    } // end switch
}

//=========================================================================
void JsonParser::ContinueNumberValue_AfterLeadingZero () {
    switch (m_source[m_sourceIndex]) {
        case '.': {
            m_parserStatus = ParserStatus::NumberSawDecimalPoint;
            m_tempData[m_tempDataIndex] = '0';
            ++m_tempDataIndex;
            m_tempData[m_tempDataIndex] = '.';
            ++m_tempDataIndex;
            ++m_sourceIndex;
            ++m_currentColumn;
        } break;

        case ',':
        case '}':
        case ']':
        case ' ': {
            m_parserStatus = ParserStatus::FinishedValue;
            FinishNumberValueZero();
        } break;

        default: {
            if (IsWhitespace(m_source[m_sourceIndex], true)) {
                m_parserStatus = ParserStatus::FinishedValue;
                FinishNumberValueZero();
            }
            else {
                m_parserStatus = ParserStatus::Done;
                m_errorStatus  = ErrorStatus::ParseError_ExpectedDecimalOrEndOfNumber;
                NotifyOfError("Expected either decimal point, or end of number, after the leading digit was a zero.");
            }
        } break;
    } // end switch
}

//=========================================================================
void JsonParser::ContinueNumberValue_ReadingWholeDigits () {
    size_t dataLen = 0;
    // read number value, while watching for both end of buffer,
    //   and end of whole digits
    while (
        m_sourceIndex + dataLen < m_sourceSize   &&
        m_source[m_sourceIndex + dataLen] >= '0' &&
        m_source[m_sourceIndex + dataLen] <= '9'
    ) {
        ++dataLen;
    }

    // copy found number string into temp buffer for holding.
    //   Being careful not to overflow our internal buffer.
    size_t copy_amount = dataLen;
    if (m_tempNameIndex + copy_amount >= s_maxStringLength)
        copy_amount = s_maxStringLength - m_tempNameIndex;

    memcpy(m_tempData + m_tempDataIndex, m_source + m_sourceIndex, copy_amount);
    m_tempDataIndex += copy_amount;

    m_sourceIndex   += dataLen;
    m_currentColumn += dataLen;
}

//=========================================================================
void JsonParser::ContinueNumberValue_ReadingFractionalDigits () {
    size_t dataLen = 0;
    // read number value, while watching for both end of buffer,
    //   and end of whole digits
    while (
        m_sourceIndex + dataLen < m_sourceSize   &&
        m_source[m_sourceIndex + dataLen] >= '0' &&
        m_source[m_sourceIndex + dataLen] <= '9'
    ) {
        ++dataLen;
    }

    // copy found number string into temp buffer for holding.
    //   Being careful not to overflow our internal buffer.
    size_t copyAmount = dataLen;
    if (m_tempNameIndex + copyAmount >= s_maxStringLength)
        copyAmount = s_maxStringLength - m_tempNameIndex;

    memcpy(m_tempData + m_tempDataIndex, m_source + m_sourceIndex, copyAmount);
    m_tempDataIndex += copyAmount;

    m_sourceIndex   += dataLen;
    m_currentColumn += dataLen;
}

//=========================================================================
void JsonParser::FinishNumberValueZero () {
    /*
    // we've already read "true", now check that the character immediately after
    //   it is valid (ie. whitespace, name-value separator, etc.)  As opposed
    //   to being a second 'e' character, a number, or anything like that.
    if (
        !IsWhitespace(m_source[m_sourceIndex], true) &&
        m_source[m_sourceIndex] != '}'               &&
        m_source[m_sourceIndex] != ']'               &&
        m_source[m_sourceIndex] != ','
    ) {
        m_errorStatus  = ErrorStatus::ParseError_BadValue;
        m_parserStatus = ParserStatus::Done;
        NotifyOfError("Typo found after reading \"true\" value.");
        return;
    }
    //*/

    m_parserStatus = ParserStatus::FinishedValue;
    if (m_dataCallback)
        m_dataCallback->GotInteger(m_tempName, m_tempNameIndex, 0);
}

//=========================================================================
void JsonParser::FinishNumberValueIntegral () {
    m_parserStatus = ParserStatus::FinishedValue;
    if (m_dataCallback) {
        int value = 0;
        /*
        int exponent = 1;
        // use all but first character.  We'll check that one for a negative sign
        //   separately after.
        for (int i = m_tempDataIndex - 1; i > 0; --i) {
            value    += (m_tempData[i] - '0') * exponent;
            exponent *= 10;
        }

        // check if first character is negative sign, or just another digit
        if (m_tempData[0] == '-')
            value *= -1;
        else
            value += (m_tempData[0] - '0') * exponent;
        //*/

        m_tempData[m_tempDataIndex] = '\0';
        value = atoi(m_tempData);

        m_dataCallback->GotInteger(m_tempName, m_tempNameIndex, value);
    }
}

//=========================================================================
void JsonParser::FinishNumberValueWithFractional () {
    m_parserStatus = ParserStatus::FinishedValue;
    if (m_dataCallback) {
        float value = 0;
        /*
        int exponent = 1;
        // use all but first character.  We'll check that one for a negative sign
        //   separately after.
        for (int i = m_tempDataIndex - 1; i > 0; --i) {
            value    += (m_tempData[i] - '0') * exponent;
            exponent *= 10;
        }

        // check if first character is negative sign, or just another digit
        if (m_tempData[0] == '-')
            value *= -1;
        else
            value += (m_tempData[0] - '0') * exponent;
        //*/

        m_tempData[m_tempDataIndex] = '\0';
        value = static_cast<float>( atof(m_tempData) );

        m_dataCallback->GotFloat(m_tempName, m_tempNameIndex, value);
    }
}

//=========================================================================
void JsonParser::BeginTrueValue () {
    // update internal status
    m_parserStatus = ParserStatus::ReadingTrueValue;
    // temp data index will be used to point into our static 'true' array, to
    //   track which character we need next
    m_tempDataIndex = 1;
    // leave m_sourceIndex where it is; we'll walk it along in ContinueTrueValue
    //   as we see more character matches for the 'null' keyword
    ContinueTrueValue();
}

//=========================================================================
void JsonParser::ContinueTrueValue () {
    static const char true_value[] = "true";

    // read true value, while watching for both end of buffer,
    //   and end of 'true' string
    while (m_sourceIndex + m_tempDataIndex < m_sourceSize && m_tempDataIndex < 4) {
        // check if the given characters match the 'true' keyword
        if (m_source[m_sourceIndex + m_tempDataIndex] != true_value[m_tempDataIndex]) {
            m_parserStatus = ParserStatus::Done;
            m_errorStatus  = ErrorStatus::ParseError_ExpectedContinuationOfTrueKeyword;

            char temp_buf[64] = {'\0'};
            // TODO: Ensure this sprintf cannot possibly be exploited by bad data.
            sprintf(temp_buf, "Expected '%c' in \"true\" keyword.", true_value[m_tempDataIndex]);
            // stupid-human use of sprintf extra safety (prevents over-read later,
            //   but not over-write from the line above)
            temp_buf[sizeof(temp_buf) / sizeof(temp_buf[0]) - 1] = '\0';
            NotifyOfError(temp_buf);
            return;
        }

        ++m_tempDataIndex;
        ++m_currentColumn;
    }

    m_sourceIndex += m_tempDataIndex;
}

//=========================================================================
void JsonParser::FinishTrueValue () {
    // we've already read "true", now check that the character immediately after
    //   it is valid (ie. whitespace, name-value separator, etc.)  As opposed
    //   to being a second 'e' character, a number, or anything like that.
    if (!IsWhitespace(m_source[m_sourceIndex], true) &&
        m_source[m_sourceIndex] != '}'               &&
        m_source[m_sourceIndex] != ']'               &&
        m_source[m_sourceIndex] != ','
    ) {
        m_errorStatus  = ErrorStatus::ParseError_BadValue;
        m_parserStatus = ParserStatus::Done;
        NotifyOfError("Typo found after reading \"true\" value.");
        return;
    }

    m_parserStatus = ParserStatus::FinishedValue;
    if (m_dataCallback)
        m_dataCallback->GotBoolean(m_tempName, m_tempNameIndex, true);
}

//=========================================================================
void JsonParser::BeginFalseValue () {
    // update internal status
    m_parserStatus = ParserStatus::ReadingFalseValue;
    // temp data index will be used to point into our static 'false' array, to
    //   track which character we need next
    m_tempDataIndex = 1;
    // leave m_sourceIndex where it is; we'll walk it along in ContinueFalseValue
    //   as we see more character matches for the 'null' keyword
    ContinueFalseValue();
}

//=========================================================================
void JsonParser::ContinueFalseValue () {
    static const char false_value[] = "false";

    // read false value, while watching for both end of buffer,
    //   and end of 'false' string
    while (m_sourceIndex + m_tempDataIndex < m_sourceSize && m_tempDataIndex < 5) {
        // check if the given characters match the 'false' keyword
        if (m_source[m_sourceIndex + m_tempDataIndex] != false_value[m_tempDataIndex]) {
            m_parserStatus = ParserStatus::Done;
            m_errorStatus  = ErrorStatus::ParseError_ExpectedContinuationOfFalseKeyword;
            char temp_buf[64] = {'\0'};
            // TODO: Ensure this sprintf cannot possibly be exploited by bad data.
            sprintf(temp_buf, "Expected '%c' in \"false\" keyword.",
            false_value[m_tempDataIndex]);
            // stupid-human use of sprintf extra safety (prevents over-read later,
            //   but not over-write from the line above)
            temp_buf[sizeof(temp_buf) / sizeof(temp_buf[0]) - 1] = '\0';
            NotifyOfError(temp_buf);
            return;
        }

        ++m_tempDataIndex;
        ++m_currentColumn;
    }

    m_sourceIndex += m_tempDataIndex;
}

//=========================================================================
void JsonParser::FinishFalseValue () {
    // we've already read "false", now check that the character immediately after
    //   it is valid (ie. whitespace, name-value separator, etc.)  As opposed
    //   to being a second 'e' character, a number, or anything like that.
    if (!IsWhitespace(m_source[m_sourceIndex], true) &&
        m_source[m_sourceIndex] != '}'               &&
        m_source[m_sourceIndex] != ']'               &&
        m_source[m_sourceIndex] != ','
    ) {
        m_errorStatus  = ErrorStatus::ParseError_BadValue;
        m_parserStatus = ParserStatus::Done;
        NotifyOfError("Typo found after reading \"false\" value.");
        return;
    }

    m_parserStatus = ParserStatus::FinishedValue;
    if (m_dataCallback)
        m_dataCallback->GotBoolean(m_tempName, m_tempNameIndex, false);
}

//=========================================================================
void JsonParser::BeginNullValue () {
    // update internal status
    m_parserStatus = ParserStatus::ReadingNullValue;
    // temp data index will be used to point into our static 'null' array, to
    //   track which character we need next
    m_tempDataIndex = 1;
    // leave m_sourceIndex where it is; we'll walk it along in ContinueNullValue
    //   as we see more character matches for the 'null' keyword
    ContinueNullValue();
}

//=========================================================================
void JsonParser::ContinueNullValue () {
    static const char null_value[] = "null";

    // read null value, while watching for both end of buffer,
    //   and end of 'null' string
    while (m_sourceIndex + m_tempDataIndex < m_sourceSize && m_tempDataIndex < 4) {
        // check if the given characters match the 'null' keyword
        if (m_source[m_sourceIndex + m_tempDataIndex] != null_value[m_tempDataIndex]) {
            m_parserStatus = ParserStatus::Done;
            m_errorStatus  = ErrorStatus::ParseError_ExpectedContinuationOfNullKeyword;
            char temp_buf[64] = {'\0'};
            // TODO: Ensure this sprintf cannot possibly be exploited by bad data.
            sprintf(temp_buf, "Expected '%c' in \"null\" keyword.",
            null_value[m_tempDataIndex]);
            // stupid-human use of sprintf extra safety (prevents over-read later,
            //   but not over-write from the line above)
            temp_buf[sizeof(temp_buf) / sizeof(temp_buf[0]) - 1] = '\0';
            NotifyOfError(temp_buf);
            return;
        }

        ++m_tempDataIndex;
        ++m_currentColumn;
    }

    m_sourceIndex += m_tempDataIndex;
}

//=========================================================================
void JsonParser::FinishNullValue () {
    // we've already read "null", now check that the character immediately after
    //   it is valid (ie. whitespace, name-value separator, etc.)  As opposed
    //   to being a third 'l' character, a number, or anything like that.
    if (!IsWhitespace(m_source[m_sourceIndex], true) &&
        m_source[m_sourceIndex] != '}'               &&
        m_source[m_sourceIndex] != ']'               &&
        m_source[m_sourceIndex] != ','
    ) {
        m_errorStatus  = ErrorStatus::ParseError_BadValue;
        m_parserStatus = ParserStatus::Done;
        NotifyOfError("Typo found after reading \"null\" value.");
        return;
    }

    m_parserStatus = ParserStatus::FinishedValue;
    if (m_dataCallback)
        m_dataCallback->GotNull(m_tempName, m_tempNameIndex);
}

//=========================================================================
void JsonParser::ClearNameAndDataBuffers () {
    m_tempName[0] = '\0';
    m_tempNameIndex = 0;
    m_tempData[0] = '\0';
    m_tempDataIndex = 0;
}

} // namespace CSaruJson

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
