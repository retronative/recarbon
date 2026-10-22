# bdnzt BI,target: CTR = CTR - 1, then branch if CTR != 0 and CR bit BI is set.
# The CTR test is on the low 32 bits (Xenia's bcx; XenonRecomp's bdnz), so a
# low word that reaches 0 does not branch even if the high word is nonzero.
# r3 = 1 when taken (li skipped), 2 when not. Where the remaining CTR is small
# and nonzero, a bdnz loop counts it into r6 (XenonRecomp has no mfctr, and a
# test with an untranslatable instruction is silently skipped by the harness).
# Expected values worked out by hand from the PowerPC ISA.

test_carbon_bdnzt_1:
  #_ REGISTER_IN r4 0x5
  #_ REGISTER_IN r5 0
  #_ REGISTER_IN r3 1
  mtctr r4
  cmpwi cr0, r5, 0
  bdnzt 2, .+8
  li r3, 2
  li r6, 0
  addi r6, r6, 1
  bdnz .-4
  blr
  #_ REGISTER_OUT r3 1
  #_ REGISTER_OUT r6 4

test_carbon_bdnzt_2:
  #_ REGISTER_IN r4 0x5
  #_ REGISTER_IN r5 1
  #_ REGISTER_IN r3 1
  mtctr r4
  cmpwi cr0, r5, 0
  bdnzt 2, .+8
  li r3, 2
  li r6, 0
  addi r6, r6, 1
  bdnz .-4
  blr
  #_ REGISTER_OUT r3 2
  #_ REGISTER_OUT r6 4

test_carbon_bdnzt_3:
  #_ REGISTER_IN r4 0x1
  #_ REGISTER_IN r5 0
  #_ REGISTER_IN r3 1
  mtctr r4
  cmpwi cr0, r5, 0
  bdnzt 2, .+8
  li r3, 2
  blr
  #_ REGISTER_OUT r3 2

test_carbon_bdnzt_4:
  #_ REGISTER_IN r4 0x0
  #_ REGISTER_IN r5 0
  #_ REGISTER_IN r3 1
  mtctr r4
  cmpwi cr0, r5, 0
  bdnzt 2, .+8
  li r3, 2
  blr
  #_ REGISTER_OUT r3 1

test_carbon_bdnzt_5:
  #_ REGISTER_IN r4 0x100000001
  #_ REGISTER_IN r5 0
  #_ REGISTER_IN r3 1
  mtctr r4
  cmpwi cr0, r5, 0
  bdnzt 2, .+8
  li r3, 2
  blr
  #_ REGISTER_OUT r3 2

test_carbon_bdnzt_6:
  #_ REGISTER_IN r4 0x3
  #_ REGISTER_IN r5 0xFFFFFFFFFFFFFFFF
  #_ REGISTER_IN r3 1
  mtctr r4
  cmpwi cr6, r5, 0
  bdnzt 24, .+8
  li r3, 2
  li r6, 0
  addi r6, r6, 1
  bdnz .-4
  blr
  #_ REGISTER_OUT r3 1
  #_ REGISTER_OUT r6 2

test_carbon_bdnzt_7:
  #_ REGISTER_IN r4 0x3
  #_ REGISTER_IN r5 1
  #_ REGISTER_IN r3 1
  mtctr r4
  cmpwi cr6, r5, 0
  bdnzt 24, .+8
  li r3, 2
  li r6, 0
  addi r6, r6, 1
  bdnz .-4
  blr
  #_ REGISTER_OUT r3 2
  #_ REGISTER_OUT r6 2
