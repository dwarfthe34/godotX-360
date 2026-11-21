import os


def is_active():
    return True


def get_name():
    return "Xbox 360 (xsdk)"


def can_build():
    toolchain = os.environ.get("XSDK_TOOLCHAIN", os.path.expanduser("~/xsdk-gcc"))
    return os.path.isfile(os.path.join(toolchain, "bin", "powerpc-xsdk-elf-gcc"))


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        BoolVariable(
            "debug_symbols",
            "Add debugging symbols to release/release_debug builds",
            True,
        ),
    ]


def get_flags():
    return []


def configure(env):
    toolchain = os.environ.get("XSDK_TOOLCHAIN", os.path.expanduser("~/xsdk-gcc"))
    sdk_root = os.environ.get("XSDK_ROOT", os.path.expanduser("~/Downloads/xsdk-main"))

    prefix = os.path.join(toolchain, "bin", "powerpc-xsdk-elf-")
    env["CC"] = prefix + "gcc"
    env["CXX"] = prefix + "g++"
    env["LINK"] = prefix + "g++"
    env["AR"] = prefix + "ar"
    env["RANLIB"] = prefix + "ranlib"
    env["OBJCOPY"] = prefix + "objcopy"

    env.Append(
        CPPDEFINES=[
            "XSDK",
            "XSDK_ENABLED",
            "XBOX360_ENABLED",
            # xsdk is newlib-based and provides libc fopen(), so enable the
            # portable libc file IO code path used by the Unix drivers.
            "LIBC_FILEIO_ENABLED",
        ]
    )

    env.Append(
        CCFLAGS=[
            "-mcpu=cell",
            "-fno-pic",
        ]
    )

    env.Prepend(
        CPPPATH=[
            "#platform/xbox360xsdk",
            os.path.join(sdk_root, "include"),
        ]
    )

    # xsdk links like this: --start-group [objects] -lstdc++ libxsdk.a -lc -lm -lgcc --end-group.
    # Godot objects land after LINKFLAGS and before LIBS. A File() node is emitted as
    # the literal path; ld auto-inserts the missing --end-group at the end of the line.
    env.Append(
        LINKFLAGS=[
            "-mcpu=cell",
            "-fno-pic",
            "-static",
            "-nostdlib",
            "-T" + os.path.join(sdk_root, "src", "xsdk.ld"),
            "-Wl,--start-group",
        ]
    )

    env.Append(
        LIBS=[
            "stdc++",
            env.File(os.path.join(sdk_root, "build", "libxsdk.a")),
            "c",
            "m",
            "gcc",
        ]
    )

    env["bits"] = "32"
    env["arch"] = "ppc"

    print("Configuring for Xbox 360 / xsdk")
    print("  XSDK_TOOLCHAIN: " + toolchain)
    print("  XSDK_ROOT:      " + sdk_root)
    print("  Compiler:       " + env["CXX"])
