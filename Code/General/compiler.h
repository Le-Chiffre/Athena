#ifndef Athena_General_compiler_h
#define Athena_General_compiler_h

#include "types.h"
#include <string>

namespace athena {

struct CompileSettings {

};

struct DiagnosticConsumer;
struct DiagnosticBuilder;

struct SourceLocation {
    /// @return True if this is a valid source location.
    /// A source location may be invalid if an event has to direct corresponding location in source.
    bool isValid() const {return id == 0;}
    bool isInvalid() const {return id != 0;}

    U32 id;
};

inline bool operator == (SourceLocation a, SourceLocation b) {return a.id == b.id;}
inline bool operator != (SourceLocation a, SourceLocation b) {return a.id != b.id;}
inline bool operator >= (SourceLocation a, SourceLocation b) {return a.id >= b.id;}
inline bool operator <= (SourceLocation a, SourceLocation b) {return a.id <= b.id;}
inline bool operator >  (SourceLocation a, SourceLocation b) {return a.id >  b.id;}
inline bool operator <  (SourceLocation a, SourceLocation b) {return a.id <  b.id;}


struct SourceRange {
    bool isValid() const {return start.isValid() && end.isValid();}
    bool isInvalid() const {return !isValid();}

    SourceLocation start, end;
};

inline bool operator == (SourceRange a, SourceRange b) {return a.start == b.start && a.end == b.end;}
inline bool operator != (SourceRange a, SourceRange b) {return !(a == b);}

enum class DiagnosticLevel {
    Ignored,
    Note,
    Remark,
    Warning,
    Error,
    Fatal
};

enum class DiagnosticArg {
    Int,
    UInt,
};

struct Diagnostics {
    Diagnostics(DiagnosticConsumer& c) : consumer(c) {}

    template<int length, class... P>
    void error(const char (&text)[length], P&&... p) {

    }

    template<int length, class... P>
    void warning(const char (&text)[length], P&&... p) {

    }

    DiagnosticBuilder report(SourceLocation location, Id id);

    void report(DiagnosticBuilder& builder) {

    }

private:
    /// The consumer to send diagnostics to.
    DiagnosticConsumer& consumer;

    /// The total number of errors emitted.
    U32 errorCount = 0;

    /// The total number of warnings emitted.
    U32 warningCount = 0;

    /// Set to true after the first compilation error occurs.
    bool errorOccurred = false;

    /// Set to true after the first uncompilable error occurs (any error that was not promoted from a warning).
    bool uncompilableErrorOccurred = false;
};

struct DiagnosticBuilder {
    DiagnosticBuilder(Diagnostics& diag) : diag(diag) {}

    void addString(const std::string& string) {}
    void addVal(Size, DiagnosticArg kind) {}
    void report() {diag.report(*this);}

private:
    Diagnostics& diag;
};

inline DiagnosticBuilder Diagnostics::report(SourceLocation location, Id id) { return {*this}; }

struct Diagnostic {
    Diagnostic(const Diagnostics& diag, std::string&& text) : diag(diag), text(move(text)) {}

    const Diagnostics& diag;
    std::string text;
};

struct DiagnosticConsumer {
    virtual ~DiagnosticConsumer() {}
    virtual void finish() {}
    virtual void handleDiagnostic(DiagnosticLevel level, const Diagnostic& diag) {}
};

struct StdOutDiagnosticConsumer: DiagnosticConsumer {
    void handleDiagnostic(DiagnosticLevel level, const Diagnostic& diag) override;
};

} // namespace athena

#endif // Athena_General_compiler_h