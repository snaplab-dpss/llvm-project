/*===-- target_ocaml.c - LLVM OCaml Glue ------------------------*- C++ -*-===*\
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

#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"
#include "caml/alloc.h"
#include "caml/fail.h"
#include "caml/memory.h"
#include "caml/custom.h"

/*===---- Exceptions ------------------------------------------------------===*/

static value llvm_target_error_exn;

CAMLprim value llvm_register_target_exns(value Error) {
  llvm_target_error_exn = Field(Error, 0);
  caml_register_global_root(&llvm_target_error_exn);
  return Val_unit;
}

static void llvm_raise(value Prototype, char *Message) {
  CAMLparam1(Prototype);
  CAMLlocal1(CamlMessage);

  CamlMessage = caml_copy_string(Message);
  LLVMDisposeMessage(Message);

  caml_raise_with_arg(Prototype, CamlMessage);
  abort(); /* NOTREACHED */
#ifdef CAMLnoreturn
  CAMLnoreturn; /* Silences warnings, but is missing in some versions. */
#endif
}

static value llvm_string_of_message(char* Message) {
  value String = caml_copy_string(Message);
  LLVMDisposeMessage(Message);

  return String;
}

/*===---- Data Layout -----------------------------------------------------===*/

#define DataLayout_val(v)  (*(LLVMTargetDataRef *)(Data_custom_val(v)))

static void llvm_finalize_data_layout(value DataLayout) {
  LLVMDisposeTargetData(DataLayout_val(DataLayout));
}

static struct custom_operations llvm_data_layout_ops = {
  (char *) "LLVMDataLayout",
  llvm_finalize_data_layout,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
#ifdef custom_compare_ext_default
  , custom_compare_ext_default
#endif
};

value llvm_alloc_data_layout(LLVMTargetDataRef DataLayout) {
  value V = caml_alloc_custom(&llvm_data_layout_ops, sizeof(LLVMTargetDataRef),
                              0, 1);
  DataLayout_val(V) = DataLayout;
  return V;
}

/* string -> DataLayout.t */
CAMLprim value llvm_datalayout_of_string(value StringRep) {
  return llvm_alloc_data_layout(LLVMCreateTargetData(String_val(StringRep)));
}

/* DataLayout.t -> string */
CAMLprim value llvm_datalayout_as_string(value TD) {
  char *StringRep = LLVMCopyStringRepOfTargetData(DataLayout_val(TD));
  value Copy = caml_copy_string(StringRep);
  LLVMDisposeMessage(StringRep);
  return Copy;
}

/* [<Llvm.PassManager.any] Llvm.PassManager.t -> DataLayout.t -> unit */
CAMLprim value llvm_datalayout_add_to_pass_manager(LLVMPassManagerRef PM,
                                                   value DL) {
  LLVMAddTargetData(DataLayout_val(DL), PM);
  return Val_unit;
}

/* DataLayout.t -> Endian.t */
CAMLprim value llvm_datalayout_byte_order(value DL) {
  return Val_int(LLVMByteOrder(DataLayout_val(DL)));
}

/* DataLayout.t -> int */
CAMLprim value llvm_datalayout_pointer_size(value DL) {
  return Val_int(LLVMPointerSize(DataLayout_val(DL)));
}

/* Llvm.llcontext -> DataLayout.t -> Llvm.lltype */
CAMLprim LLVMTypeRef llvm_datalayout_intptr_type(LLVMContextRef C, value DL) {
  return LLVMIntPtrTypeInContext(C, DataLayout_val(DL));;
}

/* int -> DataLayout.t -> int */
CAMLprim value llvm_datalayout_qualified_pointer_size(value AS, value DL) {
  return Val_int(LLVMPointerSizeForAS(DataLayout_val(DL), Int_val(AS)));
}

/* Llvm.llcontext -> int -> DataLayout.t -> Llvm.lltype */
CAMLprim LLVMTypeRef llvm_datalayout_qualified_intptr_type(LLVMContextRef C,
                                                           value AS,
                                                           value DL) {
  return LLVMIntPtrTypeForASInContext(C, DataLayout_val(DL), Int_val(AS));
}

/* Llvm.lltype -> DataLayout.t -> Int64.t */
CAMLprim value llvm_datalayout_size_in_bits(LLVMTypeRef Ty, value DL) {
  return caml_copy_int64(LLVMSizeOfTypeInBits(DataLayout_val(DL), Ty));
}

/* Llvm.lltype -> DataLayout.t -> Int64.t */
CAMLprim value llvm_datalayout_store_size(LLVMTypeRef Ty, value DL) {
  return caml_copy_int64(LLVMStoreSizeOfType(DataLayout_val(DL), Ty));
}

/* Llvm.lltype -> DataLayout.t -> Int64.t */
CAMLprim value llvm_datalayout_abi_size(LLVMTypeRef Ty, value DL) {
  return caml_copy_int64(LLVMABISizeOfType(DataLayout_val(DL), Ty));
}

