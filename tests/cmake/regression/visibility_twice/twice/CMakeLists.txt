if(SX_VIS_TWICE_DEFINE)
  add_library(vis_twice MODULE ${SX_TEST_SRC})
else()
  # a fresh imported target (through an ALIAS), created here
  add_library(visTwice::OSMesa INTERFACE IMPORTED)
  add_library(OSMesa::OSMesa ALIAS visTwice::OSMesa)
  supportext_add_sources(vis_twice OSMESA COMPONENTS TEXT)
endif()
