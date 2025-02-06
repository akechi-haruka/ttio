project('ttio', 'c',
        version : '1.0.0',
        default_options : ['warning_level=3', 'c_std=c18'])

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_MESON_LIBRARY']

shared_library('ttio', 'main.c',
               c_args : lib_args,
               vs_module_defs : 'exports.def',
)