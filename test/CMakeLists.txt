
include(PicofuseTests)

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
