# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
# PocketOS. core/ and ui/ are portable (also built on a PC by host/Makefile); esp/ is device-only.
file(GLOB CORE_SRCS core/*.c)
file(GLOB UI_SRCS ui/*.c ui/assets/*.c)
file(GLOB ESP_SRCS esp/*.c)

idf_component_register(
    SRCS ${CORE_SRCS} ${UI_SRCS} ${ESP_SRCS}
    INCLUDE_DIRS "core" "ui" "esp"
    EMBED_TXTFILES "www/index.html"
)

# Large generated assets (fonts, background) are const data in flash
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format-truncation)
