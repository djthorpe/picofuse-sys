
include(tests)

# Just print "hello world" to verify that the test framework is working.
picofuse_add_sys_test(sys_001
	main.c
)

# Check timestamp.
picofuse_add_sys_test(sys_002
	main.c
)

# Check timestamp progression across sleeps.
picofuse_add_sys_test(sys_003
	main.c
)

# Check basic mutex lifecycle without threading.
picofuse_add_sys_test(sys_004
	main.c
)

# Check thread core discovery.
picofuse_add_sys_test(sys_005
	main.c
)

# Check atomic operations.
picofuse_add_sys_test(sys_006
	main.c
)

# Check thread creation and completion.
picofuse_add_sys_test(sys_007
	main.c
)

# Check mutex synchronization across threads.
picofuse_add_sys_test(sys_008
	main.c
)

# Check random number generation returns non-degenerate values.
picofuse_add_sys_test(sys_009
	main.c
)

# Check that the pthread thread wrapper pool bounds concurrent threads.
picofuse_add_sys_test(sys_010
	main.c
)

# Check condition variable wait, signal, timeout, and slot reuse.
picofuse_add_sys_test(sys_011
	main.c
)

# Check condition variable no-stored-signal semantics and broadcast behavior.
picofuse_add_sys_test(sys_012
	main.c
)

# Check printf formatting for signed size_t, pointers, and custom handlers.
picofuse_add_sys_test(sys_013
	main.c
)

# Check waitgroup add/done/wait behavior.
picofuse_add_sys_test(sys_014
	main.c
)

# Check hash lifecycle and digest output.
picofuse_add_sys_test(sys_015
	main.c
)

# Check malloc/calloc/realloc/free wrappers.
picofuse_add_sys_test(sys_016
	main.c
)

# Check arena init/next/delete behavior.
picofuse_add_sys_test(sys_017
	main.c
)

# Check arena allocation behavior under concurrent access.
picofuse_add_sys_test(sys_018
	main.c
)

# Check heap-backed event queue behavior.
picofuse_add_sys_test(sys_019
	main.c
)

# Check date and time extraction, mutation, validation, and comparison behavior.
picofuse_add_sys_test(sys_020
	main.c
)

# Check environment serial, name, version, and signal handler behavior.
picofuse_add_sys_test(sys_021
	main.c
)

# Check timer lifecycle: invalid args, start/valid/deinit, pool exhaustion and reuse.
picofuse_add_sys_test(sys_022
	main.c
)

# Check timer firing: periodic counter and one-shot via deinit from callback.
picofuse_add_sys_test(sys_023
	main.c
)

# Check concurrent timer access from two cores/threads.
picofuse_add_sys_test(sys_024
	main.c
)

# Check timer deinit waits for an in-flight callback to complete.
picofuse_add_sys_test(sys_027
	main.c
)

# Check single-worker run loop: lifecycle, events, exit value, reuse.
picofuse_add_sys_test(sys_025
	main.c
)

# Check multi-worker run loop: both workers initialize, all events processed.
picofuse_add_sys_test(sys_026
	main.c
)

# Check sys_sprintf formatting for fixed-width integer types.
picofuse_add_sys_test(sys_028
	main.c
)

# Check GPIO API behavior on supported and stub backends.
picofuse_add_hw_test(hw_000
	main.c
)

# Check ADC API behavior and GPIO/channel mapping.
picofuse_add_hw_test(hw_001
	main.c
)

# Check I2C API behavior on supported and stub backends.
picofuse_add_hw_test(hw_002
	main.c
)

# Check SPI API behavior on supported and stub backends.
picofuse_add_hw_test(hw_003
	main.c
)

# Check USB API lifecycle behavior on supported and stub backends.
picofuse_add_hw_test(hw_004
	main.c
)

# Check PWM API behavior: init, config, period, duty, enable, and callbacks.
picofuse_add_hw_test(hw_005
	main.c
)

# Check block storage API behavior: capacity, init, erase/read/write, and deinit.
picofuse_add_hw_test(hw_006
	main.c
)

# Check watchdog API behavior: init, enable/reset flows, and stub fallback.
picofuse_add_hw_test(hw_007
	main.c
)

# Check HID init constraints, pool capacity, and slot reuse behavior.
picofuse_add_hid_test(hid_001
	main.c
)

# Check HID fake-device callbacks, polling, and enumeration/info behavior.
picofuse_add_hid_test(hid_002
	main.c
)

# Check mounting the current working directory as a POSIX path volume, and
# fs_vol_init_memory construction (default size, size=0 rounding, and that a
# non-NULL arena is rejected until arena-backed volumes are implemented).
picofuse_add_fs_test(fs_000
	main.c
)

# Check directory listing via fs_vol_readdir, on both the POSIX path and
# in-memory (littlefs) backends.
picofuse_add_fs_test(fs_001
	main.c
)

# Check rejection of ".", "..", relative, and out-of-root paths, on both the
# POSIX path and in-memory (littlefs) backends - the two backends confine
# paths by different mechanisms (real chroot-style confinement vs. a
# symbolic root) but must reject the same forms.
picofuse_add_fs_test(fs_002
	main.c
)

# Check volume size/free-space reporting, on both the POSIX path and
# in-memory (littlefs) backends.
picofuse_add_fs_test(fs_003
	main.c
)

# Check recursively walking and stating files under a volume, on both the
# POSIX path and in-memory (littlefs) backends.
picofuse_add_fs_test(fs_004
	main.c
)

# Check hidden ("." prefixed) entries are skipped by readdir but remain
# directly reachable via stat/mkdir/remove/move, on both the POSIX path and
# in-memory (littlefs) backends.
picofuse_add_fs_test(fs_005
	main.c
)

# Check move/rename overwrite semantics match littlefs/rename(2): same-type
# targets are atomically replaced, type mismatches and non-empty directory
# targets are rejected, and the volume root can never be overwritten. Run
# against both the POSIX path and in-memory (littlefs) backends.
picofuse_add_fs_test(fs_006
	main.c
)

# Check file create/open/close/read/write/seek lifecycle and edge cases, on
# both the POSIX path and in-memory (littlefs) backends.
picofuse_add_fs_test(fs_007
	main.c
)

# Check every fs_vol_*/fs_file_* generic dispatcher is safe when given a
# NULL volume/file (platform-independent - never mounts a volume).
picofuse_add_fs_test(fs_008
	main.c
)
