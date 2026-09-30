zephyr_library_named(deos_core)

zephyr_library_sources(
    src/deos_core.c
    src/deos_rx.c
    src/deos_tx.c
    src/deos_dispatch.c
    src/deos_codec.c
    src/deos_network.c
    src/deos_router.c
)

zephyr_include_directories(include)

target_link_libraries(app PRIVATE deos_core)
