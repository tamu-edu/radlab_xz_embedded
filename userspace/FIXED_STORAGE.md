# Fixed-storage build

The root CMake target `xz::embedded` builds a userspace LZMA2 decoder with
CRC32, CRC64, SHA-256, and concatenated-stream support. It excludes BCJ,
MicroLZMA, single-call mode, and dynamically allocated dictionary mode.
The upstream Makefile build remains available separately.

1. Initialize the enabled CRC tables before concurrent decoder use.
2. Reserve storage aligned to `max_align_t`; obtain the required byte count
   from `xz_dec_storage_size(dictionary_limit)`.
3. Call `xz_dec_init_preallocated(storage, capacity, dictionary_limit)`.
   Invalid storage returns null before modifying the buffer.
4. Use the upstream streaming API. `xz_dec_reset()` reuses the workspace;
   `xz_dec_end()` never frees caller-owned storage in this build.

Each active decoder needs an exclusive workspace. No allocator callback or
global decoder instance is used. The dictionary limit is a hard admission
bound, independent of the total expanded output size. CRC tables are shared;
initialize them before starting concurrent work.

The storage adaptation changes initialization and ownership only. Stream,
block, index, checksum, and LZMA2 decoding remain upstream code. Track upstream
`tukaani-project/xz-embedded` and Linux `lib/xz` fixes when updating the pinned
source. This target does not implement archive parsing or filesystem extraction.
