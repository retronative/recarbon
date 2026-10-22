# eqv rA,rS,rB: rA = ~(rS ^ rB) on 64 bits; eqv. sets cr0 from the low word
# (signed compare with 0, SO from XER). Expected values worked out by hand
# from the PowerPC ISA, not from the recompiler.

test_carbon_eqv_1:
  #_ REGISTER_IN r4 0x0123456789ABCDEF
  #_ REGISTER_IN r5 0x0123456789ABCDEF
  eqv r3, r4, r5
  blr
  #_ REGISTER_OUT r3 0xFFFFFFFFFFFFFFFF

test_carbon_eqv_2:
  #_ REGISTER_IN r4 0x0000000000000000
  #_ REGISTER_IN r5 0xFFFFFFFFFFFFFFFF
  eqv r3, r4, r5
  blr
  #_ REGISTER_OUT r3 0x0000000000000000

test_carbon_eqv_3:
  #_ REGISTER_IN r4 0xF0F0F0F0F0F0F0F0
  #_ REGISTER_IN r5 0xFF00FF00FF00FF00
  eqv r3, r4, r5
  blr
  #_ REGISTER_OUT r3 0xF00FF00FF00FF00F

test_carbon_eqv_4:
  #_ REGISTER_IN r4 0x00000000FFFFFFFF
  #_ REGISTER_IN r5 0x0000000012345678
  eqv r3, r4, r5
  blr
  #_ REGISTER_OUT r3 0xFFFFFFFF12345678

test_carbon_eqv_5:
  #_ REGISTER_IN r4 0x0000000000000000
  #_ REGISTER_IN r5 0x0000000080000000
  #_ REGISTER_IN r3 1
  eqv. r6, r4, r5
  bgt cr0, .+8
  li r3, 2
  blr
  #_ REGISTER_OUT r6 0xFFFFFFFF7FFFFFFF
  #_ REGISTER_OUT r3 1

test_carbon_eqv_6:
  #_ REGISTER_IN r4 0x0000000000000000
  #_ REGISTER_IN r5 0x0000000000000000
  #_ REGISTER_IN r3 1
  eqv. r6, r4, r5
  blt cr0, .+8
  li r3, 2
  blr
  #_ REGISTER_OUT r6 0xFFFFFFFFFFFFFFFF
  #_ REGISTER_OUT r3 1

test_carbon_eqv_7:
  #_ REGISTER_IN r4 0x0000000000000000
  #_ REGISTER_IN r5 0x00000000FFFFFFFF
  #_ REGISTER_IN r3 1
  eqv. r6, r4, r5
  beq cr0, .+8
  li r3, 2
  blr
  #_ REGISTER_OUT r6 0xFFFFFFFF00000000
  #_ REGISTER_OUT r3 1
