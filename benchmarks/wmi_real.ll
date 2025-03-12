; ModuleID = 'QIR (LRZ)'
source_filename = "QIR_Module"

%Qubit = type opaque
%Result = type opaque

define void @EntryPoint() #0 {
entry:
  call void @__quantum__rt__initialize(i8* null)
  call void @__quantum__qis__x__body(%Qubit* null)
  call void @__quantum__qis__mz__body(%Qubit* null, %Result* null)
  call void @__quantum__rt__array_record_output(i64 1, i8* null)
  ret void
}

declare void @__quantum__rt__initialize(i8*)

declare void @__quantum__qis__x__body(%Qubit*)

declare void @__quantum__qis__mz__body(%Qubit*, %Result*) #1

declare void @__quantum__rt__array_record_output(i64, i8*)

attributes #0 = { "entry_point" "num_required_qubits"="1" "num_required_results"="1" }
attributes #1 = { "irreversible" }
