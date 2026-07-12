# nDPI in the KSword Network Inspector plugin

This directory contains the nDPI 5.0 library source and a small C bridge. The
independent Network Inspector executable links the library through
`nDPI.vcxproj`; the KSword application itself does not link or load nDPI.

The Windows build uses an SRWLOCK-backed pthread mutex compatibility header. This covers the mutex-only pthread API used by nDPI 5.0 and removes the legacy pthreads-win32 runtime dependency from the official Windows project.

Update procedure:

1. Replace `src/` with the source tree from a reviewed nDPI release.
2. Refresh `windows/src/ndpi_config.h` and `windows/src/ndpi_define.h`.
3. Update the pinned version and commit in `NOTICE.md`.
4. Build both Debug and Release configurations and run the KSword classifier tests.
