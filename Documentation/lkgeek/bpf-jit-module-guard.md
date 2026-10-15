# BPF JIT link failure with MODULES=n and LTO disabled

The `third-update` BPF import accidentally omitted the `CONFIG_MODULES` guard
around the generic weak `bpf_jit_alloc_exec` / `bpf_jit_free_exec` fallbacks.
Those implementations call `module_alloc` / `module_memfree`, which do not
exist with the Wayne configuration (`CONFIG_MODULES=n`). ARM64 already has
strong, module-independent JIT allocation/free functions in
`arch/arm64/net/bpf_jit_comp.c`.

ThinLTO discarded the unused generic implementations in the previously tested
builds. A non-LTO link retains their relocations even though the strong ARM64
symbols win symbol resolution. Thus local ThinLTO validation did not cover
this CI failure. The original LKGeek code had this guard; restoring it fixes
the regression without enabling loadable modules or disabling BPF JIT.

The inspected KernelSU_Action workflow at `14ce26678b24` invokes make with
CC=clang but omits LLVM=1 and LLVM_IAS=1. This kernel's HAS_LTO_CLANG requires
both options, so a defconfig request for ThinLTO alone does not enable it.
To match the documented local build, pass both options during defconfig and
image builds, for example in config.env:

```text
EXTRA_CMDS:LD=ld.lld LLVM=1 LLVM_IAS=1
```

The kernel fix is still needed: non-LTO builds should link correctly too.

## Verification

A separate output configuration explicitly selects LTO_NONE=y, MODULES=n and
BPF_JIT=y. Compile `kernel/bpf/core.o` and `arch/arm64/net/bpf_jit_comp.o`, then
combine them with `ld.lld -r` and inspect the symbols with `llvm-nm -u`:

- Before the fix: unresolved module_alloc and module_memfree reproduced.
- After the fix: neither module symbol nor JIT allocator/free symbol unresolved.

This is a focused reproduction of the reported link defect, not a full
non-LTO kernel build or a rerun of the remote GitHub Actions pipeline.

The production ThinLTO Image.gz and device trees rebuilt successfully after
this fix (code commit `2739dc986e50`), gzip validation passed, and all 41
KSU/BPF/SUSFS/log assertions passed again in the ARM64 virtual machine.
Updated image and logs are retained in `.third-update-artifacts/bpf-module-guard/`.
