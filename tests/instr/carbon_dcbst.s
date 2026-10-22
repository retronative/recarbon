# dcbst rA,rB only writes a cache block back to memory: no architectural
# register effect. It must not change registers or stop execution.

test_carbon_dcbst_1:
  #_ REGISTER_IN r4 0x1000
  #_ REGISTER_IN r5 0x80
  #_ REGISTER_IN r3 1
  dcbst r4, r5
  li r3, 7
  blr
  #_ REGISTER_OUT r3 7
  #_ REGISTER_OUT r4 0x1000
  #_ REGISTER_OUT r5 0x80
