- variables:
  - name: "imm"
    in: 0
  - name: "rs2"
    in: 0
  - name: "rs1"
    in: 0
  overflow: 0
  underflow: 0
  has_valid_test_memop: 0
  has_store:
- variables:
  - name: "imm"
    in: 8191
  - name: "rs2"
    in: 0
  - name: "rs1"
    in: 4104
  overflow: 0
  underflow: 0
  has_valid_test_memop: 0
  has_store:
- variables:
  - name: "imm"
    in: 33554688
  - name: "rs2"
    in: 0
  - name: "rs1"
    in: 33562880
  overflow: 0
  underflow: 0
  has_valid_test_memop: 0
  has_store:
- variables:
  - name: "imm"
    in: 33554688
  - name: "rs2"
    in: 16711680
  - name: "rs1"
    in: 33562880
  overflow: 0
  underflow: 0
  has_valid_test_memop: 1
  has_store:
  - address: 8704
    value: 16711680
    size: 32
- variables:
  - name: "imm"
    in: 16797984
  - name: "rs2"
    in: 16711680
  - name: "rs1"
    in: 4278177600
  overflow: 1
  underflow: 0
  has_valid_test_memop: 1
  has_store:
  - address: 8288
    value: 16711680
    size: 32
