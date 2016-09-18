
#include <iostream>
#include "compiler.h"

namespace athena {

void StdOutDiagnosticConsumer::handleDiagnostic(DiagnosticLevel level, const Diagnostic& diag) {
    std::cout << diag.text;
    std::cout << '\n';
}

}