/* Llvm.lltype -> DataLayout.t -> int */
CAMLprim value llvm_datalayout_abi_align(LLVMTypeRef Ty, value DL) {
  return Val_int(LLVMABIAlignmentOfType(DataLayout_val(DL), Ty));
}

/* Llvm.lltype -> DataLayout.t -> int */
CAMLprim value llvm_datalayout_stack_align(LLVMTypeRef Ty, value DL) {
  return Val_int(LLVMCallFrameAlignmentOfType(DataLayout_val(DL), Ty));
}

/* Llvm.lltype -> DataLayout.t -> int */
CAMLprim value llvm_datalayout_preferred_align(LLVMTypeRef Ty, value DL) {
  return Val_int(LLVMPreferredAlignmentOfType(DataLayout_val(DL), Ty));
}

/* Llvm.llvalue -> DataLayout.t -> int */
CAMLprim value llvm_datalayout_preferred_align_of_global(LLVMValueRef GlobalVar,
                                                         value DL) {
  return Val_int(LLVMPreferredAlignmentOfGlobal(DataLayout_val(DL), GlobalVar));
}

/* Llvm.lltype -> Int64.t -> DataLayout.t -> int */
CAMLprim value llvm_datalayout_element_at_offset(LLVMTypeRef Ty, value Offset,
                                                 value DL) {
  return Val_int(LLVMElementAtOffset(DataLayout_val(DL), Ty,
                                     Int64_val(Offset)));
}

/* Llvm.lltype -> int -> DataLayout.t -> Int64.t */
CAMLprim value llvm_datalayout_offset_of_element(LLVMTypeRef Ty, value Index,
                                                 value DL) {
  return caml_copy_int64(LLVMOffsetOfElement(DataLayout_val(DL), Ty,
                                             Int_val(Index)));
}

/*===---- Target ----------------------------------------------------------===*/

static value llvm_target_option(LLVMTargetRef Target) {
  if(Target != NULL) {
    value Result = caml_alloc_small(1, 0);
    Store_field(Result, 0, (value) Target);
    return Result;
  }

  return Val_int(0);
}

/* unit -> string */
CAMLprim value llvm_target_default_triple(value Unit) {
  char *TripleCStr = LLVMGetDefaultTargetTriple();
  value TripleStr = caml_copy_string(TripleCStr);
  LLVMDisposeMessage(TripleCStr);

  return TripleStr;
}

/* unit -> Target.t option */
CAMLprim value llvm_target_first(value Unit) {
  return llvm_target_option(LLVMGetFirstTarget());
}

/* Target.t -> Target.t option */
CAMLprim value llvm_target_succ(LLVMTargetRef Target) {
  return llvm_target_option(LLVMGetNextTarget(Target));
}

/* string -> Target.t option */
CAMLprim value llvm_target_by_name(value Name) {
  return llvm_target_option(LLVMGetTargetFromName(String_val(Name)));
}

/* string -> Target.t */
CAMLprim LLVMTargetRef llvm_target_by_triple(value Triple) {
  LLVMTargetRef T;
  char *Error;

  if(LLVMGetTargetFromTriple(String_val(Triple), &T, &Error))
    llvm_raise(llvm_target_error_exn, Error);

  return T;
}

/* Target.t -> string */
CAMLprim value llvm_target_name(LLVMTargetRef Target) {
  return caml_copy_string(LLVMGetTargetName(Target));
}

/* Target.t -> string */
CAMLprim value llvm_target_description(LLVMTargetRef Target) {
  return caml_copy_string(LLVMGetTargetDescription(Target));
}

/* Target.t -> bool */
CAMLprim value llvm_target_has_jit(LLVMTargetRef Target) {
  return Val_bool(LLVMTargetHasJIT(Target));
}

/* Target.t -> bool */
CAMLprim value llvm_target_has_target_machine(LLVMTargetRef Target) {
  return Val_bool(LLVMTargetHasTargetMachine(Target));
}

/* Target.t -> bool */
CAMLprim value llvm_target_has_asm_backend(LLVMTargetRef Target) {
  return Val_bool(LLVMTargetHasAsmBackend(Target));
}

/*===---- Target Machine --------------------------------------------------===*/

#define TargetMachine_val(v)  (*(LLVMTargetMachineRef *)(Data_custom_val(v)))

static void llvm_finalize_target_machine(value Machine) {
  LLVMDisposeTargetMachine(TargetMachine_val(Machine));
}

static struct custom_operations llvm_target_machine_ops = {
  (char *) "LLVMTargetMachine",
  llvm_finalize_target_machine,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default
#ifdef custom_compare_ext_default
  , custom_compare_ext_default
#endif
};

static value llvm_alloc_targetmachine(LLVMTargetMachineRef Machine) {
  value V = caml_alloc_custom(&llvm_target_machine_ops, sizeof(LLVMTargetMachineRef),
                              0, 1);
  TargetMachine_val(V) = Machine;
  return V;
}

/* triple:string -> ?cpu:string -> ?features:string
   ?level:CodeGenOptLevel.t -> ?reloc_mode:RelocMode.t
   ?code_model:CodeModel.t -> Target.t -> TargetMachine.t */
