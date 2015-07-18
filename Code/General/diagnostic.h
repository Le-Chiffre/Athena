#ifndef Athena_General_diagnostic_h
#define Athena_General_diagnostic_h

#include <core.h>
#include "source_location.h"

namespace athena {

using DiagID = U32;

struct DiagnosticConsumer;
struct DiagnosticBuilder;

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

	DiagnosticBuilder report(SourceLocation location, DiagID id);

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

	void addString(String string) {}
	void addVal(Size, DiagnosticArg kind) {}

private:
	Diagnostics& diag;
};

inline DiagnosticBuilder Diagnostics::report(SourceLocation, DiagID) {return {*this};}

struct Diagnostic {
	Diagnostic(const Diagnostics& diag) : diag(diag) {}

	const Diagnostics& diag;
};

struct DiagnosticConsumer {
	virtual ~DiagnosticConsumer() {}
	virtual void finish() {}
	virtual void handleDiagnostic(DiagnosticLevel level, const Diagnostic& diag) {}
};

} // namespace athena

#endif // Athena_General_diagnostic_h