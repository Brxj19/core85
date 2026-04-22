#include "core/assembler.h"

namespace Core85 {

AssemblyResult Assembler::assemble(std::string_view source) const {
    AssemblyResult result;
    result.success = source.empty();

    if (!result.success) {
        result.errors.push_back(
            AssemblerError{1U, "", "Assembler implementation is pending Phase 3."});
    }

    return result;
}

}  // namespace Core85
