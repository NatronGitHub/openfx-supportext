# the export-set check of a generated <Package>Targets.cmake
foreach(_t openfx2::Api openfx2::Support)
  if(NOT TARGET ${_t})
    message(FATAL_ERROR "${_t} is not visible here: the export set was promoted only in part")
  endif()
endforeach()
