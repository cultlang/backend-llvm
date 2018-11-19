load("//build_tools:macros.bzl", "header_generator", "dll_generator", "entrypoint_generator")
load("//build_tools:expand_llvm.bzl", "expand_llvm")

header_generator(  
  deps = [
    "//util", 
    "//types:headers", 
    "//lisp:headers"
  ] + select({
    "@bazel_tools//src/conditions:windows": ["@llvm_windows//:headers"],
    "@bazel_tools//src/conditions:darwin": [],
    "//conditions:default": ["@llvm_linux//:headers"]
  })
)

dll_generator(
  packages = [
    "types",
    "lisp",
  ],
  deps = select({
        "@bazel_tools//src/conditions:windows": expand_llvm("llvm_windows"),
        "@bazel_tools//src/conditions:darwin": [],
        "//conditions:default": expand_llvm("llvm_linux"),
    }),
)


