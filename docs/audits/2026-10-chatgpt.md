# HC — defensive memory-safety review (ChatGPT, 22 October 2026)

**Repository:** `cleobuline/hc`, `main` at the start of this review: `de6449d51e01deb38aba7158dac61b922e6552ce`.  
**Scope:** first-party C, Cocoa, and the HC↔WASM ABI. Excludes `HC/tiers/`, `tests/attendu/`, `*.wasm`, style, comments, and refactoring.
**Execution status:** source review through the GitHub connector; **no local clone, compilation, regression suite, ASan/UBSan/LSan, or executable reproducer was run**. Findings below are not represented as confirmed crashes.

## Findings (severity medium and higher only)

### 1. Signed-length and address-space assumptions at the WASM graphics boundary
- **File:line:** `HC/hc_wasm.c:389-400` (`rappel_draw_xwindow`), `HC/hc_wasm.c:408-422` (`rappel_get_card_picture`), `HC/hc_wasm.c:441-456` (`rappel_drag_gray_rgn`).
- **Severity:** **high** (the consequence *could* be an out-of-bounds access at a downstream consumer; reachability is not established).
- **Status:** **NOT REPRODUCED** — source-level review only.
- **Scenario:** A stack embeds a hostile module that passes inconsistent or extreme dimensions and lengths. `DrawXWindow` checks `l*h` against `npx` and palette length, while `GetCardPicture` checks signed width × height against the reported length, and `DragGrayRgn` derives a count from `taille/16`. All ultimately forward native pointers into host-side operations. The initial guards are substantial, but a security claim requires checking that each downstream consumer uses no more bytes than the validated amount, never multiplies dimensions in a narrower integer type, and does not retain a pointer past the synchronous callback. In particular, `GetCardPicture` permits very large (but valid positive) differences of 32-bit endpoints before downstream rendering logic is consulted. This is an **audit gap, not proof of an actual invalid access**.
- **Suggested fix:** Establish a single documented maximum for each image/region, check total byte counts with checked `size_t` multiplication and enforce it at the host consumer. For the graphics callbacks, run a hostile-module harness with zero, negative, maximal `int32_t`, and minimally sufficient buffer lengths; use ASan/UBSan on the host code.

### 2. Unchecked additions when sizing the module parameter block
- **File:line:** `HC/hc_wasm.c:1264-1275` (`lance`).
- **Severity:** **high** (potential undersized buffer if arithmetic wraps).
- **Status:** **NOT REPRODUCED** — not demonstrated reachable with realistic 64-bit memory limits.
- **Scenario:** A hostile or malformed stack triggers an external with many/very long string parameters. `taille += strlen(...) + 1` is not checked at each addition; only the final `taille` is compared to `HC_MODULE_PAGES_MAX * 65536`. An overflow could make the final check pass and cause the marshalling code (`depose`, around line 1013) to write more than the reserved region.
- **Suggested fix:** Compute a fixed maximum and, before adding each string, reject if its length is greater than the remaining allowance (with room for the terminator). Validate event strings the same way. Add a unit test for the helper that checks the maximum without allocating enormous real strings.

### 3. Unchecked accumulation capacity in the HC text-stack parser
- **File:line:** `HC/hc_file.c:909-975` (`acc_line`, `acc_join`, `acc_seg`).
- **Severity:** **high** (theoretical integer wrap leading to too-small allocation and overwrite).
- **Status:** **NOT REPRODUCED** — requires impractically large input or a synthetic size-state harness.
- **Scenario:** A stack text file contains an extraordinarily large number of long lines or a huge base64/text block. The expressions `a->len + n + 2` and `cap *= 2` are unchecked. At overflow, the allocation size can be lower than the amount subsequently copied by `memcpy`.
- **Suggested fix:** Reject `n > SIZE_MAX - a->len - 2` before addition, check doubling against `SIZE_MAX / 2`, and propagate a hard load failure.

### 4. Unchecked output-size multiplication in classic-stack text conversion
- **File:line:** `HC/hc_origine.c:91-100` (`hc_origine_utf8`).
- **Severity:** **high** (possible undersized output allocation and overwrite if called with enormous `n`).
- **Status:** **NOT REPRODUCED** — no proven path from a real mapped classic-stack record to an overflowing `n`.
- **Scenario:** A malformed/hostile original stack somehow presents a text span with `n > (SIZE_MAX - 1)/3`. The allocation `malloc(3*n+1)` wraps while conversion can write up to three bytes per original byte.
- **Suggested fix:** Check the multiplication bound before allocating and fail the load on overflow.

## Reviewed / not established

- `HC/hc_wasm.c:929-950`: `hc_module_externes` now limits writes and stops advancing the output cursor after exhaustion. **The previous overflowing `snprintf` finding is fixed and is deliberately not repeated.**
- `HC/hc_wasm.c:460-476`: the HC-to-module string copying path checks a positive destination size and truncates the copy; a direct overflow was **not** identified in this function.
- `HC/hc_wasm.c:390-400`: `DrawXWindow` explicitly rejects nonpositive dimensions, signed multiplication overflow in `l*h`, undersized pixel counts, and palette counts below 768. That is a meaningful protective boundary.
- `HC/hc_origine.c:1120-1200`: the original-stack block directory checks that declared block sizes fit the remaining file; no new critical out-of-bounds read was established in that segment.
- `HC/hc_presse_papiers.c` and the paste path in `HC/HCview.m`: not examined end-to-end; no verified finding claimed. Clipboard ownership and partial-failure rollback require dedicated exercising.
- `HC/hc_core.c` / `HC/hct_*.c`: deferred object deletion and nested script invocation were not exhaustively traced; no use-after-free is claimed without a reproducer.

## Release assessment

**Confirmed critical issues: none. Confirmed high issues: none.** The entries above are **NOT REPRODUCED** hypotheses / boundary-hardening candidates, not validated exploitability reports. In particular, they must not be counted as four demonstrated memory-corruption defects.

This review does **not** provide a complete safety sign-off. Before release, execute `make verifie`, `./tests/lance.sh`, `./tests/lance.sh --asan` in an isolated checkout, and target the identified boundaries with a purpose-built harness and realistic malicious stack/module fixtures. The already-fixed parser recursion cap, unchecked `calloc` in `appel()`, silent save failure, `mkstemp`, and `ferror`-after-`fgets` issues have intentionally not been reported.
