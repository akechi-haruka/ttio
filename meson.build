project('ttio', 'c',
        version : '0.2',
        default_options : ['warning_level=3', 'c_std=c18'])

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_MESON_LIBRARY']

segapi = subproject('segapi')
aimelib = subproject('aimelib')

shared_library('ttio', 'main.c',
               c_args : lib_args,
               vs_module_defs : 'exports.def',
               link_with : [
                   segapi.get_variable('api_lib'),
                   aimelib.get_variable('aime_lib')
               ]
)