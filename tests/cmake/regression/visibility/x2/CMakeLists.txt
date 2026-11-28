# what an exported openfx package creates (two targets of one export set)
add_library(openfx2::Api INTERFACE IMPORTED)
add_library(openfx2::Support INTERFACE IMPORTED)
set_property(TARGET openfx2::Support PROPERTY INTERFACE_LINK_LIBRARIES openfx2::Api)
# OSMesa::OSMesa stands for any item the helper links: use openfx2::Support through it
add_library(OSMesa::OSMesa ALIAS openfx2::Support)
supportext_add_sources(vis_x OSMESA COMPONENTS TEXT)
