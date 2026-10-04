#include "Diagnostic.h"

#include <sstream>

namespace fosc {

std::string formatDiagnostic(const Diagnostic& diagnostic)
{
  std::ostringstream output;
  output << "Error " << diagnostic.code << ": " << diagnostic.message << '\n';
  output << "File: " << diagnostic.filename << '\n';
  output << "Line " << diagnostic.location.line
         << ", Column " << diagnostic.location.column;
  return output.str();
}

}  // namespace fosc
