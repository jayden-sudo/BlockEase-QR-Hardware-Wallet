file(GLOB_RECURSE src
    "./*.c"
)
set(include "./")

idf_component_register(SRCS ${src}
    INCLUDE_DIRS ${include}
    REQUIRES uBitcoin
    PRIV_INCLUDE_DIRS ".")
