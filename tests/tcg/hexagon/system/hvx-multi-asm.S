	.text
	.global setv0
setv0:
	v0 = vxor(v0, v0)
	v0 = vnot(v0)
	jumpr r31

	.global store_vector_0
store_vector_0:
	vmemu(r0) = v0
	jumpr r31


       .global enable_vector_unit
enable_vector_unit:
       r1 = ssr
       r1 = and(r1, ##0xc7ffffff)
       r1 = or (r1, r0)
       ssr = r1
       isync
       jumpr r31
