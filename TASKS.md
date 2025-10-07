# Identified Follow-up Tasks

## Fix a Typo
- **Issue**: The instructions in `include/README` refer to the `#include` directive but close the inline code span with a stray apostrophe (`#include'`), leaving the directive formatted incorrectly.
- **Proposed Task**: Replace the trailing apostrophe with a closing backtick so the inline code reads `` `#include` ``.
- **Files Impacted**: `include/README`

## Fix a Bug
- **Issue**: `setup()` computes `int result = myFunction(2, 3);` but the value is never used. This leaves the firmware with no observable behavior and, under stricter compiler settings (e.g., `-Wall -Werror`), the build fails because of the unused variable.
- **Proposed Task**: Either remove the unused variable or use it meaningfully (for example, log it or feed it into whatever initialization logic the firmware eventually needs) so the compiled sketch has an observable side effect and remains warning-free.
- **Files Impacted**: `src/main.cpp`

## Fix a Documentation Discrepancy
- **Issue**: The top-level README promises the project will "Control a Mitsubishi AC unit using an ESP32 module," but the only implementation is a placeholder function that adds two integers and does nothing with the result.
- **Proposed Task**: Update the README to describe the current stub status or, preferably, expand `src/main.cpp` so the firmware actually interfaces with a Mitsubishi AC unit as advertised.
- **Files Impacted**: `README.md`, `src/main.cpp`

## Improve a Test
- **Issue**: The `test` directory contains only documentation scaffolding and no automated tests, so none of the existing logic is exercised.
- **Proposed Task**: Add a PlatformIO unit test (for example, verifying whatever real functionality replaces `myFunction`) so future changes are automatically checked.
- **Files Impacted**: `test/`
