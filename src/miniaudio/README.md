# Miniaudio

This directory vendors `miniaudio.h` and its license from Miniaudio 0.11.21
(commit `4a5b74bef029b3592c54b6048650ee5f972c1a48`). Keeping the required
single-header library in the repository makes Flutter Git dependencies
self-contained because Dart Pub does not initialize Git submodules.

When updating Miniaudio, replace both `miniaudio.h` and `LICENSE`, then update
the version and commit recorded above.
