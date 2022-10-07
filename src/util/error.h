#pragma once

#include <exception>

#include "types.h"

struct sqlite3;

struct Error : std::exception {
    const char* file;
    int line;
    String mess;
     // Keep this cached so what() doesn't return a temporary
    mutable String longmess;
    Error (const char* f, int l, String&& m) : file(f), line(l), mess(m) { }
    Error (const char* f, int l, Str m) : Error(f, l, String(m)) { }
    Error (String&& m) : Error("", 0, m) { }
    Error (Str mess) : Error("", 0, String(mess)) { }
    const char* what () const noexcept override;
};

bool intercept_throw (const std::exception&);

 // If tap::testing is false, this will first show
 // the user an "Abort/Retry/Continue" dialog.
 //   Abort: The program will terminate.
 //   Retry: This function will return without doing anything.
 //   Continue: This function will throw an Error.
 // Arguably these options aren't very intuitive, but it's not easy to customize
 // win32 dialogs.
 //
 // If tap::testing() is true, always throws the error.
template <class E>
void throw_ (E&& ex) {
    if (!intercept_throw(ex)) {
        throw std::move(ex);
    }
}

#define ERR [](auto mess){ throw_(Error(__FILE__, __LINE__, mess)); }
#ifndef NDEBUG
#define DERR ERR
#else
#define DERR [](auto mess){ }
#endif

String error_message_assert ();
String error_message_hresult (uint32);
String error_message_sqlite3 (sqlite3* db, int rc);
String error_message_win32 ();

 // Using lambdas instead of function-like macros, because some compilers
 // aren't very good at displaying line numbers when function-like macros are
 // involved.
#define AA [](auto&& res){ if (!res) DERR(error_message_assert()); }
#define AH [](uint32 hr){ if (hr != 0) DERR(error_message_hresult(hr)); }
#define AS [](sqlite3* db, int rc){ if (rc) DERR(error_message_sqlite3(db, rc)); }
#define AW [](auto res){ if (!res) DERR(error_message_win32()); }
#define AWE [](LSTATUS ec){ if (ec != 0) DERR(error_message_win32()); }

