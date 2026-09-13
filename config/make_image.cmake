file(SIZE ${KERNEL_BIN} KERNEL_SIZE)
math(EXPR MAX_SIZE "${DAP_SECTORS} * 512")

if(KERNEL_SIZE GREATER MAX_SIZE)
    message(FATAL_ERROR "kernel.bin (${KERNEL_SIZE} bytes) exceeds DAP sector limit (${MAX_SIZE} bytes)!")
endif()

execute_process(
        COMMAND cat ${BOOT_BIN} ${KERNEL_BIN}
        OUTPUT_FILE ${OS_IMG}
)

file(SIZE ${OS_IMG} CURRENT_SIZE)
math(EXPR MIN_SIZE "(1 + ${DAP_SECTORS}) * 512")

if(CURRENT_SIZE LESS MIN_SIZE)
    set(TARGET_SIZE ${MIN_SIZE})
else()
    math(EXPR REMAINDER "${CURRENT_SIZE} % 512")
    if(REMAINDER EQUAL 0)
        set(TARGET_SIZE ${CURRENT_SIZE})
    else()
        math(EXPR TARGET_SIZE "${CURRENT_SIZE} + (512 - ${REMAINDER})")
    endif()
endif()

execute_process(
        COMMAND truncate -s ${TARGET_SIZE} ${OS_IMG}
)

message(STATUS "os.img built: ${TARGET_SIZE} bytes")