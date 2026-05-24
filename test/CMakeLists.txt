
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
