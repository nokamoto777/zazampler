# Third-party components

- JUCE 8.0.6 — JUCE licensing terms; see upstream LICENSE and the project LICENSE.
- sfizz 1.2.3 — BSD-2-Clause and bundled dependency licenses, pinned in CMakeLists.txt. ZaZampler applies checked extensions for per-sample pitch and per-voice modulation.
- VelociLoops — https://github.com/kunitoki/VelociLoops, commit f77609ec70b7bb6db362ba97030bd9703539fa01, The Unlicense (public domain dedication and warranty disclaimer in upstream LICENSE). Independent REX/REX2 decoder; no Reason Studios SDK is bundled. REX/ReCycle are Reason Studios names.

Dependencies are fetched at build time. Their upstream license files accompany the source checkouts. No manufacturer preset audio or proprietary executables are redistributed.
