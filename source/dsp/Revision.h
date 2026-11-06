#pragma once

namespace iem1176 {

// Historical letter codes only. The shipped product is Rev D.
// No colloquial panel-color nickname is used anywhere in this tree.
enum class Revision { RevA = 0, RevD = 1, RevE = 2, RevF = 3, RevG = 4, LN = 5 };

inline const char* revisionId(Revision r) {
  switch (r) {
    case Revision::RevA:
      return "RevA";
    case Revision::RevD:
      return "RevD";
    case Revision::RevE:
      return "RevE";
    case Revision::RevF:
      return "RevF";
    case Revision::RevG:
      return "RevG";
    case Revision::LN:
      return "LN";
  }
  return "RevD";
}

inline const char* revisionLabel(Revision r) {
  switch (r) {
    case Revision::RevA:
      return "Rev A";
    case Revision::RevD:
      return "Rev D";
    case Revision::RevE:
      return "Rev E";
    case Revision::RevF:
      return "Rev F";
    case Revision::RevG:
      return "Rev G";
    case Revision::LN:
      return "LN";
  }
  return "Rev D";
}

inline constexpr const char* kProductName = "IEM-1176 Rev D";
inline constexpr const char* kSemVer = "1.0.0";

}  // namespace iem1176