CAMLprim value llvm_create_targetmachine_native(value Triple, value CPU,
                  value Features, value OptLevel, value RelocMode,
                  value CodeModel, LLVMTargetRef Target) {
  LLVMTargetMachineRef Machine;
  const char *CPUStr = "", *FeaturesStr = "";
  LLVMCodeGenOptLevel OptLevelEnum = LLVMCodeGenLevelDefault;
  LLVMRelocMode RelocModeEnum = LLVMRelocDefault;
  LLVMCodeModel CodeModelEnum = LLVMCodeModelDefault;

  if(CPU != Val_int(0))
    CPUStr = String_val(Field(CPU, 0));
  if(Features != Val_int(0))
    FeaturesStr = String_val(Field(Features, 0));
  if(OptLevel != Val_int(0))
    OptLevelEnum = Int_val(Field(OptLevel, 0));
  if(RelocMode != Val_int(0))
    RelocModeEnum = Int_val(Field(RelocMode, 0));
  if(CodeModel != Val_int(0))
    CodeModelEnum = Int_val(Field(CodeModel, 0));

  Machine = LLVMCreateTargetMachine(Target, String_val(Triple), CPUStr,
                FeaturesStr, OptLevelEnum, RelocModeEnum, CodeModelEnum);

  return llvm_alloc_targetmachine(Machine);
}

CAMLprim value llvm_create_targetmachine_bytecode(value *argv, int argn) {
  return llvm_create_targetmachine_native(argv[0], argv[1], argv[2], argv[3],
                                    argv[4], argv[5], (LLVMTargetRef) argv[6]);
}

/* TargetMachine.t -> Target.t */
CAMLprim LLVMTargetRef llvm_targetmachine_target(value Machine) {
  return LLVMGetTargetMachineTarget(TargetMachine_val(Machine));
}

/* TargetMachine.t -> string */
CAMLprim value llvm_targetmachine_triple(value Machine) {
  return llvm_string_of_message(LLVMGetTargetMachineTriple(
                                TargetMachine_val(Machine)));
}

/* TargetMachine.t -> string */
CAMLprim value llvm_targetmachine_cpu(value Machine) {
  return llvm_string_of_message(LLVMGetTargetMachineCPU(
                                TargetMachine_val(Machine)));
}

/* TargetMachine.t -> string */
CAMLprim value llvm_targetmachine_features(value Machine) {
  return llvm_string_of_message(LLVMGetTargetMachineFeatureString(
                                TargetMachine_val(Machine)));
}

/* TargetMachine.t -> DataLayout.t */
CAMLprim value llvm_targetmachine_data_layout(value Machine) {
  CAMLparam1(Machine);
  CAMLlocal1(DataLayout);

  /* LLVMGetTargetMachineData returns a pointer owned by the TargetMachine,
     so it is impossible to wrap it with llvm_alloc_target_data, which assumes
     that OCaml owns the pointer. */
  LLVMTargetDataRef OrigDataLayout;
  OrigDataLayout = LLVMGetTargetMachineData(TargetMachine_val(Machine));

  char* TargetDataCStr;
  TargetDataCStr = LLVMCopyStringRepOfTargetData(OrigDataLayout);
  DataLayout = llvm_alloc_data_layout(LLVMCreateTargetData(TargetDataCStr));
  LLVMDisposeMessage(TargetDataCStr);

  CAMLreturn(DataLayout);
}

/* TargetMachine.t -> bool -> unit */
CAMLprim value llvm_targetmachine_set_verbose_asm(value Machine, value Verb) {
  LLVMSetTargetMachineAsmVerbosity(TargetMachine_val(Machine), Bool_val(Verb));
  return Val_unit;
}

/* Llvm.llmodule -> CodeGenFileType.t -> string -> TargetMachine.t -> unit */
CAMLprim value llvm_targetmachine_emit_to_file(LLVMModuleRef Module,
                            value FileType, value FileName, value Machine) {
  char* ErrorMessage;

  if(LLVMTargetMachineEmitToFile(TargetMachine_val(Machine), Module,
                                 String_val(FileName), Int_val(FileType),
                                 &ErrorMessage)) {
    llvm_raise(llvm_target_error_exn, ErrorMessage);
  }

  return Val_unit;
}

/* Llvm.llmodule -> CodeGenFileType.t -> TargetMachine.t ->
   Llvm.llmemorybuffer */
CAMLprim LLVMMemoryBufferRef llvm_targetmachine_emit_to_memory_buffer(
                                LLVMModuleRef Module, value FileType,
                                value Machine) {
  char* ErrorMessage;
  LLVMMemoryBufferRef Buffer;

  if(LLVMTargetMachineEmitToMemoryBuffer(TargetMachine_val(Machine), Module,
                                         Int_val(FileType), &ErrorMessage,
                                         &Buffer)) {
    llvm_raise(llvm_target_error_exn, ErrorMessage);
  }

  return Buffer;
}
