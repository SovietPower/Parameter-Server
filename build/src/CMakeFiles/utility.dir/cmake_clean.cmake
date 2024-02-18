file(REMOVE_RECURSE
  "libutility.a"
  "libutility.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/utility.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
