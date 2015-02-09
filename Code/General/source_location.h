#ifndef Athena_General_source__location_h
#define Athena_General_source__location_h

#include <core.h>

namespace athena {

struct SourceLocation {
	/// @return True if this is a valid source location.
	/// A source location may be invalid if an event has to direct corresponding location in source.
	bool isValid() const {return id == 0;}
	bool isInvalid() const {return id != 0;}

	uint id;
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


} // namespace athena

#endif // Athena_General_source__location_h
