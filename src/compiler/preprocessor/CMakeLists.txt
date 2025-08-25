# ANGLE Preprocessor CMakeLists.txt

set(PREPROCESSOR_SOURCES
    DiagnosticsBase.cpp
    DiagnosticsBase.h
    DirectiveHandlerBase.cpp
    DirectiveHandlerBase.h
    DirectiveParser.cpp
    DirectiveParser.h
    ExpressionParser.h
    Input.cpp
    Input.h
    Lexer.cpp
    Lexer.h
    Macro.cpp
    Macro.h
    MacroExpander.cpp
    MacroExpander.h
    Preprocessor.cpp
    Preprocessor.h
    SourceLocation.h
    Token.cpp
    Token.h
    Tokenizer.h
    numeric_lex.h
    preprocessor_lex_autogen.cpp
    preprocessor_tab_autogen.cpp
)

add_library(angle_preprocessor EXCLUDE_FROM_ALL ${PREPROCESSOR_SOURCES})

target_include_directories(angle_preprocessor PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common/base
    ${CMAKE_SOURCE_DIR}/src/common/third_party/xxhash
    ${CMAKE_SOURCE_DIR}/third_party/abseil-cpp
)

target_compile_features(angle_preprocessor PUBLIC cxx_std_20)