// RUN: %clang_cc1 -fsycl -fsycl-is-device -triple spir64-unknown-unknown-sycldevice -emit-llvm %s -o - | FileCheck %s

// CHECK: define {{.*}}spir_kernel void @_ZTSZ4mainE15kernel_function() {{[^{]+}} !kernel_arg_addr_space ![[MD:[0-9]+]] !kernel_arg_access_qual ![[MD]] !kernel_arg_type ![[MD]] !kernel_arg_base_type ![[MD]] !kernel_arg_type_qual ![[MD]]
// CHECK: ![[MD]] = !{}

template <typename name, typename Func>
__attribute__((sycl_kernel)) void kernel_single_task(Func kernelFunc) {
  kernelFunc();
}

int main() {
  kernel_single_task<class kernel_function>([]() {});
  return 0;
}
