/*===-- passmgr_builder_ocaml.c - LLVM OCaml Glue ---------------*- C++ -*-===*\
|*                                                                            *|
|*                     The LLVM Compiler Infrastructure                       *|
|*                                                                            *|
|* This file is distributed under the University of Illinois Open Source      *|
|* License. See LICENSE.TXT for details.                                      *|
|*                                                                            *|
|*===----------------------------------------------------------------------===*|
|*                                                                            *|
|* This file glues LLVM's OCaml interface to its C interface. These functions *|
|* are by and large transparent wrappers to the corresponding C functions.    *|
|*                                                                            *|
|* Note that these functions intentionally take liberties with the CAMLparamX *|
|* macros, since most of the parameters are not GC heap objects.              *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#include "llvm-c/Transforms/PassManagerBuilder.h"
#include "caml/mlvalues.h"
#include "caml/custom.h"
#include "caml/misc.h"

#define PMBuilder_val(v)  (*(LLVMPassManagerBuilderRef *)(Data_custom_val(v)))

static void llvm_finalize_pmbuilder(value PMB) {
  LLVMPassManagerBuilderDispose(PMBuilder_val(PMB));
}

static struct custom_operations pmbuilder_ops = {
  (char *) "LLVMPassManagerBuilder",
  llvm_finalize_pmbuilder,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
#ifdef custom_compare_ext_default
  , custom_compare_ext_default
#endif
};

static value alloc_pmbuilder(LLVMPassManagerBuilderRef Ref) {
  value Val = caml_alloc_custom(&pmbuilder_ops,
                           sizeof(LLVMPassManagerBuilderRef), 0, 1);
  PMBuilder_val(Val) = Ref;
  return Val;
}

/* t -> unit */
CAMLprim value llvm_pmbuilder_create(value Unit) {
  return alloc_pmbuilder(LLVMPassManagerBuilderCreate());
}

/* int -> t -> unit */
CAMLprim value llvm_pmbuilder_set_opt_level(value OptLevel, value PMB) {
  LLVMPassManagerBuilderSetOptLevel(PMBuilder_val(PMB), Int_val(OptLevel));
  return Val_unit;
}

/* int -> t -> unit */
CAMLprim value llvm_pmbuilder_set_size_level(value SizeLevel, value PMB) {
  LLVMPassManagerBuilderSetSizeLevel(PMBuilder_val(PMB), Int_val(SizeLevel));
  return Val_unit;
}

/* int -> t -> unit */
CAMLprim value llvm_pmbuilder_use_inliner_with_threshold(
                      value Threshold, value PMB) {
  LLVMPassManagerBuilderSetOptLevel(PMBuilder_val(PMB), Int_val(Threshold));
  return Val_unit;
}

/* bool -> t -> unit */
CAMLprim value llvm_pmbuilder_set_disable_unit_at_a_time(
                      value DisableUnitAtATime, value PMB) {
  LLVMPassManagerBuilderSetDisableUnitAtATime(
                      PMBuilder_val(PMB), Bool_val(DisableUnitAtATime));
  return Val_unit;
}

/* bool -> t -> unit */
CAMLprim value llvm_pmbuilder_set_disable_unroll_loops(
                      value DisableUnroll, value PMB) {
  LLVMPassManagerBuilderSetDisableUnrollLoops(
                      PMBuilder_val(PMB), Bool_val(DisableUnroll));
  return Val_unit;
}

/* [ `Function ] Llvm.PassManager.t -> t -> unit */
CAMLprim value llvm_pmbuilder_populate_function_pass_manager(
                      LLVMPassManagerRef PM, value PMB) {
  LLVMPassManagerBuilderPopulateFunctionPassManager(
                      PMBuilder_val(PMB), PM);
  return Val_unit;
}

/* [ `Module ] Llvm.PassManager.t -> t -> unit */
CAMLprim value llvm_pmbuilder_populate_module_pass_manager(
                      LLVMPassManagerRef PM, value PMB) {
  LLVMPassManagerBuilderPopulateModulePassManager(
                      PMBuilder_val(PMB), PM);
  return Val_unit;
}

/* [ `Module ] Llvm.PassManager.t ->
   internalize:bool -> run_inliner:bool -> t -> unit */
CAMLprim value llvm_pmbuilder_populate_lto_pass_manager(
                      LLVMPassManagerRef PM, value Internalize, value RunInliner,
                      value PMB) {
  LLVMPassManagerBuilderPopulateLTOPassManager(
                      PMBuilder_val(PMB), PM,
                      Bool_val(Internalize), Bool_val(RunInliner));
  return Val_unit;
}
