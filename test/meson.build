doctest_dep = declare_dependency(include_directories: '3rdparty')

test_files = {
    'basic test': files('test.cpp'),
    'aliases test': files('test_aliases.cpp'),
    'containers test': files('test_containers.cpp'),
    'flags test': files('test_flags.cpp'),
    'single range test': files('test_range.cpp'),
    'wchar_t test': files('test_wchar_t.cpp'),
}

foreach test_name, test_src : test_files
    test_exe = executable(
        test_name.underscorify(),
        test_src,

        build_by_default: false,
        dependencies: [magic_enum_dep, doctest_dep],
    )

    test(test_name, test_exe)
endforeach

# Explicit external name table: self-contained, no doctest dependency.
explicit_names_test_exe = executable(
    'explicit_names_test',
    files('test_explicit_names.cpp'),
    build_by_default: false,
    dependencies: [magic_enum_dep],
    cpp_args: ['-Werror'],
)
test('explicit names test', explicit_names_test_exe)
