load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_java",
    urls = [
        "https://github.com/bazelbuild/rules_java/releases/download/8.6.3/rules_java-8.6.3.tar.gz",
    ],
    sha256 = "6d8c6d5cd86fed031ee48424f238fa35f33abc9921fd97dd4ae1119a29fc807f",
)

load("@rules_java//java:rules_java_deps.bzl", "rules_java_dependencies")
rules_java_dependencies()

# note that the following line is what is minimally required from protobuf for the java rules
# consider using the protobuf_deps() public API from @com_google_protobuf//:protobuf_deps.bzl
load("@com_google_protobuf//bazel/private:proto_bazel_features.bzl", "proto_bazel_features")  # buildifier: disable=bzl-visibility
proto_bazel_features(name = "proto_bazel_features")

# register toolchains
load("@rules_java//java:repositories.bzl", "rules_java_toolchains")
rules_java_toolchains()

######################################################################
# gflags
######################################################################
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
#load("//rules:patched_http_archive.bzl", "patched_http_archive")
load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
    "new_git_repository",
)

http_archive(
    name = "com_github_gflags_gflags",
    patches = ["//rules:gflags.patch"],
    patch_args = ["-p1"],
    strip_prefix = "gflags-2e227c3daae2ea8899f49858a23f3d318ea39b57",
    sha256 = "c5554661e7dc413f3c8ed8ae48034c8b34cf77ab13d49ed199821ef5544dd4ef",
    urls = ["https://github.com/gflags/gflags/archive/2e227c3daae2ea8899f49858a23f3d318ea39b57.zip"],
)

bind(
    name = "gflags",
    actual = "@com_github_gflags_gflags//:gflags",
)

######################################################################
# imgui
######################################################################
new_git_repository(
    name = "imgui_git",
    build_file = "//rules:imgui.BUILD",
    remote = "https://github.com/ocornut/imgui.git",
    tag = "v1.74",
)

bind(
    name = "imgui",
    actual = "@imgui_git//:imgui",
)

bind(
    name = "imgui_sdl_opengl",
    actual = "@imgui_git//:imgui_sdl_opengl",
)

######################################################################
# IconFontCppHeaders
######################################################################
new_git_repository(
    name = "iconfonts",
    build_file = "//rules:iconfonts.BUILD",
    commit = "fda5f470b767f7b413e4a3995fa8cfe47f78b586",
    remote = "https://github.com/juliettef/IconFontCppHeaders.git",
)

bind(
    name = "fontawesome",
    actual = "@iconfonts//:fontawesome",
)

######################################################################
# Abseil
######################################################################
git_repository(
    name = "com_google_absl",
    commit = "4447c7562e3bc702ade25105912dce503f0c4010",
    remote = "https://github.com/abseil/abseil-cpp.git",
)

######################################################################
# protobuf
######################################################################
git_repository(
    name = "com_google_protobuf",
    remote = "https://github.com/google/protobuf.git",
    tag = "v29.2",
)
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

# rules_cc defines rules for generating C++ code from Protocol Buffers.
http_archive(
    name = "rules_cc",
    urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.17/rules_cc-0.0.17.tar.gz"],
    sha256 = "abc605dd850f813bb37004b77db20106a19311a96b2da1c92b789da529d28fe1",
    strip_prefix = "rules_cc-0.0.17",
)
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")
rules_cc_dependencies()

# rules_proto defines abstract rules for building Protocol Buffers.
http_archive(
    name = "rules_proto",
    sha256 = "14a225870ab4e91869652cfd69ef2028277fc1dc4910d65d353b62d6e0ae21f4",
    strip_prefix = "rules_proto-7.1.0",
    url = "https://github.com/bazelbuild/rules_proto/releases/download/7.1.0/rules_proto-7.1.0.tar.gz",
)

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies")
rules_proto_dependencies()

load("@rules_proto//proto:toolchains.bzl", "rules_proto_toolchains")
rules_proto_toolchains()


######################################################################
# native file dialog
######################################################################
new_git_repository(
    name = "nativefiledialog_git",
    build_file = "//rules:nfd.BUILD",
    commit = "5cfe5002eb0fac1e49777a17dec70134147931e2",
    remote = "https://github.com/mlabbe/nativefiledialog.git",
)

bind(
    name = "nfd",
    actual = "@nativefiledialog_git//:nfd",
)

######################################################################
# compilers for windows
######################################################################

# Local copy for testing
#local_repository(
#    name = "mxebzl",
#    path = "/home/cfrantz/src/mxebzl",
#)

git_repository(
    name = "mxebzl",
    remote = "https://github.com/cfrantz/mxebzl.git",
    tag = "20191215_RC01",
)

load("@mxebzl//compiler:repository.bzl", "mxe_compiler")

mxe_compiler(
    deps = [
        "SDL2",
        "SDL2-extras",
        "compiler",
        "pthreads",
        "python",
    ],
)
