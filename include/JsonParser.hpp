/*
Copyright (c) 2016 Christopher Higgins Barrett

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

#include <cstdio>

namespace CSaruJson {

class JsonParser {
public:
    // Types and Constants
    static const std::size_t s_maxNameLength = 28;
    static const std::size_t s_maxStringLength = 64;
    static const std::size_t s_maxDepth = 7;

    enum class ErrorStatus {
        NotStarted = 0,
        NotFinished,
        Done,

        // lowest actual error code.  If checking for error, check if status is
        //   greater-than-or-equal-to this code.
        Error_Unspecified,

        // No data buffer given, or no file given, or no DataMapMutator given.
        Error_CantAccessData,

        // Failed to read from the given file.
        Error_BadFileRead,

        // lowest parsing-based error.  This and above means your data is
        //   malformed.
        ParseError_Unspecified,

        ParseError_ExpectedBeginObject,
        ParseError_ExpectedEndOfObject,
        ParseError_ExpectedEndOfArray,
        ParseError_ExpectedString,
        ParseError_SixCharacterEscapeSequenceNotYetSupported,
        ParseError_InvalidEscapedCharacter,
        ParseError_ExpectedNameValueSeparator,
        ParseError_ExpectedValue,
        ParserError_PrematureDecimalPoint,
        ParserError_UnfinishedFractionalNumber,
        ParseError_ExpectedDigit,
        ParseError_ExpectedDecimalOrEndOfNumber,
        ParseError_ExpectedDigitOrDecimalOrEndOfNumber,
        ParseError_ExpectedDigitOrEndOfNumber,
        ParseError_ExpectedContinuationOfTrueKeyword,
        ParseError_ExpectedContinuationOfFalseKeyword,
        ParseError_ExpectedContinuationOfNullKeyword,
        ParseError_BadValue, // such as "nulll"
        ParseError_ExpectedValueSeparatorOrEndOfContainer,
        ParseError_BadStructure
    };

    enum class ParserStatus {
        NotStarted = 0,

        BeganObject,
        BeganArray,
        ReadingName,
        ReadingName_EscapedChar,
        FinishedName,
        SawNameValueSeparator,

        ReadingStringValue,
        ReadingStringValue_EscapedChar,

        NumberSawLeadingNegativeSign,
        NumberSawLeadingZero,
        NumberReadingWholeDigits,
        NumberSawDecimalPoint,
        NumberReadingFractionalDigits,

        ReadingTrueValue,
        ReadingFalseValue,
        ReadingNullValue,

        FinishedValue,
        NeedAnotherDataElement_InObject,
        NeedAnotherDataElement_InArray,

        Done,
        FinishedAllData
    };

    struct CallbackInterface {
        virtual ~CallbackInterface () {}

        virtual void BeginObject (const char * name, std::size_t name_len) = 0;
        virtual void EndObject () = 0;
        virtual void BeginArray (const char * name, std::size_t name_len) = 0;
        virtual void EndArray () = 0;
        virtual void GotString (const char * name, std::size_t name_len, const char * value, std::size_t value_len) = 0;
        virtual void GotFloat (const char * name, std::size_t name_len, float value) = 0;
        virtual void GotInteger (const char * name, std::size_t name_len, int value) = 0;
        virtual void GotBoolean (const char * name, std::size_t name_len, bool value) = 0;
        virtual void GotNull (const char * name, std::size_t name_len) = 0;
    };


private:
    // Data
    // all just for reading in from a file
    char        m_tempName[s_maxNameLength + 1];
    char        m_tempData[s_maxStringLength + 1];
    // always points at one-past-the-last element
    std::size_t m_tempNameIndex;
    std::size_t m_tempDataIndex;

    // holds true for objects, false for arrays.  Needed to keep proper track
    //   of what data has names, and what doesn't.
    bool        m_objectTypeStack[s_maxDepth];
    // points to one-past-the-last element we're using.
    std::size_t m_objectTypeStackIndex;

    ErrorStatus  m_errorStatus;
    ParserStatus m_parserStatus;

    CallbackInterface * m_dataCallback;

    std::size_t m_currentRow;
    std::size_t m_currentColumn;

    // parse-in-progress data
    const char * m_source;
    std::size_t  m_sourceSize;
    std::size_t  m_sourceIndex;

    // Helpers
    /*
    struct ParsingStates {
        enum Enum {
            kNone = 0,
            kParsingEntireFile
        };

        ParsingStates () = delete;
    };
    //*/

    void NotifyOfError (const char * message);

    bool IsWhitespace (char c, bool newlinesCount) const;

    void SkipWhitespace (bool alsoSkipNewlines);

    //
    // Parser worker functions
    //
    // Handle changes in state, temporary internal copies of data,
    //    and callbacks to the user.
    //

    void BeginObject ();
    void EndObject ();
    void BeginArray ();
    void EndArray ();

    void BeginName ();
    void ContinueName ();
    void FinishName ();

    // used by both name and data strings
    void HandleEscapedCharacter ();

    void BeginStringValue ();
    void ContinueStringValue ();
    void FinishStringValue ();

    void BeginNumberValue_AtLeadingNegative ();
    void BeginNumberValue_AtLeadingZero ();
    void BeginNumberValue_AtNormalDigit ();
    void ContinueNumberValue_AfterLeadingNegative ();
    void ContinueNumberValue_AfterLeadingZero ();
    void ContinueNumberValue_ReadingWholeDigits ();
    void ContinueNumberValue_ReadingFractionalDigits ();
    void FinishNumberValueZero ();
    void FinishNumberValueIntegral ();
    void FinishNumberValueWithFractional ();

    void BeginTrueValue ();
    void ContinueTrueValue ();
    void FinishTrueValue ();
    void BeginFalseValue ();
    void ContinueFalseValue ();
    void FinishFalseValue ();

    void BeginNullValue ();
    void ContinueNullValue ();
    void FinishNullValue ();

    // only called after the first value in an object/array.
    void ClearNameAndDataBuffers ();

public:
    // Methods
    JsonParser();

    // file [in/out]: Pointer to an already-opened file with read access in
    //   "translate" mode.
    // data_buffer [in/out]: If NULL, one will be allocated and freed
    //   automatically.  Its size will be the system page size.
    // fread_buffer_size_in_elements [in]: Number of elements in the given
    //   buffer for reading in data.
    // result [in/out]: Will hold the file starting at where it points.
    //   If the DataMapMutator was already pointing at some place inside a file,
    //   the file will be parsed into that location.
    // RETURN: true on success, false on failure.
    bool ParseEntireFile (
        std::FILE *         file,
        char *              freadBuffer,
        std::size_t         freadBufferSizeInElements,
        CallbackInterface * dataCallback
    );

    // PRE: If beginning on a new set of data, you must Reset() this first.
    bool ParseBuffer (const char * buffer, std::size_t bufferSize, CallbackInterface * dataCallback);

    // Use Reset before you parse different data.  Such as if you want to parse
    //   a totally different set of data; after a successful, failed, or
    //   (user-)canceled parse.
    void Reset ();

    inline ErrorStatus GetErrorCode () const           { return m_errorStatus; }
};

} // namespace CSaruJson
