; ModuleID = 'QIR (LRZ)'
source_filename = "QIR_Module"

%Qubit = type opaque
%Result = type opaque

define void @EntryPoint() #0 {
entry:
  call void @__quantum__rt__initialize(i8* null)
  call void @__quantum__qis__h__body(%Qubit* null)
  call void @__quantum__qis__cnot__body(%Qubit* null, %Qubit* inttoptr (i64 1 to %Qubit*))
  ;call void @__quantum__qis__mz__body(%Qubit* null, %Result* null)
  ;call void @__quantum__qis__mz__body(%Qubit* inttoptr (i64 1 to %Qubit*), %Result* inttoptr (i64 1 to %Result*))
  call void @__quantum__rt__array_record_output(i64 2, i8* null)
  ret void
}

declare void @__quantum__rt__initialize(i8*)

declare void @__quantum__qis__h__body(%Qubit*)

declare void @__quantum__qis__cnot__body(%Qubit*, %Qubit*)

declare void @__quantum__qis__mz__body(%Qubit*, %Result*) #1

declare void @__quantum__rt__array_record_output(i64, i8*)

attributes #0 = { "entry_point" "num_required_qubits"="2" "num_required_results"="2" }
attributes #1 = { "irreversible" }
